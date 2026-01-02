# Error Handling

Error handling is crucial for building robust software. The shell must handle errors gracefully at every level: parsing, process creation, file operations, and signal delivery. This chapter examines the error handling strategies used throughout the shell implementation.

## Error Handling Philosophy

The shell follows a simple error handling philosophy: errors should be reported clearly, and the shell should continue operating when possible. If an error is fatal (like running out of memory), the shell exits. If it's recoverable (like a parse error), the shell reports it and continues.

This philosophy matches user expectations. Users don't want the shell to crash when they make a typo. They want to see an error message and try again. The shell should be resilient to user mistakes and system errors.

## Error Reporting

Errors are reported in different ways depending on where they occur. Parse errors are reported by the shell itself:

```311:316:src/shell.c
        // Parse command
        pipeline_t pipeline;
        if (parse_command(line, &pipeline) == -1) {
            fprintf(stderr, "Parse error\n");
            continue;
        }
```

Execution errors are often reported by the child process. When `execvp()` fails, the child prints an error:

```163:166:src/shell.c
        // Execute command
        execvp(cmd->argv[0], cmd->argv);
        perror(cmd->argv[0]);
        exit(127); // Command not found
```

This is why you see messages like "command not found" even though the shell didn't explicitly print them. The error comes from the child process after `execvp()` fails.

## Return Value Conventions

The shell uses consistent return value conventions. Functions return 0 on success and -1 on error. This matches standard Unix practice and makes error checking straightforward.

Some functions return more specific values. `waitpid()` returns the process ID on success, -1 on error. `fork()` returns the child's PID in the parent, 0 in the child, and -1 on error. These return values encode both success/failure and additional information.

Exit statuses follow a different convention. Exit status 0 means success, and non zero means failure. The shell uses exit status 127 for "command not found", which is a common convention, though not universal.

## Error Handling in Parsing

The parser uses a goto based error handling pattern. When any error occurs, control jumps to an error label that cleans up allocated memory:

```83:158:src/parser.c
static int parse_tokens(char **tokens, pipeline_t *pipeline) {
    if (!tokens || !tokens[0]) return -1;

    // Count commands (separated by |)
    size_t num_commands = 1;
    for (int i = 0; tokens[i]; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            num_commands++;
        }
    }

    if (num_commands > MAX_PIPES) return -1;

    pipeline->commands = calloc(num_commands, sizeof(command_t));
    if (!pipeline->commands) return -1;
    pipeline->num_commands = num_commands;

    int cmd_idx = 0;
    int token_idx = 0;

    while (cmd_idx < (int)num_commands && tokens[token_idx]) {
        command_t *cmd = &pipeline->commands[cmd_idx];
        cmd->argv = NULL;
        cmd->input_file = NULL;
        cmd->output_file = NULL;
        cmd->append_output = false;
        cmd->background = false;

        // Collect arguments for this command
        char **argv = calloc(MAX_TOKENS, sizeof(char*));
        if (!argv) goto error;
        int argc = 0;

        // Parse tokens until pipe, end, or redirection
        while (tokens[token_idx]) {
            if (strcmp(tokens[token_idx], "|") == 0) {
                token_idx++;
                break;
            } else if (strcmp(tokens[token_idx], "<") == 0) {
                token_idx++;
                if (!tokens[token_idx]) goto error;
                cmd->input_file = strdup(tokens[token_idx++]);
            } else if (strcmp(tokens[token_idx], ">") == 0) {
                token_idx++;
                if (!tokens[token_idx]) goto error;
                cmd->output_file = strdup(tokens[token_idx++]);
                cmd->append_output = false;
            } else if (strcmp(tokens[token_idx], ">>") == 0) {
                token_idx++;
                if (!tokens[token_idx]) goto error;
                cmd->output_file = strdup(tokens[token_idx++]);
                cmd->append_output = true;
            } else if (strcmp(tokens[token_idx], "&") == 0) {
                // Background execution (only valid for last command)
                if (cmd_idx == (int)num_commands - 1) {
                    cmd->background = true;
                }
                token_idx++;
                break;
            } else {
                if (argc >= MAX_TOKENS - 1) goto error;
                argv[argc++] = strdup(tokens[token_idx++]);
            }
        }

        argv[argc] = NULL;
        cmd->argv = argv;
        cmd_idx++;
    }

    return 0;

error:
    free_pipeline(pipeline);
    return -1;
}
```

