/* vi: set sw=4 ts=4: */
/*
 * Mini tr implementation for busybox
 *
 ** Copyright (c) 1987,1997, Prentice Hall   All rights reserved.
 *
 * The name of Prentice Hall may not be used to endorse or promote
 * products derived from this software without specific prior
 * written permission.
 *
 * Copyright (c) Michiel Huisjes
 *
 * This version of tr is adapted from Minix tr and was modified
 * by Erik Andersen <andersen@codepoet.org> to be used in busybox.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"

// Even with -funsigned-char, gcc still complains about char as an array index.

#define GCC4_IS_STUPID int

#define ASCII 0377

/* some "globals" shared across this file */
static char com_fl, del_fl, sq_fl;
/* these last are pointers to static buffers declared in tr_main */
static char *poutput, *pvector, *pinvec, *poutvec;

static void convert(void)
{
	int read_chars = 0, in_index = 0, out_index = 0, c, coded, last = -1;

	for (;;) {
		// If we're out of input, flush output and read more input.

		if (in_index == read_chars) {
			if (out_index) {
				if (write(1, (char *) poutput, out_index) != out_index)
					bb_error_msg_and_die(bb_msg_write_error);
				out_index = 0;
			}

			if ((read_chars = read(0, bb_common_bufsiz1, BUFSIZ)) <= 0) {
				if (write(1, (char *) poutput, out_index) != out_index)
					bb_error_msg(bb_msg_write_error);
				exit(0);
			}
			in_index = 0;
		}
		c = bb_common_bufsiz1[in_index++];
		coded = pvector[c];
		if (del_fl && pinvec[c])
			continue;
		if (sq_fl && last == coded && (pinvec[c] || poutvec[coded]))
			continue;
		poutput[out_index++] = last = coded;
	}

	/* NOTREACHED */
}

static void map(char *string1, unsigned int string1_len,
		char *string2, unsigned int string2_len)
{
	char last = '0';
	unsigned int i, j;

	for (j = 0, i = 0; i < string1_len; i++) {
		if (string2_len <= j)
			pvector[(GCC4_IS_STUPID)string1[i]] = last;
		else
			pvector[(GCC4_IS_STUPID)string1[i]] = last = string2[j++];
	}
}

/* supported constructs:
 *   Ranges,  e.g.,  [0-9]  ==>  0123456789
 *   Escapes, e.g.,  \a     ==>  Control-G
 *	 Character classes, e.g. [:upper:] ==> A ... Z
 */
