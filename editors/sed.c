/*
 * sed.c - very minimalist version of sed
 *
 * Copyright (C) 1999,2000 by Lineo, inc.
 * Written by Mark Whitley <markw@lineo.com>, <markw@codepoet.org>
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


	/* GENERAL FIELDS */
	char delimiter;	    /* The delimiter used to separate regexps */

	/* address storage */
	int beg_line; /* 'sed 1p'   0 == no begining line, apply commands to all lines */
	int end_line; /* 'sed 1,3p' 0 == no end line, use only beginning. -1 == $ */
	regex_t *beg_match; /* sed -e '/match/cmd' */
	regex_t *end_match; /* sed -e '/match/,/end_match/cmd' */

	/* the command */
	char cmd; /* p,d,s (add more at your leisure :-) */


	/* SUBSTITUTION COMMAND SPECIFIC FIELDS */

	/* sed -e 's/sub_match/replace/' */
	regex_t *sub_match;
	char *replace;
	unsigned int num_backrefs:4; /* how many back references (\1..\9) */
			/* Note:  GNU/POSIX sed does not save more than nine backrefs, so
			 * we only use 4 bits to hold the number */
	unsigned int sub_g:1; /* sed -e 's/foo/bar/g' (global) */
	unsigned int sub_p:2; /* sed -e 's/foo/bar/p' (print substitution) */


	/* EDIT COMMAND (a,i,c) SPEICIFIC FIELDS */

	char *editline;
};

/* globals */
static struct sed_cmd *sed_cmds = NULL; /* growable arrary holding a sequence of sed cmds */
static int ncmds = 0; /* number of sed commands */

/*static char *cur_file = NULL;*/ /* file currently being processed XXX: do I need this? */

#ifdef BB_FEATURE_CLEAN_UP
static void destroy_cmd_strs()
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
static int index_of_next_unescaped_regexp_delim(struct sed_cmd *sed_cmd, const char *str, int idx)
{
	for ( ; str[idx]; idx++) {
		if (str[idx] == sed_cmd->delimiter && str[idx-1] != '\\')
			return idx;
	}

	/* if we make it to here, we've hit the end of the string */
	return -1;
}

/*
 * returns the index in the string just past where the address ends.
 */
static int get_address(struct sed_cmd *sed_cmd, const char *str, int *line, regex_t **regex)
{
	char *my_str = strdup(str);
	int idx = 0;

	if (isdigit(my_str[idx])) {
		do {
			idx++;
		} while (isdigit(my_str[idx]));
		my_str[idx] = 0;
		*line = atoi(my_str);
	}
	else if (my_str[idx] == '$') {
		*line = -1;
		idx++;
	}
	else if (my_str[idx] == '/') {
		idx = index_of_next_unescaped_regexp_delim(sed_cmd, my_str, ++idx);
		if (idx == -1)
			error_msg_and_die("unterminated match expression\n");
		my_str[idx] = '\0';
		*regex = (regex_t *)xmalloc(sizeof(regex_t));
		xregcomp(*regex, my_str+1, 0);
		idx++; /* so it points to the next character after the last '/' */
	}
	else {
		error_msg("get_address: no address found in string\n"
				"\t(you probably didn't check the string you passed me)\n");
		idx = -1;
	}

	free(my_str);
	return idx;
}

static char *strdup_substr(const char *str, int start, int end)
{
	int size = end - start + 1;
	char *newstr = xmalloc(size);
	memcpy(newstr, str+start, size-1);
	newstr[size-1] = '\0';
	return newstr;
}

static int parse_subst_cmd(struct sed_cmd *sed_cmd, const char *substr)
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
		error_msg_and_die("bad format in substitution expression\n");
	else
	    sed_cmd->delimiter=substr[idx];

	/* save the match string */
	oldidx = idx+1;
	idx = index_of_next_unescaped_regexp_delim(sed_cmd, substr, ++idx);
	if (idx == -1)
		error_msg_and_die("bad format in substitution expression\n");
	match = strdup_substr(substr, oldidx, idx);

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
		error_msg_and_die("bad format in substitution expression\n");
	sed_cmd->replace = strdup_substr(substr, oldidx, idx);

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
				if (strchr("; \t\v\n\r", substr[idx]))
					goto out;
				/* else */
				error_msg_and_die("bad option in substitution expression\n");
		}
	}

out:	
	/* compile the match string into a regex */
	sed_cmd->sub_match = (regex_t *)xmalloc(sizeof(regex_t));
	xregcomp(sed_cmd->sub_match, match, cflags);
	free(match);

	return idx;
}

