# Pipeline Execution

A pipeline connects multiple commands so the output of one becomes the input of the next. When you type `ls | grep test`, you're creating a pipeline: `ls` writes its output, and `grep` reads that output as its input.

Pipelines are implemented using Unix pipes, which are unidirectional communication channels between processes. Understanding how pipes work is essential to understanding how pipelines execute.

## What is a Pipe

A pipe is a kernel managed buffer that connects two processes. One process writes data to the pipe, and another process reads it. The pipe has a fixed buffer size (typically 64KB on Linux). If the buffer fills, writes block until space is available. If the buffer is empty, reads block until data arrives.

Pipes are unidirectional: data flows in one direction only. If you need bidirectional communication, you need two pipes.

The `pipe()` system call creates a pipe and returns two file descriptors: one for reading and one for writing. Data written to the write end can be read from the read end.

## Creating Pipes

For a pipeline with N commands, the shell needs N-1 pipes. Each pipe connects one command to the next. Here's how the shell creates them:

```191:199:src/shell.c
    // Create pipes for multi-command pipelines
    for (size_t i = 0; i < num_cmds - 1; i++) {
        if (pipe(pipe_fds[i]) == -1) {
            perror("pipe");
            free(pids);
            free(pipe_fds);
            return 1;
        }
    }
```

The `pipe()` function takes an array of two integers and fills them with file descriptors. `pipe_fds[i][0]` is the read end, and `pipe_fds[i][1]` is the write end. Both are initially open in the parent process.

After creating the pipes, the parent process has all the pipe file descriptors open. The child processes will inherit these descriptors when forked, but they need to be connected to stdin and stdout.

## Connecting Processes to Pipes

After forking, each child process needs its pipe file descriptors connected to standard input and output. The first command writes to pipe 0, the second reads from pipe 0 and writes to pipe 1, and so on.

The shell uses `dup2()` to make this connection:

```131:147:src/shell.c
        // Setup pipe input (if not first command)
        if (!is_first && input_fd != -1) {
            if (dup2(input_fd, STDIN_FILENO) == -1) {
                perror("dup2 input");
                exit(1);
            }
            close(input_fd);
        }

        // Setup pipe output (if not last command)
        if (!is_last && output_fd != -1) {
            if (dup2(output_fd, STDOUT_FILENO) == -1) {
                perror("dup2 output");
                exit(1);
            }
            close(output_fd);
        }
```

For the first command in a pipeline, only stdout is redirected (to the first pipe's write end). For the last command, only stdin is redirected (from the last pipe's read end). For intermediate commands, both are redirected.

After duplicating the file descriptors, the child closes the original pipe descriptors. This is critical. If the child doesn't close them, the pipe will never see EOF when the writer closes, because the file descriptor remains open in the child even though it's not being used.

## Parent Process Cleanup

The parent process also closes pipe file descriptors immediately after forking each child:

```223:225:src/shell.c
        // Close pipe file descriptors in parent
        if (input_fd != -1) close(input_fd);
        if (output_fd != -1) close(output_fd);
```

This ensures each pipe end is only open in one process. If the parent kept the write end open, the pipe would never signal EOF to the reader, because the kernel only sends EOF when all writers close the pipe.

The timing matters. The parent closes the descriptors after forking but before waiting. This allows the pipeline to start executing immediately. If the parent waited before closing, the first command could block waiting for someone to close the write end of its output pipe.

## How Data Flows

Let's trace through a simple pipeline: `ls | grep test`.

1. Parent creates one pipe with read end `pipe[0]` and write end `pipe[1]`
2. Parent forks child1 (ls):
   - Child1 duplicates `pipe[1]` to `STDOUT_FILENO`
   - Child1 closes all pipe descriptors
   - Child1 executes `ls`, which writes to stdout (now the pipe)
3. Parent closes `pipe[1]` in parent
4. Parent forks child2 (grep):
   - Child2 duplicates `pipe[0]` to `STDIN_FILENO`
   - Child2 closes all pipe descriptors
   - Child2 executes `grep test`, reading from stdin (the pipe)
5. Parent closes `pipe[0]` in parent

Now `ls` writes to the pipe, and `grep` reads from it. When `ls` finishes and closes its stdout (the write end of the pipe), `grep` sees EOF on its stdin and exits. This cascading termination is how pipelines naturally complete.

## Blocking Behavior

Pipes have finite buffer capacity. If a writer produces data faster than a reader consumes it, the writer blocks when the buffer fills. If a reader tries to read when no data is available, it blocks until data arrives or the write end closes.

This blocking is actually useful. It naturally throttles fast producers and prevents memory issues. The pipeline runs as fast as the slowest stage, which is often the desired behavior.

The shell doesn't need to manage this blocking. The kernel handles it automatically. The shell just sets up the file descriptors correctly, and the processes communicate through the pipe without any additional coordination.

## Process Synchronization

Pipelines provide implicit synchronization between processes. Each process waits for input from the previous stage, creating a natural flow control. The shell doesn't need explicit synchronization primitives like mutexes or semaphores because the pipe's blocking behavior provides the necessary coordination.

When the shell waits for the pipeline to complete, it waits for all processes:

```240:254:src/shell.c
        for (size_t i = 0; i < num_cmds; i++) {
            int status;
            if (waitpid(pids[i], &status, 0) == -1) {
                if (errno != ECHILD) {
                    perror("waitpid");
                }
            } else if (i == num_cmds - 1) {
                // Get exit status from last command
                if (WIFEXITED(status)) {
                    exit_status = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                    exit_status = 128 + WTERMSIG(status);
                }
            }
        }
```

The shell waits for all processes, not just the last one. This ensures all processes are reaped and prevents zombies. However, only the exit status of the last command is used as the pipeline's exit status, which matches standard shell behavior.

## Error Handling

If any process in the pipeline fails to start, the shell must clean up already created processes. The current implementation waits for processes that were successfully forked:

```211:219:src/shell.c
        if (pid == -1) {
            // Cleanup on error
            for (size_t j = 0; j < i; j++) {
                waitpid(pids[j], NULL, 0);
            }
            free(pids);
            free(pipe_fds);
            return 1;
        }
```

This prevents zombie processes, but it doesn't kill processes that are already running. In a production shell, you'd want to send SIGTERM to running processes when a later process fails to start, though this adds complexity.

## Complex Pipelines

The same principles apply to longer pipelines. For `cmd1 | cmd2 | cmd3 | cmd4`, the shell creates three pipes and connects them in sequence. Each command reads from one pipe and writes to the next. The data flows through all stages automatically.

The shell doesn't need special handling for longer pipelines. It just creates more pipes and connects them the same way. The complexity is in the setup, not in the execution. Once the file descriptors are connected, the processes handle the data flow themselves.

## Why Pipes Work

Pipes work because they're just file descriptors. Programs don't need to know they're reading from a pipe instead of a file. They just read from stdin and write to stdout, and the shell connects those descriptors to pipes.

This is the power of the Unix file descriptor model. Pipes, files, terminals, and network sockets are all accessed through file descriptors. Programs can be written to work with any of them without knowing which one they're using. The shell just wires up the descriptors correctly, and everything works.

Pipelines are one of Unix's most elegant abstractions. They allow simple programs to be composed into complex data processing workflows without any of the programs needing to know about the composition. The shell's job is just to set up the file descriptors correctly, and the programs handle the rest.
