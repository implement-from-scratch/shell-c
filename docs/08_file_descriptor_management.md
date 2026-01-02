# File Descriptor Management

File descriptors are the foundation of I/O in Unix systems. Understanding how they work, how they're duplicated, and how they're managed is essential for building robust shell implementations. This chapter dives deep into the mechanics of file descriptor manipulation.

## What is a File Descriptor

A file descriptor is a small non negative integer that represents an open file, pipe, socket, or device. The kernel maintains a per process table that maps descriptor numbers to actual file objects. When you call `open()`, `pipe()`, or `socket()`, the kernel allocates a file descriptor and returns its number.

File descriptors are process local. Each process has its own descriptor table. Descriptor 0 in one process is completely independent from descriptor 0 in another process, even though they might both refer to stdin.

The kernel uses file descriptors internally to track open files. When you read or write, you specify a file descriptor number, and the kernel looks it up in the process's descriptor table to find the actual file object.

## The File Descriptor Table

Each process has a file descriptor table maintained by the kernel. This table maps descriptor numbers to file objects. When a process starts, it typically has three descriptors already open:
- 0: standard input
- 1: standard output
- 2: standard error

These are inherited from the parent process. When the shell forks, the child inherits the parent's entire descriptor table. This is why pipes created in the parent are available in the child.

The descriptor table has a fixed size, typically 1024 entries on modern systems. This limit can be changed, but it's a finite resource that must be managed carefully.

## Descriptor Duplication with dup2

The `dup2()` system call is the primary tool for file descriptor management in the shell. It duplicates a file descriptor, making a second descriptor number refer to the same underlying file object.

Here's how the shell uses it for pipe setup:

```131:147:src/shell.c
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
```

The `dup2(oldfd, newfd)` call does two things atomically:
1. If `newfd` is already open, it closes it first
2. It makes `newfd` refer to whatever `oldfd` refers to

After this call, both descriptors refer to the same file object. They share the same file offset, so reading from one advances the offset for both. They're independent in that closing one doesn't close the other, but they access the same underlying object.

## Reference Counting

The kernel maintains a reference count for each open file object. This count tracks how many file descriptors point to it. When the count reaches zero, the kernel can free the file object and close the underlying file.

Here's how reference counts change:
- `open()` or `pipe()` creates a file object with count 1
- `fork()` duplicates all descriptors, incrementing counts
- `dup2()` creates a new descriptor pointing to the same object, incrementing the count
- `close()` decrements the count

This is why closing descriptors matters. If you don't close a descriptor, the reference count never reaches zero, and the file object persists even after the process exits. This is how file descriptor leaks happen.

## Descriptor Lifecycle in Pipelines

Let's trace through a pipeline to see how descriptors are managed. For `cmd1 | cmd2`:

1. Parent creates pipe: `pipe[0]` and `pipe[1]` both have count 1 in parent
2. Parent forks child1: both descriptors inherited, counts become 2 each
3. Child1 calls `dup2(pipe[1], STDOUT_FILENO)`: `pipe[1]` count becomes 3 (parent, child1 original, child1 stdout)
4. Child1 closes `pipe[1]`: count becomes 2
5. Parent closes `pipe[1]`: count becomes 1 (only child1's stdout remains)
6. Parent forks child2: `pipe[0]` inherited, count becomes 2
7. Child2 calls `dup2(pipe[0], STDIN_FILENO)`: `pipe[0]` count becomes 3
8. Child2 closes `pipe[0]`: count becomes 2
9. Parent closes `pipe[0]`: count becomes 1 (only child2's stdin remains)

When child1 exits and closes stdout, `pipe[1]`'s count reaches 0 and the write end closes. When child2 reads and sees EOF, it exits and closes stdin, making `pipe[0]`'s count reach 0.

## Why Close After dup2

After calling `dup2()`, the shell immediately closes the original descriptor. This seems redundant since we just duplicated it, but it's necessary for several reasons.

First, it prevents descriptor leaks. Each process has a limited number of descriptors available. If we don't close the original, we're wasting a descriptor slot.

Second, for pipes, closing is essential for proper EOF signaling. A pipe only signals EOF to readers when all writers close their descriptors. If we keep the original descriptor open, the pipe never sees EOF even after the duplicated descriptor closes.

Third, it makes the code clearer. After `dup2()`, we only need the new descriptor. Closing the old one makes this explicit and prevents accidental use of the wrong descriptor.

## Descriptor Ordering Matters

The order in which descriptors are set up matters. The shell sets up pipes first, then redirection. This means redirection can override pipe connections:

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

If we set up redirection first, then pipes, the pipes would override the redirection. By doing pipes first, then redirection, we ensure that explicit redirection takes precedence, which matches standard shell behavior.

## Error Handling in Descriptor Operations

File descriptor operations can fail. `open()` fails if the file doesn't exist or permissions are wrong. `dup2()` fails if the source descriptor is invalid. `close()` can fail if the descriptor is invalid, though this is rare.

The shell handles these errors by checking return values and exiting the child process on failure. The parent will see the exit status when it waits, but the error message comes from the child.

One important detail: `close()` can fail, but we typically ignore its return value. If a descriptor is already closed or invalid, `close()` returns an error, but there's nothing useful we can do about it. The descriptor is closed either way.

## Descriptor Inheritance

When a process forks, the child inherits all open file descriptors. This is both useful and dangerous. It's useful because it allows pipes and redirections to work. It's dangerous because descriptors that should be closed might leak into child processes.

The shell carefully manages which descriptors are open in children. It closes pipe descriptors that aren't needed, and it sets up redirections before executing commands. This ensures children only have the descriptors they actually need.

## Stderr Preservation

One thing the shell doesn't redirect is stderr. Standard error remains connected to the terminal in all cases. This is intentional: error messages should always go to the terminal, not to files or pipes, so users can see them.

If you wanted to redirect stderr, you'd follow the same pattern: open a file, call `dup2()` to duplicate it to `STDERR_FILENO` (descriptor 2), then close the original. The parser would need to recognize `2>` and `2>>` operators, and the command structure would need a field for stderr redirection.

## Descriptor Limits

Each process has a limit on the number of open file descriptors. This limit is typically 1024, but it can be higher on modern systems. The limit exists to prevent processes from consuming too many kernel resources.

The shell doesn't explicitly check this limit, but it manages descriptors carefully to avoid hitting it. Each command should only have a few descriptors open: stdin, stdout, stderr, and maybe a redirection file. Pipes are closed as soon as they're no longer needed.

If you hit the descriptor limit, `open()` or `pipe()` will fail with `EMFILE` (too many open files). The shell would report this error, but in practice, it's rare to hit this limit with normal shell usage.

## Best Practices

Good file descriptor management follows these principles:

1. Close descriptors as soon as they're no longer needed
2. Check return values from descriptor operations
3. Use `dup2()` to redirect standard descriptors
4. Close original descriptors after duplication
5. Set up descriptors in the correct order (pipes before redirection)

The shell follows these practices throughout. Each descriptor is closed at the earliest possible moment, and the order of operations ensures correct behavior.

File descriptor management is one of the trickier aspects of systems programming. Getting it wrong leads to leaks, hangs, and incorrect I/O behavior. But getting it right makes the shell work seamlessly, with pipes and redirections working exactly as users expect.
