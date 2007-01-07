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

#include "busybox.h"

#define FSIZE_MAX 32768

/*
 * Output flags
 */
#define D_HEADER        1	/* Print a header/footer between files */
#define D_EMPTY1        2	/* Treat first file as empty (/dev/null) */
#define D_EMPTY2        4	/* Treat second file as empty (/dev/null) */

/*
 * Status values for print_status() and diffreg() return values
 * Guide:
 * D_SAME - files are the same
 * D_DIFFER - files differ
 * D_BINARY - binary files differ
 * D_COMMON - subdirectory common to both dirs
 * D_ONLY - file only exists in one dir
 * D_MISMATCH1 - path1 a dir, path2 a file
 * D_MISMATCH2 - path1 a file, path2 a dir
 * D_ERROR - error occurred
 * D_SKIPPED1 - skipped path1 as it is a special file
 * D_SKIPPED2 - skipped path2 as it is a special file
 */

#define D_SAME		0
#define D_DIFFER	(1<<0)
#define D_BINARY	(1<<1)
#define D_COMMON	(1<<2)
#define D_ONLY		(1<<3)
#define D_MISMATCH1	(1<<4)
#define D_MISMATCH2	(1<<5)
#define D_ERROR		(1<<6)
#define D_SKIPPED1	(1<<7)
#define D_SKIPPED2	(1<<8)

/* Command line options */
#define FLAG_a	(1<<0)
#define FLAG_b	(1<<1)
#define FLAG_d  (1<<2)
#define FLAG_i	(1<<3)
#define FLAG_L	(1<<4)
#define FLAG_N	(1<<5)
#define FLAG_q	(1<<6)
#define FLAG_r	(1<<7)
#define FLAG_s	(1<<8)
#define FLAG_S	(1<<9)
#define FLAG_t	(1<<10)
#define FLAG_T	(1<<11)
#define FLAG_U	(1<<12)
#define	FLAG_w	(1<<13)

/* XXX: FIXME: the following variables should be static, but gcc currently
 * creates a much bigger object if we do this. [which version of gcc? --vda] */
/* 4.x, IIRC also 3.x --bernhard */
/* This is the default number of lines of context. */
int context = 3;
int status;
char *start;
const char *label1;
const char *label2;
struct stat stb1, stb2;
char **dl;
USE_FEATURE_DIFF_DIR(int dl_count;)

struct cand {
	int x;
	int y;
	int pred;
};

struct line {
	int serial;
	int value;
} *file[2];

/*
 * The following struct is used to record change information
 * doing a "context" or "unified" diff.  (see routine "change" to
 * understand the highly mnemonic field names)
 */
struct context_vec {
	int a;				/* start line in old file */
	int b;				/* end line in old file */
	int c;				/* start line in new file */
	int d;				/* end line in new file */
};

static int *J;			/* will be overlaid on class */
static int *class;		/* will be overlaid on file[0] */
static int *klist;		/* will be overlaid on file[0] after class */
static int *member;		/* will be overlaid on file[1] */
static int clen;
static int len[2];
static int pref, suff;	/* length of prefix and suffix */
static int slen[2];
static smallint anychange;
static long *ixnew;		/* will be overlaid on file[1] */
static long *ixold;		/* will be overlaid on klist */
static struct cand *clist;	/* merely a free storage pot for candidates */
static int clistlen;	/* the length of clist */
static struct line *sfile[2];	/* shortened by pruning common prefix/suffix */
static struct context_vec *context_vec_start;
static struct context_vec *context_vec_end;
static struct context_vec *context_vec_ptr;


static void print_only(const char *path, size_t dirlen, const char *entry)
{
	if (dirlen > 1)
		dirlen--;
	printf("Only in %.*s: %s\n", (int) dirlen, path, entry);
}


