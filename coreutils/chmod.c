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
#define OPT_STR     "R" USE_DESKTOP("vcf")

/* coreutils:
 * chmod never changes the permissions of symbolic links; the chmod
 * system call cannot change their permissions. This is not a problem
 * since the permissions of symbolic links are never used.
 * However, for each symbolic link listed on the command line, chmod changes
 * the permissions of the pointed-to file. In contrast, chmod ignores
 * symbolic links encountered during recursive directory traversals.
 */

static int fileAction(const char *fileName, struct stat *statbuf, void* junk, int depth)
{
	mode_t newmode;

	/* match coreutils behavior */
	if (depth == 0) {
		/* statbuf holds lstat result, but we need stat (follow link) */
		if (stat(fileName, statbuf))
			goto err;
	} else { /* depth > 0: skip links */
		if (S_ISLNK(statbuf->st_mode))
			return TRUE;
	}
	newmode = statbuf->st_mode;

	if (!bb_parse_mode((char *)junk, &newmode))
		bb_error_msg_and_die("invalid mode: %s", (char *)junk);

	if (chmod(fileName, newmode) == 0) {
		if (OPT_VERBOSE
		 || (OPT_CHANGED && statbuf->st_mode != newmode)
		) {
			printf("mode of '%s' changed to %04o (%s)\n", fileName,
				newmode & 07777, bb_mode_string(newmode)+1);
		}
		return TRUE;
	}
 err:
	if (!OPT_QUIET)
		bb_perror_msg("%s", fileName);
	return FALSE;
}

int chmod_main(int argc, char **argv)
{
	int retval = EXIT_SUCCESS;
	char *arg, **argp;
	char *smode;

	/* Convert first encountered -r into ar, -w into aw etc
	 * so that getopt would not eat it */
	argp = argv;
	while ((arg = *++argp)) {
		/* Mode spec must be the first arg (sans -R etc) */
		/* (protect against mishandling e.g. "chmod 644 -r") */
		if (arg[0] != '-') {
			arg = NULL;
			break;
		}
		/* An option. Not a -- or valid option? */
		if (arg[1] && !strchr("-"OPT_STR, arg[1])) {
			arg[0] = 'a';
			break;
		}
	}

	/* Parse options */
	opt_complementary = "-2";
	getopt32(argc, argv, ("-"OPT_STR) + 1); /* Reuse string */
	argv += optind;

	/* Restore option-like mode if needed */
	if (arg) arg[0] = '-';

	/* Ok, ready to do the deed now */
	smode = *argv++;
	do {
		if (!recursive_action(*argv,
			OPT_RECURSE,    // recurse
			FALSE,          // follow links: coreutils doesn't
			FALSE,          // depth first
			fileAction,     // file action
			fileAction,     // dir action
			smode,          // user data
			0)              // depth
		) {
			retval = EXIT_FAILURE;
		}
	} while (*++argv);

	return retval;
}

/*
Security: chmod is too important and too subtle.
This is a test script (busybox chmod versus coreutils).
Run it in empty dir. Probably requires bash.

#!/bin/sh
function create() {
    rm -rf $1; mkdir $1
    (
    cd $1 || exit 1
    mkdir dir
    >up
    >file
    >dir/file
    ln -s dir linkdir
    ln -s file linkfile
    ln -s ../up dir/up
    )
}
function tst() {
    (cd test1; $t1 $1)
    (cd test2; $t2 $1)
    (cd test1; ls -lR) >out1
    (cd test2; ls -lR) >out2
    echo "chmod $1" >out.diff
    if ! diff -u out1 out2 >>out.diff; then exit 1; fi
    mv out.diff out1.diff
}
t1="/tmp/busybox chmod"
t2="/usr/bin/chmod"
create test1; create test2
tst "a+w file"
tst "a-w dir"
tst "a+w linkfile"
tst "a-w linkdir"
tst "-R a+w file"
tst "-R a-w dir"
tst "-R a+w linkfile"
tst "-R a-w linkdir"
tst "a-r,a+x linkfile"
*/
