/*
 * sed.c - very minimalist version of sed
 *
 * Copyright (C) 1999,2000,2001 by Lineo, inc. and Mark Whitley
 * Copyright (C) 1999,2000,2001 by Mark Whitley <markw@codepoet.org>
 * Copyright (C) 2002  Matt Kraai
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
	 
	 (Note: Specifying an address (range) to match is *optional*; commands
	 default to the whole pattern space if no specific address match was
	 requested.)

	Unsupported features:

	 - transliteration (y/source-chars/dest-chars/) (use 'tr')
	 - no pattern space hold space storing / swapping (x, etc.)
	 - no labels / branching (: label, b, t, and friends)
	 - and lots, lots more.
*/

#include <stdio.h>
#include <unistd.h> /* for getopt() */
#include <regex.h>
#include <string.h> /* for strdup() */
#include <errno.h>
#include <ctype.h> /* for isspace() */
#include <stdlib.h>
#include "busybox.h"

/* externs */
extern void xregcomp(regex_t *preg, const char *regex, int cflags);
extern int optind; /* in unistd.h */
extern char *optarg; /* ditto */

/* options */
static int be_quiet = 0;


struct sed_cmd {
	/* Order by alignment requirements */

	/* address storage */
	regex_t *beg_match; /* sed -e '/match/cmd' */
	regex_t *end_match; /* sed -e '/match/,/end_match/cmd' */

	/* SUBSTITUTION COMMAND SPECIFIC FIELDS */

	/* sed -e 's/sub_match/replace/' */
	regex_t *sub_match;
	char *replace;

	/* EDIT COMMAND (a,i,c) SPECIFIC FIELDS */
	char *editline;

	/* FILE COMMAND (r) SPECIFIC FIELDS */
	char *filename;

	/* address storage */
	int beg_line; /* 'sed 1p'   0 == no begining line, apply commands to all lines */
	int end_line; /* 'sed 1,3p' 0 == no end line, use only beginning. -1 == $ */
	/* SUBSTITUTION COMMAND SPECIFIC FIELDS */

	unsigned int num_backrefs:4; /* how many back references (\1..\9) */
			/* Note:  GNU/POSIX sed does not save more than nine backrefs, so
			 * we only use 4 bits to hold the number */
	unsigned int sub_g:1; /* sed -e 's/foo/bar/g' (global) */
	unsigned int sub_p:2; /* sed -e 's/foo/bar/p' (print substitution) */

	/* GENERAL FIELDS */
	char delimiter;	    /* The delimiter used to separate regexps */

	/* the command */
	char cmd; /* p,d,s (add more at your leisure :-) */

	/* inversion flag */
	int invert;         /* the '!' after the address */ 
};

/* globals */
static struct sed_cmd *sed_cmds = NULL; /* growable arrary holding a sequence of sed cmds */
static int ncmds = 0; /* number of sed commands */

/*static char *cur_file = NULL;*/ /* file currently being processed XXX: do I need this? */

const char * const semicolon_whitespace = "; \n\r\t\v\0";

#ifdef CONFIG_FEATURE_CLEAN_UP
static void destroy_cmd_strs(void)
{
	if (sed_cmds == NULL)
		return;

	/* destroy all the elements in the array */
	while (--ncmds >= 0) {

		if (sed_cmds[ncmds].beg_match) {
			regfree(sed_cmds[ncmds].beg_match);
			free(sed_cmds[ncmds].beg_match);
		}
		if (sed_cmds[ncmds].end_match) {
			regfree(sed_cmds[ncmds].end_match);
			free(sed_cmds[ncmds].end_match);
		}
		if (sed_cmds[ncmds].sub_match) {
			regfree(sed_cmds[ncmds].sub_match);
			free(sed_cmds[ncmds].sub_match);
		}
		if (sed_cmds[ncmds].replace)
			free(sed_cmds[ncmds].replace);
	}

	/* destroy the array */
	free(sed_cmds);
	sed_cmds = NULL;
}
#endif


/*
 * index_of_next_unescaped_regexp_delim - walks left to right through a string
 * beginning at a specified index and returns the index of the next regular
 * expression delimiter (typically a forward * slash ('/')) not preceeded by 
 * a backslash ('\').
 */
static int index_of_next_unescaped_regexp_delim(const struct sed_cmd * const sed_cmd, const char *str, int idx)
{
	int bracket = -1;
	int escaped = 0;
	char ch;

	for ( ; (ch = str[idx]); idx++) {
		if (bracket != -1) {
			if (ch == ']' && !(bracket == idx - 1 ||
									 (bracket == idx - 2 && str[idx-1] == '^')))
				bracket = -1;
		} else if (escaped)
			escaped = 0;
		else if (ch == '\\')
			escaped = 1;
		else if (ch == '[')
			bracket = idx;
		else if (ch == sed_cmd->delimiter)
			return idx;
	}

	/* if we make it to here, we've hit the end of the string */
	return -1;
}

