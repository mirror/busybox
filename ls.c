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
	TERMINAL_WIDTH = 80,		/* use 79 if terminal has linefold bug */
	COLUMN_WIDTH = 14,			/* default if AUTOWIDTH not defined */
	COLUMN_GAP = 2,				/* includes the file type char */
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

#ifdef BB_FEATURE_LS_TIMESTAMPS
#include <time.h>
#endif

#ifndef MAJOR
#define MAJOR(dev) (((dev)>>8)&0xff)
#define MINOR(dev) ((dev)&0xff)
#endif

/* what is the overall style of the listing */
enum {
STYLE_AUTO = 0,
STYLE_LONG = 1,		/* one record per line, extended info */
STYLE_SINGLE = 2,		/* one record per line */
STYLE_COLUMNS = 3		/* fill columns */
};

/* 51306 lrwxrwxrwx  1 root     root         2 May 11 01:43 /bin/view -> vi* */
/* what file information will be listed */
#define LIST_INO		(1<<0)
#define LIST_BLOCKS		(1<<1)
#define LIST_MODEBITS	(1<<2)
#define LIST_NLINKS		(1<<3)
#define LIST_ID_NAME	(1<<4)
#define LIST_ID_NUMERIC	(1<<5)
#define LIST_SIZE		(1<<6)
#define LIST_DEV		(1<<7)
#define LIST_DATE_TIME	(1<<8)
#define LIST_FULLTIME	(1<<9)
#define LIST_FILENAME	(1<<10)
#define LIST_SYMLINK	(1<<11)
#define LIST_FILETYPE	(1<<12)
#define LIST_EXEC		(1<<13)

/* what files will be displayed */
#define DISP_NORMAL		(0)		/* show normal filenames */
#define DISP_DIRNAME	(1<<0)	/* 2 or more items? label directories */
#define DISP_HIDDEN		(1<<1)	/* show filenames starting with .  */
#define DISP_DOT		(1<<2)	/* show . and .. */
#define DISP_NOLIST		(1<<3)	/* show directory as itself, not contents */
#define DISP_RECURSIVE	(1<<4)	/* show directory and everything below it */
#define DISP_ROWS		(1<<5)	/* print across rows */

#ifdef BB_FEATURE_LS_SORTFILES
/* how will the files be sorted */
static const int SORT_FORWARD = 0;		/* sort in reverse order */
static const int SORT_REVERSE = 1;		/* sort in reverse order */
static const int SORT_NAME = 2;		/* sort by file name */
static const int SORT_SIZE = 3;		/* sort by file size */
static const int SORT_ATIME = 4;		/* sort by last access time */
static const int SORT_CTIME = 5;		/* sort by last change time */
static const int SORT_MTIME = 6;		/* sort by last modification time */
static const int SORT_VERSION = 7;		/* sort by version */
static const int SORT_EXT = 8;		/* sort by file name extension */
static const int SORT_DIR = 9;		/* sort by file or directory */
#endif

#ifdef BB_FEATURE_LS_TIMESTAMPS
/* which of the three times will be used */
static const int TIME_MOD = 0;
static const int TIME_CHANGE = 1;
static const int TIME_ACCESS = 2;
#endif

#define LIST_SHORT		(LIST_FILENAME)
#define LIST_ISHORT		(LIST_INO | LIST_FILENAME)
#define LIST_LONG		(LIST_MODEBITS | LIST_NLINKS | LIST_ID_NAME | \
						LIST_SIZE | LIST_DATE_TIME | LIST_FILENAME | \
						LIST_SYMLINK)
#define LIST_ILONG		(LIST_INO | LIST_LONG)

static const int SPLIT_DIR = 0;
static const int SPLIT_FILE = 1;
static const int SPLIT_SUBDIR = 2;

#define TYPEINDEX(mode) (((mode) >> 12) & 0x0f)
#define TYPECHAR(mode)  ("0pcCd?bB-?l?s???" [TYPEINDEX(mode)])
#if defined(BB_FEATURE_LS_FILETYPES) || defined(BB_FEATURE_LS_COLOR)
#define APPCHAR(mode)   ("\0|\0\0/\0\0\0\0\0@\0=\0\0\0" [TYPEINDEX(mode)])
#endif
/* colored LS support by JaWi, janwillem.janssen@lxtreme.nl */
#ifdef BB_FEATURE_LS_COLOR
static int show_color = 0;
#define COLOR(mode)   ("\000\043\043\043\042\000\043\043"\
					   "\000\000\044\000\043\000\000\040" [TYPEINDEX(mode)])
