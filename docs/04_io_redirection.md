# I/O Redirection

By default, programs read from standard input (stdin) and write to standard output (stdout). These are usually connected to the terminal, so when you type a command, it reads from your keyboard and prints to your screen.

Redirection changes this. Instead of reading from the terminal, a command can read from a file. Instead of writing to the terminal, it can write to a file. This is one of the most powerful features of Unix shells.

## Understanding File Descriptors

To understand redirection, you need to understand file descriptors. A file descriptor is a small integer that represents an open file or I/O channel. The operating system uses file descriptors to track what files a process has open.

Every process starts with three standard file descriptors:
- 0: standard input (stdin)
- 1: standard output (stdout)  
- 2: standard error (stderr)

When you call `printf()`, it writes to file descriptor 1. When you call `scanf()`, it reads from file descriptor 0. These are just numbers that the kernel uses to look up the actual file or device.

Redirection works by changing what these file descriptors point to. Instead of pointing to the terminal, they point to files. The program doesn't know this happened. It still reads from descriptor 0 and writes to descriptor 1, but now those descriptors are connected to files instead of the terminal.

## Input Redirection

Input redirection makes a command read from a file instead of the terminal. The `<` operator tells the shell to open a file and connect it to stdin.

Here's how the shell implements this:

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

The function first opens the file using `open()`. The `O_RDONLY` flag means open for reading only. If the file doesn't exist or can't be opened, `open()` returns -1 and the function prints an error.

After opening, the shell uses `dup2()` to duplicate the file descriptor. `dup2(fd, STDIN_FILENO)` makes file descriptor 0 (stdin) refer to the same file as `fd`. After this call, reading from stdin reads from the file.

The shell then closes the original file descriptor `fd`. This is important because file descriptors are a limited resource. Each process can only have so many open at once (typically 1024). By closing `fd`, we free that descriptor number while keeping the file open through stdin.

## The dup2 System Call

`dup2()` is the key to redirection. It takes two file descriptor numbers and makes the second one refer to whatever the first one refers to. If the second descriptor was already open, `dup2()` closes it first.

This is atomic: the operation either completely succeeds or completely fails. There's no intermediate state where one descriptor is closed but the other isn't set up yet.

After `dup2(fd, STDIN_FILENO)`, both `fd` and `STDIN_FILENO` refer to the same file. They're independent descriptors (you can close one without affecting the other), but they both access the same underlying file object in the kernel.

## Output Redirection

Output redirection works similarly but with different file open flags. The `>` operator truncates the file (empties it if it exists), and `>>` appends to it.

```79:103:src/shell.c
static int setup_output_redirection(const char *output_file, bool append) {
    if (!output_file) return -1;

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
```

The `O_WRONLY` flag opens the file for writing only. `O_CREAT` creates the file if it doesn't exist. The third argument to `open()` (0644) sets the file permissions: the owner can read and write, others can only read.

For truncation mode (`>`), the `O_TRUNC` flag clears the file's contents if it already exists. For append mode (`>>`), the `O_APPEND` flag positions the file pointer at the end, so new writes go after existing content.

After opening and duplicating, the process works the same way: the original descriptor is closed, and stdout now points to the file.

## When Redirection Happens

Redirection setup happens in the child process after forking but before executing the command:

```149:161:src/shell.c
        // Setup input redirection (overrides pipe input)
        if (cmd->input_file) {
            if (setup_input_redirection(cmd->input_file) == -1) {
                exit(1);
            }
        }

        // Setup output redirection (overrides pipe output)
        if (cmd->output_file) {
            if (setup_output_redirection(cmd->output_file, cmd->append_output) == -1) {
                exit(1);
            }
        }
```

This is important. The redirection happens in the child, so it only affects that one command. The parent shell's file descriptors are unchanged. After the child exits, the parent's stdin and stdout are still connected to the terminal.

## Redirection Precedence

When both pipes and redirection are present, redirection takes precedence. The shell sets up pipes first, then redirection overwrites them.

Consider the command `cmd1 | cmd2 < file.txt`. The `cmd2` process first sets up pipe input on stdin (from `cmd1`'s output). Then the redirection setup runs and overwrites stdin with the file. The pipe connection is lost, and `cmd2` reads from the file instead.

This matches standard shell behavior. Explicit redirection always overrides implicit pipe connections. If you want both, you need to structure the command differently.

## Error Handling

If file opening fails, the child process prints an error and exits. The error message comes from the child, not the parent shell. This is why you see messages like "No such file or directory" even though the shell didn't explicitly print them.

The child exits with status 1 on redirection errors. This is somewhat arbitrary. The important thing is that the child exits with a non zero status so the parent knows something went wrong.

## File Descriptor Lifecycle

File descriptors opened for redirection remain open for the lifetime of the command. When the command exits, the kernel automatically closes all open file descriptors. The shell doesn't need to explicitly close them in the parent, because the child's descriptors are closed when the child terminates.

However, the shell does close pipe file descriptors in the parent after forking. This is different from redirection file descriptors. Pipes need to be closed in the parent so the pipe sees EOF when the writer closes. Redirection files don't have this requirement because they're not shared between processes.

## Combining Redirection with Pipelines

The shell supports combining redirection with pipelines. For example, `cmd1 < input.txt | cmd2 > output.txt` works correctly. The first command reads from a file, the second writes to a file, and they're connected by a pipe in the middle.

Each command can have its own independent redirections. This flexibility is what makes Unix shells powerful. You can compose simple commands with pipes and redirections to create complex data processing workflows.

## Stderr Redirection

The current implementation doesn't support stderr redirection (the `2>` operator). Adding it would follow the same pattern: open the file, duplicate to `STDERR_FILENO` (file descriptor 2), and close the original descriptor.

The parser would need to recognize `2>` and `2>>` tokens, and the command structure would need a field for stderr redirection. The execution code would check this field and set up stderr redirection similar to stdout redirection.

Stderr redirection is useful for separating error output from normal output. Many commands write errors to stderr and normal output to stdout, so you can redirect them independently. For example, `cmd > output.txt 2> errors.txt` sends normal output to one file and errors to another.

## Why Redirection Works

Redirection works because programs use file descriptors, not hardcoded device names. A program doesn't say "write to the terminal". It says "write to file descriptor 1". The shell can change what descriptor 1 points to, and the program never knows.

This is the Unix philosophy: programs should be composable. They read from stdin and write to stdout, and the shell (or user) decides where those descriptors point. This allows simple programs to be combined in powerful ways without the programs needing to know about the composition.
