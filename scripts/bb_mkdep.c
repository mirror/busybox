/*
 * Another fast dependencies generator for Makefiles, Version 2.3
 *
 * Copyright (C) 2005 by Vladimir Oleynik <dzo@simtreas.ru>
 * mmaping file may be originally by Linus Torvalds.
 *
 * (c) 2005 Bernhard Fischer:
 *  - commentary typos,
 *  - move "memory exhausted" into msg_enomem,
 *  - more verbose --help output.
 *
 * This program does:
 * 1) find #define KEY VALUE or #undef KEY from include/config.h
 * 2) save include/config/key*.h if changed after previous usage
 * 3) recursive scan from "./" *.[ch] files, but skips scan of include/config/
 * 4) find #include "*.h" and KEYs using, if not as #define and #undef
 * 5) generate dependencies to stdout
 *    path/file.o: include/config/key*.h found_include_*.h
 *    path/inc.h: include/config/key*.h found_included_include_*.h
 * This program does not generate dependencies for #include <...>
 * BUG: all includes name must unique
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


typedef struct BB_KEYS {
	char *keyname;
	size_t key_sz;
	const char *value;
	char *stored_path;
	char *checked;
	struct BB_KEYS *next;
} bb_key_t;

static bb_key_t *check_key(bb_key_t *k, const char *nk, size_t key_sz);
static bb_key_t *make_new_key(bb_key_t *k, const char *nk, size_t key_sz);

/* partial and simplified libbb routine */
static void bb_error_d(const char *s, ...) __attribute__ ((noreturn, format (printf, 1, 2)));
static char * bb_asprint(const char *format, ...) __attribute__ ((format (printf, 1, 2)));

/* stolen from libbb as is */
typedef struct llist_s {
	char *data;
	struct llist_s *link;
} llist_t;
static void *xrealloc(void *p, size_t size);
static void *xmalloc(size_t size);
static char *bb_xstrdup(const char *s);
static char *bb_simplify_path(const char *path);
/* error messages */
static const char msg_enomem[] = "memory exhausted";

/* for lexical analyser */
static bb_key_t *key_top;
static llist_t *configs;

static int mode;
#define CONFIG_MODE  0
#define SOURCES_MODE 1

static void parse_inc(const char *include, const char *fname, size_t key_sz);
static void parse_conf_opt(const char *opt, const char *val,
				size_t rsz, size_t key_sz);

/* for speed tricks */
static char first_chars[257];  /* + L_EOF */
static char isalnums[257];     /* + L_EOF */
/* trick for fast find "define", "include", "undef" */
static char first_chars_diu[256];

static int pagesizem1;
static size_t mema_id = 128;   /* first allocated for id */
static char *id_s;


#define yy_error_d(s) bb_error_d("%s:%d hmm, %s", fname, line, s)

/* state */
#define S      0        /* start state */
#define STR    '"'      /* string */
#define CHR    '\''     /* char */
#define REM    '/'      /* block comment */
#define BS     '\\'     /* back slash */
#define POUND  '#'      /* # */
#define I      'i'      /* #include preprocessor's directive */
#define D      'd'      /* #define preprocessor's directive */
#define U      'u'      /* #undef preprocessor's directive */
#define LI     'I'      /* #include "... */
#define DK     'K'      /* #define KEY... (config mode) */
#define DV     'V'      /* #define KEY "VALUE or #define KEY 'VALUE */
#define NLC    'n'      /* \ and \n */
#define ANY    '*'      /* any unparsed chars */

#define L_EOF  256
/* [A-Z_a-z] */
#define ID(c) ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_')
/* [A-Z_a-z0-9] */
#define ISALNUM(c)  (ID(c) || (c >= '0' && c <= '9'))

#define getc1()     do { c = (optr >= oend) ? L_EOF : *optr++; } while(0)
#define ungetc1()   optr--

#define put_id(c)   do {    if(id_len == local_mema_id)                 \
				id = xrealloc(id, local_mema_id += 16); \
			    id[id_len++] = c; } while(0)



