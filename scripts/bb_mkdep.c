/*
 * Another dependences for Makefile mashine generator
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


/* partial and simplify libbb routine */

void bb_error_d(const char *s, ...) __attribute__ ((noreturn, format (printf, 1, 2)));
char * bb_asprint(const char *format, ...) __attribute__ ((format (printf, 1, 2)));

/* stolen from libbb as is */
typedef struct llist_s {
	char *data;
	struct llist_s *link;
} llist_t;
llist_t *llist_add_to(llist_t *old_head, char *new_item);
void *xrealloc(void *p, size_t size);
void *xmalloc(size_t size);
char *bb_xstrdup(const char *s);
char *bb_simplify_path(const char *path);

/* for lexical analyzier */
static bb_key_t *key_top;

static void parse_inc(const char *include, const char *fname);
static void parse_conf_opt(char *opt, const char *val, size_t rsz);

#define CHECK_ONLY  0
#define MAKE_NEW    1
static bb_key_t *find_already(bb_key_t *k, const char *nk, int flg_save_new);

#define yy_error_d(s) bb_error_d("%s:%d hmm, %s", fname, line, s)

/* state */
#define S      0        /* start state */
#define STR    '"'      /* string */
#define CHR    '\''     /* char */
#define REM    '*'      /* block comment */
#define POUND  '#'      /* # */
#define I      'i'      /* #include preprocessor`s directive */
#define D      'd'      /* #define preprocessor`s directive */
#define U      'u'      /* #undef preprocessor`s directive */
#define LI     'I'      /* #include "... */
#define DK     'K'      /* #define KEY... (config mode) */
#define DV     'V'      /* #define KEY "... or #define KEY '... */
#define NLC    'n'      /* \+\n */
#define ANY    '?'      /* skip unparsed . */

/* [A-Z_a-z] */
#define ID(c) ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_')
/* [A-Z_a-z0-9] */
#define ISALNUM(c)  (ID(c) || (c >= '0' && c <= '9'))

#define getc1()     do { c = (optr >= oend) ? EOF : *optr++; } while(0)
#define ungetc1()   optr--

#define put_id(c)   do {    if(id_len == mema_id)                 \
				id = xrealloc(id, mema_id += 16); \
			    id[id_len++] = c; } while(0)

