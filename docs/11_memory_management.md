# Memory Management

Memory management is critical for any C program. The shell allocates memory for command structures, argument arrays, and token strings. Managing this memory correctly prevents leaks and ensures the shell runs reliably. This chapter examines the memory management patterns used in the shell.

## Memory Allocation Patterns

The shell uses dynamic allocation for all variable sized data structures. Commands can have different numbers of arguments, pipelines can have different numbers of commands, and tokens can be different lengths. Static arrays would waste space or impose arbitrary limits.

The parser allocates memory in several places:
- Pipeline command arrays
- Command argument vectors
- Token strings
- Redirection filenames

Each allocation must be paired with a corresponding deallocation. Missing a deallocation causes a memory leak. Deallocating too early causes use after free errors. Getting it right requires careful tracking of ownership.

## Allocation in the Parser

The parser allocates memory in a structured way. First, it counts how many commands are in the pipeline:

```86:94:src/parser.c
    // Count commands (separated by |)
    size_t num_commands = 1;
    for (int i = 0; tokens[i]; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            num_commands++;
        }
    }

    if (num_commands > MAX_PIPES) return -1;

    pipeline->commands = calloc(num_commands, sizeof(command_t));
```

Then it allocates the command array using `calloc()`, which zeros the memory. This ensures all fields start with safe default values (NULL pointers, false booleans).

For each command, it allocates an argument vector:

```112:114:src/parser.c
        // Collect arguments for this command
        char **argv = calloc(MAX_TOKENS, sizeof(char*));
        if (!argv) goto error;
```

The argument vector is sized to hold the maximum number of tokens, which is more than needed but simplifies the code. A more space efficient implementation would count arguments first and allocate exactly the right size.

## String Duplication

The parser duplicates all token strings using `strdup()`. This creates independent copies that can be freed later:

```57:57:src/parser.c
            tokens[count++] = strdup(start);
```

And for arguments:

```144:144:src/parser.c
                argv[argc++] = strdup(tokens[token_idx++]);
```

`strdup()` allocates memory for the string and copies it. The caller owns this memory and must free it. The parser duplicates strings because the original token array will be freed, and we need the strings to persist in the command structures.

## Memory Deallocation

The shell provides a dedicated function for freeing pipeline structures:

```189:209:src/parser.c
void free_pipeline(pipeline_t *pipeline) {
    if (!pipeline) return;

    for (size_t i = 0; i < pipeline->num_commands; i++) {
        command_t *cmd = &pipeline->commands[i];

        if (cmd->argv) {
            for (int j = 0; cmd->argv[j]; j++) {
                free(cmd->argv[j]);
            }
            free(cmd->argv);
        }

        if (cmd->input_file) free(cmd->input_file);
        if (cmd->output_file) free(cmd->output_file);
    }

    free(pipeline->commands);
    pipeline->commands = NULL;
    pipeline->num_commands = 0;
}
```

This function walks through the entire structure, freeing nested allocations. It frees each argument string, then the argument vector, then redirection filenames, and finally the command array itself.

The function sets pointers to NULL after freeing. This isn't strictly necessary (the structure will be discarded), but it's good practice. It makes it clear that the structure is in a clean state and prevents accidental reuse.

## Error Path Cleanup

When parsing fails, the parser must clean up any memory it allocated before the error. The goto based error handling ensures this happens:

```155:158:src/parser.c
error:
    free_pipeline(pipeline);
    return -1;
}
```

The `free_pipeline()` function handles partial structures correctly. It checks each pointer before freeing, so it's safe to call even if allocation failed partway through.

## Memory Management in Execution

The execution code also allocates memory for process tracking:

```182:189:src/shell.c
    size_t num_cmds = pipeline->num_commands;
    pid_t *pids = calloc(num_cmds, sizeof(pid_t));
    int (*pipe_fds)[2] = calloc(num_cmds - 1, sizeof(int[2]));

    if (!pids || (!pipe_fds && num_cmds > 1)) {
        free(pids);
        free(pipe_fds);
        return 1;
    }
```

These allocations are simpler: just arrays of fixed size elements. They're freed at the end of the function:

```262:264:src/shell.c
    free(pids);
    free(pipe_fds);
    return exit_status;
```

The cleanup happens in all code paths, including error paths. This ensures no leaks even when execution fails.

## Ownership and Lifetime

Understanding memory ownership is crucial. The parser allocates memory and transfers ownership to the caller. The caller (the REPL loop) is responsible for freeing it:

```322:324:src/shell.c
        // Execute pipeline
        exit_status = execute_pipeline(&pipeline);
        free_pipeline(&pipeline);
```

The pipeline structure is allocated on the stack, but it contains pointers to heap allocated data. When `free_pipeline()` is called, it frees all the heap data, but the stack structure itself is automatically freed when it goes out of scope.

This ownership model is clear: the parser creates, the caller uses, the caller frees. There's no ambiguity about who's responsible for what.

## Memory Leak Prevention

The shell prevents leaks through several mechanisms:

1. **Paired allocation and deallocation**: Every `malloc()`/`calloc()`/`strdup()` has a corresponding `free()`
2. **Error path cleanup**: Goto based error handling ensures cleanup on failure
3. **Dedicated cleanup functions**: `free_pipeline()` handles all nested deallocations
4. **Clear ownership**: The caller always frees what the parser allocates

These mechanisms work together to ensure no memory leaks. Even if parsing fails partway through, all allocated memory is freed.

## Use After Free Prevention

The shell prevents use after free errors by:

1. Setting pointers to NULL after freeing (in `free_pipeline()`)
2. Checking pointers before use (in cleanup functions)
3. Not retaining references after freeing

The pipeline structure is only used between allocation and deallocation. After `free_pipeline()` is called, the structure isn't used again, so there's no risk of use after free.

## Stack vs Heap Allocation

The shell uses stack allocation for small, fixed size structures:

```312:312:src/shell.c
        pipeline_t pipeline;
```

The `pipeline_t` structure itself is small (just two pointers), so stack allocation is appropriate. The heap allocated data it points to is freed explicitly.

Larger structures like the command array are heap allocated because their size is variable. Stack space is limited (typically a few megabytes), so variable sized or large allocations must use the heap.

## Memory Limits

The shell enforces limits on memory usage:
- Maximum line length: 4096 characters
- Maximum tokens: 256 per command
- Maximum commands: 64 per pipeline

These limits prevent unbounded memory consumption on malicious or malformed input. They're generous enough for normal use but prevent pathological cases.

A more sophisticated implementation might use dynamic resizing instead of fixed limits, but that adds complexity. The current approach is simple and effective.

## Best Practices

The shell follows several memory management best practices:

1. Use `calloc()` for arrays to ensure zero initialization
2. Check allocation failures and handle them gracefully
3. Free memory in the reverse order of allocation
4. Use dedicated cleanup functions for complex structures
5. Set pointers to NULL after freeing
6. Pair every allocation with a deallocation

These practices ensure the shell manages memory correctly. There are no leaks, no use after free errors, and no double frees. The memory management is straightforward and maintainable.

Memory management in C requires discipline, but the patterns used in the shell are clear and consistent. Understanding these patterns is essential for building reliable systems software.