/* stupid C lexical analyser */
static void c_lex(const char *fname, long fsize)
{
  int c = L_EOF;                     /* stupid initialize */
  int prev_state = L_EOF;
  int called;
  int state;
  int line;
  char *id = id_s;
  size_t local_mema_id = mema_id;
  size_t id_len = 0;                /* stupid initialize */
  char *val = NULL;
  unsigned char *optr, *oend;
  unsigned char *start = NULL;      /* stupid initialize */
  size_t opt_len = 0;               /* stupid initialize */

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
  called = state = S;

  for(;;) {
	if(prev_state != state) {
	    prev_state = state;
	    getc1();
	}

	/* [ \t]+ eat first space */
	while(c == ' ' || c == '\t')
	    getc1();

	if(c == BS) {
		getc1();
		if(c == '\n') {
			/* \\\n eat continued */
			line++;
			prev_state = NLC;
			continue;
		}
		ungetc1();
		c = BS;
	}

	if(state == S) {
		while(first_chars[c] == ANY) {
		    /* <S>unparsed */
		    if(c == '\n')
			line++;
		    getc1();
		}
		if(c == L_EOF) {
			/* <S><<EOF>> */
			id_s = id;
			mema_id = local_mema_id;
			munmap(map, mapsize);
			close(fd);
			return;
		}
		if(c == REM) {
			/* <S>/ */
			getc1();    /* eat <S>/ */
			if(c == REM) {
				/* <S>"//"[^\n]* */
				do getc1(); while(c != '\n' && c != L_EOF);
			} else if(c == '*') {
				/* <S>[/][*] */
				called = S;
				state = REM;
			}
		} else if(c == POUND) {
			/* <S># */
			start = optr - 1;
			state = c;
		} else if(c == STR || c == CHR) {
			/* <S>\"|\' */
			val = NULL;
			called = S;
			state = c;
		} else if(c != BS) {
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
		} else {
		    /* <S>\\ */
		    prev_state = c;
		}
		continue;
	}
	if(state == REM) {
	  for(;;) {
		/* <REM>[^*]+ */
		while(c != '*') {
			if(c == '\n') {
				/* <REM>\n */
				if(called != S)
				    yy_error_d("unexpected newline");
				line++;
			} else if(c == L_EOF)
				yy_error_d("unexpected EOF");
			getc1();
		}
		/* <REM>[*] */
		getc1();
		if(c == REM) {
			/* <REM>[*][/] */
			state = called;
			break;
		}
	  }
	  continue;
	}
	if(state == STR || state == CHR) {
	    for(;;) {
		/* <STR,CHR>\n|<<EOF>> */
		if(c == '\n' || c == L_EOF)
			yy_error_d("unterminated");
		if(c == BS) {
			/* <STR,CHR>\\ */
			getc1();
			if(c != BS && c != '\n' && c != state) {
			    /* another usage \ in str or char */
			    if(c == L_EOF)
				yy_error_d("unexpected EOF");
			    if(val)
				put_id(c);
			    continue;
			}
			/* <STR,CHR>\\[\\\n] or <STR>\\\" or <CHR>\\\' */
			/* eat 2 char */
			if(c == '\n')
			    line++;
			else if(val)
			    put_id(c);
		} else if(c == state) {
			/* <STR>\" or <CHR>\' */
			if(called == LI) {
			    /* store "include.h" */
			    parse_inc(id, fname, id_len);
			} else if(called == DV) {
			    put_id(c);  /* config mode #define KEY "VAL"<- */
			    put_id(0);
			    parse_conf_opt(id, val, (optr - start), opt_len);
			}
			state = S;
			break;
		} else if(val)
			put_id(c);
		/* <STR,CHR>. */
		getc1();
	    }
	    continue;
	}

	/* begin preprocessor states */
	if(c == L_EOF)
	    yy_error_d("unexpected EOF");
	if(c == REM) {
		/* <#.*>/ */
		getc1();
		if(c == REM)
			yy_error_d("detected // in preprocessor line");
		if(c == '*') {
			/* <#.*>[/][*] */
			called = state;
			state = REM;
			continue;
		}
		/* hmm, #.*[/] */
		yy_error_d("strange preprocessor line");
	}
	if(state == POUND) {
	    /* tricks */
	    static const char * const preproc[] = {
		    /* 0-3 */
		    "", "", "", "",
		    /* 4 */ /* 5 */   /* 6 */
		    "ndef", "efine", "nclude",
	    };
	    size_t diu = first_chars_diu[c];   /* strlen and preproc ptr */
	    const unsigned char *p = optr;

	    while(isalnums[c]) getc1();
	    /* have str begined with c, readed == strlen key and compared */
	    if(diu != S && diu == (optr-p-1) && !memcmp(p, preproc[diu], diu)) {
		state = p[-1];
		id_len = 0; /* common for save */
	    } else {
		state = S;
	    }
	    ungetc1();
	    continue;
	}
	if(state == I) {
		if(c == STR) {
			/* <I>\" */
			val = id;
			called = LI;
			state = STR;
		} else {
		    /* another (may be wrong) #include ... */
		    ungetc1();
		    state = S;
		}
		continue;
	}
	if(state == D || state == U) {
	    if(mode == SOURCES_MODE) {
		/* ignore depend with #define or #undef KEY */
		while(isalnums[c])
		    getc1();
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
		    parse_conf_opt(id, NULL, (optr - start), id_len);
		    state = S;
		} else {
		    /* D -> DK */
		    opt_len = id_len;
		    state = DK;
		}
	    }
	    ungetc1();
	    continue;
	}
	if(state == DK) {
	    /* #define KEY[ ] (config mode) */
	    val = id + id_len;
	    if(c == STR || c == CHR) {
		/* define KEY "... or define KEY '... */
		put_id(c);
		called = DV;
		state = c;
		continue;
	    }
	    while(isalnums[c]) {
		/* VALUE */
		put_id(c);
		getc1();
	    }
	    put_id(0);
	    parse_conf_opt(id, val, (optr - start), opt_len);
	    state = S;
	    ungetc1();
	    continue;
	}
    }
}


