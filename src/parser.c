/**
 * @file parser.c
 * @brief Command line parser implementation
 */

#include "shell.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/**
 * @brief Tokenize a command line into an array of tokens
 * @param line Input command line
 * @param tokens Output array of token pointers (must be freed)
 * @param max_tokens Maximum number of tokens
 * @return Number of tokens parsed, or -1 on error
 */
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

/**
 * @brief Free token array allocated by tokenize
 */
static void free_tokens(char **tokens) {
    if (!tokens) return;
    for (int i = 0; tokens[i]; i++) {
        free(tokens[i]);
    }
    free(tokens);
}

/**
 * @brief Parse tokens into a pipeline structure
 * @param tokens Array of token strings
 * @param pipeline Output pipeline structure
 * @return 0 on success, -1 on error
 */
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
