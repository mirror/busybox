/*
 * dos2unix for BusyBox
 *
 * dos2unix '\n' convertor 0.5.0
 *   based on Unix2Dos 0.9.0 by Peter Hanecak (made 19.2.1997)
 * Copyright 1997,.. by Peter Hanecak <hanecak@megaloman.sk>.
 * All rights reserved.
 *
 * dos2unix filters reading input from stdin and writing output to stdout.
 * Without arguments it reverts the format (e.i. if source is in UNIX format,
 * output is in DOS format and vice versa).
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * See the COPYING file for license information.
 */

#include <string.h>
#include <getopt.h>
#include "busybox.h"

// if fn is NULL then input is stdin and output is stdout
static int convert(char *fn, int ConvType) {
	int c;
	char *tempFn = NULL;
	FILE *in = stdin, *out = stdout;

	if (fn != NULL) {
		if ((in = wfopen(fn, "r")) == NULL) {
			return -1;
		}
		if ((out = tmpfile()) == NULL) {
			perror_msg(NULL);
			return -2;
		}
	}

	while ((c = fgetc(in)) != EOF) {
		if (c == '\r') {
			if ((ConvType == CT_UNIX2DOS) && (fn != NULL)) {
				// file is alredy in DOS format so it is not necessery to touch it
				if (fclose(in) < 0 || fclose(out) < 0) {
					perror_msg(NULL);
					return -2;
				}
				return 0;
			}
			if (!ConvType)
				ConvType = CT_DOS2UNIX;
			break;
		}
		if (c == '\n') {
			if ((ConvType == CT_DOS2UNIX) && (fn != NULL)) {
				// file is alredy in UNIX format so it is not necessery to touch it
				if ((fclose(in) < 0) || (fclose(out) < 0)) {
					perror_msg(NULL);
					return -2;
				}
				return 0;
			}
			if (!ConvType) {
				ConvType = CT_UNIX2DOS;
			}
			if (ConvType == CT_UNIX2DOS) {
				fputc('\r', out);
			}
			fputc('\n', out);
			break;
		}
		fputc(c, out);
	}
	if (c != EOF)
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
	    if (fclose(in) < 0 || fclose(out) < 0 || 
		    (in = fopen(tempFn, "r")) == NULL || (out = fopen(fn, "w")) == NULL) {
			perror_msg(NULL);
			return -2;
	    }

	    while ((c = fgetc(in)) != EOF) {
			fputc(c, out);
		}

	    if ((fclose(in) < 0) || (fclose(out) < 0)) {
			perror_msg(NULL);
			return -2;
	    }
	}

	return 0;
}

int dos2unix_main(int argc, char *argv[]) {
	int ConvType = CT_AUTO;
	int o;

	// process parameters
	while ((o = getopt(argc, argv, "du")) != EOF) {
		switch (o) {
		case 'd':
			ConvType = CT_UNIX2DOS;
			break;
		case 'u':
			ConvType = CT_DOS2UNIX;
			break;
		default:
			show_usage();
		}
	}

	if (optind < argc) {
		while(optind < argc)
			if ((o = convert(argv[optind++], ConvType)) < 0)
				break;
	}
	else
		o = convert(NULL, ConvType);

	return o;
}