/* stupid C lexical analizator */
static void c_lex(const char *fname, int flg_config_include)
{
  int c = EOF;                      /* stupid initialize */
  int prev_state = EOF;
  int called;
  int state;
  int line;
  static size_t mema_id;
  char *id = xmalloc(mema_id=128);  /* fist allocate */
  size_t id_len = 0;                /* stupid initialize */
  char *val = NULL;
  unsigned char *optr, *oend;
  unsigned char *start = NULL;      /* stupid initialize */

  int fd;
  char *map;
  int mapsize;
  {
    /* stolen from mkdep by Linus Torvalds */
    int pagesizem1 = getpagesize() - 1;
    struct stat st;

    fd = open(fname, O_RDONLY);
    if(fd < 0) {
	perror(fname);
	return;
    }
    fstat(fd, &st);
    if (st.st_size == 0)
	bb_error_d("%s is empty", fname);
    mapsize = st.st_size;
    mapsize = (mapsize+pagesizem1) & ~pagesizem1;
    map = mmap(NULL, mapsize, PROT_READ, MAP_PRIVATE, fd, 0);
    if ((long) map == -1)
	bb_error_d("%s: mmap: %m", fname);

    /* hereinafter is my */
    optr = (unsigned char *)map;
    oend = optr + st.st_size;
  }

  line = 1;
  called = state = S;

  for(;;) {
	if(state == LI || state == DV) {
	    /* store "include.h" or config mode #define KEY "|'..."|'  */
	    put_id(0);
	    if(state == LI) {
		parse_inc(id, fname);
	    } else {
		/*
		if(val[0] == '\0')
		    yy_error_d("expected value");
		*/
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

	if(c == '\\') {
		getc1();
		if(c == '\n') {
			/* \\\n eat continued */
			line++;
			prev_state = NLC;
			continue;
		}
		ungetc1();
		c = '\\';
	}

	if(state == S) {
		while(c <= ' ' && c != EOF) {
		    /* <S>[\000- ]+ */
		    if(c == '\n')
			line++;
		    getc1();
		}
		if(c == EOF) {
			/* <S><<EOF>> */
			munmap(map, mapsize);
			close(fd);
			return;
		}
		if(c == '/') {
			/* <S>/ */
			getc1();
			if(c == '/') {
				/* <S>"//"[^\n]* */
				do getc1(); while(c != '\n' && c != EOF);
			} else if(c == '*') {
				/* <S>[/][*] */
				called = S;
				state = REM;
			}
			/* eat <S>/ */
		} else if(c == '#') {
			/* <S>\"|\'|# */
			start = optr - 1;
			state = c;
		} else if(c == STR || c == CHR) {
			/* <S>\"|\'|# */
			val = NULL;
			called = S;
			state = c;
		} else if(ISALNUM(c)) {
			/* <S>[A-Z_a-z0-9] */
			id_len = 0;
			do {
				/* <S>[A-Z_a-z0-9]+ */
				put_id(c);
				getc1();
			} while(ISALNUM(c));
			put_id(0);
			find_already(key_top, id, CHECK_ONLY);
		} else {
		    /* <S>. */
		    prev_state = ANY;
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
			} else if(c == EOF)
				yy_error_d("unexpected EOF");
			getc1();
		}
		/* <REM>[*] */
		getc1();
		if(c == '/') {
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
		if(c == '\n' || c == EOF)
			yy_error_d("unterminating");
		if(c == '\\') {
			/* <STR,CHR>\\ */
			getc1();
			if(c != '\\' && c != '\n' && c != state) {
			    /* another usage \ in str or char */
			    if(c == EOF)
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
			    put_id(c);
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
	if(c == EOF)
	    yy_error_d("unexpected EOF");
	if(c == '/') {
		/* <#.*>/ */
		getc1();
		if(c == '/')
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
	if(state == '#') {
		static const char * const preproc[] = {
		    "define", "undef", "include", ""
		};
		const char * const *str_type;

		id_len = 0;
		while(ISALNUM(c)) {
		    put_id(c);
		    getc1();
		}
		put_id(0);
		for(str_type = preproc; (state = **str_type); str_type++) {
		    if(*id == state && strcmp(id, *str_type) == 0)
			break;
		}
		/* to S if another #directive */
		ungetc1();
		id_len = 0; /* common for save */
		continue;
	}
	if(state == I) {
		if(c == STR) {
			/* <I>\" */
			val = id;
			state = STR;
			called = LI;
			continue;
		}
		/* another (may be wrong) #include ... */
		ungetc1();
		state = S;
		continue;
	}
	if(state == D || state == U) {
	    while(ISALNUM(c)) {
		if(flg_config_include) {
		    /* save KEY from #"define"|"undef" ... */
		    put_id(c);
		}
		getc1();
	    }
	    if(!flg_config_include) {
		state = S;
	    } else {
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
	    /* #define (config mode) */
	    val = id + id_len;
	    if(c == STR || c == CHR) {
		/* define KEY "... or define KEY '... */
		put_id(c);
		called = DV;
		state = c;
		continue;
	    }
	    while(ISALNUM(c)) {
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
	bb_error_d("Usage: [-I local_include_path] [-dw] "
			"[-k path_for_store_keys] [-s skip_file]");
}

static const char *kp;
static llist_t *Iop;
static bb_key_t *Ifound;
static int noiwarning;
static llist_t *configs;

static bb_key_t *find_already(bb_key_t *k, const char *nk, int flg_save_new)
{
	bb_key_t *cur;

	for(cur = k; cur; cur = cur->next) {
	    if(strcmp(cur->keyname, nk) == 0) {
		cur->checked = 1;
		return NULL;
	    }
	}
	if(flg_save_new == CHECK_ONLY)
	    return NULL;
	cur = xmalloc(sizeof(bb_key_t));
	cur->keyname = bb_xstrdup(nk);
	cur->checked = 1;
	cur->next = k;
	return cur;
}

static int store_include_fullpath(char *p_i, bb_key_t *li)
{
    struct stat st;
    int ok = 0;

    if(stat(p_i, &st) == 0) {
	li->stored_path = bb_simplify_path(p_i);
	ok = 1;
    }
    free(p_i);
    return ok;
}

static void parse_inc(const char *include, const char *fname)
{
	bb_key_t *li;
	char *p_i;
	llist_t *lo;

	if((li = find_already(Ifound, include, MAKE_NEW)) == NULL)
	    return;
	Ifound = li;
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
	bb_key_t *cur = find_already(key_top, opt, MAKE_NEW);

	if(cur != NULL) {
	    /* new key, check old key if present after previous usage */
	    char *s, *p;
	    struct stat st;
	    int fd;
	    int cmp_ok = 0;
	    static char *record_buf;
	    static char *r_cmp;
	    static size_t r_sz;

	    recordsz += 2;  /* \n\0 */
	    if(recordsz > r_sz) {
		record_buf = xrealloc(record_buf, r_sz=recordsz);
		r_cmp = xrealloc(r_cmp, recordsz);
	    }
	    s = record_buf;
	    if(val)
		sprintf(s, "#define %s%s%s\n", opt, (*val ? " " : ""), val);
	    else
		sprintf(s, "#undef %s\n", opt);
	    /* may be short count " " */
	    recordsz = strlen(s);
	    /* key converting [A-Z] -> [a-z] */
	    for(p = opt; *p; p++) {
		if(*p >= 'A' && *p <= 'Z')
			*p = *p - 'A' + 'a';
		if(*p == '_')
		    *p = '/';
	    }
	    p = bb_asprint("%s/%s.h", kp, opt);
	    cur->stored_path = opt = p;
	    while(*++p) {
		/* Auto-create directories. */
		if (*p == '/') {
		    *p = '\0';
		    if (stat(opt, &st) != 0 && mkdir(opt, 0755) != 0)
			bb_error_d("mkdir(%s): %m", opt);
		    *p = '/';
		}
	    }
	    if(stat(opt, &st) == 0) {
		    /* found */
		    if(st.st_size == recordsz) {
			fd = open(opt, O_RDONLY);
			if(fd < 0 || read(fd, r_cmp, recordsz) != recordsz)
			    bb_error_d("%s: %m", opt);
			close(fd);
			cmp_ok = memcmp(s, r_cmp, recordsz) == 0;
		    }
	    }
	    if(!cmp_ok) {
		fd = open(opt, O_WRONLY|O_CREAT|O_TRUNC, 0644);
		if(fd < 0 || write(fd, s, recordsz) != recordsz)
		    bb_error_d("%s: %m", opt);
		close(fd);
	    }
	    /* store only */
	    cur->checked = 0;
	    if(val) {
		if(*val == '\0') {
		    cur->value = "";
		} else {
		    cur->value = bb_xstrdup(val);
		}
	    } else {
		cur->value = NULL;
	    }
	    key_top = cur;
	} else {
	    /* present already */
	    for(cur = key_top; cur; cur = cur->next) {
		if(strcmp(cur->keyname, opt) == 0) {
		    cur->checked = 0;
		    if(cur->value == NULL && val == NULL)
			return;
		    if((cur->value == NULL && val != NULL) ||
			    (cur->value != NULL && val == NULL) ||
			    strcmp(cur->value, val))
			fprintf(stderr, "Warning: redefined %s\n", opt);
		    return;
		}
	    }
	}
}

static int show_dep(int first, bb_key_t *k, const char *a)
{
    bb_key_t *cur;

    for(cur = k; cur; cur = cur->next) {
	if(cur->checked && cur->stored_path) {
	    if(first) {
		const char *ext;

		if(*a == '.' && a[1] == '/')
		    a += 2;
		ext = strrchr(a, '.');
		if(ext && ext[1] == 'c' && ext[2] == '\0') {
		    /* *.c -> *.o */
		    printf("\n%.*s.o:", (ext - a), a);
		} else {
		    printf("\n%s:", a);
		}
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

static llist_t *filter_chd(const char *fe, const char *p, llist_t *pdirs)
{
    const char *e;
    struct stat st;
    char *fp;
    char *afp;
    llist_t *cfl;

    if (*fe == '.')
	return NULL;
    fp = bb_asprint("%s/%s", p, fe);
    if(stat(fp, &st)) {
	fprintf(stderr, "Warning: stat(%s): %m", fp);
	free(fp);
	return NULL;
    }
    afp = bb_simplify_path(fp);
    if(S_ISDIR(st.st_mode)) {
	if(strcmp(kp, afp) == 0) {
	    /* is autogenerated to kp/key* by previous usage */
	    free(afp);
	    free(fp);
	    /* drop scan kp/ directory */
	    return NULL;
	}
	free(afp);
	return llist_add_to(pdirs, fp);
    }
    if(!S_ISREG(st.st_mode)) {
	/* hmm, is device! */
	free(afp);
	free(fp);
	return NULL;
    }
    e = strrchr(fe, '.');
    if(e == NULL || !((e[1]=='c' || e[1]=='h') && e[2]=='\0')) {
	/* direntry is not directory or *.[ch] */
	free(afp);
	free(fp);
	return NULL;
    }
    for(cfl = configs; cfl; cfl = cfl->link) {
	if(cfl->data && strcmp(cfl->data, afp) == 0) {
	    /* parse configs.h */
	    free(afp);
	    c_lex(fp, 1);
	    free(fp);
	    free(cfl->data);
	    cfl->data = NULL;
	    return NULL;
	}
    }
    free(fp);
    /* direntry is *.[ch] regular file */
    files = llist_add_to(files, afp);
    return NULL;
}

static void scan_dir_find_ch_files(char *p)
{
    llist_t *dirs;
    llist_t *d_add;
    llist_t *d;
    struct dirent *de;
    DIR *dir;

    dirs = llist_add_to(NULL, p);
    /* emulate recursive */
    while(dirs) {
	d_add = NULL;
	while(dirs) {
	    dir = opendir(dirs->data);
	    if (dir == NULL)
		fprintf(stderr, "Warning: opendir(%s): %m", dirs->data);
	    while ((de = readdir(dir)) != NULL) {
		d = filter_chd(de->d_name, dirs->data, d_add);
		if(d)
		    d_add = d;
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

int main(int argc, char **argv)
{
	int generate_dep = 1;
	char *s;
	int i;
	llist_t *fl;

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
	if(argc > optind)
	    show_usage();

	/* defaults */
	if(kp == NULL)
	    kp = bb_simplify_path(INCLUDE_CONFIG_PATH);
	if(Iop == NULL)
	    Iop = llist_add_to(Iop, LOCAL_INCLUDE_PATH);
	if(configs == NULL) {
	    s = bb_simplify_path(INCLUDE_CONFIG_KEYS_PATH);
	    configs = llist_add_to(configs, s);
	}
	scan_dir_find_ch_files(".");

	for(fl = files; fl; fl = fl->link) {
		c_lex(fl->data, 0);
		if(generate_dep) {
			i = show_dep(1, Ifound, fl->data);
			i = show_dep(i, key_top, fl->data);
			if(i == 0)
				putchar('\n');
		}
	}
	return 0;
}

void bb_error_d(const char *s, ...)
{
	va_list p;

	va_start(p, s);
	vfprintf(stderr, s, p);
	va_end(p);
	putc('\n', stderr);
	exit(1);
}


void *xmalloc(size_t size)
{
	void *p = malloc(size);

	if(p == NULL)
		bb_error_d("memory exhausted");
	return p;
}

void *xrealloc(void *p, size_t size) {
	p = realloc(p, size);
	if(p == NULL)
		bb_error_d("memory exhausted");
	return p;
}

char *bb_asprint(const char *format, ...)
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

llist_t *llist_add_to(llist_t *old_head, char *new_item)
{
	llist_t *new_head;

	new_head = xmalloc(sizeof(llist_t));
	new_head->data = new_item;
	new_head->link = old_head;

	return(new_head);
}

char *bb_xstrdup(const char *s)
{
    char *r = strdup(s);
    if(r == NULL)
	bb_error_d("memory exhausted");
    return r;
}

char *bb_simplify_path(const char *path)
{
	char *s, *start, *p;

	if (path[0] == '/')
	      start = bb_xstrdup(path);
	else {
	      static char *pwd;

	      if(pwd == NULL) {
		    /* is not libbb, but this program have not chdir() */
		    unsigned path_max = 512;
		    char *cwd = xmalloc (path_max);
#define PATH_INCR 32
		    while (getcwd (cwd, path_max) == NULL) {
			if(errno != ERANGE)
			    bb_error_d("getcwd: %m");
			path_max += PATH_INCR;
			cwd = xrealloc (cwd, path_max);
		    }
		    pwd = cwd;
	    }
	    start = bb_asprint("%s/%s", pwd, path);
	}
	p = s = start;

	do {
		if (*p == '/') {
			if (*s == '/') {        /* skip duplicate (or initial) slash */
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

	if ((p == start) || (*p != '/')) {      /* not a trailing slash */
		++p;                            /* so keep last character */
	}
	*p = 0;

	return start;
}
