/* vi: set sw=4 ts=4: */
/*
 * tiny-ls.c version 0.1.0: A minimalist 'ls'
 * Copyright (C) 1996 Brian Candler <B.Candler@pobox.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* [date unknown. Perhaps before year 2000]
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
 * 2. hidden files can make column width too large
 *
 * NON-OPTIMAL BEHAVIOUR:
 * 1. autowidth reads directories twice
 * 2. if you do a short directory listing without filetype characters
 *    appended, there's no need to stat each one
 * PORTABILITY:
 * 1. requires lstat (BSD) - how do you do it without?
 *
 * [2009-03]
 * ls sorts listing now, and supports almost all options.
 */

#include "libbb.h"

#if ENABLE_FEATURE_ASSUME_UNICODE
#include <wchar.h>
#endif

/* This is a NOEXEC applet. Be very careful! */


#if ENABLE_FTPD
/* ftpd uses ls, and without timestamps Mozilla won't understand
 * ftpd's LIST output.
 */
# undef CONFIG_FEATURE_LS_TIMESTAMPS
# undef ENABLE_FEATURE_LS_TIMESTAMPS
# undef USE_FEATURE_LS_TIMESTAMPS
# undef SKIP_FEATURE_LS_TIMESTAMPS
# define CONFIG_FEATURE_LS_TIMESTAMPS 1
# define ENABLE_FEATURE_LS_TIMESTAMPS 1
# define USE_FEATURE_LS_TIMESTAMPS(...) __VA_ARGS__
# define SKIP_FEATURE_LS_TIMESTAMPS(...)
#endif


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
//LIST_DEV        = 1 << 8, - unused, synonym to LIST_SIZE
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

/* "[-]Cadil1", POSIX mandated options, busybox always supports */
/* "[-]gnsx", POSIX non-mandated options, busybox always supports */
/* "[-]Q" GNU option? busybox always supports */
/* "[-]Ak" GNU options, busybox always supports */
/* "[-]FLRctur", POSIX mandated options, busybox optionally supports */
/* "[-]p", POSIX non-mandated options, busybox optionally supports */
/* "[-]SXvThw", GNU options, busybox optionally supports */
/* "[-]K", SELinux mandated options, busybox optionally supports */
/* "[-]e", I think we made this one up */
static const char ls_options[] ALIGN1 =
	"Cadil1gnsxQAk" /* 13 opts, total 13 */
	USE_FEATURE_LS_TIMESTAMPS("cetu") /* 4, 17 */
	USE_FEATURE_LS_SORTFILES("SXrv")  /* 4, 21 */
	USE_FEATURE_LS_FILETYPES("Fp")    /* 2, 23 */
	USE_FEATURE_LS_FOLLOWLINKS("L")   /* 1, 24 */
	USE_FEATURE_LS_RECURSIVE("R")     /* 1, 25 */
	USE_FEATURE_HUMAN_READABLE("h")   /* 1, 26 */
	USE_SELINUX("K") /* 1, 27 */
	USE_SELINUX("Z") /* 1, 28 */
	USE_FEATURE_AUTOWIDTH("T:w:") /* 2, 30 */
	;
enum {
	//OPT_C = (1 << 0),
	//OPT_a = (1 << 1),
	//OPT_d = (1 << 2),
	//OPT_i = (1 << 3),
	//OPT_l = (1 << 4),
	//OPT_1 = (1 << 5),
	OPT_g = (1 << 6),
	//OPT_n = (1 << 7),
	//OPT_s = (1 << 8),
	//OPT_x = (1 << 9),
	OPT_Q = (1 << 10),
	//OPT_A = (1 << 11),
	//OPT_k = (1 << 12),
};

enum {
	LIST_MASK_TRIGGER	= 0,
	STYLE_MASK_TRIGGER	= STYLE_MASK,
	DISP_MASK_TRIGGER	= DISP_ROWS,
	SORT_MASK_TRIGGER	= SORT_MASK,
};

/* TODO: simple toggles may be stored as OPT_xxx bits instead */
static const unsigned opt_flags[] = {
	LIST_SHORT | STYLE_COLUMNS, /* C */
	DISP_HIDDEN | DISP_DOT,     /* a */
	DISP_NOLIST,                /* d */
	LIST_INO,                   /* i */
	LIST_LONG | STYLE_LONG,     /* l - remember LS_DISP_HR in mask! */
	LIST_SHORT | STYLE_SINGLE,  /* 1 */
	0,                          /* g (don't show group) - handled via OPT_g */
	LIST_ID_NUMERIC,            /* n */
	LIST_BLOCKS,                /* s */
	DISP_ROWS,                  /* x */
	0,                          /* Q (quote filename) - handled via OPT_Q */
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
#if ENABLE_SELINUX
	LIST_MODEBITS|LIST_ID_NAME|LIST_CONTEXT, /* Z */
#endif
	(1U<<31)
	/* options after Z are not processed through opt_flags:
	 * T, w - ignored
	 */
};


