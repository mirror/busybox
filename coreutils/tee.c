/* vi: set sw=4 ts=4: */
/*
 * tee implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/tee.html */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "busybox.h"

int tee_main(int argc, char **argv)
{
	const char *mode = "w\0a";
	FILE **files;
	FILE **p;
	char **filenames;
	int flags;
	int retval = EXIT_SUCCESS;
#ifdef CONFIG_FEATURE_TEE_USE_BLOCK_IO
	size_t c;
	RESERVE_CONFIG_BUFFER(buf, BUFSIZ);
#else
	int c;
#endif

	flags = bb_getopt_ulflags(argc, argv, "ia");	/* 'a' must be 2nd */

	mode += (flags & 2);	/* Since 'a' is the 2nd option... */

	if (flags & 1) {
		signal(SIGINT, SIG_IGN);	/* TODO - switch to sigaction.*/
	}

	/* gnu tee ignores SIGPIPE in case one of the output files is a pipe
	 * that doesn't consume all its input.  Good idea... */
	signal(SIGPIPE, SIG_IGN);		/* TODO - switch to sigaction.*/

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
	while ((c = fread(buf, 1, BUFSIZ, stdin)) != 0) {
		for (p=files ; *p ; p++) {
			fwrite(buf, 1, c, *p);
		}
	}

#ifdef CONFIG_FEATURE_CLEAN_UP
	RELEASE_CONFIG_BUFFER(buf);
#endif

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
	 * status with bb_fflush_stdout_and_exit()... although fflush()ing
	 * is unnecessary here. */

	p = files;
	*p = stdin;
	do {		/* Now check for (input and) output errors. */
		/* Checking ferror should be sufficient, but we may want to fclose.
		 * If we do, remember not to close stdin! */
		bb_xferror(*p, filenames[(int)(p - files)]);
	} while (*++p);

	bb_fflush_stdout_and_exit(retval);
}