#define ATTR(mode)   ("\00\00\01\00\01\00\01\00"\
					  "\00\00\01\00\01\00\00\01" [TYPEINDEX(mode)])
#endif

/*
 * a directory entry and its stat info are stored here
 */
struct dnode {				/* the basic node */
    char *name;				/* the dir entry name */
    char *fullname;			/* the dir entry name */
    struct stat dstat;		/* the file stat info */
    struct dnode *next;		/* point at the next node */
};
typedef struct dnode dnode_t;

static struct dnode **list_dir(char *);
static struct dnode **dnalloc(int);
static int list_single(struct dnode *);

static unsigned int disp_opts;
static unsigned int style_fmt;
static unsigned int list_fmt;
#ifdef BB_FEATURE_LS_SORTFILES
static unsigned int sort_opts;
static unsigned int sort_order;
#endif
#ifdef BB_FEATURE_LS_TIMESTAMPS
static unsigned int time_fmt;
#endif
#ifdef BB_FEATURE_LS_FOLLOWLINKS
static unsigned int follow_links=FALSE;
#endif

static unsigned short column = 0;
#ifdef BB_FEATURE_AUTOWIDTH
static unsigned short terminal_width = TERMINAL_WIDTH;
static unsigned short column_width = COLUMN_WIDTH;
static unsigned short tabstops = COLUMN_GAP;
#else
static unsigned short column_width = COLUMN_WIDTH;
#endif

static int status = EXIT_SUCCESS;

#ifdef BB_FEATURE_HUMAN_READABLE
static unsigned long ls_disp_hr = 0;
#endif

static int my_stat(struct dnode *cur)
{
#ifdef BB_FEATURE_LS_FOLLOWLINKS
	if (follow_links == TRUE) {
		if (stat(cur->fullname, &cur->dstat)) {
			perror_msg("%s", cur->fullname);
			status = EXIT_FAILURE;
			free(cur->fullname);
			free(cur);
			return -1;
		}
	} else
#endif
	if (lstat(cur->fullname, &cur->dstat)) {
		perror_msg("%s", cur->fullname);
		status = EXIT_FAILURE;
		free(cur->fullname);
		free(cur);
		return -1;
	}
	return 0;
}

static void newline(void)
{
    if (column > 0) {
        putchar('\n');
        column = 0;
    }
}

/*----------------------------------------------------------------------*/
#ifdef BB_FEATURE_LS_COLOR
static char fgcolor(mode_t mode)
{
	/* Check if the file is missing */
	if ( errno == ENOENT ) {
		errno = 0;
		/* Color it red! */
	    return '\037';
	}
	if ( LIST_EXEC && S_ISREG( mode )
	    && ( mode & ( S_IXUSR | S_IXGRP | S_IXOTH ) ) ) 
		return COLOR(0xF000);	/* File is executable ... */
	return COLOR(mode);
}

/*----------------------------------------------------------------------*/
static char bgcolor(mode_t mode)
{
	if ( LIST_EXEC && S_ISREG( mode )
	    && ( mode & ( S_IXUSR | S_IXGRP | S_IXOTH ) ) ) 
		return ATTR(0xF000);	/* File is executable ... */
	return ATTR(mode);
}
#endif

/*----------------------------------------------------------------------*/
#if defined(BB_FEATURE_LS_FILETYPES) || defined(BB_FEATURE_LS_COLOR)
static char append_char(mode_t mode)
{
	if ( !(list_fmt & LIST_FILETYPE))
		return '\0';
	if ((list_fmt & LIST_EXEC) && S_ISREG(mode)
	    && (mode & (S_IXUSR | S_IXGRP | S_IXOTH))) return '*';
		return APPCHAR(mode);
}
#endif

