# Introduction

A shell is a program that reads commands from the user and executes them. When you open a terminal and type `ls`, the shell is the program that reads that command, finds the `ls` program on your system, runs it, and displays the output.

Most users interact with shells like bash or zsh without thinking about how they work. But understanding how a shell operates teaches you fundamental concepts about operating systems: process creation, file descriptors, signals, and inter process communication.

## Why Build a Shell from Scratch

Building a shell from scratch forces you to understand these concepts at a deep level. You can't just use high level abstractions. You need to work directly with system calls like `fork()`, `exec()`, and `wait()`. You need to understand how file descriptors work, how pipes connect processes, and how signals interrupt execution.

The shell we'll build is minimal but functional. It supports:
- Executing external programs
- Piping output between commands
- Redirecting input and output to files
- Running commands in the background
- Handling Ctrl+C to interrupt running commands

This covers the core functionality that makes shells useful. Once you understand these concepts, you can extend the shell with features like job control, command history, or tab completion.

## What You'll Learn

This guide teaches you how to build a shell step by step. Each chapter focuses on one aspect:

1. **Process Execution**: How to create new processes and run programs
2. **Command Parsing**: How to break user input into commands and arguments
3. **I/O Redirection**: How to change where programs read from and write to
4. **Pipeline Execution**: How to connect multiple programs together
5. **Signal Handling**: How to respond to interrupts and other signals
6. **REPL Loop**: How to build the interactive loop that reads and executes commands

By the end, you'll have a working shell and a solid understanding of how Unix systems manage processes and I/O.

## Prerequisites

You should be comfortable with C programming. You don't need to be an expert, but you should understand:
- Pointers and memory management
- Structures and arrays
- Basic string manipulation
- Function calls and control flow

You don't need prior experience with system programming. We'll explain system calls as we use them. If you've written C programs that use `malloc()` and `free()`, you have enough background to follow along.

## System Calls We'll Use

A system call is a function that asks the operating system kernel to perform an operation. Unlike regular function calls, system calls cross the boundary between user space and kernel space. The kernel has special privileges that user programs don't have, like creating processes or accessing hardware.

The shell uses several key system calls:
- `fork()`: Creates a copy of the current process
- `exec()`: Replaces the current process with a new program
- `wait()`: Waits for a child process to finish
- `pipe()`: Creates a communication channel between processes
- `dup2()`: Duplicates a file descriptor
- `open()`: Opens a file
- `kill()`: Sends a signal to a process

We'll explain each one in detail as we use it. For now, just know that these are the tools the shell uses to do its job.

## The Shell's Job

At its core, a shell does three things:
1. Read a command from the user
2. Parse the command to understand what to do
3. Execute the command

This happens in a loop. After executing one command, the shell reads the next one. This loop continues until the user exits.

The complexity comes from the details. How do you parse a command like `ls -la | grep test > output.txt`? How do you connect `ls`'s output to `grep`'s input? How do you redirect `grep`'s output to a file? These are the problems we'll solve.

## Code Organization

The shell is organized into three main files:
- `include/shell.h`: Data structures and function declarations
- `src/parser.c`: Command parsing logic
- `src/shell.c`: Process execution and main loop

This separation makes the code easier to understand. The parser handles turning text into data structures. The shell core handles executing those data structures. The header file defines the interface between them.

As we build the shell, we'll add code to these files. Each chapter will explain what code goes where and why.

## Getting Started

To follow along, you'll need:
- A Unix like system (Linux, macOS, or WSL)
- A C compiler (gcc or clang)
- A text editor

You can compile the shell with:
```bash
gcc -Wall -Wextra -std=c11 -Iinclude src/shell.c src/parser.c -o shell
```

Then run it with:
```bash
./shell
```

You'll see a prompt. Type commands and see them execute. The shell is minimal, so don't expect all the features of bash, but it demonstrates the core concepts.

In the next chapter, we'll start building the shell by learning how to execute a simple command.
