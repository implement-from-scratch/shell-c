# Documentation

This directory contains a complete guide to building a Unix style shell from scratch. The documentation is organized into parts, with each chapter building on previous concepts.

## Table of Contents

### Part 1. Shell from 0 to 1

1. [Introduction](01_introduction.md) - What is a shell and why build one from scratch
2. [Process Execution](02_process_execution.md) - Understanding fork, exec, and wait
3. [Command Parsing](03_command_parsing.md) - Tokenizing and parsing user input
4. [I/O Redirection](04_io_redirection.md) - Redirecting input and output to files
5. [Pipeline Execution](05_pipeline_execution.md) - Connecting processes with pipes
6. [Signal Handling](06_signal_handling.md) - Handling Ctrl+C and other signals
7. [REPL Loop](07_repl_loop.md) - The read eval print loop that drives the shell

### Part 2. Advanced Topics

1. [File Descriptor Management](08_file_descriptor_management.md) - Deep dive into file descriptor duplication
2. [Process Groups](09_process_groups.md) - Process groups and job control
3. [Error Handling](10_error_handling.md) - Comprehensive error handling strategies
4. [Memory Management](11_memory_management.md) - Allocation, deallocation, and avoiding leaks

### Additional Technical Documentation

- [Process Model](process_model.md) - Detailed explanation of file descriptor duplication during pipeline execution, including memory layout diagrams and step by step walkthroughs

## How to Read This Documentation

Start with Part 1 and read sequentially. Each chapter assumes you understand the previous chapters. The code examples reference actual implementation files, so you can follow along with the source code.

If you're already familiar with Unix system programming, you can jump to specific chapters, but the documentation is written assuming minimal prior knowledge. Each chapter explains concepts from the ground up, building understanding step by step.

## Writing Style

The documentation follows a conversational but technically precise style. It assumes the reader is learning these concepts for the first time, explaining not just what the code does but why it works that way. Code references use the format `startLine:endLine:filepath` to link explanations directly to the implementation.