static int parse_edit_cmd(struct sed_cmd *sed_cmd, const char *editstr)
{
	int idx = 0;
	int slashes_eaten = 0;
	char *ptr; /* shorthand */

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

	if (editstr[1] != '\\' && (editstr[2] != '\n' || editstr[2] != '\r'))
		error_msg_and_die("bad format in edit expression\n");

	/* store the edit line text */
	/* make editline big enough to accomodate the extra '\n' we will tack on
	 * to the end */
	sed_cmd->editline = xmalloc(strlen(&editstr[3]) + 2);
	strcpy(sed_cmd->editline, &editstr[3]);
	ptr = sed_cmd->editline;

	/* now we need to go through * and: s/\\[\r\n]$/\n/g on the edit line */
	while (ptr[idx]) {
		while (ptr[idx] != '\\' && (ptr[idx+1] != '\n' || ptr[idx+1] != '\r')) {
			idx++;
			if (!ptr[idx]) {
				goto out;
			}
		}
		/* move the newline over the '\' before it (effectively eats the '\') */
		memmove(&ptr[idx], &ptr[idx+1], strlen(&ptr[idx+1]));
		ptr[strlen(ptr)-1] = 0;
		slashes_eaten++;
		/* substitue \r for \n if needed */
		if (ptr[idx] == '\r')
			ptr[idx] = '\n';
	}

out:
	ptr[idx] = '\n';
	ptr[idx+1] = 0;

	/* this accounts for discrepancies between the modified string and the
	 * original string passed in to this function */
	idx += slashes_eaten;

	/* this accounts for the fact that A) we started at index 3, not at index
	 * 0  and B) that we added an extra '\n' at the end (if you think the next
	 * line should read 'idx += 4' remember, arrays are zero-based) */

	idx += 3;

	return idx;
}

static char *parse_cmd_str(struct sed_cmd *sed_cmd, const char *cmdstr)
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

	/* last part (mandatory) will be a command */
	if (cmdstr[idx] == '\0')
		error_msg_and_die("missing command\n");
	if (!strchr("pdsaic", cmdstr[idx])) /* <-- XXX add new commands here */
		error_msg_and_die("invalid command\n");
	sed_cmd->cmd = cmdstr[idx];

	/* special-case handling for (s)ubstitution */
	if (sed_cmd->cmd == 's') {
		idx += parse_subst_cmd(sed_cmd, &cmdstr[idx]);
	}
	/* special-case handling for (a)ppend, (i)nsert, and (c)hange */
	else if (strchr("aic", cmdstr[idx])) {
		if (sed_cmd->end_line || sed_cmd->end_match)
			error_msg_and_die("only a beginning address can be specified for edit commands\n");
		idx += parse_edit_cmd(sed_cmd, &cmdstr[idx]);
	}
	/* if it was a single-letter command (such as 'p' or 'd') we need to
	 * increment the index past that command */
	else
		idx++;

	/* give back whatever's left over */
	return (char *)&cmdstr[idx];
}

static void add_cmd_str(const char *cmdstr)
{
	char *mystr = (char *)cmdstr;

	do {

		/* trim leading whitespace and semicolons */
		memmove(mystr, &mystr[strspn(mystr, "; \n\r\t\v")], strlen(mystr));
		/* if we ate the whole thing, that means there was just trailing
		 * whitespace or a final / no-op semicolon. either way, get out */
		if (strlen(mystr) == 0)
			return;
		/* if this is a comment, jump past it and keep going */
		if (mystr[0] == '#') {
			mystr = strpbrk(mystr, ";\n\r");
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
		if (line[strlen(line)-1] == '\n')
			line[strlen(line)-1] = 0;
		add_cmd_str(line);
		free(line);
	}
}

static void print_subst_w_backrefs(const char *line, const char *replace, regmatch_t *regmatch)
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
			for (j = regmatch[backref].rm_so; j < regmatch[backref].rm_eo; j++)
				fputc(line[j], stdout);
		}

		/* if we find a backslash escaped character, print the character */
		else if (replace[i] == '\\') {
			++i;
			fputc(replace[i], stdout);
		}

		/* if we find an unescaped '&' print out the whole matched text.
		 * fortunately, regmatch[0] contains the indicies to the whole matched
		 * expression (kinda seems like it was designed for just such a
		 * purpose...) */
		else if (replace[i] == '&' && replace[i-1] != '\\') {
			int j;
			for (j = regmatch[0].rm_so; j < regmatch[0].rm_eo; j++)
				fputc(line[j], stdout);
		}
		/* nothing special, just print this char of the replacement string to stdout */
		else
			fputc(replace[i], stdout);
	}
}

static int do_subst_command(const struct sed_cmd *sed_cmd, const char *line)
{
	char *hackline = (char *)line;
	int altered = 0;
	regmatch_t *regmatch = NULL;

	/* we only proceed if the substitution 'search' expression matches */
	if (regexec(sed_cmd->sub_match, line, 0, NULL, 0) == REG_NOMATCH)
		return 0;

	/* whaddaya know, it matched. get the number of back references */
	regmatch = xmalloc(sizeof(regmatch_t) * (sed_cmd->num_backrefs+1));

	/* and now, as long as we've got a line to try matching and if we can match
	 * the search string, we make substitutions */
	while (*hackline && (regexec(sed_cmd->sub_match, hackline,
					sed_cmd->num_backrefs+1, regmatch, 0) == 0) ) {
		int i;

		/* print everything before the match */
		for (i = 0; i < regmatch[0].rm_so; i++)
			fputc(hackline[i], stdout);

		/* then print the substitution string */
		print_subst_w_backrefs(hackline, sed_cmd->replace, regmatch);

		/* advance past the match */
		hackline += regmatch[0].rm_eo;
		/* flag that something has changed */
		altered++;

		/* if we're not doing this globally, get out now */
		if (!sed_cmd->sub_g)
			break;
	}

	/* if there's anything left of the line, print it */
	if (*hackline)
		fputs(hackline, stdout);

	/* cleanup */
	free(regmatch);

	return altered;
}