The goto pattern is controversial in some circles, but it's appropriate here. It ensures cleanup happens regardless of where the error occurs. Without it, you'd need to duplicate cleanup code at every error point, which is error prone.

## Error Handling in Process Creation

When `fork()` fails, the shell cannot create a new process. This is rare but possible if the system is out of process slots or memory. The shell reports the error and returns failure:

```118:122:src/shell.c
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    }
```

If `fork()` fails partway through creating a pipeline, the shell must clean up already created processes. The current implementation waits for processes that were successfully forked:

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

This prevents zombie processes, but it doesn't kill processes that are already running. A more sophisticated implementation would send SIGTERM to running processes when a later process fails to start.

## Error Handling in File Operations

File operations can fail for many reasons: files don't exist, permissions are wrong, disks are full. The shell handles these by checking return values and reporting errors:

```54:71:src/shell.c
static int setup_input_redirection(const char *input_file) {
    if (!input_file) return -1;

    int fd = open(input_file, O_RDONLY);
    if (fd == -1) {
        perror(input_file);
        return -1;
    }

    if (dup2(fd, STDIN_FILENO) == -1) {
        perror("dup2");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}
```

If `open()` fails, the function prints an error using `perror()`, which includes the system error message. If `dup2()` fails, it also prints an error and closes the file descriptor to avoid leaks.

These errors occur in the child process, so the error messages come from the child. The parent will see a non zero exit status when it waits, but the detailed error message has already been printed.

## Error Handling in Signal Delivery

Signal delivery can fail if the target process group doesn't exist. The shell's signal handler doesn't check for this:

```24:30:src/shell.c
static void sigint_handler(int sig) {
    (void)sig;
    g_interrupted = 1;
    if (g_foreground_pgid > 0) {
        kill(-g_foreground_pgid, SIGINT);
    }
}
```

This is acceptable because the failure is harmless. If the process group is gone, the processes have already terminated, so there's nothing to signal. Checking the return value would add complexity without providing value.

## Error Recovery Strategies

The shell uses different recovery strategies for different error types:

1. **Parse errors**: Report and continue. The user can correct the mistake and try again.
2. **Execution errors**: Let the child report the error and exit. The parent continues normally.
3. **Fork errors**: Report and abort the pipeline. Clean up what was created.
4. **File errors**: Let the child report and exit. The parent sees the exit status.

This matches user expectations. Parse errors are user mistakes that should be recoverable. Execution errors are command problems that the command itself should report. System errors like fork failures are rare and indicate serious problems.

## Error Message Quality

Good error messages tell the user what went wrong and why. The shell uses `perror()` for system errors, which includes both a custom message and the system error description:

```57:61:src/shell.c
    int fd = open(input_file, O_RDONLY);
    if (fd == -1) {
        perror(input_file);
        return -1;
    }
```

This produces messages like "input.txt: No such file or directory", which tells the user both what file was problematic and why.

For parse errors, the shell uses a simple message:

```313:315:src/shell.c
        if (parse_command(line, &pipeline) == -1) {
            fprintf(stderr, "Parse error\n");
            continue;
```

This could be more specific (which token caused the error, what was expected), but it's sufficient for a basic implementation.

## Error Handling Best Practices

The shell follows several error handling best practices:

1. Check return values from all system calls
2. Use `perror()` for system errors to include error descriptions
3. Clean up resources on error (close file descriptors, free memory)
4. Continue operation when errors are recoverable
5. Exit cleanly when errors are fatal

These practices ensure the shell is robust and user friendly. Errors are reported clearly, and the shell continues operating when possible. This matches the behavior users expect from production shells.

Error handling is often overlooked in educational code, but it's essential for real world software. The shell demonstrates how to handle errors at multiple levels while maintaining code clarity and user experience.
