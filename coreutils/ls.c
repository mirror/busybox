#include "internal.h"
/*
 * tiny-ls.c version 0.1.0: A minimalist 'ls'
 * Copyright (C) 1996 Brian Candler <B.Candler@pobox.com>
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * To achieve a small memory footprint, this version of 'ls' doesn't do any
 * file sorting, and only has the most essential command line switches
 * (i.e. the ones I couldn't live without :-) All features which involve
 * linking in substantial chunks of libc can be disabled.
 *
 * Although I don't really want to add new features to this program to
 * keep it small, I *am* interested to receive bug fixes and ways to make
 * it more portable.
 *
 * KNOWN BUGS:
 * 1. messy output if you mix files and directories on the command line
 * 2. ls -l of a directory doesn't give "total <blocks>" header
 * 3. ls of a symlink to a directory doesn't list directory contents
 * 4. hidden files can make column width too large
 * NON-OPTIMAL BEHAVIOUR:
 * 1. autowidth reads directories twice
 * 2. if you do a short directory listing without filetype characters
 *    appended, there's no need to stat each one
 * PORTABILITY:
 * 1. requires lstat (BSD) - how do you do it without?
 */

#define FEATURE_USERNAME	/* show username/groupnames (libc6 uses NSS) */
#define FEATURE_TIMESTAMPS	/* show file timestamps */
#define FEATURE_AUTOWIDTH	/* calculate terminal & column widths */
#define FEATURE_FILETYPECHAR	/* enable -p and -F */

#undef	OP_BUF_SIZE	1024	/* leave undefined for unbuffered output */

#define TERMINAL_WIDTH	80	/* use 79 if your terminal has linefold bug */
#define	COLUMN_WIDTH	14	/* default if AUTOWIDTH not defined */
#define COLUMN_GAP	2	/* includes the file type char, if present */

/************************************************************************/

#define HAS_REWINDDIR

#if 1 /* FIXME libc 6 */
# include <linux/types.h> 
#else
# include <sys/types.h> 
#endif
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#ifdef FEATURE_USERNAME
#include <pwd.h>
#include <grp.h>
#endif
#ifdef FEATURE_TIMESTAMPS
#include <time.h>
#endif

#define TYPEINDEX(mode)	(((mode) >> 12) & 0x0f)
#define TYPECHAR(mode)	("0pcCd?bB-?l?s???" [TYPEINDEX(mode)])
#ifdef FEATURE_FILETYPECHAR
#define APPCHAR(mode)	("\0|\0\0/\0\0\0\0\0@\0=\0\0\0" [TYPEINDEX(mode)])
#endif

#ifndef MAJOR
#define MAJOR(dev) (((dev)>>8)&0xff)
#define MINOR(dev) ((dev)&0xff)
#endif

#define MODE1  "rwxrwxrwx"
#define MODE0  "---------"
#define SMODE1 "..s..s..t"
#define SMODE0 "..S..S..T"

/* The 9 mode bits to test */

static const umode_t MBIT[] = {
  S_IRUSR, S_IWUSR, S_IXUSR,
  S_IRGRP, S_IWGRP, S_IXGRP,
  S_IROTH, S_IWOTH, S_IXOTH
};

/* The special bits. If set, display SMODE0/1 instead of MODE0/1 */

static const umode_t SBIT[] = {
  0, 0, S_ISUID,
  0, 0, S_ISGID,
  0, 0, S_ISVTX
};

#define FMT_AUTO	0
#define FMT_LONG	1	/* one record per line, extended info */
#define FMT_SINGLE	2	/* one record per line */
#define FMT_ROWS	3	/* print across rows */
#define FMT_COLUMNS	3	/* fill columns (same, since we don't sort) */

#define TIME_MOD	0
#define TIME_CHANGE	1
#define TIME_ACCESS	2

