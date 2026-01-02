# Process Model: File Descriptor Duplication During Piping

## Overview

This document explains how file descriptors are duplicated and managed during pipeline execution in the shell implementation.

## Pipeline Execution Model

When executing a pipeline like `cmd1 | cmd2 | cmd3`, the shell creates multiple processes connected via pipes. Each pipe is a unidirectional communication channel with two file descriptors: a read end and a write end.

## File Descriptor Duplication Process

### 1. Pipe Creation

For a pipeline with N commands, the shell creates N-1 pipes:

```
cmd1 | cmd2 | cmd3
     pipe1   pipe2
```

Each pipe is created using `pipe(int pipefd[2])`, which:
- Creates two file descriptors: `pipefd[0]` (read end) and `pipefd[1]` (write end)
- Both descriptors are initially open in the parent process
- Data written to `pipefd[1]` can be read from `pipefd[0]`

### 2. Process Forking

Each command in the pipeline is executed in a separate child process via `fork()`. After forking:
- Child process inherits all open file descriptors from parent
- Both parent and child have copies of the same file descriptors
- These descriptors point to the same underlying file/pipe object

### 3. File Descriptor Duplication with dup2()

The critical step is using `dup2()` to redirect standard input/output:

#### For Intermediate Commands (not first, not last):

```c
// Child process for cmd2 in "cmd1 | cmd2 | cmd3"
dup2(pipe1[0], STDIN_FILENO);   // Read from previous pipe
dup2(pipe2[1], STDOUT_FILENO);  // Write to next pipe
```

#### For First Command:

```c
// Child process for cmd1
dup2(pipe1[1], STDOUT_FILENO);  // Write to first pipe
// STDIN_FILENO remains unchanged (reads from terminal)
```

#### For Last Command:

```c
// Child process for cmd3
dup2(pipe2[0], STDIN_FILENO);   // Read from last pipe
// STDOUT_FILENO remains unchanged (writes to terminal)
```

### 4. File Descriptor Closure

After duplication, the original pipe file descriptors are closed:

```c
close(pipe1[0]);  // Close read end (no longer needed)
close(pipe1[1]);  // Close write end (no longer needed)
```

**Why close?**
- Prevents file descriptor leaks
- Ensures proper pipe termination (EOF when writer closes)
- Each pipe end should only be open in one process

## Memory Layout During Pipeline Execution

```
Parent Process (Shell)
+----------------------+
| pipe1[0], pipe1[1]   |  ← Pipe file descriptors
| pipe2[0], pipe2[1]   |
+----------------------+
         |
         | fork()
         |
    +----+----+
    |         |
Child1     Child2     Child3
+----+     +----+     +----+
|    |     |    |     |    |
|    |     |    |     |    |
+----+     +----+     +----+
  |          |          |
  | dup2     | dup2     | dup2
  |          |          |
STDOUT → STDIN → STDOUT → STDIN
(pipe1)  (pipe1)  (pipe2)  (pipe2)
```

## File Descriptor Table State

### Before dup2() in Child Process:

```
FD Table:
0: stdin (terminal)
1: stdout (terminal)
2: stderr (terminal)
3: pipe1[0] (inherited from parent)
4: pipe1[1] (inherited from parent)
5: pipe2[0] (inherited from parent)
6: pipe2[1] (inherited from parent)
```

### After dup2() for cmd2:

```
FD Table:
0: stdin → pipe1[0] (duplicated)
1: stdout → pipe2[1] (duplicated)
2: stderr (terminal)
3: pipe1[0] (closed after dup2)
4: pipe1[1] (closed after dup2)
5: pipe2[0] (closed after dup2)
6: pipe2[1] (closed after dup2)
```

## Key Concepts

### 1. File Descriptor Duplication

`dup2(oldfd, newfd)` atomically:
- Closes `newfd` if it was open
- Makes `newfd` refer to the same file/pipe as `oldfd`
- Both descriptors share the same file offset and flags

### 2. Reference Counting

The kernel maintains a reference count for each open file/pipe:
- `pipe()` creates 2 references (one per end)
- `fork()` duplicates all references
- `dup2()` creates additional references
- `close()` decrements reference count
- File/pipe is destroyed when count reaches 0

### 3. Pipe Semantics

- **Blocking I/O**: Reading from empty pipe blocks until data available
- **EOF Detection**: Reading from pipe returns 0 when all writers close
- **Broken Pipe**: Writing to pipe with no readers sends SIGPIPE signal

## Example: `ls | grep "test" | wc -l`

1. **Parent creates 2 pipes**: `pipe1` and `pipe2`
2. **Fork child1 (ls)**:
   - `dup2(pipe1[1], STDOUT_FILENO)`
   - `close(pipe1[0])`, `close(pipe1[1])`
   - `execvp("ls", ...)`
3. **Fork child2 (grep)**:
   - `dup2(pipe1[0], STDIN_FILENO)`
   - `dup2(pipe2[1], STDOUT_FILENO)`
   - Close all pipe descriptors
   - `execvp("grep", ...)`
4. **Fork child3 (wc)**:
   - `dup2(pipe2[0], STDIN_FILENO)`
   - Close all pipe descriptors
   - `execvp("wc", ...)`
5. **Parent closes all pipe descriptors** and waits for children

## Error Handling

- If `dup2()` fails, child process exits with error
- If `pipe()` fails, entire pipeline fails
- If `fork()` fails, cleanup existing pipes and report error
- Parent must close all pipe descriptors to avoid leaks

## Interaction with Redirection

When both pipes and redirection are present, redirection takes precedence:

```bash
cmd1 | cmd2 > file.txt
```

In `cmd2`:
1. `dup2(pipe[0], STDIN_FILENO)` - setup pipe input
2. `dup2(file_fd, STDOUT_FILENO)` - **overwrites** pipe output with file

The pipe write end is effectively replaced by the file descriptor.
