/* vi: set sw=4 ts=4: */
/*
 * tiny-ls.c version 0.1.0: A minimalist 'ls'
 * Copyright (C) 1996 Brian Candler <B.Candler@pobox.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
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

#include "busybox.h"
#include <getopt.h>

enum {

TERMINAL_WIDTH  = 80,           /* use 79 if terminal has linefold bug */
COLUMN_GAP      = 2,            /* includes the file type char */

/* what is the overall style of the listing */
STYLE_COLUMNS   = 1 << 21,      /* fill columns */
STYLE_LONG      = 2 << 21,      /* one record per line, extended info */
STYLE_SINGLE    = 3 << 21,      /* one record per line */
STYLE_MASK      = STYLE_SINGLE,

/* 51306 lrwxrwxrwx  1 root     root         2 May 11 01:43 /bin/view -> vi* */
/* what file information will be listed */
LIST_INO        = 1 << 0,
LIST_BLOCKS     = 1 << 1,
LIST_MODEBITS   = 1 << 2,
LIST_NLINKS     = 1 << 3,
LIST_ID_NAME    = 1 << 4,
LIST_ID_NUMERIC = 1 << 5,
LIST_CONTEXT    = 1 << 6,
LIST_SIZE       = 1 << 7,
LIST_DEV        = 1 << 8,
LIST_DATE_TIME  = 1 << 9,
LIST_FULLTIME   = 1 << 10,
LIST_FILENAME   = 1 << 11,
LIST_SYMLINK    = 1 << 12,
LIST_FILETYPE   = 1 << 13,
LIST_EXEC       = 1 << 14,
LIST_MASK       = (LIST_EXEC << 1) - 1,

/* what files will be displayed */
DISP_DIRNAME    = 1 << 15,      /* 2 or more items? label directories */
DISP_HIDDEN     = 1 << 16,      /* show filenames starting with . */
DISP_DOT        = 1 << 17,      /* show . and .. */
DISP_NOLIST     = 1 << 18,      /* show directory as itself, not contents */
DISP_RECURSIVE  = 1 << 19,      /* show directory and everything below it */
DISP_ROWS       = 1 << 20,      /* print across rows */
DISP_MASK       = ((DISP_ROWS << 1) - 1) & ~(DISP_DIRNAME - 1),

/* how will the files be sorted (CONFIG_FEATURE_LS_SORTFILES) */
SORT_FORWARD    = 0,            /* sort in reverse order */
SORT_REVERSE    = 1 << 27,      /* sort in reverse order */

SORT_NAME       = 0,            /* sort by file name */
SORT_SIZE       = 1 << 28,      /* sort by file size */
SORT_ATIME      = 2 << 28,      /* sort by last access time */
SORT_CTIME      = 3 << 28,      /* sort by last change time */
SORT_MTIME      = 4 << 28,      /* sort by last modification time */
SORT_VERSION    = 5 << 28,      /* sort by version */
SORT_EXT        = 6 << 28,      /* sort by file name extension */
SORT_DIR        = 7 << 28,      /* sort by file or directory */
SORT_MASK       = (7 << 28) * ENABLE_FEATURE_LS_SORTFILES,

/* which of the three times will be used */
TIME_CHANGE     = (1 << 23) * ENABLE_FEATURE_LS_TIMESTAMPS,
TIME_ACCESS     = (1 << 24) * ENABLE_FEATURE_LS_TIMESTAMPS,
TIME_MASK       = (3 << 23) * ENABLE_FEATURE_LS_TIMESTAMPS,

FOLLOW_LINKS    = (1 << 25) * ENABLE_FEATURE_LS_FOLLOWLINKS,

LS_DISP_HR      = (1 << 26) * ENABLE_FEATURE_HUMAN_READABLE,

LIST_SHORT      = LIST_FILENAME,
LIST_LONG       = LIST_MODEBITS | LIST_NLINKS | LIST_ID_NAME | LIST_SIZE | \
                  LIST_DATE_TIME | LIST_FILENAME | LIST_SYMLINK,

