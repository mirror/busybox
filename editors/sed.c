/*
 * sed.c - very minimalist version of sed
 *
 * Copyright (C) 1999,2000,2001 by Lineo, inc. and Mark Whitley
 * Copyright (C) 1999,2000,2001 by Mark Whitley <markw@codepoet.org>
 * Copyright (C) 2002  Matt Kraai
 * Copyright (C) 2003 by Glenn McGrath <bug1@optushome.com.au>
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
 */

/*
	Supported features and commands in this version of sed:

	 - comments ('#')
	 - address matching: num|/matchstr/[,num|/matchstr/|$]command
	 - commands: (p)rint, (d)elete, (s)ubstitue (with g & I flags)
	 - edit commands: (a)ppend, (i)nsert, (c)hange
	 - file commands: (r)ead
	 - backreferences in substitution expressions (\1, \2...\9)
	 - grouped commands: {cmd1;cmd2}
	 - transliteration (y/source-chars/dest-chars/)
	 - pattern space hold space storing / swapping (g, h, x)
	 - labels / branching (: label, b, t)

	 (Note: Specifying an address (range) to match is *optional*; commands
	 default to the whole pattern space if no specific address match was
	 requested.)

	Unsupported features:

	 - GNU extensions
	 - and lots, lots more.

	Bugs:
	
	 - Cant subst globally using ^ or $ in regex, eg. "aah" | sed 's/^a/b/g'

	Reference http://www.opengroup.org/onlinepubs/007904975/utilities/sed.html
*/

#include <stdio.h>
#include <unistd.h>		/* for getopt() */
#include <regex.h>
#include <string.h>		/* for strdup() */
#include <errno.h>
#include <ctype.h>		/* for isspace() */
#include <stdlib.h>
#include "busybox.h"

typedef struct sed_cmd_s {
	/* Order by alignment requirements */

	/* address storage */
	regex_t *beg_match;	/* sed -e '/match/cmd' */
	regex_t *end_match;	/* sed -e '/match/,/end_match/cmd' */

	/* SUBSTITUTION COMMAND SPECIFIC FIELDS */

	/* sed -e 's/sub_match/replace/' */
	regex_t *sub_match;
	char *replace;

	/* EDIT COMMAND (a,i,c) SPECIFIC FIELDS */
	char *editline;

	/* FILE COMMAND (r) SPECIFIC FIELDS */
	char *filename;

	/* address storage */
	int beg_line;		/* 'sed 1p'   0 == no begining line, apply commands to all lines */
	int end_line;		/* 'sed 1,3p' 0 == no end line, use only beginning. -1 == $ */
	/* SUBSTITUTION COMMAND SPECIFIC FIELDS */

	unsigned int num_backrefs:4;	/* how many back references (\1..\9) */
	/* Note:  GNU/POSIX sed does not save more than nine backrefs, so
	 * we only use 4 bits to hold the number */
	unsigned int sub_g:1;	/* sed -e 's/foo/bar/g' (global) */
	unsigned int sub_p:1;	/* sed -e 's/foo/bar/p' (print substitution) */

	/* TRANSLATE COMMAND */
	char *translate;

	/* GENERAL FIELDS */
	/* the command */
	char cmd;			/* p,d,s (add more at your leisure :-) */

	/* inversion flag */
	int invert;			/* the '!' after the address */

	/* Branch commands */
	char *label;

	/* next command in list (sequential list of specified commands) */
	struct sed_cmd_s *next;

} sed_cmd_t;


/* externs */
extern void xregcomp(regex_t * preg, const char *regex, int cflags);
extern int optind;		/* in unistd.h */
extern char *optarg;	/* ditto */

/* globals */
/* options */
static int be_quiet = 0;
static const char bad_format_in_subst[] =
	"bad format in substitution expression";

/* linked list of sed commands */
static sed_cmd_t sed_cmd_head;
static sed_cmd_t *sed_cmd_tail = &sed_cmd_head;
static sed_cmd_t *block_cmd;

static int in_block = 0;
const char *const semicolon_whitespace = "; \n\r\t\v\0";
static regex_t *previous_regex_ptr = NULL;


#ifdef CONFIG_FEATURE_CLEAN_UP
static void destroy_cmd_strs(void)
{
	sed_cmd_t *sed_cmd = sed_cmd_head.next;

	while (sed_cmd) {
		sed_cmd_t *sed_cmd_next = sed_cmd->next;

		if (sed_cmd->beg_match) {
			regfree(sed_cmd->beg_match);
			free(sed_cmd->beg_match);
		}
		if (sed_cmd->end_match) {
			regfree(sed_cmd->end_match);
			free(sed_cmd->end_match);
		}
		if (sed_cmd->sub_match) {
			regfree(sed_cmd->sub_match);
			free(sed_cmd->sub_match);
		}
		free(sed_cmd->replace);
		free(sed_cmd);
		sed_cmd = sed_cmd_next;
	}
}
#endif

