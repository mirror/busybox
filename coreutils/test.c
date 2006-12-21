/* vi: set sw=4 ts=4: */
/*
 * test implementation for busybox
 *
 * Copyright (c) by a whole pile of folks:
 *
 *     test(1); version 7-like  --  author Erik Baalbergen
 *     modified by Eric Gisin to be used as built-in.
 *     modified by Arnold Robbins to add SVR3 compatibility
 *     (-x -c -b -p -u -g -k) plus Korn's -L -nt -ot -ef and new -S (socket).
 *     modified by J.T. Conklin for NetBSD.
 *     modified by Herbert Xu to be used as built-in in ash.
 *     modified by Erik Andersen <andersen@codepoet.org> to be used
 *     in busybox.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * Original copyright notice states:
 *     "This program is in the Public Domain."
 */

#include "busybox.h"
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <setjmp.h>

/* test(1) accepts the following grammar:
	oexpr	::= aexpr | aexpr "-o" oexpr ;
	aexpr	::= nexpr | nexpr "-a" aexpr ;
	nexpr	::= primary | "!" primary
	primary ::= unary-operator operand
		| operand binary-operator operand
		| operand
		| "(" oexpr ")"
		;
	unary-operator ::= "-r"|"-w"|"-x"|"-f"|"-d"|"-c"|"-b"|"-p"|
		"-u"|"-g"|"-k"|"-s"|"-t"|"-z"|"-n"|"-o"|"-O"|"-G"|"-L"|"-S";

	binary-operator ::= "="|"=="|"!="|"-eq"|"-ne"|"-ge"|"-gt"|"-le"|"-lt"|
			"-nt"|"-ot"|"-ef";
	operand ::= <any legal UNIX file name>
*/

enum token {
	EOI,
	FILRD,
	FILWR,
	FILEX,
	FILEXIST,
	FILREG,
	FILDIR,
	FILCDEV,
	FILBDEV,
	FILFIFO,
	FILSOCK,
	FILSYM,
	FILGZ,
	FILTT,
	FILSUID,
	FILSGID,
	FILSTCK,
	FILNT,
	FILOT,
	FILEQ,
	FILUID,
	FILGID,
	STREZ,
	STRNZ,
	STREQ,
	STRNE,
	STRLT,
	STRGT,
	INTEQ,
	INTNE,
	INTGE,
	INTGT,
	INTLE,
	INTLT,
	UNOT,
	BAND,
	BOR,
	LPAREN,
	RPAREN,
	OPERAND
};

enum token_types {
	UNOP,
	BINOP,
	BUNOP,
	BBINOP,
	PAREN
};

static const struct t_op {
	const char *op_text;
	short op_num, op_type;
} ops[] = {
	{
	"-r", FILRD, UNOP}, {
	"-w", FILWR, UNOP}, {
	"-x", FILEX, UNOP}, {
	"-e", FILEXIST, UNOP}, {
	"-f", FILREG, UNOP}, {
	"-d", FILDIR, UNOP}, {
	"-c", FILCDEV, UNOP}, {
	"-b", FILBDEV, UNOP}, {
	"-p", FILFIFO, UNOP}, {
	"-u", FILSUID, UNOP}, {
	"-g", FILSGID, UNOP}, {
	"-k", FILSTCK, UNOP}, {
	"-s", FILGZ, UNOP}, {
	"-t", FILTT, UNOP}, {
	"-z", STREZ, UNOP}, {
	"-n", STRNZ, UNOP}, {
	"-h", FILSYM, UNOP},    /* for backwards compat */
	{
	"-O", FILUID, UNOP}, {
	"-G", FILGID, UNOP}, {
	"-L", FILSYM, UNOP}, {
	"-S", FILSOCK, UNOP}, {
	"=", STREQ, BINOP}, {
	"==", STREQ, BINOP}, {
	"!=", STRNE, BINOP}, {
	"<", STRLT, BINOP}, {
	">", STRGT, BINOP}, {
	"-eq", INTEQ, BINOP}, {
	"-ne", INTNE, BINOP}, {
	"-ge", INTGE, BINOP}, {
	"-gt", INTGT, BINOP}, {
	"-le", INTLE, BINOP}, {
	"-lt", INTLT, BINOP}, {
	"-nt", FILNT, BINOP}, {
	"-ot", FILOT, BINOP}, {
	"-ef", FILEQ, BINOP}, {
	"!", UNOT, BUNOP}, {
	"-a", BAND, BBINOP}, {
	"-o", BOR, BBINOP}, {
	"(", LPAREN, PAREN}, {
	")", RPAREN, PAREN}, {
	0, 0, 0}
};