/*----------------------------------------------------------------------*/
static void nexttabstop( void )
{
	static short nexttab= 0;
	int n=0;

	if (column > 0) {
		n= nexttab - column;
		if (n < 1) n= 1;
		while (n--) {
			putchar(' ');
			column++;
		}
	}
	nexttab= column + column_width + COLUMN_GAP; 
}

/*----------------------------------------------------------------------*/
static int is_subdir(struct dnode *dn)
{
	return (S_ISDIR(dn->dstat.st_mode) && strcmp(dn->name, ".") != 0 &&
			strcmp(dn->name, "..") != 0);
}

static int countdirs(struct dnode **dn, int nfiles)
{
	int i, dirs;

	if (dn==NULL || nfiles < 1) return(0);
	dirs= 0;
	for (i=0; i<nfiles; i++) {
		if (S_ISDIR(dn[i]->dstat.st_mode)) dirs++;
	}
	return(dirs);
}

static int countsubdirs(struct dnode **dn, int nfiles)
{
	int i, subdirs;

	if (dn == NULL || nfiles < 1) return 0;
	subdirs = 0;
	for (i = 0; i < nfiles; i++)
		if (is_subdir(dn[i]))
			subdirs++;
	return subdirs;
}

static int countfiles(struct dnode **dnp)
{
	int nfiles;
	struct dnode *cur;

	if (dnp == NULL) return(0);
	nfiles= 0;
	for (cur= dnp[0];  cur->next != NULL ; cur= cur->next) nfiles++;
	nfiles++;
	return(nfiles);
}

/* get memory to hold an array of pointers */
static struct dnode **dnalloc(int num)
{
	struct dnode **p;

	if (num < 1) return(NULL);

	p= (struct dnode **)xcalloc((size_t)num, (size_t)(sizeof(struct dnode *)));
	return(p);
}

#ifdef BB_FEATURE_LS_RECURSIVE
static void dfree(struct dnode **dnp)
{
	struct dnode *cur, *next;

	if(dnp == NULL) return;

	cur=dnp[0];
	while (cur != NULL) {
		if (cur->fullname != NULL) free(cur->fullname);	/* free the filename */
		next= cur->next;
		free(cur);				/* free the dnode */
		cur= next;
	}
	free(dnp);	/* free the array holding the dnode pointers */
}
#endif

static struct dnode **splitdnarray(struct dnode **dn, int nfiles, int which)
{
	int dncnt, i, d;
	struct dnode **dnp;

	if (dn==NULL || nfiles < 1) return(NULL);

	/* count how many dirs and regular files there are */
	if (which == SPLIT_SUBDIR)
		dncnt = countsubdirs(dn, nfiles);
	else {
		dncnt= countdirs(dn, nfiles); /* assume we are looking for dirs */
		if (which == SPLIT_FILE)
			dncnt= nfiles - dncnt;  /* looking for files */
	}

	/* allocate a file array and a dir array */
	dnp= dnalloc(dncnt);

	/* copy the entrys into the file or dir array */
	for (d= i=0; i<nfiles; i++) {
		if (which == SPLIT_DIR) {
			if (S_ISDIR(dn[i]->dstat.st_mode)) {
				dnp[d++]= dn[i];
			}  /* else skip the file */
		} else if (which == SPLIT_SUBDIR) {
			if (is_subdir(dn[i])) {
				dnp[d++]= dn[i];
			}  /* else skip the file or dir */
		} else {
			if (!(S_ISDIR(dn[i]->dstat.st_mode))) {
				dnp[d++]= dn[i];
			}  /* else skip the dir */
		}
	}
	return(dnp);
}

