# REPL Loop

The Read Eval Print Loop (REPL) is the core of the interactive shell. It reads a command from the user, executes it, and then loops back to read the next command. This loop continues until the user exits or the shell encounters an unrecoverable error.

The REPL is deceptively simple. It's just a while loop that does the same three things repeatedly: read, parse, execute. But getting the details right is what makes a shell usable.

## The Main Loop

The REPL is implemented as a simple while loop in `shell_main()`:

```271:333:src/shell.c
int shell_main(void) {
    char line[MAX_LINE_LEN];
    int exit_status = 0;

    setup_signal_handlers();

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

    return exit_status;
}
```

The loop runs indefinitely until explicitly broken. Each iteration follows the same pattern: prompt, read, parse, execute, repeat. The simplicity of this structure makes it easy to understand and modify.

## Prompt Display

The shell prints a simple prompt before each command. The prompt is minimal: just `shell> ` followed by a space. Production shells typically show more information like the current directory, username, or hostname, but a simple prompt is sufficient for a basic shell.

The `fflush(stdout)` call is important. It ensures the prompt appears immediately, even if stdout is buffered. Without this, the prompt might not show until a newline is printed, making the shell feel unresponsive. Users expect to see the prompt as soon as they're ready to type.

## Reading Input

The shell uses `fgets()` to read a line of input. This function reads up to the specified buffer size minus one, leaving room for the null terminator. It stops at a newline or EOF.

If `fgets()` returns NULL, the shell checks whether it hit EOF (Ctrl+D) or encountered an error. EOF breaks the loop, allowing the user to exit by pressing Ctrl+D. This is a common Unix convention: Ctrl+D sends EOF to the input stream.

Errors reading from stdin are rare but possible. If the terminal is closed or the process loses its controlling terminal, `fgets()` will fail. In this case, the shell exits because there's no way to continue reading input.

## Processing the Input

After reading, the shell removes the newline character. `fgets()` includes the newline in the buffer if it fits, but the shell doesn't need it. Removing it simplifies later string comparisons and parsing.

Empty lines are skipped with a continue statement. This prevents the shell from trying to parse and execute nothing, which would waste cycles. The check happens after newline removal, so lines containing only whitespace become empty and are also skipped.

## Built-in Commands

The shell recognizes a few built-in commands that are handled directly without forking. Currently, only `exit` is implemented. When the user types `exit`, the loop breaks and the shell terminates.

Built-in commands are checked before parsing. This is efficient because parsing is unnecessary for commands the shell handles internally. However, it means the comparison is case sensitive and exact: `Exit` or `EXIT` won't work, only `exit`.

Adding more built-ins would follow the same pattern: check the command string, perform the action, and continue or break the loop as appropriate. Common built-ins include `cd` for changing directories, `export` for setting environment variables, and `history` for command history.

## Parsing and Execution

After handling built-ins, the shell parses the command line into a pipeline structure. If parsing fails, the shell prints an error and continues to the next iteration. This allows the user to correct mistakes without the shell crashing.

If parsing succeeds but produces zero commands (empty line or comment), the shell also continues. This handles edge cases gracefully.

Execution happens through `execute_pipeline()`, which handles all the complexity of process creation, pipe setup, and waiting. The function returns the exit status of the last command in the pipeline, which the shell stores but doesn't currently use for anything.

After execution, the shell frees the pipeline structure to prevent memory leaks. The `free_pipeline()` function walks through all commands and frees all allocated strings and arrays.

## Loop Termination

The loop can exit in three ways:
1. User types `exit`
2. User presses Ctrl+D (EOF on stdin)
3. Error reading from stdin

In all cases, the shell returns the last command's exit status. This allows scripts or other programs that invoke the shell to check whether commands succeeded.

The exit status propagation is basic. A production shell would track the exit status more carefully and make it available through the `$?` variable. The current implementation just returns it from `main()`, which becomes the shell's exit code.

## Error Recovery

The REPL is designed to be resilient. Parse errors don't crash the shell; they're reported and the loop continues. Execution errors are handled by the execution functions, which return error statuses that the shell can act on.

The one case where the shell can't recover is stdin errors. If `fgets()` fails due to a system error, there's no way to continue reading input, so the shell exits. This is rare in practice, as stdin errors typically only occur when the terminal is closed or the process loses its controlling terminal.

## State Management

The REPL maintains minimal state. The `exit_status` variable tracks the last command's result, but it's not used for anything beyond being returned at the end. The `g_interrupted` flag tracks signal delivery, but again it's not used for control flow.

A more sophisticated shell would maintain more state: current directory, environment variables, command history, aliases, and so on. The current implementation keeps it simple, focusing on the core loop mechanics rather than shell features.

## Why the REPL Matters

The REPL is what makes the shell interactive. Without it, the shell would be a one shot program that reads one command and exits. The loop allows users to run multiple commands in sequence, building up complex workflows.

The REPL's simplicity is its strength. The loop is easy to understand and modify. Adding features like command history or tab completion would integrate into this same loop structure, reading from a different source or processing input before parsing, but the fundamental pattern remains the same.

Every interactive program has some form of REPL, whether it's a shell, a debugger, or a database client. Understanding how to build a REPL is a fundamental skill for systems programming.