SPLIT_DIR       = 1,
SPLIT_FILE      = 0,
SPLIT_SUBDIR    = 2,

};

#define TYPEINDEX(mode) (((mode) >> 12) & 0x0f)
#define TYPECHAR(mode)  ("0pcCd?bB-?l?s???" [TYPEINDEX(mode)])
#define APPCHAR(mode)   ("\0|\0\0/\0\0\0\0\0@\0=\0\0\0" [TYPEINDEX(mode)])
#define COLOR(mode)	("\000\043\043\043\042\000\043\043"\
			 "\000\000\044\000\043\000\000\040" [TYPEINDEX(mode)])
#define ATTR(mode)	("\00\00\01\00\01\00\01\00"\
			 "\00\00\01\00\01\00\00\01" [TYPEINDEX(mode)])

/* colored LS support by JaWi, janwillem.janssen@lxtreme.nl */
#if ENABLE_FEATURE_LS_COLOR
static int show_color;
/* long option entry used only for --color, which has no short option
 * equivalent */
static const struct option ls_color_opt[] = {
	{ "color", optional_argument, NULL, 1 },
	{ NULL, 0, NULL, 0 }
};
#else
enum { show_color = 0 };
#endif

/*
 * a directory entry and its stat info are stored here
 */
struct dnode {                  /* the basic node */
	char *name;             /* the dir entry name */
	char *fullname;         /* the dir entry name */
	int   allocated;
	struct stat dstat;      /* the file stat info */
	USE_SELINUX(security_context_t sid;)
	struct dnode *next;     /* point at the next node */
};
typedef struct dnode dnode_t;

static struct dnode **list_dir(const char *);
static struct dnode **dnalloc(int);
static int list_single(struct dnode *);

static unsigned all_fmt;

#if ENABLE_FEATURE_AUTOWIDTH
static unsigned tabstops = COLUMN_GAP;
static unsigned terminal_width = TERMINAL_WIDTH;
#else
enum {
	tabstops = COLUMN_GAP,
	terminal_width = TERMINAL_WIDTH,
};
#endif

static int status = EXIT_SUCCESS;

static struct dnode *my_stat(char *fullname, char *name, int force_follow)
{
	struct stat dstat;
	struct dnode *cur;
	USE_SELINUX(security_context_t sid = NULL;)

	if ((all_fmt & FOLLOW_LINKS) || force_follow) {
#if ENABLE_SELINUX
		if (is_selinux_enabled())  {
			 getfilecon(fullname, &sid);
		}
#endif
		if (stat(fullname, &dstat)) {
			bb_perror_msg("%s", fullname);
			status = EXIT_FAILURE;
			return 0;
		}
	} else {
#if ENABLE_SELINUX
		if (is_selinux_enabled()) {
			lgetfilecon(fullname, &sid);
		}
#endif
		if (lstat(fullname, &dstat)) {
			bb_perror_msg("%s", fullname);
			status = EXIT_FAILURE;
			return 0;
		}
	}

	cur = xmalloc(sizeof(struct dnode));
	cur->fullname = fullname;
	cur->name = name;
	cur->dstat = dstat;
	USE_SELINUX(cur->sid = sid;)
	return cur;
}

