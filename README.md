# Unix-Style Shell Implementation

A minimal Unix-style shell implementation in C supporting pipes, I/O redirection, background execution, and signal handling.

## Architecture Overview

The shell consists of three main components:

- **Parser** (`src/parser.c`): Tokenizes command lines and builds pipeline structures
- **Shell Core** (`src/shell.c`): Implements REPL loop, process execution, and I/O redirection
- **Header** (`include/shell.h`): Defines data structures and function interfaces

The shell uses standard Unix process control primitives (`fork`, `execvp`, `waitpid`) to execute commands and manage pipelines.

## Read-Eval-Print Loop (REPL) Flowchart

```
                    ┌─────────────────┐
                    │   Shell Start   │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │ Setup Signal    │
                    │   Handlers      │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │  Print Prompt   │
                    │   "shell> "     │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │  Read Command   │
                    │   Line (fgets)  │
                    └────────┬────────┘
                             │
                ┌────────────┴────────────┐
                │                         │
                ▼                         ▼
        ┌───────────────┐        ┌──────────────┐
        │   EOF (Ctrl+D)│        │  Read Error  │
        └───────┬───────┘        └──────┬───────┘
                │                       │
                │                       │
                └───────────┬───────────┘
                            │
                            ▼
                    ┌─────────────────┐
                    │  Remove Newline │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │  Empty Line?    │
                    └────────┬────────┘
                             │
                ┌────────────┴────────────┐
                │                         │
                YES                       NO
                │                         │
                │                         ▼
                │                ┌─────────────────┐
                │                │  Built-in cmd?  │
                │                │  (e.g., exit)  │
                │                └────────┬────────┘
                │                         │
                │            ┌────────────┴────────────┐
                │            │                         │
                │            YES                       NO
                │            │                         │
                │            │                         ▼
                │            │                ┌─────────────────┐
                │            │                │  Parse Command  │
                │            │                │  (parse_command)│
                │            │                └────────┬────────┘
                │            │                         │
                │            │            ┌────────────┴────────────┐
                │            │            │                         │
                │            │            Parse Error              Success
                │            │            │                         │
                │            │            │                         ▼
                │            │            │                ┌─────────────────┐
                │            │            │                │  Execute        │
                │            │            │                │  Pipeline       │
                │            │            │                │(execute_pipeline)│
                │            │            │                └────────┬────────┘
                │            │            │                         │
                │            │            │                         ▼
                │            │            │                ┌─────────────────┐
                │            │            │                │  Create Pipes   │
                │            │            │                │  (N-1 pipes for │
                │            │            │                │   N commands)   │
                │            │            │                └────────┬────────┘
                │            │            │                         │
                │            │            │                         ▼
                │            │            │                ┌─────────────────┐
                │            │            │                │  For Each Cmd:  │
                │            │            │                │  - fork()       │
                │            │            │                │  - dup2() pipes │
                │            │            │                │  - setup I/O    │
                │            │            │                │  - execvp()     │
                │            │            │                └────────┬────────┘
                │            │            │                         │
                │            │            │                         ▼
                │            │            │                ┌─────────────────┐
                │            │            │                │  Background?    │
                │            │            │                └────────┬────────┘
                │            │            │                         │
                │            │            │            ┌────────────┴────────────┐
                │            │            │            │                         │
                │            │            │            YES                       NO
                │            │            │            │                         │
                │            │            │            │                         ▼
                │            │            │            │                ┌─────────────────┐
                │            │            │            │                │  waitpid() for  │
                │            │            │            │                │  all children   │
                │            │            │            │                └────────┬────────┘
                │            │            │            │                         │
                │            │            │            │                         ▼
                │            │            │            │                ┌─────────────────┐
                │            │            │            │                │  Get Exit       │
                │            │            │            │                │  Status         │
                │            │            │            │                └────────┬────────┘
                │            │            │            │                         │
                │            │            │            │                         │
                │            │            │            └────────────┬────────────┘
                │            │            │                         │
                │            │            │                         │
                │            │            └────────────┬────────────┘
                │            │                         │
                │            │                         │
                │            └────────────┬────────────┘
                │                         │
                │                         │
                └─────────────┬───────────┘
                              │
                              │
                              ▼
                    ┌─────────────────┐
                    │  Check SIGINT   │
                    │  (Ctrl+C)       │
                    └────────┬────────┘
                             │
                             │
                             ▼
                    ┌─────────────────┐
                    │  Loop Back to   │
                    │  Print Prompt   │
                    └─────────────────┘
```

## Memory Layout

