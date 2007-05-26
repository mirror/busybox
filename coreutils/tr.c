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
/* http://www.opengroup.org/onlinepubs/009695399/utilities/tr.html
 * TODO: xdigit, graph, print
 */
#include "libbb.h"

#define ASCII 0377

#define TR_OPT_complement	(1<<0)
#define TR_OPT_delete		(1<<1)
#define TR_OPT_squeeze_reps	(1<<2)
/* some "globals" shared across this file */
/* these last are pointers to static buffers declared in tr_main */
static char *poutput, *pvector, *pinvec, *poutvec;

static void ATTRIBUTE_NORETURN convert(const smalluint flags)
{
	size_t read_chars = 0, in_index = 0, out_index = 0, c, coded, last = -1;

	for (;;) {
		/* If we're out of input, flush output and read more input. */
		if (in_index == read_chars) {
			if (out_index) {
				xwrite(STDOUT_FILENO, (char *)poutput, out_index);
				out_index = 0;
			}
			if ((read_chars = read(STDIN_FILENO, bb_common_bufsiz1, BUFSIZ)) <= 0) {
				if (write(STDOUT_FILENO, (char *)poutput, out_index) != out_index)
					bb_perror_msg(bb_msg_write_error);
				exit(EXIT_SUCCESS);
			}
			in_index = 0;
		}
		c = bb_common_bufsiz1[in_index++];
		coded = pvector[c];
		if ((flags & TR_OPT_delete) && pinvec[c])
			continue;
		if ((flags & TR_OPT_squeeze_reps) && last == coded &&
			(pinvec[c] || poutvec[coded]))
			continue;
		poutput[out_index++] = last = coded;
	}
	/* NOTREACHED */
}

static void map(unsigned char *string1, unsigned int string1_len,
		unsigned char *string2, unsigned int string2_len)
{
	char last = '0';
	unsigned int i, j;

	for (j = 0, i = 0; i < string1_len; i++) {
		if (string2_len <= j)
			pvector[string1[i]] = last;
		else
			pvector[string1[i]] = last = string2[j++];
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
	unsigned i; /* XXX: FIXME: use unsigned char? */
	unsigned char ac;
#define CLO ":]"
	static const char * const classes[] = {
		"alpha"CLO, "alnum"CLO, "digit"CLO, "lower"CLO, "upper"CLO, "space"CLO,
		"blank"CLO, "punct"CLO, "cntrl"CLO, NULL
	};
#define CLASS_invalid 0 /* we increment the retval */
#define CLASS_alpha 1
#define CLASS_alnum 2
#define CLASS_digit 3
#define CLASS_lower 4
#define CLASS_upper 5
#define CLASS_space 6
#define CLASS_blank 7
#define CLASS_punct 8
#define CLASS_cntrl 9
//#define CLASS_xdigit 10
//#define CLASS_graph 11
//#define CLASS_print 12
	while (*arg) {
		if (*arg == '\\') {
			arg++;
			*buffer++ = bb_process_escape_sequence(&arg);
		} else if (*(arg+1) == '-') {
			ac = *(arg+2);
			if (ac == 0) {
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
				smalluint j;
				{ /* not really pretty.. */
				char *tmp = xstrndup(arg, 7); // warning: xdigit needs 8, not 7
				j = index_in_str_array(classes, tmp) + 1;
				free(tmp);
				}
				if (j == CLASS_alnum || j == CLASS_digit) {
					for (i = '0'; i <= '9'; i++)
						*buffer++ = i;
				}
				if (j == CLASS_alpha || j == CLASS_alnum || j == CLASS_upper) {
					for (i = 'A'; i <= 'Z'; i++)
						*buffer++ = i;
				}
				if (j == CLASS_alpha || j == CLASS_alnum || j == CLASS_lower) {
					for (i = 'a'; i <= 'z'; i++)
						*buffer++ = i;
				}
				if (j == CLASS_space || j == CLASS_blank) {
					*buffer++ = '\t';
					if (j == CLASS_space) {
						*buffer++ = '\n';
						*buffer++ = '\v';
						*buffer++ = '\f';
						*buffer++ = '\r';
					}
					*buffer++ = ' ';
				}
				if (j == CLASS_punct || j == CLASS_cntrl) {
					for (i = 0; i <= ASCII; i++)
						if ((j == CLASS_punct &&
							 isprint(i) && (!isalnum(i)) && (!isspace(i))) ||
							(j == CLASS_cntrl && iscntrl(i)))
							*buffer++ = i;
				}
				if (j == CLASS_invalid) {
					*buffer++ = '[';
					*buffer++ = ':';
					continue;
				}
				break;
			}
			if (ENABLE_FEATURE_TR_EQUIV && i == '=') {
				*buffer++ = *arg;
				arg += 3;	/* Skip the closing =] */
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
			arg++;	/* Skip the assumed ']' */
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
	int output_length = 0, input_length;
	int idx = 1;
	int i;
	smalluint flags = 0;
	RESERVE_CONFIG_UBUFFER(output, BUFSIZ);
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
			if (*ptr == 'c')
				flags |= TR_OPT_complement;
			else if (*ptr == 'd')
				flags |= TR_OPT_delete;
			else if (*ptr == 's')
				flags |= TR_OPT_squeeze_reps;
			else
				bb_show_usage();
		}
		idx++;
	}
	for (i = 0; i <= ASCII; i++) {
		vector[i] = i;
		invec[i] = outvec[i] = FALSE;
	}

	if (argv[idx] != NULL) {
		input_length = expand(argv[idx++], bb_common_bufsiz1);
		if (flags & TR_OPT_complement)
			input_length = complement(bb_common_bufsiz1, input_length);
		if (argv[idx] != NULL) {
			if (*argv[idx] == '\0')
				bb_error_msg_and_die("STRING2 cannot be empty");
			output_length = expand(argv[idx], output);
			map(bb_common_bufsiz1, input_length, output, output_length);
		}
		for (i = 0; i < input_length; i++)
			invec[(unsigned char)bb_common_bufsiz1[i]] = TRUE;
		for (i = 0; i < output_length; i++)
			outvec[output[i]] = TRUE;
	}
	convert(flags);
	return EXIT_SUCCESS;
}
