/*
 * Copyright (c) 1999 by David I. Bell
 * Permission is granted to use, distribute, or modify this source,
 * provided that this copyright notice remains intact.
 *
 * The "dd" command, originally taken from sash.
 *
 * Permission to distribute this code under the GPL has been granted.
 * Mostly rewritten and bugs fixed for busybox by Erik Andersen <andersee@debian.org>
 */

#include "internal.h"
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

static const char dd_usage[] =
    "Copy a file, converting and formatting according to options\n\
\n\
usage: [if=name] [of=name] [bs=n] [count=n]\n\
\tif=FILE\tread from FILE instead of stdin\n\
\tof=FILE\twrite to FILE instead of stout\n\
\tbs=n\tread and write N BYTES at a time\n\
\tcount=n\tcopy only n input blocks\n\
\tskip=n\tskip n input blocks\n\
\n\
BYTES may be suffixed: by k for x1024, b for x512, and w for x2.\n";




/*
 * Read a number with a possible multiplier.
 * Returns -1 if the number format is illegal.
 */
static long getNum (const char *cp)
{
    long value;

    if (!isDecimal (*cp))
	return -1;

    value = 0;

    while (isDecimal (*cp))
	value = value * 10 + *cp++ - '0';

    switch (*cp++) {
    case 'k':
	value *= 1024;
	break;

    case 'b':
	value *= 512;
	break;

    case 'w':
	value *= 2;
	break;

    case '\0':
	return value;

    default:
	return -1;
    }

    if (*cp)
	return -1;

    return value;
}


extern int dd_main (int argc, char **argv)
{
    const char *inFile;
    const char *outFile;
    char *cp;
    int inFd;
    int outFd;
    int inCc = 0;
    int outCc;
    int skipBlocks;
    int blockSize;
    long count;
    long intotal;
    long outTotal;
    unsigned char *buf;

    inFile = NULL;
    outFile = NULL;
    blockSize = 512;
    skipBlocks = 0;
    count = 1;


    argc--;
    argv++;

    /* Parse any options */
    while (argc) {
	if (inFile == NULL && (strncmp(*argv, "if", 2) == 0))
	    inFile=((strchr(*argv, '='))+1);
	else if (outFile == NULL && (strncmp(*argv, "of", 2) == 0))
	    outFile=((strchr(*argv, '='))+1);
	else if (strncmp("count", *argv, 5) == 0) {
	    count = getNum ((strchr(*argv, '='))+1);
	    if (count <= 0) {
		fprintf (stderr, "Bad count value %ld\n", count);
		goto usage;
	    }
	}
	else if (strncmp(*argv, "bs", 2) == 0) {
	    blockSize = getNum ((strchr(*argv, '='))+1);
	    if (blockSize <= 0) {
		fprintf (stderr, "Bad block size value %d\n", blockSize);
		goto usage;
	    }
	}
	else if (strncmp(*argv, "skip", 4) == 0) {
	    skipBlocks = atoi( *argv); 
	    if (skipBlocks <= 0) {
		fprintf (stderr, "Bad skip value %d\n", skipBlocks);
		goto usage;
	    }

	}
	else {
	    fprintf (stderr, "Got here. argv=%s\n", *argv);
	    goto usage;
	}
	argc--;
	argv++;
    }
    if ( inFile == NULL || outFile == NULL)
	goto usage;

    buf = malloc (blockSize);
    if (buf == NULL) {
	fprintf (stderr, "Cannot allocate buffer\n");
	exit( FALSE);
    }

    intotal = 0;
    outTotal = 0;

    if (inFile == NULL)
	inFd = STDIN;
    else
	inFd = open (inFile, 0);

    if (inFd < 0) {
	perror (inFile);
	free (buf);
	exit( FALSE);
    }

    if (outFile == NULL)
	outFd = STDOUT;
    else
	outFd = creat (outFile, 0666);

    if (outFd < 0) {
	perror (outFile);
	close (inFd);
	free (buf);
	exit( FALSE);
    }

    //lseek(inFd, skipBlocks*blockSize, SEEK_SET);
    while (outTotal < count * blockSize) {
	inCc = read (inFd, buf, blockSize);
	if (inCc < 0) {
	    perror (inFile);
	    goto cleanup;
	}
	intotal += inCc;
	cp = buf;

	while (intotal > outTotal) {
	    if (outTotal + inCc > count * blockSize)
		inCc = count * blockSize - outTotal;
	    outCc = write (outFd, cp, inCc);
	    if (outCc < 0) {
		perror (outFile);
		goto cleanup;
	    }

	    inCc -= outCc;
	    cp += outCc;
	    outTotal += outCc;
	}
    }

    if (inCc < 0)
	perror (inFile);

  cleanup:
    close (inFd);
    close (outFd);
    free (buf);

    printf ("%ld+%d records in\n", intotal / blockSize,
	    (intotal % blockSize) != 0);
    printf ("%ld+%d records out\n", outTotal / blockSize,
	    (outTotal % blockSize) != 0);
    exit( TRUE);
  usage:

    fprintf (stderr, "%s", dd_usage);
    exit( FALSE);
}