#if ENABLE_FEATURE_LS_COLOR
static char fgcolor(mode_t mode)
{
	/* Check wheter the file is existing (if so, color it red!) */
	if (errno == ENOENT)
		return '\037';
	if (S_ISREG(mode) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
		return COLOR(0xF000);	/* File is executable ... */
	return COLOR(mode);
}

static char bgcolor(mode_t mode)
{
	if (S_ISREG(mode) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
		return ATTR(0xF000);	/* File is executable ... */
	return ATTR(mode);
}
#endif

#if ENABLE_FEATURE_LS_FILETYPES || ENABLE_FEATURE_LS_COLOR
static char append_char(mode_t mode)
{
	if (!(all_fmt & LIST_FILETYPE))
		return '\0';
	if (S_ISDIR(mode))
		return '/';
	if (!(all_fmt & LIST_EXEC))
		return '\0';
	if (S_ISREG(mode) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
		return '*';
	return APPCHAR(mode);
}
#endif

#define countdirs(A, B) count_dirs((A), (B), 1)
#define countsubdirs(A, B) count_dirs((A), (B), 0)
static int count_dirs(struct dnode **dn, int nfiles, int notsubdirs)
{
	int i, dirs;

	if (!dn)
		return 0;
	dirs = 0;
	for (i = 0; i < nfiles; i++) {
		char *name;
		if (!S_ISDIR(dn[i]->dstat.st_mode))
			continue;
		name = dn[i]->name;
		if (notsubdirs
		 || name[0]!='.' || (name[1] && (name[1]!='.' || name[2]))
		) {
			dirs++;
		}
	}
	return dirs;
}

static int countfiles(struct dnode **dnp)
{
	int nfiles;
	struct dnode *cur;

	if (dnp == NULL)
		return 0;
	nfiles = 0;
	for (cur = dnp[0]; cur->next; cur = cur->next)
		nfiles++;
	nfiles++;
	return nfiles;
}

/* get memory to hold an array of pointers */
static struct dnode **dnalloc(int num)
{
	if (num < 1)
		return NULL;

	return xzalloc(num * sizeof(struct dnode *));
}

#if ENABLE_FEATURE_LS_RECURSIVE
static void dfree(struct dnode **dnp, int nfiles)
{
	int i;

	if (dnp == NULL)
		return;

	for (i = 0; i < nfiles; i++) {
		struct dnode *cur = dnp[i];
		if (cur->allocated)
			free(cur->fullname);	/* free the filename */
		free(cur);		/* free the dnode */
	}
	free(dnp);			/* free the array holding the dnode pointers */
}
#else
#define dfree(...) ((void)0)
#endif

static struct dnode **splitdnarray(struct dnode **dn, int nfiles, int which)
{
	int dncnt, i, d;
	struct dnode **dnp;

	if (dn == NULL || nfiles < 1)
		return NULL;

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
			char *name;
			if (!(which & (SPLIT_DIR|SPLIT_SUBDIR)))
				continue;
			name = dn[i]->name;
			if ((which & SPLIT_DIR)
			 || name[0]!='.' || (name[1] && (name[1]!='.' || name[2]))
			) {
				dnp[d++] = dn[i];
			}
		} else if (!(which & (SPLIT_DIR|SPLIT_SUBDIR))) {
			dnp[d++] = dn[i];
		}
	}
	return dnp;
}

#if ENABLE_FEATURE_LS_SORTFILES
static int sortcmp(const void *a, const void *b)
{
	struct dnode *d1 = *(struct dnode **)a;
	struct dnode *d2 = *(struct dnode **)b;
	unsigned sort_opts = all_fmt & SORT_MASK;
	int dif;

	dif = 0; /* assume SORT_NAME */
	// TODO: use pre-initialized function pointer
	// instead of branch forest
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
		/* sort by name - may be a tie_breaker for time or size cmp */
		if (ENABLE_LOCALE_SUPPORT) dif = strcoll(d1->name, d2->name);
		else dif = strcmp(d1->name, d2->name);
	}

	if (all_fmt & SORT_REVERSE) {
		dif = -dif;
	}
	return dif;
}

static void dnsort(struct dnode **dn, int size)
{
	qsort(dn, size, sizeof(*dn), sortcmp);
}
#else
#define dnsort(dn, size) ((void)0)
#endif