/*
 * index_of_next_unescaped_regexp_delim - walks left to right through a string
 * beginning at a specified index and returns the index of the next regular
 * expression delimiter (typically a forward * slash ('/')) not preceeded by 
 * a backslash ('\').
 */
static int index_of_next_unescaped_regexp_delim(const char delimiter,
												const char *str)
{
	int bracket = -1;
	int escaped = 0;
	int idx = 0;
	char ch;

	for (; (ch = str[idx]); idx++) {
		if (bracket != -1) {
			if (ch == ']' && !(bracket == idx - 1 ||
							   (bracket == idx - 2 && str[idx - 1] == '^')))
				bracket = -1;
		} else if (escaped)
			escaped = 0;
		else if (ch == '\\')
			escaped = 1;
		else if (ch == '[')
			bracket = idx;
		else if (ch == delimiter)
			return idx;
	}

	/* if we make it to here, we've hit the end of the string */
	return -1;
}

static int parse_regex_delim(const char *cmdstr, char **match, char **replace)
{
	const char *cmdstr_ptr = cmdstr;
	char delimiter;
	int idx = 0;

	/* verify that the 's' is followed by something.  That something
	 * (typically a 'slash') is now our regexp delimiter... */
	if (*cmdstr == '\0')
		bb_error_msg_and_die(bad_format_in_subst);
	else
		delimiter = *cmdstr_ptr;

	cmdstr_ptr++;

	/* save the match string */
	idx = index_of_next_unescaped_regexp_delim(delimiter, cmdstr_ptr);
	if (idx == -1) {
		bb_error_msg_and_die(bad_format_in_subst);
	}
	*match = bb_xstrndup(cmdstr_ptr, idx);

	/* save the replacement string */
	cmdstr_ptr += idx + 1;
	idx = index_of_next_unescaped_regexp_delim(delimiter, cmdstr_ptr);
	if (idx == -1) {
		bb_error_msg_and_die(bad_format_in_subst);
	}
	*replace = bb_xstrndup(cmdstr_ptr, idx);

	return ((cmdstr_ptr - cmdstr) + idx);
}

/*
 * returns the index in the string just past where the address ends.
 */
static int get_address(char *my_str, int *linenum, regex_t ** regex)
{
	int idx = 0;

	if (isdigit(my_str[idx])) {
		char *endstr;

		*linenum = strtol(my_str, &endstr, 10);
		/* endstr shouldnt ever equal NULL */
		idx = endstr - my_str;
	} else if (my_str[idx] == '$') {
		*linenum = -1;
		idx++;
	} else if (my_str[idx] == '/' || my_str[idx] == '\\') {
		int idx_start = 1;
		char delimiter;

		delimiter = '/';
		if (my_str[idx] == '\\') {
			idx_start++;
			delimiter = my_str[++idx];
		}
		idx++;
		idx += index_of_next_unescaped_regexp_delim(delimiter, my_str + idx);
		if (idx == -1) {
			bb_error_msg_and_die("unterminated match expression");
		}
		my_str[idx] = '\0';

		*regex = (regex_t *) xmalloc(sizeof(regex_t));
		xregcomp(*regex, my_str + idx_start, REG_NEWLINE);
		idx++;			/* so it points to the next character after the last '/' */
	}
	return idx;
}

static int parse_subst_cmd(sed_cmd_t * const sed_cmd, const char *substr)
{
	int cflags = 0;
	char *match;
	int idx = 0;
	int j;

	/*
	 * the string that gets passed to this function should look like this:
	 *    s/match/replace/gIp
	 *    ||     |        |||
	 *    mandatory       optional
	 *
	 *    (all three of the '/' slashes are mandatory)
	 */
	idx = parse_regex_delim(substr, &match, &sed_cmd->replace);

	/* determine the number of back references in the match string */
	/* Note: we compute this here rather than in the do_subst_command()
	 * function to save processor time, at the expense of a little more memory
	 * (4 bits) per sed_cmd */

	/* sed_cmd->num_backrefs = 0; *//* XXX: not needed? --apparently not */
	for (j = 0; match[j]; j++) {
		/* GNU/POSIX sed does not save more than nine backrefs */
		if (match[j] == '\\' && match[j + 1] == '('
			&& sed_cmd->num_backrefs <= 9)
			sed_cmd->num_backrefs++;
	}

	/* process the flags */
	while (substr[++idx]) {
		switch (substr[idx]) {
		case 'g':
			sed_cmd->sub_g = 1;
			break;
			/* Hmm, i dont see the I option mentioned in the standard */
		case 'I':
			cflags |= REG_ICASE;
			break;
		case 'p':
			sed_cmd->sub_p = 1;
			break;
		default:
			/* any whitespace or semicolon trailing after a s/// is ok */
			if (strchr(semicolon_whitespace, substr[idx]))
				goto out;
			/* else */
			bb_error_msg_and_die("bad option in substitution expression");
		}
	}

  out:
	/* compile the match string into a regex */
	if (*match != '\0') {
		/* If match is empty, we use last regex used at runtime */
		sed_cmd->sub_match = (regex_t *) xmalloc(sizeof(regex_t));
		xregcomp(sed_cmd->sub_match, match, cflags);
	}
	free(match);

	return idx;
}