/*
 * returns the index in the string just past where the address ends.
 */
static int get_address(struct sed_cmd *sed_cmd, const char *str, int *linenum, regex_t **regex)
{
	char *my_str = xstrdup(str);
	int idx = 0;
	char olddelimiter;
	olddelimiter = sed_cmd->delimiter;
	sed_cmd->delimiter = '/';

	if (isdigit(my_str[idx])) {
		do {
			idx++;
		} while (isdigit(my_str[idx]));
		my_str[idx] = 0;
		*linenum = atoi(my_str);
	}
	else if (my_str[idx] == '$') {
		*linenum = -1;
		idx++;
	}
	else if (my_str[idx] == '/') {
		idx = index_of_next_unescaped_regexp_delim(sed_cmd, my_str, ++idx);
		if (idx == -1)
			error_msg_and_die("unterminated match expression");
		my_str[idx] = '\0';
		*regex = (regex_t *)xmalloc(sizeof(regex_t));
		xregcomp(*regex, my_str+1, REG_NEWLINE);
		idx++; /* so it points to the next character after the last '/' */
	}
	else {
		error_msg("get_address: no address found in string\n"
				"\t(you probably didn't check the string you passed me)");
		idx = -1;
	}

	free(my_str);
	sed_cmd->delimiter = olddelimiter;
	return idx;
}

static int parse_subst_cmd(struct sed_cmd * const sed_cmd, const char *substr)
{
	int oldidx, cflags = REG_NEWLINE;
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

	/* verify that the 's' is followed by something.  That something
	 * (typically a 'slash') is now our regexp delimiter... */
	if (!substr[++idx])
		error_msg_and_die("bad format in substitution expression");
	else
	    sed_cmd->delimiter=substr[idx];

	/* save the match string */
	oldidx = idx+1;
	idx = index_of_next_unescaped_regexp_delim(sed_cmd, substr, ++idx);
	if (idx == -1)
		error_msg_and_die("bad format in substitution expression");
	match = xstrndup(substr + oldidx, idx - oldidx);

	/* determine the number of back references in the match string */
	/* Note: we compute this here rather than in the do_subst_command()
	 * function to save processor time, at the expense of a little more memory
	 * (4 bits) per sed_cmd */
	
	/* sed_cmd->num_backrefs = 0; */ /* XXX: not needed? --apparently not */ 
	for (j = 0; match[j]; j++) {
		/* GNU/POSIX sed does not save more than nine backrefs */
		if (match[j] == '\\' && match[j+1] == '(' && sed_cmd->num_backrefs <= 9)
			sed_cmd->num_backrefs++;
	}

	/* save the replacement string */
	oldidx = idx+1;
	idx = index_of_next_unescaped_regexp_delim(sed_cmd, substr, ++idx);
	if (idx == -1)
		error_msg_and_die("bad format in substitution expression");
	sed_cmd->replace = xstrndup(substr + oldidx, idx - oldidx);

	/* process the flags */
	while (substr[++idx]) {
		switch (substr[idx]) {
			case 'g':
				sed_cmd->sub_g = 1;
				break;
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
				error_msg_and_die("bad option in substitution expression");
		}
	}

out:	
	/* compile the match string into a regex */
	sed_cmd->sub_match = (regex_t *)xmalloc(sizeof(regex_t));
	xregcomp(sed_cmd->sub_match, match, cflags);
	free(match);

	return idx;
}

static void move_back(char *str, int offset)
{
	memmove(str, str + offset, strlen(str + offset) + 1);
}

static int parse_edit_cmd(struct sed_cmd *sed_cmd, const char *editstr)
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

	if (editstr[1] != '\\' || (editstr[2] != '\n' && editstr[2] != '\r'))
		error_msg_and_die("bad format in edit expression");

	/* store the edit line text */
	sed_cmd->editline = xmalloc(strlen(&editstr[3]) + 2);
	for (i = 3, j = 0; editstr[i] != '\0' && strchr("\r\n", editstr[i]) == NULL;
			i++, j++) {
		if (editstr[i] == '\\' && strchr("\n\r", editstr[i+1]) != NULL) {
			sed_cmd->editline[j] = '\n';
			i++;
		} else
			sed_cmd->editline[j] = editstr[i];
	}

	/* figure out if we need to add a newline */
	if (sed_cmd->editline[j-1] != '\n')
		sed_cmd->editline[j++] = '\n';

	/* terminate string */
	sed_cmd->editline[j] = '\0';

	return i;
}