/*----------------------------------------------------------------------*/
#ifdef BB_FEATURE_LS_SORTFILES
static int sortcmp(struct dnode *d1, struct dnode *d2)
{
	int cmp, dif;

	cmp= 0;
	if (sort_opts == SORT_SIZE) {
		dif= (int)(d1->dstat.st_size - d2->dstat.st_size);
	} else if (sort_opts == SORT_ATIME) {
		dif= (int)(d1->dstat.st_atime - d2->dstat.st_atime);
	} else if (sort_opts == SORT_CTIME) {
		dif= (int)(d1->dstat.st_ctime - d2->dstat.st_ctime);
	} else if (sort_opts == SORT_MTIME) {
		dif= (int)(d1->dstat.st_mtime - d2->dstat.st_mtime);
	} else if (sort_opts == SORT_DIR) {
		dif= S_ISDIR(d1->dstat.st_mode) - S_ISDIR(d2->dstat.st_mode);
	/* } else if (sort_opts == SORT_VERSION) { */
	/* } else if (sort_opts == SORT_EXT) { */
	} else {    /* assume SORT_NAME */
		dif= 0;
	}

	if (dif > 0) cmp= -1;
	if (dif < 0) cmp=  1;
	if (dif == 0) {
		/* sort by name- may be a tie_breaker for time or size cmp */
		dif= strcmp(d1->name, d2->name);
		if (dif > 0) cmp=  1;
		if (dif < 0) cmp= -1;
	}

	if (sort_order == SORT_REVERSE) {
		cmp=  -1 * cmp;
	}
	return(cmp);
}

/*----------------------------------------------------------------------*/
static void shellsort(struct dnode **dn, int size)
{
	struct dnode *temp;
	int gap, i, j;

	/* shell short the array */
	if(dn==NULL || size < 2) return;

	for (gap= size/2; gap>0; gap /=2) {
		for (i=gap; i<size; i++) {
			for (j= i-gap; j>=0; j-=gap) {
				if (sortcmp(dn[j], dn[j+gap]) <= 0)
					break;
				/* they are out of order, swap them */
				temp= dn[j];
				dn[j]= dn[j+gap];
				dn[j+gap]= temp;
			}
		}
	}
}
#endif

/*----------------------------------------------------------------------*/
static void showfiles(struct dnode **dn, int nfiles)
{
	int i, ncols, nrows, row, nc;
#ifdef BB_FEATURE_AUTOWIDTH
	int len;
#endif

	if(dn==NULL || nfiles < 1) return;

#ifdef BB_FEATURE_AUTOWIDTH
	/* find the longest file name-  use that as the column width */
	column_width= 0;
	for (i=0; i<nfiles; i++) {
		len= strlen(dn[i]->name) +
			((list_fmt & LIST_INO) ? 8 : 0) +
			((list_fmt & LIST_BLOCKS) ? 5 : 0)
			;
		if (column_width < len) 
			column_width= len;
	}
	if (column_width >= 1)
		ncols = (int)(terminal_width / (column_width + COLUMN_GAP));
	else {
		ncols = 1;
		column_width = COLUMN_WIDTH;
	}
#else
	ncols= TERMINAL_WIDTH;
#endif
	switch (style_fmt) {
		case STYLE_LONG:	/* one record per line, extended info */
		case STYLE_SINGLE:	/* one record per line */
			ncols= 1;
			break;
	}

	if (ncols > 1) {
		nrows = nfiles / ncols;
	} else {
		nrows = nfiles;
		ncols = 1;
	}
	if ((nrows * ncols) < nfiles) nrows++; /* round up fractionals */

	if (nrows > nfiles) nrows= nfiles;
	for (row=0; row<nrows; row++) {
		for (nc=0; nc<ncols; nc++) {
			/* reach into the array based on the column and row */
			i= (nc * nrows) + row;		/* assume display by column */
			if (disp_opts & DISP_ROWS)
				i= (row * ncols) + nc;	/* display across row */
			if (i < nfiles) {
				nexttabstop();
				list_single(dn[i]);
			}
		}
		newline();
	}
}