static unsigned int expand(const char *arg, char *buffer)
{
	char *buffer_start = buffer;
	int i, ac;

	while (*arg) {
		if (*arg == '\\') {
			arg++;
			*buffer++ = bb_process_escape_sequence(&arg);
		} else if (*(arg+1) == '-') {
			ac = *(arg+2);
			if(ac == 0) {
				*buffer++ = *arg++;
				continue;
			}
			i = *arg;
			while (i <= ac)
				*buffer++ = i++;
			arg += 3; /* Skip the assumed a-z */
		} else if (*arg == '[') {
			arg++;
			i = *arg++;
			if (ENABLE_FEATURE_TR_CLASSES && i == ':') {
				if (strncmp(arg, "alpha", 5) == 0) {
					for (i = 'A'; i <= 'Z'; i++)
						*buffer++ = i;
					for (i = 'a'; i <= 'z'; i++)
						*buffer++ = i;
				}
				else if (strncmp(arg, "alnum", 5) == 0) {
					for (i = '0'; i <= '9'; i++)
						*buffer++ = i;
					for (i = 'A'; i <= 'Z'; i++)
						*buffer++ = i;
					for (i = 'a'; i <= 'z'; i++)
						*buffer++ = i;
				}
				else if (strncmp(arg, "digit", 5) == 0)
					for (i = '0'; i <= '9'; i++)
						*buffer++ = i;
				else if (strncmp(arg, "lower", 5) == 0)
					for (i = 'a'; i <= 'z'; i++)
						*buffer++ = i;
				else if (strncmp(arg, "upper", 5) == 0)
					for (i = 'A'; i <= 'Z'; i++)
						*buffer++ = i;
				else if (strncmp(arg, "space", 5) == 0) {
					const char s[] = "\t\n\v\f\r ";
					strcat((char*)buffer, s);
					buffer += sizeof(s) - 1;
				}
				else if (strncmp(arg, "blank", 5) == 0) {
					*buffer++ = '\t';
					*buffer++ = ' ';
				}
				/* gcc gives a warning if braces aren't used here */
				else if (strncmp(arg, "punct", 5) == 0) {
					for (i = 0; i <= ASCII; i++)
						if (isprint(i) && (!isalnum(i)) && (!isspace(i)))
							*buffer++ = i;
				}
				else if (strncmp(arg, "cntrl", 5) == 0) {
					for (i = 0; i <= ASCII; i++)
						if (iscntrl(i))
							*buffer++ = i;
				}
				else {
					*buffer++ = '[';
					*buffer++ = ':';
					continue;
				}
				break;
			}
			if (ENABLE_FEATURE_TR_EQUIV && i == '=') {
				*buffer++ = *arg;
				/* skip the closing =] */
				arg += 3;
				continue;
			}
			if (*arg++ != '-') {
				*buffer++ = '[';
				arg -= 2;
				continue;
			}
			ac = *arg++;
			while (i <= ac)
				*buffer++ = i++;
			arg++;				/* Skip the assumed ']' */
		} else
			*buffer++ = *arg++;
	}

	return (buffer - buffer_start);
}

static int complement(char *buffer, int buffer_len)
{
	short i, j, ix;
	char conv[ASCII + 2];

	ix = 0;
	for (i = 0; i <= ASCII; i++) {
		for (j = 0; j < buffer_len; j++)
			if (buffer[j] == i)
				break;
		if (j == buffer_len)
			conv[ix++] = i & ASCII;
	}
	memcpy(buffer, conv, ix);
	return ix;
}

int tr_main(int argc, char **argv);
int tr_main(int argc, char **argv)
{
	unsigned char *ptr;
	int output_length=0, input_length;
	int idx = 1;
	int i;
	RESERVE_CONFIG_BUFFER(output, BUFSIZ);
	RESERVE_CONFIG_BUFFER(vector, ASCII+1);
	RESERVE_CONFIG_BUFFER(invec,  ASCII+1);
	RESERVE_CONFIG_BUFFER(outvec, ASCII+1);

	/* ... but make them available globally */
	poutput = output;
	pvector = vector;
	pinvec  = invec;
	poutvec = outvec;

	if (argc > 1 && argv[idx][0] == '-') {
		for (ptr = (unsigned char *) &argv[idx][1]; *ptr; ptr++) {
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
				bb_show_usage();
			}
		}
		idx++;
	}
	for (i = 0; i <= ASCII; i++) {
		vector[i] = i;
		invec[i] = outvec[i] = FALSE;
	}

	if (argv[idx] != NULL) {
		input_length = expand(argv[idx++], bb_common_bufsiz1);
		if (com_fl)
			input_length = complement(bb_common_bufsiz1, input_length);
		if (argv[idx] != NULL) {
			if (*argv[idx] == '\0')
				bb_error_msg_and_die("STRING2 cannot be empty");
			output_length = expand(argv[idx], output);
			map(bb_common_bufsiz1, input_length, output, output_length);
		}
		for (i = 0; i < input_length; i++)
			invec[(GCC4_IS_STUPID)bb_common_bufsiz1[i]] = TRUE;
		for (i = 0; i < output_length; i++)
			outvec[(GCC4_IS_STUPID)output[i]] = TRUE;
	}
	convert();
	return 0;
}