static void replace_slash_n(char *string)
{
	int i;
	int remaining = strlen(string);

	for (i = 0; string[i]; i++) {
		if ((string[i] == '\\') && (string[i + 1] == 'n')) {
			string[i] = '\n';
			memmove(string + i + 1, string + i + 1, remaining - 1);
		} else {
			remaining--;
		}
	}
}

static int parse_translate_cmd(sed_cmd_t * const sed_cmd, const char *cmdstr)
{
	char *match;
	char *replace;
	int idx;
	int i;

	idx = parse_regex_delim(cmdstr, &match, &replace);
	replace_slash_n(match);
	replace_slash_n(replace);
	sed_cmd->translate = xcalloc(1, (strlen(match) + 1) * 2);
	for (i = 0; (match[i] != 0) && (replace[i] != 0); i++) {
		sed_cmd->translate[i * 2] = match[i];
		sed_cmd->translate[(i * 2) + 1] = replace[i];
	}
	return (idx + 1);
}

static int parse_edit_cmd(sed_cmd_t * sed_cmd, const char *editstr)
{
	int i, j;

	/*
	 * the string that gets passed to this function should look like this:
	 *
	 *    need one of these 
	 *    |
	 *    |    this backslash (immediately following the edit command) is mandatory
	 *    |    |
	 *    [aic]\
	 *    TEXT1\
	 *    TEXT2\
	 *    TEXTN
	 *
	 * as soon as we hit a TEXT line that has no trailing '\', we're done.
	 * this means a command like:
	 *
	 * i\
	 * INSERTME
	 *
	 * is a-ok.
	 *
	 */
	if ((*editstr != '\\') || ((editstr[1] != '\n') && (editstr[1] != '\r'))) {
		bb_error_msg_and_die("bad format in edit expression");
	}

	/* store the edit line text */
	sed_cmd->editline = xmalloc(strlen(&editstr[2]) + 2);
	for (i = 2, j = 0;
		 editstr[i] != '\0' && strchr("\r\n", editstr[i]) == NULL; i++, j++) {
		if ((editstr[i] == '\\') && strchr("\n\r", editstr[i + 1]) != NULL) {
			sed_cmd->editline[j] = '\n';
			i++;
		} else
			sed_cmd->editline[j] = editstr[i];
	}

	/* figure out if we need to add a newline */
	if (sed_cmd->editline[j - 1] != '\n')
		sed_cmd->editline[j++] = '\n';

	/* terminate string */
	sed_cmd->editline[j] = '\0';

	return i;
}


static int parse_file_cmd(sed_cmd_t * sed_cmd, const char *filecmdstr)
{
	int idx = 0;
	int filenamelen = 0;

	/*
	 * the string that gets passed to this function should look like this:
	 *    '[ ]filename'
	 *      |  |
	 *      |  a filename
	 *      |
	 *     optional whitespace

	 *   re: the file to be read, the GNU manual says the following: "Note that
	 *   if filename cannot be read, it is treated as if it were an empty file,
	 *   without any error indication." Thus, all of the following commands are
	 *   perfectly leagal:
	 *
	 *   sed -e '1r noexist'
	 *   sed -e '1r ;'
	 *   sed -e '1r'
	 */

	/* the file command may be followed by whitespace; move past it. */
	while (isspace(filecmdstr[++idx])) {;
	}

	/* the first non-whitespace we get is a filename. the filename ends when we
	 * hit a normal sed command terminator or end of string */
	filenamelen = strcspn(&filecmdstr[idx], semicolon_whitespace);
	sed_cmd->filename = xmalloc(filenamelen + 1);
	safe_strncpy(sed_cmd->filename, &filecmdstr[idx], filenamelen + 1);
	return idx + filenamelen;
}

