/* vi: set sw=4 ts=4: */
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
 * (i.e., the ones I couldn't live without :-) All features which involve
 * linking in substantial chunks of libc can be disabled.
 *
 * Although I don't really want to add new features to this program to
 * keep it small, I *am* interested to receive bug fixes and ways to make
 * it more portable.
 *
 * KNOWN BUGS:
 * 1. ls -l of a directory doesn't give "total <blocks>" header
 * 2. ls of a symlink to a directory doesn't list directory contents
 * 3. hidden files can make column width too large
 *
 * NON-OPTIMAL BEHAVIOUR:
 * 1. autowidth reads directories twice
 * 2. if you do a short directory listing without filetype characters
 *    appended, there's no need to stat each one
 * PORTABILITY:
 * 1. requires lstat (BSD) - how do you do it without?
 */

enum {
	TERMINAL_WIDTH = 80,	/* use 79 if terminal has linefold bug */
	COLUMN_GAP = 2,		/* includes the file type char */
};

/************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "busybox.h"
#ifdef CONFIG_SELINUX
#include <fs_secure.h>
#include <flask_util.h>
#include <ss.h>
#endif

#ifdef CONFIG_FEATURE_LS_TIMESTAMPS
#include <time.h>
#endif

#ifndef MAJOR
#define MAJOR(dev) (((dev)>>8)&0xff)
#define MINOR(dev) ((dev)&0xff)
#endif

/* what is the overall style of the listing */
#define STYLE_AUTO      (0)
#define STYLE_COLUMNS   (1U<<21)	/* fill columns */
#define STYLE_LONG      (2U<<21)	/* one record per line, extended info */
#define STYLE_SINGLE    (3U<<21)	/* one record per line */

#define STYLE_MASK                 STYLE_SINGLE
#define STYLE_ONE_RECORD_FLAG      STYLE_LONG

/* 51306 lrwxrwxrwx  1 root     root         2 May 11 01:43 /bin/view -> vi* */
/* what file information will be listed */
#define LIST_INO		(1U<<0)
#define LIST_BLOCKS		(1U<<1)
#define LIST_MODEBITS	(1U<<2)
#define LIST_NLINKS		(1U<<3)
#define LIST_ID_NAME	(1U<<4)
#define LIST_ID_NUMERIC	(1U<<5)
#define LIST_CONTEXT	(1U<<6)
#define LIST_SIZE		(1U<<7)
#define LIST_DEV		(1U<<8)
#define LIST_DATE_TIME	(1U<<9)
#define LIST_FULLTIME	(1U<<10)
#define LIST_FILENAME	(1U<<11)
#define LIST_SYMLINK	(1U<<12)
#define LIST_FILETYPE	(1U<<13)
#define LIST_EXEC	(1U<<14)

#define LIST_MASK       ((LIST_EXEC << 1) - 1)

/* what files will be displayed */
/* TODO -- We may be able to make DISP_NORMAL 0 to save a bit slot. */
#define DISP_NORMAL		(1U<<14)	/* show normal filenames */
#define DISP_DIRNAME	(1U<<15)	/* 2 or more items? label directories */
#define DISP_HIDDEN		(1U<<16)	/* show filenames starting with .  */
#define DISP_DOT		(1U<<17)	/* show . and .. */
#define DISP_NOLIST		(1U<<18)	/* show directory as itself, not contents */
#define DISP_RECURSIVE	(1U<<19)	/* show directory and everything below it */
#define DISP_ROWS		(1U<<20)	/* print across rows */

#define DISP_MASK       (((DISP_ROWS << 1) - 1) & ~(DISP_NORMAL - 1))

#ifdef CONFIG_FEATURE_LS_SORTFILES
/* how will the files be sorted */
#define SORT_ORDER_FORWARD   0			/* sort in reverse order */
#define SORT_ORDER_REVERSE   (1U<<27)	/* sort in reverse order */

#define SORT_NAME      0			/* sort by file name */
#define SORT_SIZE      (1U<<28)		/* sort by file size */
#define SORT_ATIME     (2U<<28)		/* sort by last access time */
#define SORT_CTIME     (3U<<28)		/* sort by last change time */
#define SORT_MTIME     (4U<<28)		/* sort by last modification time */
#define SORT_VERSION   (5U<<28)		/* sort by version */
#define SORT_EXT       (6U<<28)		/* sort by file name extension */
#define SORT_DIR       (7U<<28)		/* sort by file or directory */

#define SORT_MASK      (7U<<28)
#endif

#ifdef CONFIG_FEATURE_LS_TIMESTAMPS
/* which of the three times will be used */
#define TIME_MOD       0
#define TIME_CHANGE    (1U<<23)
#define TIME_ACCESS    (1U<<24)

#define TIME_MASK      (3U<<23)
#endif

#ifdef CONFIG_FEATURE_LS_FOLLOWLINKS
#define FOLLOW_LINKS   (1U<<25)
#endif
#ifdef CONFIG_FEATURE_HUMAN_READABLE
#define LS_DISP_HR     (1U<<26)
#endif

#define LIST_SHORT	(LIST_FILENAME)
#define LIST_ISHORT	(LIST_INO | LIST_FILENAME)
#define LIST_LONG	(LIST_MODEBITS | LIST_NLINKS | LIST_ID_NAME | LIST_SIZE | \
						LIST_DATE_TIME | LIST_FILENAME | LIST_SYMLINK)