```
┌─────────────────────────────────────────────────────────┐
│                    Stack Segment                         │
│  ┌───────────────────────────────────────────────────┐  │
│  │  shell_main() stack frame                         │  │
│  │  - line[MAX_LINE_LEN] (4096 bytes)                │  │
│  │  - pipeline_t structure                           │  │
│  │  - Local variables                                │  │
│  └───────────────────────────────────────────────────┘  │
│                                                          │
│  ┌───────────────────────────────────────────────────┐  │
│  │  execute_pipeline() stack frame                   │  │
│  │  - pids[] array (pointer to heap)                 │  │
│  │  - pipe_fds[][] array (pointer to heap)           │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│                    Heap Segment                          │
│  ┌───────────────────────────────────────────────────┐  │
│  │  pipeline_t.commands[]                            │  │
│  │  ┌─────────────┐  ┌─────────────┐  ┌───────────┐ │  │
│  │  │ command_t   │  │ command_t   │  │ command_t │ │  │
│  │  │ - argv[]    │  │ - argv[]    │  │ - argv[]  │ │  │
│  │  │ - input_f   │  │ - input_f   │  │ - input_f │ │  │
│  │  │ - output_f  │  │ - output_f  │  │ - output_f│ │  │
│  │  └─────────────┘  └─────────────┘  └───────────┘ │  │
│  └───────────────────────────────────────────────────┘  │
│                                                          │
│  ┌───────────────────────────────────────────────────┐  │
│  │  Token strings (from parser)                      │  │
│  │  - argv[0], argv[1], ... (char*)                  │  │
│  │  - input_file, output_file (char*)                │  │
│  └───────────────────────────────────────────────────┘  │
│                                                          │
│  ┌───────────────────────────────────────────────────┐  │
│  │  Process ID arrays                                │  │
│  │  - pids[] (pid_t*)                                 │  │
│  │  - pipe_fds[][] (int[2]*)                         │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│              Kernel Space (File Descriptors)             │
│  ┌───────────────────────────────────────────────────┐  │
│  │  Process File Descriptor Table                   │  │
│  │  0: stdin  → terminal                             │  │
│  │  1: stdout → terminal (or pipe/file)              │  │
│  │  2: stderr → terminal                             │  │
│  │  3: pipe[0] → read end                            │  │
│  │  4: pipe[1] → write end                            │  │
│  │  ...                                               │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

## How it Works

### 1. Command Parsing

The parser tokenizes the input line and identifies:
- Command arguments (space-separated tokens)
- Pipe operators (`|`)
- Redirection operators (`<`, `>`, `>>`)
- Background execution (`&`)

The parser builds a `pipeline_t` structure containing an array of `command_t` structures.

### 2. Pipeline Execution

For a pipeline like `cmd1 | cmd2 | cmd3`:

1. **Create Pipes**: Allocate N-1 pipes for N commands
2. **Fork Processes**: Create child process for each command
3. **Setup I/O**: Use `dup2()` to redirect stdin/stdout through pipes
4. **Execute**: Call `execvp()` to replace child process image
5. **Wait**: Parent waits for all children (unless background)

### 3. I/O Redirection

- **Input** (`<`): Opens file and duplicates to `STDIN_FILENO`
- **Output** (`>`): Opens file (truncate) and duplicates to `STDOUT_FILENO`
- **Append** (`>>`): Opens file (append) and duplicates to `STDOUT_FILENO`

Redirection takes precedence over pipes when both are specified.

### 4. Signal Handling

- **SIGINT (Ctrl+C)**: Handler sends signal to foreground process group
- Parent shell ignores interrupt and continues REPL loop
- Background processes are not affected by Ctrl+C

### 5. Background Execution

Commands ending with `&` execute in background:
- Parent does not wait for completion
- Process ID is printed
- Shell immediately returns to prompt

## Build Instructions

```bash
# Compile
gcc -Wall -Wextra -std=c11 -Iinclude src/shell.c src/parser.c -o shell

# Run
./shell

# Example commands
shell> ls -la
shell> echo "hello" | grep "h"
shell> ls > output.txt
shell> cat < input.txt
shell> sleep 5 &
```

## Features

- Process control (`fork`, `execvp`, `waitpid`)
- Pipeline execution (`|`)
- Input redirection (`<`)
- Output redirection (`>`, `>>`)
- Background execution (`&`)
- Signal handling (SIGINT/Ctrl+C)
- Command parsing and tokenization

## Documentation

Complete documentation is available in the `docs/` directory. The documentation is organized into parts, with each chapter building on previous concepts.

### Table of Contents

#### Part 1. Shell from 0 to 1

1. [Introduction](docs/01_introduction.md) - What is a shell and why build one from scratch
2. [Process Execution](docs/02_process_execution.md) - Understanding fork, exec, and wait
3. [Command Parsing](docs/03_command_parsing.md) - Tokenizing and parsing user input
4. [I/O Redirection](docs/04_io_redirection.md) - Redirecting input and output to files
5. [Pipeline Execution](docs/05_pipeline_execution.md) - Connecting processes with pipes
6. [Signal Handling](docs/06_signal_handling.md) - Handling Ctrl+C and other signals
7. [REPL Loop](docs/07_repl_loop.md) - The read eval print loop that drives the shell

#### Part 2. Advanced Topics

1. [File Descriptor Management](docs/08_file_descriptor_management.md) - Deep dive into file descriptor duplication
2. [Process Groups](docs/09_process_groups.md) - Process groups and job control
3. [Error Handling](docs/10_error_handling.md) - Comprehensive error handling strategies
4. [Memory Management](docs/11_memory_management.md) - Allocation, deallocation, and avoiding leaks

See [docs/README.md](docs/README.md) for the complete documentation index.
