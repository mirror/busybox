/* vi: set sw=4 ts=4: */
/*
 * awk implementation for busybox
 *
 * Copyright (C) 2002 by Dmitry Zakharov <dmit@crp.bank.gov.ua>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <getopt.h>
#include <regex.h>

#include "busybox.h"


#define	MAXVARFMT	240
#define	MINNVBLOCK	64

/* variable flags */
#define	VF_NUMBER	0x0001	/* 1 = primary type is number */
#define	VF_ARRAY	0x0002	/* 1 = it's an array */

#define	VF_CACHED	0x0100	/* 1 = num/str value has cached str/num eq */
#define	VF_USER		0x0200	/* 1 = user input (may be numeric string) */
#define	VF_SPECIAL	0x0400	/* 1 = requires extra handling when changed */
#define	VF_WALK		0x0800	/* 1 = variable has alloc'd x.walker list */
#define	VF_FSTR		0x1000	/* 1 = string points to fstring buffer */
#define	VF_CHILD	0x2000	/* 1 = function arg; x.parent points to source */
#define	VF_DIRTY	0x4000	/* 1 = variable was set explicitly */

/* these flags are static, don't change them when value is changed */
#define	VF_DONTTOUCH (VF_ARRAY | VF_SPECIAL | VF_WALK | VF_CHILD | VF_DIRTY)

/* Variable */
typedef struct var_s {
	unsigned short type;		/* flags */
	double number;
	char *string;
	union {
		int aidx;				/* func arg index (on compilation stage) */
		struct xhash_s *array;	/* array ptr */
		struct var_s *parent;	/* for func args, ptr to actual parameter */
		char **walker;			/* list of array elements (for..in) */
	} x;
} var;

/* Node chain (pattern-action chain, BEGIN, END, function bodies) */
typedef struct chain_s {
	struct node_s *first;
	struct node_s *last;
	char *programname;
} chain;

/* Function */
typedef struct func_s {
	unsigned short nargs;
	struct chain_s body;
} func;

/* I/O stream */
typedef struct rstream_s {
	FILE *F;
	char *buffer;
	int adv;
	int size;
	int pos;
	unsigned short is_pipe;
} rstream;

typedef struct hash_item_s {
	union {
		struct var_s v;			/* variable/array hash */
		struct rstream_s rs;	/* redirect streams hash */
		struct func_s f;		/* functions hash */
	} data;
	struct hash_item_s *next;	/* next in chain */
	char name[1];				/* really it's longer */
} hash_item;

typedef struct xhash_s {
	unsigned int nel;					/* num of elements */
	unsigned int csize;					/* current hash size */
	unsigned int nprime;				/* next hash size in PRIMES[] */
	unsigned int glen;					/* summary length of item names */
	struct hash_item_s **items;
} xhash;

/* Tree node */
typedef struct node_s {
	unsigned long info;
	unsigned short lineno;
	union {
		struct node_s *n;
		var *v;
		int i;
		char *s;
		regex_t *re;
	} l;
	union {
		struct node_s *n;
		regex_t *ire;
		func *f;
		int argno;
	} r;
	union {
		struct node_s *n;
	} a;
} node;

/* Block of temporary variables */
typedef struct nvblock_s {
	int size;
	var *pos;
	struct nvblock_s *prev;
	struct nvblock_s *next;
	var nv[0];
} nvblock;

typedef struct tsplitter_s {
	node n;
	regex_t re[2];
} tsplitter;

/* simple token classes */
/* Order and hex values are very important!!!  See next_token() */
#define	TC_SEQSTART	 1				/* ( */
#define	TC_SEQTERM	(1 << 1)		/* ) */
#define	TC_REGEXP	(1 << 2)		/* /.../ */
#define	TC_OUTRDR	(1 << 3)		/* | > >> */
#define	TC_UOPPOST	(1 << 4)		/* unary postfix operator */
#define	TC_UOPPRE1	(1 << 5)		/* unary prefix operator */
#define	TC_BINOPX	(1 << 6)		/* two-opnd operator */
#define	TC_IN		(1 << 7)
#define	TC_COMMA	(1 << 8)
#define	TC_PIPE		(1 << 9)		/* input redirection pipe */
#define	TC_UOPPRE2	(1 << 10)		/* unary prefix operator */
#define	TC_ARRTERM	(1 << 11)		/* ] */
#define	TC_GRPSTART	(1 << 12)		/* { */
#define	TC_GRPTERM	(1 << 13)		/* } */
#define	TC_SEMICOL	(1 << 14)
#define	TC_NEWLINE	(1 << 15)
#define	TC_STATX	(1 << 16)		/* ctl statement (for, next...) */
#define	TC_WHILE	(1 << 17)
#define	TC_ELSE		(1 << 18)
#define	TC_BUILTIN	(1 << 19)
#define	TC_GETLINE	(1 << 20)
#define	TC_FUNCDECL	(1 << 21)		/* `function' `func' */
#define	TC_BEGIN	(1 << 22)
#define	TC_END		(1 << 23)
#define	TC_EOF		(1 << 24)
#define	TC_VARIABLE	(1 << 25)
#define	TC_ARRAY	(1 << 26)
#define	TC_FUNCTION	(1 << 27)
#define	TC_STRING	(1 << 28)
#define	TC_NUMBER	(1 << 29)

#define	TC_UOPPRE	(TC_UOPPRE1 | TC_UOPPRE2)

/* combined token classes */
#define	TC_BINOP	(TC_BINOPX | TC_COMMA | TC_PIPE | TC_IN)
#define	TC_UNARYOP	(TC_UOPPRE | TC_UOPPOST)
#define	TC_OPERAND	(TC_VARIABLE | TC_ARRAY | TC_FUNCTION | \
	TC_BUILTIN | TC_GETLINE | TC_SEQSTART | TC_STRING | TC_NUMBER)

#define	TC_STATEMNT	(TC_STATX | TC_WHILE)
#define	TC_OPTERM	(TC_SEMICOL | TC_NEWLINE)

/* word tokens, cannot mean something else if not expected */
#define	TC_WORD		(TC_IN | TC_STATEMNT | TC_ELSE | TC_BUILTIN | \
	TC_GETLINE | TC_FUNCDECL | TC_BEGIN | TC_END)

/* discard newlines after these */
#define	TC_NOTERM	(TC_COMMA | TC_GRPSTART | TC_GRPTERM | \
	TC_BINOP | TC_OPTERM)

/* what can expression begin with */
#define	TC_OPSEQ	(TC_OPERAND | TC_UOPPRE | TC_REGEXP)
/* what can group begin with */
#define	TC_GRPSEQ	(TC_OPSEQ | TC_OPTERM | TC_STATEMNT | TC_GRPSTART)

/* if previous token class is CONCAT1 and next is CONCAT2, concatenation */
/* operator is inserted between them */
#define	TC_CONCAT1	(TC_VARIABLE | TC_ARRTERM | TC_SEQTERM | \
	TC_STRING | TC_NUMBER | TC_UOPPOST)
#define	TC_CONCAT2	(TC_OPERAND | TC_UOPPRE)

#define	OF_RES1		0x010000
#define	OF_RES2		0x020000
#define	OF_STR1		0x040000
#define	OF_STR2		0x080000
#define	OF_NUM1		0x100000
#define	OF_CHECKED	0x200000

/* combined operator flags */
#define	xx	0
#define	xV	OF_RES2
#define	xS	(OF_RES2 | OF_STR2)
#define	Vx	OF_RES1
#define	VV	(OF_RES1 | OF_RES2)
#define	Nx	(OF_RES1 | OF_NUM1)
#define	NV	(OF_RES1 | OF_NUM1 | OF_RES2)
#define	Sx	(OF_RES1 | OF_STR1)
#define	SV	(OF_RES1 | OF_STR1 | OF_RES2)
#define	SS	(OF_RES1 | OF_STR1 | OF_RES2 | OF_STR2)

#define	OPCLSMASK	0xFF00
#define	OPNMASK		0x007F

/* operator priority is a highest byte (even: r->l, odd: l->r grouping)
 * For builtins it has different meaning: n n s3 s2 s1 v3 v2 v1,
 * n - min. number of args, vN - resolve Nth arg to var, sN - resolve to string
 */
#define	P(x)	(x << 24)
#define	PRIMASK		0x7F000000
#define	PRIMASK2	0x7E000000

/* Operation classes */

#define	SHIFT_TIL_THIS	0x0600
#define	RECUR_FROM_THIS	0x1000

enum {
	OC_DELETE=0x0100,	OC_EXEC=0x0200,		OC_NEWSOURCE=0x0300,
	OC_PRINT=0x0400,	OC_PRINTF=0x0500,	OC_WALKINIT=0x0600,

	OC_BR=0x0700,		OC_BREAK=0x0800,	OC_CONTINUE=0x0900,
	OC_EXIT=0x0a00,		OC_NEXT=0x0b00,		OC_NEXTFILE=0x0c00,
	OC_TEST=0x0d00,		OC_WALKNEXT=0x0e00,

	OC_BINARY=0x1000,	OC_BUILTIN=0x1100,	OC_COLON=0x1200,
	OC_COMMA=0x1300,	OC_COMPARE=0x1400,	OC_CONCAT=0x1500,
	OC_FBLTIN=0x1600,	OC_FIELD=0x1700,	OC_FNARG=0x1800,
	OC_FUNC=0x1900,		OC_GETLINE=0x1a00,	OC_IN=0x1b00,
	OC_LAND=0x1c00,		OC_LOR=0x1d00,		OC_MATCH=0x1e00,
	OC_MOVE=0x1f00,		OC_PGETLINE=0x2000,	OC_REGEXP=0x2100,
	OC_REPLACE=0x2200,	OC_RETURN=0x2300,	OC_SPRINTF=0x2400,
	OC_TERNARY=0x2500,	OC_UNARY=0x2600,	OC_VAR=0x2700,
	OC_DONE=0x2800,

	ST_IF=0x3000,		ST_DO=0x3100,		ST_FOR=0x3200,
	ST_WHILE=0x3300
};

/* simple builtins */
enum {
	F_in=0,	F_rn,	F_co,	F_ex,	F_lg,	F_si,	F_sq,	F_sr,
	F_ti,	F_le,	F_sy,	F_ff,	F_cl
};