static void print_status(int val, char *path1, char *path2, char *entry)
{
	const char * const _entry = entry ? entry : "";
	char * const _path1 = entry ? concat_path_file(path1, _entry) : path1;
	char * const _path2 = entry ? concat_path_file(path2, _entry) : path2;

	switch (val) {
	case D_ONLY:
		print_only(path1, strlen(path1), entry);
		break;
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
	case D_MISMATCH1:
		printf("File %s is a %s while file %s is a %s\n",
			   _path1, "directory", _path2, "regular file");
		break;
	case D_MISMATCH2:
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
	if (entry) {
		free(_path1);
		free(_path2);
	}
}
static void fiddle_sum(int *sum, int t)
{
	*sum = (int)(*sum * 127 + t);
}
/*
 * Hash function taken from Robert Sedgewick, Algorithms in C, 3d ed., p 578.
 */
static int readhash(FILE * f)
{
	int i, t, space;
	int sum;

	sum = 1;
	space = 0;
	if (!(option_mask32 & FLAG_b) && !(option_mask32 & FLAG_w)) {
		for (i = 0; (t = getc(f)) != '\n'; i++) {
			if (t == EOF) {
				if (i == 0)
					return 0;
				break;
			}
			fiddle_sum(&sum, t);
		}
	} else {
		for (i = 0;;) {
			switch (t = getc(f)) {
			case '\t':
			case '\r':
			case '\v':
			case '\f':
			case ' ':
				space++;
				continue;
			default:
				if (space && !(option_mask32 & FLAG_w)) {
					i++;
					space = 0;
				}
				fiddle_sum(&sum, t);
				i++;
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


/*
 * Check to see if the given files differ.
 * Returns 0 if they are the same, 1 if different, and -1 on error.
 */
static int files_differ(FILE * f1, FILE * f2, int flags)
{
	size_t i, j;

	if ((flags & (D_EMPTY1 | D_EMPTY2)) || stb1.st_size != stb2.st_size ||
		(stb1.st_mode & S_IFMT) != (stb2.st_mode & S_IFMT))
		return 1;
	while (1) {
		i = fread(bb_common_bufsiz1,            1, BUFSIZ/2, f1);
		j = fread(bb_common_bufsiz1 + BUFSIZ/2, 1, BUFSIZ/2, f2);
		if (i != j)
			return 1;
		if (i == 0 && j == 0) {
			if (ferror(f1) || ferror(f2))
				return 1;
			return 0;
		}
		if (memcmp(bb_common_bufsiz1,
		           bb_common_bufsiz1 + BUFSIZ/2, i) != 0)
			return 1;
	}
}


static void prepare(int i, FILE * fd, off_t filesize)
{
	struct line *p;
	int h;
	size_t j, sz;

	rewind(fd);

	sz = (filesize <= FSIZE_MAX ? filesize : FSIZE_MAX) / 25;
	if (sz < 100)
		sz = 100;

	p = xmalloc((sz + 3) * sizeof(struct line));
	j = 0;
	while ((h = readhash(fd))) {
		if (j == sz) {
			sz = sz * 3 / 2;
			p = xrealloc(p, (sz + 3) * sizeof(struct line));
		}
		p[++j].value = h;
	}
	len[i] = j;
	file[i] = p;
}


static void prune(void)
{
	int i, j;

	for (pref = 0; pref < len[0] && pref < len[1] &&
		 file[0][pref + 1].value == file[1][pref + 1].value; pref++)
		 ;
	for (suff = 0; suff < len[0] - pref && suff < len[1] - pref &&
		 file[0][len[0] - suff].value == file[1][len[1] - suff].value;
		 suff++)
		 ;
	for (j = 0; j < 2; j++) {
		sfile[j] = file[j] + pref;
		slen[j] = len[j] - pref - suff;
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
	int y, x = 1;

	if (n == 0)
		return 0;

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

	for (i = 0; i <= len[0]; i++)
		J[i] = i <= pref ? i : i > len[0] - suff ? i + len[1] - len[0] : 0;
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


static int skipline(FILE * f)
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
static void check(FILE * f1, FILE * f2)
{
	int i, j, jackpot, c, d;
	long ctold, ctnew;

	rewind(f1);
	rewind(f2);
	j = 1;
	ixold[0] = ixnew[0] = 0;
	jackpot = 0;
	ctold = ctnew = 0;
	for (i = 1; i <= len[0]; i++) {
		if (J[i] == 0) {
			ixold[i] = ctold += skipline(f1);
			continue;
		}
		while (j < J[i]) {
			ixnew[j] = ctnew += skipline(f2);
			j++;
		}
		if ((option_mask32 & FLAG_b) || (option_mask32 & FLAG_w)
			|| (option_mask32 & FLAG_i)) {
			while (1) {
				c = getc(f1);
				d = getc(f2);
				/*
				 * GNU diff ignores a missing newline
				 * in one file if bflag || wflag.
				 */
				if (((option_mask32 & FLAG_b) || (option_mask32 & FLAG_w)) &&
					((c == EOF && d == '\n') || (c == '\n' && d == EOF))) {
					break;
				}
				ctold++;
				ctnew++;
				if ((option_mask32 & FLAG_b) && isspace(c) && isspace(d)) {
					do {
						if (c == '\n')
							break;
						ctold++;
					} while (isspace(c = getc(f1)));
					do {
						if (d == '\n')
							break;
						ctnew++;
					} while (isspace(d = getc(f2)));
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
				if ((c = getc(f1)) != (d = getc(f2))) {
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
		}
		ixold[i] = ctold;
		ixnew[j] = ctnew;
		j++;
	}
	for (; j <= len[1]; j++)
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
				if (aim->value > ai[0].value ||
					(aim->value == ai[0].value && aim->serial > ai[0].serial))
					break;
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


static void fetch(long *f, int a, int b, FILE * lb, int ch)
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
			if ((c = getc(lb)) == EOF) {
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
	return;
}


static int asciifile(FILE * f)
{
#if ENABLE_FEATURE_DIFF_BINARY
	int i, cnt;
#endif

	if ((option_mask32 & FLAG_a) || f == NULL)
		return 1;

#if ENABLE_FEATURE_DIFF_BINARY
	rewind(f);
	cnt = fread(bb_common_bufsiz1, 1, BUFSIZ, f);
	for (i = 0; i < cnt; i++) {
		if (!isprint(bb_common_bufsiz1[i])
		 && !isspace(bb_common_bufsiz1[i])) {
			return 0;
		}
	}
#endif
	return 1;
}


/* dump accumulated "unified" diff changes */
static void dump_unified_vec(FILE * f1, FILE * f2)
{
	struct context_vec *cvp = context_vec_start;
	int lowa, upb, lowc, upd;
	int a, b, c, d;
	char ch;

	if (context_vec_start > context_vec_ptr)
		return;

	b = d = 0;			/* gcc */
	lowa = MAX(1, cvp->a - context);
	upb = MIN(len[0], context_vec_ptr->b + context);
	lowc = MAX(1, cvp->c - context);
	upd = MIN(len[1], context_vec_ptr->d + context);

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
		if (ch == 'c' || ch == 'd') {
			fetch(ixold, lowa, a - 1, f1, ' ');
			fetch(ixold, a, b, f1, '-');
		}
		if (ch == 'a')
			fetch(ixnew, lowc, c - 1, f2, ' ');
		if (ch == 'c' || ch == 'a')
			fetch(ixnew, c, d, f2, '+');
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
static void change(char *file1, FILE * f1, char *file2, FILE * f2, int a,
				   int b, int c, int d)
{
	static size_t max_context = 64;

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
	} else if (a > context_vec_ptr->b + (2 * context) + 1 &&
			   c > context_vec_ptr->d + (2 * context) + 1) {
		/*
		 * If this change is more than 'context' lines from the
		 * previous change, dump the record and reset it.
		 */
		dump_unified_vec(f1, f2);
	}
	context_vec_ptr++;
	context_vec_ptr->a = a;
	context_vec_ptr->b = b;
	context_vec_ptr->c = c;
	context_vec_ptr->d = d;
	anychange = 1;
}


static void output(char *file1, FILE * f1, char *file2, FILE * f2)
{
	/* Note that j0 and j1 can't be used as they are defined in math.h.
	 * This also allows the rather amusing variable 'j00'... */
	int m, i0, i1, j00, j01;

	rewind(f1);
	rewind(f2);
	m = len[0];
	J[0] = 0;
	J[m + 1] = len[1] + 1;
	for (i0 = 1; i0 <= m; i0 = i1 + 1) {
		while (i0 <= m && J[i0] == J[i0 - 1] + 1)
			i0++;
		j00 = J[i0 - 1] + 1;
		i1 = i0 - 1;
		while (i1 < m && J[i1 + 1] == 0)
			i1++;
		j01 = J[i1 + 1] - 1;
		J[i1] = j01;
		change(file1, f1, file2, f2, i0, i1, j00, j01);
	}
	if (m == 0) {
		change(file1, f1, file2, f2, 1, 0, 1, len[1]);
	}
	if (anychange != 0 && !(option_mask32 & FLAG_q)) {
		dump_unified_vec(f1, f2);
	}
}

/*
 *      The following code uses an algorithm due to Harold Stone,
 *      which finds a pair of longest identical subsequences in
 *      the two files.
 *
 *      The major goal is to generate the match vector J.
 *      J[i] is the index of the line in file1 corresponding
 *      to line i file0. J[i] = 0 if there is no
 *      such line in file1.
 *
 *      Lines are hashed so as to work in core. All potential
 *      matches are located by sorting the lines of each file
 *      on the hash (called ``value''). In particular, this
 *      collects the equivalence classes in file1 together.
 *      Subroutine equiv replaces the value of each line in
 *      file0 by the index of the first element of its
 *      matching equivalence in (the reordered) file1.
 *      To save space equiv squeezes file1 into a single
 *      array member in which the equivalence classes
 *      are simply concatenated, except that their first
 *      members are flagged by changing sign.
 *
 *      Next the indices that point into member are unsorted into
 *      array class according to the original order of file0.
 *
 *      The cleverness lies in routine stone. This marches
 *      through the lines of file0, developing a vector klist
 *      of "k-candidates". At step i a k-candidate is a matched
 *      pair of lines x,y (x in file0 y in file1) such that
 *      there is a common subsequence of length k
 *      between the first i lines of file0 and the first y
 *      lines of file1, but there is no such subsequence for
 *      any smaller y. x is the earliest possible mate to y
 *      that occurs in such a subsequence.
 *
 *      Whenever any of the members of the equivalence class of
 *      lines in file1 matable to a line in file0 has serial number
 *      less than the y of some k-candidate, that k-candidate
 *      with the smallest such y is replaced. The new
 *      k-candidate is chained (via pred) to the current
 *      k-1 candidate so that the actual subsequence can
 *      be recovered. When a member has serial number greater
 *      that the y of all k-candidates, the klist is extended.
 *      At the end, the longest subsequence is pulled out
 *      and placed in the array J by unravel
 *
 *      With J in hand, the matches there recorded are
 *      checked against reality to assure that no spurious
 *      matches have crept in due to hashing. If they have,
 *      they are broken, and "jackpot" is recorded--a harmless
 *      matter except that a true match for a spuriously
 *      mated line may now be unnecessarily reported as a change.
 *
 *      Much of the complexity of the program comes simply
 *      from trying to minimize core utilization and
 *      maximize the range of doable problems by dynamically
 *      allocating what is needed and reusing what is not.
 *      The core requirements for problems larger than somewhat
 *      are (in words) 2*length(file0) + length(file1) +
 *      3*(number of k-candidates installed),  typically about
 *      6n words for files of length n.
 */
static unsigned diffreg(char * ofile1, char * ofile2, int flags)
{
	char *file1 = ofile1;
	char *file2 = ofile2;
	FILE *f1 = stdin, *f2 = stdin;
	unsigned rval;
	int i;

	anychange = 0;
	context_vec_ptr = context_vec_start - 1;

	if (S_ISDIR(stb1.st_mode) != S_ISDIR(stb2.st_mode))
		return (S_ISDIR(stb1.st_mode) ? D_MISMATCH1 : D_MISMATCH2);

	rval = D_SAME;

	if (LONE_DASH(file1) && LONE_DASH(file2))
		goto closem;

	if (flags & D_EMPTY1)
		f1 = xfopen(bb_dev_null, "r");
	else if (NOT_LONE_DASH(file1))
		f1 = xfopen(file1, "r");
	if (flags & D_EMPTY2)
		f2 = xfopen(bb_dev_null, "r");
	else if (NOT_LONE_DASH(file2))
		f2 = xfopen(file2, "r");

/* We can't diff non-seekable stream - we use rewind(), fseek().
 * This can be fixed (volunteers?).
 * Meanwhile we should check it here by stat'ing input fds,
 * but I am lazy and check that in main() instead.
 * Check in main won't catch "diffing fifos buried in subdirectories"
 * failure scenario - not very likely in real life... */

	i = files_differ(f1, f2, flags);
	if (i == 0)
		goto closem;
	else if (i != 1) {	/* 1 == ok */
		/* error */
		status |= 2;
		goto closem;
	}

	if (!asciifile(f1) || !asciifile(f2)) {
		rval = D_BINARY;
		status |= 1;
		goto closem;
	}

	prepare(0, f1, stb1.st_size);
	prepare(1, f2, stb2.st_size);
	prune();
	sort(sfile[0], slen[0]);
	sort(sfile[1], slen[1]);

	member = (int *) file[1];
	equiv(sfile[0], slen[0], sfile[1], slen[1], member);
	member = xrealloc(member, (slen[1] + 2) * sizeof(int));

	class = (int *) file[0];
	unsort(sfile[0], slen[0], class);
	class = xrealloc(class, (slen[0] + 2) * sizeof(int));

	klist = xmalloc((slen[0] + 2) * sizeof(int));
	clen = 0;
	clistlen = 100;
	clist = xmalloc(clistlen * sizeof(struct cand));
	i = stone(class, slen[0], member, klist);
	free(member);
	free(class);

	J = xrealloc(J, (len[0] + 2) * sizeof(int));
	unravel(klist[i]);
	free(clist);
	free(klist);

	ixold = xrealloc(ixold, (len[0] + 2) * sizeof(long));
	ixnew = xrealloc(ixnew, (len[1] + 2) * sizeof(long));
	check(f1, f2);
	output(file1, f1, file2, f2);

 closem:
	if (anychange) {
		status |= 1;
		if (rval == D_SAME)
			rval = D_DIFFER;
	}
	fclose_if_not_stdin(f1);
	fclose_if_not_stdin(f2);
	if (file1 != ofile1)
		free(file1);
	if (file2 != ofile2)
		free(file2);
	return rval;
}


#if ENABLE_FEATURE_DIFF_DIR
static void do_diff(char *dir1, char *path1, char *dir2, char *path2)
{
	int flags = D_HEADER;
	int val;

	char *fullpath1 = concat_path_file(dir1, path1);
	char *fullpath2 = concat_path_file(dir2, path2);

	if (stat(fullpath1, &stb1) != 0) {
		flags |= D_EMPTY1;
		memset(&stb1, 0, sizeof(stb1));
		if (ENABLE_FEATURE_CLEAN_UP)
			free(fullpath1);
		fullpath1 = concat_path_file(dir1, path2);
	}
	if (stat(fullpath2, &stb2) != 0) {
		flags |= D_EMPTY2;
		memset(&stb2, 0, sizeof(stb2));
		stb2.st_mode = stb1.st_mode;
		if (ENABLE_FEATURE_CLEAN_UP)
			free(fullpath2);
		fullpath2 = concat_path_file(dir2, path1);
	}

	if (stb1.st_mode == 0)
		stb1.st_mode = stb2.st_mode;

	if (S_ISDIR(stb1.st_mode) && S_ISDIR(stb2.st_mode)) {
		printf("Common subdirectories: %s and %s\n", fullpath1, fullpath2);
		return;
	}

	if (!S_ISREG(stb1.st_mode) && !S_ISDIR(stb1.st_mode))
		val = D_SKIPPED1;
	else if (!S_ISREG(stb2.st_mode) && !S_ISDIR(stb2.st_mode))
		val = D_SKIPPED2;
	else
		val = diffreg(fullpath1, fullpath2, flags);

	print_status(val, fullpath1, fullpath2, NULL);
}
#endif


#if ENABLE_FEATURE_DIFF_DIR
static int dir_strcmp(const void *p1, const void *p2)
{
	return strcmp(*(char *const *) p1, *(char *const *) p2);
}


/* This function adds a filename to dl, the directory listing. */
static int add_to_dirlist(const char *filename,
		struct stat ATTRIBUTE_UNUSED * sb, void *userdata,
		int depth ATTRIBUTE_UNUSED)
{
	dl_count++;
	dl = xrealloc(dl, dl_count * sizeof(char *));
	dl[dl_count - 1] = xstrdup(filename);
	if (option_mask32 & FLAG_r) {
		int *pp = (int *) userdata;
		int path_len = *pp + 1;

		dl[dl_count - 1] = &(dl[dl_count - 1])[path_len];
	}
	return TRUE;
}


/* This returns a sorted directory listing. */
static char **get_dir(char *path)
{
	int i;
	char **retval;

	/* If -r has been set, then the recursive_action function will be
	 * used. Unfortunately, this outputs the root directory along with
	 * the recursed paths, so use void *userdata to specify the string
	 * length of the root directory. It can then be removed in
	 * add_to_dirlist. */

	int path_len = strlen(path);
	void *userdata = &path_len;

	/* Reset dl_count - there's no need to free dl as xrealloc does
	 * the job nicely. */
	dl_count = 0;

	/* Now fill dl with a listing. */
	if (option_mask32 & FLAG_r)
		recursive_action(path, TRUE, TRUE, FALSE, add_to_dirlist, NULL,
						 userdata, 0);
	else {
		DIR *dp;
		struct dirent *ep;

		dp = warn_opendir(path);
		while ((ep = readdir(dp))) {
			if (!strcmp(ep->d_name, "..") || LONE_CHAR(ep->d_name, '.'))
				continue;
			add_to_dirlist(ep->d_name, NULL, NULL, 0);
		}
		closedir(dp);
	}

	/* Sort dl alphabetically. */
	qsort(dl, dl_count, sizeof(char *), dir_strcmp);

	/* Copy dl so that we can return it. */
	retval = xmalloc(dl_count * sizeof(char *));
	for (i = 0; i < dl_count; i++)
		retval[i] = xstrdup(dl[i]);

	return retval;
}


static void diffdir(char *p1, char *p2)
{
	char **dirlist1, **dirlist2;
	char *dp1, *dp2;
	int dirlist1_count, dirlist2_count;
	int pos;

	/* Check for trailing slashes. */

	dp1 = last_char_is(p1, '/');
	if (dp1 != NULL)
		*dp1 = '\0';
	dp2 = last_char_is(p2, '/');
	if (dp2 != NULL)
		*dp2 = '\0';

	/* Get directory listings for p1 and p2. */

	dirlist1 = get_dir(p1);
	dirlist1_count = dl_count;
	dirlist1[dirlist1_count] = NULL;
	dirlist2 = get_dir(p2);
	dirlist2_count = dl_count;
	dirlist2[dirlist2_count] = NULL;

	/* If -S was set, find the starting point. */
	if (start) {
		while (*dirlist1 != NULL && strcmp(*dirlist1, start) < 0)
			dirlist1++;
		while (*dirlist2 != NULL && strcmp(*dirlist2, start) < 0)
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
		pos = dp1 == NULL ? 1 : dp2 == NULL ? -1 : strcmp(dp1, dp2);
		if (pos == 0) {
			do_diff(p1, dp1, p2, dp2);
			dirlist1++;
			dirlist2++;
		} else if (pos < 0) {
			if (option_mask32 & FLAG_N)
				do_diff(p1, dp1, p2, NULL);
			else
				print_only(p1, strlen(p1) + 1, dp1);
			dirlist1++;
		} else {
			if (option_mask32 & FLAG_N)
				do_diff(p1, NULL, p2, dp2);
			else
				print_only(p2, strlen(p2) + 1, dp2);
			dirlist2++;
		}
	}
}
#endif


int diff_main(int argc, char **argv)
{
	smallint gotstdin = 0;
	char *U_opt;
	char *f1, *f2;
	llist_t *L_arg = NULL;

	/* exactly 2 params; collect multiple -L <label> */
	opt_complementary = "=2:L::";
	getopt32(argc, argv, "abdiL:NqrsS:tTU:wu"
			"p" /* ignored (for compatibility) */,
			&L_arg, &start, &U_opt);
	/*argc -= optind;*/
	argv += optind;
	while (L_arg) {
		if (label1 && label2)
			bb_show_usage();
		if (!label1)
			label1 = L_arg->data;
		else { /* then label2 is NULL */
			label2 = label1;
			label1 = L_arg->data;
		}
		/* we leak L_arg here... */
		L_arg = L_arg->link;
	}
	if (option_mask32 & FLAG_U)
		context = xatou_range(U_opt, 1, INT_MAX);

	/*
	 * Do sanity checks, fill in stb1 and stb2 and call the appropriate
	 * driver routine.  Both drivers use the contents of stb1 and stb2.
	 */

	f1 = argv[0];
	f2 = argv[1];
	if (LONE_DASH(f1)) {
		fstat(STDIN_FILENO, &stb1);
		gotstdin = 1;
	} else
		xstat(f1, &stb1);
	if (LONE_DASH(f2)) {
		fstat(STDIN_FILENO, &stb2);
		gotstdin = 1;
	} else
		xstat(f2, &stb2);
	if (gotstdin && (S_ISDIR(stb1.st_mode) || S_ISDIR(stb2.st_mode)))
		bb_error_msg_and_die("can't compare - to a directory");
	if (S_ISDIR(stb1.st_mode) && S_ISDIR(stb2.st_mode)) {
#if ENABLE_FEATURE_DIFF_DIR
		diffdir(f1, f2);
#else
		bb_error_msg_and_die("directory comparison not supported");
#endif
	} else {
		if (S_ISDIR(stb1.st_mode)) {
			f1 = concat_path_file(f1, f2);
			xstat(f1, &stb1);
		}
		if (S_ISDIR(stb2.st_mode)) {
			f2 = concat_path_file(f2, f1);
			xstat(f2, &stb2);
		}
/* XXX: FIXME: */
/* We can't diff e.g. stdin supplied by a pipe - we use rewind(), fseek().
 * This can be fixed (volunteers?) */
		if (!S_ISREG(stb1.st_mode) || !S_ISREG(stb2.st_mode))
			bb_error_msg_and_die("can't diff non-seekable stream");
		print_status(diffreg(f1, f2, 0), f1, f2, NULL);
	}
	return status;
}