#define DISP_FTYPE	1	/* show character for file type */
#define DISP_EXEC	2	/* show '*' if regular executable file */
#define DISP_HIDDEN	4	/* show files starting . (except . and ..) */
#define DISP_DOT	8	/* show . and .. */
#define DISP_NUMERIC	16	/* numeric uid and gid */
#define DISP_FULLTIME	32	/* show extended time display */
#define DIR_NOLIST	64	/* show directory as itself, not contents */
#define DISP_DIRNAME	128	/* show directory name (for internal use) */
#define DIR_RECURSE	256	/* -R (not yet implemented) */

static unsigned char	display_fmt = FMT_AUTO;
static unsigned short	opts = 0;
static unsigned short	column = 0;

#ifdef FEATURE_AUTOWIDTH
static unsigned short terminal_width = 0, column_width = 0;
#else
#define terminal_width	TERMINAL_WIDTH
#define column_width	COLUMN_WIDTH
#endif

#ifdef FEATURE_TIMESTAMPS
static unsigned char time_fmt = TIME_MOD;
#endif

#define wr(data,len) fwrite(data, 1, len, stdout)

static void writenum(long val, short minwidth)
{
	char	scratch[20];

	char *p = scratch + sizeof(scratch);
	short len = 0;
	short neg = (val < 0);
	
	if (neg) val = -val;
	do
		*--p = (val % 10) + '0', len++, val /= 10;
	while (val);
	if (neg)
		*--p = '-', len++;
	while (len < minwidth)
		*--p = ' ', len++;
	wr(p, len);
	column += len;
}

static void newline(void)
{
	if (column > 0) {
		wr("\n", 1);
		column = 0;
	}
}

static void tab(short col)
{
	static const char spaces[] = "                ";
	#define nspaces ((sizeof spaces)-1)	/* null terminator! */
	
	short n = col - column;

	if (n > 0) {
		column = col;
		while (n > nspaces) {
			wr(spaces, nspaces);
			n -= nspaces;
		}
		/* must be 1...(sizeof spaces) left */
		wr(spaces, n);
	}
	#undef nspaces
}

#ifdef FEATURE_FILETYPECHAR
static char append_char(umode_t mode)
{
	if (!(opts & DISP_FTYPE))
		return '\0';
	if ((opts & DISP_EXEC) && S_ISREG(mode) && (mode & (S_IXUSR|S_IXGRP|S_IXOTH)))
		return '*';
	return APPCHAR(mode);
}
#endif

/**
 **
 ** Display a file or directory as a single item
 ** (in either long or short format)
 **
 **/

