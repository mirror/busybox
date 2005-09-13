/*
 * Another dependences for Makefile fast mashine generator
 *
 * Copyright (C) 2005 by Vladimir Oleynik <dzo@simtreas.ru>
 *
 * This programm do
 * 1) find #define KEY VALUE or #undef KEY from include/config.h
 * 2) save include/config/key*.h if changed after previous usage
 * 3) recursive scan from "./" *.[ch] files, but skip scan include/config/...
 * 4) find #include "*.h" and KEYs using, if not as #define and #undef
 * 5) generate depend to stdout
 *    path/file.o: include/config/key*.h found_include_*.h
 *    path/inc.h: include/config/key*.h found_included_include_*.h
 * This programm do not generate dependences for #include <...>
 *
 * Options:
 * -I local_include_path  (include`s paths, default: LOCAL_INCLUDE_PATH)
 * -d                     (don`t generate depend)
 * -w                     (show warning if include files not found)
 * -k include/config      (default: INCLUDE_CONFIG_PATH)
 * -c include/config.h    (configs, default: INCLUDE_CONFIG_KEYS_PATH)
 * dirs_for_scan          (default ".")
*/

#define LOCAL_INCLUDE_PATH          "include"
#define INCLUDE_CONFIG_PATH         LOCAL_INCLUDE_PATH"/config"
#define INCLUDE_CONFIG_KEYS_PATH    LOCAL_INCLUDE_PATH"/config.h"

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
	const char *value;
	char *stored_path;
	int  checked;
	struct BB_KEYS *next;
} bb_key_t;

typedef struct FILE_LIST {
	char *name;
	char *ext;      /* *.c or *.h, point to last char */
	long size;
} file_list_t;

/* partial and simplify libbb routine */
static void bb_error_d(const char *s, ...) __attribute__ ((noreturn, format (printf, 1, 2)));
static char * bb_asprint(const char *format, ...) __attribute__ ((format (printf, 1, 2)));

/* stolen from libbb as is */
typedef struct llist_s {
	char *data;
	struct llist_s *link;
} llist_t;
static llist_t *llist_add_to(llist_t *old_head, char *new_item);
static void *xrealloc(void *p, size_t size);
static void *xmalloc(size_t size);
static char *bb_xstrdup(const char *s);
static char *bb_simplify_path(const char *path);

/* for lexical analyzier */
static bb_key_t *key_top;
static llist_t *configs;

static void parse_inc(const char *include, const char *fname);
static void parse_conf_opt(char *opt, const char *val, size_t rsz);

/* for speed triks */
static char first_chars[257];  /* + L_EOF */

static int pagesizem1;
static size_t mema_id = 128;   /* first allocated for id */
static char *id_s;

static bb_key_t *check_key(bb_key_t *k, const char *nk);
static bb_key_t *make_new_key(bb_key_t *k, const char *nk);

#define yy_error_d(s) bb_error_d("%s:%d hmm, %s", fname, line, s)

/* state */
#define S      0        /* start state */
#define STR    '"'      /* string */
#define CHR    '\''     /* char */
#define REM    '/'      /* block comment */
#define BS     '\\'     /* back slash */
#define POUND  '#'      /* # */
#define I      'i'      /* #include preprocessor`s directive */
#define D      'd'      /* #define preprocessor`s directive */
#define U      'u'      /* #undef preprocessor`s directive */
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

