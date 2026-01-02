# Process Execution

To execute a command, the shell must create a new process and run the program in that process. This chapter explains the three system calls that make this possible: `fork()`, `execvp()`, and `waitpid()`. Understanding these calls is fundamental to understanding how Unix systems work.

## What is a Process

A process is a running instance of a program. When you run `ls`, the operating system creates a process that loads the `ls` program into memory and executes it. Each process has its own memory space, file descriptors, and execution context.

The shell itself is a process. When you type a command, the shell creates a new child process to run that command. The shell process (the parent) waits for the child to finish, then continues reading the next command.

This separation is crucial. If the shell ran commands in its own process, a buggy command could crash the shell. By running each command in a separate process, the shell remains stable even when commands fail.

## The Fork System Call

`fork()` is the system call that creates a new process. It's called "fork" because it splits one process into two identical copies. After `fork()` returns, you have two processes running the same code, but they can take different paths based on the return value.

Here's how the shell uses `fork()`:

```114:122:src/shell.c
static pid_t execute_command(const command_t *cmd, int input_fd, int output_fd,
                             bool is_first, bool is_last) {
    if (!cmd || !cmd->argv || !cmd->argv[0]) return -1;

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    }
```

The `fork()` call returns three possible values:
- In the parent process: the child's process ID (a positive integer)
- In the child process: 0
- On error: -1

This design is elegant. The same code runs in both processes, but they can distinguish themselves by checking the return value. The parent gets the child's PID so it can track the child. The child gets 0, which is never a valid PID, so it knows it's the child.

## Understanding Fork Behavior

When `fork()` is called, the kernel creates an almost exact copy of the parent process. The child gets:
- A copy of the parent's memory (code, data, stack)
- Copies of all open file descriptors
- The same current working directory
- The same environment variables

The child is independent after the fork. Changes to memory in one process don't affect the other. This is called copy on write: the kernel shares memory pages between parent and child until one tries to modify them, at which point the kernel makes a copy.

This copying might seem inefficient, but it's optimized. The kernel doesn't actually copy all memory immediately. It uses virtual memory tricks to share pages until they're modified. For a shell that just wants to run a program, most of the parent's memory won't be used by the child anyway, since `exec()` will replace it.

## The Exec System Call

After forking, the child process is still running the shell's code. To run a user's command, the child must replace itself with the target program. This is what `exec()` does.

The shell uses `execvp()`, which is one variant of the `exec()` family:

```163:166:src/shell.c
        // Execute command
        execvp(cmd->argv[0], cmd->argv);
        perror(cmd->argv[0]);
        exit(127); // Command not found
```

The `v` in `execvp` means it takes an argument vector (an array of strings). The `p` means it searches the PATH environment variable to find the executable.

`execvp()` does something remarkable: it completely replaces the current process. The shell's code disappears. The shell's data disappears. Everything is replaced with the new program's code and data. The process ID stays the same, but it's now running a completely different program.

If `execvp()` succeeds, it never returns. The function call never completes because the code that called it no longer exists. If it fails (for example, the program doesn't exist), it returns -1, and the child prints an error and exits.

## Why Fork Then Exec

You might wonder why we need both `fork()` and `exec()`. Why not just have a system call that creates a new process and runs a program in one step?

The answer is flexibility. Sometimes you want to do things in the child process before executing the new program. The shell does this: it sets up file descriptors for pipes and redirection before calling `exec()`. If `fork()` and `exec()` were combined, you'd lose this ability.

The fork exec pattern is so common that some systems provide a combined function like `posix_spawn()`, but the traditional approach of separate calls is more flexible and is what most shells use.

## The Wait System Call

After forking, the parent process needs to wait for the child to finish. This serves two purposes:
1. It prevents zombie processes (processes that have finished but haven't been cleaned up)
2. It allows the parent to get the child's exit status

The shell uses `waitpid()` to wait for a specific child:

```238:254:src/shell.c
    if (!background) {
        // Wait for all processes in foreground
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
        g_foreground_pgid = 0;
```

`waitpid()` blocks until the specified process terminates. The second argument is a pointer to an integer where the wait status is stored. The third argument can include flags; we use 0 here to wait indefinitely.

The wait status is a bitfield that encodes how the process terminated. The `WIFEXITED()` macro checks if the process exited normally (by calling `exit()` or returning from `main()`). If true, `WEXITSTATUS()` extracts the exit code. If the process was killed by a signal, `WIFSIGNALED()` is true and `WTERMSIG()` extracts the signal number.

For pipelines with multiple processes, the shell waits for all of them. However, it only uses the exit status of the last command as the pipeline's exit status. This matches standard shell behavior.

## Zombie Processes

When a process terminates, it doesn't disappear immediately. It becomes a zombie: a process that has finished executing but still has an entry in the process table. The zombie exists so the parent can read its exit status with `wait()`.

If the parent never calls `wait()`, the zombie remains forever, consuming a slot in the process table. This is why the shell always waits for foreground processes. For background processes, the shell doesn't wait immediately, but the process will be reaped when the shell exits or when it eventually calls `wait()`.

## Background Execution

When a command ends with `&`, the shell runs it in the background. The shell doesn't wait for it to finish:

```256:260:src/shell.c
    } else {
        // Background execution - don't wait, just print PID
        printf("[%d]\n", (int)pids[num_cmds - 1]);
        g_foreground_pgid = 0;
    }
```

The shell prints the process ID and immediately returns to the prompt. The command continues running in the background. The shell doesn't track when it finishes. In a production shell, you'd implement job control to track background jobs and allow bringing them to the foreground.

## Process Groups

Process groups are used to manage signals. When you press Ctrl+C, the terminal sends SIGINT to the entire foreground process group, not just one process. This is important for pipelines: you want to kill all processes in the pipeline, not just one.

The shell sets up process groups like this:

```227:231:src/shell.c
        // Set foreground process group for signal handling
        if (!pipeline->commands[i].background && is_first) {
            g_foreground_pgid = pid;
            setpgid(pid, pid);
        }
```

The first process in a foreground pipeline becomes the process group leader. All processes in that pipeline should be in the same group. When Ctrl+C is pressed, the signal goes to the entire group.

The `setpgid(pid, pid)` call sets the process group ID. The first argument is the process ID, and the second is the desired group ID. By setting both to the same value, we make that process the leader of a new group.

## Error Handling

`fork()` can fail if the system is out of resources. This is rare but possible. The shell checks for this and reports an error. If `fork()` fails, there's nothing to clean up, so the shell just returns an error.

`execvp()` can fail if the program doesn't exist or isn't executable. The child handles this by printing an error message and exiting with status 127, the conventional code for "command not found". The parent will see this exit status when it waits, but the error message has already been printed by the child.

This is why you see error messages like "ls: command not found" even though the shell didn't explicitly print them. The error comes from the child process after `execvp()` fails.

## Putting It All Together

Here's the complete sequence for executing a simple command like `ls`:

1. User types `ls` and presses enter
2. Shell calls `fork()`, creating a child process
3. In the child: shell calls `execvp("ls", ...)`, replacing itself with the `ls` program
4. In the parent: shell calls `waitpid()` to wait for the child
5. The `ls` program runs, prints its output, and exits
6. The parent's `waitpid()` returns with the exit status
7. Shell prints a new prompt and reads the next command

This sequence happens for every command, whether it's a simple `ls` or a complex pipeline. The fork exec wait pattern is the foundation of process execution in Unix systems.