/*
 *  Process the commands arguments
 */
static char *parse_cmd_str(sed_cmd_t *sed_cmd, char *cmdstr)
{
	/* handle (s)ubstitution command */
	if (sed_cmd->cmd == 's') {
		cmdstr += parse_subst_cmd(sed_cmd, cmdstr);
	}
	/* handle edit cmds: (a)ppend, (i)nsert, and (c)hange */
	else if (strchr("aic", sed_cmd->cmd)) {
		if ((sed_cmd->end_line || sed_cmd->end_match) && sed_cmd->cmd != 'c')
			bb_error_msg_and_die
				("only a beginning address can be specified for edit commands");
		cmdstr += parse_edit_cmd(sed_cmd, cmdstr);
	}
	/* handle file cmds: (r)ead */
	else if (sed_cmd->cmd == 'r') {
		if (sed_cmd->end_line || sed_cmd->end_match)
			bb_error_msg_and_die("Command only uses one address");
		cmdstr += parse_file_cmd(sed_cmd, cmdstr);
	}
	/* handle branch commands */
	else if (strchr(":bt", sed_cmd->cmd)) {
		int length;

		cmdstr += strspn(cmdstr, " ");
		length = strcspn(cmdstr, "; \n");
		sed_cmd->label = strndup(cmdstr, length);
		cmdstr += length;
	}
	/* translation command */
	else if (sed_cmd->cmd == 'y') {
		cmdstr += parse_translate_cmd(sed_cmd, cmdstr);
	}
	/* if it wasnt a single-letter command that takes no arguments
	 * then it must be an invalid command.
	 */
	else if (strchr("dghnNpPqx=", sed_cmd->cmd) == 0) {
		bb_error_msg_and_die("Unsupported command %c", sed_cmd->cmd);
	}

	/* give back whatever's left over */
	return (cmdstr);
}