#define LIST_ILONG	(LIST_INO | LIST_LONG)

#define SPLIT_DIR      1
#define SPLIT_FILE     0
#define SPLIT_SUBDIR   2

#define TYPEINDEX(mode) (((mode) >> 12) & 0x0f)
#define TYPECHAR(mode)  ("0pcCd?bB-?l?s???" [TYPEINDEX(mode)])

#if defined(CONFIG_FEATURE_LS_FILETYPES) || defined(CONFIG_FEATURE_LS_COLOR)
# define APPCHAR(mode)   ("\0|\0\0/\0\0\0\0\0@\0=\0\0\0" [TYPEINDEX(mode)])
#endif

/* colored LS support by JaWi, janwillem.janssen@lxtreme.nl */
#ifdef CONFIG_FEATURE_LS_COLOR
static int show_color = 0;

#define COLOR(mode)	("\000\043\043\043\042\000\043\043"\
					"\000\000\044\000\043\000\000\040" [TYPEINDEX(mode)])
#define ATTR(mode)	("\00\00\01\00\01\00\01\00"\
					"\00\00\01\00\01\00\00\01" [TYPEINDEX(mode)])
#endif

/*
 * a directory entry and its stat info are stored here
 */
struct dnode {			/* the basic node */
	char *name;			/* the dir entry name */
	char *fullname;		/* the dir entry name */
	struct stat dstat;	/* the file stat info */
#ifdef CONFIG_SELINUX
	security_id_t sid;
#endif
	struct dnode *next;	/* point at the next node */
};
typedef struct dnode dnode_t;

static struct dnode **list_dir(const char *);
static struct dnode **dnalloc(int);
static int list_single(struct dnode *);

static unsigned int all_fmt;

#ifdef CONFIG_SELINUX
static int is_flask_enabled_flag;
#endif

#ifdef CONFIG_FEATURE_AUTOWIDTH
static int terminal_width = TERMINAL_WIDTH;
static unsigned short tabstops = COLUMN_GAP;
#else
#define tabstops COLUMN_GAP
#define terminal_width TERMINAL_WIDTH
#endif

static int status = EXIT_SUCCESS;

static struct dnode *my_stat(char *fullname, char *name)
{
	struct stat dstat;
	struct dnode *cur;
#ifdef CONFIG_SELINUX
	security_id_t sid;
#endif
	int rc;

#ifdef CONFIG_FEATURE_LS_FOLLOWLINKS
	if (all_fmt & FOLLOW_LINKS) {
#ifdef CONFIG_SELINUX
		if(is_flask_enabled_flag)
			rc = stat_secure(fullname, &dstat, &sid);
		else
#endif
			rc = stat(fullname, &dstat);
		if(rc)
		{
			bb_perror_msg("%s", fullname);
			status = EXIT_FAILURE;
			return 0;
		}
	} else
#endif
	{
#ifdef CONFIG_SELINUX
		if(is_flask_enabled_flag)
			rc = lstat_secure(fullname, &dstat, &sid);
		else
#endif
			rc = lstat(fullname, &dstat);
		if(rc)
		{
			bb_perror_msg("%s", fullname);
			status = EXIT_FAILURE;
			return 0;
		}
	}

	cur = (struct dnode *) xmalloc(sizeof(struct dnode));
	cur->fullname = fullname;
	cur->name = name;
	cur->dstat = dstat;
#ifdef CONFIG_SELINUX
	cur->sid = sid;
#endif
	return cur;
}

