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
#include <getopt.h>
#include <stdlib.h>
#include "dump.h"
#include "busybox.h"

extern FS *fshead;				/* head of format strings */
extern int blocksize;				/* data block size */
extern int length;			/* max bytes to read */

#define	ishexdigit(c) \
	((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))

static void
odoffset(int argc, char ***argvp)
{
	extern off_t skip;
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
	if (*p != '+' && (argc < 2 ||
	    (!isdigit(p[0]) && (p[0] != 'x' || !ishexdigit(p[1])))))
		return;

	base = 0;
	/*
	 * skip over leading '+', 'x[0-9a-fA-f]' or '0x', and
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

	/* skip over the number */
	if (base == 16)
		for (num = p; ishexdigit(*p); ++p);
	else
		for (num = p; isdigit(*p); ++p);

	/* check for no number */
	if (num == p)
		return;

	/* if terminates with a '.', base is decimal */
	if (*p == '.') {
		if (base)
			return;
		base = 10;
	}

	skip = strtol(num, &end, base ? base : 8);

	/* if end isn't the same as p, we got a non-octal digit */
	if (end != p)
		skip = 0;
	else {
		if (*p) {
			if (*p == 'b')
				skip *= 512;
			else if (*p == 'B')
				skip *= 1024;
			++p;
		}
		if (*p)
			skip = 0;
		else {
			++*argvp;
			/*
			 * If the offset uses a non-octal base, the base of
			 * the offset is changed as well.  This isn't pretty,
			 * but it's easy.
			 */
#define	TYPE_OFFSET	7
			if (base == 16) {
				fshead->nextfu->fmt[TYPE_OFFSET] = 'x';
				fshead->nextfs->nextfu->fmt[TYPE_OFFSET] = 'x';
			} else if (base == 10) {
				fshead->nextfu->fmt[TYPE_OFFSET] = 'd';
				fshead->nextfs->nextfu->fmt[TYPE_OFFSET] = 'd';
			}
		}
	}
}

static void odprecede(void)
{
	static int first = 1;

	if (first) {
		first = 0;
		add("\"%07.7_Ao\n\"");
		add("\"%07.7_ao  \"");
	} else
		add("\"         \"");
}

int od_main(int argc, char **argv)
{
	int ch;
	extern enum _vflag vflag;
	vflag = FIRST;
	length = -1;

	while ((ch = getopt(argc, argv, "aBbcDdeFfHhIiLlOoPpswvXx")) != EOF)
		switch (ch) {
		case 'a':
			odprecede();
			add("16/1 \"%3_u \" \"\\n\"");
			break;
		case 'B':
		case 'o':
			odprecede();
			add("8/2 \" %06o \" \"\\n\"");
			break;
		case 'b':
			odprecede();
			add("16/1 \"%03o \" \"\\n\"");
			break;
		case 'c':
			odprecede();
			add("16/1 \"%3_c \" \"\\n\"");
			break;
		case 'd':
			odprecede();
			add("8/2 \"  %05u \" \"\\n\"");
			break;
		case 'D':
			odprecede();
			add("4/4 \"     %010u \" \"\\n\"");
			break;
		case 'e':		/* undocumented in od */
		case 'F':
			odprecede();
			add("2/8 \"          %21.14e \" \"\\n\"");
			break;
			
		case 'f':
			odprecede();
			add("4/4 \" %14.7e \" \"\\n\"");
			break;
		case 'H':
		case 'X':
			odprecede();
			add("4/4 \"       %08x \" \"\\n\"");
			break;
		case 'h':
		case 'x':
			odprecede();
			add("8/2 \"   %04x \" \"\\n\"");
			break;
		case 'I':
		case 'L':
		case 'l':
			odprecede();
			add("4/4 \"    %11d \" \"\\n\"");
			break;
		case 'i':
			odprecede();
			add("8/2 \" %6d \" \"\\n\"");
			break;
		case 'O':
			odprecede();
			add("4/4 \"    %011o \" \"\\n\"");
			break;
		case 'v':
			vflag = ALL;
			break;
		case 'P':
		case 'p':
		case 's':
		case 'w':
		case '?':
		default:
			error_msg("od: od(1) has been deprecated for hexdump(1).\n");
			if (ch != '?') {
				error_msg("od: hexdump(1) compatibility doesn't support the -%c option%s\n",
				    ch, ch == 's' ? "; see strings(1)." : ".");
			}
			show_usage();
		}

	if (!fshead) {
		add("\"%07.7_Ao\n\"");
		add("\"%07.7_ao  \" 8/2 \"%06o \" \"\\n\"");
	}

	argc -= optind;
	argv += optind;

	odoffset(argc, &argv);

	return(dump(argv));
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
