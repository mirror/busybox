/* vi: set sw=4 ts=4: */
/*
 * Mini diff implementation for busybox, adapted from OpenBSD diff.
 *
 * Copyright (C) 2006 by Robert Sullivan <cogito.ergo.cogito@hotmail.com>
 * Copyright (c) 2003 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

// #define FSIZE_MAX 32768

/* NOINLINEs added to prevent gcc from merging too much into diffreg()
 * (it bites more than it can (efficiently) chew). */

/*
 * Output flags
 */
enum {
	/* Print a header/footer between files */
	/* D_HEADER = 1, - unused */
	/* Treat file as empty (/dev/null) */
	D_EMPTY1 = 2 * ENABLE_FEATURE_DIFF_DIR,
	D_EMPTY2 = 4 * ENABLE_FEATURE_DIFF_DIR,
};

/*
 * Status values for print_status() and diffreg() return values
 * Guide:
 * D_SAME - files are the same
 * D_DIFFER - files differ
 * D_BINARY - binary files differ
 * D_COMMON - subdirectory common to both dirs
 * D_ONLY - file only exists in one dir
 * D_ISDIR1 - path1 a dir, path2 a file
 * D_ISDIR2 - path1 a file, path2 a dir
 * D_ERROR - error occurred
 * D_SKIPPED1 - skipped path1 as it is a special file
 * D_SKIPPED2 - skipped path2 as it is a special file
 */
#define D_SAME          0
#define D_DIFFER        (1 << 0)
#define D_BINARY        (1 << 1)
#define D_COMMON        (1 << 2)
/*#define D_ONLY        (1 << 3) - unused */
#define D_ISDIR1        (1 << 4)
#define D_ISDIR2        (1 << 5)
#define D_ERROR         (1 << 6)
#define D_SKIPPED1      (1 << 7)
#define D_SKIPPED2      (1 << 8)

/* Command line options */
#define FLAG_a  (1 << 0)
#define FLAG_b  (1 << 1)
#define FLAG_d  (1 << 2)
#define FLAG_i  (1 << 3)
#define FLAG_L  (1 << 4)
#define FLAG_N  (1 << 5)
#define FLAG_q  (1 << 6)
#define FLAG_r  (1 << 7)
#define FLAG_s  (1 << 8)
#define FLAG_S  (1 << 9)
#define FLAG_t  (1 << 10)
#define FLAG_T  (1 << 11)
#define FLAG_U  (1 << 12)
#define FLAG_w  (1 << 13)


struct cand {
	int x;
	int y;
	int pred;
};

struct line {
	int serial;
	int value;
};

/*
 * The following struct is used to record change information
 * doing a "context" or "unified" diff.  (see routine "change" to
 * understand the highly mnemonic field names)
 */
struct context_vec {
	int a;          /* start line in old file */
	int b;          /* end line in old file */
	int c;          /* start line in new file */
	int d;          /* end line in new file */
};


#define g_read_buf bb_common_bufsiz1

struct globals {
	bool anychange;
	smallint exit_status;
	int opt_U_context;
	size_t max_context;     /* size of context_vec_start */
	USE_FEATURE_DIFF_DIR(int dl_count;)
	USE_FEATURE_DIFF_DIR(char **dl;)
	char *opt_S_start;
	const char *label1;
	const char *label2;
	int *J;                 /* will be overlaid on class */
	int clen;
	int pref, suff;         /* length of prefix and suffix */
	int nlen[2];
	int slen[2];
	int clistlen;           /* the length of clist */
	struct cand *clist;     /* merely a free storage pot for candidates */
	long *ixnew;            /* will be overlaid on nfile[1] */
	long *ixold;            /* will be overlaid on klist */
	struct line *nfile[2];
	struct line *sfile[2];  /* shortened by pruning common prefix/suffix */
	struct context_vec *context_vec_start;
	struct context_vec *context_vec_end;
	struct context_vec *context_vec_ptr;
	char *tempname1, *tempname2;
	struct stat stb1, stb2;
};
#define G (*ptr_to_globals)
#define anychange          (G.anychange         )
#define exit_status        (G.exit_status       )
#define opt_U_context      (G.opt_U_context     )
#define max_context        (G.max_context       )
#define dl_count           (G.dl_count          )
#define dl                 (G.dl                )
#define opt_S_start        (G.opt_S_start       )
#define label1             (G.label1            )
#define label2             (G.label2            )
#define J                  (G.J                 )
#define clen               (G.clen              )
#define pref               (G.pref              )
#define suff               (G.suff              )
#define nlen               (G.nlen              )
#define slen               (G.slen              )
#define clistlen           (G.clistlen          )
#define clist              (G.clist             )
#define ixnew              (G.ixnew             )
#define ixold              (G.ixold             )
#define nfile              (G.nfile             )
#define sfile              (G.sfile             )
#define context_vec_start  (G.context_vec_start )
#define context_vec_end    (G.context_vec_end   )
#define context_vec_ptr    (G.context_vec_ptr   )
#define stb1               (G.stb1              )
#define stb2               (G.stb2              )
#define tempname1          (G.tempname1         )
#define tempname2          (G.tempname2         )
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
	opt_U_context = 3; \
	max_context = 64; \
} while (0)


