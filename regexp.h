/*
 * Busybox regexp header file
 *
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
 * Based in part on code from sash, Copyright (c) 1999 by David I. Bell 
 * Permission has been granted to redistribute this code under the GPL.
 *
 */
#ifndef	_REGEXP_H_
#define	_REGEXP_H_




#define NSUBEXP  10
typedef struct regexp {
	char	*startp[NSUBEXP];
	char	*endp[NSUBEXP];
	int	minlen;		/* length of shortest possible match */
	char	first;		/* first character, if known; else \0 */
	char	bol;		/* boolean: must start at beginning of line? */
	char	program[1];	/* Unwarranted chumminess with compiler. */
} regexp;



extern regexp *regcomp(char* text);
extern int regexec(struct regexp* re, char* str, int bol, int ignoreCase);
extern void regsub(struct regexp* re, char* src, char* dst);

extern int find_match(char *haystack, char *needle, int ignoreCase);
extern int replace_match(char *haystack, char *needle, char *newNeedle, int ignoreCase);

#endif


