/* vi: set sw=4 ts=4: */
/*
 * Mini tr implementation for busybox
 *
 * Copyright (c) Michiel Huisjes
 *
 * This version of tr is adapted from Minix tr and was modified 
 * by Erik Andersen <andersee@debian.org> to be used in busybox.
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

#include "busybox.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#define BB_DECLARE_EXTERN
#define bb_need_write_error
#include "messages.c"

static const int ASCII = 0377;

/* some glabals shared across this file */
static char com_fl, del_fl, sq_fl;
static short in_index, out_index;
/* these last are pointers to static buffers declared in tr_main */
static unsigned char *poutput, *pinput;
static unsigned char *pvector;
static char *pinvec, *poutvec;


static void convert()
{
	short read_chars = 0;
	short c, coded;
	short last = -1;

	for (;;) {
		if (in_index == read_chars) {
			if ((read_chars = read(0, (char *) pinput, BUFSIZ)) <= 0) {
				if (write(1, (char *) poutput, out_index) != out_index)
					write(2, write_error, strlen(write_error));
				exit(0);
			}
			in_index = 0;
		}
		c = pinput[in_index++];
		coded = pvector[c];
		if (del_fl && pinvec[c])
			continue;
		if (sq_fl && last == coded && (pinvec[c] || poutvec[coded]))
			continue;
		poutput[out_index++] = last = coded;
		if (out_index == BUFSIZ) {
			if (write(1, (char *) poutput, out_index) != out_index) {
				write(2, write_error, strlen(write_error));
				exit(1);
			}
			out_index = 0;
		}
	}

	/* NOTREACHED */
}

static void map(register unsigned char *string1, unsigned int string1_len,
		register unsigned char *string2, unsigned int string2_len)
{
	unsigned char last = '0';
	unsigned int i, j;

	for (j = 0, i = 0; i < string1_len; i++) {
		if (string2_len <= j)
			pvector[string1[i]] = last;
		else
			pvector[string1[i]] = last = string2[j++];
	}
}

static unsigned int expand(char *arg, register unsigned char *buffer)
{
	unsigned char *buffer_start = buffer;
	int i, ac;

	while (*arg) {
		if (*arg == '\\') {
			arg++;
			*buffer++ = process_escape_sequence(&arg);
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

	return (buffer - buffer_start);
}

static int complement(unsigned char *buffer, int buffer_len)
{
	register short i, j, index;
	char conv[ASCII + 2];

	index = 0;
	for (i = 0; i <= ASCII; i++) {
		for (j = 0; j < buffer_len; j++)
			if (buffer[j] == i)
				break;
		if (j == buffer_len)
			conv[index++] = i & ASCII;
	}
	memcpy(buffer, conv, index);
	return index;
}

extern int tr_main(int argc, char **argv)
{
	register unsigned char *ptr;
	int output_length=0, input_length;
	int index = 1;
	int i;
	RESERVE_BB_BUFFER(output, BUFSIZ);
	RESERVE_BB_BUFFER(input,  BUFSIZ);
	RESERVE_BB_UBUFFER(vector, ASCII+1);
	RESERVE_BB_BUFFER(invec,  ASCII+1);
	RESERVE_BB_BUFFER(outvec, ASCII+1);

	/* ... but make them available globally */
	poutput = output;
	pinput  = input;
	pvector = vector;
	pinvec  = invec;
	poutvec = outvec;

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
				usage(tr_usage);
			}
		}
		index++;
	}
	for (i = 0; i <= ASCII; i++) {
		vector[i] = i;
		invec[i] = outvec[i] = FALSE;
	}

	if (argv[index] != NULL) {
		input_length = expand(argv[index++], input);
		if (com_fl)
			input_length = complement(input, input_length);
		if (argv[index] != NULL) {
			if (*argv[index] == '\0')
				error_msg_and_die("STRING2 cannot be empty\n");
			output_length = expand(argv[index], output);
			map(input, input_length, output, output_length);
		}
		for (i = 0; i < input_length; i++)
			invec[input[i]] = TRUE;
		for (i = 0; i < output_length; i++)
			outvec[output[i]] = TRUE;
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