/*----------------------------------------------------------------------*/
#ifdef CONFIG_FEATURE_LS_COLOR
static char fgcolor(mode_t mode)
{
	/* Check wheter the file is existing (if so, color it red!) */
	if (errno == ENOENT) {
		return '\037';
	}
	if (LIST_EXEC && S_ISREG(mode)
		&& (mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
		return COLOR(0xF000);	/* File is executable ... */
	return COLOR(mode);
}

/*----------------------------------------------------------------------*/
static char bgcolor(mode_t mode)
{
	if (LIST_EXEC && S_ISREG(mode)
		&& (mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
		return ATTR(0xF000);	/* File is executable ... */
	return ATTR(mode);
}
#endif

/*----------------------------------------------------------------------*/
#if defined(CONFIG_FEATURE_LS_FILETYPES) || defined(CONFIG_FEATURE_LS_COLOR)
static char append_char(mode_t mode)
{
	if (!(all_fmt & LIST_FILETYPE))
		return '\0';
	if ((all_fmt & LIST_EXEC) && S_ISREG(mode)
		&& (mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
		return '*';
	return APPCHAR(mode);
}
#endif

/*----------------------------------------------------------------------*/

#define countdirs(A,B) count_dirs((A), (B), 1)
#define countsubdirs(A,B) count_dirs((A), (B), 0)

static int count_dirs(struct dnode **dn, int nfiles, int notsubdirs)
{
	int i, dirs;

	if (dn == NULL || nfiles < 1)
		return (0);
	dirs = 0;
	for (i = 0; i < nfiles; i++) {
		if (S_ISDIR(dn[i]->dstat.st_mode)
			&& (notsubdirs
				|| ((dn[i]->name[0] != '.')
					|| (dn[i]->name[1]
						&& ((dn[i]->name[1] != '.')
							|| dn[i]->name[2])))))
			dirs++;
	}
	return (dirs);
}

static int countfiles(struct dnode **dnp)
{
	int nfiles;
	struct dnode *cur;

	if (dnp == NULL)
		return (0);
	nfiles = 0;
	for (cur = dnp[0]; cur->next != NULL; cur = cur->next)
		nfiles++;
	nfiles++;
	return (nfiles);
}

/* get memory to hold an array of pointers */
static struct dnode **dnalloc(int num)
{
	struct dnode **p;

	if (num < 1)
		return (NULL);

	p = (struct dnode **) xcalloc((size_t) num,
								  (size_t) (sizeof(struct dnode *)));
	return (p);
}

#ifdef CONFIG_FEATURE_LS_RECURSIVE
static void dfree(struct dnode **dnp)
{
	struct dnode *cur, *next;

	if (dnp == NULL)
		return;

	cur = dnp[0];
	while (cur != NULL) {
		free(cur->fullname);	/* free the filename */
		next = cur->next;
		free(cur);		/* free the dnode */
		cur = next;
	}
	free(dnp);			/* free the array holding the dnode pointers */
}
#endif

static struct dnode **splitdnarray(struct dnode **dn, int nfiles, int which)
{
	int dncnt, i, d;
	struct dnode **dnp;

	if (dn == NULL || nfiles < 1)
		return (NULL);

	/* count how many dirs and regular files there are */
	if (which == SPLIT_SUBDIR)
		dncnt = countsubdirs(dn, nfiles);
	else {
		dncnt = countdirs(dn, nfiles);	/* assume we are looking for dirs */
		if (which == SPLIT_FILE)
			dncnt = nfiles - dncnt;	/* looking for files */
	}

	/* allocate a file array and a dir array */
	dnp = dnalloc(dncnt);

	/* copy the entrys into the file or dir array */
	for (d = i = 0; i < nfiles; i++) {
		if (S_ISDIR(dn[i]->dstat.st_mode)) {
			if (which & (SPLIT_DIR|SPLIT_SUBDIR)) {
				if ((which & SPLIT_DIR)
					|| ((dn[i]->name[0] != '.')
						|| (dn[i]->name[1]
							&& ((dn[i]->name[1] != '.')
								|| dn[i]->name[2])))) {
									dnp[d++] = dn[i];
								}
			}
		} else if (!(which & (SPLIT_DIR|SPLIT_SUBDIR))) {
			dnp[d++] = dn[i];
		}
	}
	return (dnp);
}

/*----------------------------------------------------------------------*/
#ifdef CONFIG_FEATURE_LS_SORTFILES
static int sortcmp(struct dnode *d1, struct dnode *d2)
{
	unsigned int sort_opts = all_fmt & SORT_MASK;
	int dif;

	dif = 0;			/* assume SORT_NAME */
	if (sort_opts == SORT_SIZE) {
		dif = (int) (d2->dstat.st_size - d1->dstat.st_size);
	} else if (sort_opts == SORT_ATIME) {
		dif = (int) (d2->dstat.st_atime - d1->dstat.st_atime);
	} else if (sort_opts == SORT_CTIME) {
		dif = (int) (d2->dstat.st_ctime - d1->dstat.st_ctime);
	} else if (sort_opts == SORT_MTIME) {
		dif = (int) (d2->dstat.st_mtime - d1->dstat.st_mtime);
	} else if (sort_opts == SORT_DIR) {
		dif = S_ISDIR(d2->dstat.st_mode) - S_ISDIR(d1->dstat.st_mode);
		/* } else if (sort_opts == SORT_VERSION) { */
		/* } else if (sort_opts == SORT_EXT) { */
	}

	if (dif == 0) {
		/* sort by name- may be a tie_breaker for time or size cmp */
#ifdef CONFIG_LOCALE_SUPPORT
		dif = strcoll(d1->name, d2->name);
#else
		dif = strcmp(d1->name, d2->name);
#endif
	}

	if (all_fmt & SORT_ORDER_REVERSE) {
		dif = -dif;
	}
	return (dif);
}

/*----------------------------------------------------------------------*/
static void shellsort(struct dnode **dn, int size)
{
	struct dnode *temp;
	int gap, i, j;

	/* shell short the array */
	if (dn == NULL || size < 2)
		return;

	for (gap = size / 2; gap > 0; gap /= 2) {
		for (i = gap; i < size; i++) {
			for (j = i - gap; j >= 0; j -= gap) {
				if (sortcmp(dn[j], dn[j + gap]) <= 0)
					break;
				/* they are out of order, swap them */
				temp = dn[j];
				dn[j] = dn[j + gap];
				dn[j + gap] = temp;
			}
		}
	}
}
#endif

/*----------------------------------------------------------------------*/
static void showfiles(struct dnode **dn, int nfiles)
{
	int i, ncols, nrows, row, nc;
	int column = 0;
	int nexttab = 0;
	int column_width = 0; /* for STYLE_LONG and STYLE_SINGLE not used */

	if (dn == NULL || nfiles < 1)
		return;

	if (all_fmt & STYLE_ONE_RECORD_FLAG) {
		ncols = 1;
	} else {
		/* find the longest file name-  use that as the column width */
		for (i = 0; i < nfiles; i++) {
			int len = strlen(dn[i]->name) +
#ifdef CONFIG_SELINUX
			((all_fmt & LIST_CONTEXT) ? 33 : 0) +
#endif
			((all_fmt & LIST_INO) ? 8 : 0) +
			((all_fmt & LIST_BLOCKS) ? 5 : 0);
			if (column_width < len)
				column_width = len;
		}
		column_width += tabstops;
		ncols = (int) (terminal_width / column_width);
	}

	if (ncols > 1) {
		nrows = nfiles / ncols;
		if ((nrows * ncols) < nfiles)
			nrows++;                /* round up fractionals */
	} else {
		nrows = nfiles;
		ncols = 1;
	}

	for (row = 0; row < nrows; row++) {
		for (nc = 0; nc < ncols; nc++) {
			/* reach into the array based on the column and row */
			i = (nc * nrows) + row;	/* assume display by column */
			if (all_fmt & DISP_ROWS)
				i = (row * ncols) + nc;	/* display across row */
			if (i < nfiles) {
				if (column > 0) {
					nexttab -= column;
					while (nexttab--) {
						putchar(' ');
						column++;
					}
			}
				nexttab = column + column_width;
				column += list_single(dn[i]);
		}
		}
		putchar('\n');
		column = 0;
	}
}

/*----------------------------------------------------------------------*/
static void showdirs(struct dnode **dn, int ndirs, int first)
{
	int i, nfiles;
	struct dnode **subdnp;

#ifdef CONFIG_FEATURE_LS_RECURSIVE
	int dndirs;
	struct dnode **dnd;
#endif

	if (dn == NULL || ndirs < 1)
		return;

	for (i = 0; i < ndirs; i++) {
		if (all_fmt & (DISP_DIRNAME | DISP_RECURSIVE)) {
			if (!first)
				printf("\n");
			first = 0;
			printf("%s:\n", dn[i]->fullname);
		}
		subdnp = list_dir(dn[i]->fullname);
		nfiles = countfiles(subdnp);
		if (nfiles > 0) {
			/* list all files at this level */
#ifdef CONFIG_FEATURE_LS_SORTFILES
			shellsort(subdnp, nfiles);
#endif
			showfiles(subdnp, nfiles);
#ifdef CONFIG_FEATURE_LS_RECURSIVE
			if (all_fmt & DISP_RECURSIVE) {
				/* recursive- list the sub-dirs */
				dnd = splitdnarray(subdnp, nfiles, SPLIT_SUBDIR);
				dndirs = countsubdirs(subdnp, nfiles);
				if (dndirs > 0) {
#ifdef CONFIG_FEATURE_LS_SORTFILES
					shellsort(dnd, dndirs);
#endif
					showdirs(dnd, dndirs, 0);
					free(dnd);	/* free the array of dnode pointers to the dirs */
				}
			}
			dfree(subdnp);	/* free the dnodes and the fullname mem */
#endif
		}
	}
}

/*----------------------------------------------------------------------*/
static struct dnode **list_dir(const char *path)
{
	struct dnode *dn, *cur, **dnp;
	struct dirent *entry;
	DIR *dir;
	int i, nfiles;

	if (path == NULL)
		return (NULL);

	dn = NULL;
	nfiles = 0;
	dir = opendir(path);
	if (dir == NULL) {
		bb_perror_msg("%s", path);
		status = EXIT_FAILURE;
		return (NULL);	/* could not open the dir */
	}
	while ((entry = readdir(dir)) != NULL) {
		char *fullname;

		/* are we going to list the file- it may be . or .. or a hidden file */
		if (entry->d_name[0] == '.') {
			if ((entry->d_name[1] == 0 || (
				entry->d_name[1] == '.'
				&& entry->d_name[2] == 0))
					&& !(all_fmt & DISP_DOT))
			continue;
			if (!(all_fmt & DISP_HIDDEN))
			continue;
		}
		fullname = concat_path_file(path, entry->d_name);
		cur = my_stat(fullname, strrchr(fullname, '/') + 1);
		if (!cur)
			continue;
		cur->next = dn;
		dn = cur;
		nfiles++;
	}
	closedir(dir);

	/* now that we know how many files there are
	   ** allocate memory for an array to hold dnode pointers
	 */
	if (dn == NULL)
		return (NULL);
	dnp = dnalloc(nfiles);
	for (i = 0, cur = dn; i < nfiles; i++) {
		dnp[i] = cur;	/* save pointer to node in array */
		cur = cur->next;
	}

	return (dnp);
}

/*----------------------------------------------------------------------*/
static int list_single(struct dnode *dn)
{
	int i, column = 0;

#ifdef CONFIG_FEATURE_LS_USERNAME
	char scratch[16];
#endif
#ifdef CONFIG_FEATURE_LS_TIMESTAMPS
	char *filetime;
	time_t ttime, age;
#endif
#if defined(CONFIG_FEATURE_LS_FILETYPES) || defined (CONFIG_FEATURE_LS_COLOR)
	struct stat info;
	char append;
#endif

	if (dn->fullname == NULL)
		return (0);

#ifdef CONFIG_FEATURE_LS_TIMESTAMPS
	ttime = dn->dstat.st_mtime;	/* the default time */
	if (all_fmt & TIME_ACCESS)
		ttime = dn->dstat.st_atime;
	if (all_fmt & TIME_CHANGE)
		ttime = dn->dstat.st_ctime;
	filetime = ctime(&ttime);
#endif
#ifdef CONFIG_FEATURE_LS_FILETYPES
	append = append_char(dn->dstat.st_mode);
#endif

	for (i = 0; i <= 31; i++) {
		switch (all_fmt & (1 << i)) {
		case LIST_INO:
			column += printf("%7ld ", (long int) dn->dstat.st_ino);
			break;
		case LIST_BLOCKS:
#if _FILE_OFFSET_BITS == 64
			column += printf("%4lld ", dn->dstat.st_blocks >> 1);
#else
			column += printf("%4ld ", dn->dstat.st_blocks >> 1);
#endif
			break;
		case LIST_MODEBITS:
			column += printf("%-10s ", (char *) bb_mode_string(dn->dstat.st_mode));
			break;
		case LIST_NLINKS:
			column += printf("%4ld ", (long) dn->dstat.st_nlink);
			break;
		case LIST_ID_NAME:
#ifdef CONFIG_FEATURE_LS_USERNAME
			my_getpwuid(scratch, dn->dstat.st_uid);
			printf("%-8.8s ", scratch);
			my_getgrgid(scratch, dn->dstat.st_gid);
			printf("%-8.8s", scratch);
			column += 17;
			break;
#endif
		case LIST_ID_NUMERIC:
			column += printf("%-8d %-8d", dn->dstat.st_uid, dn->dstat.st_gid);
			break;
		case LIST_SIZE:
		case LIST_DEV:
			if (S_ISBLK(dn->dstat.st_mode) || S_ISCHR(dn->dstat.st_mode)) {
				column += printf("%4d, %3d ", (int) MAJOR(dn->dstat.st_rdev),
					   (int) MINOR(dn->dstat.st_rdev));
			} else {
#ifdef CONFIG_FEATURE_HUMAN_READABLE
				if (all_fmt & LS_DISP_HR) {
					column += printf("%9s ",
							make_human_readable_str(dn->dstat.st_size, 1, 0));
				} else
#endif
				{
#if _FILE_OFFSET_BITS == 64
					column += printf("%9lld ", (long long) dn->dstat.st_size);
#else
					column += printf("%9ld ", dn->dstat.st_size);
#endif
				}
			}
			break;
#ifdef CONFIG_FEATURE_LS_TIMESTAMPS
		case LIST_FULLTIME:
			printf("%24.24s ", filetime);
			column += 25;
			break;
		case LIST_DATE_TIME:
			if ((all_fmt & LIST_FULLTIME) == 0) {
				age = time(NULL) - ttime;
				printf("%6.6s ", filetime + 4);
				if (age < 3600L * 24 * 365 / 2 && age > -15 * 60) {
					/* hh:mm if less than 6 months old */
					printf("%5.5s ", filetime + 11);
				} else {
					printf(" %4.4s ", filetime + 20);
				}
				column += 13;
			}
			break;
#endif
#ifdef CONFIG_SELINUX
		case LIST_CONTEXT:
			{
				char context[64];
				int len = sizeof(context);
				if(security_sid_to_context(dn->sid, context, &len))
				{
					strcpy(context, "unknown");
					len = 7;
				}
				printf("%-32s ", context);
				column += MAX(33, len);
			}
			break;
#endif
		case LIST_FILENAME:
#ifdef CONFIG_FEATURE_LS_COLOR
			errno = 0;
			if (show_color && !lstat(dn->fullname, &info)) {
				printf("\033[%d;%dm", bgcolor(info.st_mode),
					   fgcolor(info.st_mode));
			}
#endif
			column += printf("%s", dn->name);
#ifdef CONFIG_FEATURE_LS_COLOR
			if (show_color) {
				printf("\033[0m");
			}
#endif
			break;
		case LIST_SYMLINK:
			if (S_ISLNK(dn->dstat.st_mode)) {
				char *lpath = xreadlink(dn->fullname);

				if (lpath) {
					printf(" -> ");
#if defined(CONFIG_FEATURE_LS_FILETYPES) || defined (CONFIG_FEATURE_LS_COLOR)
					if (!stat(dn->fullname, &info)) {
						append = append_char(info.st_mode);
					}
#endif
#ifdef CONFIG_FEATURE_LS_COLOR
					if (show_color) {
						errno = 0;
						printf("\033[%d;%dm", bgcolor(info.st_mode),
							   fgcolor(info.st_mode));
					}
#endif
					column += printf("%s", lpath) + 4;
#ifdef CONFIG_FEATURE_LS_COLOR
					if (show_color) {
						printf("\033[0m");
					}
#endif
					free(lpath);
				}
			}
			break;
#ifdef CONFIG_FEATURE_LS_FILETYPES
		case LIST_FILETYPE:
			if (append != '\0') {
				printf("%1c", append);
				column++;
			}
			break;
#endif
		}
	}

	return column;
}

/*----------------------------------------------------------------------*/

/* "[-]Cadil1", POSIX mandated options, busybox always supports */
/* "[-]gnsx", POSIX non-mandated options, busybox always supports */
/* "[-]Ak" GNU options, busybox always supports */
/* "[-]FLRctur", POSIX mandated options, busybox optionally supports */
/* "[-]p", POSIX non-mandated options, busybox optionally supports */
/* "[-]SXvThw", GNU options, busybox optionally supports */
/* "[-]K", SELinux mandated options, busybox optionally supports */
/* "[-]e", I think we made this one up */

#ifdef CONFIG_FEATURE_LS_TIMESTAMPS
# define LS_STR_TIMESTAMPS	"cetu"
#else
# define LS_STR_TIMESTAMPS	""
#endif

#ifdef CONFIG_FEATURE_LS_SORTFILES
# define LS_STR_SORTFILES	"SXrv"
#else
# define LS_STR_SORTFILES	""
#endif

#ifdef CONFIG_FEATURE_LS_FILETYPES
# define LS_STR_FILETYPES	"Fp"
#else
# define LS_STR_FILETYPES	""
#endif

#ifdef CONFIG_FEATURE_LS_FOLLOWLINKS
# define LS_STR_FOLLOW_LINKS	"L"
#else
# define LS_STR_FOLLOW_LINKS	""
#endif

#ifdef CONFIG_FEATURE_LS_RECURSIVE
# define LS_STR_RECURSIVE	"R"
#else
# define LS_STR_RECURSIVE	""
#endif

#ifdef CONFIG_FEATURE_HUMAN_READABLE
# define LS_STR_HUMAN_READABLE	"h"
#else
# define LS_STR_HUMAN_READABLE	""
#endif

#ifdef CONFIG_SELINUX
# define LS_STR_SELINUX	"K"
#else
# define LS_STR_SELINUX	""
#endif

#ifdef CONFIG_FEATURE_AUTOWIDTH
# define LS_STR_AUTOWIDTH	"T:w:"
#else
# define LS_STR_AUTOWIDTH	""
#endif

static const char ls_options[]="Cadil1gnsxAk" \
	LS_STR_TIMESTAMPS \
	LS_STR_SORTFILES \
	LS_STR_FILETYPES \
	LS_STR_FOLLOW_LINKS \
	LS_STR_RECURSIVE \
	LS_STR_HUMAN_READABLE \
	LS_STR_SELINUX \
	LS_STR_AUTOWIDTH;

#define LIST_MASK_TRIGGER	LIST_SHORT
#define STYLE_MASK_TRIGGER	STYLE_MASK
#define SORT_MASK_TRIGGER	SORT_MASK
#define DISP_MASK_TRIGGER	DISP_ROWS
#define TIME_MASK_TRIGGER	TIME_MASK

static const unsigned opt_flags[] = {
	LIST_SHORT | STYLE_COLUMNS,	/* C */
	DISP_HIDDEN | DISP_DOT,    	/* a */
	DISP_NOLIST,               	/* d */
	LIST_INO,                  	/* i */
	LIST_LONG | STYLE_LONG,    	/* l - remember LS_DISP_HR in mask! */
	LIST_SHORT | STYLE_SINGLE, 	/* 1 */
	0,                         	/* g - ingored */
	LIST_ID_NUMERIC,           	/* n */
	LIST_BLOCKS,              	/* s */
	DISP_ROWS,                	/* x */
	DISP_HIDDEN,              	/* A */
#ifdef CONFIG_SELINUX
	LIST_CONTEXT,             	/* k */
#else
	0,                        	/* k - ingored */
#endif
#ifdef CONFIG_FEATURE_LS_TIMESTAMPS
# ifdef CONFIG_FEATURE_LS_SORTFILES
	TIME_CHANGE | SORT_CTIME,	/* c */
# else
	TIME_CHANGE,             	/* c */
# endif
	LIST_FULLTIME,           	/* e */
# ifdef CONFIG_FEATURE_LS_SORTFILES
	SORT_MTIME,              	/* t */
# else
	0,                      	/* t - ignored -- is this correct? */
# endif
# ifdef CONFIG_FEATURE_LS_SORTFILES
	TIME_ACCESS | SORT_ATIME,	/* u */
# else
	TIME_ACCESS,             	/* u */
# endif
#endif
#ifdef CONFIG_FEATURE_LS_SORTFILES
	SORT_SIZE,               	/* S */
	SORT_EXT,                	/* X */
	SORT_ORDER_REVERSE,       	/* r */
	SORT_VERSION,             	/* v */
#endif
#ifdef CONFIG_FEATURE_LS_FILETYPES
	LIST_FILETYPE | LIST_EXEC,	/* F */
	LIST_FILETYPE,            	/* p */
#endif
#ifdef CONFIG_FEATURE_LS_FOLLOWLINKS
	FOLLOW_LINKS,             	/* L */
#endif
#ifdef CONFIG_FEATURE_LS_RECURSIVE
	DISP_RECURSIVE,           	/* R */
#endif
#ifdef CONFIG_FEATURE_HUMAN_READABLE
	LS_DISP_HR,               	/* h */
#endif
#ifdef CONFIG_SELINUX
	LIST_MODEBITS|LIST_NLINKS|LIST_CONTEXT|LIST_SIZE|LIST_DATE_TIME, /* K */
#endif
	(1U<<31)
};


/*----------------------------------------------------------------------*/

extern int ls_main(int argc, char **argv)
{
	struct dnode **dnd;
	struct dnode **dnf;
	struct dnode **dnp;
	struct dnode *dn;
	struct dnode *cur;
	long opt;
	int nfiles = 0;
	int dnfiles;
	int dndirs;
	int oi;
	int ac;
	int i;
	char **av;
#ifdef CONFIG_FEATURE_AUTOWIDTH
	char *tabstops_str = NULL;
	char *terminal_width_str = NULL;
#endif

#ifdef CONFIG_SELINUX
	is_flask_enabled_flag = is_flask_enabled();
#endif

	all_fmt = LIST_SHORT | DISP_NORMAL | STYLE_AUTO
#ifdef CONFIG_FEATURE_LS_TIMESTAMPS
		| TIME_MOD
#endif
#ifdef CONFIG_FEATURE_LS_SORTFILES
		| SORT_NAME | SORT_ORDER_FORWARD
#endif
		;

#ifdef CONFIG_FEATURE_AUTOWIDTH
	/* Obtain the terminal width.  */
	get_terminal_width_height(STDOUT_FILENO, &terminal_width, NULL);
	/* Go one less... */
	terminal_width--;
#endif

#ifdef CONFIG_FEATURE_LS_COLOR
	if (isatty(STDOUT_FILENO))
		show_color = 1;
#endif

	/* process options */
#ifdef CONFIG_FEATURE_AUTOWIDTH
	opt = bb_getopt_ulflags(argc, argv, ls_options, &tabstops_str, &terminal_width_str);
	if (tabstops_str) {
		tabstops = atoi(tabstops_str);
	}
	if (terminal_width_str) {
		terminal_width = atoi(terminal_width_str);
	}
#else
	opt = bb_getopt_ulflags(argc, argv, ls_options);
#endif
	for (i = 0; opt_flags[i] != (1U<<31); i++) {
		if (opt & (1 << i)) {
			unsigned int flags = opt_flags[i];
			if (flags & LIST_MASK_TRIGGER) {
				all_fmt &= ~LIST_MASK;
			}
			if (flags & STYLE_MASK_TRIGGER) {
				all_fmt &= ~STYLE_MASK;
			}
#ifdef CONFIG_FEATURE_LS_SORTFILES
			if (flags & SORT_MASK_TRIGGER) {
				all_fmt &= ~SORT_MASK;
			}
#endif
			if (flags & DISP_MASK_TRIGGER) {
				all_fmt &= ~DISP_MASK;
			}
#ifdef CONFIG_FEATURE_LS_TIMESTAMPS
			if (flags & TIME_MASK_TRIGGER) {
				all_fmt &= ~TIME_MASK;
			}
#endif
			if (flags & LIST_CONTEXT) {
				all_fmt |= STYLE_SINGLE;
			}
#ifdef CONFIG_FEATURE_HUMAN_READABLE
			if (opt == 'l') {
				all_fmt &= ~LS_DISP_HR;
			}
#endif
			all_fmt |= flags;
		}
	}

	/* sort out which command line options take precedence */
#ifdef CONFIG_FEATURE_LS_RECURSIVE
	if (all_fmt & DISP_NOLIST)
		all_fmt &= ~DISP_RECURSIVE;	/* no recurse if listing only dir */
#endif
#if defined (CONFIG_FEATURE_LS_TIMESTAMPS) && defined (CONFIG_FEATURE_LS_SORTFILES)
	if (all_fmt & TIME_CHANGE)
		all_fmt = (all_fmt & ~SORT_MASK) | SORT_CTIME;
	if (all_fmt & TIME_ACCESS)
		all_fmt = (all_fmt & ~SORT_MASK) | SORT_ATIME;
#endif
	if ((all_fmt & STYLE_MASK) != STYLE_LONG) /* only for long list */
		all_fmt &= ~(LIST_ID_NUMERIC|LIST_FULLTIME|LIST_ID_NAME|LIST_ID_NUMERIC);
#ifdef CONFIG_FEATURE_LS_USERNAME
	if ((all_fmt & STYLE_MASK) == STYLE_LONG && (all_fmt & LIST_ID_NUMERIC))
		all_fmt &= ~LIST_ID_NAME;	/* don't list names if numeric uid */
#endif

	/* choose a display format */
	if ((all_fmt & STYLE_MASK) == STYLE_AUTO)
#if STYLE_AUTO != 0
		all_fmt = (all_fmt & ~STYLE_MASK)
				| (isatty(STDOUT_FILENO) ? STYLE_COLUMNS : STYLE_SINGLE);
#else
		all_fmt |= (isatty(STDOUT_FILENO) ? STYLE_COLUMNS : STYLE_SINGLE);
#endif

	/*
	 * when there are no cmd line args we have to supply a default "." arg.
	 * we will create a second argv array, "av" that will hold either
	 * our created "." arg, or the real cmd line args.  The av array
	 * just holds the pointers- we don't move the date the pointers
	 * point to.
	 */
	ac = argc - optind;	/* how many cmd line args are left */
	if (ac < 1) {
		av = (char **) xcalloc((size_t) 1, (size_t) (sizeof(char *)));
		av[0] = bb_xstrdup(".");
		ac = 1;
	} else {
		av = (char **) xcalloc((size_t) ac, (size_t) (sizeof(char *)));
		for (oi = 0; oi < ac; oi++) {
			av[oi] = argv[optind++];	/* copy pointer to real cmd line arg */
		}
	}

	/* now, everything is in the av array */
	if (ac > 1)
		all_fmt |= DISP_DIRNAME;	/* 2 or more items? label directories */

	/* stuff the command line file names into an dnode array */
	dn = NULL;
	for (oi = 0; oi < ac; oi++) {
		char *fullname = bb_xstrdup(av[oi]);

		cur = my_stat(fullname, fullname);
		if (!cur)
			continue;
		cur->next = dn;
		dn = cur;
		nfiles++;
	}

	/* now that we know how many files there are
	   ** allocate memory for an array to hold dnode pointers
	 */
	dnp = dnalloc(nfiles);
	for (i = 0, cur = dn; i < nfiles; i++) {
		dnp[i] = cur;	/* save pointer to node in array */
		cur = cur->next;
	}

	if (all_fmt & DISP_NOLIST) {
#ifdef CONFIG_FEATURE_LS_SORTFILES
		shellsort(dnp, nfiles);
#endif
		if (nfiles > 0)
			showfiles(dnp, nfiles);
	} else {
		dnd = splitdnarray(dnp, nfiles, SPLIT_DIR);
		dnf = splitdnarray(dnp, nfiles, SPLIT_FILE);
		dndirs = countdirs(dnp, nfiles);
		dnfiles = nfiles - dndirs;
		if (dnfiles > 0) {
#ifdef CONFIG_FEATURE_LS_SORTFILES
			shellsort(dnf, dnfiles);
#endif
			showfiles(dnf, dnfiles);
		}
		if (dndirs > 0) {
#ifdef CONFIG_FEATURE_LS_SORTFILES
			shellsort(dnd, dndirs);
#endif
			showdirs(dnd, dndirs, dnfiles == 0);
		}
	}
	return (status);
}
