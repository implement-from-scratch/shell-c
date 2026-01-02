/**
 * @file shell.c
 * @brief Main shell implementation with process control and I/O redirection
 */

#include "shell.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t g_interrupted = 0;
static pid_t g_foreground_pgid = 0;

/**
 * @brief Signal handler for SIGINT (Ctrl+C)
 * @param sig Signal number
 */
static void sigint_handler(int sig) {
  (void)sig;
  g_interrupted = 1;
  if (g_foreground_pgid > 0) {
    kill(-g_foreground_pgid, SIGINT);
  }
}

/**
 * @brief Setup signal handlers for the shell
 */
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

/**
 * @brief Setup input redirection for a command
 * @param input_file File path for input redirection
 * @return File descriptor, or -1 on error
 */
static int setup_input_redirection(const char *input_file) {
  if (!input_file)
    return -1;

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

/**
 * @brief Setup output redirection for a command
 * @param output_file File path for output redirection
 * @param append Whether to append or truncate
 * @return 0 on success, -1 on error
 */
static int setup_output_redirection(const char *output_file, bool append) {
  if (!output_file)
    return -1;

  int flags = O_WRONLY | O_CREAT;
  if (append) {
    flags |= O_APPEND;
  } else {
    flags |= O_TRUNC;
  }

  int fd = open(output_file, flags, 0644);
  if (fd == -1) {
    perror(output_file);
    return -1;
  }

  if (dup2(fd, STDOUT_FILENO) == -1) {
    perror("dup2");
    close(fd);
    return -1;
  }

  close(fd);
  return 0;
}

/**
 * @brief Execute a single command with redirection
 * @param cmd Command structure
 * @param input_fd Input file descriptor (for pipes)
 * @param output_fd Output file descriptor (for pipes)
 * @param is_first Whether this is the first command in pipeline
 * @param is_last Whether this is the last command in pipeline
 * @return Process ID on success, -1 on error
 */
static pid_t execute_command(const command_t *cmd, int input_fd, int output_fd,
                             bool is_first, bool is_last) {
  if (!cmd || !cmd->argv || !cmd->argv[0])
    return -1;

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
    return -1;
  }

  if (pid == 0) {
    // Child process
    // Set process group for signal handling
    if (!cmd->background) {
      setpgid(0, 0);
    }

    // Setup pipe input (if not first command)
    if (!is_first && input_fd != -1) {
      if (dup2(input_fd, STDIN_FILENO) == -1) {
        perror("dup2 input");
        exit(1);
      }
      close(input_fd);
    }

    // Setup pipe output (if not last command)
    if (!is_last && output_fd != -1) {
      if (dup2(output_fd, STDOUT_FILENO) == -1) {
        perror("dup2 output");
        exit(1);
      }
      close(output_fd);
    }

    // Setup input redirection (overrides pipe input)
    if (cmd->input_file) {
      if (setup_input_redirection(cmd->input_file) == -1) {
        exit(1);
      }
    }

    // Setup output redirection (overrides pipe output)
    if (cmd->output_file) {
      if (setup_output_redirection(cmd->output_file, cmd->append_output) ==
          -1) {
        exit(1);
      }
    }

    // Execute command
    execvp(cmd->argv[0], cmd->argv);
    perror(cmd->argv[0]);
    exit(127); // Command not found
  }

  // Parent process
  return pid;
}

/**
 * @brief Execute a pipeline of commands
 * @param pipeline Pipeline structure to execute
 * @return Exit status of the last command in pipeline
 */
int execute_pipeline(pipeline_t *pipeline) {
  if (!pipeline || pipeline->num_commands == 0)
    return 0;

  size_t num_cmds = pipeline->num_commands;
  pid_t *pids = calloc(num_cmds, sizeof(pid_t));
  int (*pipe_fds)[2] = calloc(num_cmds - 1, sizeof(int[2]));

  if (!pids || (!pipe_fds && num_cmds > 1)) {
    free(pids);
    free(pipe_fds);
    return 1;
  }

  // Create pipes for multi-command pipelines
  for (size_t i = 0; i < num_cmds - 1; i++) {
    if (pipe(pipe_fds[i]) == -1) {
      perror("pipe");
      free(pids);
      free(pipe_fds);
      return 1;
    }
  }

  // Execute commands
  for (size_t i = 0; i < num_cmds; i++) {
    int input_fd = (i > 0) ? pipe_fds[i - 1][0] : -1;
    int output_fd = (i < num_cmds - 1) ? pipe_fds[i][1] : -1;

    bool is_first = (i == 0);
    bool is_last = (i == num_cmds - 1);

    pid_t pid = execute_command(&pipeline->commands[i], input_fd, output_fd,
                                is_first, is_last);
    if (pid == -1) {
      // Cleanup on error
      for (size_t j = 0; j < i; j++) {
        waitpid(pids[j], NULL, 0);
      }
      free(pids);
      free(pipe_fds);
      return 1;
    }

    pids[i] = pid;

    // Close pipe file descriptors in parent
    if (input_fd != -1)
      close(input_fd);
    if (output_fd != -1)
      close(output_fd);

    // Set foreground process group for signal handling
    if (!pipeline->commands[i].background && is_first) {
      g_foreground_pgid = pid;
      setpgid(pid, pid);
    }
  }

  // Wait for all processes
  int exit_status = 0;
  bool background = pipeline->commands[num_cmds - 1].background;

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
  } else {
    // Background execution - don't wait, just print PID
    printf("[%d]\n", (int)pids[num_cmds - 1]);
    g_foreground_pgid = 0;
  }

  free(pids);
  free(pipe_fds);
  return exit_status;
}

/**
 * @brief Main shell REPL loop
 * @return Exit status
 */
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
    if (strlen(line) == 0)
      continue;

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

/**
 * @brief Main entry point
 */
int main(void) { return shell_main(); }