static void show_usage(void) __attribute__ ((noreturn));
static void show_usage(void)
{
	bb_error_d("%s\n%s\n", bb_mkdep_terse_options, bb_mkdep_full_options);
}

static const char *kp;
static size_t kp_len;
static llist_t *Iop;
static bb_key_t *Ifound;
static int noiwarning;

static bb_key_t *check_key(bb_key_t *k, const char *nk, size_t key_sz)
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

static bb_key_t *make_new_key(bb_key_t *k, const char *nk, size_t key_sz)
{
	bb_key_t *cur;

	cur = xmalloc(sizeof(bb_key_t) + key_sz + 1);
	cur->keyname = memcpy(cur + 1, nk, key_sz);
	cur->keyname[key_sz] = '\0';
	cur->key_sz = key_sz;
	cur->checked = NULL;
	cur->next = k;
	return cur;
}

static inline char *store_include_fullpath(char *p_i, bb_key_t *li)
{
    char *ok;

    if(access(p_i, F_OK) == 0) {
	ok = li->stored_path = bb_simplify_path(p_i);
	li->checked = ok;
    } else {
	ok = NULL;
    }
    free(p_i);
    return ok;
}

static void parse_inc(const char *include, const char *fname, size_t key_sz)
{
	bb_key_t *li;
	char *p_i;
	llist_t *lo;

	li = check_key(Ifound, include, key_sz);
	if(li)
	    return;
	Ifound = li = make_new_key(Ifound, include, key_sz);
	include = li->keyname;
	if(include[0] != '/') {
	    /* relative */
	    int w;

	    p_i = strrchr(fname, '/');  /* fname have absolute pathname */
	    w = (p_i-fname);
	    /* find from current directory of source file */
	    p_i = bb_asprint("%.*s/%s", w, fname, include);
	    if(store_include_fullpath(p_i, li))
		return;
	    /* find from "-I include" specified directories */
	    for(lo = Iop; lo; lo = lo->link) {
		p_i = bb_asprint("%s/%s", lo->data, include);
		if(store_include_fullpath(p_i, li))
		    return;
	    }
	} else {
	    /* absolute include pathname */
	    if(access(include, F_OK) == 0) {
		li->checked = li->stored_path = bb_xstrdup(include);
		return;
	    }
	}
	li->stored_path = NULL;
	if(noiwarning)
	    fprintf(stderr, "%s: Warning: #include \"%s\" not found in specified paths\n", fname, include);
}

static void parse_conf_opt(const char *opt, const char *val,
			   size_t recordsz, size_t key_sz)
{
	bb_key_t *cur;
	char *k;
	char *s, *p;
	struct stat st;
	int fd;
	int cmp_ok = 0;
	static char *record_buf;
	static size_t r_sz;
	ssize_t rw_ret;

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
	    key_top = cur = make_new_key(key_top, opt, key_sz);
	    k = cur->keyname;
	}
	/* do generate record */
	recordsz += 2;  /* \n\0 */
	if(recordsz > r_sz)
	    record_buf = xrealloc(record_buf, (r_sz = recordsz) * 2);
	s = record_buf;
	/* may be short count " " */
	if(val) {
	    if(*val == '\0') {
		cur->value = "";
		recordsz = sprintf(s, "#define %s\n", k);
	    } else {
		cur->value = bb_xstrdup(val);
		recordsz = sprintf(s, "#define %s %s\n", k, val);
	    }
	} else {
	    cur->value = NULL;
	    recordsz = sprintf(s, "#undef %s\n", k);
	}
	/* size_t -> ssize_t :( */
	rw_ret = (ssize_t)recordsz;
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
	/* check kp/key.h if present after previous usage */
	if(stat(k, &st)) {
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
		    char *r_cmp = s + recordsz;

		    fd = open(k, O_RDONLY);
		    if(fd < 0 || read(fd, r_cmp, recordsz) < rw_ret)
			bb_error_d("%s: %m", k);
		    close(fd);
		    cmp_ok = memcmp(s, r_cmp, recordsz) == 0;
		}
	}
	if(!cmp_ok) {
	    fd = open(k, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	    if(fd < 0 || write(fd, s, recordsz) < rw_ret)
		bb_error_d("%s: %m", k);
	    close(fd);
	}
}

