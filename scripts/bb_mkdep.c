/*
 * Another fast dependencies generator for Makefiles, Version 3.0
 *
 * Copyright (C) 2005 by Vladimir Oleynik <dzo@simtreas.ru>
 *
 * mmaping file may be originally by Linus Torvalds.
 *
 * bb_simplify_path()
 *      Copyright (C) 2005 Manuel Novoa III <mjn3@codepoet.org>
 *
 * xmalloc() bb_xstrdup() bb_error_d():
 *      Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * llist routine
 *      Copyright (C) 2003 Glenn McGrath
 *      Copyright (C) Vladimir Oleynik <dzo@simtreas.ru>
 *
 * (c) 2005 Bernhard Fischer:
 *  - commentary typos,
 *  - move "memory exhausted" into msg_enomem,
 *  - more verbose --help output.
 *
 * This program does:
 * 1) find #define KEY VALUE or #undef KEY from include/config.h
 * 2) recursive find and scan *.[ch] files, but skips scan of include/config/
 * 3) find #include "*.h" and KEYs using, if not as #define and #undef
 * 4) generate dependencies to stdout
 *    pwd/file.o: include/config/key*.h found_include_*.h
 *    path/inc.h: include/config/key*.h found_included_include_*.h
 * 5) save include/config/key*.h if changed after previous usage
 * This program does not generate dependencies for #include <...>
 */

#define LOCAL_INCLUDE_PATH          "include"
#define INCLUDE_CONFIG_PATH         LOCAL_INCLUDE_PATH"/config"
#define INCLUDE_CONFIG_KEYS_PATH    LOCAL_INCLUDE_PATH"/config.h"

#define bb_mkdep_full_options \
"\nOptions:" \
"\n\t-I local_include_path  include paths, default: \"" LOCAL_INCLUDE_PATH "\"" \
"\n\t-d                     don't generate depend" \
"\n\t-w                     show warning if include files not found" \
"\n\t-k include/config      default: \"" INCLUDE_CONFIG_PATH "\"" \
"\n\t-c include/config.h    configs, default: \"" INCLUDE_CONFIG_KEYS_PATH "\"" \
"\n\tdirs_to_scan           default \".\""

#define bb_mkdep_terse_options "Usage: [-I local_include_paths] [-dw] " \
		   "[-k path_for_stored_keys] [dirs]"


#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <getopt.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>


