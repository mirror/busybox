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
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/time.h>
#include "busybox.h"

#define CT_AUTO         0
#define CT_UNIX2DOS     1
#define CT_DOS2UNIX     2

/* We are making a lame pseudo-random string generator here.  in
 * convert(), each pass through the while loop will add more and more
 * stuff into value, which is _supposed_ to wrap.  We don't care about
 * it being accurate.  We care about it being messy, since we then mod
 * it by the sizeof(letters) and then use that as an index into letters
 * to pick a random letter to add to out temporary file. */
typedef unsigned long int bb_uint64_t;

static const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

// if fn is NULL then input is stdin and output is stdout
static int convert(char *fn, int ConvType) 
{
	int c, fd;
	struct timeval tv;
	char tempFn[BUFSIZ];
	static bb_uint64_t value=0;
	FILE *in = stdin, *out = stdout;

	if (fn != NULL) {
		in = bb_xfopen(fn, "rw");
		safe_strncpy(tempFn, fn, sizeof(tempFn));
		c = strlen(tempFn);
		tempFn[c] = '.';
		while(1) {
		    if (c >=BUFSIZ)
			bb_error_msg_and_die("unique name not found");
		    /* Get some semi random stuff to try and make a
		     * random filename based (and in the same dir as)
		     * the input file... */
		    gettimeofday (&tv, NULL);
		    value += ((bb_uint64_t) tv.tv_usec << 16) ^ tv.tv_sec ^ getpid ();
		    tempFn[++c] = letters[value % 62];
		    tempFn[c+1] = '\0';
		    value /= 62;

		    if ((fd = open(tempFn, O_RDWR | O_CREAT | O_EXCL, 0600)) < 0 ) {
			continue;
		    }
		    out = fdopen(fd, "w+");
		    if (!out) {
			close(fd);
			remove(tempFn);
			continue;
		    }
		    break;
		}
	}

	while ((c = fgetc(in)) != EOF) {
		if (c == '\r') {
			if ((ConvType == CT_UNIX2DOS) && (fn != NULL)) {
				// file is alredy in DOS format so it is not necessery to touch it
				remove(tempFn);
				if (fclose(in) < 0 || fclose(out) < 0) {
					bb_perror_nomsg();
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
				remove(tempFn);
				if ((fclose(in) < 0) || (fclose(out) < 0)) {
					bb_perror_nomsg();
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
	    if (fclose(in) < 0 || fclose(out) < 0) {
		bb_perror_nomsg();
		remove(tempFn);
		return -2;
	    }

	    /* Assume they are both on the same filesystem (which
	     * should be true since we put them into the same directory
	     * so we _should_ be ok, but you never know... */
	    if (rename(tempFn, fn) < 0) {
		bb_perror_msg("unable to rename '%s' as '%s'", tempFn, fn);
		return -1;
	    }
	}

	return 0;
}

int dos2unix_main(int argc, char *argv[]) 
{
	int ConvType = CT_AUTO;
	int o;

	//See if we are supposed to be doing dos2unix or unix2dos 
	if (argv[0][0]=='d') {
	    ConvType = CT_DOS2UNIX;
	}
	if (argv[0][0]=='u') {
	    ConvType = CT_UNIX2DOS;
	}

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
			bb_show_usage();
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

