/**
 * @file test_parser.c
 * @brief Unit tests for command parsing functionality
 */

#include "../include/shell.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Test parsing a simple command without arguments
 * @return 0 on success, 1 on failure
 */
static int test_parse_simple_command(void) {
  pipeline_t pipeline;
  int result = parse_command("ls", &pipeline);

  if (result != 0) {
    fprintf(stderr, "test_parse_simple_command: parse failed\n");
    return 1;
  }

  if (pipeline.num_commands != 1) {
    fprintf(stderr, "test_parse_simple_command: expected 1 command, got %zu\n",
            pipeline.num_commands);
    free_pipeline(&pipeline);
    return 1;
  }

  if (strcmp(pipeline.commands[0].argv[0], "ls") != 0) {
    fprintf(stderr, "test_parse_simple_command: expected 'ls', got '%s'\n",
            pipeline.commands[0].argv[0]);
    free_pipeline(&pipeline);
    return 1;
  }

  if (pipeline.commands[0].argv[1] != NULL) {
    fprintf(stderr, "test_parse_simple_command: expected NULL terminator\n");
    free_pipeline(&pipeline);
    return 1;
  }

  free_pipeline(&pipeline);
  return 0;
}

/**
 * @brief Test parsing a command with arguments
 * @return 0 on success, 1 on failure
 */
static int test_parse_command_with_args(void) {
  pipeline_t pipeline;
  int result = parse_command("ls -la /tmp", &pipeline);

  if (result != 0) {
    fprintf(stderr, "test_parse_command_with_args: parse failed\n");
    return 1;
  }

  if (pipeline.num_commands != 1) {
    fprintf(stderr, "test_parse_command_with_args: expected 1 command\n");
    free_pipeline(&pipeline);
    return 1;
  }

  if (strcmp(pipeline.commands[0].argv[0], "ls") != 0 ||
      strcmp(pipeline.commands[0].argv[1], "-la") != 0 ||
      strcmp(pipeline.commands[0].argv[2], "/tmp") != 0) {
    fprintf(stderr, "test_parse_command_with_args: arguments mismatch\n");
    free_pipeline(&pipeline);
    return 1;
  }

  if (pipeline.commands[0].argv[3] != NULL) {
    fprintf(stderr, "test_parse_command_with_args: expected NULL terminator\n");
    free_pipeline(&pipeline);
    return 1;
  }

  free_pipeline(&pipeline);
  return 0;
}

/**
 * @brief Test parsing a pipeline with two commands
 * @return 0 on success, 1 on failure
 */
static int test_parse_pipeline(void) {
  pipeline_t pipeline;
  int result = parse_command("ls | grep test", &pipeline);

  if (result != 0) {
    fprintf(stderr, "test_parse_pipeline: parse failed\n");
    return 1;
  }

  if (pipeline.num_commands != 2) {
    fprintf(stderr, "test_parse_pipeline: expected 2 commands, got %zu\n",
            pipeline.num_commands);
    free_pipeline(&pipeline);
    return 1;
  }

  if (strcmp(pipeline.commands[0].argv[0], "ls") != 0) {
    fprintf(stderr, "test_parse_pipeline: first command mismatch\n");
    free_pipeline(&pipeline);
    return 1;
  }

  if (strcmp(pipeline.commands[1].argv[0], "grep") != 0 ||
      strcmp(pipeline.commands[1].argv[1], "test") != 0) {
    fprintf(stderr, "test_parse_pipeline: second command mismatch\n");
    free_pipeline(&pipeline);
    return 1;
  }

  free_pipeline(&pipeline);
  return 0;
}

/**
 * @brief Test parsing input redirection
 * @return 0 on success, 1 on failure
 */
static int test_parse_input_redirection(void) {
  pipeline_t pipeline;
  int result = parse_command("cat < input.txt", &pipeline);

  if (result != 0) {
    fprintf(stderr, "test_parse_input_redirection: parse failed\n");
    return 1;
  }

  if (pipeline.num_commands != 1) {
    fprintf(stderr, "test_parse_input_redirection: expected 1 command\n");
    free_pipeline(&pipeline);
    return 1;
  }

  if (pipeline.commands[0].input_file == NULL ||
      strcmp(pipeline.commands[0].input_file, "input.txt") != 0) {
    fprintf(stderr, "test_parse_input_redirection: input file mismatch\n");
    free_pipeline(&pipeline);
    return 1;
  }

  if (pipeline.commands[0].output_file != NULL) {
    fprintf(stderr, "test_parse_input_redirection: unexpected output file\n");
    free_pipeline(&pipeline);
    return 1;
  }

  free_pipeline(&pipeline);
  return 0;
}

/**
 * @brief Test parsing output redirection
 * @return 0 on success, 1 on failure
 */
static int test_parse_output_redirection(void) {
  pipeline_t pipeline;
  int result = parse_command("ls > output.txt", &pipeline);

  if (result != 0) {
    fprintf(stderr, "test_parse_output_redirection: parse failed\n");
    return 1;
  }

  if (pipeline.commands[0].output_file == NULL ||
      strcmp(pipeline.commands[0].output_file, "output.txt") != 0) {
    fprintf(stderr, "test_parse_output_redirection: output file mismatch\n");
    free_pipeline(&pipeline);
    return 1;
  }

  if (pipeline.commands[0].append_output != false) {
    fprintf(stderr, "test_parse_output_redirection: expected truncate mode\n");
    free_pipeline(&pipeline);
    return 1;
  }

  free_pipeline(&pipeline);
  return 0;
}