#if ENABLE_FEATURE_DIFF_DIR
static void print_only(const char *path, const char *entry)
{
	printf("Only in %s: %s\n", path, entry);
}
#endif


static void print_status(int val, char *_path1, char *_path2)
{
	/*const char *const _entry = entry ? entry : "";*/
	/*char *const _path1 = entry ? concat_path_file(path1, _entry) : path1;*/
	/*char *const _path2 = entry ? concat_path_file(path2, _entry) : path2;*/

	switch (val) {
/*	case D_ONLY:
		print_only(path1, entry);
		break;
*/
	case D_COMMON:
		printf("Common subdirectories: %s and %s\n", _path1, _path2);
		break;
	case D_BINARY:
		printf("Binary files %s and %s differ\n", _path1, _path2);
		break;
	case D_DIFFER:
		if (option_mask32 & FLAG_q)
			printf("Files %s and %s differ\n", _path1, _path2);
		break;
	case D_SAME:
		if (option_mask32 & FLAG_s)
			printf("Files %s and %s are identical\n", _path1, _path2);
		break;
	case D_ISDIR1:
		printf("File %s is a %s while file %s is a %s\n",
			   _path1, "directory", _path2, "regular file");
		break;
	case D_ISDIR2:
		printf("File %s is a %s while file %s is a %s\n",
			   _path1, "regular file", _path2, "directory");
		break;
	case D_SKIPPED1:
		printf("File %s is not a regular file or directory and was skipped\n",
			   _path1);
		break;
	case D_SKIPPED2:
		printf("File %s is not a regular file or directory and was skipped\n",
			   _path2);
		break;
	}
/*
	if (entry) {
		free(_path1);
		free(_path2);
	}
*/
}


/* Read line, return its nonzero hash. Return 0 if EOF.
 *
 * Hash function taken from Robert Sedgewick, Algorithms in C, 3d ed., p 578.
 */
static ALWAYS_INLINE int fiddle_sum(int sum, int t)
{
	return sum * 127 + t;
}
static int readhash(FILE *fp)
{
	int i, t, space;
	int sum;

	sum = 1;
	space = 0;
	i = 0;
	if (!(option_mask32 & (FLAG_b | FLAG_w))) {
		while ((t = getc(fp)) != '\n') {
			if (t == EOF) {
				if (i == 0)
					return 0;
				break;
			}
			sum = fiddle_sum(sum, t);
			i = 1;
		}
	} else {
		while (1) {
			switch (t = getc(fp)) {
			case '\t':
			case '\r':
			case '\v':
			case '\f':
			case ' ':
				space = 1;
				continue;
			default:
				if (space && !(option_mask32 & FLAG_w)) {
					i = 1;
					space = 0;
				}
				sum = fiddle_sum(sum, t);
				i = 1;
				continue;
			case EOF:
				if (i == 0)
					return 0;
				/* FALLTHROUGH */
			case '\n':
				break;
			}
			break;
		}
	}
	/*
	 * There is a remote possibility that we end up with a zero sum.
	 * Zero is used as an EOF marker, so return 1 instead.
	 */
	return (sum == 0 ? 1 : sum);
}


/* Our diff implementation is using seek.
 * When we meet non-seekable file, we must make a temp copy.
 */
static char *make_temp(FILE *f, struct stat *sb)
{
	char *name;
	int fd;

	if (S_ISREG(sb->st_mode) || S_ISBLK(sb->st_mode))
		return NULL;
	name = xstrdup("/tmp/difXXXXXX");
	fd = mkstemp(name);
	if (fd < 0)
		bb_perror_msg_and_die("mkstemp");
	if (bb_copyfd_eof(fileno(f), fd) < 0) {
 clean_up:
		unlink(name);
		xfunc_die(); /* error message is printed by bb_copyfd_eof */
	}
	fstat(fd, sb);
	close(fd);
	if (freopen(name, "r+", f) == NULL) {
		bb_perror_msg("freopen");
		goto clean_up;
	}
	return name;
}


/*
 * Check to see if the given files differ.
 * Returns 0 if they are the same, 1 if different, and -1 on error.
 */
static NOINLINE int files_differ(FILE *f1, FILE *f2)
{
	size_t i, j;

	/* Prevent making copies for "/dev/null" (too common) */
	/* Deal with input from pipes etc */
	tempname1 = make_temp(f1, &stb1);
	tempname2 = make_temp(f2, &stb2);
	if (stb1.st_size != stb2.st_size) {
		return 1;
	}
	while (1) {
		i = fread(g_read_buf,                    1, COMMON_BUFSIZE/2, f1);
		j = fread(g_read_buf + COMMON_BUFSIZE/2, 1, COMMON_BUFSIZE/2, f2);
		if (i != j)
			return 1;
		if (i == 0)
			return (ferror(f1) || ferror(f2)) ? -1 : 0;
		if (memcmp(g_read_buf,
		           g_read_buf + COMMON_BUFSIZE/2, i) != 0)
			return 1;
	}
}