/* builtins */
enum {
	B_a2=0,	B_ix,	B_ma,	B_sp,	B_ss,	B_ti,	B_lo,	B_up,
	B_ge,	B_gs,	B_su
};

/* tokens and their corresponding info values */

#define	NTC		"\377"		/* switch to next token class (tc<<1) */
#define	NTCC	'\377'

#define	OC_B	OC_BUILTIN

static char * const tokenlist =
	"\1("		NTC
	"\1)"		NTC
	"\1/"		NTC									/* REGEXP */
	"\2>>"		"\1>"		"\1|"		NTC			/* OUTRDR */
	"\2++"		"\2--"		NTC						/* UOPPOST */
	"\2++"		"\2--"		"\1$"		NTC			/* UOPPRE1 */
	"\2=="		"\1="		"\2+="		"\2-="		/* BINOPX */
	"\2*="		"\2/="		"\2%="		"\2^="
	"\1+"		"\1-"		"\3**="		"\2**"
	"\1/"		"\1%"		"\1^"		"\1*"
	"\2!="		"\2>="		"\2<="		"\1>"
	"\1<"		"\2!~"		"\1~"		"\2&&"
	"\2||"		"\1?"		"\1:"		NTC
	"\2in"		NTC
	"\1,"		NTC
	"\1|"		NTC
	"\1+"		"\1-"		"\1!"		NTC			/* UOPPRE2 */
	"\1]"		NTC
	"\1{"		NTC
	"\1}"		NTC
	"\1;"		NTC
	"\1\n"		NTC
	"\2if"		"\2do"		"\3for"		"\5break"	/* STATX */
	"\10continue"			"\6delete"	"\5print"
	"\6printf"	"\4next"	"\10nextfile"
	"\6return"	"\4exit"	NTC
	"\5while"	NTC
	"\4else"	NTC

	"\5close"	"\6system"	"\6fflush"	"\5atan2"	/* BUILTIN */
	"\3cos"		"\3exp"		"\3int"		"\3log"
	"\4rand"	"\3sin"		"\4sqrt"	"\5srand"
	"\6gensub"	"\4gsub"	"\5index"	"\6length"
	"\5match"	"\5split"	"\7sprintf"	"\3sub"
	"\6substr"	"\7systime"	"\10strftime"
	"\7tolower"	"\7toupper"	NTC
	"\7getline"	NTC
	"\4func"	"\10function"	NTC
	"\5BEGIN"	NTC
	"\3END"		"\0"
	;

static unsigned long tokeninfo[] = {

	0,
	0,
	OC_REGEXP,
	xS|'a',		xS|'w',		xS|'|',
	OC_UNARY|xV|P(9)|'p',		OC_UNARY|xV|P(9)|'m',
	OC_UNARY|xV|P(9)|'P',		OC_UNARY|xV|P(9)|'M',
		OC_FIELD|xV|P(5),
	OC_COMPARE|VV|P(39)|5,		OC_MOVE|VV|P(74),
		OC_REPLACE|NV|P(74)|'+',	OC_REPLACE|NV|P(74)|'-',
	OC_REPLACE|NV|P(74)|'*',	OC_REPLACE|NV|P(74)|'/',
		OC_REPLACE|NV|P(74)|'%',	OC_REPLACE|NV|P(74)|'&',
	OC_BINARY|NV|P(29)|'+',		OC_BINARY|NV|P(29)|'-',
		OC_REPLACE|NV|P(74)|'&',	OC_BINARY|NV|P(15)|'&',
	OC_BINARY|NV|P(25)|'/',		OC_BINARY|NV|P(25)|'%',
		OC_BINARY|NV|P(15)|'&',		OC_BINARY|NV|P(25)|'*',
	OC_COMPARE|VV|P(39)|4,		OC_COMPARE|VV|P(39)|3,
		OC_COMPARE|VV|P(39)|0,		OC_COMPARE|VV|P(39)|1,
	OC_COMPARE|VV|P(39)|2,		OC_MATCH|Sx|P(45)|'!',
		OC_MATCH|Sx|P(45)|'~',		OC_LAND|Vx|P(55),
	OC_LOR|Vx|P(59),			OC_TERNARY|Vx|P(64)|'?',
		OC_COLON|xx|P(67)|':',
	OC_IN|SV|P(49),
	OC_COMMA|SS|P(80),
	OC_PGETLINE|SV|P(37),
	OC_UNARY|xV|P(19)|'+',		OC_UNARY|xV|P(19)|'-',
		OC_UNARY|xV|P(19)|'!',
	0,
	0,
	0,
	0,
	0,
	ST_IF,			ST_DO,			ST_FOR,			OC_BREAK,
	OC_CONTINUE,					OC_DELETE|Vx,	OC_PRINT,
	OC_PRINTF,		OC_NEXT,		OC_NEXTFILE,
	OC_RETURN|Vx,	OC_EXIT|Nx,
	ST_WHILE,
	0,

	OC_FBLTIN|Sx|F_cl, OC_FBLTIN|Sx|F_sy, OC_FBLTIN|Sx|F_ff, OC_B|B_a2|P(0x83),
	OC_FBLTIN|Nx|F_co, OC_FBLTIN|Nx|F_ex, OC_FBLTIN|Nx|F_in, OC_FBLTIN|Nx|F_lg,
	OC_FBLTIN|F_rn,    OC_FBLTIN|Nx|F_si, OC_FBLTIN|Nx|F_sq, OC_FBLTIN|Nx|F_sr,
	OC_B|B_ge|P(0xd6), OC_B|B_gs|P(0xb6), OC_B|B_ix|P(0x9b), OC_FBLTIN|Sx|F_le,
	OC_B|B_ma|P(0x89), OC_B|B_sp|P(0x8b), OC_SPRINTF,        OC_B|B_su|P(0xb6),
	OC_B|B_ss|P(0x8f), OC_FBLTIN|F_ti,    OC_B|B_ti|P(0x0b),
	OC_B|B_lo|P(0x49), OC_B|B_up|P(0x49),
	OC_GETLINE|SV|P(0),
	0,	0,
	0,
	0
};

/* internal variable names and their initial values       */
/* asterisk marks SPECIAL vars; $ is just no-named Field0 */ 
enum {
	CONVFMT=0,	OFMT,		FS,			OFS,
	ORS,		RS,			RT,			FILENAME,
	SUBSEP,		ARGIND,		ARGC,		ARGV,
	ERRNO,		FNR,
	NR,			NF,			IGNORECASE,
	ENVIRON,	F0,			_intvarcount_
};

static char * vNames =
	"CONVFMT\0"	"OFMT\0"	"FS\0*"		"OFS\0"
	"ORS\0"		"RS\0*"		"RT\0"		"FILENAME\0"	
	"SUBSEP\0"	"ARGIND\0"	"ARGC\0"	"ARGV\0"
	"ERRNO\0"	"FNR\0"
	"NR\0"		"NF\0*"		"IGNORECASE\0*"
	"ENVIRON\0"	"$\0*"		"\0";

static char * vValues =
	"%.6g\0"	"%.6g\0"	" \0"		" \0"
	"\n\0"		"\n\0"		"\0"		"\0"
	"\034\0"
	"\377";

/* hash size may grow to these values */
#define FIRST_PRIME 61;
static const unsigned int PRIMES[] = { 251, 1021, 4093, 16381, 65521 };
static const unsigned int NPRIMES = sizeof(PRIMES) / sizeof(unsigned int);

/* globals */

extern char **environ;

static var * V[_intvarcount_];
static chain beginseq, mainseq, endseq, *seq;
static int nextrec, nextfile;
static node *break_ptr, *continue_ptr;
static rstream *iF;
static xhash *vhash, *ahash, *fdhash, *fnhash;
static char *programname;
static short lineno;
static int is_f0_split;
static int nfields = 0;
static var *Fields = NULL;
static tsplitter fsplitter, rsplitter;
static nvblock *cb = NULL;
static char *pos;
static char *buf;
static int icase = FALSE;
static int exiting = FALSE;

static struct {
	unsigned long tclass;
	unsigned long info;
	char *string;
	double number;
	short lineno;
	int rollback;
} t;

/* function prototypes */
extern void xregcomp(regex_t *preg, const char *regex, int cflags);
static void handle_special(var *);
static node *parse_expr(unsigned long);
static void chain_group(void);
static var *evaluate(node *, var *);
static rstream *next_input_file(void);
static int fmt_num(char *, int, char *, double, int);
static int awk_exit(int);

/* ---- error handling ---- */

static const char EMSG_INTERNAL_ERROR[] = "Internal error";
static const char EMSG_UNEXP_EOS[] = "Unexpected end of string";
static const char EMSG_UNEXP_TOKEN[] = "Unexpected token";
static const char EMSG_DIV_BY_ZERO[] = "Division by zero";
static const char EMSG_INV_FMT[] = "Invalid format specifier";
static const char EMSG_TOO_FEW_ARGS[] = "Too few arguments for builtin";
static const char EMSG_NOT_ARRAY[] = "Not an array";
static const char EMSG_POSSIBLE_ERROR[] = "Possible syntax error";
static const char EMSG_UNDEF_FUNC[] = "Call to undefined function";
#ifndef CONFIG_FEATURE_AWK_MATH
static const char EMSG_NO_MATH[] = "Math support is not compiled in";
#endif

static void syntax_error(const char * const message)
{
	bb_error_msg("%s:%i: %s", programname, lineno, message);
	exit(1);
}

#define runtime_error(x) syntax_error(x)


/* ---- hash stuff ---- */

static unsigned int hashidx(char *name) {

	register unsigned int idx=0;

	while (*name)  idx = *name++ + (idx << 6) - idx;
	return idx;
}

/* create new hash */
static xhash *hash_init(void) {

	xhash *newhash;
	
	newhash = (xhash *)xcalloc(1, sizeof(xhash));
	newhash->csize = FIRST_PRIME;
	newhash->items = (hash_item **)xcalloc(newhash->csize, sizeof(hash_item *));

	return newhash;
}

/* find item in hash, return ptr to data, NULL if not found */
static void *hash_search(xhash *hash, char *name) {

	hash_item *hi;

	hi = hash->items [ hashidx(name) % hash->csize ];
	while (hi) {
		if (strcmp(hi->name, name) == 0)
			return &(hi->data);
		hi = hi->next;
	}
	return NULL;
}

