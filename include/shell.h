/**
 * @file shell.h
 * @brief Unix-style shell implementation header
 */

#ifndef SHELL_H
#define SHELL_H

#include <stdbool.h>
#include <sys/types.h>

/**
 * @brief Maximum command line length
 */
#define MAX_LINE_LEN 4096

/**
 * @brief Maximum number of tokens in a command
 */
#define MAX_TOKENS 256

/**
 * @brief Maximum number of commands in a pipeline
 */
#define MAX_PIPES 64

/**
 * @brief Command structure representing a single command in a pipeline
 */
typedef struct {
  char **argv;        /**< Argument vector (NULL-terminated) */
  char *input_file;   /**< Input redirection file (NULL if none) */
  char *output_file;  /**< Output redirection file (NULL if none) */
  bool append_output; /**< Append mode for output redirection */
  bool background;    /**< Background execution flag */
} command_t;

/**
 * @brief Pipeline structure containing multiple commands
 */
typedef struct {
  command_t *commands; /**< Array of commands */
  size_t num_commands; /**< Number of commands in pipeline */
} pipeline_t;

/**
 * @brief Parse a command line into a pipeline structure
 * @param line Input command line string
 * @param pipeline Output pipeline structure (must be freed with free_pipeline)
 * @return 0 on success, -1 on error
 */
int parse_command(const char *line, pipeline_t *pipeline);

/**
 * @brief Free resources allocated by parse_command
 * @param pipeline Pipeline structure to free
 */
void free_pipeline(pipeline_t *pipeline);

/**
 * @brief Execute a pipeline of commands
 * @param pipeline Pipeline structure to execute
 * @return Exit status of the last command in pipeline
 */
int execute_pipeline(pipeline_t *pipeline);

/**
 * @brief Main shell REPL loop
 * @return Exit status
 */
int shell_main(void);

/**
 * @brief Setup signal handlers for the shell
 */
void setup_signal_handlers(void);

#endif /* SHELL_H */
