/* vi: set sw=4 ts=4: */
/*
 * run command from specified directory
 *
 *
 * Copyright (C) 2001 by Emanuele Aina <emanuele.aina@tiscali.it>
 * rewrite to vfork usage by
 * Copyright (C) 2002 by Vladimir Oleynik <dzo@simtreas.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *
 */


#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#include "libbb.h"

/* valid_name */
/* True or false? Is this a valid filename (upper/lower alpha, digits,
 * underscores, and hyphens only?)
 */
static int valid_name(const struct dirent *d)
{
	char *c = d->d_name;

	while (*c) {
		if (!isalnum(*c) && (*c != '_') && (*c != '-')) {
			return 0;
		}
		++c;
	}
	return 1;
}

/* run_parts */
/* Find the parts to run & call run_part() */
extern int run_parts(char **args, const unsigned char test_mode)
{
	struct dirent **namelist = 0;
	struct stat st;
	char *filename;
	char *arg0 = args[0];
	int entries;
	int i;
	int exitstatus = 0;

#if __GNUC__
	/* Avoid longjmp clobbering */
	(void) &i;
	(void) &exitstatus;
#endif
	/* scandir() isn't POSIX, but it makes things easy. */
	entries = scandir(arg0, &namelist, valid_name, alphasort);

	if (entries == -1) {
		perror_msg_and_die("failed to open directory %s", arg0);
	}

	for (i = 0; i < entries; i++) {

		filename = concat_path_file(arg0, namelist[i]->d_name);

		if (stat(filename, &st) < 0) {
			perror_msg_and_die("failed to stat component %s", filename);
		}
		if (S_ISREG(st.st_mode) && !access(filename, X_OK)) {
			if (test_mode) {
				puts("%s", filename);
			} else {
				/* exec_errno is common vfork variable */
				volatile int exec_errno = 0;
				int result;
				int pid;

				if ((pid = vfork()) < 0) {
					perror_msg_and_die("failed to fork");
				} else if (!pid) {
					args[0] = filename;
					execv(filename, args);
					exec_errno = errno;
					_exit(1);
				}

				waitpid(pid, &result, 0);
				if(exec_errno) {
					errno = exec_errno;
					perror_msg_and_die("failed to exec %s", filename);
				}
				if (WIFEXITED(result) && WEXITSTATUS(result)) {
					perror_msg("%s exited with return code %d", filename, WEXITSTATUS(result));
					exitstatus = 1;
				} else if (WIFSIGNALED(result)) {
					perror_msg("%s exited because of uncaught signal %d", filename, WTERMSIG(result));
					exitstatus = 1;
				}
			}
		} 
		else if (!S_ISDIR(st.st_mode)) {
			error_msg("component %s is not an executable plain file", filename);
			exitstatus = 1;
		}

		free(namelist[i]);
		free(filename);
	}
	free(namelist);
	
	return(exitstatus);
}
