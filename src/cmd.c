// SPDX-License-Identifier: BSD-3-Clause
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "cmd.h"
#include "utils.h"

#define READ		0
#define WRITE		1

/**
 * Internal change-directory command.
 */
extern char *path = NULL;

int checkPath(char *path) {
	int fd = open(path, O_PATH);
	if (fd > 0)
		return 0;
	return -1;
}

int changePath(char *path) {
	int fd = open(path, O_PATH);
	if (fd > 0) {
		chdir(path);
		return 0;
	}
	return -1;
}

static bool shell_cd(word_t *dir)
{
	/* TODO: Execute cd. */
	// set up home as starting path or go back home if we input "cd" or "cd ~"

	if (!path || !dir || !strcmp(dir->string, "~")) {
		strcpy(path, getenv("HOME"));
		chdir(path);
		return true;
	}

	char newpath[pathSize] = "";
	if (dir->string[0] != '/') {
 		strcpy(newpath, path);
		strcat(newpath, "/");
		strcat(newpath, dir->string);
	} else 
		strcat(newpath, dir->string);

	if (changePath(newpath) == 0) {
		strcat(path, "/");
		strcat(path, dir->string);
	}

	// get current path
	getcwd(path, pathSize);

	return 0;
}

/**
 * Internal exit/quit command.
 */
static int shell_exit(void)
{
	/* TODO: Execute exit/quit. */


	return SHELL_EXIT; /* TODO: Replace with actual exit code. */
}

/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	/* TODO: Sanity checks. */

	int r = 0;

	if (!strcmp(s->verb->string, "cd")) {
		r = shell_cd(s->params);
		return r;
	}

	if (!strcmp(s->verb->string, "exit") || !(strcmp(s->verb->string, "quit"))) {
		r = shell_exit();
		return r;
	}

	/* TODO: If variable assignment, execute the assignment and return
	 * the exit status.
	 */

	/* TODO: If external command:
	 *   1. Fork new process
	 *     2c. Perform redirections in child
	 *     3c. Load executable in child
	 *   2. Wait for child
	 *   3. Return exit status
	 */

	int argc = 0;
	char **args = malloc(sizeof(char*) * maxArgc);

	args[argc++] = s->verb->string;

	for (word_t *params = s->params ; params != NULL ; params = params->next_word)
		args[argc++] = params->string;

	args[argc] = NULL;



	pid_t p = fork();
	int status = 0;

	fflush(0);

	switch (p)
	{
		case -1:
			return -1;
			break;
	
		case 0:
			if (s->in) {
				int fd = open(s->in->string, O_RDONLY);
				dup2(fd, STDIN_FILENO);
				close(fd);
			}
			if (s->out) {

				int flags = O_RDWR | O_CREAT;

				if (s->io_flags & IO_OUT_APPEND)
					flags |= O_APPEND;
				else
					flags |= O_TRUNC;

				int fd = open(s->out->string, flags, 0644);
				dup2(fd, STDOUT_FILENO);
				close(fd);
			}
			if (s->err) {

				int flags = O_RDWR | O_CREAT;

				if (s->io_flags & IO_ERR_APPEND)
					flags |= O_APPEND;
				else
					flags |= O_TRUNC;

				int fd = open(s->err->string, flags, 0644);
				dup2(fd, STDERR_FILENO);
				close(fd);
			}
			execvp(args[0], args);
			break;
		default:
			waitpid(p, &status, 0);
				free(args);
	}


	return status;
}

/**
 * Process two commands in parallel, by creating two children.
 */
static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	int r = 0;
	int status1, status2;
	pid_t p1 = fork();

	switch (p1) {
		case -1:
			return -1;
			break;
	
		case 0:
			r = parse_command(cmd1, level, father);
			exit(r);
			break;
		default:
	}

	pid_t p2 = fork();

	switch (p2) {
		case -1:
			return -1;
			break;
	
		case 0:
			r = parse_command(cmd2, level, father);
			exit(r);
			break;
		default:
	}

	waitpid(p1, &status1, 0);
	waitpid(p2, &status2, 0);


	return status1 && status2;
}

/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2).
 */
static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	/* TODO: Redirect the output of cmd1 to the input of cmd2. */

	int r = 0;
	int status;
	int fd[2];

	// error creating pipe
	if (pipe(fd) == -1)
		return false;

	pid_t p1 = fork();

		switch (p1) {
		case -1:
			return -1;
			break;
	
		case 0:
			r = parse_command(cmd1, level, father);
			exit(r);
			break;
		default:
		// wait for the child process to finish
		waitpid(p1, NULL, 0);
	}

	pid_t p2 = fork();

	switch (p2) {
		case -1:
			return -1;
			break;
	
		case 0:
			r = parse_command(cmd2, level, father);
			exit(r);
			break;
		default:
	}

	return true; /* TODO: Replace with actual exit status. */
}

/**
 * Parse and execute a command.
 */
int parse_command(command_t *c, int level, command_t *father)
{
	/* TODO: sanity checks */
	if (c == NULL)
		return 0;
	int r = 0;

	if (c->op == OP_NONE) {

		r = parse_simple(c->scmd, 0, c);

		return r;
	}

	switch (c->op) {
	case OP_SEQUENTIAL:

		if (c->cmd1)
			r = parse_command(c->cmd1, level, c);
		if (c->cmd2)
			r = parse_command(c->cmd2, level, c);
		break;

	case OP_PARALLEL:

		run_in_parallel(c->cmd1, c->cmd2, level, c);
		break;

	case OP_CONDITIONAL_NZERO:

		r = parse_command(c->cmd1, level, c);
		if (r)
			r = parse_command(c->cmd2, level, c);
		break;

	case OP_CONDITIONAL_ZERO:

		r = parse_command(c->cmd1, level, c);
		if (!r)
			r = parse_command(c->cmd2, level, c);
		break;

	case OP_PIPE:
		/* TODO: Redirect the output of the first command to the
		 * input of the second.
		 */
		break;

	default:
		return SHELL_EXIT;
	}

	return r;
}