/* partial and simplified libbb routine */
static void bb_error_d(const char *s, ...) __attribute__ ((noreturn, format (printf, 1, 2)));
static char * bb_asprint(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
static char *bb_simplify_path(const char *path);

/* stolen from libbb as is */
typedef struct llist_s {
	char *data;
	struct llist_s *link;
} llist_t;

static const char msg_enomem[] = "memory exhausted";

/* inline realization for fast works */
static inline void *xmalloc(size_t size)
{
	void *p = malloc(size);

	if(p == NULL)
		bb_error_d(msg_enomem);
	return p;
}

static inline char *bb_xstrdup(const char *s)
{
	char *r = strdup(s);

	if(r == NULL)
	    bb_error_d(msg_enomem);
	return r;
}


static int dontgenerate_dep;    /* flag -d usaged */
static int noiwarning;          /* flag -w usaged */
static llist_t *configs;    /* list of -c usaged and them stat() after parsed */
static llist_t *Iop;        /* list of -I include usaged */

static char *pwd;           /* current work directory */
static size_t replace;      /* replace current work derectory to build dir */

static const char *kp;      /* KEY path, argument of -k usaged */
static size_t kp_len;
static struct stat st_kp;   /* stat(kp) */

typedef struct BB_KEYS {
	char *keyname;
	size_t key_sz;
	const char *value;
	const char *checked;
	char *stored_path;
	const char *src_have_this_key;
	struct BB_KEYS *next;
} bb_key_t;

static bb_key_t *key_top;   /* list of found KEYs */
static bb_key_t *Ifound;    /* list of parsed includes */


static void parse_conf_opt(const char *opt, const char *val, size_t key_sz);
static void parse_inc(const char *include, const char *fname);

static inline bb_key_t *check_key(bb_key_t *k, const char *nk, size_t key_sz)
{
    bb_key_t *cur;

    for(cur = k; cur; cur = cur->next) {
	if(key_sz == cur->key_sz && memcmp(cur->keyname, nk, key_sz) == 0) {
	    cur->checked = cur->stored_path;
	    return cur;
	}
    }
    return NULL;
}

/* for lexical analyser */
static int pagesizem1;      /* padding mask = getpagesize() - 1 */

/* for speed tricks */
static char first_chars[1+UCHAR_MAX];               /* + L_EOF */
static char isalnums[1+UCHAR_MAX];                  /* + L_EOF */
/* trick for fast find "define", "include", "undef" */
static const char first_chars_diu[UCHAR_MAX] = {
	[(int)'d'] = (char)5,           /* strlen("define")  - 1; */
	[(int)'i'] = (char)6,           /* strlen("include") - 1; */
	[(int)'u'] = (char)4,           /* strlen("undef")   - 1; */
};

#define CONFIG_MODE  0
#define SOURCES_MODE 1
static int mode;

#define yy_error_d(s) bb_error_d("%s:%d hmm, %s", fname, line, s)

/* state */
#define S      0        /* start state */
#define STR    '"'      /* string */
#define CHR    '\''     /* char */
#define REM    '/'      /* block comment */
#define BS     '\\'     /* back slash */
#define POUND  '#'      /* # */
#define D      '5'      /* #define preprocessor's directive */
#define I      '6'      /* #include preprocessor's directive */
#define U      '4'      /* #undef preprocessor's directive */
#define DK     'K'      /* #define KEY... (config mode) */
#define ANY    '*'      /* any unparsed chars */

/* [A-Z_a-z] */
#define ID(c) ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_')
/* [A-Z_a-z0-9] */
#define ISALNUM(c)  (ID(c) || (c >= '0' && c <= '9'))

#define L_EOF  (1+UCHAR_MAX)

#define getc0()     do { c = (optr >= oend) ? L_EOF : *optr++; } while(0)

#define getc1()     do { getc0(); if(c == BS) { getc0(); \
				    if(c == '\n') { line++; continue; } \
				    else { optr--; c = BS; } \
				  } break; } while(1)

static char id_s[4096];
#define put_id(ic)  do {    if(id_len == sizeof(id_s)) goto too_long; \
				id[id_len++] = ic; } while(0)


