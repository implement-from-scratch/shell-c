# Command Parsing

Before the shell can execute a command, it must understand what the user typed. The string `ls -la | grep test` means nothing to the execution engine. It needs to know: run `ls` with arguments `-la`, pipe its output to `grep` with argument `test`.

This transformation from text to structured data is called parsing. The shell's parser does this in two phases: first it breaks the input into tokens, then it builds a data structure representing the command.

## What is Tokenization

Tokenization is the process of breaking a string into smaller pieces called tokens. In the command `ls -la`, there are three tokens: `ls`, `-la`, and the pipe symbol `|` if present.

The tricky part is knowing where one token ends and another begins. Spaces usually separate tokens, but not always. Quoted strings like `"hello world"` should be treated as a single token even though they contain a space.

Consider this command:
```
echo "hello world" | grep hello
```

A naive tokenizer that splits on spaces would produce: `echo`, `"hello`, `world"`, `|`, `grep`, `hello`. But we want: `echo`, `"hello world"`, `|`, `grep`, `hello`. The quotes tell us that everything inside them is one token.

## The Tokenization Algorithm

The shell's tokenizer scans the input character by character, tracking whether it's inside quotes:

```18:64:src/parser.c
static int tokenize(const char *line, char **tokens, int max_tokens) {
    char *line_copy = strdup(line);
    if (!line_copy) return -1;

    int count = 0;
    char *token = line_copy;

    while (count < max_tokens - 1) {
        // Skip leading whitespace
        while (isspace(*token)) token++;

        if (*token == '\0') break;

        char *start = token;
        bool in_quotes = false;
        char quote_char = '\0';

        // Find end of token
        while (*token != '\0') {
            if (!in_quotes && (*token == '"' || *token == '\'')) {
                in_quotes = true;
                quote_char = *token;
                token++;
            } else if (in_quotes && *token == quote_char) {
                in_quotes = false;
                token++;
            } else if (!in_quotes && isspace(*token)) {
                break;
            } else {
                token++;
            }
        }

        if (token != start) {
            // Null-terminate token
            if (*token != '\0') {
                *token = '\0';
                token++;
            }
            tokens[count++] = strdup(start);
        }
    }

    tokens[count] = NULL;
    free(line_copy);
    return count;
}
```

The function starts by creating a mutable copy of the input string. It needs to modify the string in place to null terminate tokens, so it can't work with the original const string.

The outer loop processes one token per iteration. It first skips any leading whitespace, then finds where the token ends. The inner loop scans characters, tracking quote state.

When the tokenizer sees a quote character (`"` or `'`), it enters quote mode. In this mode, whitespace is treated as part of the token, not as a separator. The tokenizer continues until it finds the matching closing quote.

When not in quotes, whitespace ends the token. The tokenizer null terminates the token in place (by writing `\0` into the string), then duplicates it into the tokens array. The original string is modified, but that's okay because it's a copy.

## Handling Different Quote Types

The tokenizer supports both single and double quotes. It tracks which type opened the quote with `quote_char` and only exits quote mode when it sees the matching closing quote. This means `"hello'world"` is one token (the single quote inside is just a character), and `'hello"world'` is also one token.

This matches standard shell behavior. Single and double quotes work the same way for tokenization purposes, though they might have different meaning for variable expansion in more advanced shells.

## Building the Command Structure

After tokenization, the parser builds a `pipeline_t` structure. This structure represents the entire command line, which may contain multiple commands connected by pipes.

The structure contains an array of `command_t` objects. Each `command_t` represents one command in the pipeline with its arguments, redirections, and execution flags.

Here's how the parser builds this structure:

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

The parser first counts how many commands are in the pipeline by counting pipe symbols. Each `|` separates two commands, so N pipes means N+1 commands.

Then it allocates an array of `command_t` structures, one for each command. The parsing loop processes tokens sequentially, building each command structure.

Regular tokens become arguments in the `argv` array. Special tokens trigger different behavior:
- `|` marks the end of the current command
- `<` indicates input redirection, the next token is the filename
- `>` indicates output redirection with truncation
- `>>` indicates output redirection with append mode
- `&` marks background execution (only valid for the last command)

## Redirection Parsing

When the parser sees a redirection operator, it consumes the next token as the filename. If there's no next token, that's an error: you can't have `<` without a filename.

The parser stores redirection information in the `command_t` structure. The execution engine will use this later to set up file descriptors before running the command.

## Background Execution

The `&` operator marks a command for background execution. The parser only accepts `&` on the last command in a pipeline. If you write `cmd1 & | cmd2`, the `&` is ignored because it's not on the last command.

This matches standard shell behavior. The entire pipeline runs in the foreground or background as a unit. You can't have some commands in the foreground and others in the background within the same pipeline.

## Error Handling

The parser uses a goto based error handling pattern. When any error occurs, control jumps to the error label which calls `free_pipeline()` to clean up all allocated memory.

This ensures no memory leaks even when parsing fails partway through. The parser might have allocated some command structures and argument arrays before hitting an error, and the cleanup function frees everything.

## Memory Management

The parser allocates memory for:
- The pipeline structure's command array
- Each command's argument vector
- Each token string (duplicated from the original input)
- Redirection filenames

All of this must be freed when done. The `free_pipeline()` function walks through the entire structure, freeing all allocated strings and arrays:

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

The cleanup is careful to free nested structures. Each command has an `argv` array, and each element of that array is a string that was allocated separately. All of these must be freed.

## Edge Cases

The parser handles several edge cases:

Empty lines are detected early and skipped. The entry point checks for empty input:

```160:186:src/parser.c
int parse_command(const char *line, pipeline_t *pipeline) {
    if (!line || !pipeline) return -1;

    // Initialize pipeline
    pipeline->commands = NULL;
    pipeline->num_commands = 0;

    // Skip empty lines and comments
    const char *p = line;
    while (isspace(*p)) p++;
    if (*p == '\0' || *p == '#') return 0;

    // Tokenize
    char **tokens = calloc(MAX_TOKENS, sizeof(char*));
    if (!tokens) return -1;

    int token_count = tokenize(line, tokens, MAX_TOKENS);
    if (token_count < 0) {
        free(tokens);
        return -1;
    }

    // Parse tokens into pipeline
    int result = parse_tokens(tokens, pipeline);
    free_tokens(tokens);

    return result;
}
```

Lines starting with `#` are treated as comments and skipped. This allows users to add comments in scripts, though the current shell doesn't support scripts, only interactive use.

The parser also enforces limits. There's a maximum number of tokens per command and a maximum number of commands per pipeline. These limits prevent the parser from consuming unbounded memory on malicious or malformed input.

## The Result

After parsing, the shell has a `pipeline_t` structure that completely describes what to execute. This structure is passed to the execution engine, which uses it to create processes, set up pipes, and run commands. The parser's job is done: it has transformed text into executable instructions.
