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
 * Licensed under GPL v2 or later, see file LICENSE in this tarball for details.
 */

/* This is my first attempt to write a program in C (well, this is my first
 * attempt to write a program! :-) . */

/* This piece of code is heavily based on the original version of run-parts,
 * taken from debian-utils. I've only removed the long options and a the
 * report mode. As the original run-parts support only long options, I've
 * broken compatibility because the BusyBox policy doesn't allow them.
 * The supported options are:
 * -t			test. Print the name of the files to be executed, without
 *				execute them.
 * -a ARG		argument. Pass ARG as an argument the program executed. It can
 *				be repeated to pass multiple arguments.
 * -u MASK		umask. Set the umask of the program executed to MASK. */

/* TODO
 * done - convert calls to error in perror... and remove error()
 * done - convert malloc/realloc to their x... counterparts
 * done - remove catch_sigchld
 * done - use bb's concat_path_file()
 * done - declare run_parts_main() as extern and any other function as static?
 */

#include "busybox.h"
#include <getopt.h>

static const struct option runparts_long_options[] = {
	{ "test",       0,      NULL,   't' },
	{ "umask",      1,      NULL,   'u' },
	{ "arg",        1,      NULL,   'a' },
	{ 0,            0,      0,      0   }
};

/* valid_name */
/* True or false? Is this a valid filename (upper/lower alpha, digits,
 * underscores, and hyphens only?)
 */
static int valid_name(const struct dirent *d)
{
	const char *c = d->d_name;

	while (*c) {
		if (!isalnum(*c) && (*c != '_') && (*c != '-')) {
			return 0;
		}
		++c;
	}
	return 1;
}

/* test mode = 1 is the same as official run_parts
 * test_mode = 2 means to fail silently on missing directories
 */
static int run_parts(char **args, const unsigned char test_mode)
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
			return 2;
		}
		bb_perror_msg_and_die("cannot open '%s'", arg0);
	}

	for (i = 0; i < entries; i++) {
		filename = concat_path_file(arg0, namelist[i]->d_name);

		xstat(filename, &st);
		if (S_ISREG(st.st_mode) && !access(filename, X_OK)) {
			if (test_mode) {
				puts(filename);
			} else {
				/* exec_errno is common vfork variable */
				volatile int exec_errno = 0;
				int result;
				int pid;

				if ((pid = vfork()) < 0) {
					bb_perror_msg_and_die("failed to fork");
				} else if (!pid) {
					args[0] = filename;
					execve(filename, args, environ);
					exec_errno = errno;
					_exit(1);
				}

				waitpid(pid, &result, 0);
				if (exec_errno) {
					errno = exec_errno;
					bb_perror_msg("failed to exec %s", filename);
					exitstatus = 1;
				}
				if (WIFEXITED(result) && WEXITSTATUS(result)) {
					bb_perror_msg("%s exited with return code %d", filename, WEXITSTATUS(result));
					exitstatus = 1;
				} else if (WIFSIGNALED(result)) {
					bb_perror_msg("%s exited because of uncaught signal %d", filename, WTERMSIG(result));
					exitstatus = 1;
				}
			}
		} else if (!S_ISDIR(st.st_mode)) {
			bb_error_msg("component %s is not an executable plain file", filename);
			exitstatus = 1;
		}

		free(namelist[i]);
		free(filename);
	}
	free(namelist);

	return exitstatus;
}


/* run_parts_main */
/* Process options */
int run_parts_main(int argc, char **argv);
int run_parts_main(int argc, char **argv)
{
	char **args = xmalloc(2 * sizeof(char *));
	unsigned char test_mode = 0;
	unsigned short argcount = 1;
	int opt;

	umask(022);

	while ((opt = getopt_long(argc, argv, "tu:a:",
					runparts_long_options, NULL)) > 0)
	{
		switch (opt) {
		/* Enable test mode */
		case 't':
			test_mode++;
			break;
		/* Set the umask of the programs executed */
		case 'u':
			/* Check and set the umask of the program executed. As stated in the original
			 * run-parts, the octal conversion in libc is not foolproof; it will take the
			 * 8 and 9 digits under some circumstances. We'll just have to live with it.
			 */
			umask(xstrtoul_range(optarg, 8, 0, 07777));
			break;
		/* Pass an argument to the programs */
		case 'a':
			/* Add an argument to the commands that we will call.
			 * Called once for every argument. */
			args = xrealloc(args, (argcount + 2) * (sizeof(char *)));
			args[argcount++] = optarg;
			break;
		default:
			bb_show_usage();
		}
	}

	/* We require exactly one argument: the directory name */
	if (optind != (argc - 1)) {
		bb_show_usage();
	}

	args[0] = argv[optind];
	args[argcount] = 0;

	return run_parts(args, test_mode);
}
