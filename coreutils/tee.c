/* vi: set sw=4 ts=4: */
/*
 * tee implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/tee.html */

#include "busybox.h"
#include <signal.h>

int tee_main(int argc, char **argv)
{
	const char *mode = "w\0a";
	FILE **files;
	FILE **p;
	char **filenames;
	int flags;
	int retval = EXIT_SUCCESS;
#ifdef CONFIG_FEATURE_TEE_USE_BLOCK_IO
	ssize_t c;
# define buf bb_common_bufsiz1
#else
	int c;
#endif

	flags = getopt32(argc, argv, "ia");	/* 'a' must be 2nd */

	mode += (flags & 2);	/* Since 'a' is the 2nd option... */

	if (flags & 1) {
		signal(SIGINT, SIG_IGN);	/* TODO - switch to sigaction.*/
	}

	/* gnu tee ignores SIGPIPE in case one of the output files is a pipe
	 * that doesn't consume all its input.  Good idea... */
	signal(SIGPIPE, SIG_IGN);	/* TODO - switch to sigaction.*/

	/* Allocate an array of FILE *'s, with one extra for a sentinal. */
	p = files = (FILE **)xmalloc(sizeof(FILE *) * (argc - optind + 2));
	*p = stdout;
	argv += optind - 1;
	filenames = argv - 1;
	*filenames = (char *) bb_msg_standard_input;	/* for later */
	goto GOT_NEW_FILE;

	do {
		if ((*p = bb_wfopen(*argv, mode)) == NULL) {
			retval = EXIT_FAILURE;
			continue;
		}
		filenames[(int)(p - files)] = *argv;
	GOT_NEW_FILE:
		setbuf(*p, NULL);	/* tee must not buffer output. */
		++p;
	} while (*++argv);

	*p = NULL;				/* Store the sentinal value. */

#ifdef CONFIG_FEATURE_TEE_USE_BLOCK_IO
	while ((c = safe_read(STDIN_FILENO, buf, BUFSIZ)) > 0) {
		for (p=files ; *p ; p++) {
			fwrite(buf, 1, c, *p);
		}
	}

	if (c < 0) {			/* Make sure read errors are signaled. */
		retval = EXIT_FAILURE;
	}

#else
	setvbuf(stdout, NULL, _IONBF, 0);
	while ((c = getchar()) != EOF) {
		for (p=files ; *p ; p++) {
			putc(c, *p);
		}
	}
#endif

	/* Now we need to check for i/o errors on stdin and the various
	 * output files.  Since we know that the first entry in the output
	 * file table is stdout, we can save one "if ferror" test by
	 * setting the first entry to stdin and checking stdout error
	 * status with fflush_stdout_and_exit()... although fflush()ing
	 * is unnecessary here. */

	p = files;
	*p = stdin;
	do {		/* Now check for (input and) output errors. */
		/* Checking ferror should be sufficient, but we may want to fclose.
		 * If we do, remember not to close stdin! */
		die_if_ferror(*p, filenames[(int)(p - files)]);
	} while (*++p);

	fflush_stdout_and_exit(retval);
}
