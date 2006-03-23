/*
 * od implementation for busybox
 * Based on code from util-linux v 2.11l
 *
 * Copyright (c) 1990
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

#include <ctype.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include "busybox.h"
#include "dump.h"

#define isdecdigit(c) (isdigit)(c)
#define ishexdigit(c) (isxdigit)(c)

static void
odoffset(int argc, char ***argvp)
{
	register char *num, *p;
	int base;
	char *end;

	/*
	 * The offset syntax of od(1) was genuinely bizarre.  First, if
	 * it started with a plus it had to be an offset.  Otherwise, if
	 * there were at least two arguments, a number or lower-case 'x'
	 * followed by a number makes it an offset.  By default it was
	 * octal; if it started with 'x' or '0x' it was hex.  If it ended
	 * in a '.', it was decimal.  If a 'b' or 'B' was appended, it
	 * multiplied the number by 512 or 1024 byte units.  There was
	 * no way to assign a block count to a hex offset.
	 *
	 * We assumes it's a file if the offset is bad.
	 */
	p = **argvp;

	if (!p) {
		/* hey someone is probably piping to us ... */
		return;
	}

	if ((*p != '+')
		&& (argc < 2
			|| (!isdecdigit(p[0])
				&& ((p[0] != 'x') || !ishexdigit(p[1])))))
		return;

	base = 0;
	/*
	 * bb_dump_skip over leading '+', 'x[0-9a-fA-f]' or '0x', and
	 * set base.
	 */
	if (p[0] == '+')
		++p;
	if (p[0] == 'x' && ishexdigit(p[1])) {
		++p;
		base = 16;
	} else if (p[0] == '0' && p[1] == 'x') {
		p += 2;
		base = 16;
	}

	/* bb_dump_skip over the number */
	if (base == 16)
		for (num = p; ishexdigit(*p); ++p);
	else
		for (num = p; isdecdigit(*p); ++p);

	/* check for no number */
	if (num == p)
		return;

	/* if terminates with a '.', base is decimal */
	if (*p == '.') {
		if (base)
			return;
		base = 10;
	}

	bb_dump_skip = strtol(num, &end, base ? base : 8);

	/* if end isn't the same as p, we got a non-octal digit */
	if (end != p)
		bb_dump_skip = 0;
	else {
		if (*p) {
			if (*p == 'b') {
				bb_dump_skip *= 512;
				++p;
			} else if (*p == 'B') {
				bb_dump_skip *= 1024;
				++p;
			}
		}
		if (*p)
			bb_dump_skip = 0;
		else {
			++*argvp;
			/*
			 * If the offset uses a non-octal base, the base of
			 * the offset is changed as well.  This isn't pretty,
			 * but it's easy.
			 */
#define	TYPE_OFFSET	7
			{
				char x_or_d;
				if (base == 16) {
					x_or_d = 'x';
					goto DO_X_OR_D;
				}
				if (base == 10) {
					x_or_d = 'd';
				DO_X_OR_D:
					bb_dump_fshead->nextfu->fmt[TYPE_OFFSET]
						= bb_dump_fshead->nextfs->nextfu->fmt[TYPE_OFFSET]
						= x_or_d;
				}
			}
		}
	}
}

static const char * const add_strings[] = {
	"16/1 \"%3_u \" \"\\n\"",				/* a */
	"8/2 \" %06o \" \"\\n\"",				/* B, o */
	"16/1 \"%03o \" \"\\n\"",				/* b */
	"16/1 \"%3_c \" \"\\n\"",				/* c */
	"8/2 \"  %05u \" \"\\n\"",				/* d */
	"4/4 \"     %010u \" \"\\n\"",			/* D */
	"2/8 \"          %21.14e \" \"\\n\"",	/* e (undocumented in od), F */
	"4/4 \" %14.7e \" \"\\n\"",				/* f */
	"4/4 \"       %08x \" \"\\n\"",			/* H, X */
	"8/2 \"   %04x \" \"\\n\"",				/* h, x */
	"4/4 \"    %11d \" \"\\n\"",			/* I, L, l */
	"8/2 \" %6d \" \"\\n\"",				/* i */
	"4/4 \"    %011o \" \"\\n\"",			/* O */
};

static const signed char od_opts[] = "aBbcDdeFfHhIiLlOoXxv";

static const signed char od_o2si[] = {
	0, 1, 2, 3, 5,
	4, 6, 6, 7, 8,
	9, 0xa, 0xb, 0xa, 0xa,
	0xb, 1, 8, 9,
};

int od_main(int argc, char **argv)
{
	int ch;
	int first = 1;
	signed char *p;
	bb_dump_vflag = FIRST;
	bb_dump_length = -1;

	while ((ch = getopt(argc, argv, od_opts)) > 0) {
		if (ch == 'v') {
			bb_dump_vflag = ALL;
		} else if (((p = strchr(od_opts, ch)) != NULL) && (*p >= 0)) {
			if (first) {
				first = 0;
				bb_dump_add("\"%07.7_Ao\n\"");
				bb_dump_add("\"%07.7_ao  \"");
			} else {
				bb_dump_add("\"         \"");
			}
			bb_dump_add(add_strings[od_o2si[(int)(p-od_opts)]]);
		} else {	/* P, p, s, w, or other unhandled */
			bb_show_usage();
		}
	}
	if (!bb_dump_fshead) {
		bb_dump_add("\"%07.7_Ao\n\"");
		bb_dump_add("\"%07.7_ao  \" 8/2 \"%06o \" \"\\n\"");
	}

	argc -= optind;
	argv += optind;

	odoffset(argc, &argv);

	return(bb_dump_dump(argv));
}

/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
