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

#include "libbb.h"

enum {
	CT_UNIX2DOS = 1,
	CT_DOS2UNIX
};

/* if fn is NULL then input is stdin and output is stdout */
static void convert(char *fn, int conv_type)
{
	FILE *in, *out;
	int i;
	char *name_buf = name_buf; /* for compiler */

	in = stdin;
	out = stdout;
	if (fn != NULL) {
		in = xfopen(fn, "r");
		/*
		   The file is then created with mode read/write and
		   permissions 0666 for glibc 2.0.6 and earlier or
		   0600 for glibc 2.0.7 and later.
		 */
		name_buf = xasprintf("%sXXXXXX", fn);
		i = mkstemp(name_buf);
		if (i == -1
		 || fchmod(i, 0600) == -1
		 || !(out = fdopen(i, "w+"))
		) {
			bb_perror_nomsg_and_die();
		}
	}

	while ((i = fgetc(in)) != EOF) {
		if (i == '\r')
			continue;
		if (i == '\n')
			if (conv_type == CT_UNIX2DOS)
				fputc('\r', out);
		fputc(i, out);
	}

	if (fn != NULL) {
		if (fclose(in) < 0 || fclose(out) < 0) {
			unlink(name_buf);
			bb_perror_nomsg_and_die();
		}
// TODO: destroys symlinks. See how passwd handles this
		xrename(name_buf, fn);
		free(name_buf);
	}
}

int dos2unix_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int dos2unix_main(int argc, char **argv)
{
	int o, conv_type;

	/* See if we are supposed to be doing dos2unix or unix2dos */
	conv_type = CT_UNIX2DOS;
	if (applet_name[0] == 'd') {
		conv_type = CT_DOS2UNIX;
	}

	/* -u convert to unix, -d convert to dos */
	opt_complementary = "u--d:d--u"; /* mutually exclusive */
	o = getopt32(argv, "du");

	/* Do the conversion requested by an argument else do the default
	 * conversion depending on our name.  */
	if (o)
		conv_type = o;

	do {
		/* might be convert(NULL) if there is no filename given */
		convert(argv[optind], conv_type);
		optind++;
	} while (optind < argc);

	return 0;
}