/* grow hash if it becomes too big */
static void hash_rebuild(xhash *hash) {

	unsigned int newsize, i, idx;
	hash_item **newitems, *hi, *thi;

	if (hash->nprime == NPRIMES)
		return;

	newsize = PRIMES[hash->nprime++];
	newitems = (hash_item **)xcalloc(newsize, sizeof(hash_item *));

	for (i=0; i<hash->csize; i++) {
		hi = hash->items[i];
		while (hi) {
			thi = hi;
			hi = thi->next;
			idx = hashidx(thi->name) % newsize;
			thi->next = newitems[idx];
			newitems[idx] = thi;
		}
	}

	free(hash->items);
	hash->csize = newsize;
	hash->items = newitems;
}

/* find item in hash, add it if necessary. Return ptr to data */
static void *hash_find(xhash *hash, char *name) {

	hash_item *hi;
	unsigned int idx;
	int l;

	hi = hash_search(hash, name);
	if (! hi) {
		if (++hash->nel / hash->csize > 10)
			hash_rebuild(hash);

		l = bb_strlen(name) + 1;
		hi = xcalloc(sizeof(hash_item) + l, 1);
		memcpy(hi->name, name, l);

		idx = hashidx(name) % hash->csize;
		hi->next = hash->items[idx];
		hash->items[idx] = hi;
		hash->glen += l;
	}
	return &(hi->data);
}

#define findvar(hash, name) (var *) hash_find ( (hash) , (name) )
#define newvar(name) (var *) hash_find ( vhash , (name) )
#define newfile(name) (rstream *) hash_find ( fdhash , (name) )
#define newfunc(name) (func *) hash_find ( fnhash , (name) )

static void hash_remove(xhash *hash, char *name) {

	hash_item *hi, **phi;

	phi = &(hash->items[ hashidx(name) % hash->csize ]);
	while (*phi) {
		hi = *phi;
		if (strcmp(hi->name, name) == 0) {
			hash->glen -= (bb_strlen(name) + 1);
			hash->nel--;
			*phi = hi->next;
			free(hi);
			break;
		}
		phi = &(hi->next);
	}
}

/* ------ some useful functions ------ */

static void skip_spaces(char **s) {

	register char *p = *s;

	while(*p == ' ' || *p == '\t' ||
					(*p == '\\' && *(p+1) == '\n' && (++p, ++t.lineno))) {
	 	p++;
	}
	*s = p;
}

static char *nextword(char **s) {

	register char *p = *s;

	while (*(*s)++) ;

	return p;
}

static char nextchar(char **s) {

	register char c, *pps;

	c = *((*s)++);
	pps = *s;
	if (c == '\\') c = bb_process_escape_sequence((const char**)s);
	if (c == '\\' && *s == pps) c = *((*s)++);
	return c;
}

static inline int isalnum_(int c) {

	return (isalnum(c) || c == '_');
}

static FILE *afopen(const char *path, const char *mode) {

	return (*path == '-' && *(path+1) == '\0') ? stdin : bb_xfopen(path, mode);
}

/* -------- working with variables (set/get/copy/etc) -------- */

static xhash *iamarray(var *v) {

	var *a = v;

	while (a->type & VF_CHILD)
		a = a->x.parent;

	if (! (a->type & VF_ARRAY)) {
		a->type |= VF_ARRAY;
		a->x.array = hash_init();
	}
	return a->x.array;
}

static void clear_array(xhash *array) {

	unsigned int i;
	hash_item *hi, *thi;

	for (i=0; i<array->csize; i++) {
		hi = array->items[i];
		while (hi) {
			thi = hi;
			hi = hi->next;
			free(thi->data.v.string);
			free(thi);
		}
		array->items[i] = NULL;
	}
	array->glen = array->nel = 0;
}

/* clear a variable */
static var *clrvar(var *v) {

	if (!(v->type & VF_FSTR))
		free(v->string);

	v->type &= VF_DONTTOUCH;
	v->type |= VF_DIRTY;
	v->string = NULL;
	return v;
}

/* assign string value to variable */
static var *setvar_p(var *v, char *value) {

	clrvar(v);
	v->string = value;
	handle_special(v);

	return v;
}

/* same as setvar_p but make a copy of string */
static var *setvar_s(var *v, char *value) {

	return setvar_p(v, (value && *value) ? bb_xstrdup(value) : NULL);
}

/* same as setvar_s but set USER flag */
static var *setvar_u(var *v, char *value) {

	setvar_s(v, value);
	v->type |= VF_USER;
	return v;
}

/* set array element to user string */
static void setari_u(var *a, int idx, char *s) {

	register var *v;
	static char sidx[12];

	sprintf(sidx, "%d", idx);
	v = findvar(iamarray(a), sidx);
	setvar_u(v, s);
}

/* assign numeric value to variable */
static var *setvar_i(var *v, double value) {

	clrvar(v);
	v->type |= VF_NUMBER;
	v->number = value;
	handle_special(v);
	return v;
}

static char *getvar_s(var *v) {

	/* if v is numeric and has no cached string, convert it to string */
	if ((v->type & (VF_NUMBER | VF_CACHED)) == VF_NUMBER) {
		fmt_num(buf, MAXVARFMT, getvar_s(V[CONVFMT]), v->number, TRUE);
		v->string = bb_xstrdup(buf);
		v->type |= VF_CACHED;
	}
	return (v->string == NULL) ? "" : v->string;
}

static double getvar_i(var *v) {

	char *s;

	if ((v->type & (VF_NUMBER | VF_CACHED)) == 0) {
		v->number = 0;
		s = v->string;
		if (s && *s) {
			v->number = strtod(s, &s);
			if (v->type & VF_USER) {
				skip_spaces(&s);
				if (*s != '\0')
					v->type &= ~VF_USER;
			}
		} else {
			v->type &= ~VF_USER;
		}
		v->type |= VF_CACHED;
	}
	return v->number;
}

static var *copyvar(var *dest, var *src) {

	if (dest != src) {
		clrvar(dest);
		dest->type |= (src->type & ~VF_DONTTOUCH);
		dest->number = src->number;
		if (src->string)
			dest->string = bb_xstrdup(src->string);
	}
	handle_special(dest);
	return dest;
}

static var *incvar(var *v) {

	return setvar_i(v, getvar_i(v)+1.);
}

/* return true if v is number or numeric string */
static int is_numeric(var *v) {

	getvar_i(v);
	return ((v->type ^ VF_DIRTY) & (VF_NUMBER | VF_USER | VF_DIRTY));
}

/* return 1 when value of v corresponds to true, 0 otherwise */
static int istrue(var *v) {

	if (is_numeric(v))
		return (v->number == 0) ? 0 : 1;
	else
		return (v->string && *(v->string)) ? 1 : 0;
}

/* temporary varables allocator. Last allocated should be first freed */
static var *nvalloc(int n) {

	nvblock *pb = NULL;
	var *v, *r;
	int size;

	while (cb) {
		pb = cb;
		if ((cb->pos - cb->nv) + n <= cb->size) break;
		cb = cb->next;
	}

	if (! cb) {
		size = (n <= MINNVBLOCK) ? MINNVBLOCK : n;
		cb = (nvblock *)xmalloc(sizeof(nvblock) + size * sizeof(var));
		cb->size = size;
		cb->pos = cb->nv;
		cb->prev = pb;
		cb->next = NULL;
		if (pb) pb->next = cb;
	}

	v = r = cb->pos;
	cb->pos += n;

	while (v < cb->pos) {
		v->type = 0;
		v->string = NULL;
		v++;
	}

	return r;
}

static void nvfree(var *v) {

	var *p;

	if (v < cb->nv || v >= cb->pos)
		runtime_error(EMSG_INTERNAL_ERROR);

	for (p=v; p<cb->pos; p++) {
		if ((p->type & (VF_ARRAY|VF_CHILD)) == VF_ARRAY) {
			clear_array(iamarray(p));
			free(p->x.array->items);
			free(p->x.array);
		}
		if (p->type & VF_WALK)
			free(p->x.walker);

		clrvar(p);
	}

	cb->pos = v;
	while (cb->prev && cb->pos == cb->nv) {
		cb = cb->prev;
	}
}

/* ------- awk program text parsing ------- */

/* Parse next token pointed by global pos, place results into global t.
 * If token isn't expected, give away. Return token class
 */
