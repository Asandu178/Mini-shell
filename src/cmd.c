// SPDX-License-Identifier: BSD-3-Clause
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <stdlib.h>
#include <stdio.h>
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

static bool shell_cd(word_t *dir)
{
	char *path = NULL;
	int malloced = 0;
	if (dir && strchr(dir->string, '~') == NULL) {
		path = get_word(dir);
		malloced = 1;
	} else {
		// this is just a ref
		path = getenv("HOME");
	}
	int r = chdir(path);
	if (r == -1) {
		if (malloced)
			free(path);
		return 1;
	}
	else  {
		if (malloced)
			free(path);
		return 0;
	}
}

/**
 * Internal exit/quit command.
 */
static int shell_exit(void)
{
	/* TODO: Execute exit/quit. */


	return SHELL_EXIT; /* TODO: Replace with actual exit code. */
}

void handleRedirects(simple_command_t *s, int builtin) {
	char *in = get_word(s->in);
	char *out = get_word(s->out);
	char *err = get_word(s->err);
	if (in) {
			int fd = open(in, O_RDONLY);
			dup2(fd, STDIN_FILENO);
			close(fd);
		}
	if (out && err && !strcmp(out, err)) {
		int flags = O_WRONLY | O_CREAT;
		if (s->io_flags & IO_OUT_APPEND || s->io_flags & IO_ERR_APPEND)
			flags |= O_APPEND;
		else
			flags |= O_TRUNC;
		int fd = open(out, flags, 0644);
		if (!builtin) {
			dup2(fd, STDERR_FILENO);
			dup2(fd, STDOUT_FILENO);
		}
		close(fd);
	} else {
		if (out) {
			int flags = O_WRONLY | O_CREAT;

			if (s->io_flags & IO_OUT_APPEND)
				flags |= O_APPEND;
			else
				flags |= O_TRUNC;

			int fd = open(out, flags, 0644);
			if (!builtin)
				dup2(fd, STDOUT_FILENO);
			close(fd);
		}
		if (err) {
			int flags = O_WRONLY | O_CREAT;

			if (s->io_flags & IO_ERR_APPEND)
				flags |= O_APPEND;
			else
				flags |= O_TRUNC;

			int fd = open(err, flags, 0644);
			if (!builtin)
				dup2(fd, STDERR_FILENO);
			close(fd);
		}
	}

	for (int i = 3 ; i < 1024 ; i++)
		close(i);
	if (in)
		free(in);
	if (out)
		free(out);
	if (err)
		free(err);
}

/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	/* TODO: Sanity checks. */

	char *command = get_word(s->verb);

	int r = 0;

	if (!strcmp(command, "cd")) {
		handleRedirects(s, true);
		r = shell_cd(s->params);
		free(command);
		return r;
	}

	if (!strcmp(command, "exit") || !(strcmp(command, "quit"))) {
		r = shell_exit();
		free(command);
		return r;
	}

	if (strchr(command, '=') != NULL) {
		char *save;
		char *copy = malloc(200);
		strcpy(copy, command);
		char *key = strtok(copy, "=");
		char *value = strtok(NULL, "=");

		r = setenv(key, value, 1);
		if (r == -1)
			printf("eroare frate\n");
		free(command);
		free(copy);
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
	char **args = get_argv(s, &argc);

	pid_t p = fork();
	int status = 0;


	switch (p)
	{
		case -1:
			return -1;
			break;
	
		case 0:
			handleRedirects(s, false);
			execvp(args[0], args);
			break;
		default:
			waitpid(p, &status, 0);
	}

	for (int i = 0 ; i < argc ; i++)
		free(args[i]);
	free(args);
	free(command);


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
			r = parse_command(cmd1, level + 1, father);
			exit(r);
			break;
		default:
			;
	}

	pid_t p2 = fork();

	switch (p2) {
		case -1:
			return -1;
			break;
	
		case 0:
			r = parse_command(cmd2, level + 1, father);
			exit(r);
			break;
		default:
			;
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

	DIE(pipe(fd) == -1, "Pipe error\n");

	pid_t p1 = fork();

	switch (p1) {
		case -1:
			return -1;
			break;
	
		case 0:
			close(fd[READ]);
			dup2(fd[WRITE], STDOUT_FILENO);
			close(fd[WRITE]);
			r = parse_command(cmd1, level + 1, father);
			exit(r);
			break;
		default:
			;
	}

	pid_t p2 = fork();

	switch (p2) {
		case -1:
			return -1;
			break;
	
		case 0:
			close(fd[WRITE]);
			dup2(fd[READ], STDIN_FILENO);
			close(fd[READ]);
			r = parse_command(cmd2, level + 1, father);
			exit(r);
			break;
		default:
			;
	}

	close(fd[READ]);
	close(fd[WRITE]);

	waitpid(p1, NULL, 0);
	waitpid(p2, &status, 0);



	return status; /* TODO: Replace with actual exit status. */
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
		r = run_on_pipe(c->cmd1, c->cmd2, level, c);
		break;

	default:
		return SHELL_EXIT;
	}

	return r;
}