/* stupid C lexical analyser */
static void c_lex(const char *fname, long fsize)
{
  int c;
  int state;
  int line;
  char *id = id_s;
  size_t id_len = 0;                /* stupid initialize */
  unsigned char *optr, *oend;

  int fd;
  char *map;
  int mapsize;

  if(fsize == 0) {
    fprintf(stderr, "Warning: %s is empty\n", fname);
    return;
  }
  fd = open(fname, O_RDONLY);
  if(fd < 0) {
	perror(fname);
	return;
  }
  mapsize = (fsize+pagesizem1) & ~pagesizem1;
  map = mmap(NULL, mapsize, PROT_READ, MAP_PRIVATE, fd, 0);
  if ((long) map == -1)
	bb_error_d("%s: mmap: %m", fname);

  optr = (unsigned char *)map;
  oend = optr + fsize;

  line = 1;
  state = S;

  for(;;) {
    getc1();
    for(;;) {
	/* [ \t]+ eat first space */
	while(c == ' ' || c == '\t')
	    getc1();

	if(state == S) {
		while(first_chars[c] == ANY) {
		    /* <S>unparsed */
		    if(c == '\n')
			line++;
		    getc1();
		}
		if(c == L_EOF) {
			/* <S><<EOF>> */
			munmap(map, mapsize);
			close(fd);
			return;
		}
		if(c == REM) {
			/* <S>/ */
			getc0();
			if(c == REM) {
				/* <S>"//"[^\n]* */
				do getc0(); while(c != '\n' && c != L_EOF);
			} else if(c == '*') {
				/* <S>[/][*] goto parse block comments */
				break;
			}
		} else if(c == POUND) {
			/* <S># */
			state = c;
			getc1();
		} else if(c == STR || c == CHR) {
			/* <S>\"|\' */
			int qc = c;

			for(;;) {
			    /* <STR,CHR>. */
			    getc1();
			    if(c == qc) {
				/* <STR>\" or <CHR>\' */
				break;
			    }
			    if(c == BS) {
				/* <STR,CHR>\\ but is not <STR,CHR>\\\n */
				getc0();
			    }
			    if(c == '\n' || c == L_EOF)
				yy_error_d("unterminated");
			}
			getc1();
		} else {
			/* <S>[A-Z_a-z0-9] */

			/* trick for fast drop id
			   if key with this first char undefined */
			if(first_chars[c] == 0) {
			    /* skip <S>[A-Z_a-z0-9]+ */
			    do getc1(); while(isalnums[c]);
			} else {
			    id_len = 0;
			    do {
				/* <S>[A-Z_a-z0-9]+ */
				put_id(c);
				getc1();
			    } while(isalnums[c]);
			    check_key(key_top, id, id_len);
			}
		}
		continue;
	}
	/* begin preprocessor states */
	if(c == L_EOF)
	    yy_error_d("unexpected EOF");
	if(c == REM) {
		/* <#.*>/ */
		getc0();
		if(c == REM)
			yy_error_d("detected // in preprocessor line");
		if(c == '*') {
			/* <#.*>[/][*] goto parse block comments */
			break;
		}
		/* hmm, #.*[/] */
		yy_error_d("strange preprocessor line");
	}
	if(state == POUND) {
	    /* tricks */
	    static const char * const preproc[] = {
		    /* 0 1   2  3     4        5        6 */
		    "", "", "", "", "ndef", "efine", "nclude"
	    };
	    size_t diu = first_chars_diu[c];   /* strlen and preproc ptr */

	    state = S;
	    if(diu != S) {
		getc1();
		id_len = 0;
		while(isalnums[c]) {
		    put_id(c);
		    getc1();
		}
		/* have str begined with c, readed == strlen key and compared */
		if(diu == id_len && !memcmp(id, preproc[diu], diu)) {
		    state = diu + '0';
		    id_len = 0; /* common for save */
		}
	    } else {
		while(isalnums[c]) getc1();
	    }
	} else if(state == I) {
		if(c == STR) {
			/* <I>\" */
			for(;;) {
			    getc1();
			    if(c == STR)
				break;
			    if(c == L_EOF)
				yy_error_d("unexpected EOF");
			    put_id(c);
			}
			put_id(0);
			/* store "include.h" */
			parse_inc(id, fname);
			getc1();
		}
		/* else another (may be wrong) #include ... */
		state = S;
	} else if(state == D || state == U) {
	    if(mode == SOURCES_MODE) {
		/* ignore depend with #define or #undef KEY */
		while(isalnums[c]) getc1();
		state = S;
	    } else {
		/* save KEY from #"define"|"undef" ... */
		while(isalnums[c]) {
		    put_id(c);
		    getc1();
		}
		if(!id_len)
		    yy_error_d("expected identifier");
		if(state == U) {
		    parse_conf_opt(id, NULL, id_len);
		    state = S;
		} else {
		    /* D -> DK */
		    if(c == '(')
			yy_error_d("unexpected function macro");
		    state = DK;
		}
	    }
	} else {
	    /* state==<DK> #define KEY[ ] (config mode) */
	    size_t opt_len = id_len;
	    char *val = id + opt_len;
	    char *sp;

	    for(;;) {
		if(c == L_EOF || c == '\n')
		    break;
		put_id(c);
		getc1();
	    }
	    sp = id + id_len;
	    put_id(0);
	    /* trim tail spaces */
	    while(--sp >= val && (*sp == ' ' || *sp == '\t'
				|| *sp == '\f' || *sp == '\v'))
		*sp = '\0';
	    parse_conf_opt(id, val, opt_len);
	    state = S;
	}
    }

    /* <REM> */
    getc0();
    for(;;) {
	  /* <REM>[^*]+ */
	  while(c != '*') {
		if(c == '\n') {
		    /* <REM>\n */
		    if(state != S)
			yy_error_d("unexpected newline");
		    line++;
		} else if(c == L_EOF)
		    yy_error_d("unexpected EOF");
		getc0();
	  }
	  /* <REM>[*] */
	  getc0();
	  if(c == REM) {
		/* <REM>[*][/] */
		break;
	  }
    }
  }
too_long:
  yy_error_d("phrase too long");
}