static unsigned long next_token(unsigned long expected) {

	char *p, *pp, *s;
	char *tl;
	unsigned long tc, *ti;
	int l;
	static int concat_inserted = FALSE;
	static unsigned long save_tclass, save_info;
	static unsigned long ltclass = TC_OPTERM;

	if (t.rollback) {

		t.rollback = FALSE;

	} else if (concat_inserted) {

		concat_inserted = FALSE;
		t.tclass = save_tclass;
		t.info = save_info;

	} else {

		p = pos;

	readnext:
		skip_spaces(&p);
		lineno = t.lineno;
		if (*p == '#')
			while (*p != '\n' && *p != '\0') p++;

		if (*p == '\n')
			t.lineno++;

		if (*p == '\0') {
			tc = TC_EOF;

		} else if (*p == '\"') {
			/* it's a string */
			t.string = s = ++p;
			while (*p != '\"') {
				if (*p == '\0' || *p == '\n')
					syntax_error(EMSG_UNEXP_EOS);
				*(s++) = nextchar(&p);
			}
			p++;
			*s = '\0';
			tc = TC_STRING;

		} else if ((expected & TC_REGEXP) && *p == '/') {
			/* it's regexp */
			t.string = s = ++p;
			while (*p != '/') {
				if (*p == '\0' || *p == '\n')
					syntax_error(EMSG_UNEXP_EOS);
				if ((*s++ = *p++) == '\\') {
					pp = p;
					*(s-1) = bb_process_escape_sequence((const char **)&p);
					if (*pp == '\\') *s++ = '\\';
					if (p == pp) *s++ = *p++;
				}
			}
			p++;
			*s = '\0';
			tc = TC_REGEXP;

		} else if (*p == '.' || isdigit(*p)) {
			/* it's a number */
			t.number = strtod(p, &p);
			if (*p == '.')
				syntax_error(EMSG_UNEXP_TOKEN);
			tc = TC_NUMBER;

		} else {
			/* search for something known */
			tl = tokenlist;
			tc = 0x00000001;
			ti = tokeninfo;
			while (*tl) {
				l = *(tl++);
				if (l == NTCC) {
					tc <<= 1;
					continue;
				}
				/* if token class is expected, token
				 * matches and it's not a longer word,
				 * then this is what we are looking for
				 */
				if ((tc & (expected | TC_WORD | TC_NEWLINE)) &&
				*tl == *p && strncmp(p, tl, l) == 0 &&
				!((tc & TC_WORD) && isalnum_(*(p + l)))) {
					t.info = *ti;
					p += l;
					break;
				}
				ti++;
				tl += l;
			}

			if (! *tl) {
				/* it's a name (var/array/function),
				 * otherwise it's something wrong
				 */
				if (! isalnum_(*p))
					syntax_error(EMSG_UNEXP_TOKEN);

				t.string = --p;
				while(isalnum_(*(++p))) {
					*(p-1) = *p;
				}
				*(p-1) = '\0';
				tc = TC_VARIABLE;
				if (*p == '(') {
					tc = TC_FUNCTION;
				} else {
					skip_spaces(&p);
					if (*p == '[') {
						p++;
						tc = TC_ARRAY;
					}
				}
			}
		}
		pos = p;

		/* skipping newlines in some cases */
		if ((ltclass & TC_NOTERM) && (tc & TC_NEWLINE))
			goto readnext;

		/* insert concatenation operator when needed */
		if ((ltclass&TC_CONCAT1) && (tc&TC_CONCAT2) && (expected&TC_BINOP)) {
			concat_inserted = TRUE;
			save_tclass = tc;
			save_info = t.info;
			tc = TC_BINOP;
			t.info = OC_CONCAT | SS | P(35);
		}

		t.tclass = tc;
	}
	ltclass = t.tclass;

	/* Are we ready for this? */
	if (! (ltclass & expected))
		syntax_error((ltclass & (TC_NEWLINE | TC_EOF)) ?
								EMSG_UNEXP_EOS : EMSG_UNEXP_TOKEN);

	return ltclass;
}

static void rollback_token(void) { t.rollback = TRUE; }

static node *new_node(unsigned long info) {

	register node *n;

	n = (node *)xcalloc(sizeof(node), 1);
	n->info = info;
	n->lineno = lineno;
	return n;
}

static node *mk_re_node(char *s, node *n, regex_t *re) {

	n->info = OC_REGEXP;
	n->l.re = re;
	n->r.ire = re + 1;
	xregcomp(re, s, REG_EXTENDED);
	xregcomp(re+1, s, REG_EXTENDED | REG_ICASE);

	return n;
}

static node *condition(void) {

	next_token(TC_SEQSTART);
	return parse_expr(TC_SEQTERM);
}

/* parse expression terminated by given argument, return ptr
 * to built subtree. Terminator is eaten by parse_expr */
static node *parse_expr(unsigned long iexp) {

	node sn;
	node *cn = &sn;
	node *vn, *glptr;
	unsigned long tc, xtc;
	var *v;

	sn.info = PRIMASK;
	sn.r.n = glptr = NULL;
	xtc = TC_OPERAND | TC_UOPPRE | TC_REGEXP | iexp;

	while (! ((tc = next_token(xtc)) & iexp)) {
		if (glptr && (t.info == (OC_COMPARE|VV|P(39)|2))) {
			/* input redirection (<) attached to glptr node */
			cn = glptr->l.n = new_node(OC_CONCAT|SS|P(37));
			xtc = TC_OPERAND | TC_UOPPRE;
			glptr = NULL;

		} else if (tc & (TC_BINOP | TC_UOPPOST)) {
			/* for binary and postfix-unary operators, jump back over
			 * previous operators with higher priority */
			vn = cn;
			while ( ((t.info & PRIMASK) > (vn->a.n->info & PRIMASK2)) || 
			  ((t.info == vn->info) && ((t.info & OPCLSMASK) == OC_COLON)) )
				vn = vn->a.n;
			if ((t.info & OPCLSMASK) == OC_TERNARY)
				t.info += P(6);
			cn = vn->a.n->r.n = new_node(t.info);
			cn->a.n = vn->a.n;
			if (tc & TC_BINOP) {
				cn->l.n = vn;
				xtc = TC_OPERAND | TC_UOPPRE | TC_REGEXP;
				if ((t.info & OPCLSMASK) == OC_PGETLINE) {
					/* it's a pipe */
					next_token(TC_GETLINE);
					/* give maximum priority to this pipe */
					cn->info &= ~PRIMASK;
					xtc = TC_OPERAND | TC_UOPPRE | TC_BINOP | iexp;
				}
			} else {
				cn->r.n = vn;
				xtc = TC_OPERAND | TC_UOPPRE | TC_BINOP | iexp;
			}
			vn->a.n = cn;

		} else {
			/* for operands and prefix-unary operators, attach them
			 * to last node */
			vn = cn;
		  	cn = vn->r.n = new_node(t.info);
			cn->a.n = vn;
			xtc = TC_OPERAND | TC_UOPPRE | TC_REGEXP;
			if (tc & (TC_OPERAND | TC_REGEXP)) {
				xtc = TC_UOPPRE | TC_BINOP | TC_OPERAND | iexp;
				/* one should be very careful with switch on tclass - 
				 * only simple tclasses should be used! */
				switch (tc) {
				  case TC_VARIABLE:
				  case TC_ARRAY:
					cn->info = OC_VAR;
				  	if ((v = hash_search(ahash, t.string)) != NULL) {
						cn->info = OC_FNARG;
						cn->l.i = v->x.aidx;
					} else {
				  		cn->l.v = newvar(t.string);
					}
					if (tc & TC_ARRAY) {
						cn->info |= xS;
						cn->r.n = parse_expr(TC_ARRTERM);
					}
					xtc = TC_UOPPOST | TC_UOPPRE | TC_BINOP | TC_OPERAND | iexp;
					break;
				  	
				  case TC_NUMBER:
				  case TC_STRING:
					cn->info = OC_VAR;
					v = cn->l.v = xcalloc(sizeof(var), 1);
					if (tc & TC_NUMBER)
						setvar_i(v, t.number);
					else
						setvar_s(v, t.string);
					break;

				  case TC_REGEXP:
					mk_re_node(t.string, cn,
									(regex_t *)xcalloc(sizeof(regex_t),2));
					break;

				  case TC_FUNCTION:
				  	cn->info = OC_FUNC;
					cn->r.f = newfunc(t.string);
					cn->l.n = condition();
					break;

				  case TC_SEQSTART:
					cn = vn->r.n = parse_expr(TC_SEQTERM);
					cn->a.n = vn;
					break;

				  case TC_GETLINE:
					glptr = cn;
					xtc = TC_OPERAND | TC_UOPPRE | TC_BINOP | iexp;
					break;

				  case TC_BUILTIN:
					cn->l.n = condition();
					break;
				}
			}
		}
	}
	return sn.r.n;
}

/* add node to chain. Return ptr to alloc'd node */
static node *chain_node(unsigned long info) {

	register node *n;

	if (! seq->first)
		seq->first = seq->last = new_node(0);

	if (seq->programname != programname) {
		seq->programname = programname;
		n = chain_node(OC_NEWSOURCE);
		n->l.s = bb_xstrdup(programname);
	}

	n = seq->last;
	n->info = info;
	seq->last = n->a.n = new_node(OC_DONE);

	return n;
}

static void chain_expr(unsigned long info) {

	node *n;

	n = chain_node(info);
	n->l.n = parse_expr(TC_OPTERM | TC_GRPTERM);
	if (t.tclass & TC_GRPTERM)
		rollback_token();
}

static node *chain_loop(node *nn) {

	node *n, *n2, *save_brk, *save_cont;

	save_brk = break_ptr;
	save_cont = continue_ptr;

	n = chain_node(OC_BR | Vx);
	continue_ptr = new_node(OC_EXEC);
	break_ptr = new_node(OC_EXEC);
	chain_group();
	n2 = chain_node(OC_EXEC | Vx);
	n2->l.n = nn;
	n2->a.n = n;
	continue_ptr->a.n = n2;
	break_ptr->a.n = n->r.n = seq->last;

	continue_ptr = save_cont;
	break_ptr = save_brk;

	return n;
}

/* parse group and attach it to chain */
static void chain_group(void) {

	unsigned long c;
	node *n, *n2, *n3;

	do {
		c = next_token(TC_GRPSEQ);
	} while (c & TC_NEWLINE);

	if (c & TC_GRPSTART) {
		while(next_token(TC_GRPSEQ | TC_GRPTERM) != TC_GRPTERM) {
			rollback_token();
			chain_group();
		}
	} else if (c & (TC_OPSEQ | TC_OPTERM)) {
		rollback_token();
		chain_expr(OC_EXEC | Vx);
	} else {						/* TC_STATEMNT */
		switch (t.info & OPCLSMASK) {
			case ST_IF:
				n = chain_node(OC_BR | Vx);
				n->l.n = condition();
				chain_group();
				n2 = chain_node(OC_EXEC);
				n->r.n = seq->last;
				if (next_token(TC_GRPSEQ | TC_GRPTERM | TC_ELSE)==TC_ELSE) {
					chain_group();
					n2->a.n = seq->last;
				} else {
					rollback_token();
				}
				break;

			case ST_WHILE:
				n2 = condition();
				n = chain_loop(NULL);
				n->l.n = n2;
				break;

			case ST_DO:
				n2 = chain_node(OC_EXEC);
				n = chain_loop(NULL);
				n2->a.n = n->a.n;
				next_token(TC_WHILE);
				n->l.n = condition();
				break;

			case ST_FOR:
				next_token(TC_SEQSTART);
				n2 = parse_expr(TC_SEMICOL | TC_SEQTERM);
				if (t.tclass & TC_SEQTERM) {				/* for-in */
					if ((n2->info & OPCLSMASK) != OC_IN)
						syntax_error(EMSG_UNEXP_TOKEN);
					n = chain_node(OC_WALKINIT | VV);
					n->l.n = n2->l.n;
					n->r.n = n2->r.n;
					n = chain_loop(NULL);
					n->info = OC_WALKNEXT | Vx;
					n->l.n = n2->l.n;
				} else {									/* for(;;) */
					n = chain_node(OC_EXEC | Vx);
					n->l.n = n2;
					n2 = parse_expr(TC_SEMICOL);
					n3 = parse_expr(TC_SEQTERM);
					n = chain_loop(n3);
					n->l.n = n2;
					if (! n2)
						n->info = OC_EXEC;
				}
				break;

			case OC_PRINT:
			case OC_PRINTF:
				n = chain_node(t.info);
				n->l.n = parse_expr(TC_OPTERM | TC_OUTRDR | TC_GRPTERM);
				if (t.tclass & TC_OUTRDR) {
					n->info |= t.info;
					n->r.n = parse_expr(TC_OPTERM | TC_GRPTERM);
				}
				if (t.tclass & TC_GRPTERM)
					rollback_token();
				break;

			case OC_BREAK:
				n = chain_node(OC_EXEC);
				n->a.n = break_ptr;
				break;

			case OC_CONTINUE:
				n = chain_node(OC_EXEC);
				n->a.n = continue_ptr;
				break;

			/* delete, next, nextfile, return, exit */
			default:
				chain_expr(t.info);

		}
	}
}