static int parse_file_cmd(struct sed_cmd *sed_cmd, const char *filecmdstr)
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
	while (isspace(filecmdstr[++idx]))
		{ ; }
		
	/* the first non-whitespace we get is a filename. the filename ends when we
	 * hit a normal sed command terminator or end of string */
	filenamelen = strcspn(&filecmdstr[idx], semicolon_whitespace);
	sed_cmd->filename = xmalloc(filenamelen + 1);
	safe_strncpy(sed_cmd->filename, &filecmdstr[idx], filenamelen + 1);

	return idx + filenamelen;
}


static char *parse_cmd_str(struct sed_cmd * const sed_cmd, const char *const cmdstr)
{
	int idx = 0;

	/* parse the command
	 * format is: [addr][,addr]cmd
	 *            |----||-----||-|
	 *            part1 part2  part3
	 */

	/* first part (if present) is an address: either a number or a /regex/ */
	if (isdigit(cmdstr[idx]) || cmdstr[idx] == '/')
		idx = get_address(sed_cmd, cmdstr, &sed_cmd->beg_line, &sed_cmd->beg_match);

	/* second part (if present) will begin with a comma */
	if (cmdstr[idx] == ',')
		idx += get_address(sed_cmd, &cmdstr[++idx], &sed_cmd->end_line, &sed_cmd->end_match);

	/* skip whitespace before the command */
	while (isspace(cmdstr[idx]))
		idx++;

	/* there my be the inversion flag between part2 and part3 */
	sed_cmd->invert = 0;
	if (cmdstr[idx] == '!') {
		sed_cmd->invert = 1;
		idx++;

		/* skip whitespace before the command */
		while (isspace(cmdstr[idx]))
			idx++;
	}

	/* last part (mandatory) will be a command */
	if (cmdstr[idx] == '\0')
		error_msg_and_die("missing command");
	sed_cmd->cmd = cmdstr[idx];

	/* if it was a single-letter command that takes no arguments (such as 'p'
	 * or 'd') all we need to do is increment the index past that command */
	if (strchr("pd", sed_cmd->cmd)) {
		idx++;
	}
	/* handle (s)ubstitution command */
	else if (sed_cmd->cmd == 's') {
		idx += parse_subst_cmd(sed_cmd, &cmdstr[idx]);
	}
	/* handle edit cmds: (a)ppend, (i)nsert, and (c)hange */
	else if (strchr("aic", sed_cmd->cmd)) {
		if ((sed_cmd->end_line || sed_cmd->end_match) && sed_cmd->cmd != 'c')
			error_msg_and_die("only a beginning address can be specified for edit commands");
		idx += parse_edit_cmd(sed_cmd, &cmdstr[idx]);
	}
	/* handle file cmds: (r)ead */
	else if (sed_cmd->cmd == 'r') {
		if (sed_cmd->end_line || sed_cmd->end_match)
			error_msg_and_die("Command only uses one address");
		idx += parse_file_cmd(sed_cmd, &cmdstr[idx]);
	}
	else {
		error_msg_and_die("invalid command");
	}

	/* give back whatever's left over */
	return (char *)&cmdstr[idx];
}

static void add_cmd_str(const char * const cmdstr)
{
	char *mystr = (char *)cmdstr;

	do {

		/* trim leading whitespace and semicolons */
		move_back(mystr, strspn(mystr, semicolon_whitespace));
		/* if we ate the whole thing, that means there was just trailing
		 * whitespace or a final / no-op semicolon. either way, get out */
		if (strlen(mystr) == 0)
			return;
		/* if this is a comment, jump past it and keep going */
		if (mystr[0] == '#') {
			mystr = strpbrk(mystr, "\n\r");
			continue;
		}
		/* grow the array */
		sed_cmds = xrealloc(sed_cmds, sizeof(struct sed_cmd) * (++ncmds));
		/* zero new element */
		memset(&sed_cmds[ncmds-1], 0, sizeof(struct sed_cmd));
		/* load command string into new array element, get remainder */
		mystr = parse_cmd_str(&sed_cmds[ncmds-1], mystr);

	} while (mystr && strlen(mystr));
}