static void list_single(const char *name, struct stat *info)
{
	char scratch[20];
	short len = strlen(name);
#ifdef FEATURE_FILETYPECHAR
	char append = append_char(info->st_mode);
#endif
	
	if (display_fmt == FMT_LONG) {
		umode_t mode = info->st_mode; 
		int i;
		
		scratch[0] = TYPECHAR(mode);
		for (i=0; i<9; i++)
			if (mode & SBIT[i])
				scratch[i+1] = (mode & MBIT[i])
						? SMODE1[i]
						: SMODE0[i];
			else
				scratch[i+1] = (mode & MBIT[i])
						? MODE1[i]
						: MODE0[i];
		newline();
		wr(scratch, 10);
		column=10;
		writenum((long)info->st_nlink,(short)4);
		fputs(" ", stdout);
#ifdef FEATURE_USERNAME
		if (!(opts & DISP_NUMERIC)) {
			struct passwd *pw = getpwuid(info->st_uid);
			if (pw)
				fputs(pw->pw_name, stdout);
			else
				writenum((long)info->st_uid,(short)0);
		} else
#endif
		writenum((long)info->st_uid,(short)0);
		tab(24);
#ifdef FEATURE_USERNAME
		if (!(opts & DISP_NUMERIC)) {
			struct group *gr = getgrgid(info->st_gid);
			if (gr)
				fputs(gr->gr_name, stdout);
			else
				writenum((long)info->st_gid,(short)0);
		} else
#endif
		writenum((long)info->st_gid,(short)0);
		tab(33);
		if (S_ISBLK(mode) || S_ISCHR(mode)) {
			writenum((long)MAJOR(info->st_rdev),(short)3);
			fputs(", ", stdout);
			writenum((long)MINOR(info->st_rdev),(short)3);
		}
		else
			writenum((long)info->st_size,(short)8);
		fputs(" ", stdout);
#ifdef FEATURE_TIMESTAMPS
		{
			time_t cal;
			char *string;
			
			switch(time_fmt) {
			case TIME_CHANGE:
				cal=info->st_ctime; break;
			case TIME_ACCESS:
				cal=info->st_atime; break;
			default:
				cal=info->st_mtime; break;
			}
			string=ctime(&cal);
			if (opts & DISP_FULLTIME)
				wr(string,24);
			else {
				time_t age = time(NULL) - cal;
				wr(string+4,7);	/* mmm_dd_ */
				if(age < 3600L*24*365/2 && age > -15*60)
					/* hh:mm if less than 6 months old */
					wr(string+11,5);
				else
					/* _yyyy otherwise */
					wr(string+19,5);
			}
			wr(" ", 1);
		}
#else
		fputs("--- -- ----- ", stdout);
#endif
		wr(name, len);
		if (S_ISLNK(mode)) {
			wr(" -> ", 4);
			len = readlink(name, scratch, sizeof scratch);
			if (len > 0) fwrite(scratch, 1, len, stdout);
#ifdef FEATURE_FILETYPECHAR
			/* show type of destination */
			if (opts & DISP_FTYPE) {
				if (!stat(name, info)) {
					append = append_char(info->st_mode);
					if (append)
						fputc(append, stdout);
				}
			}
#endif
		}
#ifdef FEATURE_FILETYPECHAR
		else if (append)
			wr(&append, 1);
#endif
	} else {
		static short nexttab = 0;
		
		/* sort out column alignment */
		if (column == 0)
			; /* nothing to do */
		else if (display_fmt == FMT_SINGLE)
			newline();
		else {
			if (nexttab + column_width > terminal_width
#ifndef FEATURE_AUTOWIDTH
			|| nexttab + len >= terminal_width
#endif
			)
				newline();
			else
				tab(nexttab);
		}
		/* work out where next column starts */
#ifdef FEATURE_AUTOWIDTH
		/* we know the calculated width is big enough */
		nexttab = column + column_width + COLUMN_GAP;
#else
		/* might cover more than one fixed-width column */
		nexttab = column;
		do
			nexttab += column_width + COLUMN_GAP;
		while (nexttab < (column + len + COLUMN_GAP));
#endif
		/* now write the data */
		wr(name, len);
		column = column + len;
#ifdef FEATURE_FILETYPECHAR
		if (append)
			wr(&append, 1), column++;
#endif
	}
}

/**
 **
 ** List the given file or directory, expanding a directory
 ** to show its contents if required
 **
 **/

static int list_item(const char *name)
{
	struct stat info;
	DIR *dir;
	struct dirent *entry;
	char fullname[MAXNAMLEN+1], *fnend;
	
	if (lstat(name, &info))
		goto listerr;
	
	if (!S_ISDIR(info.st_mode) || 
	    (opts & DIR_NOLIST)) {
		list_single(name, &info);
		return 0;
	}

	/* Otherwise, it's a directory we want to list the contents of */

	if (opts & DISP_DIRNAME) {   /* identify the directory */
		if (column)
			wr("\n\n", 2), column = 0;
		wr(name, strlen(name));
		wr(":\n", 2);
	}
	
	dir = opendir(name);
	if (!dir) goto listerr;
#ifdef FEATURE_AUTOWIDTH
	column_width = 0;
	while ((entry = readdir(dir)) != NULL) {
		short w = strlen(entry->d_name);
		if (column_width < w)
			column_width = w;
	}
#ifdef HAS_REWINDDIR
	rewinddir(dir);
#else
	closedir(dir);
	dir = opendir(name);
	if (!dir) goto listerr;
#endif
#endif

	/* List the contents */
	
	strcpy(fullname,name);	/* *** ignore '.' by itself */
	fnend=fullname+strlen(fullname);
	if (fnend[-1] != '/')
		*fnend++ = '/';
	
	while ((entry = readdir(dir)) != NULL) {
		const char *en=entry->d_name;
		if (en[0] == '.') {
			if (!en[1] || (en[1] == '.' && !en[2])) { /* . or .. */
				if (!(opts & DISP_DOT))
					continue;
			}
			else if (!(opts & DISP_HIDDEN))
				continue;
		}
		/* FIXME: avoid stat if not required */
		strcpy(fnend, entry->d_name);
		if (lstat(fullname, &info))
			goto direrr; /* (shouldn't fail) */
		list_single(entry->d_name, &info);
	}
	closedir(dir);
	return 0;

direrr:
	closedir(dir);	
listerr:
	newline();
	name_and_error(name);
	return 1;
}

