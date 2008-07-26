/* vi: set sw=4 ts=4: */
/*
 * config file parser helper
 *
 * Copyright (C) 2008 by Vladimir Dronnikov <dronnikov@gmail.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

#if ENABLE_PARSE
int parse_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int parse_main(int argc UNUSED_PARAM, char **argv)
{
	const char *delims = "# \t";
	unsigned flags = PARSE_NORMAL;
	int mintokens = 0, ntokens = 128;

	opt_complementary = "-1:n+:m+:f+";
	getopt32(argv, "n:m:d:f:", &ntokens, &mintokens, &delims, &flags);
	//argc -= optind;
	argv += optind;
	while (*argv) {
		parser_t *p = config_open(*argv);
		if (p) {
			int n;
			char **t = xmalloc(sizeof(char *) * ntokens);
			while ((n = config_read(p, t, ntokens, mintokens, delims, flags)) != 0) {
				for (int i = 0; i < n; ++i)
					printf("[%s]", t[i]);
				puts("");
			}
			config_close(p);
		}
		argv++;
	}
	return EXIT_SUCCESS;
}
#endif

/*

Typical usage:

----- CUT -----
	char *t[3];	// tokens placeholder
	parser_t *p = config_open(filename);
	if (p) {
		// parse line-by-line
		while (config_read(p, t, 3, 0, delimiters, flags)) { // 1..3 tokens
			// use tokens
			bb_error_msg("TOKENS: [%s][%s][%s]", t[0], t[1], t[2]);
		}
		...
		// free parser
		config_close(p);
	}
----- CUT -----

*/

parser_t* FAST_FUNC config_open2(const char *filename, FILE* FAST_FUNC (*fopen_func)(const char *path))
{
	FILE* fp;
	parser_t *parser;

	fp = fopen_func(filename);
	if (!fp)
		return NULL;
	parser = xzalloc(sizeof(*parser));
	parser->fp = fp;
	return parser;
}

parser_t* FAST_FUNC config_open(const char *filename)
{
	return config_open2(filename, fopen_or_warn_stdin);
}

static void config_free_data(parser_t *const parser)
{
	free(parser->line);
	parser->line = NULL;
	if (PARSE_KEEP_COPY) { /* compile-time constant */
		free(parser->data);
		parser->data = NULL;
	}
}

void FAST_FUNC config_close(parser_t *parser)
{
	if (parser) {
		config_free_data(parser);
		fclose(parser->fp);
		free(parser);
	}
}

/*
0. If parser is NULL return 0.
1. Read a line from config file. If nothing to read then return 0.
   Handle continuation character. Advance lineno for each physical line.
   Discard everything past comment characher.
2. if PARSE_TRIM is set (default), remove leading and trailing delimiters.
3. If resulting line is empty goto 1.
4. Look for first delimiter. If !PARSE_COLLAPSE or !PARSE_TRIM is set then
   remember the token as empty.
5. Else (default) if number of seen tokens is equal to max number of tokens
   (token is the last one) and PARSE_GREEDY is set then the remainder
   of the line is the last token.
   Else (token is not last or PARSE_GREEDY is not set) just replace
   first delimiter with '\0' thus delimiting the token.
6. Advance line pointer past the end of token. If number of seen tokens
   is less than required number of tokens then goto 4.
7. Check the number of seen tokens is not less the min number of tokens.
   Complain or die otherwise depending on PARSE_MIN_DIE.
8. Return the number of seen tokens.

mintokens > 0 make config_read() print error message if less than mintokens
(but more than 0) are found. Empty lines are always skipped (not warned about).
*/
#undef config_read
int FAST_FUNC config_read(parser_t *parser, char **tokens, unsigned flags, const char *delims)
{
	char *line, *q;
	char comment;
	int ii;
	int ntokens;
	int mintokens;

	comment = *delims++;
	ntokens = flags & 0xFF;
	mintokens = (flags & 0xFF00) >> 8;

 again:
	memset(tokens, 0, sizeof(tokens[0]) * ntokens);
	if (!parser)
		return 0;
	config_free_data(parser);

	while (1) {
//TODO: speed up xmalloc_fgetline by internally using fgets, not fgetc
		line = xmalloc_fgetline(parser->fp);
		if (!line)
			return 0;

		parser->lineno++;
		// handle continuations. Tito's code stolen :)
		while (1) {
			ii = strlen(line);
			if (!ii)
				goto next_line;
			if (line[ii - 1] != '\\')
				break;
			// multi-line object
			line[--ii] = '\0';
//TODO: add xmalloc_fgetline-like iface but with appending to existing str
			q = xmalloc_fgetline(parser->fp);
			if (!q)
				break;
			parser->lineno++;
			line = xasprintf("%s%s", line, q);
			free(q);
		}
		// discard comments
		if (comment) {
			q = strchrnul(line, comment);
			*q = '\0';
			ii = q - line;
		}
		// skip leading and trailing delimiters
		if (flags & PARSE_TRIM) {
			// skip leading
			int n = strspn(line, delims);
			if (n) {
				ii -= n;
				overlapping_strcpy(line, line + n);
			}
			// cut trailing
			if (ii) {
				while (strchr(delims, line[--ii]))
					continue;
				line[++ii] = '\0';
			}
		}
		// if something still remains -> return it
		if (ii)
			break;

 next_line:
		// skip empty line
		free(line);
	}
	// non-empty line found, parse and return the number of tokens

	// store line
	parser->line = line = xrealloc(line, ii + 1);
	if (flags & PARSE_KEEP_COPY) {
		parser->data = xstrdup(line);
	}

	// split line to tokens
	ntokens--; // now it's max allowed token no
	// N.B. non-empty remainder is also a token,
	// so if ntokens <= 1, we just return the whole line
	// N.B. if PARSE_GREEDY is set the remainder of the line is stuck to the last token
	ii = 0;
	while (*line && ii <= ntokens) {
		//bb_info_msg("L[%s]", line);
		// get next token
		// at last token and need greedy token ->
		if ((flags & PARSE_GREEDY) && (ii == ntokens)) {
			// skip possible delimiters
			if (flags & PARSE_COLLAPSE)
				line += strspn(line, delims);
			// don't cut the line
			q = line + strlen(line);
		} else {
			// vanilla token. cut the line at the first delim
			q = line + strcspn(line, delims);
			if (*q) // watch out: do not step past the line end!
				*q++ = '\0';
		}
		// pin token
		if (!(flags & (PARSE_COLLAPSE | PARSE_TRIM)) || *line) {
			//bb_info_msg("N[%d] T[%s]", ii, line);
			tokens[ii++] = line;
			// process escapes in token
#if 0 // unused so far
			if (flags & PARSE_ESCAPE) {
				char *s = line;
				while (*s) {
					if (*s == '\\') {
						s++;
						*line++ = bb_process_escape_sequence((const char **)&s);
					} else {
						*line++ = *s++;
					}
				}
				*line = '\0';
			}
#endif
		}
		line = q;
		//bb_info_msg("A[%s]", line);
	}

	if (ii < mintokens) {
		bb_error_msg("bad line %u: %d tokens found, %d needed",
 				parser->lineno, ii, mintokens);
		if (flags & PARSE_MIN_DIE)
			xfunc_die();
		ntokens++;
		goto again;
	}

	return ii;
}