static void prepare(int i, FILE *fp /*, off_t filesize*/)
{
	struct line *p;
	int h;
	size_t j, sz;

	rewind(fp);

	/*sz = (filesize <= FSIZE_MAX ? filesize : FSIZE_MAX) / 25;*/
	/*if (sz < 100)*/
	sz = 100;

	p = xmalloc((sz + 3) * sizeof(p[0]));
	j = 0;
	while ((h = readhash(fp)) != 0) { /* while not EOF */
		if (j == sz) {
			sz = sz * 3 / 2;
			p = xrealloc(p, (sz + 3) * sizeof(p[0]));
		}
		p[++j].value = h;
	}
	nlen[i] = j;
	nfile[i] = p;
}


static void prune(void)
{
	int i, j;

	for (pref = 0; pref < nlen[0] && pref < nlen[1] &&
		nfile[0][pref + 1].value == nfile[1][pref + 1].value; pref++)
		continue;
	for (suff = 0; suff < nlen[0] - pref && suff < nlen[1] - pref &&
		nfile[0][nlen[0] - suff].value == nfile[1][nlen[1] - suff].value;
		suff++)
		continue;
	for (j = 0; j < 2; j++) {
		sfile[j] = nfile[j] + pref;
		slen[j] = nlen[j] - pref - suff;
		for (i = 0; i <= slen[j]; i++)
			sfile[j][i].serial = i;
	}
}


static void equiv(struct line *a, int n, struct line *b, int m, int *c)
{
	int i, j;

	i = j = 1;
	while (i <= n && j <= m) {
		if (a[i].value < b[j].value)
			a[i++].value = 0;
		else if (a[i].value == b[j].value)
			a[i++].value = j;
		else
			j++;
	}
	while (i <= n)
		a[i++].value = 0;
	b[m + 1].value = 0;
	j = 0;
	while (++j <= m) {
		c[j] = -b[j].serial;
		while (b[j + 1].value == b[j].value) {
			j++;
			c[j] = b[j].serial;
		}
	}
	c[j] = -1;
}


static int isqrt(int n)
{
	int y, x;

	if (n == 0)
		return 0;
	x = 1;
	do {
		y = x;
		x = n / x;
		x += y;
		x /= 2;
	} while ((x - y) > 1 || (x - y) < -1);

	return x;
}


static int newcand(int x, int y, int pred)
{
	struct cand *q;

	if (clen == clistlen) {
		clistlen = clistlen * 11 / 10;
		clist = xrealloc(clist, clistlen * sizeof(struct cand));
	}
	q = clist + clen;
	q->x = x;
	q->y = y;
	q->pred = pred;
	return clen++;
}


static int search(int *c, int k, int y)
{
	int i, j, l, t;

	if (clist[c[k]].y < y)	/* quick look for typical case */
		return k + 1;
	i = 0;
	j = k + 1;
	while (1) {
		l = i + j;
		if ((l >>= 1) <= i)
			break;
		t = clist[c[l]].y;
		if (t > y)
			j = l;
		else if (t < y)
			i = l;
		else
			return l;
	}
	return l + 1;
}


static int stone(int *a, int n, int *b, int *c)
{
	int i, k, y, j, l;
	int oldc, tc, oldl;
	unsigned int numtries;
#if ENABLE_FEATURE_DIFF_MINIMAL
	const unsigned int bound =
		(option_mask32 & FLAG_d) ? UINT_MAX : MAX(256, isqrt(n));
#else
	const unsigned int bound = MAX(256, isqrt(n));
#endif

	k = 0;
	c[0] = newcand(0, 0, 0);
	for (i = 1; i <= n; i++) {
		j = a[i];
		if (j == 0)
			continue;
		y = -b[j];
		oldl = 0;
		oldc = c[0];
		numtries = 0;
		do {
			if (y <= clist[oldc].y)
				continue;
			l = search(c, k, y);
			if (l != oldl + 1)
				oldc = c[l - 1];
			if (l <= k) {
				if (clist[c[l]].y <= y)
					continue;
				tc = c[l];
				c[l] = newcand(i, y, oldc);
				oldc = tc;
				oldl = l;
				numtries++;
			} else {
				c[l] = newcand(i, y, oldc);
				k++;
				break;
			}
		} while ((y = b[++j]) > 0 && numtries < bound);
	}
	return k;
}


static void unravel(int p)
{
	struct cand *q;
	int i;

	for (i = 0; i <= nlen[0]; i++)
		J[i] = i <= pref ? i : i > nlen[0] - suff ? i + nlen[1] - nlen[0] : 0;
	for (q = clist + p; q->y != 0; q = clist + q->pred)
		J[q->x + pref] = q->y + pref;
}


static void unsort(struct line *f, int l, int *b)
{
	int *a, i;

	a = xmalloc((l + 1) * sizeof(int));
	for (i = 1; i <= l; i++)
		a[f[i].serial] = f[i].value;
	for (i = 1; i <= l; i++)
		b[i] = a[i];
	free(a);
}


