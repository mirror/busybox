/* vi: set sw=4 ts=4: */
/*
 * Mini tr implementation for busybox
 *
 * This version of tr is adapted from Minix tr
 * Author: Michiel Huisjes
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

#include "internal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>



#ifdef TRUE
#undef TRUE
#undef FALSE
#define TRUE	1
#define FALSE	0
#endif

#define ASCII		0377

/* some glabals shared across this file */
static char com_fl, del_fl, sq_fl;
static unsigned char output[BUFSIZ], input[BUFSIZ];
static unsigned char vector[ASCII + 1];
static char invec[ASCII + 1], outvec[ASCII + 1];
static short in_index, out_index;


static void convert()
{
	short read_chars = 0;
	short c, coded;
	short last = -1;

	for (;;) {
		if (in_index == read_chars) {
			if ((read_chars = read(0, (char *) input, BUFSIZ)) <= 0) {
				if (write(1, (char *) output, out_index) != out_index)
					write(2, "Bad write\n", 10);
				exit(0);
			}
			in_index = 0;
		}
		c = input[in_index++];
		coded = vector[c];
		if (del_fl && invec[c])
			continue;
		if (sq_fl && last == coded && outvec[coded])
			continue;
		output[out_index++] = last = coded;
		if (out_index == BUFSIZ) {
			if (write(1, (char *) output, out_index) != out_index) {
				write(2, "Bad write\n", 10);
				exit(1);
			}
			out_index = 0;
		}
	}

	/* NOTREACHED */
}

static void map(register unsigned char *string1, register unsigned char *string2)
{
	unsigned char last = '0';

	while (*string1) {
		if (*string2 == '\0')
			vector[*string1] = last;
		else
			vector[*string1] = last = *string2++;
		string1++;
	}
}

static void expand(register char *arg, register unsigned char *buffer)
{
	int i, ac;

	while (*arg) {
		if (*arg == '\\') {
			arg++;
			i = ac = 0;
			if (*arg >= '0' && *arg <= '7') {
				do {
					ac = (ac << 3) + *arg++ - '0';
					i++;
				} while (i < 4 && *arg >= '0' && *arg <= '7');
				*buffer++ = ac;
			} else if (*arg != '\0')
				*buffer++ = *arg++;
		} else if (*arg == '[') {
			arg++;
			i = *arg++;
			if (*arg++ != '-') {
				*buffer++ = '[';
				arg -= 2;
				continue;
			}
			ac = *arg++;
			while (i <= ac)
				*buffer++ = i++;
			arg++;				/* Skip ']' */
		} else
			*buffer++ = *arg++;
	}
}

static void complement(unsigned char *buffer)
{
	register unsigned char *ptr;
	register short i, index;
	unsigned char conv[ASCII + 2];

	index = 0;
	for (i = 1; i <= ASCII; i++) {
		for (ptr = buffer; *ptr; ptr++)
			if (*ptr == i)
				break;
		if (*ptr == '\0')
			conv[index++] = i & ASCII;
	}
	conv[index] = '\0';
	strcpy((char *) buffer, (char *) conv);
}

extern int tr_main(int argc, char **argv)
{
	register unsigned char *ptr;
	int index = 1;
	short i;

	if (argc > 1 && argv[index][0] == '-') {
		for (ptr = (unsigned char *) &argv[index][1]; *ptr; ptr++) {
			switch (*ptr) {
			case 'c':
				com_fl = TRUE;
				break;
			case 'd':
				del_fl = TRUE;
				break;
			case 's':
				sq_fl = TRUE;
				break;
			default:
				usage("tr [-cds] STRING1 [STRING2]\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
					  "\nTranslate, squeeze, and/or delete characters from\n"
					  "standard input, writing to standard output.\n\n"
					  "Options:\n"
					  "\t-c\ttake complement of STRING1\n"
					  "\t-d\tdelete input characters coded STRING1\n"
					  "\t-s\tsqueeze multiple output characters of STRING2 into one character\n"
#endif
					  );
			}
		}
		index++;
	}
	for (i = 0; i <= ASCII; i++) {
		vector[i] = i;
		invec[i] = outvec[i] = FALSE;
	}

	if (argv[index] != NULL) {
		expand(argv[index++], input);
		if (com_fl)
			complement(input);
		if (argv[index] != NULL)
			expand(argv[index], output);
		if (argv[index] != NULL)
			map(input, output);
		for (ptr = input; *ptr; ptr++)
			invec[*ptr] = TRUE;
		for (ptr = output; *ptr; ptr++)
			outvec[*ptr] = TRUE;
	}
	convert();
	return (0);
}

/*
 * Copyright (c) 1987,1997, Prentice Hall
 * All rights reserved.
 * 
 * Redistribution and use of the MINIX operating system in source and
 * binary forms, with or without modification, are permitted provided
 * that the following conditions are met:
 * 
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 * 
 * Neither the name of Prentice Hall nor the names of the software
 * authors or contributors may be used to endorse or promote
 * products derived from this software without specific prior
 * written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS, AUTHORS, AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL PRENTICE HALL OR ANY AUTHORS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