static int show_dep(int first, bb_key_t *k, const char *name)
{
    bb_key_t *cur;

    for(cur = k; cur; cur = cur->next) {
	if(cur->checked) {
	    if(first) {
		printf("\n%s:", name);
		first = 0;
	    } else {
		printf(" \\\n  ");
	    }
	    printf(" %s", cur->checked);
	}
	cur->checked = NULL;
    }
    return first;
}

static struct stat st_kp;
static int dontgenerate_dep;

static char *
parse_chd(const char *fe, const char *p, size_t dirlen)
{
    struct stat st;
    char *fp;
    size_t df_sz;
    static char *dir_and_entry;
    static size_t dir_and_entry_sz;

    df_sz = dirlen + strlen(fe) + 2;     /* dir/file\0 */
    if(df_sz > dir_and_entry_sz)
	dir_and_entry = xrealloc(dir_and_entry, dir_and_entry_sz = df_sz);
    fp = dir_and_entry;
    sprintf(fp, "%s/%s", p, fe);

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
	if(!dontgenerate_dep) {
	    int first;

	    c_lex(fp, st.st_size);
	    if(*e == 'c') {
		/* *.c -> *.o */
		*e = 'o';
	    }
	    first = show_dep(1, Ifound, fp);
	    first = show_dep(first, key_top, fp);
	    if(first == 0)
		putchar('\n');
	}
	return NULL;
    } else if(S_ISDIR(st.st_mode)) {
	if (st.st_dev == st_kp.st_dev && st.st_ino == st_kp.st_ino)
	    return NULL;    /* drop scan kp/ directory */
	/* direntry is directory. buff is returned, begin of zero allocate */
	dir_and_entry = NULL;
	dir_and_entry_sz = 0;
	return fp;
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

static char *pwd;

int main(int argc, char **argv)
{
	char *s;
	int i;
	llist_t *fl;

	{
	    /* for bb_simplify_path */
	    /* libbb xgetcwd(), this program have not chdir() */
	    unsigned path_max = 512;

	    s = xmalloc (path_max);
#define PATH_INCR 32
	    while (getcwd (s, path_max) == NULL) {
		if(errno != ERANGE)
		    bb_error_d("getcwd: %m");
		s = xrealloc (s, path_max += PATH_INCR);
		}
	    pwd = s;
	}

	while ((i = getopt(argc, argv, "I:c:dk:w")) > 0) {
		switch(i) {
		    case 'I':
			    Iop = llist_add_to(Iop, optarg);
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
	    Iop = llist_add_to(Iop, LOCAL_INCLUDE_PATH);
	if(configs == NULL) {
	    s = bb_simplify_path(INCLUDE_CONFIG_KEYS_PATH);
	    configs = llist_add_to(configs, s);
	}
	/* for c_lex */
	pagesizem1 = getpagesize() - 1;
	id_s = xmalloc(mema_id);
	for(i = 0; i < 256; i++) {
	    if(ISALNUM(i))
		isalnums[i] = i;
	    /* set unparsed chars for speed up of parser */
	    else if(i != CHR && i != STR && i != POUND && i != REM && i != BS)
		first_chars[i] = ANY;
	}
	first_chars[i] = '-';   /* L_EOF */
	/* trick for fast find "define", "include", "undef" */
	first_chars_diu[(int)'d'] = (char)5;    /* strlen("define") - 1;  */
	first_chars_diu[(int)'i'] = (char)6;    /* strlen("include") - 1; */
	first_chars_diu[(int)'u'] = (char)4;    /* strlen("undef") - 1;   */

	/* parse configs */
	for(fl = configs; fl; fl = fl->link) {
	    struct stat st;

	    if(stat(fl->data, &st))
		bb_error_d("stat(%s): %m", fl->data);
	    c_lex(fl->data, st.st_size);
	    /* trick for fast comparing found files with configs */
	    fl->data = xrealloc(fl->data, sizeof(struct stat));
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
static void *xmalloc(size_t size)
{
	void *p = malloc(size);

	if(p == NULL)
		bb_error_d(msg_enomem);
	return p;
}

static void *xrealloc(void *p, size_t size) {
	p = realloc(p, size);
	if(p == NULL)
		bb_error_d(msg_enomem);
	return p;
}

static char *bb_xstrdup(const char *s)
{
	char *r = strdup(s);

	if(r == NULL)
	    bb_error_d(msg_enomem);
	return r;
}

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
