/* vi: set sw=4 ts=4: */
/*
 * Mini dd implementation for busybox
 *
 * Copyright (C) 1999 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
 * based in part on code taken from sash. 
 *
 * Copyright (c) 1999 by David I. Bell
 * Permission is granted to use, distribute, or modify this source,
 * provided that this copyright notice remains intact.
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

static const char dd_usage[] =
	"dd [if=name] [of=name] [bs=n] [count=n] [skip=n] [seek=n]\n\n"
	"Copy a file, converting and formatting according to options\n\n"
	"\tif=FILE\tread from FILE instead of stdin\n"
	"\tof=FILE\twrite to FILE instead of stdout\n"
	"\tbs=n\tread and write n bytes at a time\n"
	"\tcount=n\tcopy only n input blocks\n"
	"\tskip=n\tskip n input blocks\n"
	"\tseek=n\tskip n output blocks\n"

	"\n"
	"Numbers may be suffixed by w (x2), k (x1024), b (x512), or M (x1024^2)\n";



extern int dd_main(int argc, char **argv)
{
	const char *inFile = NULL;
	const char *outFile = NULL;
	char *cp;
	int inFd;
	int outFd;
	int inCc = 0;
	int outCc;
	long blockSize = 512;
	uintmax_t skipBlocks = 0;
	uintmax_t seekBlocks = 0;
	uintmax_t count = (uintmax_t) - 1;
	uintmax_t intotal;
	uintmax_t outTotal;
	unsigned char *buf;

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
				fprintf(stderr, "Bad count value %s\n", *argv);
				goto usage;
			}
		} else if (strncmp(*argv, "bs", 2) == 0) {
			blockSize = getNum((strchr(*argv, '=')) + 1);
			if (blockSize <= 0) {
				fprintf(stderr, "Bad block size value %s\n", *argv);
				goto usage;
			}
		} else if (strncmp(*argv, "skip", 4) == 0) {
			skipBlocks = getNum((strchr(*argv, '=')) + 1);
			if (skipBlocks <= 0) {
				fprintf(stderr, "Bad skip value %s\n", *argv);
				goto usage;
			}

		} else if (strncmp(*argv, "seek", 4) == 0) {
			seekBlocks = getNum((strchr(*argv, '=')) + 1);
			if (seekBlocks <= 0) {
				fprintf(stderr, "Bad seek value %s\n", *argv);
				goto usage;
			}

		} else {
			goto usage;
		}
		argc--;
		argv++;
	}

	buf = xmalloc(blockSize);

	intotal = 0;
	outTotal = 0;

	if (inFile == NULL)
		inFd = fileno(stdin);
	else
		inFd = open(inFile, 0);

	if (inFd < 0) {
		perror(inFile);
		free(buf);
		exit(FALSE);
	}

	if (outFile == NULL)
		outFd = fileno(stdout);
	else
		outFd = open(outFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);

	if (outFd < 0) {
		perror(outFile);
		close(inFd);
		free(buf);
		exit(FALSE);
	}

	lseek(inFd, skipBlocks * blockSize, SEEK_SET);
	lseek(outFd, seekBlocks * blockSize, SEEK_SET);
	//
	//TODO: Convert to using fullRead & fullWrite
	// from utility.c
	//  -Erik
	while (outTotal < count * blockSize) {
		inCc = read(inFd, buf, blockSize);
		if (inCc < 0) {
			perror(inFile);
			goto cleanup;
		} else if (inCc == 0) {
			goto cleanup;
		}
		intotal += inCc;
		cp = buf;

		while (intotal > outTotal) {
			if (outTotal + inCc > count * blockSize)
				inCc = count * blockSize - outTotal;
			outCc = write(outFd, cp, inCc);
			if (outCc < 0) {
				perror(outFile);
				goto cleanup;
			} else if (outCc == 0) {
				goto cleanup;
			}

			inCc -= outCc;
			cp += outCc;
			outTotal += outCc;
		}
	}

	if (inCc < 0)
		perror(inFile);

  cleanup:
	close(inFd);
	close(outFd);
	free(buf);

	printf("%ld+%d records in\n", (long) (intotal / blockSize),
		   (intotal % blockSize) != 0);
	printf("%ld+%d records out\n", (long) (outTotal / blockSize),
		   (outTotal % blockSize) != 0);
	exit(TRUE);
  usage:

	usage(dd_usage);
}
