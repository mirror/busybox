/*
 * Taken from dos2unix for BusyBox
 *
 * dos2unix '\n' convertor 0.5.0
 *   based on Unix2Dos 0.9.0 by Peter Hanecak (made 19.2.1997)
 * Copyright 1997,.. by Peter Hanecak <hanecak@megaloman.sk>.
 * All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "libbb.h"

// if fn is NULL then input is stdin and output is stdout
extern int convert(char *fn, int ConvType) {
	char c;
	char *tempFn = NULL;
	FILE *in = stdin, *out = stdout;

	if (fn != NULL) {
		if ((in = fopen(fn, "r")) == NULL) {
			perror_msg(fn);
			return -1;
		}
		tempFn = tmpnam(NULL);
		if (tempFn == NULL || (out = fopen(tempFn, "w")) == NULL) {
			perror_msg(NULL);
			return -2;
		}
	}

	while ((c = fgetc(in)) != EOF) {
		if (c == '\r') {
			if ((ConvType == CT_UNIX2DOS) && (fn != NULL)) {
				// file is alredy in DOS format so it is not necessery to touch it
				if (fclose(in) < 0 || fclose(out) < 0 || remove(tempFn) < 0) {
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
				if (fclose(in) < 0 || fclose(out) < 0 || remove(tempFn) < 0) {
					perror_msg(NULL);
					return -2;
				}
				return 0;
			}
			if (!ConvType)
				ConvType = CT_UNIX2DOS;
			if (ConvType == CT_UNIX2DOS)
				fputc('\r', out);
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

	    while ((c = fgetc(in)) != EOF)
		fputc(c, out);

	    if (fclose(in) < 0 || fclose(out) < 0 || remove(tempFn) < 0) {
		perror_msg(NULL);
		return -2;
	    }
	}

	return 0;
}