static char *add_cmd(sed_cmd_t * sed_cmd, char *cmdstr)
{
	/* Skip over leading whitespace and semicolons */
	cmdstr += strspn(cmdstr, semicolon_whitespace);

	/* if we ate the whole thing, that means there was just trailing
	 * whitespace or a final / no-op semicolon. either way, get out */
	if (*cmdstr == '\0') {
		return (NULL);
	}

	/* if this is a comment, jump past it and keep going */
	if (*cmdstr == '#') {
		/* "#n" is the same as using -n on the command line */
		if (cmdstr[1] == 'n') {
			be_quiet++;
		}
		return (strpbrk(cmdstr, "\n\r"));
	}

	/* Test for end of block */
	if (*cmdstr == '}') {
		in_block = 0;
		cmdstr++;
		return (cmdstr);
	}

	/* parse the command
	 * format is: [addr][,addr]cmd
	 *            |----||-----||-|
	 *            part1 part2  part3
	 */

	/* first part (if present) is an address: either a '$', a number or a /regex/ */
	cmdstr += get_address(cmdstr, &sed_cmd->beg_line, &sed_cmd->beg_match);

	/* second part (if present) will begin with a comma */
	if (*cmdstr == ',') {
		int idx;

		cmdstr++;
		idx = get_address(cmdstr, &sed_cmd->end_line, &sed_cmd->end_match);
		if (idx == 0) {
			bb_error_msg_and_die("get_address: no address found in string\n"
								 "\t(you probably didn't check the string you passed me)");
		}
		cmdstr += idx;
	}

	/* skip whitespace before the command */
	while (isspace(*cmdstr)) {
		cmdstr++;
	}

	/* there my be the inversion flag between part2 and part3 */
	if (*cmdstr == '!') {
		sed_cmd->invert = 1;
		cmdstr++;

#ifdef SED_FEATURE_STRICT_CHECKING
		/* According to the spec
		 * It is unspecified whether <blank>s can follow a '!' character,
		 * and conforming applications shall not follow a '!' character
		 * with <blank>s.
		 */
		if (isblank(cmdstr[idx]) {
			bb_error_msg_and_die("blank follows '!'");}
#else
		/* skip whitespace before the command */
		while (isspace(*cmdstr)) {
			cmdstr++;
		}
#endif
	}

	/* last part (mandatory) will be a command */
	if (*cmdstr == '\0')
		bb_error_msg_and_die("missing command");

	/* This is the start of a block of commands */
	if (*cmdstr == '{') {
		if (in_block != 0) {
			bb_error_msg_and_die("cant handle sub-blocks");
		}
		in_block = 1;
		block_cmd = sed_cmd;

		return (cmdstr + 1);
	}

	sed_cmd->cmd = *cmdstr;
	cmdstr++;

	if (in_block == 1) {
		sed_cmd->beg_match = block_cmd->beg_match;
		sed_cmd->end_match = block_cmd->end_match;
		sed_cmd->beg_line = block_cmd->beg_line;
		sed_cmd->end_line = block_cmd->end_line;
		sed_cmd->invert = block_cmd->invert;
	}

	cmdstr = parse_cmd_str(sed_cmd, cmdstr);

	/* Add the command to the command array */
	sed_cmd_tail->next = sed_cmd;
	sed_cmd_tail = sed_cmd_tail->next;

	return (cmdstr);
}

static void add_cmd_str(char *cmdstr)
{
#ifdef CONFIG_FEATURE_SED_EMBEDED_NEWLINE
	char *cmdstr_ptr = cmdstr;

	/* HACK: convert "\n" to match tranlated '\n' string */
	while ((cmdstr_ptr = strstr(cmdstr_ptr, "\\n")) != NULL) {
		cmdstr = xrealloc(cmdstr, strlen(cmdstr) + 2);
		cmdstr_ptr = strstr(cmdstr, "\\n");
		memmove(cmdstr_ptr + 1, cmdstr_ptr, strlen(cmdstr_ptr) + 1);
		cmdstr_ptr[0] = '\\';
		cmdstr_ptr += 3;
	}
#endif
	do {
		sed_cmd_t *sed_cmd;

		sed_cmd = xcalloc(1, sizeof(sed_cmd_t));
		cmdstr = add_cmd(sed_cmd, cmdstr);
	} while (cmdstr && strlen(cmdstr));
}


static void load_cmd_file(char *filename)
{
	FILE *cmdfile;
	char *line;
	char *nextline;
	char *e;

	cmdfile = bb_xfopen(filename, "r");

	while ((line = bb_get_line_from_file(cmdfile)) != NULL) {
		/* if a line ends with '\' it needs the next line appended to it */
		while (((e = last_char_is(line, '\n')) != NULL)
			   && (e > line) && (e[-1] == '\\')
			   && ((nextline = bb_get_line_from_file(cmdfile)) != NULL)) {
			line = xrealloc(line, (e - line) + 1 + strlen(nextline) + 1);
			strcat(line, nextline);
			free(nextline);
		}
		/* eat trailing newline (if any) --if I don't do this, edit commands
		 * (aic) will print an extra newline */
		chomp(line);
		add_cmd_str(line);
		free(line);
	}
}

struct pipeline {
	char *buf;
	int idx;
	int len;
};

#define PIPE_MAGIC 0x7f
#define PIPE_GROW 64

void pipe_putc(struct pipeline *const pipeline, char c)
{
	if (pipeline->buf[pipeline->idx] == PIPE_MAGIC) {
		pipeline->buf = xrealloc(pipeline->buf, pipeline->len + PIPE_GROW);
		memset(pipeline->buf + pipeline->len, 0, PIPE_GROW);
		pipeline->len += PIPE_GROW;
		pipeline->buf[pipeline->len - 1] = PIPE_MAGIC;
	}
	pipeline->buf[pipeline->idx++] = (c);
}

#define pipeputc(c) 	pipe_putc(pipeline, c)

#if 0
{
	if (pipeline[pipeline_idx] == PIPE_MAGIC) {
		pipeline = xrealloc(pipeline, pipeline_len + PIPE_GROW);
		memset(pipeline + pipeline_len, 0, PIPE_GROW);
		pipeline_len += PIPE_GROW;
		pipeline[pipeline_len - 1] = PIPE_MAGIC;
	}
	pipeline[pipeline_idx++] = (c);
}
#endif

static void print_subst_w_backrefs(const char *line, const char *replace,
								   regmatch_t * regmatch,
								   struct pipeline *const pipeline,
								   int matches)
{
	int i;

	/* go through the replacement string */
	for (i = 0; replace[i]; i++) {
		/* if we find a backreference (\1, \2, etc.) print the backref'ed * text */
		if (replace[i] == '\\' && isdigit(replace[i + 1])) {
			int j;
			char tmpstr[2];
			int backref;

			++i;		/* i now indexes the backref number, instead of the leading slash */
			tmpstr[0] = replace[i];
			tmpstr[1] = 0;
			backref = atoi(tmpstr);
			/* print out the text held in regmatch[backref] */
			if (backref <= matches && regmatch[backref].rm_so != -1)
				for (j = regmatch[backref].rm_so; j < regmatch[backref].rm_eo;
					 j++)
					pipeputc(line[j]);
		}

		/* if we find a backslash escaped character, print the character */
		else if (replace[i] == '\\') {
			++i;
			pipeputc(replace[i]);
		}

		/* if we find an unescaped '&' print out the whole matched text.
		 * fortunately, regmatch[0] contains the indicies to the whole matched
		 * expression (kinda seems like it was designed for just such a
		 * purpose...) */
		else if (replace[i] == '&') {
			int j;
			for (j = regmatch[0].rm_so; j < regmatch[0].rm_eo; j++)
				pipeputc(line[j]);
		}
		/* nothing special, just print this char of the replacement string to stdout */
		else
			pipeputc(replace[i]);
	}
}

static int do_subst_command(sed_cmd_t * sed_cmd, char **line)
{
	char *hackline = *line;
	struct pipeline thepipe = { NULL, 0, 0 };
	struct pipeline *const pipeline = &thepipe;
	int altered = 0;
	int result;
	regmatch_t *regmatch = NULL;
	regex_t *current_regex;

	if (sed_cmd->sub_match == NULL) {
		current_regex = previous_regex_ptr;
	} else {
		previous_regex_ptr = current_regex = sed_cmd->sub_match;
	}
	result = regexec(current_regex, hackline, 0, NULL, 0);

	/* we only proceed if the substitution 'search' expression matches */
	if (result == REG_NOMATCH) {
		return 0;
	}

	/* whaddaya know, it matched. get the number of back references */
	regmatch = xmalloc(sizeof(regmatch_t) * (sed_cmd->num_backrefs + 1));

	/* allocate more PIPE_GROW bytes
	   if replaced string is larger than original */
	thepipe.len = strlen(hackline) + PIPE_GROW;
	thepipe.buf = xcalloc(1, thepipe.len);
	/* buffer magic */
	thepipe.buf[thepipe.len - 1] = PIPE_MAGIC;

	/* and now, as long as we've got a line to try matching and if we can match
	 * the search string, we make substitutions */
	while ((*hackline || !altered) && (regexec(current_regex, hackline,
											   sed_cmd->num_backrefs + 1,
											   regmatch, 0) != REG_NOMATCH)) {
		int i;

		/* print everything before the match */
		for (i = 0; i < regmatch[0].rm_so; i++)
			pipeputc(hackline[i]);

		/* then print the substitution string */
		print_subst_w_backrefs(hackline, sed_cmd->replace, regmatch,
							   pipeline, sed_cmd->num_backrefs);

		/* advance past the match */
		hackline += regmatch[0].rm_eo;
		/* flag that something has changed */
		altered++;

		/* if we're not doing this globally, get out now */
		if (!sed_cmd->sub_g) {
			break;
		}
	}
	for (; *hackline; hackline++)
		pipeputc(*hackline);
	if (thepipe.buf[thepipe.idx] == PIPE_MAGIC)
		thepipe.buf[thepipe.idx] = 0;

	/* cleanup */
	free(regmatch);

	free(*line);
	*line = thepipe.buf;
	return altered;
}

static sed_cmd_t *branch_to(const char *label)
{
	sed_cmd_t *sed_cmd;
	for (sed_cmd = sed_cmd_head.next; sed_cmd; sed_cmd = sed_cmd->next) {
		if ((sed_cmd->label) && (strcmp(sed_cmd->label, label) == 0)) {
			break;
		}
	}

	/* If no match returns last command */
	return (sed_cmd);
}

static void process_file(FILE * file)
{
	char *pattern_space;	/* Posix requires it be able to hold at least 8192 bytes */
	char *hold_space = NULL;	/* Posix requires it be able to hold at least 8192 bytes */
	static int linenum = 0;	/* GNU sed does not restart counting lines at EOF */
	unsigned int still_in_range = 0;
	int altered;
	int force_print;

	pattern_space = bb_get_chomped_line_from_file(file);
	if (pattern_space == NULL) {
		return;
	}

	/* go through every line in the file */
	do {
		char *next_line;
		sed_cmd_t *sed_cmd;
		int substituted = 0;

		/* Read one line in advance so we can act on the last line, the '$' address */
		next_line = bb_get_chomped_line_from_file(file);

		linenum++;
		altered = 0;
		force_print = 0;

		/* for every line, go through all the commands */
		for (sed_cmd = sed_cmd_head.next; sed_cmd;
			 sed_cmd = sed_cmd->next) {
			int deleted = 0;

			/*
			 * entry point into sedding...
			 */
			int matched = (
							  /* no range necessary */
							  (sed_cmd->beg_line == 0
							   && sed_cmd->end_line == 0
							   && sed_cmd->beg_match == NULL
							   && sed_cmd->end_match == NULL) ||
							  /* this line number is the first address we're looking for */
							  (sed_cmd->beg_line
							   && (sed_cmd->beg_line == linenum)) ||
							  /* this line matches our first address regex */
							  (sed_cmd->beg_match
							   &&
							   (regexec
								(sed_cmd->beg_match, pattern_space, 0, NULL,
								 0) == 0)) ||
							  /* we are currently within the beginning & ending address range */
							  still_in_range || ((sed_cmd->beg_line == -1)
												 && (next_line == NULL))
				);

			if (sed_cmd->invert ^ matched) {
				/* Update last used regex incase a blank substitute BRE is found */
				if (sed_cmd->beg_match) {
					previous_regex_ptr = sed_cmd->beg_match;
				}

				/*
				 * actual sedding
				 */
				switch (sed_cmd->cmd) {
				case '=':
					printf("%d\n", linenum);
					break;
				case 'P':{
					/* Write the current pattern space upto the first newline */
					char *tmp = strchr(pattern_space, '\n');

					if (tmp) {
						*tmp = '\0';
					}
				}
				case 'p':	/* Write the current pattern space to output */
					puts(pattern_space);
					break;
				case 'd':
					altered++;
					deleted = 1;
					break;

				case 's':

					/*
					 * Some special cases for 's' printing to make it compliant with
					 * GNU sed printing behavior (aka "The -n | s///p Matrix"):
					 *
					 *    -n ONLY = never print anything regardless of any successful
					 *    substitution
					 *
					 *    s///p ONLY = always print successful substitutions, even if
					 *    the pattern_space is going to be printed anyway (pattern_space
					 *    will be printed twice).
					 *
					 *    -n AND s///p = print ONLY a successful substitution ONE TIME;
					 *    no other lines are printed - this is the reason why the 'p'
					 *    flag exists in the first place.
					 */

#ifdef CONFIG_FEATURE_SED_EMBEDED_NEWLINE
					/* HACK: escape newlines twice so regex can match them */
				{
					int offset = 0;

					while (strchr(pattern_space + offset, '\n') != NULL) {
						char *tmp;

						pattern_space =
							xrealloc(pattern_space,
									 strlen(pattern_space) + 2);
						tmp = strchr(pattern_space + offset, '\n');
						memmove(tmp + 1, tmp, strlen(tmp) + 1);
						tmp[0] = '\\';
						tmp[1] = 'n';
						offset = tmp - pattern_space + 2;
					}
				}
#endif
					/* we print the pattern_space once, unless we were told to be quiet */
					substituted = do_subst_command(sed_cmd, &pattern_space);

#ifdef CONFIG_FEATURE_SED_EMBEDED_NEWLINE
					/* undo HACK: escape newlines twice so regex can match them */
					{
						char *tmp = pattern_space;

						while ((tmp = strstr(tmp, "\\n")) != NULL) {
							memmove(tmp, tmp + 1, strlen(tmp + 1) + 1);
							tmp[0] = '\n';
						}
					}
#endif
					altered |= substituted;
					if (!be_quiet && altered && ((sed_cmd->next == NULL)
												 || (sed_cmd->next->cmd !=
													 's'))) {
						force_print = 1;
					}

					/* we also print the line if we were given the 'p' flag
					 * (this is quite possibly the second printing) */
					if ((sed_cmd->sub_p) && altered) {
						puts(pattern_space);
					}
					break;
				case 'a':
					puts(pattern_space);
					fputs(sed_cmd->editline, stdout);
					altered++;
					break;

				case 'i':
					fputs(sed_cmd->editline, stdout);
					break;

				case 'c':
					/* single-address case */
					if ((sed_cmd->end_match == NULL && sed_cmd->end_line == 0)
						/* multi-address case */
						/* - matching text */
						|| (sed_cmd->end_match
							&&
							(regexec
							 (sed_cmd->end_match, pattern_space, 0, NULL,
							  0) == 0))
						/* - matching line numbers */
						|| (sed_cmd->end_line > 0
							&& sed_cmd->end_line == linenum)) {
						fputs(sed_cmd->editline, stdout);
					}
					altered++;

					break;

				case 'r':{
					FILE *outfile;

					outfile = fopen(sed_cmd->filename, "r");
					if (outfile) {
						bb_xprint_and_close_file(outfile);
					}
					/* else if we couldn't open the output file,
					 * no biggie, just don't print anything */
					altered++;
				}
					break;
				case 'q':	/* Branch to end of script and quit */
					deleted = 1;
					/* Exit the outer while loop */
					free(next_line);
					next_line = NULL;
					break;
				case 'n':	/* Read next line from input */
					free(pattern_space);
					pattern_space = next_line;
					next_line = bb_get_chomped_line_from_file(file);
					linenum++;
					break;
				case 'N':	/* Append the next line to the current line */
					if (next_line) {
						pattern_space =
							realloc(pattern_space,
									strlen(pattern_space) +
									strlen(next_line) + 2);
						strcat(pattern_space, "\n");
						strcat(pattern_space, next_line);
						next_line = bb_get_chomped_line_from_file(file);
						linenum++;
					}
					break;
				case 'b':
					sed_cmd = branch_to(sed_cmd->label);
					break;
				case 't':
					if (substituted) {
						sed_cmd = branch_to(sed_cmd->label);
					}
					break;
				case 'y':{
					int i;

					for (i = 0; pattern_space[i] != 0; i++) {
						int j;

						for (j = 0; sed_cmd->translate[j]; j += 2) {
							if (pattern_space[i] == sed_cmd->translate[j]) {
								pattern_space[i] = sed_cmd->translate[j + 1];
							}
						}
					}
				}
					break;
				case 'g':	/* Replace pattern space with hold space */
					free(pattern_space);
					pattern_space = strdup(hold_space);
					break;
				case 'h':	/* Replace hold space with pattern space */
					free(hold_space);
					hold_space = strdup(pattern_space);
					break;
				case 'x':{
					/* Swap hold and pattern space */
					char *tmp;

					tmp = pattern_space;
					pattern_space = hold_space;
					hold_space = tmp;
				}
				}
			}

			/*
			 * exit point from sedding...
			 */
			if (matched) {
				if (
					   /* this is a single-address command or... */
					   (sed_cmd->end_line == 0 && sed_cmd->end_match == NULL)
					   || (
							  /* If only one address */
							  /* we were in the middle of our address range (this
							   * isn't the first time through) and.. */
							  (still_in_range == 1) && (
														   /* this line number is the last address we're looking for or... */
														   (sed_cmd->
															end_line
															&& (sed_cmd->
																end_line ==
																linenum))
														   ||
														   /* this line matches our last address regex */
														   (sed_cmd->
															end_match
															&&
															(regexec
															 (sed_cmd->
															  end_match,
															  pattern_space,
															  0, NULL,
															  0) == 0))
							  )
					   )
					) {
					/* we're out of our address range */
					still_in_range = 0;
				}

				/* didn't hit the exit? then we're still in the middle of an address range */
				else {
					still_in_range = 1;
				}
			}

			if (deleted)
				break;
		}

		/* we will print the line unless we were told to be quiet or if the
		 * line was altered (via a 'd'elete or 's'ubstitution), in which case
		 * the altered line was already printed */
		if ((!be_quiet && !altered) || force_print) {
			puts(pattern_space);
		}
		free(pattern_space);
		pattern_space = next_line;
	} while (pattern_space);
}

extern int sed_main(int argc, char **argv)
{
	int opt, status = EXIT_SUCCESS;

#ifdef CONFIG_FEATURE_CLEAN_UP
	/* destroy command strings on exit */
	if (atexit(destroy_cmd_strs) == -1)
		bb_perror_msg_and_die("atexit");
#endif

	/* do normal option parsing */
	while ((opt = getopt(argc, argv, "ne:f:")) > 0) {
		switch (opt) {
		case 'n':
			be_quiet++;
			break;
		case 'e':{
			char *str_cmd = strdup(optarg);

			add_cmd_str(str_cmd);
			free(str_cmd);
			break;
		}
		case 'f':
			load_cmd_file(optarg);
			break;
		default:
			bb_show_usage();
		}
	}

	/* if we didn't get a pattern from a -e and no command file was specified,
	 * argv[optind] should be the pattern. no pattern, no worky */
	if (sed_cmd_head.next == NULL) {
		if (argv[optind] == NULL)
			bb_show_usage();
		else {
			char *str_cmd = strdup(argv[optind]);

			add_cmd_str(strdup(str_cmd));
			free(str_cmd);
			optind++;
		}
	}

	/* argv[(optind)..(argc-1)] should be names of file to process. If no
	 * files were specified or '-' was specified, take input from stdin.
	 * Otherwise, we process all the files specified. */
	if (argv[optind] == NULL || (strcmp(argv[optind], "-") == 0)) {
		process_file(stdin);
	} else {
		int i;
		FILE *file;

		for (i = optind; i < argc; i++) {
			file = bb_wfopen(argv[i], "r");
			if (file) {
				process_file(file);
				fclose(file);
			} else
				status = EXIT_FAILURE;
		}
	}

	return status;
}