/* bb_simplify_path special variant for apsolute pathname */
static size_t bb_qa_simplify_path(char *path)
{
	char *s, *p;

	p = s = path;

	do {
	    if (*p == '/') {
		if (*s == '/') {    /* skip duplicate (or initial) slash */
		    continue;
		} else if (*s == '.') {
		    if (s[1] == '/' || s[1] == 0) { /* remove extra '.' */
			continue;
		    } else if ((s[1] == '.') && (s[2] == '/' || s[2] == 0)) {
			++s;
			if (p > path) {
			    while (*--p != '/');    /* omit previous dir */
			}
			continue;
		    }
		}
	    }
	    *++p = *s;
	} while (*++s);

	if ((p == path) || (*p != '/')) {  /* not a trailing slash */
	    ++p;                            /* so keep last character */
	}
	*p = 0;

	return (p - path);
}

static void parse_inc(const char *include, const char *fname)
{
    bb_key_t *cur;
    const char *p_i;
    llist_t *lo;
    char *ap;
    size_t key_sz;

    if(*include == '/') {
	lo = NULL;
	ap = bb_xstrdup(include);
    } else {
	int w;
	const char *p;

	lo = Iop;
	p = strrchr(fname, '/');    /* fname have absolute pathname */
	w = (p-fname);
	/* find from current directory of source file */
	ap = bb_asprint("%.*s/%s", w, fname, include);
    }

    for(;;) {
	key_sz = bb_qa_simplify_path(ap);
	cur = check_key(Ifound, ap, key_sz);
	if(cur) {
	    cur->checked = cur->value;
	    free(ap);
	    return;
	}
	if(access(ap, F_OK) == 0) {
	    /* found */
	    p_i = ap;
	    break;
	} else if(lo == NULL) {
	    p_i = NULL;
	    break;
	}

	/* find from "-I include" specified directories */
	free(ap);
	/* lo->data have absolute pathname */
	ap = bb_asprint("%s/%s", lo->data, include);
	lo = lo->link;
    }

    cur = xmalloc(sizeof(bb_key_t));
    cur->keyname = ap;
    cur->key_sz = key_sz;
    cur->stored_path = ap;
    cur->value = cur->checked = p_i;
    if(p_i == NULL && noiwarning)
	fprintf(stderr, "%s: Warning: #include \"%s\" not found\n", fname, include);
    cur->next = Ifound;
    Ifound = cur;
}

static size_t max_rec_sz;

static void parse_conf_opt(const char *opt, const char *val, size_t key_sz)
{
	bb_key_t *cur;
	char *k;
	char *p;
	size_t val_sz = 0;

	cur = check_key(key_top, opt, key_sz);
	if(cur != NULL) {
	    /* present already */
	    cur->checked = NULL;        /* store only */
	    if(cur->value == NULL && val == NULL)
		return;
	    if(cur->value != NULL && val != NULL && !strcmp(cur->value, val))
		return;
	    k = cur->keyname;
	    fprintf(stderr, "Warning: redefined %s\n", k);
	} else {
	    size_t recordsz;

	    if(val && *val)
		val_sz = strlen(val) + 1;
	    recordsz = key_sz + val_sz + 1;
	    if(max_rec_sz < recordsz)
		max_rec_sz = recordsz;
	    cur = xmalloc(sizeof(bb_key_t) + recordsz);
	    k = cur->keyname = memcpy(cur + 1, opt, key_sz);
	    cur->keyname[key_sz] = '\0';
	    cur->key_sz = key_sz;
	    cur->checked = NULL;
	    cur->src_have_this_key = NULL;
	    cur->next = key_top;
	    key_top = cur;
	}
	/* save VAL */
	if(val) {
	    if(*val == '\0') {
		cur->value = "";
	    } else {
		cur->value = p = cur->keyname + key_sz + 1;
		memcpy(p, val, val_sz);
	    }
	} else {
	    cur->value = NULL;
	}
	/* trick, save first char KEY for do fast identify id */
	first_chars[(int)*k] = *k;

	cur->stored_path = k = bb_asprint("%s/%s.h", kp, k);
	/* key converting [A-Z_] -> [a-z/] */
	for(p = k + kp_len + 1; *p; p++) {
	    if(*p >= 'A' && *p <= 'Z')
		    *p = *p - 'A' + 'a';
	    else if(*p == '_' && p[1] > '9')    /* do not change A_1 to A/1 */
		    *p = '/';
	}
}

