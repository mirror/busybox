/*
 * hexdump implementation for busybox
 * Based on code from util-linux v 2.11l
 *
 * Copyright (c) 1989
 *	The Regents of the University of California.  All rights reserved.
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
 * Original copyright notice is retained at the end of this file.
 */

#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include "busybox.h"
#include "dump.h"

static void bb_dump_addfile(char *name)
{
	register char *p;
	FILE *fp;
	char *buf;

	fp = bb_xfopen(name, "r");

	while ((buf = bb_get_chomped_line_from_file(fp)) != NULL) {
		p = (char *) bb_skip_whitespace(buf);

		if (*p && (*p != '#')) {
			bb_dump_add(p);
		}
		free(buf);
	}
	fclose(fp);
}

static const char * const add_strings[] = {
			"\"%07.7_ax \" 16/1 \"%03o \" \"\\n\"",		/* b */
			"\"%07.7_ax \" 16/1 \"%3_c \" \"\\n\"",		/* c */
			"\"%07.7_ax \" 8/2 \"  %05u \" \"\\n\"",	/* d */
			"\"%07.7_ax \" 8/2 \" %06o \" \"\\n\"",		/* o */
			"\"%07.7_ax \" 8/2 \"   %04x \" \"\\n\"",	/* x */
};

static const char add_first[] = "\"%07.7_Ax\n\"";

static const char hexdump_opts[] = "bcdoxe:f:n:s:v";

static const struct suffix_mult suffixes[] = {
	{"b",  512 },
	{"k",  1024 },
	{"m",  1024*1024 },
	{NULL, 0 }
};

int hexdump_main(int argc, char **argv)
{
//	register FS *tfs;
	const char *p;
	int ch;

	bb_dump_vflag = FIRST;
	bb_dump_length = -1;

	while ((ch = getopt(argc, argv, hexdump_opts)) > 0) {
		if ((p = strchr(hexdump_opts, ch)) != NULL) {
			if ((p - hexdump_opts) < 5) {
				bb_dump_add(add_first);
				bb_dump_add(add_strings[(int)(p - hexdump_opts)]);
			} else {
				/* Sae a little bit of space below by omitting the 'else's. */
				if (ch == 'e') {
					bb_dump_add(optarg);
				} /* else */
				if (ch == 'f') {
					bb_dump_addfile(optarg);
				} /* else */
				if (ch == 'n') {
					bb_dump_length = bb_xgetularg10_bnd(optarg, 0, INT_MAX);
				} /* else */
				if (ch == 's') {
					bb_dump_skip = bb_xgetularg_bnd_sfx(optarg, 10, 0, LONG_MAX, suffixes);
				} /* else */
				if (ch == 'v') {
					bb_dump_vflag = ALL;
				}
			}
		} else {
			bb_show_usage();
		}
	}

	if (!bb_dump_fshead) {
		bb_dump_add(add_first);
		bb_dump_add("\"%07.7_ax \" 8/2 \"%04x \" \"\\n\"");
	}

	argv += optind;

	return(bb_dump_dump(argv));
}
/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
