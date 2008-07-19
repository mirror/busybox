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
	unsigned flags = 0;
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
			while ((n = config_read(p, t, ntokens, mintokens, delims, flags)) > 0) {
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

parser_t* FAST_FUNC config_open(const char *filename)
{
	parser_t *parser = xzalloc(sizeof(parser_t));
	/* empty file configures nothing */
	parser->fp = fopen_or_warn_stdin(filename);
	if (parser->fp)
		return parser;
	if (ENABLE_FEATURE_CLEAN_UP)
		free(parser);
	return NULL;
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
	config_free_data(parser);
	fclose(parser->fp);
}

/*
1. Read a line from config file. If nothing to read then bail out returning 0.
   Handle continuation character. Advance lineno for each physical line. Cut comments.
2. if PARSE_DONT_TRIM is not set (default) skip leading and cut trailing delimiters, if any.
3. If resulting line is empty goto 1.
4. Look for first delimiter. If PARSE_DONT_REDUCE or PARSE_DONT_TRIM is set then pin empty token.
5. Else (default) if number of seen tokens is equal to max number of tokens (token is the last one)
   and PARSE_LAST_IS_GREEDY is set then pin the remainder of the line as the last token.
   Else (token is not last or PARSE_LAST_IS_GREEDY is not set) just replace first delimiter with '\0'
   thus delimiting token and pin it.
6. Advance line pointer past the end of token. If number of seen tokens is less than required number
   of tokens then goto 4.
7. Control the number of seen tokens is not less the min number of tokens. Die if condition is not met.
8. Return the number of seen tokens.

mintokens > 0 make config_read() exit with error message if less than mintokens
(but more than 0) are found. Empty lines are always skipped (not warned about).
*/
#undef config_read
int FAST_FUNC config_read(parser_t *parser, char **tokens, unsigned flags, const char *delims)
{
	char *line, *q;
	char comment = *delims++;
	int ii;
	int ntokens = flags & 0xFF;
	int mintokens = (flags & 0xFF00) >> 8;

	/*
	// N.B. this could only be used in read-in-one-go version, or when tokens use xstrdup(). TODO
	if (!parser->lineno || !(flags & PARSE_DONT_NULL))
	*/
		memset(tokens, 0, sizeof(tokens[0]) * ntokens);
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
			if (q) {
				parser->lineno++;
				line = xasprintf("%s%s", line, q);
				free(q);
			}
		}
		// comments mean EOLs
		if (comment) {
			q = strchrnul(line, comment);
			*q = '\0';
			ii = q - line;
		}
		// skip leading and trailing delimiters
		if (!(flags & PARSE_DONT_TRIM)) {
			// skip leading
			int n = strspn(line, delims);
			if (n) {
				ii -= n;
				strcpy(line, line + n);
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
		/* skip empty line */
		free(line);
	}

	// non-empty line found, parse and return the number of tokens

	// store line
	parser->line = line = xrealloc(line, ii + 1);
	if (flags & PARSE_KEEP_COPY) {
		parser->data = xstrdup(line);
	}

	/* now split line to tokens */
	ntokens--; // now it's max allowed token no
	// N.B, non-empty remainder is also a token,
	// so if ntokens <= 1, we just return the whole line
	// N.B. if PARSE_LAST_IS_GREEDY is set the remainder of the line is stuck to the last token
	for (ii = 0; *line && ii <= ntokens; ) {
		//bb_info_msg("L[%s]", line);
		// get next token
		// at the last token and need greedy token ->
		if ((flags & PARSE_LAST_IS_GREEDY) && (ii == ntokens)) {
			// ... don't cut the line
			q = line + strlen(line);
		} else {
			// vanilla token. cut the line at the first delim
			q = line + strcspn(line, delims);
			*q++ = '\0';
		}
		// pin token
		if ((flags & (PARSE_DONT_REDUCE|PARSE_DONT_TRIM)) || *line) {
			//bb_info_msg("N[%d] T[%s]", ii, line);
			tokens[ii++] = line;
		}
		line = q;
	}

	if (ii < mintokens)
		bb_error_msg_and_die("bad line %u: %d tokens found, %d needed",
				parser->lineno, ii, mintokens);

	return ii;
}