static void showfiles(struct dnode **dn, int nfiles)
{
	int i, ncols, nrows, row, nc;
	int column = 0;
	int nexttab = 0;
	int column_width = 0; /* for STYLE_LONG and STYLE_SINGLE not used */

	if (dn == NULL || nfiles < 1)
		return;

	if (all_fmt & STYLE_LONG) {
		ncols = 1;
	} else {
		/* find the longest file name, use that as the column width */
		for (i = 0; i < nfiles; i++) {
			int len = strlen(dn[i]->name);
			if (column_width < len)
				column_width = len;
		}
		column_width += tabstops +
			USE_SELINUX( ((all_fmt & LIST_CONTEXT) ? 33 : 0) + )
			             ((all_fmt & LIST_INO) ? 8 : 0) +
			             ((all_fmt & LIST_BLOCKS) ? 5 : 0);
		ncols = (int) (terminal_width / column_width);
	}

	if (ncols > 1) {
		nrows = nfiles / ncols;
		if (nrows * ncols < nfiles)
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
					printf("%*s", nexttab, "");
					column += nexttab;
				}
				nexttab = column + column_width;
				column += list_single(dn[i]);
			}
		}
		putchar('\n');
		column = 0;
	}
}


static void showdirs(struct dnode **dn, int ndirs, int first)
{
	int i, nfiles;
	struct dnode **subdnp;
	int dndirs;
	struct dnode **dnd;

	if (dn == NULL || ndirs < 1)
		return;

	for (i = 0; i < ndirs; i++) {
		if (all_fmt & (DISP_DIRNAME | DISP_RECURSIVE)) {
			if (!first)
				puts("");
			first = 0;
			printf("%s:\n", dn[i]->fullname);
		}
		subdnp = list_dir(dn[i]->fullname);
		nfiles = countfiles(subdnp);
		if (nfiles > 0) {
			/* list all files at this level */
			dnsort(subdnp, nfiles);
			showfiles(subdnp, nfiles);
			if (ENABLE_FEATURE_LS_RECURSIVE) {
				if (all_fmt & DISP_RECURSIVE) {
					/* recursive- list the sub-dirs */
					dnd = splitdnarray(subdnp, nfiles, SPLIT_SUBDIR);
					dndirs = countsubdirs(subdnp, nfiles);
					if (dndirs > 0) {
						dnsort(dnd, dndirs);
						showdirs(dnd, dndirs, 0);
						/* free the array of dnode pointers to the dirs */
						free(dnd);
					}
				}
				/* free the dnodes and the fullname mem */
				dfree(subdnp, nfiles);
			}
		}
	}
}


static struct dnode **list_dir(const char *path)
{
	struct dnode *dn, *cur, **dnp;
	struct dirent *entry;
	DIR *dir;
	int i, nfiles;

	if (path == NULL)
		return NULL;

	dn = NULL;
	nfiles = 0;
	dir = warn_opendir(path);
	if (dir == NULL) {
		status = EXIT_FAILURE;
		return NULL;	/* could not open the dir */
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
		cur = my_stat(fullname, strrchr(fullname, '/') + 1, 0);
		if (!cur) {
			free(fullname);
			continue;
		}
		cur->allocated = 1;
		cur->next = dn;
		dn = cur;
		nfiles++;
	}
	closedir(dir);

	/* now that we know how many files there are
	 * allocate memory for an array to hold dnode pointers
	 */
	if (dn == NULL)
		return NULL;
	dnp = dnalloc(nfiles);
	for (i = 0, cur = dn; i < nfiles; i++) {
		dnp[i] = cur;	/* save pointer to node in array */
		cur = cur->next;
	}

	return dnp;
}


#if ENABLE_FEATURE_LS_TIMESTAMPS
/* Do time() just once. Saves one syscall per file for "ls -l" */
/* Initialized in main() */
static time_t current_time_t;
#endif