static void load_cmd_file(char *filename)
{
	FILE *cmdfile;
	char *line;
	char *nextline;

	cmdfile = xfopen(filename, "r");

	while ((line = get_line_from_file(cmdfile)) != NULL) {
		/* if a line ends with '\' it needs the next line appended to it */
		while (line[strlen(line)-2] == '\\' &&
				(nextline = get_line_from_file(cmdfile)) != NULL) {
			line = xrealloc(line, strlen(line) + strlen(nextline) + 1);
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
		pipeline->buf =
			xrealloc(pipeline->buf, pipeline->len + PIPE_GROW);
		memset(pipeline->buf + pipeline->len, 0, PIPE_GROW);
		pipeline->len += PIPE_GROW;
		pipeline->buf[pipeline->len - 1] = PIPE_MAGIC;
	}
	pipeline->buf[pipeline->idx++] = (c);
}

#define pipeputc(c) 	pipe_putc(pipeline, c)

#if 0
{ if (pipeline[pipeline_idx] == PIPE_MAGIC) { \
	pipeline = xrealloc(pipeline, pipeline_len+PIPE_GROW); \
	memset(pipeline+pipeline_len, 0, PIPE_GROW); \
	pipeline_len += PIPE_GROW; \
	pipeline[pipeline_len-1] = PIPE_MAGIC; } \
	pipeline[pipeline_idx++] = (c); }
#endif

static void print_subst_w_backrefs(const char *line, const char *replace, 
	regmatch_t *regmatch, struct pipeline *const pipeline, int matches)
{
	int i;

	/* go through the replacement string */
	for (i = 0; replace[i]; i++) {
		/* if we find a backreference (\1, \2, etc.) print the backref'ed * text */
		if (replace[i] == '\\' && isdigit(replace[i+1])) {
			int j;
			char tmpstr[2];
			int backref;
			++i; /* i now indexes the backref number, instead of the leading slash */
			tmpstr[0] = replace[i];
			tmpstr[1] = 0;
			backref = atoi(tmpstr);
			/* print out the text held in regmatch[backref] */
			if (backref <= matches && regmatch[backref].rm_so != -1)
				for (j = regmatch[backref].rm_so; j < regmatch[backref].rm_eo; j++)
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
		else if (replace[i] == '&' && replace[i-1] != '\\') {
			int j;
			for (j = regmatch[0].rm_so; j < regmatch[0].rm_eo; j++)
				pipeputc(line[j]);
		}
		/* nothing special, just print this char of the replacement string to stdout */
		else
			pipeputc(replace[i]);
	}
}

static int do_subst_command(const struct sed_cmd *sed_cmd, char **line)
{
	char *hackline = *line;
	struct pipeline thepipe = { NULL, 0 , 0};
	struct pipeline *const pipeline = &thepipe;
	int altered = 0;
	regmatch_t *regmatch = NULL;

	/* we only proceed if the substitution 'search' expression matches */
	if (regexec(sed_cmd->sub_match, hackline, 0, NULL, 0) == REG_NOMATCH)
		return 0;

	/* whaddaya know, it matched. get the number of back references */
	regmatch = xmalloc(sizeof(regmatch_t) * (sed_cmd->num_backrefs+1));

	/* allocate more PIPE_GROW bytes
	   if replaced string is larger than original */
	thepipe.len = strlen(hackline)+PIPE_GROW;
	thepipe.buf = xcalloc(1, thepipe.len);
	/* buffer magic */
	thepipe.buf[thepipe.len-1] = PIPE_MAGIC;

	/* and now, as long as we've got a line to try matching and if we can match
	 * the search string, we make substitutions */
	while ((*hackline || !altered) && (regexec(sed_cmd->sub_match, hackline,
					sed_cmd->num_backrefs+1, regmatch, 0) != REG_NOMATCH) ) {
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
		if (!sed_cmd->sub_g)
			break;
	}

	for (; *hackline; hackline++) pipeputc(*hackline);
	if (thepipe.buf[thepipe.idx] == PIPE_MAGIC) thepipe.buf[thepipe.idx] = 0;

	/* cleanup */
	free(regmatch);

	free(*line);
	*line = thepipe.buf;
	return altered;
}


static void process_file(FILE *file)
{
	char *line = NULL;
	static int linenum = 0; /* GNU sed does not restart counting lines at EOF */
	unsigned int still_in_range = 0;
	int altered;
	int i;

	/* go through every line in the file */
	while ((line = get_line_from_file(file)) != NULL) {

		chomp(line);
		linenum++;
		altered = 0;

		/* for every line, go through all the commands */
		for (i = 0; i < ncmds; i++) {
			struct sed_cmd *sed_cmd = &sed_cmds[i];
			int deleted = 0;

			/*
			 * entry point into sedding...
			 */
			int matched = (
					/* no range necessary */
					(sed_cmd->beg_line == 0 && sed_cmd->end_line == 0 &&
					 sed_cmd->beg_match == NULL &&
					 sed_cmd->end_match == NULL) ||
					/* this line number is the first address we're looking for */
					(sed_cmd->beg_line && (sed_cmd->beg_line == linenum)) ||
					/* this line matches our first address regex */
					(sed_cmd->beg_match && (regexec(sed_cmd->beg_match, line, 0, NULL, 0) == 0)) ||
					/* we are currently within the beginning & ending address range */
					still_in_range
			   );

			if (sed_cmd->invert ^ matched) {

				/*
				 * actual sedding
				 */
				switch (sed_cmd->cmd) {

					case 'p':
						puts(line);
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
						 *    the line is going to be printed anyway (line will be printed
						 *    twice).
						 *
						 *    -n AND s///p = print ONLY a successful substitution ONE TIME;
						 *    no other lines are printed - this is the reason why the 'p'
						 *    flag exists in the first place.
						 */

						/* if the user specified that they didn't want anything printed (i.e., a -n
						 * flag and no 'p' flag after the s///), then there's really no point doing
						 * anything here. */
						if (be_quiet && !sed_cmd->sub_p)
							break;

						/* we print the line once, unless we were told to be quiet */
						if (!be_quiet)
							altered |= do_subst_command(sed_cmd, &line);

						/* we also print the line if we were given the 'p' flag
						 * (this is quite possibly the second printing) */
						if (sed_cmd->sub_p)
							altered |= do_subst_command(sed_cmd, &line);
						if (altered && (i+1 >= ncmds || sed_cmds[i+1].cmd != 's'))
							puts(line);

						break;

					case 'a':
						puts(line);
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
						|| (sed_cmd->end_match && (regexec(sed_cmd->end_match, line, 0, NULL, 0) == 0))
						/* - matching line numbers */
						|| (sed_cmd->end_line > 0 && sed_cmd->end_line == linenum))
						{
							fputs(sed_cmd->editline, stdout);
						}
						altered++;

						break;

					case 'r': {
								  FILE *outfile;
								  puts(line);
								  outfile = fopen(sed_cmd->filename, "r");
								  if (outfile)
									  print_file(outfile);
								  /* else if we couldn't open the output file,
								   * no biggie, just don't print anything */
								  altered++;
						  }
							  break;
				}
			}

			/*
			 * exit point from sedding...
			 */
			if (matched) {
				if (
					/* this is a single-address command or... */
					(sed_cmd->end_line == 0 && sed_cmd->end_match == NULL) || (
						/* we were in the middle of our address range (this
						 * isn't the first time through) and.. */
						(still_in_range == 1) && (
							/* this line number is the last address we're looking for or... */
							(sed_cmd->end_line && (sed_cmd->end_line == linenum)) ||
							/* this line matches our last address regex */
							(sed_cmd->end_match && (regexec(sed_cmd->end_match, line, 0, NULL, 0) == 0))
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
		if (!be_quiet && !altered)
			puts(line);

		free(line);
	}
}

extern int sed_main(int argc, char **argv)
{
	int opt, status = EXIT_SUCCESS;

#ifdef CONFIG_FEATURE_CLEAN_UP
	/* destroy command strings on exit */
	if (atexit(destroy_cmd_strs) == -1)
		perror_msg_and_die("atexit");
#endif

	/* do normal option parsing */
	while ((opt = getopt(argc, argv, "ne:f:")) > 0) {
		switch (opt) {
			case 'n':
				be_quiet++;
				break;
			case 'e':
				add_cmd_str(optarg);
				break;
			case 'f': 
				load_cmd_file(optarg);
				break;
			default:
				show_usage();
		}
	}

	/* if we didn't get a pattern from a -e and no command file was specified,
	 * argv[optind] should be the pattern. no pattern, no worky */
	if (ncmds == 0) {
		if (argv[optind] == NULL)
			show_usage();
		else {
			add_cmd_str(argv[optind]);
			optind++;
		}
	}


	/* argv[(optind)..(argc-1)] should be names of file to process. If no
	 * files were specified or '-' was specified, take input from stdin.
	 * Otherwise, we process all the files specified. */
	if (argv[optind] == NULL || (strcmp(argv[optind], "-") == 0)) {
		process_file(stdin);
	}
	else {
		int i;
		FILE *file;
		for (i = optind; i < argc; i++) {
			file = wfopen(argv[i], "r");
			if (file) {
				process_file(file);
				fclose(file);
			} else
				status = EXIT_FAILURE;
		}
	}
	
	return status;
}