/*
 * a directory entry and its stat info are stored here
 */
struct dnode {                  /* the basic node */
	const char *name;             /* the dir entry name */
	const char *fullname;         /* the dir entry name */
	int   allocated;
	struct stat dstat;      /* the file stat info */
	USE_SELINUX(security_context_t sid;)
	struct dnode *next;     /* point at the next node */
};

static struct dnode **list_dir(const char *);
static struct dnode **dnalloc(int);
static int list_single(const struct dnode *);


struct globals {
#if ENABLE_FEATURE_LS_COLOR
	smallint show_color;
#endif
	smallint exit_code;
	unsigned all_fmt;
#if ENABLE_FEATURE_AUTOWIDTH
	unsigned tabstops; // = COLUMN_GAP;
	unsigned terminal_width; // = TERMINAL_WIDTH;
#endif
#if ENABLE_FEATURE_LS_TIMESTAMPS
	/* Do time() just once. Saves one syscall per file for "ls -l" */
	time_t current_time_t;
#endif
};
#define G (*(struct globals*)&bb_common_bufsiz1)
#if ENABLE_FEATURE_LS_COLOR
#define show_color     (G.show_color    )
#else
enum { show_color = 0 };
#endif
#define exit_code      (G.exit_code     )
#define all_fmt        (G.all_fmt       )
#if ENABLE_FEATURE_AUTOWIDTH
#define tabstops       (G.tabstops      )
#define terminal_width (G.terminal_width)
#else
enum {
	tabstops = COLUMN_GAP,
	terminal_width = TERMINAL_WIDTH,
};
#endif
#define current_time_t (G.current_time_t)
/* memset: we have to zero it out because of NOEXEC */
#define INIT_G() do { \
	memset(&G, 0, sizeof(G)); \
	USE_FEATURE_AUTOWIDTH(tabstops = COLUMN_GAP;) \
	USE_FEATURE_AUTOWIDTH(terminal_width = TERMINAL_WIDTH;) \
	USE_FEATURE_LS_TIMESTAMPS(time(&current_time_t);) \
} while (0)


#if ENABLE_FEATURE_ASSUME_UNICODE
/* libbb candidate */
static size_t mbstrlen(const char *string)
{
	size_t width = mbsrtowcs(NULL /*dest*/, &string,
				MAXINT(size_t) /*len*/, NULL /*state*/);
	if (width == (size_t)-1)
		return strlen(string);
	return width;
}
#else
#define mbstrlen(string) strlen(string)
#endif


static struct dnode *my_stat(const char *fullname, const char *name, int force_follow)
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
			bb_simple_perror_msg(fullname);
			exit_code = EXIT_FAILURE;
			return 0;
		}
	} else {
#if ENABLE_SELINUX
		if (is_selinux_enabled()) {
			lgetfilecon(fullname, &sid);
		}
#endif
		if (lstat(fullname, &dstat)) {
			bb_simple_perror_msg(fullname);
			exit_code = EXIT_FAILURE;
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


/* FYI type values: 1:fifo 2:char 4:dir 6:blk 8:file 10:link 12:socket
 * (various wacky OSes: 13:Sun door 14:BSD whiteout 5:XENIX named file
 *  3/7:multiplexed char/block device)
 * and we use 0 for unknown and 15 for executables (see below) */
#define TYPEINDEX(mode) (((mode) >> 12) & 0x0f)
#define TYPECHAR(mode)  ("0pcCd?bB-?l?s???" [TYPEINDEX(mode)])
#define APPCHAR(mode)   ("\0|\0\0/\0\0\0\0\0@\0=\0\0\0" [TYPEINDEX(mode)])
/* 036 black foreground              050 black background
   037 red foreground                051 red background
   040 green foreground              052 green background
   041 brown foreground              053 brown background
   042 blue foreground               054 blue background
   043 magenta (purple) foreground   055 magenta background
   044 cyan (light blue) foreground  056 cyan background
   045 gray foreground               057 white background
*/
#define COLOR(mode) ( \
	/*un  fi  chr     dir     blk     file    link    sock        exe */ \
	"\037\043\043\045\042\045\043\043\000\045\044\045\043\045\045\040" \
	[TYPEINDEX(mode)])
/* Select normal (0) [actually "reset all"] or bold (1)
 * (other attributes are 2:dim 4:underline 5:blink 7:reverse,
 *  let's use 7 for "impossible" types, just for fun)
 * Note: coreutils 6.9 uses inverted red for setuid binaries.
 */
#define ATTR(mode) ( \
	/*un fi chr   dir   blk   file  link  sock     exe */ \
	"\01\00\01\07\01\07\01\07\00\07\01\07\01\07\07\01" \
	[TYPEINDEX(mode)])