const char	ls_usage[] = "Usage: ls [-1a"
#ifdef FEATURE_TIMESTAMPS
	"c"
#endif
	"d"
#ifdef FEATURE_TIMESTAMPS
	"e"
#endif
	"ln"
#ifdef FEATURE_FILETYPECHAR
	"p"
#endif
#ifdef FEATURE_TIMESTAMPS
	"u"
#endif
	"xAC"
#ifdef FEATURE_FILETYPECHAR
	"F"
#endif
#ifdef FEATURE_RECURSIVE
	"R"
#endif
	"] [filenames...]\n";

extern int
ls_main(struct FileInfo * not_used, int argc, char * * argv)
{
	int argi=1, i;
	
	/* process options */
	while (argi < argc && argv[argi][0] == '-') {
		const char *p = &argv[argi][1];
		
		if (!*p) goto print_usage_message;	/* "-" by itself not allowed */
		if (*p == '-') {
			if (!p[1]) {	/* "--" forces end of options */
				argi++;
				break;
			}
			/* it's a long option name - we don't support them */
			goto print_usage_message;
		}
		
		while (*p)
			switch (*p++) {
			case 'l':	display_fmt = FMT_LONG; break;
			case '1':	display_fmt = FMT_SINGLE; break;
			case 'x':	display_fmt = FMT_ROWS; break;
			case 'C':	display_fmt = FMT_COLUMNS; break;
#ifdef FEATURE_FILETYPECHAR
			case 'p':	opts |= DISP_FTYPE; break;
			case 'F':	opts |= DISP_FTYPE|DISP_EXEC; break;
#endif
			case 'A':	opts |= DISP_HIDDEN; break;
			case 'a':	opts |= DISP_HIDDEN|DISP_DOT; break;
			case 'n':	opts |= DISP_NUMERIC; break;
			case 'd':	opts |= DIR_NOLIST; break;
#ifdef FEATURE_RECURSIVE
			case 'R':	opts |= DIR_RECURSE; break;
#endif
#ifdef FEATURE_TIMESTAMPS
			case 'u':	time_fmt = TIME_ACCESS; break;
			case 'c':	time_fmt = TIME_CHANGE; break;
			case 'e':	opts |= DISP_FULLTIME; break;
#endif
			default:	goto print_usage_message;
			}
		
		argi++;
	}

	/* choose a display format */
	if (display_fmt == FMT_AUTO)
		display_fmt = isatty(STDOUT_FILENO) ? FMT_COLUMNS : FMT_SINGLE;
	if (argi < argc - 1)
		opts |= DISP_DIRNAME; /* 2 or more items? label directories */
#ifdef FEATURE_AUTOWIDTH
	/* could add a -w option and/or TIOCGWINSZ call */
	if (terminal_width < 1) terminal_width = TERMINAL_WIDTH;
	
	for (i = argi; i < argc; i++) {
		int len = strlen(argv[i]);
		if (column_width < len)
			column_width = len;
	}
#endif

	/* process files specified, or current directory if none */
	i=0;
	if (argi == argc)
		i = list_item(".");
	while (argi < argc)
		i |= list_item(argv[argi++]);
	newline();
	return i;

print_usage_message:
	usage(ls_usage);
	return 1;
}
