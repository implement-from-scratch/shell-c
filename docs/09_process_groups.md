# Process Groups

Process groups are a Unix mechanism for organizing related processes. They're essential for signal delivery and job control. When you press Ctrl+C, the terminal sends a signal to the entire foreground process group, not just one process. This chapter explains how process groups work and how the shell uses them.

## What is a Process Group

A process group is a collection of related processes that share a common process group ID (PGID). Processes in the same group can receive signals together. This is useful for pipelines: when you interrupt a pipeline, you want all processes in it to stop, not just one.

Each process belongs to exactly one process group. The process group ID is just a number, typically the process ID of the group leader (the first process in the group). Process groups are independent of the process hierarchy: a parent and child can be in different groups.

The kernel uses process groups to determine which processes should receive terminal generated signals. When you press Ctrl+C, the terminal driver sends SIGINT to the foreground process group. All processes in that group receive the signal.

## Creating Process Groups

The shell creates a new process group for each foreground pipeline. The first process in the pipeline becomes the group leader:

```227:231:src/shell.c
        // Set foreground process group for signal handling
        if (!pipeline->commands[i].background && is_first) {
            g_foreground_pgid = pid;
            setpgid(pid, pid);
        }
```

The `setpgid(pid, pid)` call sets the process group ID of the process `pid` to `pid`, making it the leader of a new group. The first argument is the process ID, and the second is the desired group ID. By setting both to the same value, we make that process the leader.

This must be done carefully. There's a race condition: if the parent sets the group before the child calls `setpgid()`, they might conflict. The proper way is to call `setpgid()` in both parent and child, but the current implementation only does it in the parent for simplicity.

## Process Group Inheritance

When a process forks, the child initially belongs to the same process group as the parent. This is why the shell must explicitly create new groups. Without `setpgid()`, all child processes would remain in the shell's process group, and Ctrl+C would kill the shell itself.

After forking, the child should also call `setpgid()` to join the new group. This ensures the group is set up correctly even if there's a race between parent and child. The current implementation doesn't do this in the child, which works but isn't ideal.

## Foreground vs Background Groups

Foreground process groups receive terminal generated signals. Background groups do not. This is how the shell protects background jobs from Ctrl+C.

The shell only creates process groups for foreground commands:

```127:129:src/shell.c
        // Set process group for signal handling
        if (!cmd->background) {
            setpgid(0, 0);
        }
```

Background commands don't get their own process group in the current implementation. They remain in the shell's group, but since they're not the foreground group, they won't receive terminal signals.

A more complete implementation would move background processes to their own groups and track them separately. This would enable job control features like bringing background jobs to the foreground.

## Signal Delivery to Process Groups

When the user presses Ctrl+C, the terminal sends SIGINT to the foreground process group. The shell's signal handler then forwards this signal:

```24:30:src/shell.c
static void sigint_handler(int sig) {
    (void)sig;
    g_interrupted = 1;
    if (g_foreground_pgid > 0) {
        kill(-g_foreground_pgid, SIGINT);
    }
}
```

The `kill(-g_foreground_pgid, SIGINT)` call sends SIGINT to all processes in the group. The negative process group ID tells `kill()` to send to the entire group, not just one process.

This ensures that all processes in a pipeline receive the interrupt signal. If only one process received it, the others would continue running, which is not the desired behavior.

## Process Group Lifecycle

Process groups are created when a foreground pipeline starts and destroyed when all processes in the group exit. The kernel automatically cleans up empty process groups.

The shell tracks the foreground group ID in `g_foreground_pgid`. This is set when the first process in a foreground pipeline is created and cleared when the pipeline completes:

```255:260:src/shell.c
        g_foreground_pgid = 0;
    } else {
        // Background execution - don't wait, just print PID
        printf("[%d]\n", (int)pids[num_cmds - 1]);
        g_foreground_pgid = 0;
    }
```

After waiting for foreground processes or starting background ones, the shell clears the foreground group ID. This ensures the signal handler doesn't try to signal a group that no longer exists.

## Job Control Concepts

Full job control would require more sophisticated process group management. A job is a pipeline or command that can be managed as a unit. Jobs can be:
- Brought to the foreground
- Sent to the background
- Suspended and resumed
- Listed and tracked

The current implementation doesn't provide job control, but process groups are the foundation for it. To implement job control, you'd need to:
- Track all process groups (foreground and background)
- Maintain a job table
- Implement `fg` and `bg` built-in commands
- Handle SIGTSTP (Ctrl+Z) to suspend jobs
- Manage terminal ownership

This is beyond the scope of the basic shell, but understanding process groups is the first step toward implementing these features.

## Terminal Control

Process groups are also used for terminal control. The terminal has a notion of the foreground process group, which is the group that currently "owns" the terminal. Only the foreground group can read from the terminal.

When a process group becomes foreground, it receives SIGCONT if it was stopped. When it loses foreground status, it receives SIGHUP if the terminal disconnects. The shell doesn't implement these features, but they rely on process groups.

## Race Conditions

There's a subtle race condition in process group setup. The parent calls `setpgid()` on the child, but the child might also need to call it. If they both call it at the same time, they might conflict.

The standard solution is to call `setpgid()` in both parent and child, but use the child's actual PID (which is 0 in the child). This ensures they're both trying to set the same group ID, and the kernel handles any conflicts.

The current implementation avoids this by only calling `setpgid()` in the parent. This works because the parent waits a bit before the child starts executing, but it's not ideal. A more robust implementation would call it in both places.

## Process Group IDs

Process group IDs are just numbers. They're typically the process ID of the group leader, but this isn't required. Any process ID can be used as a group ID, as long as a process with that ID exists or existed.

When the group leader exits, the group ID doesn't change. The group continues to exist as long as any process remains in it. This allows processes to outlive their group leader, which is useful for daemons and background jobs.

## Why Process Groups Matter

Process groups enable coordinated signal delivery. Without them, you'd have to send signals to each process individually, and you'd risk missing some or sending to the wrong ones. With process groups, one signal goes to all processes in the group automatically.

They also enable job control, which is a powerful feature for managing long running tasks. Even though the current shell doesn't implement full job control, the process group infrastructure is in place.

Process groups are one of those Unix features that seem simple but enable complex behaviors. They're the foundation for signal delivery, job control, and terminal management. Understanding them is essential for building robust shell implementations.
