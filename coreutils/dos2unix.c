/* vi: set sw=4 ts=4: */
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
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
*/

#include "busybox.h"

enum ConvType {
	CT_UNIX2DOS = 1,
	CT_DOS2UNIX
} ConvType;

/* if fn is NULL then input is stdin and output is stdout */
static int convert(char *fn)
{
	FILE *in, *out;
	int i;

	if (fn != NULL) {
		in = xfopen(fn, "rw");
		/*
		   The file is then created with mode read/write and
		   permissions 0666 for glibc 2.0.6 and earlier or
		   0600  for glibc  2.0.7 and  later.
		 */
		snprintf(bb_common_bufsiz1, sizeof(bb_common_bufsiz1), "%sXXXXXX", fn);
		/*
		   sizeof bb_common_bufsiz1 is 4096, so it should be big enough to
		   hold the full path.  However if the output is truncated the
		   subsequent call to mkstemp would fail.
		 */
		if ((i = mkstemp(&bb_common_bufsiz1[0])) == -1
			|| chmod(bb_common_bufsiz1, 0600) == -1) {
			bb_perror_nomsg_and_die();
		}
		out = fdopen(i, "w+");
		if (!out) {
			close(i);
			remove(bb_common_bufsiz1);
		}
	} else {
		in = stdin;
		out = stdout;
	}

	while ((i = fgetc(in)) != EOF) {
		if (i == '\r')
			continue;
		if (i == '\n') {
			if (ConvType == CT_UNIX2DOS)
				fputc('\r', out);
			fputc('\n', out);
			continue;
		}
		fputc(i, out);
	}

	if (fn != NULL) {
		if (fclose(in) < 0 || fclose(out) < 0) {
			bb_perror_nomsg();
			remove(bb_common_bufsiz1);
			return -2;
		}
		/* Assume they are both on the same filesystem (which
		 * should be true since we put them into the same directory
		 * so we _should_ be ok, but you never know... */
		if (rename(bb_common_bufsiz1, fn) < 0) {
			bb_perror_msg("cannot rename '%s' as '%s'", bb_common_bufsiz1, fn);
			return -1;
		}
	}

	return 0;
}

int dos2unix_main(int argc, char *argv[]);
int dos2unix_main(int argc, char *argv[])
{
	int o;

	/* See if we are supposed to be doing dos2unix or unix2dos */
	if (applet_name[0] == 'd') {
		ConvType = CT_DOS2UNIX;	/*2 */
	} else {
		ConvType = CT_UNIX2DOS;	/*1 */
	}
	/* -u and -d are mutally exclusive */
	opt_complementary = "?:u--d:d--u";
	/* process parameters */
	/* -u convert to unix */
	/* -d convert to dos  */
	o = getopt32(argc, argv, "du");

	/* Do the conversion requested by an argument else do the default
	 * conversion depending on our name.  */
	if (o)
		ConvType = o;

	if (optind < argc) {
		while (optind < argc)
			if ((o = convert(argv[optind++])) < 0)
				break;
	} else
		o = convert(NULL);

	return o;
}