/*----------------------------------------------------------------------*/
static void showdirs(struct dnode **dn, int ndirs)
{
	int i, nfiles;
	struct dnode **subdnp;
#ifdef BB_FEATURE_LS_RECURSIVE
	int dndirs;
	struct dnode **dnd;
#endif

	if (dn==NULL || ndirs < 1) return;

	for (i=0; i<ndirs; i++) {
		if (disp_opts & (DISP_DIRNAME | DISP_RECURSIVE)) {
			printf("\n%s:\n", dn[i]->fullname);
		}
		subdnp= list_dir(dn[i]->fullname);
		nfiles= countfiles(subdnp);
		if (nfiles > 0) {
			/* list all files at this level */
#ifdef BB_FEATURE_LS_SORTFILES
			shellsort(subdnp, nfiles);
#endif
			showfiles(subdnp, nfiles);
#ifdef BB_FEATURE_LS_RECURSIVE
			if (disp_opts & DISP_RECURSIVE) {
				/* recursive- list the sub-dirs */
				dnd= splitdnarray(subdnp, nfiles, SPLIT_SUBDIR);
				dndirs= countsubdirs(subdnp, nfiles);
				if (dndirs > 0) {
#ifdef BB_FEATURE_LS_SORTFILES
					shellsort(dnd, dndirs);
#endif
					showdirs(dnd, dndirs);
					free(dnd);  /* free the array of dnode pointers to the dirs */
				}
			}
			dfree(subdnp);  /* free the dnodes and the fullname mem */
#endif
		}
	}
}

/*----------------------------------------------------------------------*/
static struct dnode **list_dir(char *path)
{
	struct dnode *dn, *cur, **dnp;
	struct dirent *entry;
	DIR *dir;
	int i, nfiles;

	if (path==NULL) return(NULL);

	dn= NULL;
	nfiles= 0;
	dir = opendir(path);
	if (dir == NULL) {
		perror_msg("%s", path);
		status = EXIT_FAILURE;
		return(NULL);	/* could not open the dir */
	}
	while ((entry = readdir(dir)) != NULL) {
		/* are we going to list the file- it may be . or .. or a hidden file */
		if ((strcmp(entry->d_name, ".")==0) && !(disp_opts & DISP_DOT))
			continue;
		if ((strcmp(entry->d_name, "..")==0) && !(disp_opts & DISP_DOT))
			continue;
		if ((entry->d_name[0] ==  '.') && !(disp_opts & DISP_HIDDEN))
			continue;
		cur= (struct dnode *)xmalloc(sizeof(struct dnode));
		cur->fullname = concat_path_file(path, entry->d_name);
		cur->name = cur->fullname +
				(strlen(cur->fullname) - strlen(entry->d_name));
		if (my_stat(cur))
			continue;
		cur->next= dn;
		dn= cur;
		nfiles++;
	}
	closedir(dir);

	/* now that we know how many files there are
	** allocate memory for an array to hold dnode pointers
	*/
	if (nfiles < 1) return(NULL);
	dnp= dnalloc(nfiles);
	for (i=0, cur=dn; i<nfiles; i++) {
		dnp[i]= cur;   /* save pointer to node in array */
		cur= cur->next;
	}

	return(dnp);
}

