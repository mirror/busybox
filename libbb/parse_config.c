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

FILE* FAST_FUNC config_open(parser_t *parser, const char *filename)
{
	// empty file configures nothing!
	parser->fp = fopen_or_warn(filename, "r");
	if (!parser->fp)
		return parser->fp;

	// init parser
	parser->line = NULL;
	parser->lineno = 0;

	return parser->fp;
}

void FAST_FUNC config_close(parser_t *parser)
{
	free(parser->line);
	free(parser->data);
	fclose(parser->fp);
}

int FAST_FUNC config_read(parser_t *parser, char **tokens, int ntokens, int mintokens, const char *delims, char comment)
{
	char *line, *q;
	int token_num, len;
	int noreduce = (ntokens < 0); // do not treat subsequent delimiters as one delimiter

	if (ntokens < 0)
		ntokens = -ntokens;

	// nullify tokens
	memset(tokens, 0, sizeof(void *) * ntokens);

	// free used line
	free(parser->line);
	parser->line = NULL;
	free(parser->data);
	parser->data = NULL;

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
			len = strlen(line);
			if (!len)
				goto free_and_cont;
			if (line[len - 1] != '\\')
				break;
			// multi-line object
			line[--len] = '\0';
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
			len = q - line;
		}
		// skip leading delimiters
		n = strspn(line, delims);
		if (n) {
			len -= n;
			strcpy(line, line + n);
		}
		if (len)
			break;
		// skip empty lines
 free_and_cont:
		free(line);
	}

	// non-empty line found, parse and return

	// store line
	parser->line = line = xrealloc(line, len + 1);
	parser->data = xstrdup(line);

	// now split line to tokens
//TODO: discard consecutive delimiters?
	token_num = 0;
	ntokens--; // now it's max allowed token no
	while (1) {
		// get next token
		if (token_num == ntokens)
			break;
		q = line + strcspn(line, delims);
		if (!*q)
			break;
		// pin token
		*q++ = '\0';
		if (noreduce || *line) {
			tokens[token_num++] = line;
//bb_error_msg("L[%d] T[%s]", token_num, line);
		}
		line = q;
 	}

	// non-empty remainder is also a token,
	// so if ntokens <= 1, we just return the whole line
	if (noreduce || *line)
		tokens[token_num++] = line;

	if (token_num < mintokens)
		bb_error_msg_and_die("bad line %u: %d tokens found, %d needed",
				parser->lineno, token_num, mintokens);

	return token_num;
}