static void store_keys(void)
{
    bb_key_t *cur;
    char *k;
    struct stat st;
    int cmp_ok;
    ssize_t rw_ret;
    size_t recordsz = max_rec_sz * 2 + 10 * 2 + 16;
    /* buffer for double "#define KEY VAL\n" */
    char *record_buf = xmalloc(recordsz);

    for(cur = key_top; cur; cur = cur->next) {
	if(cur->src_have_this_key) {
	    /* do generate record */
	    k = cur->keyname;
	    if(cur->value == NULL) {
		recordsz = sprintf(record_buf, "#undef %s\n", k);
	    } else {
		const char *val = cur->value;
		if(*val == '\0') {
		    recordsz = sprintf(record_buf, "#define %s\n", k);
		} else {
		    recordsz = sprintf(record_buf, "#define %s %s\n", k, val);
		}
	    }
	    /* size_t -> ssize_t :( */
	    rw_ret = (ssize_t)recordsz;
	    /* check kp/key.h, compare after previous usage */
	    cmp_ok = 0;
	    k = cur->stored_path;
	    if(stat(k, &st)) {
		char *p;
		for(p = k + kp_len + 1; *p; p++) {
		    /* Auto-create directories. */
		    if (*p == '/') {
			*p = '\0';
			if (access(k, F_OK) != 0 && mkdir(k, 0755) != 0)
			    bb_error_d("mkdir(%s): %m", k);
			*p = '/';
		    }
		}
	    } else {
		    /* found */
		    if(st.st_size == (off_t)recordsz) {
			char *r_cmp;
			int fd;
			size_t padded = recordsz;

			/* 16-byte padding for read(2) and memcmp(3) */
			padded = (padded+15) & ~15;
			r_cmp = record_buf + padded;
			fd = open(k, O_RDONLY);
			if(fd < 0 || read(fd, r_cmp, recordsz) < rw_ret)
			    bb_error_d("%s: %m", k);
			close(fd);
			cmp_ok = memcmp(record_buf, r_cmp, recordsz) == 0;
		    }
	    }
	    if(!cmp_ok) {
		int fd = open(k, O_WRONLY|O_CREAT|O_TRUNC, 0644);
		if(fd < 0 || write(fd, record_buf, recordsz) < rw_ret)
		    bb_error_d("%s: %m", k);
		close(fd);
	    }
	}
    }
}

static int show_dep(int first, bb_key_t *k, const char *name, const char *f)
{
    bb_key_t *cur;

    for(cur = k; cur; cur = cur->next) {
	if(cur->checked) {
	    if(first >= 0) {
		if(first) {
		    if(f == NULL)
			printf("\n%s:", name);
		    else
			printf("\n%s/%s:", pwd, name);
		    first = 0;
		} else {
		    printf(" \\\n  ");
		}
		printf(" %s", cur->checked);
	    }
	    cur->src_have_this_key = cur->checked;
	    cur->checked = NULL;
	}
    }
    return first;
}

