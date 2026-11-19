# Minishell

## Overview

This project was originally developed as a homework assignment for the Operating Systems course. I decided to make the repository public to showcase some of the concepts I worked with, particularly process management and system calls.

The goal of the assignment was to implement a minimal Bash-like shell capable of executing commands, handling input/output redirection, and supporting different command operators.

## Implementation

Working on this project helped me understand several concepts related to how shells work, such as:

- Creating child processes using `fork()` and executing programs using `exec()`.
- Implementing built-in commands such as `cd`, `pwd`, and `exit`.
- Handling environment variables and command arguments.
- Implementing command chaining using sequential (`;`), parallel (`&`), and conditional (`&&`, `||`) operators.
- Connecting commands using pipes (`|`) and redirecting input and output using file descriptors.
- Using system calls such as `pipe()`, `dup2()`, `open()`, and `close()` to manage process I/O.

This assignment gave me practical experience with process creation, inter-process communication, and the way a shell interacts with the operating system.