/* vi: set sw=4 ts=4: */
/*
 * Mini dd implementation for busybox
 *
 * Copyright (C) 1999, 2000 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
 *
 * Based in part on code taken from sash. 
 *   Copyright (c) 1999 by David I. Bell
 *   Permission is granted to use, distribute, or modify this source,
 *   provided that this copyright notice remains intact.
 *
 * Permission to distribute this code under the GPL has been granted.
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


#include "internal.h"
#include <features.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#if (__GLIBC__ >= 2) && (__GLIBC_MINOR__ >= 1)
#include <inttypes.h>
#else
typedef unsigned long long int uintmax_t;
#endif

extern int dd_main(int argc, char **argv)
{
	char *inFile = NULL;
	char *outFile = NULL;
	int inFd;
	int outFd;
	int inCc = 0;
	int outCc;
	long blockSize = 512;
	uintmax_t skipBlocks = 0;
	uintmax_t seekBlocks = 0;
	uintmax_t count = (uintmax_t) - 1;
	uintmax_t inTotal = 0;
	uintmax_t outTotal = 0;
	uintmax_t totalSize;
	uintmax_t readSize;
	unsigned char buf[BUFSIZ];

	argc--;
	argv++;

	/* Parse any options */
	while (argc) {
		if (inFile == NULL && (strncmp(*argv, "if", 2) == 0))
			inFile = ((strchr(*argv, '=')) + 1);
		else if (outFile == NULL && (strncmp(*argv, "of", 2) == 0))
			outFile = ((strchr(*argv, '=')) + 1);
		else if (strncmp("count", *argv, 5) == 0) {
			count = getNum((strchr(*argv, '=')) + 1);
			if (count <= 0) {
				errorMsg("Bad count value %s\n", *argv);
				goto usage;
			}
		} else if (strncmp(*argv, "bs", 2) == 0) {
			blockSize = getNum((strchr(*argv, '=')) + 1);
			if (blockSize <= 0) {
				errorMsg("Bad block size value %s\n", *argv);
				goto usage;
			}
		} else if (strncmp(*argv, "skip", 4) == 0) {
			skipBlocks = getNum((strchr(*argv, '=')) + 1);
			if (skipBlocks <= 0) {
				errorMsg("Bad skip value %s\n", *argv);
				goto usage;
			}

		} else if (strncmp(*argv, "seek", 4) == 0) {
			seekBlocks = getNum((strchr(*argv, '=')) + 1);
			if (seekBlocks <= 0) {
				errorMsg("Bad seek value %s\n", *argv);
				goto usage;
			}

		} else {
			goto usage;
		}
		argc--;
		argv++;
	}

	if (inFile == NULL)
		inFd = fileno(stdin);
	else
		inFd = open(inFile, 0);

	if (inFd < 0) {
		/* Note that we are not freeing buf or closing
		 * files here to save a few bytes. This exits
		 * here anyways... */

		/* free(buf); */
		fatalError( inFile);
	}

	if (outFile == NULL)
		outFd = fileno(stdout);
	else
		outFd = open(outFile, O_WRONLY | O_CREAT, 0666);

	if (outFd < 0) {
		/* Note that we are not freeing buf or closing
		 * files here to save a few bytes. This exits
		 * here anyways... */

		/* close(inFd);
		   free(buf); */
		fatalError( outFile);
	}

	lseek(inFd, (off_t) (skipBlocks * blockSize), SEEK_SET);
	lseek(outFd, (off_t) (seekBlocks * blockSize), SEEK_SET);
	totalSize=count*blockSize;
	while ((readSize = totalSize - inTotal) > 0) {
		if (readSize > BUFSIZ)
			readSize=BUFSIZ;
		inCc = read(inFd, buf, readSize);
		inTotal += inCc;
		if ((outCc = fullWrite(outFd, buf, inCc)) < 0)
			break;
		outTotal += outCc;
        }

	/* Note that we are not freeing memory or closing
	 * files here, to save a few bytes. */
#ifdef BB_FEATURE_CLEAN_UP
	close(inFd);
	close(outFd);
#endif

	printf("%ld+%d records in\n", (long) (inTotal / blockSize),
		   (inTotal % blockSize) != 0);
	printf("%ld+%d records out\n", (long) (outTotal / blockSize),
		   (outTotal % blockSize) != 0);
	exit(TRUE);
  usage:

	usage(dd_usage);
}