static int skipline(FILE *f)
{
	int i, c;

	for (i = 1; (c = getc(f)) != '\n' && c != EOF; i++)
		continue;
	return i;
}


/*
 * Check does double duty:
 *  1.  ferret out any fortuitous correspondences due
 *      to confounding by hashing (which result in "jackpot")
 *  2.  collect random access indexes to the two files
 */
static NOINLINE void check(FILE *f1, FILE *f2)
{
	int i, j, jackpot, c, d;
	long ctold, ctnew;

	rewind(f1);
	rewind(f2);
	j = 1;
	ixold[0] = ixnew[0] = 0;
	jackpot = 0;
	ctold = ctnew = 0;
	for (i = 1; i <= nlen[0]; i++) {
		if (J[i] == 0) {
			ixold[i] = ctold += skipline(f1);
			continue;
		}
		while (j < J[i]) {
			ixnew[j] = ctnew += skipline(f2);
			j++;
		}
		if (option_mask32 & (FLAG_b | FLAG_w | FLAG_i)) {
			while (1) {
				c = getc(f1);
				d = getc(f2);
				/*
				 * GNU diff ignores a missing newline
				 * in one file if bflag || wflag.
				 */
				if ((option_mask32 & (FLAG_b | FLAG_w))
				 && ((c == EOF && d == '\n') || (c == '\n' && d == EOF))
				) {
					break;
				}
				ctold++;
				ctnew++;
				if ((option_mask32 & FLAG_b) && isspace(c) && isspace(d)) {
					do {
						if (c == '\n')
							break;
						ctold++;
						c = getc(f1);
					} while (isspace(c));
					do {
						if (d == '\n')
							break;
						ctnew++;
						d = getc(f2);
					} while (isspace(d));
				} else if (option_mask32 & FLAG_w) {
					while (isspace(c) && c != '\n') {
						c = getc(f1);
						ctold++;
					}
					while (isspace(d) && d != '\n') {
						d = getc(f2);
						ctnew++;
					}
				}
				if (c != d) {
					jackpot++;
					J[i] = 0;
					if (c != '\n' && c != EOF)
						ctold += skipline(f1);
					if (d != '\n' && c != EOF)
						ctnew += skipline(f2);
					break;
				}
				if (c == '\n' || c == EOF)
					break;
			}
		} else {
			while (1) {
				ctold++;
				ctnew++;
				c = getc(f1);
				d = getc(f2);
				if (c != d) {
					J[i] = 0;
					if (c != '\n' && c != EOF)
						ctold += skipline(f1);
/* was buggy? "if (d != '\n' && c != EOF)" */
					if (d != '\n' && d != EOF)
						ctnew += skipline(f2);
					break;
				}
				if (c == '\n' || c == EOF)
					break;
			}
		}
		ixold[i] = ctold;
		ixnew[j] = ctnew;
		j++;
	}
	for (; j <= nlen[1]; j++)
		ixnew[j] = ctnew += skipline(f2);
}


/* shellsort CACM #201 */
static void sort(struct line *a, int n)
{
	struct line *ai, *aim, w;
	int j, m = 0, k;

	if (n == 0)
		return;
	for (j = 1; j <= n; j *= 2)
		m = 2 * j - 1;
	for (m /= 2; m != 0; m /= 2) {
		k = n - m;
		for (j = 1; j <= k; j++) {
			for (ai = &a[j]; ai > a; ai -= m) {
				aim = &ai[m];
				if (aim < ai)
					break;	/* wraparound */
				if (aim->value > ai[0].value
				 || (aim->value == ai[0].value && aim->serial > ai[0].serial)
				) {
					break;
				}
				w.value = ai[0].value;
				ai[0].value = aim->value;
				aim->value = w.value;
				w.serial = ai[0].serial;
				ai[0].serial = aim->serial;
				aim->serial = w.serial;
			}
		}
	}
}


static void uni_range(int a, int b)
{
	if (a < b)
		printf("%d,%d", a, b - a + 1);
	else if (a == b)
		printf("%d", b);
	else
		printf("%d,0", b);
}


static void fetch(long *f, int a, int b, FILE *lb, int ch)
{
	int i, j, c, lastc, col, nc;

	if (a > b)
		return;
	for (i = a; i <= b; i++) {
		fseek(lb, f[i - 1], SEEK_SET);
		nc = f[i] - f[i - 1];
		if (ch != '\0') {
			putchar(ch);
			if (option_mask32 & FLAG_T)
				putchar('\t');
		}
		col = 0;
		for (j = 0, lastc = '\0'; j < nc; j++, lastc = c) {
			c = getc(lb);
			if (c == EOF) {
				printf("\n\\ No newline at end of file\n");
				return;
			}
			if (c == '\t' && (option_mask32 & FLAG_t)) {
				do {
					putchar(' ');
				} while (++col & 7);
			} else {
				putchar(c);
				col++;
			}
		}
	}
}