/*----------------------------------------------------------------------*/
static int list_single(struct dnode *dn)
{
	int i;
#ifdef BB_FEATURE_LS_USERNAME
	char scratch[BUFSIZ + 1];
#endif
#ifdef BB_FEATURE_LS_TIMESTAMPS
	char *filetime;
	time_t ttime, age;
#endif
#if defined (BB_FEATURE_LS_FILETYPES) || defined (BB_FEATURE_LS_COLOR)
	struct stat info;
	char append;
#endif

	if (dn==NULL || dn->fullname==NULL) return(0);

#ifdef BB_FEATURE_LS_TIMESTAMPS
	ttime= dn->dstat.st_mtime;      /* the default time */
	if (time_fmt & TIME_ACCESS) ttime= dn->dstat.st_atime;
	if (time_fmt & TIME_CHANGE) ttime= dn->dstat.st_ctime;
	filetime= ctime(&ttime);
#endif
#ifdef BB_FEATURE_LS_FILETYPES
	append = append_char(dn->dstat.st_mode);
#endif

	for (i=0; i<=31; i++) {
		switch (list_fmt & (1<<i)) {
			case LIST_INO:
				printf("%7ld ", (long int)dn->dstat.st_ino);
				column += 8;
				break;
			case LIST_BLOCKS:
#ifdef BB_FEATURE_HUMAN_READABLE
				fprintf(stdout, "%6s ", make_human_readable_str(dn->dstat.st_blocks>>1, 
							KILOBYTE, (ls_disp_hr==TRUE)? 0: KILOBYTE));
#else
#if _FILE_OFFSET_BITS == 64
				printf("%4lld ", dn->dstat.st_blocks>>1);
#else
				printf("%4ld ", dn->dstat.st_blocks>>1);
#endif
#endif
				column += 5;
				break;
			case LIST_MODEBITS:
				printf("%-10s ", (char *)mode_string(dn->dstat.st_mode));
				column += 10;
				break;
			case LIST_NLINKS:
				printf("%4ld ", (long)dn->dstat.st_nlink);
				column += 10;
				break;
			case LIST_ID_NAME:
#ifdef BB_FEATURE_LS_USERNAME
				my_getpwuid(scratch, dn->dstat.st_uid);
				printf("%-8.8s ", scratch);
				my_getgrgid(scratch, dn->dstat.st_gid);
				printf("%-8.8s", scratch);
				column += 17;
				break;
#endif
			case LIST_ID_NUMERIC:
				printf("%-8ld %-8ld", (long)dn->dstat.st_uid, (long)dn->dstat.st_gid);
				column += 17;
				break;
			case LIST_SIZE:
			case LIST_DEV:
				if (S_ISBLK(dn->dstat.st_mode) || S_ISCHR(dn->dstat.st_mode)) {
					printf("%4d, %3d ", (int)MAJOR(dn->dstat.st_rdev), (int)MINOR(dn->dstat.st_rdev));
				} else {
#ifdef BB_FEATURE_HUMAN_READABLE
					if (ls_disp_hr==TRUE) {
						fprintf(stdout, "%8s ", make_human_readable_str(dn->dstat.st_size, 1, 0));
					} else 
#endif	
					{
#if _FILE_OFFSET_BITS == 64
						printf("%9lld ", (long long)dn->dstat.st_size);
#else
						printf("%9ld ", dn->dstat.st_size);
#endif
					}
				}
				column += 10;
				break;
#ifdef BB_FEATURE_LS_TIMESTAMPS
			case LIST_FULLTIME:
			case LIST_DATE_TIME:
				if (list_fmt & LIST_FULLTIME) {
					printf("%24.24s ", filetime);
					column += 25;
					break;
				}
				age = time(NULL) - ttime;
				printf("%6.6s ", filetime+4);
				if (age < 3600L * 24 * 365 / 2 && age > -15 * 60) {
					/* hh:mm if less than 6 months old */
					printf("%5.5s ", filetime+11);
				} else {
					printf(" %4.4s ", filetime+20);
				}
				column += 13;
				break;
#endif
			case LIST_FILENAME:
#ifdef BB_FEATURE_LS_COLOR
				errno = 0;
				if (show_color && !lstat(dn->fullname, &info)) {
				    printf( "\033[%d;%dm", bgcolor(info.st_mode), 
								fgcolor(info.st_mode) );
				}
#endif
				printf("%s", dn->name);
#ifdef BB_FEATURE_LS_COLOR
				if (show_color) {
					printf( "\033[0m" );
				}
#endif
				column += strlen(dn->name);
				break;
			case LIST_SYMLINK:
				if (S_ISLNK(dn->dstat.st_mode)) {
					char *lpath = xreadlink(dn->fullname);
					if (lpath) {
						printf(" -> ");
#if defined(BB_FEATURE_LS_FILETYPES) || defined(BB_FEATURE_LS_COLOR)
						if (!stat(dn->fullname, &info)) {
							append = append_char(info.st_mode);
						}
#endif
#ifdef BB_FEATURE_LS_COLOR
						if (show_color) {
							errno = 0;
							printf( "\033[%d;%dm", bgcolor(info.st_mode), 
									fgcolor(info.st_mode) );
						}
#endif
						printf("%s", lpath);
#ifdef BB_FEATURE_LS_COLOR
						if (show_color) {
							printf( "\033[0m" );
						}
#endif
						column += strlen(lpath) + 4;
						free(lpath);
					}
				}
				break;
#ifdef BB_FEATURE_LS_FILETYPES
			case LIST_FILETYPE:
				if (append != '\0') {
					printf("%1c", append);
					column++;
				}
				break;
#endif
		}
	}

	return(0);
}