#ifdef CONFIG_FEATURE_TEST_64
typedef int64_t arith_t;
#else
typedef int arith_t;
#endif

static char **t_wp;
static struct t_op const *t_wp_op;
static gid_t *group_array;
static int ngroups;

static enum token t_lex(char *s);
static arith_t oexpr(enum token n);
static arith_t aexpr(enum token n);
static arith_t nexpr(enum token n);
static int binop(void);
static arith_t primary(enum token n);
static int filstat(char *nm, enum token mode);
static arith_t getn(const char *s);
static int newerf(const char *f1, const char *f2);
static int olderf(const char *f1, const char *f2);
static int equalf(const char *f1, const char *f2);
static int test_eaccess(char *path, int mode);
static int is_a_group_member(gid_t gid);
static void initialize_group_array(void);

static jmp_buf leaving;

int bb_test(int argc, char **argv)
{
	int res;

	if (LONE_CHAR(argv[0], '[')) {
		--argc;
		if (NOT_LONE_CHAR(argv[argc], ']')) {
			bb_error_msg("missing ]");
			return 2;
		}
		argv[argc] = NULL;
	} else if (strcmp(argv[0], "[[") == 0) {
		--argc;
		if (strcmp(argv[argc], "]]")) {
			bb_error_msg("missing ]]");
			return 2;
		}
		argv[argc] = NULL;
	}

	res = setjmp(leaving);
	if (res)
		return res;

	/* resetting ngroups is probably unnecessary.  it will
	 * force a new call to getgroups(), which prevents using
	 * group data fetched during a previous call.  but the
	 * only way the group data could be stale is if there's
	 * been an intervening call to setgroups(), and this
	 * isn't likely in the case of a shell.  paranoia
	 * prevails...
	 */
	 ngroups = 0;

	/* Implement special cases from POSIX.2, section 4.62.4 */
	switch (argc) {
	case 1:
		return 1;
	case 2:
		return *argv[1] == '\0';
	case 3:
		if (argv[1][0] == '!' && argv[1][1] == '\0') {
			return *argv[2] != '\0';
		}
		break;
	case 4:
		if (argv[1][0] != '!' || argv[1][1] != '\0') {
			if (t_lex(argv[2]), t_wp_op && t_wp_op->op_type == BINOP) {
				t_wp = &argv[1];
				return binop() == 0;
			}
		}
		break;
	case 5:
		if (argv[1][0] == '!' && argv[1][1] == '\0') {
			if (t_lex(argv[3]), t_wp_op && t_wp_op->op_type == BINOP) {
				t_wp = &argv[2];
				return binop() != 0;
			}
		}
		break;
	}

	t_wp = &argv[1];
	res = !oexpr(t_lex(*t_wp));

	if (*t_wp != NULL && *++t_wp != NULL) {
		bb_error_msg("%s: unknown operand", *t_wp);
		return 2;
	}
	return res;
}

static void syntax(const char *op, const char *msg)
{
	if (op && *op) {
		bb_error_msg("%s: %s", op, msg);
	} else {
		bb_error_msg("%s", msg);
	}
	longjmp(leaving, 2);
}

