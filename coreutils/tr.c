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

static void map(char *pvector,
		unsigned char *string1, unsigned int string1_len,
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
 *   Ranges,  e.g.,  0-9   ==>  0123456789
 *   Ranges,  e.g.,  [0-9] ==>  0123456789
 *   Escapes, e.g.,  \a    ==>  Control-G
 *   Character classes, e.g. [:upper:] ==> A...Z
 *   Equiv classess, e.g. [=A=] ==> A   (hmmmmmmm?)
 */
static unsigned int expand(const char *arg, char *buffer)
{
	char *buffer_start = buffer;
	unsigned i; /* can't be unsigned char: must be able to hold 256 */
	unsigned char ac;

	while (*arg) {
		if (*arg == '\\') {
			arg++;
			*buffer++ = bb_process_escape_sequence(&arg);
			continue;
		}
		if (arg[1] == '-') { /* "0-9..." */
			ac = arg[2];
			if (ac == '\0') { /* "0-": copy verbatim */
				*buffer++ = *arg++; /* copy '0' */
				continue; /* next iter will copy '-' and stop */
			}
			i = *arg;
			while (i <= ac) /* ok: i is unsigned _int_ */
				*buffer++ = i++;
			arg += 3; /* skip 0-9 */
			continue;
		}
		if (*arg == '[') { /* "[xyz..." */
			arg++;
			i = *arg++;
			/* "[xyz...", i=x, arg points to y */
			if (ENABLE_FEATURE_TR_CLASSES && i == ':') {
#define CLO ":]\0"
				static const char classes[] ALIGN1 =
					"alpha"CLO "alnum"CLO "digit"CLO
					"lower"CLO "upper"CLO "space"CLO
					"blank"CLO "punct"CLO "cntrl"CLO;
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
				smalluint j;
				{ /* not really pretty.. */
					char *tmp = xstrndup(arg, 7); // warning: xdigit would need 8, not 7
					j = index_in_strings(classes, tmp) + 1;
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
					for (i = '\0'; i <= ASCII; i++)
						if ((j == CLASS_punct && isprint(i) && !isalnum(i) && !isspace(i))
						 || (j == CLASS_cntrl && iscntrl(i)))
							*buffer++ = i;
				}
				if (j == CLASS_invalid) {
					*buffer++ = '[';
					*buffer++ = ':';
					continue;
				}
				break;
			}
			/* "[xyz...", i=x, arg points to y */
			if (ENABLE_FEATURE_TR_EQUIV && i == '=') { /* [=CHAR=] */
				*buffer++ = *arg; /* copy CHAR */
				arg += 3;	/* skip CHAR=] */
				continue;
			}
			if (*arg != '-') { /* not [x-...] - copy verbatim */
				*buffer++ = '[';
				arg--; /* points to x */
				continue; /* copy all, including eventual ']' */
			}
			/* [x-y...] */
			arg++;
			ac = *arg++;
			while (i <= ac)
				*buffer++ = i++;
			arg++;	/* skip the assumed ']' */
			continue;
		}
		*buffer++ = *arg++;
	}
	return (buffer - buffer_start);
}

static int complement(char *buffer, int buffer_len)
{
	int i, j, ix;
	char conv[ASCII + 2];

	ix = 0;
	for (i = '\0'; i <= ASCII; i++) {
		for (j = 0; j < buffer_len; j++)
			if (buffer[j] == i)
				break;
		if (j == buffer_len)
			conv[ix++] = i & ASCII;
	}
	memcpy(buffer, conv, ix);
	return ix;
}

int tr_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int tr_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	int output_length = 0, input_length;
	int i;
	smalluint flags;
	ssize_t read_chars = 0;
	size_t in_index = 0, out_index = 0;
	unsigned last = UCHAR_MAX + 1; /* not equal to any char */
	unsigned char coded, c;
	unsigned char *output = xmalloc(BUFSIZ);
	char *vector = xzalloc((ASCII+1) * 3);
	char *invec  = vector + (ASCII+1);
	char *outvec = vector + (ASCII+1) * 2;

#define TR_OPT_complement	(1 << 0)
#define TR_OPT_delete		(1 << 1)
#define TR_OPT_squeeze_reps	(1 << 2)

	flags = getopt32(argv, "+cds"); /* '+': stop at first non-option */
	argv += optind;

	for (i = 0; i <= ASCII; i++) {
		vector[i] = i;
		/*invec[i] = outvec[i] = FALSE; - done by xzalloc */
	}

#define tr_buf bb_common_bufsiz1
	if (*argv != NULL) {
		input_length = expand(*argv++, tr_buf);
		if (flags & TR_OPT_complement)
			input_length = complement(tr_buf, input_length);
		if (*argv) {
			if (argv[0][0] == '\0')
				bb_error_msg_and_die("STRING2 cannot be empty");
			output_length = expand(*argv, (char *)output);
			map(vector, (unsigned char *)tr_buf, input_length, output, output_length);
		}
		for (i = 0; i < input_length; i++)
			invec[(unsigned char)tr_buf[i]] = TRUE;
		for (i = 0; i < output_length; i++)
			outvec[output[i]] = TRUE;
	}

	for (;;) {
		/* If we're out of input, flush output and read more input. */
		if ((ssize_t)in_index == read_chars) {
			if (out_index) {
				xwrite(STDOUT_FILENO, (char *)output, out_index);
				out_index = 0;
			}
			read_chars = safe_read(STDIN_FILENO, tr_buf, BUFSIZ);
			if (read_chars <= 0) {
				if (read_chars < 0)
					bb_perror_msg_and_die(bb_msg_read_error);
				exit(EXIT_SUCCESS);
			}
			in_index = 0;
		}
		c = tr_buf[in_index++];
		coded = vector[c];
		if ((flags & TR_OPT_delete) && invec[c])
			continue;
		if ((flags & TR_OPT_squeeze_reps) && last == coded
		 && (invec[c] || outvec[coded]))
			continue;
		output[out_index++] = last = coded;
	}
	/* NOTREACHED */
	return EXIT_SUCCESS;
}
