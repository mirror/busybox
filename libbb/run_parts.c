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

/* test mode = 1 is the same as offical run_parts
 * test_mode = 2 means to fail siliently on missing directories
 */

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
		if (test_mode & 2) {
			return(2);
		}
		bb_perror_msg_and_die("failed to open directory %s", arg0);
	}

	for (i = 0; i < entries; i++) {

		filename = concat_path_file(arg0, namelist[i]->d_name);

		if (stat(filename, &st) < 0) {
			bb_perror_msg_and_die("failed to stat component %s", filename);
		}
		if (S_ISREG(st.st_mode) && !access(filename, X_OK)) {
			if (test_mode & 1) {
				puts(filename);
			} else {
				pid_t pid, wpid;
				int result;

				if ((pid = vfork()) < 0) {
					bb_perror_msg_and_die("failed to fork");
				} else if (pid==0) {
					execv(filename, args);
					_exit(1);
				}

				/* Wait for the child process to exit.  Since we use vfork
				 * we shouldn't actually have to do any waiting... */
				wpid = wait(&result);
				while (wpid > 0) {
					/* Find out who died, make sure it is the right process */
					if (pid == wpid) {
						if (WIFEXITED(result) && WEXITSTATUS(result)) {
							bb_perror_msg("%s exited with return code %d", filename, WEXITSTATUS(result));
							exitstatus = 1;
						} else if (WIFSIGNALED(result) && WIFSIGNALED(result)) {
							int sig;
							sig = WTERMSIG(result);
							bb_perror_msg("%s exited because of uncaught signal %d (%s)", 
									filename, sig, u_signal_names(0, &sig, 1));
							exitstatus = 1;
						}
						break;
					} else {
						/* Just in case some _other_ random child process exits */
						wpid = wait(&result);
					}
				}
			}
		} 
		else if (!S_ISDIR(st.st_mode)) {
			bb_error_msg("component %s is not an executable plain file", filename);
			exitstatus = 1;
		}

		free(namelist[i]);
		free(filename);
	}
	free(namelist);
	
	return(exitstatus);
}