/*----------------------------------------------------------------------*/
extern int ls_main(int argc, char **argv)
{
	struct dnode **dnf, **dnd;
	int dnfiles, dndirs;
	struct dnode *dn, *cur, **dnp;
	int i, nfiles;
	int opt;
	int oi, ac;
	char **av;
#ifdef BB_FEATURE_AUTOWIDTH
	struct winsize win = { 0, 0, 0, 0 };
#endif

	disp_opts= DISP_NORMAL;
	style_fmt= STYLE_AUTO;
	list_fmt=  LIST_SHORT;
#ifdef BB_FEATURE_LS_SORTFILES
	sort_opts= SORT_NAME;
	sort_order=	SORT_FORWARD;
#endif
#ifdef BB_FEATURE_LS_TIMESTAMPS
	time_fmt= TIME_MOD;
#endif
#ifdef BB_FEATURE_AUTOWIDTH
	ioctl(fileno(stdout), TIOCGWINSZ, &win);
	if (win.ws_row > 4)
		column_width = win.ws_row - 2;
	if (win.ws_col > 0)
		terminal_width = win.ws_col - 1;
#endif
	nfiles=0;

#ifdef BB_FEATURE_LS_COLOR
	if (isatty(fileno(stdout)))
		show_color = 1;
#endif

	/* process options */
	while ((opt = getopt(argc, argv, "1AaCdgilnsx"
#ifdef BB_FEATURE_AUTOWIDTH
"T:w:"
#endif
#ifdef BB_FEATURE_LS_FILETYPES
"Fp"
#endif
#ifdef BB_FEATURE_LS_RECURSIVE
"R"
#endif
#ifdef BB_FEATURE_LS_SORTFILES
"rSvX"
#endif
#ifdef BB_FEATURE_LS_TIMESTAMPS
"cetu"
#endif
#ifdef BB_FEATURE_LS_FOLLOWLINKS
"L"
#endif
#ifdef BB_FEATURE_HUMAN_READABLE
"h"
#endif
"k")) > 0) {
		switch (opt) {
			case '1': style_fmt = STYLE_SINGLE; break;
			case 'A': disp_opts |= DISP_HIDDEN; break;
			case 'a': disp_opts |= DISP_HIDDEN | DISP_DOT; break;
			case 'C': style_fmt = STYLE_COLUMNS; break;
			case 'd': disp_opts |= DISP_NOLIST; break;
			case 'g': /* ignore -- for ftp servers */ break;
			case 'i': list_fmt |= LIST_INO; break;
			case 'l':
				style_fmt = STYLE_LONG;
				list_fmt |= LIST_LONG;
#ifdef BB_FEATURE_HUMAN_READABLE
				ls_disp_hr = FALSE;
#endif
			break;
			case 'n': list_fmt |= LIST_ID_NUMERIC; break;
			case 's': list_fmt |= LIST_BLOCKS; break;
			case 'x': disp_opts = DISP_ROWS; break;
#ifdef BB_FEATURE_LS_FILETYPES
			case 'F': list_fmt |= LIST_FILETYPE | LIST_EXEC; break;
			case 'p': list_fmt |= LIST_FILETYPE; break;
#endif
#ifdef BB_FEATURE_LS_RECURSIVE
			case 'R': disp_opts |= DISP_RECURSIVE; break;
#endif
#ifdef BB_FEATURE_LS_SORTFILES
			case 'r': sort_order |= SORT_REVERSE; break;
			case 'S': sort_opts= SORT_SIZE; break;
			case 'v': sort_opts= SORT_VERSION; break;
			case 'X': sort_opts= SORT_EXT; break;
#endif
#ifdef BB_FEATURE_LS_TIMESTAMPS
			case 'e': list_fmt |= LIST_FULLTIME; break;
			case 'c':
				time_fmt = TIME_CHANGE;
#ifdef BB_FEATURE_LS_SORTFILES
				sort_opts= SORT_CTIME;
#endif
				break;
			case 'u':
				time_fmt = TIME_ACCESS;
#ifdef BB_FEATURE_LS_SORTFILES
				sort_opts= SORT_ATIME;
#endif
				break;
			case 't':
#ifdef BB_FEATURE_LS_SORTFILES
				sort_opts= SORT_MTIME;
#endif
				break;
#endif
#ifdef BB_FEATURE_LS_FOLLOWLINKS
			case 'L': follow_links= TRUE; break;
#endif
#ifdef BB_FEATURE_AUTOWIDTH
			case 'T': tabstops= atoi(optarg); break;
			case 'w': terminal_width= atoi(optarg); break;
#endif
#ifdef BB_FEATURE_HUMAN_READABLE
			case 'h': ls_disp_hr = TRUE; break;
#endif
			case 'k': break;
			default:
				goto print_usage_message;
		}
	}

	/* sort out which command line options take precedence */
#ifdef BB_FEATURE_LS_RECURSIVE
	if (disp_opts & DISP_NOLIST)
		disp_opts &= ~DISP_RECURSIVE;   /* no recurse if listing only dir */
#endif
#if defined (BB_FEATURE_LS_TIMESTAMPS) && defined (BB_FEATURE_LS_SORTFILES)
	if (time_fmt & TIME_CHANGE) sort_opts= SORT_CTIME;
	if (time_fmt & TIME_ACCESS) sort_opts= SORT_ATIME;
#endif
	if (style_fmt != STYLE_LONG)
			list_fmt &= ~LIST_ID_NUMERIC;  /* numeric uid only for long list */
#ifdef BB_FEATURE_LS_USERNAME
	if (style_fmt == STYLE_LONG && (list_fmt & LIST_ID_NUMERIC))
			list_fmt &= ~LIST_ID_NAME;  /* don't list names if numeric uid */
#endif

	/* choose a display format */
	if (style_fmt == STYLE_AUTO)
		style_fmt = isatty(fileno(stdout)) ? STYLE_COLUMNS : STYLE_SINGLE;

	/*
	 * when there are no cmd line args we have to supply a default "." arg.
	 * we will create a second argv array, "av" that will hold either
	 * our created "." arg, or the real cmd line args.  The av array
	 * just holds the pointers- we don't move the date the pointers
	 * point to.
	 */
	ac= argc - optind;   /* how many cmd line args are left */
	if (ac < 1) {
		av= (char **)xcalloc((size_t)1, (size_t)(sizeof(char *)));
		av[0]= xstrdup(".");
		ac=1;
	} else {
		av= (char **)xcalloc((size_t)ac, (size_t)(sizeof(char *)));
		for (oi=0 ; oi < ac; oi++) {
			av[oi]= argv[optind++];  /* copy pointer to real cmd line arg */
		}
	}

	/* now, everything is in the av array */
	if (ac > 1)
		disp_opts |= DISP_DIRNAME;   /* 2 or more items? label directories */

	/* stuff the command line file names into an dnode array */
	dn=NULL;
	for (oi=0 ; oi < ac; oi++) {
		cur= (struct dnode *)xmalloc(sizeof(struct dnode));
		cur->fullname= xstrdup(av[oi]);
		cur->name= cur->fullname;
		if (my_stat(cur))
			continue;
		cur->next= dn;
		dn= cur;
		nfiles++;
	}

	/* now that we know how many files there are
	** allocate memory for an array to hold dnode pointers
	*/
	dnp= dnalloc(nfiles);
	for (i=0, cur=dn; i<nfiles; i++) {
		dnp[i]= cur;   /* save pointer to node in array */
		cur= cur->next;
	}


	if (disp_opts & DISP_NOLIST) {
#ifdef BB_FEATURE_LS_SORTFILES
		shellsort(dnp, nfiles);
#endif
		if (nfiles > 0) showfiles(dnp, nfiles);
	} else {
		dnd= splitdnarray(dnp, nfiles, SPLIT_DIR);
		dnf= splitdnarray(dnp, nfiles, SPLIT_FILE);
		dndirs= countdirs(dnp, nfiles);
		dnfiles= nfiles - dndirs;
		if (dnfiles > 0) {
#ifdef BB_FEATURE_LS_SORTFILES
			shellsort(dnf, dnfiles);
#endif
			showfiles(dnf, dnfiles);
		}
		if (dndirs > 0) {
#ifdef BB_FEATURE_LS_SORTFILES
			shellsort(dnd, dndirs);
#endif
			showdirs(dnd, dndirs);
		}
	}
	return(status);

  print_usage_message:
	show_usage();
}