static arith_t oexpr(enum token n)
{
	arith_t res;

	res = aexpr(n);
	if (t_lex(*++t_wp) == BOR) {
		return oexpr(t_lex(*++t_wp)) || res;
	}
	t_wp--;
	return res;
}

static arith_t aexpr(enum token n)
{
	arith_t res;

	res = nexpr(n);
	if (t_lex(*++t_wp) == BAND)
		return aexpr(t_lex(*++t_wp)) && res;
	t_wp--;
	return res;
}

static arith_t nexpr(enum token n)
{
	if (n == UNOT)
		return !nexpr(t_lex(*++t_wp));
	return primary(n);
}

static arith_t primary(enum token n)
{
	arith_t res;

	if (n == EOI) {
		syntax(NULL, "argument expected");
	}
	if (n == LPAREN) {
		res = oexpr(t_lex(*++t_wp));
		if (t_lex(*++t_wp) != RPAREN)
			syntax(NULL, "closing paren expected");
		return res;
	}
	if (t_wp_op && t_wp_op->op_type == UNOP) {
		/* unary expression */
		if (*++t_wp == NULL)
			syntax(t_wp_op->op_text, "argument expected");
		switch (n) {
		case STREZ:
			return strlen(*t_wp) == 0;
		case STRNZ:
			return strlen(*t_wp) != 0;
		case FILTT:
			return isatty(getn(*t_wp));
		default:
			return filstat(*t_wp, n);
		}
	}

	if (t_lex(t_wp[1]), t_wp_op && t_wp_op->op_type == BINOP) {
		return binop();
	}

	return strlen(*t_wp) > 0;
}

static int binop(void)
{
	const char *opnd1, *opnd2;
	struct t_op const *op;

	opnd1 = *t_wp;
	(void) t_lex(*++t_wp);
	op = t_wp_op;

	if ((opnd2 = *++t_wp) == (char *) 0)
		syntax(op->op_text, "argument expected");

	switch (op->op_num) {
	case STREQ:
		return strcmp(opnd1, opnd2) == 0;
	case STRNE:
		return strcmp(opnd1, opnd2) != 0;
	case STRLT:
		return strcmp(opnd1, opnd2) < 0;
	case STRGT:
		return strcmp(opnd1, opnd2) > 0;
	case INTEQ:
		return getn(opnd1) == getn(opnd2);
	case INTNE:
		return getn(opnd1) != getn(opnd2);
	case INTGE:
		return getn(opnd1) >= getn(opnd2);
	case INTGT:
		return getn(opnd1) > getn(opnd2);
	case INTLE:
		return getn(opnd1) <= getn(opnd2);
	case INTLT:
		return getn(opnd1) < getn(opnd2);
	case FILNT:
		return newerf(opnd1, opnd2);
	case FILOT:
		return olderf(opnd1, opnd2);
	case FILEQ:
		return equalf(opnd1, opnd2);
	}
	/* NOTREACHED */
	return 1;
}

static int filstat(char *nm, enum token mode)
{
	struct stat s;
	unsigned int i;

	if (mode == FILSYM) {
#ifdef S_IFLNK
		if (lstat(nm, &s) == 0) {
			i = S_IFLNK;
			goto filetype;
		}
#endif
		return 0;
	}

	if (stat(nm, &s) != 0)
		return 0;

	switch (mode) {
	case FILRD:
		return test_eaccess(nm, R_OK) == 0;
	case FILWR:
		return test_eaccess(nm, W_OK) == 0;
	case FILEX:
		return test_eaccess(nm, X_OK) == 0;
	case FILEXIST:
		return 1;
	case FILREG:
		i = S_IFREG;
		goto filetype;
	case FILDIR:
		i = S_IFDIR;
		goto filetype;
	case FILCDEV:
		i = S_IFCHR;
		goto filetype;
	case FILBDEV:
		i = S_IFBLK;
		goto filetype;
	case FILFIFO:
#ifdef S_IFIFO
		i = S_IFIFO;
		goto filetype;
#else
		return 0;
#endif
	case FILSOCK:
#ifdef S_IFSOCK
		i = S_IFSOCK;
		goto filetype;
#else
		return 0;
#endif
	case FILSUID:
		i = S_ISUID;
		goto filebit;
	case FILSGID:
		i = S_ISGID;
		goto filebit;
	case FILSTCK:
		i = S_ISVTX;
		goto filebit;
	case FILGZ:
		return s.st_size > 0L;
	case FILUID:
		return s.st_uid == geteuid();
	case FILGID:
		return s.st_gid == getegid();
	default:
		return 1;
	}

  filetype:
	return ((s.st_mode & S_IFMT) == i);

  filebit:
	return ((s.st_mode & i) != 0);
}

