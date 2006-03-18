/*
 * dos2unix for BusyBox
 *
 * dos2unix '\n' convertor 0.5.0
 * based on Unix2Dos 0.9.0 by Peter Hanecak (made 19.2.1997)
 * Copyright 1997,.. by Peter Hanecak <hanecak@megaloman.sk>.
 * All rights reserved.
 *
 * dos2unix filters reading input from stdin and writing output to stdout.
 *
 *  Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
*/

#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/time.h>
#include "busybox.h"

#define CT_UNIX2DOS     1
#define CT_DOS2UNIX     2
#define tempFn bb_common_bufsiz1

/* if fn is NULL then input is stdin and output is stdout */
static int convert(char *fn, int ConvType)
{
	int c, fd;
	FILE *in, *out;

	if (fn != NULL) {
		in = bb_xfopen(fn, "rw");
		/*
		The file is then created with mode read/write and
		permissions 0666 for glibc 2.0.6 and earlier or
		0600  for glibc  2.0.7 and  later. 
		*/
		snprintf(tempFn, sizeof(tempFn), "%sXXXXXX", fn);
		/*
		sizeof tempFn is 4096, so it should be big enough to hold the full
		path. however if the output is truncated the subsequent call to
		mkstemp would fail.
		*/
		if ((fd = mkstemp(&tempFn[0])) == -1 || chmod(tempFn, 0600) == -1) {
			bb_perror_nomsg_and_die();
		}
		out = fdopen(fd, "w+");
		if (!out) {
			close(fd);
			remove(tempFn);
		}
	} else {
		in = stdin;
		out = stdout;
	}

	while ((c = fgetc(in)) != EOF) {
		if (c == '\r')
			continue;
		if (c == '\n') {
			if (ConvType == CT_UNIX2DOS)
				fputc('\r', out);
			fputc('\n', out);
			continue;
		}
		fputc(c, out);
	}

	if (fn != NULL) {
		if (fclose(in) < 0 || fclose(out) < 0) {
			bb_perror_nomsg();
			remove(tempFn);
			return -2;
	    }
	
		/* Assume they are both on the same filesystem (which
		* should be true since we put them into the same directory
		* so we _should_ be ok, but you never know... */
		if (rename(tempFn, fn) < 0) {
			bb_perror_msg("cannot rename '%s' as '%s'", tempFn, fn);
			return -1;
	    }
	}

	return 0;
}

int dos2unix_main(int argc, char *argv[])
{
	int ConvType;
	int o;

	/* See if we are supposed to be doing dos2unix or unix2dos */
	if (argv[0][0]=='d') {
	    ConvType = CT_DOS2UNIX;  /*2*/
	} else {
	    ConvType = CT_UNIX2DOS;  /*1*/
	}
	/* -u and -d are mutally exclusive */
	bb_opt_complementally = "?:u--d:d--u";
	/* process parameters */
	/* -u convert to unix */
	/* -d convert to dos  */
	o = bb_getopt_ulflags(argc, argv, "du");

	/* Do the conversion requested by an argument else do the default
	 * conversion depending on our name.  */
	if (o)
		ConvType = o;

	if (optind < argc) {
		while(optind < argc)
			if ((o = convert(argv[optind++], ConvType)) < 0)
				break;
	}
	else
		o = convert(NULL, ConvType);

	return o;
}