static int list_single(struct dnode *dn)
{
	int i, column = 0;

#if ENABLE_FEATURE_LS_TIMESTAMPS
	char *filetime;
	time_t ttime, age;
#endif
#if ENABLE_FEATURE_LS_FILETYPES || ENABLE_FEATURE_LS_COLOR
	struct stat info;
	char append;
#endif

	if (dn->fullname == NULL)
		return 0;

#if ENABLE_FEATURE_LS_TIMESTAMPS
	ttime = dn->dstat.st_mtime;	/* the default time */
	if (all_fmt & TIME_ACCESS)
		ttime = dn->dstat.st_atime;
	if (all_fmt & TIME_CHANGE)
		ttime = dn->dstat.st_ctime;
	filetime = ctime(&ttime);
#endif
#if ENABLE_FEATURE_LS_FILETYPES
	append = append_char(dn->dstat.st_mode);
#endif

	for (i = 0; i <= 31; i++) {
		switch (all_fmt & (1 << i)) {
		case LIST_INO:
			column += printf("%7ld ", (long) dn->dstat.st_ino);
			break;
		case LIST_BLOCKS:
			column += printf("%4"OFF_FMT"d ", (off_t) dn->dstat.st_blocks >> 1);
			break;
		case LIST_MODEBITS:
			column += printf("%-10s ", (char *) bb_mode_string(dn->dstat.st_mode));
			break;
		case LIST_NLINKS:
			column += printf("%4ld ", (long) dn->dstat.st_nlink);
			break;
		case LIST_ID_NAME:
#if ENABLE_FEATURE_LS_USERNAME
			printf("%-8.8s %-8.8s",
				get_cached_username(dn->dstat.st_uid),
				get_cached_groupname(dn->dstat.st_gid));
			column += 17;
			break;
#endif
		case LIST_ID_NUMERIC:
			column += printf("%-8d %-8d", dn->dstat.st_uid, dn->dstat.st_gid);
			break;
		case LIST_SIZE:
		case LIST_DEV:
			if (S_ISBLK(dn->dstat.st_mode) || S_ISCHR(dn->dstat.st_mode)) {
				column += printf("%4d, %3d ", (int) major(dn->dstat.st_rdev),
					   (int) minor(dn->dstat.st_rdev));
			} else {
				if (all_fmt & LS_DISP_HR) {
					column += printf("%9s ",
						make_human_readable_str(dn->dstat.st_size, 1, 0));
				} else {
					column += printf("%9"OFF_FMT"d ", (off_t) dn->dstat.st_size);
				}
			}
			break;
#if ENABLE_FEATURE_LS_TIMESTAMPS
		case LIST_FULLTIME:
			printf("%24.24s ", filetime);
			column += 25;
			break;
		case LIST_DATE_TIME:
			if ((all_fmt & LIST_FULLTIME) == 0) {
				/* current_time_t ~== time(NULL) */
				age = current_time_t - ttime;
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
#if ENABLE_SELINUX
		case LIST_CONTEXT:
			{
				char context[80];
				int len = 0;

				if (dn->sid) {
					/* I assume sid initilized with NULL */
					len = strlen(dn->sid) + 1;
					safe_strncpy(context, dn->sid, len);
					freecon(dn->sid);
				} else {
					safe_strncpy(context, "unknown", 8);
				}
				printf("%-32s ", context);
				column += MAX(33, len);
			}
			break;
#endif
		case LIST_FILENAME:
			errno = 0;
#if ENABLE_FEATURE_LS_COLOR
			if (show_color && !lstat(dn->fullname, &info)) {
				printf("\033[%d;%dm", bgcolor(info.st_mode),
						fgcolor(info.st_mode));
			}
#endif
			column += printf("%s", dn->name);
			if (show_color) {
				printf("\033[0m");
			}
			break;
		case LIST_SYMLINK:
			if (S_ISLNK(dn->dstat.st_mode)) {
				char *lpath = xmalloc_readlink_or_warn(dn->fullname);
				if (!lpath) break;
				printf(" -> ");
#if ENABLE_FEATURE_LS_FILETYPES || ENABLE_FEATURE_LS_COLOR
				if (!stat(dn->fullname, &info)) {
					append = append_char(info.st_mode);
				}
#endif
#if ENABLE_FEATURE_LS_COLOR
				if (show_color) {
					errno = 0;
					printf("\033[%d;%dm", bgcolor(info.st_mode),
						   fgcolor(info.st_mode));
				}
#endif
				column += printf("%s", lpath) + 4;
				if (show_color) {
					printf("\033[0m");
				}
				free(lpath);
			}
			break;
#if ENABLE_FEATURE_LS_FILETYPES
		case LIST_FILETYPE:
			if (append) {
				putchar(append);
				column++;
			}
			break;
#endif
		}
	}

	return column;
}

/* "[-]Cadil1", POSIX mandated options, busybox always supports */
/* "[-]gnsx", POSIX non-mandated options, busybox always supports */
/* "[-]Ak" GNU options, busybox always supports */
/* "[-]FLRctur", POSIX mandated options, busybox optionally supports */
/* "[-]p", POSIX non-mandated options, busybox optionally supports */
/* "[-]SXvThw", GNU options, busybox optionally supports */
/* "[-]K", SELinux mandated options, busybox optionally supports */
/* "[-]e", I think we made this one up */
static const char ls_options[] = "Cadil1gnsxAk"
	USE_FEATURE_LS_TIMESTAMPS("cetu")
	USE_FEATURE_LS_SORTFILES("SXrv")
	USE_FEATURE_LS_FILETYPES("Fp")
	USE_FEATURE_LS_FOLLOWLINKS("L")
	USE_FEATURE_LS_RECURSIVE("R")
	USE_FEATURE_HUMAN_READABLE("h")
	USE_SELINUX("K")
	USE_FEATURE_AUTOWIDTH("T:w:")
	USE_SELINUX("Z");

enum {
	LIST_MASK_TRIGGER	= 0,
	STYLE_MASK_TRIGGER	= STYLE_MASK,
	DISP_MASK_TRIGGER	= DISP_ROWS,
	SORT_MASK_TRIGGER	= SORT_MASK,
};

static const unsigned opt_flags[] = {
	LIST_SHORT | STYLE_COLUMNS, /* C */
	DISP_HIDDEN | DISP_DOT,     /* a */
	DISP_NOLIST,                /* d */
	LIST_INO,                   /* i */
	LIST_LONG | STYLE_LONG,     /* l - remember LS_DISP_HR in mask! */
	LIST_SHORT | STYLE_SINGLE,  /* 1 */
	0,                          /* g - ingored */
	LIST_ID_NUMERIC,            /* n */
	LIST_BLOCKS,                /* s */
	DISP_ROWS,                  /* x */
	DISP_HIDDEN,                /* A */
	ENABLE_SELINUX * LIST_CONTEXT, /* k (ignored if !SELINUX) */
#if ENABLE_FEATURE_LS_TIMESTAMPS
	TIME_CHANGE | (ENABLE_FEATURE_LS_SORTFILES * SORT_CTIME),   /* c */
	LIST_FULLTIME,              /* e */
	ENABLE_FEATURE_LS_SORTFILES * SORT_MTIME,   /* t */
	TIME_ACCESS | (ENABLE_FEATURE_LS_SORTFILES * SORT_ATIME),   /* u */
#endif
#if ENABLE_FEATURE_LS_SORTFILES
	SORT_SIZE,                  /* S */
	SORT_EXT,                   /* X */
	SORT_REVERSE,               /* r */
	SORT_VERSION,               /* v */
#endif
#if ENABLE_FEATURE_LS_FILETYPES
	LIST_FILETYPE | LIST_EXEC,  /* F */
	LIST_FILETYPE,              /* p */
#endif
#if ENABLE_FEATURE_LS_FOLLOWLINKS
	FOLLOW_LINKS,               /* L */
#endif
#if ENABLE_FEATURE_LS_RECURSIVE
	DISP_RECURSIVE,             /* R */
#endif
#if ENABLE_FEATURE_HUMAN_READABLE
	LS_DISP_HR,                 /* h */
#endif
#if ENABLE_SELINUX
	LIST_MODEBITS|LIST_NLINKS|LIST_CONTEXT|LIST_SIZE|LIST_DATE_TIME, /* K */
#endif
#if ENABLE_FEATURE_AUTOWIDTH
	0, 0,                       /* T, w - ignored */
#endif
#if ENABLE_SELINUX
	LIST_MODEBITS|LIST_ID_NAME|LIST_CONTEXT, /* Z */
#endif
	(1U<<31)
};


/* THIS IS A "SAFE" APPLET, main() MAY BE CALLED INTERNALLY FROM SHELL */
/* BE CAREFUL! */

int ls_main(int argc, char **argv);
int ls_main(int argc, char **argv)
{
	struct dnode **dnd;
	struct dnode **dnf;
	struct dnode **dnp;
	struct dnode *dn;
	struct dnode *cur;
	unsigned opt;
	int nfiles = 0;
	int dnfiles;
	int dndirs;
	int oi;
	int ac;
	int i;
	char **av;
	USE_FEATURE_AUTOWIDTH(char *tabstops_str = NULL;)
	USE_FEATURE_AUTOWIDTH(char *terminal_width_str = NULL;)
	USE_FEATURE_LS_COLOR(char *color_opt;)

#if ENABLE_FEATURE_LS_TIMESTAMPS
	time(&current_time_t);
#endif

	all_fmt = LIST_SHORT |
		(ENABLE_FEATURE_LS_SORTFILES * (SORT_NAME | SORT_FORWARD));

#if ENABLE_FEATURE_AUTOWIDTH
	/* Obtain the terminal width */
	get_terminal_width_height(STDOUT_FILENO, &terminal_width, NULL);
	/* Go one less... */
	terminal_width--;
#endif

	/* process options */
	USE_FEATURE_LS_COLOR(applet_long_options = ls_color_opt;)
#if ENABLE_FEATURE_AUTOWIDTH
	opt = getopt32(argc, argv, ls_options, &tabstops_str, &terminal_width_str
				USE_FEATURE_LS_COLOR(, &color_opt));
	if (tabstops_str)
		tabstops = xatou(tabstops_str);
	if (terminal_width_str)
		terminal_width = xatou(terminal_width_str);
#else
	opt = getopt32(argc, argv, ls_options USE_FEATURE_LS_COLOR(, &color_opt));
#endif
	for (i = 0; opt_flags[i] != (1U<<31); i++) {
		if (opt & (1 << i)) {
			unsigned flags = opt_flags[i];

			if (flags & LIST_MASK_TRIGGER)
				all_fmt &= ~LIST_MASK;
			if (flags & STYLE_MASK_TRIGGER)
				all_fmt &= ~STYLE_MASK;
			if (flags & SORT_MASK_TRIGGER)
				all_fmt &= ~SORT_MASK;
			if (flags & DISP_MASK_TRIGGER)
				all_fmt &= ~DISP_MASK;
			if (flags & TIME_MASK)
				all_fmt &= ~TIME_MASK;
			if (flags & LIST_CONTEXT)
				all_fmt |= STYLE_SINGLE;
			/* huh?? opt cannot be 'l' */
			//if (LS_DISP_HR && opt == 'l')
			//	all_fmt &= ~LS_DISP_HR;
			all_fmt |= flags;
		}
	}

#if ENABLE_FEATURE_LS_COLOR
	/* find color bit value - last position for short getopt */
	if (ENABLE_FEATURE_LS_COLOR_IS_DEFAULT && isatty(STDOUT_FILENO)) {
		char *p = getenv("LS_COLORS");
		/* LS_COLORS is unset, or (not empty && not "none") ? */
		if (!p || (p[0] && strcmp(p, "none")))
			show_color = 1;
	}
	if (opt & (1 << i)) {  /* next flag after short options */
		if (!color_opt || !strcmp("always", color_opt))
			show_color = 1;
		else if (color_opt && !strcmp("never", color_opt))
			show_color = 0;
		else if (color_opt && !strcmp("auto", color_opt) && isatty(STDOUT_FILENO))
			show_color = 1;
	}
#endif

	/* sort out which command line options take precedence */
	if (ENABLE_FEATURE_LS_RECURSIVE && (all_fmt & DISP_NOLIST))
		all_fmt &= ~DISP_RECURSIVE;	/* no recurse if listing only dir */
	if (ENABLE_FEATURE_LS_TIMESTAMPS && ENABLE_FEATURE_LS_SORTFILES) {
		if (all_fmt & TIME_CHANGE)
			all_fmt = (all_fmt & ~SORT_MASK) | SORT_CTIME;
		if (all_fmt & TIME_ACCESS)
			all_fmt = (all_fmt & ~SORT_MASK) | SORT_ATIME;
	}
	if ((all_fmt & STYLE_MASK) != STYLE_LONG) /* only for long list */
		all_fmt &= ~(LIST_ID_NUMERIC|LIST_FULLTIME|LIST_ID_NAME|LIST_ID_NUMERIC);
	if (ENABLE_FEATURE_LS_USERNAME)
		if ((all_fmt & STYLE_MASK) == STYLE_LONG && (all_fmt & LIST_ID_NUMERIC))
			all_fmt &= ~LIST_ID_NAME; /* don't list names if numeric uid */

	/* choose a display format */
	if (!(all_fmt & STYLE_MASK))
		all_fmt |= (isatty(STDOUT_FILENO) ? STYLE_COLUMNS : STYLE_SINGLE);

	/*
	 * when there are no cmd line args we have to supply a default "." arg.
	 * we will create a second argv array, "av" that will hold either
	 * our created "." arg, or the real cmd line args.  The av array
	 * just holds the pointers- we don't move the date the pointers
	 * point to.
	 */
	ac = argc - optind;	/* how many cmd line args are left */
	if (ac < 1) {
		static const char *const dotdir[] = { "." };

		av = (char **) dotdir;
		ac = 1;
	} else {
		av = argv + optind;
	}

	/* now, everything is in the av array */
	if (ac > 1)
		all_fmt |= DISP_DIRNAME;	/* 2 or more items? label directories */

	/* stuff the command line file names into a dnode array */
	dn = NULL;
	for (oi = 0; oi < ac; oi++) {
		/* ls w/o -l follows links on command line */
		cur = my_stat(av[oi], av[oi], !(all_fmt & STYLE_LONG));
		if (!cur)
			continue;
		cur->allocated = 0;
		cur->next = dn;
		dn = cur;
		nfiles++;
	}

	/* now that we know how many files there are
	 * allocate memory for an array to hold dnode pointers
	 */
	dnp = dnalloc(nfiles);
	for (i = 0, cur = dn; i < nfiles; i++) {
		dnp[i] = cur;	/* save pointer to node in array */
		cur = cur->next;
	}

	if (all_fmt & DISP_NOLIST) {
		dnsort(dnp, nfiles);
		if (nfiles > 0)
			showfiles(dnp, nfiles);
	} else {
		dnd = splitdnarray(dnp, nfiles, SPLIT_DIR);
		dnf = splitdnarray(dnp, nfiles, SPLIT_FILE);
		dndirs = countdirs(dnp, nfiles);
		dnfiles = nfiles - dndirs;
		if (dnfiles > 0) {
			dnsort(dnf, dnfiles);
			showfiles(dnf, dnfiles);
			if (ENABLE_FEATURE_CLEAN_UP)
				free(dnf);
		}
		if (dndirs > 0) {
			dnsort(dnd, dndirs);
			showdirs(dnd, dndirs, dnfiles == 0);
			if (ENABLE_FEATURE_CLEAN_UP)
				free(dnd);
		}
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		dfree(dnp, nfiles);
	return status;
}
