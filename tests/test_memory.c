/**
 * @file test_memory.c
 * @brief Unit tests for memory management and cleanup
 */

#include "../include/shell.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Test that free_pipeline handles NULL gracefully
 * @return 0 on success, 1 on failure
 */
static int test_free_pipeline_null(void) {
  pipeline_t pipeline = {0};
  free_pipeline(&pipeline);
  free_pipeline(NULL);
  return 0;
}

/**
 * @brief Test that free_pipeline cleans up all allocated memory
 * @return 0 on success, 1 on failure
 */
static int test_free_pipeline_cleanup(void) {
  pipeline_t pipeline;
  int result = parse_command("ls -la | grep test > output.txt", &pipeline);

  if (result != 0) {
    fprintf(stderr, "test_free_pipeline_cleanup: parse failed\n");
    return 1;
  }

  free_pipeline(&pipeline);

  if (pipeline.commands != NULL) {
    fprintf(stderr, "test_free_pipeline_cleanup: commands not NULL after free\n");
    return 1;
  }

  if (pipeline.num_commands != 0) {
    fprintf(stderr, "test_free_pipeline_cleanup: num_commands not zero after free\n");
    return 1;
  }

  return 0;
}

/**
 * @brief Test memory cleanup with multiple commands
 * @return 0 on success, 1 on failure
 */
static int test_free_pipeline_multiple_commands(void) {
  pipeline_t pipeline;
  int result = parse_command("cmd1 | cmd2 | cmd3 | cmd4", &pipeline);

  if (result != 0) {
    fprintf(stderr, "test_free_pipeline_multiple_commands: parse failed\n");
    return 1;
  }

  if (pipeline.num_commands != 4) {
    fprintf(stderr, "test_free_pipeline_multiple_commands: expected 4 commands\n");
    free_pipeline(&pipeline);
    return 1;
  }

  free_pipeline(&pipeline);
  return 0;
}

/**
 * @brief Test memory cleanup with redirections
 * @return 0 on success, 1 on failure
 */
static int test_free_pipeline_with_redirections(void) {
  pipeline_t pipeline;
  int result = parse_command("cat < in.txt | grep test > out.txt", &pipeline);

  if (result != 0) {
    fprintf(stderr, "test_free_pipeline_with_redirections: parse failed\n");
    return 1;
  }

  free_pipeline(&pipeline);
  return 0;
}

/**
 * @brief Run all memory management tests
 * @return 0 if all tests pass, 1 if any test fails
 */
int main(void) {
  int failures = 0;

  printf("Running memory management tests...\n");

  failures += test_free_pipeline_null();
  failures += test_free_pipeline_cleanup();
  failures += test_free_pipeline_multiple_commands();
  failures += test_free_pipeline_with_redirections();

  if (failures == 0) {
    printf("All memory management tests passed!\n");
    return 0;
  } else {
    printf("%d test(s) failed\n", failures);
    return 1;
  }
}