static void parse_program(char *p) {

	unsigned long tclass;
	node *cn;
	func *f;
	var *v;

	pos = p;
	t.lineno = 1;
	while((tclass = next_token(TC_EOF | TC_OPSEQ | TC_GRPSTART |
				TC_OPTERM | TC_BEGIN | TC_END | TC_FUNCDECL)) != TC_EOF) {

		if (tclass & TC_OPTERM)
			continue;

		seq = &mainseq;
		if (tclass & TC_BEGIN) {
			seq = &beginseq;
			chain_group();

		} else if (tclass & TC_END) {
			seq = &endseq;
			chain_group();

		} else if (tclass & TC_FUNCDECL) {
			next_token(TC_FUNCTION);
			pos++;
			f = newfunc(t.string);
			f->body.first = NULL;
			f->nargs = 0;
			while(next_token(TC_VARIABLE | TC_SEQTERM) & TC_VARIABLE) {
				v = findvar(ahash, t.string);
				v->x.aidx = (f->nargs)++;

				if (next_token(TC_COMMA | TC_SEQTERM) & TC_SEQTERM)
					break;
			}
			seq = &(f->body);
			chain_group();
			clear_array(ahash);

		} else if (tclass & TC_OPSEQ) {
			rollback_token();
			cn = chain_node(OC_TEST);
			cn->l.n = parse_expr(TC_OPTERM | TC_EOF | TC_GRPSTART);
			if (t.tclass & TC_GRPSTART) {
				rollback_token();
				chain_group();
			} else {
				chain_node(OC_PRINT);
			}
			cn->r.n = mainseq.last;

		} else /* if (tclass & TC_GRPSTART) */ {
			rollback_token();
			chain_group();
		}
	}
}


/* -------- program execution part -------- */

static node *mk_splitter(char *s, tsplitter *spl) {

	register regex_t *re, *ire;
	node *n;

	re = &spl->re[0];
	ire = &spl->re[1];
	n = &spl->n;
	if ((n->info && OPCLSMASK) == OC_REGEXP) {
		regfree(re);
		regfree(ire);
	}
	if (bb_strlen(s) > 1) {
		mk_re_node(s, n, re);
	} else {
		n->info = (unsigned long) *s;
	}

	return n;
}

/* use node as a regular expression. Supplied with node ptr and regex_t
 * storage space. Return ptr to regex (if result points to preg, it shuold
 * be later regfree'd manually
 */
static regex_t *as_regex(node *op, regex_t *preg) {

	var *v;
	char *s;

	if ((op->info & OPCLSMASK) == OC_REGEXP) {
		return icase ? op->r.ire : op->l.re;
	} else {
		v = nvalloc(1);
		s = getvar_s(evaluate(op, v));
		xregcomp(preg, s, icase ? REG_EXTENDED | REG_ICASE : REG_EXTENDED);
		nvfree(v);
		return preg;
	}
}

/* gradually increasing buffer */
static void qrealloc(char **b, int n, int *size) {

	if (! *b || n >= *size)
		*b = xrealloc(*b, *size = n + (n>>1) + 80);
}

/* resize field storage space */
static void fsrealloc(int size) {

	static int maxfields = 0;
	int i;

	if (size >= maxfields) {
		i = maxfields;
		maxfields = size + 16;
		Fields = (var *)xrealloc(Fields, maxfields * sizeof(var));
		for (; i<maxfields; i++) {
			Fields[i].type = VF_SPECIAL;
			Fields[i].string = NULL;
		}
	}

	if (size < nfields) {
		for (i=size; i<nfields; i++) {
			clrvar(Fields+i);
		}
	}
	nfields = size;
}

static int awk_split(char *s, node *spl, char **slist) {

	int l, n=0;
	char c[4];
	char *s1;
	regmatch_t pmatch[2];

	/* in worst case, each char would be a separate field */
	*slist = s1 = bb_xstrndup(s, bb_strlen(s) * 2 + 3);

	c[0] = c[1] = (char)spl->info;
	c[2] = c[3] = '\0';
	if (*getvar_s(V[RS]) == '\0') c[2] = '\n';

	if ((spl->info & OPCLSMASK) == OC_REGEXP) {		/* regex split */
		while (*s) {
			l = strcspn(s, c+2);
			if (regexec(icase ? spl->r.ire : spl->l.re, s, 1, pmatch, 0) == 0 &&
			pmatch[0].rm_so <= l) {
				l = pmatch[0].rm_so;
				if (pmatch[0].rm_eo == 0) { l++; pmatch[0].rm_eo++; }
			} else {
				pmatch[0].rm_eo = l;
				if (*(s+l)) pmatch[0].rm_eo++;
			}

			memcpy(s1, s, l);
			*(s1+l) = '\0';
			nextword(&s1);
			s += pmatch[0].rm_eo;
			n++;
		}
	} else if (c[0] == '\0') {		/* null split */
		while(*s) {
			*(s1++) = *(s++);
			*(s1++) = '\0';
			n++;
		}
	} else if (c[0] != ' ') {		/* single-character split */
		if (icase) {
			c[0] = toupper(c[0]);
			c[1] = tolower(c[1]);
		}
		if (*s1) n++;
		while ((s1 = strpbrk(s1, c))) {
			*(s1++) = '\0';
			n++;
		}
	} else {				/* space split */
		while (*s) {
			while (isspace(*s)) s++;
			if (! *s) break;
			n++;
			while (*s && !isspace(*s))
				*(s1++) = *(s++);
			*(s1++) = '\0';
		}
	}
	return n;
}

static void split_f0(void) {

	static char *fstrings = NULL;
	int i, n;
	char *s;

	if (is_f0_split)
		return;

	is_f0_split = TRUE;
	free(fstrings);
	fsrealloc(0);
	n = awk_split(getvar_s(V[F0]), &fsplitter.n, &fstrings);
	fsrealloc(n);
	s = fstrings;
	for (i=0; i<n; i++) {
		Fields[i].string = nextword(&s);
		Fields[i].type |= (VF_FSTR | VF_USER | VF_DIRTY);
	}

	/* set NF manually to avoid side effects */
	clrvar(V[NF]);
	V[NF]->type = VF_NUMBER | VF_SPECIAL;
	V[NF]->number = nfields;
}

/* perform additional actions when some internal variables changed */
static void handle_special(var *v) {

	int n;
	char *b, *sep, *s;
	int sl, l, len, i, bsize;

	if (! (v->type & VF_SPECIAL))
		return;

	if (v == V[NF]) {
		n = (int)getvar_i(v);
		fsrealloc(n);

		/* recalculate $0 */
		sep = getvar_s(V[OFS]);
		sl = bb_strlen(sep);
		b = NULL;
		len = 0;
		for (i=0; i<n; i++) {
			s = getvar_s(&Fields[i]);
			l = bb_strlen(s);
			if (b) {
				memcpy(b+len, sep, sl);
				len += sl;
			}
			qrealloc(&b, len+l+sl, &bsize);
			memcpy(b+len, s, l);
			len += l;
		}
		b[len] = '\0';
		setvar_p(V[F0], b);
		is_f0_split = TRUE;

	} else if (v == V[F0]) {
		is_f0_split = FALSE;

	} else if (v == V[FS]) {
		mk_splitter(getvar_s(v), &fsplitter);

	} else if (v == V[RS]) {
		mk_splitter(getvar_s(v), &rsplitter);

	} else if (v == V[IGNORECASE]) {
		icase = istrue(v);

	} else {						/* $n */
		n = getvar_i(V[NF]);
		setvar_i(V[NF], n > v-Fields ? n : v-Fields+1);
		/* right here v is invalid. Just to note... */
	}
}

/* step through func/builtin/etc arguments */
static node *nextarg(node **pn) {

	node *n;

	n = *pn;
	if (n && (n->info & OPCLSMASK) == OC_COMMA) {
		*pn = n->r.n;
		n = n->l.n;
	} else {
		*pn = NULL;
	}
	return n;
}

static void hashwalk_init(var *v, xhash *array) {

	char **w;
	hash_item *hi;
	int i;

	if (v->type & VF_WALK)
		free(v->x.walker);

	v->type |= VF_WALK;
	w = v->x.walker = (char **)xcalloc(2 + 2*sizeof(char *) + array->glen, 1);
	*w = *(w+1) = (char *)(w + 2);
	for (i=0; i<array->csize; i++) {
		hi = array->items[i];
		while(hi) {
			strcpy(*w, hi->name);
			nextword(w);
			hi = hi->next;
		}
	}
}

static int hashwalk_next(var *v) {

	char **w;

	w = v->x.walker;
	if (*(w+1) == *w)
		return FALSE;

	setvar_s(v, nextword(w+1));
	return TRUE;
}

/* evaluate node, return 1 when result is true, 0 otherwise */
static int ptest(node *pattern) {
	static var v;

	return istrue(evaluate(pattern, &v));
}