static char *
parse_chd(const char *fe, const char *p, size_t dirlen)
{
    struct stat st;
    char *fp;
    size_t df_sz;
    static char dir_and_entry[4096];
    size_t fe_sz = strlen(fe) + 1;

    df_sz = dirlen + fe_sz + 1;         /* dir/file\0 */
    if(df_sz > sizeof(dir_and_entry))
	bb_error_d("%s: file name too long", fe);
    fp = dir_and_entry;
    /* sprintf(fp, "%s/%s", p, fe); */
    memcpy(fp, p, dirlen);
    fp[dirlen] = '/';
    memcpy(fp + dirlen + 1, fe, fe_sz);

    if(stat(fp, &st)) {
	fprintf(stderr, "Warning: stat(%s): %m\n", fp);
	return NULL;
    }
    if(S_ISREG(st.st_mode)) {
	llist_t *cfl;
	char *e = fp + df_sz - 3;

	if(*e++ != '.' || (*e != 'c' && *e != 'h')) {
	    /* direntry is regular file, but is not *.[ch] */
	    return NULL;
	}
	for(cfl = configs; cfl; cfl = cfl->link) {
	    struct stat *config = (struct stat *)cfl->data;

	    if (st.st_dev == config->st_dev && st.st_ino == config->st_ino) {
		/* skip already parsed configs.h */
		return NULL;
	    }
	}
	/* direntry is *.[ch] regular file and is not configs */
	c_lex(fp, st.st_size);
	if(!dontgenerate_dep) {
	    int first;
	    if(*e == 'c') {
		/* *.c -> *.o */
		*e = 'o';
		/* /src_dir/path/file.o to path/file.o */
		fp += replace;
		if(*fp == '/')
		    fp++;
	    } else {
		e = NULL;
	    }
	    first = show_dep(1, Ifound, fp, e);
	    first = show_dep(first, key_top, fp, e);
	    if(first == 0)
		putchar('\n');
	} else {
	    show_dep(-1, key_top, NULL, NULL);
	}
	return NULL;
    } else if(S_ISDIR(st.st_mode)) {
	if (st.st_dev == st_kp.st_dev && st.st_ino == st_kp.st_ino)
	    return NULL;    /* drop scan kp/ directory */
	/* direntry is directory. buff is returned */
	return bb_xstrdup(fp);
    }
    /* hmm, direntry is device! */
    return NULL;
}

/* from libbb but inline for fast */
static inline llist_t *llist_add_to(llist_t *old_head, char *new_item)
{
	llist_t *new_head;

	new_head = xmalloc(sizeof(llist_t));
	new_head->data = new_item;
	new_head->link = old_head;

	return(new_head);
}

static void scan_dir_find_ch_files(const char *p)
{
    llist_t *dirs;
    llist_t *d_add;
    llist_t *d;
    struct dirent *de;
    DIR *dir;
    size_t dirlen;

    dirs = llist_add_to(NULL, bb_simplify_path(p));
    replace = strlen(dirs->data);
    /* emulate recursive */
    while(dirs) {
	d_add = NULL;
	while(dirs) {
	    dir = opendir(dirs->data);
	    if (dir == NULL)
		fprintf(stderr, "Warning: opendir(%s): %m\n", dirs->data);
	    dirlen = strlen(dirs->data);
	    while ((de = readdir(dir)) != NULL) {
		char *found_dir;

		if (de->d_name[0] == '.')
			continue;
		found_dir = parse_chd(de->d_name, dirs->data, dirlen);
		if(found_dir)
		    d_add = llist_add_to(d_add, found_dir);
	    }
	    closedir(dir);
	    free(dirs->data);
	    d = dirs;
	    dirs = dirs->link;
	    free(d);
	}
	dirs = d_add;
    }
}

static void show_usage(void) __attribute__ ((noreturn));
static void show_usage(void)
{
	bb_error_d("%s\n%s\n", bb_mkdep_terse_options, bb_mkdep_full_options);
}