#if ENABLE_FEATURE_DIFF_BINARY
static int asciifile(FILE *f)
{
	int i, cnt;

	if (option_mask32 & FLAG_a)
		return 1;
	rewind(f);
	cnt = fread(g_read_buf, 1, COMMON_BUFSIZE, f);
	for (i = 0; i < cnt; i++) {
		if (!isprint(g_read_buf[i])
		 && !isspace(g_read_buf[i])
		) {
			return 0;
		}
	}
	return 1;
}
#else
#define asciifile(f) 1
#endif


/* dump accumulated "unified" diff changes */
static void dump_unified_vec(FILE *f1, FILE *f2)
{
	struct context_vec *cvp = context_vec_start;
	int lowa, upb, lowc, upd;
	int a, b, c, d;
	char ch;

	if (context_vec_start > context_vec_ptr)
		return;

	b = d = 0;			/* gcc */
	lowa = MAX(1, cvp->a - opt_U_context);
	upb = MIN(nlen[0], context_vec_ptr->b + opt_U_context);
	lowc = MAX(1, cvp->c - opt_U_context);
	upd = MIN(nlen[1], context_vec_ptr->d + opt_U_context);

	printf("@@ -");
	uni_range(lowa, upb);
	printf(" +");
	uni_range(lowc, upd);
	printf(" @@\n");

	/*
	 * Output changes in "unified" diff format--the old and new lines
	 * are printed together.
	 */
	for (; cvp <= context_vec_ptr; cvp++) {
		a = cvp->a;
		b = cvp->b;
		c = cvp->c;
		d = cvp->d;

		/*
		 * c: both new and old changes
		 * d: only changes in the old file
		 * a: only changes in the new file
		 */
		if (a <= b && c <= d)
			ch = 'c';
		else
			ch = (a <= b) ? 'd' : 'a';
#if 0
		switch (ch) {
		case 'c':
// fetch() seeks!
			fetch(ixold, lowa, a - 1, f1, ' ');
			fetch(ixold, a, b, f1, '-');
			fetch(ixnew, c, d, f2, '+');
			break;
		case 'd':
			fetch(ixold, lowa, a - 1, f1, ' ');
			fetch(ixold, a, b, f1, '-');
			break;
		case 'a':
			fetch(ixnew, lowc, c - 1, f2, ' ');
			fetch(ixnew, c, d, f2, '+');
			break;
		}
#else
		if (ch == 'c' || ch == 'd') {
			fetch(ixold, lowa, a - 1, f1, ' ');
			fetch(ixold, a, b, f1, '-');
		}
		if (ch == 'a')
			fetch(ixnew, lowc, c - 1, f2, ' ');
		if (ch == 'c' || ch == 'a')
			fetch(ixnew, c, d, f2, '+');
#endif
		lowa = b + 1;
		lowc = d + 1;
	}
	fetch(ixnew, d + 1, upd, f2, ' ');

	context_vec_ptr = context_vec_start - 1;
}


static void print_header(const char *file1, const char *file2)
{
	if (label1)
		printf("--- %s\n", label1);
	else
		printf("--- %s\t%s", file1, ctime(&stb1.st_mtime));
	if (label2)
		printf("+++ %s\n", label2);
	else
		printf("+++ %s\t%s", file2, ctime(&stb2.st_mtime));
}


/*
 * Indicate that there is a difference between lines a and b of the from file
 * to get to lines c to d of the to file.  If a is greater than b then there
 * are no lines in the from file involved and this means that there were
 * lines appended (beginning at b).  If c is greater than d then there are
 * lines missing from the to file.
 */
static void change(const char *file1, FILE *f1, const char *file2, FILE *f2,
			int a, int b, int c, int d)
{
	if ((a > b && c > d) || (option_mask32 & FLAG_q)) {
		anychange = 1;
		return;
	}

	/*
	 * Allocate change records as needed.
	 */
	if (context_vec_ptr == context_vec_end - 1) {
		ptrdiff_t offset = context_vec_ptr - context_vec_start;

		max_context <<= 1;
		context_vec_start = xrealloc(context_vec_start,
				max_context * sizeof(struct context_vec));
		context_vec_end = context_vec_start + max_context;
		context_vec_ptr = context_vec_start + offset;
	}
	if (anychange == 0) {
		/*
		 * Print the context/unidiff header first time through.
		 */
		print_header(file1, file2);
	} else if (a > context_vec_ptr->b + (2 * opt_U_context) + 1
	        && c > context_vec_ptr->d + (2 * opt_U_context) + 1
	) {
		/*
		 * If this change is more than 'context' lines from the
		 * previous change, dump the record and reset it.
		 */
// dump_unified_vec() seeks!
		dump_unified_vec(f1, f2);
	}
	context_vec_ptr++;
	context_vec_ptr->a = a;
	context_vec_ptr->b = b;
	context_vec_ptr->c = c;
	context_vec_ptr->d = d;
	anychange = 1;
}