/* read next record from stream rsm into a variable v */
static int awk_getline(rstream *rsm, var *v) {

	char *b;
	regmatch_t pmatch[2];
	int a, p, pp=0, size;
	int fd, so, eo, r, rp;
	char c, *m, *s;

	/* we're using our own buffer since we need access to accumulating
	 * characters
	 */
	fd = fileno(rsm->F);
	m = rsm->buffer;
	a = rsm->adv;
	p = rsm->pos;
	size = rsm->size;
	c = (char) rsplitter.n.info;
	rp = 0;

	if (! m) qrealloc(&m, 256, &size);
	do {
		b = m + a;
		so = eo = p;
		r = 1;
		if (p > 0) {
			if ((rsplitter.n.info & OPCLSMASK) == OC_REGEXP) {
				if (regexec(icase ? rsplitter.n.r.ire : rsplitter.n.l.re,
												b, 1, pmatch, 0) == 0) {
					so = pmatch[0].rm_so;
					eo = pmatch[0].rm_eo;
					if (b[eo] != '\0')
						break;
				}
			} else if (c != '\0') {
				s = strchr(b+pp, c);
				if (s) {
					so = eo = s-b;
					eo++;
					break;
				}
			} else {
				while (b[rp] == '\n')
					rp++;
				s = strstr(b+rp, "\n\n");
				if (s) {
					so = eo = s-b;
					while (b[eo] == '\n') eo++;
					if (b[eo] != '\0')
						break;
				}
			}
		}

		if (a > 0) {
			memmove(m, (const void *)(m+a), p+1);
			b = m;
			a = 0;
		}

		qrealloc(&m, a+p+128, &size);
		b = m + a;
		pp = p;
		p += safe_read(fd, b+p, size-p-1);
		if (p < pp) {
			p = 0;
			r = 0;
			setvar_i(V[ERRNO], errno);
		}
		b[p] = '\0';

	} while (p > pp);

	if (p == 0) {
		r--;
	} else {
		c = b[so]; b[so] = '\0';
		setvar_s(v, b+rp);
		v->type |= VF_USER;
		b[so] = c;
		c = b[eo]; b[eo] = '\0';
		setvar_s(V[RT], b+so);
		b[eo] = c;
	}

	rsm->buffer = m;
	rsm->adv = a + eo;
	rsm->pos = p - eo;
	rsm->size = size;

	return r;
}

static int fmt_num(char *b, int size, char *format, double n, int int_as_int) {

	int r=0;
	char c, *s=format;

	if (int_as_int && n == (int)n) {
		r = snprintf(b, size, "%d", (int)n);
	} else {
		do { c = *s; } while (*s && *++s);
		if (strchr("diouxX", c)) {
			r = snprintf(b, size, format, (int)n);
		} else if (strchr("eEfgG", c)) {
			r = snprintf(b, size, format, n);
		} else {
			runtime_error(EMSG_INV_FMT);
		}
	}
	return r;
}


/* formatted output into an allocated buffer, return ptr to buffer */
static char *awk_printf(node *n) {

	char *b = NULL;
	char *fmt, *s, *s1, *f;
	int i, j, incr, bsize;
	char c, c1;
	var *v, *arg;

	v = nvalloc(1);
	fmt = f = bb_xstrdup(getvar_s(evaluate(nextarg(&n), v)));

	i = 0;
	while (*f) {
		s = f;
		while (*f && (*f != '%' || *(++f) == '%'))
			f++;
		while (*f && !isalpha(*f)) 
			f++;

		incr = (f - s) + MAXVARFMT;
		qrealloc(&b, incr+i, &bsize);
		c = *f; if (c != '\0') f++;
		c1 = *f ; *f = '\0';
		arg = evaluate(nextarg(&n), v);

		j = i;
		if (c == 'c' || !c) {
			i += sprintf(b+i, s,
					is_numeric(arg) ? (char)getvar_i(arg) : *getvar_s(arg));

		} else if (c == 's') {
		    s1 = getvar_s(arg);
			qrealloc(&b, incr+i+bb_strlen(s1), &bsize);
			i += sprintf(b+i, s, s1);

		} else {
			i += fmt_num(b+i, incr, s, getvar_i(arg), FALSE);
		}
		*f = c1;

		/* if there was an error while sprintf, return value is negative */
		if (i < j) i = j;

	}

	b = xrealloc(b, i+1);
	free(fmt);
	nvfree(v);
	b[i] = '\0';
	return b;
}

/* common substitution routine
 * replace (nm) substring of (src) that match (n) with (repl), store
 * result into (dest), return number of substitutions. If nm=0, replace
 * all matches. If src or dst is NULL, use $0. If ex=TRUE, enable
 * subexpression matching (\1-\9)
 */
static int awk_sub(node *rn, char *repl, int nm, var *src, var *dest, int ex) {

	char *ds = NULL;
	char *sp, *s;
	int c, i, j, di, rl, so, eo, nbs, n, dssize;
	regmatch_t pmatch[10];
	regex_t sreg, *re;

	re = as_regex(rn, &sreg);
	if (! src) src = V[F0];
	if (! dest) dest = V[F0];

	i = di = 0;
	sp = getvar_s(src);
	rl = bb_strlen(repl);
	while (regexec(re, sp, 10, pmatch, sp==getvar_s(src) ? 0:REG_NOTBOL) == 0) {
		so = pmatch[0].rm_so;
		eo = pmatch[0].rm_eo;

		qrealloc(&ds, di + eo + rl, &dssize);
		memcpy(ds + di, sp, eo);
		di += eo;
		if (++i >= nm) {
			/* replace */
			di -= (eo - so);
			nbs = 0;
			for (s = repl; *s; s++) {
				ds[di++] = c = *s;
				if (c == '\\') {
					nbs++;
					continue;
				}
				if (c == '&' || (ex && c >= '0' && c <= '9')) {
					di -= ((nbs + 3) >> 1);
					j = 0;
					if (c != '&') {
						j = c - '0';
						nbs++;
					}
					if (nbs % 2) {
						ds[di++] = c;
					} else {
						n = pmatch[j].rm_eo - pmatch[j].rm_so;
						qrealloc(&ds, di + rl + n, &dssize);
						memcpy(ds + di, sp + pmatch[j].rm_so, n);
						di += n;
					}
				}
				nbs = 0;
			}
		}

		sp += eo;
		if (i == nm) break;
		if (eo == so) {
			if (! (ds[di++] = *sp++)) break;
		}
	}

	qrealloc(&ds, di + strlen(sp), &dssize);
	strcpy(ds + di, sp);
	setvar_p(dest, ds);
	if (re == &sreg) regfree(re);
	return i;
}

static var *exec_builtin(node *op, var *res) {

	int (*to_xxx)(int);
	var *tv;
	node *an[4];
	var  *av[4];
	char *as[4];
	regmatch_t pmatch[2];
	regex_t sreg, *re;
	static tsplitter tspl;
	node *spl;
	unsigned long isr, info;
	int nargs;
	time_t tt;
	char *s, *s1;
	int i, l, ll, n;

	tv = nvalloc(4);
	isr = info = op->info;
	op = op->l.n;

	av[2] = av[3] = NULL;
	for (i=0 ; i<4 && op ; i++) {
		an[i] = nextarg(&op);
		if (isr & 0x09000000) av[i] = evaluate(an[i], &tv[i]);
		if (isr & 0x08000000) as[i] = getvar_s(av[i]);
		isr >>= 1;
	}

	nargs = i;
	if (nargs < (info >> 30))
		runtime_error(EMSG_TOO_FEW_ARGS);

	switch (info & OPNMASK) {

	  case B_a2:
#ifdef CONFIG_FEATURE_AWK_MATH
		setvar_i(res, atan2(getvar_i(av[i]), getvar_i(av[1])));
#else
		runtime_error(EMSG_NO_MATH);
#endif
		break;

	  case B_sp:
		if (nargs > 2) {
			spl = (an[2]->info & OPCLSMASK) == OC_REGEXP ?
				an[2] : mk_splitter(getvar_s(evaluate(an[2], &tv[2])), &tspl);
		} else {
			spl = &fsplitter.n;
		}

		n = awk_split(as[0], spl, &s);
		s1 = s;
		clear_array(iamarray(av[1]));
		for (i=1; i<=n; i++)
			setari_u(av[1], i, nextword(&s1));
		free(s);
		setvar_i(res, n);
		break;

	  case B_ss:
		l = bb_strlen(as[0]);
		i = getvar_i(av[1]) - 1;
		if (i>l) i=l; if (i<0) i=0;
		n = (nargs > 2) ? getvar_i(av[2]) : l-i;
		if (n<0) n=0;
		s = xmalloc(n+1);
		strncpy(s, as[0]+i, n);
		s[n] = '\0';
		setvar_p(res, s);
		break;

	  case B_lo:
		to_xxx = tolower;
		goto lo_cont;

	  case B_up:
		to_xxx = toupper;
lo_cont:
		s1 = s = bb_xstrdup(as[0]);
		while (*s1) {
			*s1 = (*to_xxx)(*s1);
			s1++;
		}
		setvar_p(res, s);
		break;

	  case B_ix:
		n = 0;
		ll = bb_strlen(as[1]);
		l = bb_strlen(as[0]) - ll;
		if (ll > 0 && l >= 0) {
			if (! icase) {
				s = strstr(as[0], as[1]);
				if (s) n = (s - as[0]) + 1;
			} else {
				/* this piece of code is terribly slow and
				 * really should be rewritten
				 */
				for (i=0; i<=l; i++) {
					if (strncasecmp(as[0]+i, as[1], ll) == 0) {
						n = i+1;
						break;
					}
				}
			}
		}
		setvar_i(res, n);
		break;

	  case B_ti:
		if (nargs > 1)
			tt = getvar_i(av[1]);
		else
			time(&tt);
		s = (nargs > 0) ? as[0] : "%a %b %d %H:%M:%S %Z %Y";
		i = strftime(buf, MAXVARFMT, s, localtime(&tt));
		buf[i] = '\0';
		setvar_s(res, buf);
		break;

	  case B_ma:
		re = as_regex(an[1], &sreg);
		n = regexec(re, as[0], 1, pmatch, 0);
		if (n == 0) {
			pmatch[0].rm_so++;
			pmatch[0].rm_eo++;
		} else {
			pmatch[0].rm_so = 0;
			pmatch[0].rm_eo = -1;
		}
		setvar_i(newvar("RSTART"), pmatch[0].rm_so);
		setvar_i(newvar("RLENGTH"), pmatch[0].rm_eo - pmatch[0].rm_so);
		setvar_i(res, pmatch[0].rm_so);
		if (re == &sreg) regfree(re);
		break;

	  case B_ge:
		awk_sub(an[0], as[1], getvar_i(av[2]), av[3], res, TRUE);
		break;

	  case B_gs:
		setvar_i(res, awk_sub(an[0], as[1], 0, av[2], av[2], FALSE));
		break;

	  case B_su:
		setvar_i(res, awk_sub(an[0], as[1], 1, av[2], av[2], FALSE));
		break;
	}

	nvfree(tv);
	return res;
}