static enum token t_lex(char *s)
{
	struct t_op const *op = ops;

	if (s == 0) {
		t_wp_op = (struct t_op *) 0;
		return EOI;
	}
	while (op->op_text) {
		if (strcmp(s, op->op_text) == 0) {
			t_wp_op = op;
			return op->op_num;
		}
		op++;
	}
	t_wp_op = (struct t_op *) 0;
	return OPERAND;
}

/* atoi with error detection */
static arith_t getn(const char *s)
{
	char *p;
#ifdef CONFIG_FEATURE_TEST_64
	long long r;
#else
	long r;
#endif

	errno = 0;
#ifdef CONFIG_FEATURE_TEST_64
	r = strtoll(s, &p, 10);
#else
	r = strtol(s, &p, 10);
#endif

	if (errno != 0)
		syntax(s, "out of range");

	if (*(skip_whitespace(p)))
		syntax(s, "bad number");

	return r;
}

static int newerf(const char *f1, const char *f2)
{
	struct stat b1, b2;

	return (stat(f1, &b1) == 0 &&
			stat(f2, &b2) == 0 && b1.st_mtime > b2.st_mtime);
}

static int olderf(const char *f1, const char *f2)
{
	struct stat b1, b2;

	return (stat(f1, &b1) == 0 &&
			stat(f2, &b2) == 0 && b1.st_mtime < b2.st_mtime);
}

static int equalf(const char *f1, const char *f2)
{
	struct stat b1, b2;

	return (stat(f1, &b1) == 0 &&
			stat(f2, &b2) == 0 &&
			b1.st_dev == b2.st_dev && b1.st_ino == b2.st_ino);
}

/* Do the same thing access(2) does, but use the effective uid and gid,
   and don't make the mistake of telling root that any file is
   executable. */
static int test_eaccess(char *path, int mode)
{
	struct stat st;
	unsigned int euid = geteuid();

	if (stat(path, &st) < 0)
		return -1;

	if (euid == 0) {
		/* Root can read or write any file. */
		if (mode != X_OK)
			return 0;

		/* Root can execute any file that has any one of the execute
		   bits set. */
		if (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
			return 0;
	}

	if (st.st_uid == euid)  /* owner */
		mode <<= 6;
	else if (is_a_group_member(st.st_gid))
		mode <<= 3;

	if (st.st_mode & mode)
		return 0;

	return -1;
}

static void initialize_group_array(void)
{
	ngroups = getgroups(0, NULL);
	if (ngroups > 0) {
		group_array = xmalloc(ngroups * sizeof(gid_t));
		getgroups(ngroups, group_array);
	}
}

/* Return non-zero if GID is one that we have in our groups list. */
static int is_a_group_member(gid_t gid)
{
	int i;

	/* Short-circuit if possible, maybe saving a call to getgroups(). */
	if (gid == getgid() || gid == getegid())
		return 1;

	if (ngroups == 0)
		initialize_group_array();

	/* Search through the list looking for GID. */
	for (i = 0; i < ngroups; i++)
		if (gid == group_array[i])
			return 1;

	return 0;
}


/* applet entry point */

int test_main(int argc, char **argv)
{
	return bb_test(argc, argv);
}