static void output(const char *file1, FILE *f1, const char *file2, FILE *f2)
{
	/* Note that j0 and j1 can't be used as they are defined in math.h.
	 * This also allows the rather amusing variable 'j00'... */
	int m, i0, i1, j00, j01;

	rewind(f1);
	rewind(f2);
	m = nlen[0];
	J[0] = 0;
	J[m + 1] = nlen[1] + 1;
	for (i0 = 1; i0 <= m; i0 = i1 + 1) {
		while (i0 <= m && J[i0] == J[i0 - 1] + 1)
			i0++;
		j00 = J[i0 - 1] + 1;
		i1 = i0 - 1;
		while (i1 < m && J[i1 + 1] == 0)
			i1++;
		j01 = J[i1 + 1] - 1;
		J[i1] = j01;
// change() seeks!
		change(file1, f1, file2, f2, i0, i1, j00, j01);
	}
	if (m == 0) {
// change() seeks!
		change(file1, f1, file2, f2, 1, 0, 1, nlen[1]);
	}
	if (anychange != 0 && !(option_mask32 & FLAG_q)) {
// dump_unified_vec() seeks!
		dump_unified_vec(f1, f2);
	}
}

/*
 * The following code uses an algorithm due to Harold Stone,
 * which finds a pair of longest identical subsequences in
 * the two files.
 *
 * The major goal is to generate the match vector J.
 * J[i] is the index of the line in file1 corresponding
 * to line i in file0. J[i] = 0 if there is no
 * such line in file1.
 *
 * Lines are hashed so as to work in core. All potential
 * matches are located by sorting the lines of each file
 * on the hash (called "value"). In particular, this
 * collects the equivalence classes in file1 together.
 * Subroutine equiv replaces the value of each line in
 * file0 by the index of the first element of its
 * matching equivalence in (the reordered) file1.
 * To save space equiv squeezes file1 into a single
 * array member in which the equivalence classes
 * are simply concatenated, except that their first
 * members are flagged by changing sign.
 *
 * Next the indices that point into member are unsorted into
 * array class according to the original order of file0.
 *
 * The cleverness lies in routine stone. This marches
 * through the lines of file0, developing a vector klist
 * of "k-candidates". At step i a k-candidate is a matched
 * pair of lines x,y (x in file0, y in file1) such that
 * there is a common subsequence of length k
 * between the first i lines of file0 and the first y
 * lines of file1, but there is no such subsequence for
 * any smaller y. x is the earliest possible mate to y
 * that occurs in such a subsequence.
 *
 * Whenever any of the members of the equivalence class of
 * lines in file1 matable to a line in file0 has serial number
 * less than the y of some k-candidate, that k-candidate
 * with the smallest such y is replaced. The new
 * k-candidate is chained (via pred) to the current
 * k-1 candidate so that the actual subsequence can
 * be recovered. When a member has serial number greater
 * that the y of all k-candidates, the klist is extended.
 * At the end, the longest subsequence is pulled out
 * and placed in the array J by unravel
 *
 * With J in hand, the matches there recorded are
 * checked against reality to assure that no spurious
 * matches have crept in due to hashing. If they have,
 * they are broken, and "jackpot" is recorded--a harmless
 * matter except that a true match for a spuriously
 * mated line may now be unnecessarily reported as a change.
 *
 * Much of the complexity of the program comes simply
 * from trying to minimize core utilization and
 * maximize the range of doable problems by dynamically
 * allocating what is needed and reusing what is not.
 * The core requirements for problems larger than somewhat
 * are (in words) 2*length(file0) + length(file1) +
 * 3*(number of k-candidates installed), typically about
 * 6n words for files of length n.
 */
/* NB: files can be not REGular. The only sure thing that they
 * are not both DIRectories. */