/*
 * Evaluate node - the heart of the program. Supplied with subtree
 * and place where to store result. returns ptr to result.
 */
#define XC(n) ((n) >> 8)

static var *evaluate(node *op, var *res) {

 	/* This procedure is recursive so we should count every byte */
	static var *fnargs = NULL;
	static unsigned int seed = 1;
	static regex_t sreg;
	node *op1;
	var *v1;
	union {
		var *v;
		char *s;
		double d;
		int i;
	} L, R;
	unsigned long opinfo;
	short opn;
	union {
		char *s;
		rstream *rsm;
		FILE *F;
		var *v;
		regex_t *re;
		unsigned long info;
	} X;

	if (! op)
		return setvar_s(res, NULL);

	v1 = nvalloc(2);

	while (op) {

		opinfo = op->info;
		opn = (short)(opinfo & OPNMASK);
		lineno = op->lineno;

 		/* execute inevitable things */
		op1 = op->l.n;
		if (opinfo & OF_RES1) X.v = L.v = evaluate(op1, v1);
		if (opinfo & OF_RES2) R.v = evaluate(op->r.n, v1+1);
		if (opinfo & OF_STR1) L.s = getvar_s(L.v);
		if (opinfo & OF_STR2) R.s = getvar_s(R.v);
		if (opinfo & OF_NUM1) L.d = getvar_i(L.v);

		switch (XC(opinfo & OPCLSMASK)) {

		  /* -- iterative node type -- */

		  /* test pattern */
		  case XC( OC_TEST ):
			if ((op1->info & OPCLSMASK) == OC_COMMA) {
				/* it's range pattern */
				if ((opinfo & OF_CHECKED) || ptest(op1->l.n)) {
					op->info |= OF_CHECKED;
					if (ptest(op1->r.n))
						op->info &= ~OF_CHECKED;

					op = op->a.n;
				} else {
					op = op->r.n;
				}
			} else {
				op = (ptest(op1)) ? op->a.n : op->r.n;
			}
			break;

		  /* just evaluate an expression, also used as unconditional jump */
		  case XC( OC_EXEC ):
		  	break;

		  /* branch, used in if-else and various loops */
		  case XC( OC_BR ):
		  	op = istrue(L.v) ? op->a.n : op->r.n;
			break;

		  /* initialize for-in loop */
		  case XC( OC_WALKINIT ):
		  	hashwalk_init(L.v, iamarray(R.v));
			break;

		  /* get next array item */
		  case XC( OC_WALKNEXT ):
			op = hashwalk_next(L.v) ? op->a.n : op->r.n;
			break;

		  case XC( OC_PRINT ):
		  case XC( OC_PRINTF ):
			X.F = stdout;
		  	if (op->r.n) {
				X.rsm = newfile(R.s);
				if (! X.rsm->F) {
					if (opn == '|') {
						if((X.rsm->F = popen(R.s, "w")) == NULL)
							bb_perror_msg_and_die("popen");
						X.rsm->is_pipe = 1;
					} else {
						X.rsm->F = bb_xfopen(R.s, opn=='w' ? "w" : "a");
					}
				}
				X.F = X.rsm->F;
			}

			if ((opinfo & OPCLSMASK) == OC_PRINT) {
		  		if (! op1) {
					fputs(getvar_s(V[F0]), X.F);
				} else {
					while (op1) {
						L.v = evaluate(nextarg(&op1), v1);
						if (L.v->type & VF_NUMBER) {
							fmt_num(buf, MAXVARFMT, getvar_s(V[OFMT]),
														getvar_i(L.v), TRUE);
							fputs(buf, X.F);
						} else {
							fputs(getvar_s(L.v), X.F);
						}

						if (op1) fputs(getvar_s(V[OFS]), X.F);
					}
				}
				fputs(getvar_s(V[ORS]), X.F);

			} else {	/* OC_PRINTF */
				L.s = awk_printf(op1);
				fputs(L.s, X.F);
				free(L.s);
			}
			fflush(X.F);
			break;

		  case XC( OC_DELETE ):
		  	X.info = op1->info & OPCLSMASK;
		  	if (X.info == OC_VAR) {
				R.v = op1->l.v;
			} else if (X.info == OC_FNARG) {
				R.v = &fnargs[op1->l.i];
			} else {
				runtime_error(EMSG_NOT_ARRAY);
			}

		  	if (op1->r.n) {
				clrvar(L.v);
				L.s = getvar_s(evaluate(op1->r.n, v1));
				hash_remove(iamarray(R.v), L.s);
			} else {
				clear_array(iamarray(R.v));
			}
			break;

		  case XC( OC_NEWSOURCE ):
		  	programname = op->l.s;
			break;

		  case XC( OC_RETURN ):
			copyvar(res, L.v);
			break;

		  case XC( OC_NEXTFILE ):
		  	nextfile = TRUE;
		  case XC( OC_NEXT ):
		  	nextrec = TRUE;
		  case XC( OC_DONE ):
			clrvar(res);
			break;

		  case XC( OC_EXIT ):
		  	awk_exit(L.d);

		  /* -- recursive node type -- */

		  case XC( OC_VAR ):
		  	L.v = op->l.v;
			if (L.v == V[NF])
				split_f0();
			goto v_cont;

		  case XC( OC_FNARG ):
		  	L.v = &fnargs[op->l.i];

v_cont:
		 	res = (op->r.n) ? findvar(iamarray(L.v), R.s) : L.v;
			break;

		  case XC( OC_IN ):
			setvar_i(res, hash_search(iamarray(R.v), L.s) ? 1 : 0);
			break;

		  case XC( OC_REGEXP ):
		  	op1 = op;
			L.s = getvar_s(V[F0]);
			goto re_cont;

		  case XC( OC_MATCH ):
		  	op1 = op->r.n;
re_cont:
			X.re = as_regex(op1, &sreg);
			R.i = regexec(X.re, L.s, 0, NULL, 0);
			if (X.re == &sreg) regfree(X.re);
			setvar_i(res, (R.i == 0 ? 1 : 0) ^ (opn == '!' ? 1 : 0));
			break;

		  case XC( OC_MOVE ):
		  	/* if source is a temporary string, jusk relink it to dest */
			if (R.v == v1+1 && R.v->string) {
				res = setvar_p(L.v, R.v->string);
				R.v->string = NULL;
			} else {
		  		res = copyvar(L.v, R.v);
			}
			break;

		  case XC( OC_TERNARY ):
		  	if ((op->r.n->info & OPCLSMASK) != OC_COLON)
				runtime_error(EMSG_POSSIBLE_ERROR);
			res = evaluate(istrue(L.v) ? op->r.n->l.n : op->r.n->r.n, res);
			break;

		  case XC( OC_FUNC ):
		  	if (! op->r.f->body.first)
				runtime_error(EMSG_UNDEF_FUNC);

			X.v = R.v = nvalloc(op->r.f->nargs+1);
			while (op1) {
				L.v = evaluate(nextarg(&op1), v1);
				copyvar(R.v, L.v);
				R.v->type |= VF_CHILD;
				R.v->x.parent = L.v;
				if (++R.v - X.v >= op->r.f->nargs)
					break;
			}

			R.v = fnargs;
			fnargs = X.v;

			L.s = programname;
			res = evaluate(op->r.f->body.first, res);
			programname = L.s;

			nvfree(fnargs);
			fnargs = R.v;
			break;

		  case XC( OC_GETLINE ):
		  case XC( OC_PGETLINE ):
		  	if (op1) {
				X.rsm = newfile(L.s);
				if (! X.rsm->F) {
					if ((opinfo & OPCLSMASK) == OC_PGETLINE) {
						X.rsm->F = popen(L.s, "r");
						X.rsm->is_pipe = TRUE;
					} else {
						X.rsm->F = fopen(L.s, "r");		/* not bb_xfopen! */
					}
				}
			} else {
				if (! iF) iF = next_input_file();
				X.rsm = iF;
			}

			if (! X.rsm->F) {
				setvar_i(V[ERRNO], errno);
				setvar_i(res, -1);
				break;
			}

			if (! op->r.n)
				R.v = V[F0];

			L.i = awk_getline(X.rsm, R.v);
			if (L.i > 0) {
				if (! op1) {
					incvar(V[FNR]);
					incvar(V[NR]);
				}
			}
			setvar_i(res, L.i);
			break;

   		  /* simple builtins */
		  case XC( OC_FBLTIN ):
		  	switch (opn) {

			  case F_in:
			  	R.d = (int)L.d;
				break;

			  case F_rn:
			  	R.d =  (double)rand() / (double)RAND_MAX;
				break;

#ifdef CONFIG_FEATURE_AWK_MATH
			  case F_co:
			  	R.d = cos(L.d);
				break;

			  case F_ex:
			  	R.d = exp(L.d);
				break;

			  case F_lg:
			  	R.d = log(L.d);
				break;

			  case F_si:
			  	R.d = sin(L.d);
				break;

			  case F_sq:
			  	R.d = sqrt(L.d);
				break;
#else
			  case F_co:
			  case F_ex:
			  case F_lg:
			  case F_si:
			  case F_sq:
				runtime_error(EMSG_NO_MATH);
				break;
#endif

			  case F_sr:
				R.d = (double)seed;
				seed = op1 ? (unsigned int)L.d : (unsigned int)time(NULL);
				srand(seed);
				break;

			  case F_ti:
				R.d = time(NULL);
				break;

			  case F_le:
			  	if (! op1)
					L.s = getvar_s(V[F0]);
				R.d = bb_strlen(L.s);
				break;

			  case F_sy:
				fflush(NULL);
				R.d = (L.s && *L.s) ? system(L.s) : 0;
				break;

			  case F_ff:
				if (! op1)
					fflush(stdout);
				else {
					if (L.s && *L.s) {
						X.rsm = newfile(L.s);
						fflush(X.rsm->F);
					} else {
						fflush(NULL);
					}
				}
				break;

			  case F_cl:
				X.rsm = (rstream *)hash_search(fdhash, L.s);
				if (X.rsm) {
					R.i = X.rsm->is_pipe ? pclose(X.rsm->F) : fclose(X.rsm->F);
					free(X.rsm->buffer);
					hash_remove(fdhash, L.s);
				}
				if (R.i != 0)
					setvar_i(V[ERRNO], errno);
				R.d = (double)R.i;
				break;
			}
			setvar_i(res, R.d);
			break;

		  case XC( OC_BUILTIN ):
			res = exec_builtin(op, res);
			break;

		  case XC( OC_SPRINTF ):
		  	setvar_p(res, awk_printf(op1));
			break;

		  case XC( OC_UNARY ):
		  	X.v = R.v;
		  	L.d = R.d = getvar_i(R.v);
		  	switch (opn) {
			  case 'P':
			  	L.d = ++R.d;
				goto r_op_change;
			  case 'p':
			  	R.d++;
				goto r_op_change;
			  case 'M':
			  	L.d = --R.d;
				goto r_op_change;
			  case 'm':
			  	R.d--;
				goto r_op_change;
			  case '!':
			    L.d = istrue(X.v) ? 0 : 1;
				break;
			  case '-':
			  	L.d = -R.d;
				break;
			r_op_change:
				setvar_i(X.v, R.d);
			}
			setvar_i(res, L.d);
			break;

		  case XC( OC_FIELD ):
		  	R.i = (int)getvar_i(R.v);
		  	if (R.i == 0) {
				res = V[F0];
			} else {
				split_f0();
				if (R.i > nfields)
					fsrealloc(R.i);

				res = &Fields[R.i-1];
			}
			break;

		  /* concatenation (" ") and index joining (",") */
		  case XC( OC_CONCAT ):
		  case XC( OC_COMMA ):
			opn = bb_strlen(L.s) + bb_strlen(R.s) + 2;
		  	X.s = (char *)xmalloc(opn);
			strcpy(X.s, L.s);
			if ((opinfo & OPCLSMASK) == OC_COMMA) {
				L.s = getvar_s(V[SUBSEP]);
				X.s = (char *)xrealloc(X.s, opn + bb_strlen(L.s));
				strcat(X.s, L.s);
			}
			strcat(X.s, R.s);
			setvar_p(res, X.s);
			break;

		  case XC( OC_LAND ):
			setvar_i(res, istrue(L.v) ? ptest(op->r.n) : 0);
			break;

		  case XC( OC_LOR ):
			setvar_i(res, istrue(L.v) ? 1 : ptest(op->r.n));
			break;

		  case XC( OC_BINARY ):
		  case XC( OC_REPLACE ):
		  	R.d = getvar_i(R.v);
			switch (opn) {
			  case '+':
			  	L.d += R.d;
				break;
			  case '-':
			  	L.d -= R.d;
				break;
			  case '*':
			  	L.d *= R.d;
				break;
			  case '/':
			  	if (R.d == 0) runtime_error(EMSG_DIV_BY_ZERO);
			  	L.d /= R.d;
				break;
			  case '&':
#ifdef CONFIG_FEATURE_AWK_MATH
			  	L.d = pow(L.d, R.d);
#else
				runtime_error(EMSG_NO_MATH);
#endif
				break;
			  case '%':
			  	if (R.d == 0) runtime_error(EMSG_DIV_BY_ZERO);
			  	L.d -= (int)(L.d / R.d) * R.d;
				break;
			}
			res = setvar_i(((opinfo&OPCLSMASK) == OC_BINARY) ? res : X.v, L.d);
			break;

		  case XC( OC_COMPARE ):
			if (is_numeric(L.v) && is_numeric(R.v)) {
				L.d = getvar_i(L.v) - getvar_i(R.v);
			} else {
				L.s = getvar_s(L.v);
				R.s = getvar_s(R.v);
				L.d = icase ? strcasecmp(L.s, R.s) : strcmp(L.s, R.s);
			}
			switch (opn & 0xfe) {
			  case 0:
			  	R.i = (L.d > 0);
				break;
			  case 2:
			  	R.i = (L.d >= 0);
				break;
			  case 4:
			  	R.i = (L.d == 0);
				break;
			}
			setvar_i(res, (opn & 0x1 ? R.i : !R.i) ? 1 : 0);
			break;

		  default:
		  	runtime_error(EMSG_POSSIBLE_ERROR);
		}
		if ((opinfo & OPCLSMASK) <= SHIFT_TIL_THIS)
			op = op->a.n;
		if ((opinfo & OPCLSMASK) >= RECUR_FROM_THIS)
			break;
		if (nextrec)
			break;
	}
	nvfree(v1);
	return res;
}


