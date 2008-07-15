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
		while (*config_read(&p, t, 3, 0, delimiters, comment_char)) { // 0..3 tokens
			// use tokens
			bb_error_msg("TOKENS: [%s][%s][%s]", t[0], t[1], t[2]);
		}
		...
		// free parser
		config_close(&p);
	}
----- CUT -----

*/

#if !PARSER_STDIO_BASED

char* FAST_FUNC config_open(parser_t *parser, const char *filename)
{
	// empty file configures nothing!
	char *data = xmalloc_open_read_close(filename, NULL);
	if (!data)
		return data;

	// convert 0x5c 0x0a (backslashes at the very end of line) to 0x20 0x20 (spaces)
	for (char *s = data; (s = strchr(s, '\\')) != NULL; ++s)
		if ('\n' == s[1]) {
			s[0] = s[1] = ' ';
		}

	// init parser
	parser->line = parser->data = data;
	parser->lineno = 0;

	return data;
}

void FAST_FUNC config_close(parser_t *parser)
{
	// for now just free config data
	free(parser->data);
}

char* FAST_FUNC config_read(parser_t *parser, char **tokens, int ntokens, int mintokens, const char *delims, char comment)
{
	char *ret, *line;
	int noreduce = (ntokens<0); // do not treat subsequent delimiters as one delimiter
	if (ntokens < 0)
		ntokens = -ntokens;
	ret = line = parser->line;
	// nullify tokens
	memset(tokens, 0, sizeof(void *) * ntokens);
	// now split to lines
	while (*line) {
		int token_num = 0;
		// limit the line
		char *ptr = strchrnul(line, '\n');
		*ptr++ = '\0';
		// line number
		parser->lineno++;
		// comments mean EOLs
		if (comment)
			*strchrnul(line, comment) = '\0';
		// skip leading delimiters
		while (*line && strchr(delims, *line))
			line++;
		// skip empty lines
		if (*line) {
			char *s;
			// now split line to tokens
			s = line;
			while (s) {
				char *p;
				// get next token
				if (token_num+1 >= ntokens)
					break;
				p = s;
				while (*p && !strchr(delims, *p))
					p++;
				if (!*p)
					break;
				*p++ = '\0';
				// pin token
				if (noreduce || *s) {
					tokens[token_num++] = s;
//bb_error_msg("L[%d] T[%s]", token_num, s);
				}
				s = p;
	 		}
			// non-empty remainder is also a token. So if ntokens == 0, we just return the whole line
			if (s && (noreduce || *s))
				tokens[token_num++] = s;
			// sanity check: have we got all required tokens?
			if (token_num < mintokens)
				bb_error_msg_and_die("bad line %u, %d tokens found, %d needed", parser->lineno, token_num, mintokens);
			// advance data for the next call
			line = ptr;
			break;
		}
		// line didn't contain any token -> try next line
		ret = line = ptr;
 	}
	parser->line = line;

	// return current line. caller must check *ret to determine whether to continue
	return ret;
}

#else // stdio-based

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
	fclose(parser->fp);
}

char* FAST_FUNC config_read(parser_t *parser, char **tokens, int ntokens, int mintokens, const char *delims, char comment)
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

	while (1) {
		int n;

		// get fresh line
//TODO: speed up xmalloc_fgetline by internally using fgets, not fgetc
		line = xmalloc_fgetline(parser->fp);
		if (!line)
			return line;

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

	return parser->line; // maybe token_num instead?
}

#endif