static unsigned diffreg(const char *file1, const char *file2, int flags)
{
	int *member;     /* will be overlaid on nfile[1] */
	int *class;      /* will be overlaid on nfile[0] */
	int *klist;      /* will be overlaid on nfile[0] after class */
	FILE *f1;
	FILE *f2;
	unsigned rval;
	int i;

	anychange = 0;
	context_vec_ptr = context_vec_start - 1;
	tempname1 = tempname2 = NULL;

	/* Is any of them a directory? Then it's simple */
	if (S_ISDIR(stb1.st_mode) != S_ISDIR(stb2.st_mode))
		return (S_ISDIR(stb1.st_mode) ? D_ISDIR1 : D_ISDIR2);

	/* None of them are directories */
	rval = D_SAME;

	if (flags & D_EMPTY1)
		/* can't be stdin, but xfopen_stdin() is smaller code */
		file1 = bb_dev_null;
	f1 = xfopen_stdin(file1);
	if (flags & D_EMPTY2)
		file2 = bb_dev_null;
	f2 = xfopen_stdin(file2);

	/* NB: if D_EMPTY1/2 is set, other file is always a regular file,
	 * not pipe/fifo/chardev/etc - D_EMPTY is used by "diff -r" only,
	 * and it never diffs non-ordinary files in subdirs. */
	if (!(flags & (D_EMPTY1 | D_EMPTY2))) {
		/* Quick check whether they are different */
		/* NB: copies non-REG files to tempfiles and fills tempname1/2 */
		i = files_differ(f1, f2);
		if (i != 1) { /* not different? */
			if (i != 0) /* error? */
				exit_status |= 2;
			goto closem;
		}
	}

	if (!asciifile(f1) || !asciifile(f2)) {
		rval = D_BINARY;
		exit_status |= 1;
		goto closem;
	}

// Rewind inside!
	prepare(0, f1 /*, stb1.st_size*/);
	prepare(1, f2 /*, stb2.st_size*/);
	prune();
	sort(sfile[0], slen[0]);
	sort(sfile[1], slen[1]);

	member = (int *) nfile[1];
	equiv(sfile[0], slen[0], sfile[1], slen[1], member);
//TODO: xrealloc_vector?
	member = xrealloc(member, (slen[1] + 2) * sizeof(int));

	class = (int *) nfile[0];
	unsort(sfile[0], slen[0], class);
	class = xrealloc(class, (slen[0] + 2) * sizeof(int));

	klist = xmalloc((slen[0] + 2) * sizeof(int));
	clen = 0;
	clistlen = 100;
	clist = xmalloc(clistlen * sizeof(struct cand));
	i = stone(class, slen[0], member, klist);
	free(member);
	free(class);

	J = xrealloc(J, (nlen[0] + 2) * sizeof(int));
	unravel(klist[i]);
	free(clist);
	free(klist);

	ixold = xrealloc(ixold, (nlen[0] + 2) * sizeof(long));
	ixnew = xrealloc(ixnew, (nlen[1] + 2) * sizeof(long));
// Rewind inside!
	check(f1, f2);
// Rewind inside!
	output(file1, f1, file2, f2);

 closem:
	if (anychange) {
		exit_status |= 1;
		if (rval == D_SAME)
			rval = D_DIFFER;
	}
	fclose_if_not_stdin(f1);
	fclose_if_not_stdin(f2);
	if (tempname1) {
		unlink(tempname1);
		free(tempname1);
	}
	if (tempname2) {
		unlink(tempname2);
		free(tempname2);
	}
	return rval;
}


#if ENABLE_FEATURE_DIFF_DIR
static void do_diff(char *dir1, char *path1, char *dir2, char *path2)
{
	int flags = 0; /*D_HEADER;*/
	int val;
	char *fullpath1 = NULL; /* if -N */
	char *fullpath2 = NULL;

	if (path1)
		fullpath1 = concat_path_file(dir1, path1);
	if (path2)
		fullpath2 = concat_path_file(dir2, path2);

	if (!fullpath1 || stat(fullpath1, &stb1) != 0) {
		flags |= D_EMPTY1;
		memset(&stb1, 0, sizeof(stb1));
		if (path2) {
			free(fullpath1);
			fullpath1 = concat_path_file(dir1, path2);
		}
	}
	if (!fullpath2 || stat(fullpath2, &stb2) != 0) {
		flags |= D_EMPTY2;
		memset(&stb2, 0, sizeof(stb2));
		stb2.st_mode = stb1.st_mode;
		if (path1) {
			free(fullpath2);
			fullpath2 = concat_path_file(dir2, path1);
		}
	}

	if (stb1.st_mode == 0)
		stb1.st_mode = stb2.st_mode;

	if (S_ISDIR(stb1.st_mode) && S_ISDIR(stb2.st_mode)) {
		printf("Common subdirectories: %s and %s\n", fullpath1, fullpath2);
		goto ret;
	}

	if (!S_ISREG(stb1.st_mode) && !S_ISDIR(stb1.st_mode))
		val = D_SKIPPED1;
	else if (!S_ISREG(stb2.st_mode) && !S_ISDIR(stb2.st_mode))
		val = D_SKIPPED2;
	else {
		/* Both files are either REGular or DIRectories */
		val = diffreg(fullpath1, fullpath2, flags);
	}

	print_status(val, fullpath1, fullpath2 /*, NULL*/);
 ret:
	free(fullpath1);
	free(fullpath2);
}
#endif


#if ENABLE_FEATURE_DIFF_DIR
/* This function adds a filename to dl, the directory listing. */
static int FAST_FUNC add_to_dirlist(const char *filename,
		struct stat *sb UNUSED_PARAM,
		void *userdata,
		int depth UNUSED_PARAM)
{
	dl = xrealloc_vector(dl, 5, dl_count);
	dl[dl_count] = xstrdup(filename + (int)(ptrdiff_t)userdata);
	dl_count++;
	return TRUE;
}


/* This returns a sorted directory listing. */
static char **get_recursive_dirlist(char *path)
{
	dl_count = 0;
	dl = xzalloc(sizeof(dl[0]));

	/* We need to trim root directory prefix.
	 * Using void *userdata to specify its length,
	 * add_to_dirlist will remove it. */
	if (option_mask32 & FLAG_r) {
		recursive_action(path, ACTION_RECURSE|ACTION_FOLLOWLINKS,
					add_to_dirlist, /* file_action */
					NULL, /* dir_action */
					(void*)(ptrdiff_t)(strlen(path) + 1),
					0);
	} else {
		DIR *dp;
		struct dirent *ep;

		dp = warn_opendir(path);
		while ((ep = readdir(dp))) {
			if (!strcmp(ep->d_name, "..") || LONE_CHAR(ep->d_name, '.'))
				continue;
			add_to_dirlist(ep->d_name, NULL, (void*)(int)0, 0);
		}
		closedir(dp);
	}

	/* Sort dl alphabetically. */
	qsort_string_vector(dl, dl_count);

	dl[dl_count] = NULL;
	return dl;
}