/* stupid C lexical analizator */
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

  int fd;
  char *map;
  int mapsize;

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
	if(state == LI || state == DV) {
	    /* store "include.h" or config mode #define KEY "|'..."|'  */
	    put_id(0);
	    if(state == LI) {
		parse_inc(id, fname);
	    } else {
		/* #define KEY "[VAL]" */
		parse_conf_opt(id, val, (optr - start));
	    }
	    state = S;
	}
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
			    do getc1(); while(ISALNUM(c));
			} else {
			    id_len = 0;
			    do {
				/* <S>[A-Z_a-z0-9]+ */
				put_id(c);
				getc1();
			    } while(ISALNUM(c));
			    put_id(0);
			    check_key(key_top, id);
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
			yy_error_d("unterminating");
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
			if(called == DV)
			    put_id(c);  /* #define KEY "VALUE"<- */
			state = called;
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
			yy_error_d("detect // in preprocessor line");
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
	    if(c != 'd' && c != 'u' && c != 'i') {
		while(ISALNUM(c))
		    getc1();
		state = S;
	    } else {
		static const char * const preproc[] = {
		    "define", "undef", "include", ""
		};
		const char * const *str_type;

		id_len = 0;
		do { put_id(c); getc1(); } while(ISALNUM(c));
		put_id(0);
		for(str_type = preproc; (state = **str_type); str_type++) {
		    if(*id == state && strcmp(id, *str_type) == 0)
			break;
		}
		/* to S if another #directive */
		id_len = 0; /* common for save */
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
	    if(configs == NULL) {
		/* ignore depend with #define or #undef KEY */
		while(ISALNUM(c))
		    getc1();
		state = S;
	    } else {
		/* save KEY from #"define"|"undef" ... */
		while(ISALNUM(c)) {
		    put_id(c);
		    getc1();
		}
		if(!id_len)
		    yy_error_d("expected identificator");
		put_id(0);
		if(state == U) {
		    parse_conf_opt(id, NULL, (optr - start));
		    state = S;
		} else {
		    /* D -> DK */
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
	    while(ISALNUM(c)) {
		/* VALUE */
		put_id(c);
		getc1();
	    }
	    ungetc1();
	    state = DV;
	    continue;
	}
    }
}


static void show_usage(void) __attribute__ ((noreturn));
static void show_usage(void)
{
	bb_error_d("Usage: [-I local_include_paths] [-dw] "
		   "[-k path_for_store_keys] [-s skip_file] [dirs]");
}

static const char *kp;
static size_t kp_len;
static llist_t *Iop;
static bb_key_t *Ifound;
static int noiwarning;

static bb_key_t *check_key(bb_key_t *k, const char *nk)
{
    bb_key_t *cur;

    for(cur = k; cur; cur = cur->next) {
	if(strcmp(cur->keyname, nk) == 0) {
	    cur->checked = 1;
	    return cur;
	}
    }
    return NULL;
}

static bb_key_t *make_new_key(bb_key_t *k, const char *nk)
{
	bb_key_t *cur;
	size_t nk_size;

	nk_size = strlen(nk) + 1;
	cur = xmalloc(sizeof(bb_key_t) + nk_size);
	cur->keyname = memcpy(cur + 1, nk, nk_size);
	cur->checked = 1;
	cur->next = k;
	return cur;
}

static inline char *store_include_fullpath(char *p_i, bb_key_t *li)
{
    struct stat st;
    char *ok;

    if(stat(p_i, &st) == 0) {
	ok = li->stored_path = bb_simplify_path(p_i);
    } else {
	ok = NULL;
    }
    free(p_i);
    return ok;
}

static void parse_inc(const char *include, const char *fname)
{
	bb_key_t *li;
	char *p_i;
	llist_t *lo;

	li = check_key(Ifound, include);
	if(li)
	    return;
	Ifound = li = make_new_key(Ifound, include);

	if(include[0] != '/') {
	    /* relative */
	    int w;
	    const char *p;

	    p_i = strrchr(fname, '/');
	    if(p_i == NULL) {
		p = ".";
		w = 1;
	    } else {
		w = (p_i-fname);
		p = fname;
	    }
	    p_i = bb_asprint("%.*s/%s", w, p, include);
	    if(store_include_fullpath(p_i, li))
		return;
	}
	for(lo = Iop; lo; lo = lo->link) {
	    p_i = bb_asprint("%s/%s", lo->data, include);
	    if(store_include_fullpath(p_i, li))
		return;
	}
	li->stored_path = NULL;
	if(noiwarning)
	    fprintf(stderr, "%s: Warning: #include \"%s\" not found in specified paths\n", fname, include);
}

static void parse_conf_opt(char *opt, const char *val, size_t recordsz)
{
	bb_key_t *cur;

	cur = check_key(key_top, opt);
	if(cur == NULL) {
	    /* new key, check old key if present after previous usage */
	    char *s, *p;
	    struct stat st;
	    int fd;
	    int cmp_ok = 0;
	    static char *record_buf;
	    static char *r_cmp;
	    static size_t r_sz;
	    ssize_t rw_ret;

	    cur = make_new_key(key_top, opt);

	    recordsz += 2;  /* \n\0 */
	    if(recordsz > r_sz) {
		record_buf = xrealloc(record_buf, r_sz=recordsz);
		r_cmp = xrealloc(r_cmp, recordsz);
	    }
	    s = record_buf;
	    /* may be short count " " */
	    if(val) {
		if(*val == '\0') {
		    cur->value = "";
		    recordsz = sprintf(s, "#define %s\n", opt);
		} else {
		    cur->value = bb_xstrdup(val);
		    recordsz = sprintf(s, "#define %s %s\n", opt, val);
		}
	    } else {
		cur->value = NULL;
		recordsz = sprintf(s, "#undef %s\n", opt);
	    }
	    /* size_t -> ssize_t :( */
	    rw_ret = (ssize_t)recordsz;
	    /* trick, save first char KEY for do fast identify id */
	    first_chars[(int)*opt] = *opt;

	    /* key converting [A-Z_] -> [a-z/] */
	    for(p = opt; *p; p++) {
		if(*p >= 'A' && *p <= 'Z')
			*p = *p - 'A' + 'a';
		else if(*p == '_')
			*p = '/';
	    }
	    p = bb_asprint("%s/%s.h", kp, opt);
	    cur->stored_path = opt = p;
	    if(stat(opt, &st)) {
		p += kp_len;
		while(*++p) {
		    /* Auto-create directories. */
		    if (*p == '/') {
			*p = '\0';
			if (stat(opt, &st) != 0 && mkdir(opt, 0755) != 0)
			    bb_error_d("mkdir(%s): %m", opt);
			*p = '/';
		    }
		}
	    } else {
		    /* found */
		    if(st.st_size == (off_t)recordsz) {
			fd = open(opt, O_RDONLY);
			if(fd < 0 || read(fd, r_cmp, recordsz) < rw_ret)
			    bb_error_d("%s: %m", opt);
			close(fd);
			cmp_ok = memcmp(s, r_cmp, recordsz) == 0;
		    }
	    }
	    if(!cmp_ok) {
		fd = open(opt, O_WRONLY|O_CREAT|O_TRUNC, 0644);
		if(fd < 0 || write(fd, s, recordsz) < rw_ret)
		    bb_error_d("%s: %m", opt);
		close(fd);
	    }
	    key_top = cur;
	} else {
	    /* present already */
	    if((cur->value == NULL && val != NULL) ||
	       (cur->value != NULL && val == NULL) ||
	       (cur->value != NULL && val != NULL && strcmp(cur->value, val)))
		    fprintf(stderr, "Warning: redefined %s\n", opt);
	}
	/* store only */
	cur->checked = 0;
}

static int show_dep(int first, bb_key_t *k, const char *name)
{
    bb_key_t *cur;

    for(cur = k; cur; cur = cur->next) {
	if(cur->checked && cur->stored_path) {
	    if(first) {
		printf("\n%s:", name);
		first = 0;
	    } else {
		printf(" \\\n  ");
	    }
	    printf(" %s", cur->stored_path);
	}
	cur->checked = 0;
    }
    return first;
}

static llist_t *files;
static struct stat st_kp;

static char *dir_and_entry;

static char *
filter_chd(const char *fe, const char *p, size_t dirlen)
{
    struct stat st;
    char *fp;
    char *afp;
    llist_t *cfl;
    file_list_t *f;
    size_t df_sz;
    static size_t dir_and_entry_sz;

    if (*fe == '.')
	return NULL;

    df_sz = dirlen + strlen(fe) + 2;     /* dir/file\0 */
    if(df_sz > dir_and_entry_sz)
	dir_and_entry = xrealloc(dir_and_entry, dir_and_entry_sz = df_sz);
    fp = dir_and_entry;
    sprintf(fp, "%s/%s", p, fe);

    if(stat(fp, &st)) {
	fprintf(stderr, "Warning: stat(%s): %m", fp);
	return NULL;
    }
    if(S_ISREG(st.st_mode)) {
	afp = fp + df_sz - 3;
	if(*afp++ != '.' || (*afp != 'c' && *afp != 'h')) {
	    /* direntry is regular file, but is not *.[ch] */
	    return NULL;
	}
	if (st.st_size == 0) {
	    fprintf(stderr, "Warning: %s is empty\n", fp);
	    return NULL;
	}
    } else {
	if(S_ISDIR(st.st_mode)) {
	    if (st.st_dev == st_kp.st_dev && st.st_ino == st_kp.st_ino) {
		/* drop scan kp/ directory */
		return NULL;
	    }
	    /* buff is returned, begin of zero allocate */
	    dir_and_entry = NULL;
	    dir_and_entry_sz = 0;
	    return fp;
	}
	/* hmm, is device! */
	return NULL;
    }
    afp = bb_simplify_path(fp);
    for(cfl = configs; cfl; cfl = cfl->link) {
	if(cfl->data && strcmp(cfl->data, afp) == 0) {
		/* parse configs.h */
		free(afp);
		c_lex(fp, st.st_size);
		free(cfl->data);
		cfl->data = NULL;
		return NULL;
	}
    }
    /* direntry is *.[ch] regular file */
    f = xmalloc(sizeof(file_list_t));
    f->name = afp;
    f->ext = strrchr(afp, '.') + 1;
    f->size = st.st_size;
    files = llist_add_to(files, (char *)f);
    return NULL;
}

static void scan_dir_find_ch_files(char *p)
{
    llist_t *dirs;
    llist_t *d_add;
    llist_t *d;
    struct dirent *de;
    DIR *dir;
    size_t dirlen;

    dirs = llist_add_to(NULL, p);
    /* emulate recursive */
    while(dirs) {
	d_add = NULL;
	while(dirs) {
	    dir = opendir(dirs->data);
	    if (dir == NULL)
		fprintf(stderr, "Warning: opendir(%s): %m", dirs->data);
	    dirlen = strlen(dirs->data);
	    while ((de = readdir(dir)) != NULL) {
		char *found_dir = filter_chd(de->d_name, dirs->data, dirlen);

		if(found_dir)
		    d_add = llist_add_to(d_add, found_dir);
	    }
	    closedir(dir);
	    if(dirs->data != p)
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
	int generate_dep = 1;
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
		path_max += PATH_INCR;
		s = xrealloc (s, path_max);
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
			    generate_dep = 0;
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
	/* defaults */
	if(kp == NULL)
	    kp = bb_simplify_path(INCLUDE_CONFIG_PATH);
	if(Iop == NULL)
	    Iop = llist_add_to(Iop, LOCAL_INCLUDE_PATH);
	if(configs == NULL) {
	    s = bb_simplify_path(INCLUDE_CONFIG_KEYS_PATH);
	    configs = llist_add_to(configs, s);
	}
	/* globals initialize */
	/* for c_lex */
	pagesizem1 = getpagesize() - 1;
	id_s = xmalloc(mema_id);
	for(i = 0; i < 256; i++) {
	    /* set unparsed chars for speed up of parser */
	    if(!ISALNUM(i) && i != CHR && i != STR &&
			      i != POUND && i != REM && i != BS)
		first_chars[i] = ANY;
	}
	first_chars[i] = '-';   /* L_EOF */

	kp_len = strlen(kp);
	if(stat(kp, &st_kp))
	    bb_error_d("stat(%s): %m", kp);
	if(!S_ISDIR(st_kp.st_mode))
	    bb_error_d("%s is not directory", kp);

	/* main loops */
	argv += optind;
	if(*argv) {
	    while(*argv)
		scan_dir_find_ch_files(*argv++);
	} else {
		scan_dir_find_ch_files(".");
	}

	for(fl = configs; fl; fl = fl->link) {
	    if(fl->data) {
		/* configs.h placed outsize of scanned dirs or not "*.ch" */
		struct stat st;

		if(stat(fl->data, &st))
		    bb_error_d("stat(%s): %m", fl->data);
		c_lex(fl->data, st.st_size);
		free(fl->data);
	    }
	}
	free(configs);
	configs = NULL;     /* flag read config --> parse sourses mode */

	for(fl = files; fl; fl = fl->link) {
		file_list_t *t = (file_list_t *)(fl->data);
		c_lex(t->name, t->size);
		if(generate_dep) {
			if(t->ext[0] == 'c') {
			    /* *.c -> *.o */
			    t->ext[0] = 'o';
			}
			i = show_dep(1, Ifound, t->name);
			i = show_dep(i, key_top, t->name);
			if(i == 0)
				putchar('\n');
		}
	}
	return 0;
}

/* partial and simplify libbb routine */
static void bb_error_d(const char *s, ...)
{
	va_list p;

	va_start(p, s);
	vfprintf(stderr, s, p);
	va_end(p);
	putc('\n', stderr);
	exit(1);
}


static void *xmalloc(size_t size)
{
	void *p = malloc(size);

	if(p == NULL)
		bb_error_d("memory exhausted");
	return p;
}

static void *xrealloc(void *p, size_t size) {
	p = realloc(p, size);
	if(p == NULL)
		bb_error_d("memory exhausted");
	return p;
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

static llist_t *llist_add_to(llist_t *old_head, char *new_item)
{
	llist_t *new_head;

	new_head = xmalloc(sizeof(llist_t));
	new_head->data = new_item;
	new_head->link = old_head;

	return(new_head);
}

static char *bb_xstrdup(const char *s)
{
	char *r = strdup(s);

	if(r == NULL)
	    bb_error_d("memory exhausted");
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
