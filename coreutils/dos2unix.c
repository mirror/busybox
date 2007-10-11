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
static int convert(char *fn, int conv_type)
{
	FILE *in, *out;
	int i;
#define name_buf bb_common_bufsiz1

	in = stdin;
	out = stdout;
	if (fn != NULL) {
		in = xfopen(fn, "rw");
		/*
		   The file is then created with mode read/write and
		   permissions 0666 for glibc 2.0.6 and earlier or
		   0600 for glibc 2.0.7 and later.
		 */
		snprintf(name_buf, sizeof(name_buf), "%sXXXXXX", fn);
		i = mkstemp(&name_buf[0]);
		if (i == -1 || chmod(name_buf, 0600) == -1) {
			bb_perror_nomsg_and_die();
		}
		out = fdopen(i, "w+");
		if (!out) {
			close(i);
			remove(name_buf);
			return -2;
		}
	}

	while ((i = fgetc(in)) != EOF) {
		if (i == '\r')
			continue;
		if (i == '\n') {
			if (conv_type == CT_UNIX2DOS)
				fputc('\r', out);
			fputc('\n', out);
			continue;
		}
		fputc(i, out);
	}

	if (fn != NULL) {
		if (fclose(in) < 0 || fclose(out) < 0) {
			bb_perror_nomsg();
			remove(name_buf);
			return -2;
		}
		/* Assume they are both on the same filesystem (which
		 * should be true since we put them into the same directory
		 * so we _should_ be ok, but you never know... */
		if (rename(name_buf, fn) < 0) {
			bb_perror_msg("cannot rename '%s' as '%s'", name_buf, fn);
			return -1;
		}
	}

	return 0;
}

int dos2unix_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int dos2unix_main(int argc, char **argv)
{
	int o, conv_type;

	/* See if we are supposed to be doing dos2unix or unix2dos */
	if (applet_name[0] == 'd') {
		conv_type = CT_DOS2UNIX;	/* 2 */
	} else {
		conv_type = CT_UNIX2DOS;	/* 1 */
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
		o = convert(argv[optind], conv_type);
		if (o < 0)
			break;
		optind++;
	} while (optind < argc);

	return o;
}