/**
 * @brief Test parsing append redirection
 * @return 0 on success, 1 on failure
 */
static int test_parse_append_redirection(void) {
  pipeline_t pipeline;
  int result = parse_command("echo hello >> log.txt", &pipeline);

  if (result != 0) {
    fprintf(stderr, "test_parse_append_redirection: parse failed\n");
    return 1;
  }

  if (pipeline.commands[0].output_file == NULL ||
      strcmp(pipeline.commands[0].output_file, "log.txt") != 0) {
    fprintf(stderr, "test_parse_append_redirection: output file mismatch\n");
    free_pipeline(&pipeline);
    return 1;
  }

  if (pipeline.commands[0].append_output != true) {
    fprintf(stderr, "test_parse_append_redirection: expected append mode\n");
    free_pipeline(&pipeline);
    return 1;
  }

  free_pipeline(&pipeline);
  return 0;
}

/**
 * @brief Test parsing background execution
 * @return 0 on success, 1 on failure
 */
static int test_parse_background(void) {
  pipeline_t pipeline;
  int result = parse_command("sleep 5 &", &pipeline);

  if (result != 0) {
    fprintf(stderr, "test_parse_background: parse failed\n");
    return 1;
  }

  if (pipeline.commands[0].background != true) {
    fprintf(stderr, "test_parse_background: expected background flag\n");
    free_pipeline(&pipeline);
    return 1;
  }

  free_pipeline(&pipeline);
  return 0;
}

/**
 * @brief Test parsing quoted strings
 * @return 0 on success, 1 on failure
 */
static int test_parse_quoted_strings(void) {
  pipeline_t pipeline;
  int result = parse_command("echo \"hello world\"", &pipeline);

  if (result != 0) {
    fprintf(stderr, "test_parse_quoted_strings: parse failed\n");
    return 1;
  }

  if (strcmp(pipeline.commands[0].argv[1], "hello world") != 0) {
    fprintf(stderr, "test_parse_quoted_strings: quoted string mismatch\n");
    free_pipeline(&pipeline);
    return 1;
  }

  free_pipeline(&pipeline);
  return 0;
}

/**
 * @brief Test parsing empty line
 * @return 0 on success, 1 on failure
 */
static int test_parse_empty_line(void) {
  pipeline_t pipeline;
  int result = parse_command("", &pipeline);

  if (result != 0) {
    fprintf(stderr, "test_parse_empty_line: parse failed\n");
    return 1;
  }

  if (pipeline.num_commands != 0) {
    fprintf(stderr, "test_parse_empty_line: expected 0 commands\n");
    free_pipeline(&pipeline);
    return 1;
  }

  return 0;
}

/**
 * @brief Test parsing comment line
 * @return 0 on success, 1 on failure
 */
static int test_parse_comment(void) {
  pipeline_t pipeline;
  int result = parse_command("# This is a comment", &pipeline);

  if (result != 0) {
    fprintf(stderr, "test_parse_comment: parse failed\n");
    return 1;
  }

  if (pipeline.num_commands != 0) {
    fprintf(stderr, "test_parse_comment: expected 0 commands\n");
    free_pipeline(&pipeline);
    return 1;
  }

  return 0;
}

/**
 * @brief Test parsing complex pipeline with redirection
 * @return 0 on success, 1 on failure
 */
static int test_parse_complex_pipeline(void) {
  pipeline_t pipeline;
  int result = parse_command("cat < input.txt | grep test > output.txt", &pipeline);

  if (result != 0) {
    fprintf(stderr, "test_parse_complex_pipeline: parse failed\n");
    return 1;
  }

  if (pipeline.num_commands != 2) {
    fprintf(stderr, "test_parse_complex_pipeline: expected 2 commands\n");
    free_pipeline(&pipeline);
    return 1;
  }

  if (pipeline.commands[0].input_file == NULL ||
      strcmp(pipeline.commands[0].input_file, "input.txt") != 0) {
    fprintf(stderr, "test_parse_complex_pipeline: first command input mismatch\n");
    free_pipeline(&pipeline);
    return 1;
  }

  if (pipeline.commands[1].output_file == NULL ||
      strcmp(pipeline.commands[1].output_file, "output.txt") != 0) {
    fprintf(stderr, "test_parse_complex_pipeline: second command output mismatch\n");
    free_pipeline(&pipeline);
    return 1;
  }

  free_pipeline(&pipeline);
  return 0;
}

/**
 * @brief Run all parser tests
 * @return 0 if all tests pass, 1 if any test fails
 */
int main(void) {
  int failures = 0;

  printf("Running parser tests...\n");

  failures += test_parse_simple_command();
  failures += test_parse_command_with_args();
  failures += test_parse_pipeline();
  failures += test_parse_input_redirection();
  failures += test_parse_output_redirection();
  failures += test_parse_append_redirection();
  failures += test_parse_background();
  failures += test_parse_quoted_strings();
  failures += test_parse_empty_line();
  failures += test_parse_comment();
  failures += test_parse_complex_pipeline();

  if (failures == 0) {
    printf("All parser tests passed!\n");
    return 0;
  } else {
    printf("%d test(s) failed\n", failures);
    return 1;
  }
}
