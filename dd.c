/*
 * Copyright (c) 1999 by David I. Bell
 * Permission is granted to use, distribute, or modify this source,
 * provided that this copyright notice remains intact.
 *
 * The "dd" command, originally taken from sash.
 *
 * Permission to distribute this code under the GPL has been granted.
 * Majorly modified, and bugs fixed for busybox by Erik Andersen <andersee@debian.org> <andersen@lineo.com>
 */

#include "internal.h"
#ifdef BB_DD

const char dd_usage[] = 
"Copy a file, converting and formatting according to options\n\
\n\
usage: [if=name] [of=name] [bs=n] [count=n]\n\
\tif=FILE\tread from FILE instead of stdin\n\
\tof=FILE\twrite to FILE instead of stout\n\
\tbs=n\tread and write N bytes at a time\n\
\tcount=n\tcopy only n input blocks\n\
\n\
BYTES may be suffixed: by k for x1024, b for x512, and w for x2.\n";


#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>


#define	PAR_NONE	0
#define	PAR_IF		1
#define	PAR_OF		2
#define	PAR_BS		3
#define	PAR_COUNT	4


typedef	struct
{
	const char *	name;
	int		value;
} PARAM;


static const PARAM	params[] =
{
	{"if",		PAR_IF},
	{"of",		PAR_OF},
	{"bs",		PAR_BS},
	{"count",	PAR_COUNT},
	{NULL,		PAR_NONE}
};


static	long	getNum(const char * cp);

extern int
dd_main (struct FileInfo *unused, int argc, char **argv)
{
	const char *	str;
	const PARAM *	par;
	const char *	inFile;
	const char *	outFile;
	char *		cp;
	int		inFd;
	int		outFd;
	int		inCc=0;
	int		outCc;
	int		blockSize;
	long		count;
	long		intotal;
	long		outTotal;
	unsigned char*	buf;
	unsigned char	localBuf[BUF_SIZE];

	inFile = NULL;
	outFile = NULL;
	blockSize = 512;
	count = 1;


	while (--argc > 0)
	{
		str = *++argv;
		cp = strchr(str, '=');

		if (cp == NULL)
		{
			fprintf(stderr, "Bad dd argument\n");
			goto usage;
		}

		*cp++ = '\0';

		for (par = params; par->name; par++)
		{
			if (strcmp(str, par->name) == 0)
				break;
		}

		switch (par->value)
		{
			case PAR_IF:
				if (inFile)
				{
					fprintf(stderr, "Multiple input files illegal\n");
					goto usage;
				}
	
				//fprintf(stderr, "if=%s\n", cp);
				inFile = cp;
				break;

			case PAR_OF:
				if (outFile)
				{
					fprintf(stderr, "Multiple output files illegal\n");
					goto usage;
				}

				//fprintf(stderr, "of=%s\n", cp);
				outFile = cp;
				break;

			case PAR_BS:
				blockSize = getNum(cp);
				//fprintf(stderr, "bs=%d\n", blockSize);

				if (blockSize <= 0)
				{
					fprintf(stderr, "Bad block size value\n");
					goto usage;
				}

				break;

			case PAR_COUNT:
				count = getNum(cp);
				//fprintf(stderr, "count=%ld\n", count);

				if (count < 0)
				{
					fprintf(stderr, "Bad count value\n");
					goto usage;
				}

				break;

			default:
				goto usage;
		}
	}

	buf = localBuf;

	if (blockSize > sizeof(localBuf))
	{
		buf = malloc(blockSize);

		if (buf == NULL)
		{
			fprintf(stderr, "Cannot allocate buffer\n");
			return 1;
		}
	}

	intotal = 0;
	outTotal = 0;

	if (inFile == NULL)
	    inFd = STDIN;
	else
	    inFd = open(inFile, 0);

	if (inFd < 0)
	{
		perror(inFile);

		if (buf != localBuf)
			free(buf);

		return 1;
	}

	if (outFile == NULL)
	    outFd = STDOUT;
	else
	    outFd = creat(outFile, 0666);

	if (outFd < 0)
	{
		perror(outFile);
		close(inFd);

		if (buf != localBuf)
			free(buf);

		return 1;
	}

	while ( outTotal < count*blockSize )
	{
		inCc = read(inFd, buf, blockSize);
		if (inCc < 0) {
		    perror(inFile);
		    goto cleanup;
		}
		//fprintf(stderr, "read in =%d\n", inCc);
		intotal += inCc;
		cp = buf;


		while ( intotal > outTotal )
		{
			if (outTotal+inCc > count*blockSize)
			    inCc=count*blockSize-outTotal;
			outCc = write(outFd, cp, inCc);
			if (outCc < 0)
			{
				perror(outFile);
				goto cleanup;
			}
			//fprintf(stderr, "wrote out =%d\n", outCc);

			inCc -= outCc;
			cp += outCc;
			outTotal += outCc;
			//fprintf(stderr, "outTotal=%ld\n", outTotal);
		}
	}

	if (inCc < 0)
		perror(inFile);

cleanup:
	close(inFd);

	if (close(outFd) < 0)
		perror(outFile);

	if (buf != localBuf)
		free(buf);

	printf("%ld+%d records in\n", intotal / blockSize,
		(intotal % blockSize) != 0);

	printf("%ld+%d records out\n", outTotal / blockSize,
		(outTotal % blockSize) != 0);
	return 0;
usage:
	
	fprintf(stderr, "%s", dd_usage);
	return 1;
}


/*
 * Read a number with a possible multiplier.
 * Returns -1 if the number format is illegal.
 */
static long
getNum(const char * cp)
{
	long	value;

	if (!isDecimal(*cp))
		return -1;

	value = 0;

	while (isDecimal(*cp))
		value = value * 10 + *cp++ - '0';

	switch (*cp++)
	{
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

#endif
/* END CODE */