/* -------- main & co. -------- */

static int awk_exit(int r) {

	unsigned int i;
	hash_item *hi;
	static var tv;

	if (! exiting) {
		exiting = TRUE;
		evaluate(endseq.first, &tv);
	}

	/* waiting for children */
	for (i=0; i<fdhash->csize; i++) {
		hi = fdhash->items[i];
		while(hi) {
			if (hi->data.rs.F && hi->data.rs.is_pipe)
				pclose(hi->data.rs.F);
			hi = hi->next;
		}
	}

	exit(r);
}

/* if expr looks like "var=value", perform assignment and return 1,
 * otherwise return 0 */
static int is_assignment(char *expr) {

	char *exprc, *s, *s0, *s1;

	exprc = bb_xstrdup(expr);
	if (!isalnum_(*exprc) || (s = strchr(exprc, '=')) == NULL) {
		free(exprc);
		return FALSE;
	}

	*(s++) = '\0';
	s0 = s1 = s;
	while (*s)
		*(s1++) = nextchar(&s);

	*s1 = '\0';
	setvar_u(newvar(exprc), s0);
	free(exprc);
	return TRUE;
}

/* switch to next input file */
static rstream *next_input_file(void) {

	static rstream rsm;
	FILE *F = NULL;
	char *fname, *ind;
	static int files_happen = FALSE;

	if (rsm.F) fclose(rsm.F);
	rsm.F = NULL;
	rsm.pos = rsm.adv = 0;

	do {
		if (getvar_i(V[ARGIND])+1 >= getvar_i(V[ARGC])) {
			if (files_happen)
				return NULL;
			fname = "-";
			F = stdin;
		} else {
			ind = getvar_s(incvar(V[ARGIND]));
			fname = getvar_s(findvar(iamarray(V[ARGV]), ind));
			if (fname && *fname && !is_assignment(fname))
				F = afopen(fname, "r");
		}
	} while (!F);

	files_happen = TRUE;
	setvar_s(V[FILENAME], fname);
	rsm.F = F;
	return &rsm;
}

extern int awk_main(int argc, char **argv) {

	char *s, *s1;
	int i, j, c;
	var *v;
	static var tv;
	char **envp;
	static int from_file = FALSE;
	rstream *rsm;
	FILE *F, *stdfiles[3];
	static char * stdnames = "/dev/stdin\0/dev/stdout\0/dev/stderr";

	/* allocate global buffer */
	buf = xmalloc(MAXVARFMT+1);

	vhash = hash_init();
	ahash = hash_init();
	fdhash = hash_init();
	fnhash = hash_init();

	/* initialize variables */
	for (i=0;  *vNames;  i++) {
		V[i] = v = newvar(nextword(&vNames));
		if (*vValues != '\377')
			setvar_s(v, nextword(&vValues));
		else
			setvar_i(v, 0);

		if (*vNames == '*') {
			v->type |= VF_SPECIAL;
			vNames++;
		}
	}

	handle_special(V[FS]);
	handle_special(V[RS]);

	stdfiles[0] = stdin;
	stdfiles[1] = stdout;
	stdfiles[2] = stderr;
	for (i=0; i<3; i++) {
		rsm = newfile(nextword(&stdnames));
		rsm->F = stdfiles[i];
	}

	for (envp=environ; *envp; envp++) {
		s = bb_xstrdup(*envp);
		s1 = strchr(s, '=');
		*(s1++) = '\0';
		setvar_u(findvar(iamarray(V[ENVIRON]), s), s1);
		free(s);
	}

	while((c = getopt(argc, argv, "F:v:f:W:")) != EOF) {
		switch (c) {
			case 'F':
				setvar_s(V[FS], optarg);
				break;
			case 'v':
				if (! is_assignment(optarg))
					bb_show_usage();
				break;
			case 'f':
				from_file = TRUE;
				F = afopen(programname = optarg, "r");
				s = NULL;
				/* one byte is reserved for some trick in next_token */
				for (i=j=1; j>0; i+=j) {
					s = (char *)xrealloc(s, i+4096);
					j = fread(s+i, 1, 4094, F);
				}
				s[i] = '\0';
				fclose(F);
				parse_program(s+1);
				free(s);
				break;
			case 'W':
				bb_error_msg("Warning: unrecognized option '-W %s' ignored\n", optarg);
				break;

			default:
				bb_show_usage();
		}
	}

	if (!from_file) {
		if (argc == optind)
			bb_show_usage();
		programname="cmd. line";
		parse_program(argv[optind++]);

	}

	/* fill in ARGV array */
	setvar_i(V[ARGC], argc - optind + 1);
	setari_u(V[ARGV], 0, "awk");
	for(i=optind; i < argc; i++)
		setari_u(V[ARGV], i+1-optind, argv[i]);

	evaluate(beginseq.first, &tv);
	if (! mainseq.first && ! endseq.first)
		awk_exit(EXIT_SUCCESS);

	/* input file could already be opened in BEGIN block */
	if (! iF) iF = next_input_file();

	/* passing through input files */
	while (iF) {

		nextfile = FALSE;
		setvar_i(V[FNR], 0);

		while ((c = awk_getline(iF, V[F0])) > 0) {

			nextrec = FALSE;
			incvar(V[NR]);
			incvar(V[FNR]);
			evaluate(mainseq.first, &tv);

			if (nextfile)
				break;
		}

		if (c < 0)
			runtime_error(strerror(errno));

		iF = next_input_file();

	}

	awk_exit(EXIT_SUCCESS);

	return 0;
}