int main(int argc, char **argv)
{
	char *s;
	int i;
	llist_t *fl;

	{
	    /* for bb_simplify_path, this program have not chdir() */
	    /* libbb-like my xgetcwd() */
	    unsigned path_max = 512;

	    s = xmalloc (path_max);
	    while (getcwd (s, path_max) == NULL) {
		if(errno != ERANGE)
		    bb_error_d("getcwd: %m");
		free(s);
		s = xmalloc(path_max *= 2);
	    }
	    pwd = s;
	}

	while ((i = getopt(argc, argv, "I:c:dk:w")) > 0) {
		switch(i) {
		    case 'I':
			    s = bb_simplify_path(optarg);
			    Iop = llist_add_to(Iop, s);
			    break;
		    case 'c':
			    s = bb_simplify_path(optarg);
			    configs = llist_add_to(configs, s);
			    break;
		    case 'd':
			    dontgenerate_dep = 1;
			    break;
		    case 'k':
			    if(kp)
				bb_error_d("Hmm, why multiple -k?");
			    kp = bb_simplify_path(optarg);
			    break;
		    case 'w':
			    noiwarning = 1;
			    break;
		    default:
			    show_usage();
		}
	}
	/* default kp */
	if(kp == NULL)
	    kp = bb_simplify_path(INCLUDE_CONFIG_PATH);
	/* globals initialize */
	kp_len = strlen(kp);
	if(stat(kp, &st_kp))
	    bb_error_d("stat(%s): %m", kp);
	if(!S_ISDIR(st_kp.st_mode))
	    bb_error_d("%s is not directory", kp);
	/* defaults */
	if(Iop == NULL)
	    Iop = llist_add_to(Iop, bb_simplify_path(LOCAL_INCLUDE_PATH));
	if(configs == NULL) {
	    s = bb_simplify_path(INCLUDE_CONFIG_KEYS_PATH);
	    configs = llist_add_to(configs, s);
	}
	/* for c_lex */
	pagesizem1 = getpagesize() - 1;
	for(i = 0; i < UCHAR_MAX; i++) {
	    if(ISALNUM(i))
		isalnums[i] = i;
	    /* set unparsed chars for speed up of parser */
	    else if(i != CHR && i != STR && i != POUND && i != REM)
		first_chars[i] = ANY;
	}
	first_chars[i] = '-';   /* L_EOF */

	/* parse configs */
	for(fl = configs; fl; fl = fl->link) {
	    struct stat st;

	    if(stat(fl->data, &st))
		bb_error_d("stat(%s): %m", fl->data);
	    c_lex(fl->data, st.st_size);
	    free(fl->data);
	    /* trick for fast comparing found files with configs */
	    fl->data = xmalloc(sizeof(struct stat));
	    memcpy(fl->data, &st, sizeof(struct stat));
	}

	/* main loop */
	mode = SOURCES_MODE;
	argv += optind;
	if(*argv) {
	    while(*argv)
		scan_dir_find_ch_files(*argv++);
	} else {
		scan_dir_find_ch_files(".");
	}
	store_keys();
	return 0;
}

/* partial and simplified libbb routine */
static void bb_error_d(const char *s, ...)
{
	va_list p;

	va_start(p, s);
	vfprintf(stderr, s, p);
	va_end(p);
	putc('\n', stderr);
	exit(1);
}

static char *bb_asprint(const char *format, ...)
{
	va_list p;
	int r;
	char *out;

	va_start(p, format);
	r = vasprintf(&out, format, p);
	va_end(p);

	if (r < 0)
		bb_error_d("bb_asprint: %m");
	return out;
}

/* partial libbb routine as is */

static char *bb_simplify_path(const char *path)
{
	char *s, *start, *p;

	if (path[0] == '/')
		start = bb_xstrdup(path);
	else {
		/* is not libbb, but this program have not chdir() */
		start = bb_asprint("%s/%s", pwd, path);
	}
	p = s = start;

	do {
	    if (*p == '/') {
		if (*s == '/') {    /* skip duplicate (or initial) slash */
		    continue;
		} else if (*s == '.') {
		    if (s[1] == '/' || s[1] == 0) { /* remove extra '.' */
			continue;
		    } else if ((s[1] == '.') && (s[2] == '/' || s[2] == 0)) {
			++s;
			if (p > start) {
			    while (*--p != '/');    /* omit previous dir */
			}
			continue;
		    }
		}
	    }
	    *++p = *s;
	} while (*++s);

	if ((p == start) || (*p != '/')) {  /* not a trailing slash */
	    ++p;                            /* so keep last character */
	}
	*p = 0;

	return start;
}
