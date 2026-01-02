# Signal Handling

Signals are asynchronous notifications sent to processes by the kernel or other processes. When you press Ctrl+C in a terminal, the terminal sends a SIGINT signal to the foreground process group. The shell must handle this signal to provide a responsive user experience.

## What are Signals

Signals are a form of inter process communication. Unlike pipes or files, signals are asynchronous: they can arrive at any time during program execution. When a signal arrives, the kernel interrupts the process's normal execution and calls a signal handler function.

Common signals include:
- SIGINT: Interrupt signal (Ctrl+C)
- SIGTERM: Termination request
- SIGKILL: Forceful kill (cannot be caught)
- SIGSTOP: Stop execution (Ctrl+Z)

The shell needs to handle SIGINT specially. When the user presses Ctrl+C, they want to interrupt the running command, not kill the shell itself. The shell must forward the signal to the command while continuing to operate.

## Setting Up Signal Handlers

The shell registers a signal handler using `sigaction()`, which provides more control than the older `signal()` function:

```35:47:src/shell.c
void setup_signal_handlers(void) {
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
    }

    // Ignore SIGTSTP (Ctrl+Z) for now
    signal(SIGTSTP, SIG_IGN);
}
```

The `sa_handler` field specifies the function to call when the signal arrives. `sa_mask` specifies which signals should be blocked during handler execution. An empty mask means no additional signals are blocked.

The `SA_RESTART` flag is important. It causes system calls interrupted by the signal to automatically restart. Without this, calls like `read()` would fail with EINTR when interrupted, requiring the shell to retry them manually.

## The Signal Handler

The signal handler must be async signal safe. It can only call functions that are guaranteed to work correctly when invoked from a signal handler. Most standard library functions are not safe, so handlers must do minimal work.

Here's the shell's SIGINT handler:

```24:30:src/shell.c
static void sigint_handler(int sig) {
    (void)sig;
    g_interrupted = 1;
    if (g_foreground_pgid > 0) {
        kill(-g_foreground_pgid, SIGINT);
    }
}
```

The handler sets a global flag `g_interrupted` to record that an interrupt occurred. It then sends SIGINT to the foreground process group if one exists. The negative process group ID tells `kill()` to send the signal to all processes in that group, not just one process.

The handler is minimal: it sets a flag and calls `kill()`, both of which are async signal safe. It doesn't print messages or do complex operations that might not be safe in a signal handler context.

## Process Groups

Process groups are used to manage signal delivery. When you press Ctrl+C, the terminal sends SIGINT to the entire foreground process group, not just one process. This is important for pipelines: you want to kill all processes in the pipeline, not just one.

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

## Background Process Protection

Background processes should not receive SIGINT from Ctrl+C. The shell only sets the foreground process group for foreground commands:

```127:129:src/shell.c
        // Set process group for signal handling
        if (!cmd->background) {
            setpgid(0, 0);
        }
```

Background commands don't create a new process group or set the foreground group ID. They remain in the shell's process group, but since they're not the foreground group, they won't receive terminal generated signals.

This is a simplification. A full implementation would move background processes to their own process groups and manage them separately. The current code works for basic use cases but doesn't provide complete job control.

## The Interrupt Flag

The shell checks the interrupt flag in the REPL loop:

```277:329:src/shell.c
    while (1) {
        // Reset interrupt flag
        g_interrupted = 0;

        // Print prompt
        printf("shell> ");
        fflush(stdout);

        // Read command line
        if (!fgets(line, sizeof(line), stdin)) {
            if (feof(stdin)) {
                printf("\n");
                break; // EOF (Ctrl+D)
            }
            if (ferror(stdin)) {
                perror("fgets");
                break;
            }
        }

        // Remove newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        // Skip empty lines
        if (strlen(line) == 0) continue;

        // Handle built-in commands
        if (strcmp(line, "exit") == 0) {
            break;
        }

        // Parse command
        pipeline_t pipeline;
        if (parse_command(line, &pipeline) == -1) {
            fprintf(stderr, "Parse error\n");
            continue;
        }

        if (pipeline.num_commands == 0) {
            continue;
        }

        // Execute pipeline
        exit_status = execute_pipeline(&pipeline);
        free_pipeline(&pipeline);

        // Check for interrupt
        if (g_interrupted) {
            g_interrupted = 0;
        }
    }
```

The flag is reset at the start of each loop iteration. After command execution, the shell checks the flag, though it doesn't take any special action. The flag is mainly useful for tracking that an interrupt occurred, which could be used for cleanup or status reporting.

The `g_interrupted` variable is declared `volatile sig_atomic_t` to ensure the compiler doesn't optimize away reads and writes, and to guarantee atomic access. Signal handlers can execute at any time, so shared variables must be handled carefully.

## Signal Delivery Timing

Signals can arrive at any point during execution. If SIGINT arrives while the shell is parsing or setting up processes, the handler runs immediately. The shell continues execution after the handler returns, which is usually the desired behavior.

If a signal arrives during `waitpid()`, the call is interrupted and returns with EINTR. However, with `SA_RESTART`, the call automatically restarts, so the shell doesn't need to handle EINTR explicitly. This simplifies the code but means the shell can't easily detect when a wait was interrupted.

## Error Recovery

If `sigaction()` fails, the shell prints an error but continues. Without signal handling, Ctrl+C would kill the shell itself, which is undesirable. However, the shell can still function, just without proper signal forwarding to commands.

The `kill()` call in the handler can fail if the process group no longer exists, but the handler doesn't check for errors. This is acceptable because the failure is harmless: if the process group is gone, the processes have already terminated, so there's nothing to signal.

## Why Signal Handling Matters

Without proper signal handling, the shell would be unusable. Every Ctrl+C would kill the shell, forcing users to restart it constantly. By forwarding signals to commands, the shell provides the expected behavior: commands can be interrupted, but the shell keeps running.

Signal handling in a shell is inherently complex because signals are asynchronous and can arrive at any time. The current implementation handles the common case correctly: forwarding Ctrl+C to running commands while keeping the shell alive. More sophisticated features like job control require additional complexity that's beyond the scope of this basic implementation.
