/* vi: set sw=4 ts=4: */
/*
 * Mini run-parts implementation for busybox
 *
 *
 * Copyright (C) 2001 by Emanuele Aina <emanuele.aina@tiscali.it>
 *
 * Based on the Debian run-parts program, version 1.15
 *   Copyright (C) 1996 Jeff Noxon <jeff@router.patch.net>,
 *   Copyright (C) 1996-1999 Guy Maor <maor@debian.org>
 *   
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA
 *
 */

/* This is my first attempt to write a program in C (well, this is my first
 * attempt to write a program! :-) . */

/* This piece of code is heavily based on the original version of run-parts,
 * taken from debian-utils. I've only removed the long options and a the 
 * report mode. As the original run-parts support only long options, I've
 * broken compatibility because the BusyBox policy doesn't allow them. 
 * The supported options are: 
 * -t			test. Print the name of the files to be executed, without
 * 				execute them.
 * -a ARG		argument. Pass ARG as an argument the program executed. It can 
 * 				be repeated to pass multiple arguments.
 * -u MASK 		umask. Set the umask of the program executed to MASK. */

/* TODO 
 * done - convert calls to error in perror... and remove error()
 * done - convert malloc/realloc to their x... counterparts 
 * done - remove catch_sigchld
 * done - use bb's concat_path_file() 
 * done - declare run_parts_main() as extern and any other function as static?
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>

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
	int entries;
	int i;
	int exitstatus = 0;

	/* scandir() isn't POSIX, but it makes things easy. */
	entries = scandir(args[0], &namelist, valid_name, alphasort);

	if (entries == -1) {
		perror_msg_and_die("failed to open directory %s", args[0]);
	}

	for (i = 0; i < entries; i++) {

		filename = concat_path_file(args[0], namelist[i]->d_name);

		if (stat(filename, &st) < 0) {
			perror_msg_and_die("failed to stat component %s", filename);
		}
		if (S_ISREG(st.st_mode) && !access(filename, X_OK)) {
			if (test_mode)
				printf("run-parts would run %s\n", filename);
			else {
				int result;
				int pid;

				if ((pid = fork()) < 0) {
					perror_msg_and_die("failed to fork");
				} else if (!pid) {
					execv(args[0], args);
					perror_msg_and_die("failed to exec %s", args[0]);
				}

				waitpid(pid, &result, 0);

				if (WIFEXITED(result) && WEXITSTATUS(result)) {
					perror_msg("%s exited with return code %d", args[0], WEXITSTATUS(result));
					exitstatus = 1;
				} else if (WIFSIGNALED(result)) {
					perror_msg("%s exited because of uncaught signal %d", args[0], WTERMSIG(result));
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
