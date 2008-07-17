/* vi: set sw=4 ts=4: */
/*
 * config file parser helper
 *
 * Copyright (C) 2008 by Vladimir Dronnikov <dronnikov@gmail.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

/*

Typical usage:

----- CUT -----
	char *t[3];	// tokens placeholder
	parser_t p;	// parser structure
	// open file
	if (config_open(filename, &p)) {
		// parse line-by-line
		while (*config_read(&p, t, 3, 0, delimiters, comment_char) >= 0) { // 0..3 tokens
			// use tokens
			bb_error_msg("TOKENS: [%s][%s][%s]", t[0], t[1], t[2]);
		}
		...
		// free parser
		config_close(&p);
	}
----- CUT -----

*/

parser_t* FAST_FUNC config_open(const char *filename)
{
	parser_t *parser = xzalloc(sizeof(parser_t));
	/* empty file configures nothing */
	parser->fp = fopen_or_warn(filename, "r");
	if (parser->fp)
		return parser;
	config_close (parser);
	if (ENABLE_FEATURE_CLEAN_UP)
	  free(parser);
	return NULL;
}

static void config_free_data(parser_t *const parser)
{
	free(parser->line);
	free(parser->data);
	parser->line = parser->data = NULL;
}
void FAST_FUNC config_close(parser_t *parser)
{
	config_free_data(parser);
	fclose(parser->fp);
}

int FAST_FUNC config_read(parser_t *parser, char **tokens, int ntokens, int mintokens, const char*delims,char comment)
{
	char *line, *q;
	int ii;
	/* do not treat subsequent delimiters as one delimiter */
	bool noreduce = (ntokens < 0);
	if (noreduce)
		ntokens = -ntokens;

	memset(tokens, 0, sizeof(void *) * ntokens);
	config_free_data(parser);

	while (1) {
		int n;

		// get fresh line
//TODO: speed up xmalloc_fgetline by internally using fgets, not fgetc
		line = xmalloc_fgetline(parser->fp);
		if (!line)
			return -1;

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
		// skip leading delimiters
		n = strspn(line, delims);
		if (n) {
			ii -= n;
			strcpy(line, line + n);
		}
		if (ii)
			break;

 next_line:
		/* skip empty line */
		free(line);
	}

	// non-empty line found, parse and return

	// store line
	parser->line = line = xrealloc(line, ii + 1);
	parser->data = xstrdup(line);

	// now split line to tokens
//TODO: discard consecutive delimiters?
	ii = 0;
	ntokens--; // now it's max allowed token no
	while (1) {
		// get next token
		if (ii == ntokens)
			break;
		q = line + strcspn(line, delims);
		if (!*q)
			break;
		// pin token
		*q++ = '\0';
		if (noreduce || *line) {
			tokens[ii++] = line;
//bb_info_msg("L[%d] T[%s]\n", ii, line);
		}
		line = q;
	}

	// non-empty remainder is also a token,
	// so if ntokens <= 1, we just return the whole line
	if (noreduce || *line)
		tokens[ii++] = line;

	if (ii < mintokens)
		bb_error_msg_and_die("bad line %u: %d tokens found, %d needed",
				parser->lineno, ii, mintokens);

	return ii;
}