#if ENABLE_FEATURE_LS_COLOR
/* mode of zero is interpreted as "unknown" (stat failed) */
static char fgcolor(mode_t mode)
{
	if (S_ISREG(mode) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
		return COLOR(0xF000);	/* File is executable ... */
	return COLOR(mode);
}
static char bold(mode_t mode)
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
		const char *name;
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
			free((char*)cur->fullname);	/* free the filename */
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
			const char *name;
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
			int len = mbstrlen(dn[i]->name);
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
				bb_putchar('\n');
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
		exit_code = EXIT_FAILURE;
		return NULL;	/* could not open the dir */
	}
	while ((entry = readdir(dir)) != NULL) {
		char *fullname;

		/* are we going to list the file- it may be . or .. or a hidden file */
		if (entry->d_name[0] == '.') {
			if ((!entry->d_name[1] || (entry->d_name[1] == '.' && !entry->d_name[2]))
			 && !(all_fmt & DISP_DOT)
			) {
				continue;
			}
			if (!(all_fmt & DISP_HIDDEN))
				continue;
		}
		fullname = concat_path_file(path, entry->d_name);
		cur = my_stat(fullname, bb_basename(fullname), 0);
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


static int print_name(const char *name)
{
	if (option_mask32 & OPT_Q) {
#if ENABLE_FEATURE_ASSUME_UNICODE
		int len = 2 + mbstrlen(name);
#else
		int len = 2;
#endif
		putchar('"');
		while (*name) {
			if (*name == '"') {
				putchar('\\');
				len++;
			}
			putchar(*name++);
			if (!ENABLE_FEATURE_ASSUME_UNICODE)
				len++;
		}
		putchar('"');
		return len;
	}
	/* No -Q: */
#if ENABLE_FEATURE_ASSUME_UNICODE
	fputs(name, stdout);
	return mbstrlen(name);
#else
	return printf("%s", name);
#endif
}


static int list_single(const struct dnode *dn)
{
	int column = 0;
	char *lpath = lpath; /* for compiler */
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

	/* Do readlink early, so that if it fails, error message
	 * does not appear *inside* of the "ls -l" line */
	if (all_fmt & LIST_SYMLINK)
		if (S_ISLNK(dn->dstat.st_mode))
			lpath = xmalloc_readlink_or_warn(dn->fullname);

	if (all_fmt & LIST_INO)
		column += printf("%7lu ", (long) dn->dstat.st_ino);
	if (all_fmt & LIST_BLOCKS)
		column += printf("%4"OFF_FMT"u ", (off_t) dn->dstat.st_blocks >> 1);
	if (all_fmt & LIST_MODEBITS)
		column += printf("%-10s ", (char *) bb_mode_string(dn->dstat.st_mode));
	if (all_fmt & LIST_NLINKS)
		column += printf("%4lu ", (long) dn->dstat.st_nlink);
#if ENABLE_FEATURE_LS_USERNAME
	if (all_fmt & LIST_ID_NAME) {
		if (option_mask32 & OPT_g) {
			column += printf("%-8.8s",
				get_cached_username(dn->dstat.st_uid));
		} else {
			column += printf("%-8.8s %-8.8s",
				get_cached_username(dn->dstat.st_uid),
				get_cached_groupname(dn->dstat.st_gid));
		}
	}
#endif
	if (all_fmt & LIST_ID_NUMERIC) {
		if (option_mask32 & OPT_g)
			column += printf("%-8u", (int) dn->dstat.st_uid);
		else
			column += printf("%-8u %-8u",
					(int) dn->dstat.st_uid,
					(int) dn->dstat.st_gid);
	}
	if (all_fmt & (LIST_SIZE /*|LIST_DEV*/ )) {
		if (S_ISBLK(dn->dstat.st_mode) || S_ISCHR(dn->dstat.st_mode)) {
			column += printf("%4u, %3u ",
					(int) major(dn->dstat.st_rdev),
					(int) minor(dn->dstat.st_rdev));
		} else {
			if (all_fmt & LS_DISP_HR) {
				column += printf("%9s ",
					make_human_readable_str(dn->dstat.st_size, 1, 0));
			} else {
				column += printf("%9"OFF_FMT"u ", (off_t) dn->dstat.st_size);
			}
		}
	}
#if ENABLE_FEATURE_LS_TIMESTAMPS
	if (all_fmt & LIST_FULLTIME)
		column += printf("%24.24s ", filetime);
	if (all_fmt & LIST_DATE_TIME)
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
#endif
#if ENABLE_SELINUX
	if (all_fmt & LIST_CONTEXT) {
		column += printf("%-32s ", dn->sid ? dn->sid : "unknown");
		freecon(dn->sid);
	}
#endif
	if (all_fmt & LIST_FILENAME) {
#if ENABLE_FEATURE_LS_COLOR
		if (show_color) {
			info.st_mode = 0; /* for fgcolor() */
			lstat(dn->fullname, &info);
			printf("\033[%u;%um", bold(info.st_mode),
					fgcolor(info.st_mode));
		}
#endif
		column += print_name(dn->name);
		if (show_color) {
			printf("\033[0m");
		}
	}
	if (all_fmt & LIST_SYMLINK) {
		if (S_ISLNK(dn->dstat.st_mode) && lpath) {
			printf(" -> ");
#if ENABLE_FEATURE_LS_FILETYPES || ENABLE_FEATURE_LS_COLOR
#if ENABLE_FEATURE_LS_COLOR
			info.st_mode = 0; /* for fgcolor() */
#endif
			if (stat(dn->fullname, &info) == 0) {
				append = append_char(info.st_mode);
			}
#endif
#if ENABLE_FEATURE_LS_COLOR
			if (show_color) {
				printf("\033[%u;%um", bold(info.st_mode),
					   fgcolor(info.st_mode));
			}
#endif
			column += print_name(lpath) + 4;
			if (show_color) {
				printf("\033[0m");
			}
			free(lpath);
		}
	}
#if ENABLE_FEATURE_LS_FILETYPES
	if (all_fmt & LIST_FILETYPE) {
		if (append) {
			putchar(append);
			column++;
		}
	}
#endif

	return column;
}


/* colored LS support by JaWi, janwillem.janssen@lxtreme.nl */
#if ENABLE_FEATURE_LS_COLOR
/* long option entry used only for --color, which has no short option
 * equivalent */
static const char ls_color_opt[] ALIGN1 =
	"color\0" Optional_argument "\xff" /* no short equivalent */
	;
#endif


int ls_main(int argc UNUSED_PARAM, char **argv)
{
	struct dnode **dnd;
	struct dnode **dnf;
	struct dnode **dnp;
	struct dnode *dn;
	struct dnode *cur;
	unsigned opt;
	int nfiles;
	int dnfiles;
	int dndirs;
	int i;
	/* need to initialize since --color has _an optional_ argument */
	USE_FEATURE_LS_COLOR(const char *color_opt = "always";)

	INIT_G();

	all_fmt = LIST_SHORT |
		(ENABLE_FEATURE_LS_SORTFILES * (SORT_NAME | SORT_FORWARD));

#if ENABLE_FEATURE_AUTOWIDTH
	/* Obtain the terminal width */
	get_terminal_width_height(STDIN_FILENO, &terminal_width, NULL);
	/* Go one less... */
	terminal_width--;
#endif

	/* process options */
	USE_FEATURE_LS_COLOR(applet_long_options = ls_color_opt;)
#if ENABLE_FEATURE_AUTOWIDTH
	opt_complementary = "T+:w+"; /* -T N, -w N */
	opt = getopt32(argv, ls_options, &tabstops, &terminal_width
				USE_FEATURE_LS_COLOR(, &color_opt));
#else
	opt = getopt32(argv, ls_options USE_FEATURE_LS_COLOR(, &color_opt));
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
		if (!p || (p[0] && strcmp(p, "none") != 0))
			show_color = 1;
	}
	if (opt & (1 << i)) {  /* next flag after short options */
		if (strcmp("always", color_opt) == 0)
			show_color = 1;
		else if (strcmp("never", color_opt) == 0)
			show_color = 0;
		else if (strcmp("auto", color_opt) == 0 && isatty(STDOUT_FILENO))
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

	argv += optind;
	if (!argv[0])
		*--argv = (char*)".";

	if (argv[1])
		all_fmt |= DISP_DIRNAME; /* 2 or more items? label directories */

	/* stuff the command line file names into a dnode array */
	dn = NULL;
	nfiles = 0;
	do {
		/* NB: follow links on command line unless -l or -s */
		cur = my_stat(*argv, *argv, !(all_fmt & (STYLE_LONG|LIST_BLOCKS)));
		argv++;
		if (!cur)
			continue;
		cur->allocated = 0;
		cur->next = dn;
		dn = cur;
		nfiles++;
	} while (*argv);

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
	return exit_code;
}