static int do_sed_command(const struct sed_cmd *sed_cmd, const char *line) 
{
	int altered = 0;

	switch (sed_cmd->cmd) {

		case 'p':
			fputs(line, stdout);
			break;

		case 'd':
			altered++;
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

			/* if the user specified that they didn't want anything printed (i.e. a -n
			 * flag and no 'p' flag after the s///), then there's really no point doing
			 * anything here. */
			if (be_quiet && !sed_cmd->sub_p)
				break;

			/* we print the line once, unless we were told to be quiet */
			if (!be_quiet)
				altered = do_subst_command(sed_cmd, line);

			/* we also print the line if we were given the 'p' flag
			 * (this is quite possibly the second printing) */
			if (sed_cmd->sub_p)
				altered = do_subst_command(sed_cmd, line);

			break;

		case 'a':
			fputs(line, stdout);
			fputs(sed_cmd->editline, stdout);
			altered++;
			break;

		case 'i':
			fputs(sed_cmd->editline, stdout);
			break;

		case 'c':
			fputs(sed_cmd->editline, stdout);
			altered++;
			break;
	}

	return altered;
}

static void process_file(FILE *file)
{
	char *line = NULL;
	static int linenum = 0; /* GNU sed does not restart counting lines at EOF */
	unsigned int still_in_range = 0;
	int line_altered;
	int i;

	/* go through every line in the file */
	while ((line = get_line_from_file(file)) != NULL) {

		linenum++;
		line_altered = 0;

		/* for every line, go through all the commands */
		for (i = 0; i < ncmds; i++) {

			/* are we acting on a range of matched lines? */
			if (sed_cmds[i].beg_match && sed_cmds[i].end_match) {
				if (still_in_range || regexec(sed_cmds[i].beg_match, line, 0, NULL, 0) == 0) {
					line_altered += do_sed_command(&sed_cmds[i], line);
					still_in_range = 1; 
					if (regexec(sed_cmds[i].end_match, line, 0, NULL, 0) == 0)
						still_in_range = 0;
				}
			}

			/* are we trying to match a single line? */
			else if (sed_cmds[i].beg_match) {
				if (regexec(sed_cmds[i].beg_match, line, 0, NULL, 0) == 0)
					line_altered += do_sed_command(&sed_cmds[i], line);
			}

			/* are we acting on a range of line numbers? */
			else if (sed_cmds[i].beg_line > 0 && sed_cmds[i].end_line != 0) {
				if (linenum >= sed_cmds[i].beg_line &&
						(sed_cmds[i].end_line == -1 || linenum <= sed_cmds[i].end_line))
					line_altered += do_sed_command(&sed_cmds[i], line);
			}

			/* are we acting on a specified line number */
			else if (sed_cmds[i].beg_line > 0) {
				if (linenum == sed_cmds[i].beg_line)
					line_altered += do_sed_command(&sed_cmds[i], line);
			}

			/* not acting on matches or line numbers. act on every line */
			else 
				line_altered += do_sed_command(&sed_cmds[i], line);

		}

		/* we will print the line unless we were told to be quiet or if the
		 * line was altered (via a 'd'elete or 's'ubstitution), in which case
		 * the altered line was already printed */
		if (!be_quiet && !line_altered)
			fputs(line, stdout);

		free(line);
	}
}

extern int sed_main(int argc, char **argv)
{
	int opt;

#ifdef BB_FEATURE_CLEAN_UP
	/* destroy command strings on exit */
	if (atexit(destroy_cmd_strs) == -1)
		perror_msg_and_die("atexit");
#endif

	/* do normal option parsing */
	while ((opt = getopt(argc, argv, "hne:f:")) > 0) {
		switch (opt) {
			case 'h':
				usage(sed_usage);
				break;
			case 'n':
				be_quiet++;
				break;
			case 'e':
				add_cmd_str(optarg);
				break;
			case 'f': 
				load_cmd_file(optarg);
				break;
		}
	}

	/* if we didn't get a pattern from a -e and no command file was specified,
	 * argv[optind] should be the pattern. no pattern, no worky */
	if (ncmds == 0) {
		if (argv[optind] == NULL)
			usage(sed_usage);
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
			file = fopen(argv[i], "r");
			if (file == NULL) {
				perror_msg("%s", argv[i]);
			} else {
				process_file(file);
				fclose(file);
			}
		}
	}
	
	return 0;
}