static void diffdir(char *p1, char *p2)
{
	char **dirlist1, **dirlist2;
	char *dp1, *dp2;
	int pos;

	/* Check for trailing slashes. */
	dp1 = last_char_is(p1, '/');
	if (dp1 != NULL)
		*dp1 = '\0';
	dp2 = last_char_is(p2, '/');
	if (dp2 != NULL)
		*dp2 = '\0';

	/* Get directory listings for p1 and p2. */
	dirlist1 = get_recursive_dirlist(p1);
	dirlist2 = get_recursive_dirlist(p2);

	/* If -S was set, find the starting point. */
	if (opt_S_start) {
		while (*dirlist1 != NULL && strcmp(*dirlist1, opt_S_start) < 0)
			dirlist1++;
		while (*dirlist2 != NULL && strcmp(*dirlist2, opt_S_start) < 0)
			dirlist2++;
		if ((*dirlist1 == NULL) || (*dirlist2 == NULL))
			bb_error_msg(bb_msg_invalid_arg, "NULL", "-S");
	}

	/* Now that both dirlist1 and dirlist2 contain sorted directory
	 * listings, we can start to go through dirlist1. If both listings
	 * contain the same file, then do a normal diff. Otherwise, behaviour
	 * is determined by whether the -N flag is set. */
	while (*dirlist1 != NULL || *dirlist2 != NULL) {
		dp1 = *dirlist1;
		dp2 = *dirlist2;
		pos = dp1 == NULL ? 1 : (dp2 == NULL ? -1 : strcmp(dp1, dp2));
		if (pos == 0) {
			do_diff(p1, dp1, p2, dp2);
			dirlist1++;
			dirlist2++;
		} else if (pos < 0) {
			if (option_mask32 & FLAG_N)
				do_diff(p1, dp1, p2, NULL);
			else
				print_only(p1, dp1);
			dirlist1++;
		} else {
			if (option_mask32 & FLAG_N)
				do_diff(p1, NULL, p2, dp2);
			else
				print_only(p2, dp2);
			dirlist2++;
		}
	}
}
#endif


int diff_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int diff_main(int argc UNUSED_PARAM, char **argv)
{
	int gotstdin = 0;
	char *f1, *f2;
	llist_t *L_arg = NULL;

	INIT_G();

	/* exactly 2 params; collect multiple -L <label>; -U N */
	opt_complementary = "=2:L::U+";
	getopt32(argv, "abdiL:NqrsS:tTU:wu"
			"p" /* ignored (for compatibility) */,
			&L_arg, &opt_S_start, &opt_U_context);
	/*argc -= optind;*/
	argv += optind;
	while (L_arg) {
		if (label1 && label2)
			bb_show_usage();
		if (label1) /* then label2 is NULL */
			label2 = label1;
		label1 = llist_pop(&L_arg);
	}

	/*
	 * Do sanity checks, fill in stb1 and stb2 and call the appropriate
	 * driver routine.  Both drivers use the contents of stb1 and stb2.
	 */
	f1 = argv[0];
	f2 = argv[1];
	if (LONE_DASH(f1)) {
		fstat(STDIN_FILENO, &stb1);
		gotstdin++;
	} else
		xstat(f1, &stb1);
	if (LONE_DASH(f2)) {
		fstat(STDIN_FILENO, &stb2);
		gotstdin++;
	} else
		xstat(f2, &stb2);

	if (gotstdin && (S_ISDIR(stb1.st_mode) || S_ISDIR(stb2.st_mode)))
		bb_error_msg_and_die("can't compare stdin to a directory");

	if (S_ISDIR(stb1.st_mode) && S_ISDIR(stb2.st_mode)) {
#if ENABLE_FEATURE_DIFF_DIR
		diffdir(f1, f2);
		return exit_status;
#else
		bb_error_msg_and_die("no support for directory comparison");
#endif
	}

	if (S_ISDIR(stb1.st_mode)) { /* "diff dir file" */
		/* NB: "diff dir      dir2/dir3/file" must become
		 *     "diff dir/file dir2/dir3/file" */
		char *slash = strrchr(f2, '/');
		f1 = concat_path_file(f1, slash ? slash + 1 : f2);
		xstat(f1, &stb1);
	}
	if (S_ISDIR(stb2.st_mode)) {
		char *slash = strrchr(f1, '/');
		f2 = concat_path_file(f2, slash ? slash + 1 : f1);
		xstat(f2, &stb2);
	}

	/* diffreg can get non-regular files here,
	 * they are not both DIRestories */
	print_status((gotstdin > 1 ? D_SAME : diffreg(f1, f2, 0)),
			f1, f2 /*, NULL*/);
	return exit_status;
}
