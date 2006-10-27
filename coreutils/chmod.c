/* vi: set sw=4 ts=4: */
/*
 * Mini chmod implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Reworked by (C) 2002 Vladimir Oleynik <dzo@simtreas.ru>
 *  to correctly parse '-rwxgoa'
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 compliant */
/* BB_AUDIT GNU defects - unsupported long options. */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/chmod.html */

#include "busybox.h"

#define OPT_RECURSE (option_mask32 & 1)
#define OPT_VERBOSE (USE_DESKTOP(option_mask32 & 2) SKIP_DESKTOP(0))
#define OPT_CHANGED (USE_DESKTOP(option_mask32 & 4) SKIP_DESKTOP(0))
#define OPT_QUIET   (USE_DESKTOP(option_mask32 & 8) SKIP_DESKTOP(0))
#define OPT_STR     ("-R" USE_DESKTOP("vcf"))

/* TODO:
 * chmod never changes the permissions of symbolic links; the chmod
 * system call cannot change their permissions. This is not a problem
 * since the permissions of symbolic links are never used.
 * However, for each symbolic link listed on the command line, chmod changes
 * the permissions of the pointed-to file. In contrast, chmod ignores
 * symbolic links encountered during recursive directory traversals.
 */

static int fileAction(const char *fileName, struct stat *statbuf, void* junk)
{
	mode_t newmode = statbuf->st_mode;

	// TODO: match GNU behavior:
	// if (depth > 0 && S_ISLNK(statbuf->st_mode)) return TRUE;
	// if (depth == 0) follow link

	if (!bb_parse_mode((char *)junk, &newmode))
		bb_error_msg_and_die("invalid mode: %s", (char *)junk);

	if (chmod(fileName, statbuf->st_mode) == 0) {
		if (OPT_VERBOSE /* -v verbose? or -c changed? */
		 || (OPT_CHANGED && statbuf->st_mode != newmode)
		) {
			printf("mode of '%s' changed to %04o (%s)\n", fileName,
				newmode & 07777, bb_mode_string(newmode)+1);
		}
		return TRUE;
	}
	if (!OPT_QUIET) /* not silent (-f)? */
		bb_perror_msg("%s", fileName);
	return FALSE;
}

int chmod_main(int argc, char **argv)
{
	int retval = EXIT_SUCCESS;
	char *arg, **argp;
	char *smode;

	/* Convert first encountered -r into a-r, -w into a-w etc */
	argp = argv + 1;
	while ((arg = *argp)) {
		/* Mode spec must be the first arg (sans -R etc) */
		/* (protect against mishandling e.g. "chmod 644 -r") */
		if (arg[0] != '-')
			break;
		/* An option. Not a -- or valid option? */
		if (arg[1] && !strchr(OPT_STR, arg[1])) {
			argp[0] = xasprintf("a%s", arg);
			break;
		}
		argp++;
	}
	/* "chmod -rzzz abc" will say "invalid mode: a-rzzz"!
	 * It is easily fixable, but deemed not worth the code */

	opt_complementary = "-2";
	getopt32(argc, argv, OPT_STR + 1); /* Reuse string */
	argv += optind;

	smode = *argv++;

	/* Ok, ready to do the deed now */
	do {
		if (!recursive_action(*argv,
				OPT_RECURSE,    // recurse
				FALSE,          // follow links: GNU doesn't
				FALSE,          // depth first
				fileAction,     // file action
				fileAction,     // dir action
				smode)          // user data
		) {
			retval = EXIT_FAILURE;
		}
	} while (*++argv);

	return retval;
}
