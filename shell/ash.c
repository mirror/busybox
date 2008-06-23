/* vi: set sw=4 ts=4: */
/*
 * ash shell port for busybox
 *
 * Copyright (c) 1989, 1991, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1997-2005 Herbert Xu <herbert@gondor.apana.org.au>
 * was re-ported from NetBSD and debianized.
 *
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 *
 * Original BSD copyright notice is retained at the end of this file.
 */

/*
 * rewrite arith.y to micro stack based cryptic algorithm by
 * Copyright (c) 2001 Aaron Lehmann <aaronl@vitelus.com>
 *
 * Modified by Paul Mundt <lethal@linux-sh.org> (c) 2004 to support
 * dynamic variables.
 *
 * Modified by Vladimir Oleynik <dzo@simtreas.ru> (c) 2001-2005 to be
 * used in busybox and size optimizations,
 * rewrote arith (see notes to this), added locale support,
 * rewrote dynamic variables.
 *
 */

/*
 * The follow should be set to reflect the type of system you have:
 *      JOBS -> 1 if you have Berkeley job control, 0 otherwise.
 *      define SYSV if you are running under System V.
 *      define DEBUG=1 to compile in debugging ('set -o debug' to turn on)
 *      define DEBUG=2 to compile in and turn on debugging.
 *
 * When debugging is on, debugging info will be written to ./trace and
 * a quit signal will generate a core dump.
 */
#define DEBUG 0
#define PROFILE 0

#define IFS_BROKEN

#define JOBS ENABLE_ASH_JOB_CONTROL

#if DEBUG
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

#include "busybox.h" /* for applet_names */
#include <paths.h>
#include <setjmp.h>
#include <fnmatch.h>
#if JOBS || ENABLE_ASH_READ_NCHARS
#include <termios.h>
#endif

#if defined(__uClinux__)
#error "Do not even bother, ash will not run on uClinux"
#endif


/* ============ Hash table sizes. Configurable. */

#define VTABSIZE 39
#define ATABSIZE 39
#define CMDTABLESIZE 31         /* should be prime */


/* ============ Misc helpers */

#define xbarrier() do { __asm__ __volatile__ ("": : :"memory"); } while (0)

/* C99 say: "char" declaration may be signed or unsigned default */
#define signed_char2int(sc) ((int)((signed char)sc))


/* ============ Shell options */

static const char *const optletters_optnames[] = {
	"e"   "errexit",
	"f"   "noglob",
	"I"   "ignoreeof",
	"i"   "interactive",
	"m"   "monitor",
	"n"   "noexec",
	"s"   "stdin",
	"x"   "xtrace",
	"v"   "verbose",
	"C"   "noclobber",
	"a"   "allexport",
	"b"   "notify",
	"u"   "nounset",
	"\0"  "vi"
#if DEBUG
	,"\0"  "nolog"
	,"\0"  "debug"
#endif
};

#define optletters(n) optletters_optnames[(n)][0]
#define optnames(n) (&optletters_optnames[(n)][1])

enum { NOPTS = ARRAY_SIZE(optletters_optnames) };

static char optlist[NOPTS] ALIGN1;

#define eflag optlist[0]
#define fflag optlist[1]
#define Iflag optlist[2]
#define iflag optlist[3]
#define mflag optlist[4]
#define nflag optlist[5]
#define sflag optlist[6]
#define xflag optlist[7]
#define vflag optlist[8]
#define Cflag optlist[9]
#define aflag optlist[10]
#define bflag optlist[11]
#define uflag optlist[12]
#define viflag optlist[13]
#if DEBUG
#define nolog optlist[14]
#define debug optlist[15]
#endif


/* ============ Misc data */

static const char homestr[] ALIGN1 = "HOME";
static const char snlfmt[] ALIGN1 = "%s\n";
static const char illnum[] ALIGN1 = "Illegal number: %s";

/*
 * We enclose jmp_buf in a structure so that we can declare pointers to
 * jump locations.  The global variable handler contains the location to
 * jump to when an exception occurs, and the global variable exception
 * contains a code identifying the exception.  To implement nested
 * exception handlers, the user should save the value of handler on entry
 * to an inner scope, set handler to point to a jmploc structure for the
 * inner scope, and restore handler on exit from the scope.
 */
struct jmploc {
	jmp_buf loc;
};

struct globals_misc {
	/* pid of main shell */
	int rootpid;
	/* shell level: 0 for the main shell, 1 for its children, and so on */
	int shlvl;
#define rootshell (!shlvl)
	char *minusc;  /* argument to -c option */

	char *curdir; // = nullstr;     /* current working directory */
	char *physdir; // = nullstr;    /* physical working directory */

	char *arg0; /* value of $0 */

	struct jmploc *exception_handler;

// disabled by vda: cannot understand how it was supposed to work -
// cannot fix bugs. That's why you have to explain your non-trivial designs!
//	/* do we generate EXSIG events */
//	int exsig; /* counter */
	volatile int suppressint; /* counter */
	volatile /*sig_atomic_t*/ smallint intpending; /* 1 = got SIGINT */
	/* last pending signal */
	volatile /*sig_atomic_t*/ smallint pendingsig;
	smallint exception; /* kind of exception (0..5) */
	/* exceptions */
#define EXINT 0         /* SIGINT received */
#define EXERROR 1       /* a generic error */
#define EXSHELLPROC 2   /* execute a shell procedure */
#define EXEXEC 3        /* command execution failed */
#define EXEXIT 4        /* exit the shell */
#define EXSIG 5         /* trapped signal in wait(1) */

	/* trap handler commands */
	smallint isloginsh;
	char *trap[NSIG];
	char nullstr[1];        /* zero length string */
	/*
	 * Sigmode records the current value of the signal handlers for the various
	 * modes.  A value of zero means that the current handler is not known.
	 * S_HARD_IGN indicates that the signal was ignored on entry to the shell,
	 */
	char sigmode[NSIG - 1];
#define S_DFL 1                 /* default signal handling (SIG_DFL) */
#define S_CATCH 2               /* signal is caught */
#define S_IGN 3                 /* signal is ignored (SIG_IGN) */
#define S_HARD_IGN 4            /* signal is ignored permenantly */
#define S_RESET 5               /* temporary - to reset a hard ignored sig */

	/* indicates specified signal received */
	char gotsig[NSIG - 1];
};
extern struct globals_misc *const ash_ptr_to_globals_misc;
#define G_misc (*ash_ptr_to_globals_misc)
#define rootpid   (G_misc.rootpid  )
#define shlvl     (G_misc.shlvl    )
#define minusc    (G_misc.minusc   )
#define curdir    (G_misc.curdir   )
#define physdir   (G_misc.physdir  )
#define arg0      (G_misc.arg0     )
#define exception_handler (G_misc.exception_handler)
#define exception         (G_misc.exception        )
#define suppressint       (G_misc.suppressint      )
#define intpending        (G_misc.intpending       )
//#define exsig             (G_misc.exsig            )
#define pendingsig        (G_misc.pendingsig       )
#define isloginsh (G_misc.isloginsh)
#define trap      (G_misc.trap     )
#define nullstr   (G_misc.nullstr  )
#define sigmode   (G_misc.sigmode  )
#define gotsig    (G_misc.gotsig   )
#define INIT_G_misc() do { \
	(*(struct globals_misc**)&ash_ptr_to_globals_misc) = xzalloc(sizeof(G_misc)); \
	barrier(); \
	curdir = nullstr; \
	physdir = nullstr; \
} while (0)


/* ============ Interrupts / exceptions */

/*
 * These macros allow the user to suspend the handling of interrupt signals
 * over a period of time.  This is similar to SIGHOLD or to sigblock, but
 * much more efficient and portable.  (But hacking the kernel is so much
 * more fun than worrying about efficiency and portability. :-))
 */
#define INT_OFF \
	do { \
		suppressint++; \
		xbarrier(); \
	} while (0)

/*
 * Called to raise an exception.  Since C doesn't include exceptions, we
 * just do a longjmp to the exception handler.  The type of exception is
 * stored in the global variable "exception".
 */
static void raise_exception(int) ATTRIBUTE_NORETURN;
static void
raise_exception(int e)
{
#if DEBUG
	if (exception_handler == NULL)
		abort();
#endif
	INT_OFF;
	exception = e;
	longjmp(exception_handler->loc, 1);
}

/*
 * Called from trap.c when a SIGINT is received.  (If the user specifies
 * that SIGINT is to be trapped or ignored using the trap builtin, then
 * this routine is not called.)  Suppressint is nonzero when interrupts
 * are held using the INT_OFF macro.  (The test for iflag is just
 * defensive programming.)
 */
static void raise_interrupt(void) ATTRIBUTE_NORETURN;
static void
raise_interrupt(void)
{
	int i;

	intpending = 0;
	/* Signal is not automatically unmasked after it is raised,
	 * do it ourself - unmask all signals */
	sigprocmask_allsigs(SIG_UNBLOCK);
	/* pendingsig = 0; - now done in onsig() */

	i = EXSIG;
	if (gotsig[SIGINT - 1] && !trap[SIGINT]) {
		if (!(rootshell && iflag)) {
			/* Kill ourself with SIGINT */
			signal(SIGINT, SIG_DFL);
			raise(SIGINT);
		}
		i = EXINT;
	}
	raise_exception(i);
	/* NOTREACHED */
}

#if ENABLE_ASH_OPTIMIZE_FOR_SIZE
static void
int_on(void)
{
	if (--suppressint == 0 && intpending) {
		raise_interrupt();
	}
}
#define INT_ON int_on()
static void
force_int_on(void)
{
	suppressint = 0;
	if (intpending)
		raise_interrupt();
}
#define FORCE_INT_ON force_int_on()
#else
#define INT_ON \
	do { \
		xbarrier(); \
		if (--suppressint == 0 && intpending) \
			raise_interrupt(); \
	} while (0)
#define FORCE_INT_ON \
	do { \
		xbarrier(); \
		suppressint = 0; \
		if (intpending) \
			raise_interrupt(); \
	} while (0)
#endif /* ASH_OPTIMIZE_FOR_SIZE */

#define SAVE_INT(v) ((v) = suppressint)

#define RESTORE_INT(v) \
	do { \
		xbarrier(); \
		suppressint = (v); \
		if (suppressint == 0 && intpending) \
			raise_interrupt(); \
	} while (0)

/*
 * Ignore a signal. Only one usage site - in forkchild()
 */
static void
ignoresig(int signo)
{
	if (sigmode[signo - 1] != S_IGN && sigmode[signo - 1] != S_HARD_IGN) {
		signal(signo, SIG_IGN);
	}
	sigmode[signo - 1] = S_HARD_IGN;
}

/*
 * Signal handler. Only one usage site - in setsignal()
 */
static void
onsig(int signo)
{
	gotsig[signo - 1] = 1;
	pendingsig = signo;

	if (/* exsig || */ (signo == SIGINT && !trap[SIGINT])) {
		if (!suppressint) {
			pendingsig = 0;
			raise_interrupt(); /* does not return */
		}
		intpending = 1;
	}
}


/* ============ Stdout/stderr output */

static void
outstr(const char *p, FILE *file)
{
	INT_OFF;
	fputs(p, file);
	INT_ON;
}

static void
flush_stdout_stderr(void)
{
	INT_OFF;
	fflush(stdout);
	fflush(stderr);
	INT_ON;
}

static void
flush_stderr(void)
{
	INT_OFF;
	fflush(stderr);
	INT_ON;
}

static void
outcslow(int c, FILE *dest)
{
	INT_OFF;
	putc(c, dest);
	fflush(dest);
	INT_ON;
}

static int out1fmt(const char *, ...) __attribute__((__format__(__printf__,1,2)));
static int
out1fmt(const char *fmt, ...)
{
	va_list ap;
	int r;

	INT_OFF;
	va_start(ap, fmt);
	r = vprintf(fmt, ap);
	va_end(ap);
	INT_ON;
	return r;
}

static int fmtstr(char *, size_t, const char *, ...) __attribute__((__format__(__printf__,3,4)));
static int
fmtstr(char *outbuf, size_t length, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	INT_OFF;
	ret = vsnprintf(outbuf, length, fmt, ap);
	va_end(ap);
	INT_ON;
	return ret;
}

static void
out1str(const char *p)
{
	outstr(p, stdout);
}

static void
out2str(const char *p)
{
	outstr(p, stderr);
	flush_stderr();
}


/* ============ Parser structures */

/* control characters in argument strings */
#define CTLESC '\201'           /* escape next character */
#define CTLVAR '\202'           /* variable defn */
#define CTLENDVAR '\203'
#define CTLBACKQ '\204'
#define CTLQUOTE 01             /* ored with CTLBACKQ code if in quotes */
/*      CTLBACKQ | CTLQUOTE == '\205' */
#define CTLARI  '\206'          /* arithmetic expression */
#define CTLENDARI '\207'
#define CTLQUOTEMARK '\210'

/* variable substitution byte (follows CTLVAR) */
#define VSTYPE  0x0f            /* type of variable substitution */
#define VSNUL   0x10            /* colon--treat the empty string as unset */
#define VSQUOTE 0x80            /* inside double quotes--suppress splitting */

/* values of VSTYPE field */
#define VSNORMAL        0x1     /* normal variable:  $var or ${var} */
#define VSMINUS         0x2     /* ${var-text} */
#define VSPLUS          0x3     /* ${var+text} */
#define VSQUESTION      0x4     /* ${var?message} */
#define VSASSIGN        0x5     /* ${var=text} */
#define VSTRIMRIGHT     0x6     /* ${var%pattern} */
#define VSTRIMRIGHTMAX  0x7     /* ${var%%pattern} */
#define VSTRIMLEFT      0x8     /* ${var#pattern} */
#define VSTRIMLEFTMAX   0x9     /* ${var##pattern} */
#define VSLENGTH        0xa     /* ${#var} */
#if ENABLE_ASH_BASH_COMPAT
#define VSSUBSTR        0xc     /* ${var:position:length} */
#define VSREPLACE       0xd     /* ${var/pattern/replacement} */
#define VSREPLACEALL    0xe     /* ${var//pattern/replacement} */
#endif

static const char dolatstr[] ALIGN1 = {
	CTLVAR, VSNORMAL|VSQUOTE, '@', '=', '\0'
};

#define NCMD 0
#define NPIPE 1
#define NREDIR 2
#define NBACKGND 3
#define NSUBSHELL 4
#define NAND 5
#define NOR 6
#define NSEMI 7
#define NIF 8
#define NWHILE 9
#define NUNTIL 10
#define NFOR 11
#define NCASE 12
#define NCLIST 13
#define NDEFUN 14
#define NARG 15
#define NTO 16
#define NCLOBBER 17
#define NFROM 18
#define NFROMTO 19
#define NAPPEND 20
#define NTOFD 21
#define NFROMFD 22
#define NHERE 23
#define NXHERE 24
#define NNOT 25

union node;

struct ncmd {
	int type;
	union node *assign;
	union node *args;
	union node *redirect;
};

struct npipe {
	int type;
	int backgnd;
	struct nodelist *cmdlist;
};

struct nredir {
	int type;
	union node *n;
	union node *redirect;
};

struct nbinary {
	int type;
	union node *ch1;
	union node *ch2;
};

struct nif {
	int type;
	union node *test;
	union node *ifpart;
	union node *elsepart;
};

struct nfor {
	int type;
	union node *args;
	union node *body;
	char *var;
};

struct ncase {
	int type;
	union node *expr;
	union node *cases;
};

struct nclist {
	int type;
	union node *next;
	union node *pattern;
	union node *body;
};

struct narg {
	int type;
	union node *next;
	char *text;
	struct nodelist *backquote;
};

struct nfile {
	int type;
	union node *next;
	int fd;
	union node *fname;
	char *expfname;
};

struct ndup {
	int type;
	union node *next;
	int fd;
	int dupfd;
	union node *vname;
};

struct nhere {
	int type;
	union node *next;
	int fd;
	union node *doc;
};

struct nnot {
	int type;
	union node *com;
};

union node {
	int type;
	struct ncmd ncmd;
	struct npipe npipe;
	struct nredir nredir;
	struct nbinary nbinary;
	struct nif nif;
	struct nfor nfor;
	struct ncase ncase;
	struct nclist nclist;
	struct narg narg;
	struct nfile nfile;
	struct ndup ndup;
	struct nhere nhere;
	struct nnot nnot;
};

struct nodelist {
	struct nodelist *next;
	union node *n;
};

struct funcnode {
	int count;
	union node n;
};

/*
 * Free a parse tree.
 */
static void
freefunc(struct funcnode *f)
{
	if (f && --f->count < 0)
		free(f);
}


/* ============ Debugging output */

#if DEBUG

static FILE *tracefile;

static void
trace_printf(const char *fmt, ...)
{
	va_list va;

	if (debug != 1)
		return;
	va_start(va, fmt);
	vfprintf(tracefile, fmt, va);
	va_end(va);
}

static void
trace_vprintf(const char *fmt, va_list va)
{
	if (debug != 1)
		return;
	vfprintf(tracefile, fmt, va);
}

static void
trace_puts(const char *s)
{
	if (debug != 1)
		return;
	fputs(s, tracefile);
}

static void
trace_puts_quoted(char *s)
{
	char *p;
	char c;

	if (debug != 1)
		return;
	putc('"', tracefile);
	for (p = s; *p; p++) {
		switch (*p) {
		case '\n':  c = 'n';  goto backslash;
		case '\t':  c = 't';  goto backslash;
		case '\r':  c = 'r';  goto backslash;
		case '"':  c = '"';  goto backslash;
		case '\\':  c = '\\';  goto backslash;
		case CTLESC:  c = 'e';  goto backslash;
		case CTLVAR:  c = 'v';  goto backslash;
		case CTLVAR+CTLQUOTE:  c = 'V'; goto backslash;
		case CTLBACKQ:  c = 'q';  goto backslash;
		case CTLBACKQ+CTLQUOTE:  c = 'Q'; goto backslash;
 backslash:
			putc('\\', tracefile);
			putc(c, tracefile);
			break;
		default:
			if (*p >= ' ' && *p <= '~')
				putc(*p, tracefile);
			else {
				putc('\\', tracefile);
				putc(*p >> 6 & 03, tracefile);
				putc(*p >> 3 & 07, tracefile);
				putc(*p & 07, tracefile);
			}
			break;
		}
	}
	putc('"', tracefile);
}

static void
trace_puts_args(char **ap)
{
	if (debug != 1)
		return;
	if (!*ap)
		return;
	while (1) {
		trace_puts_quoted(*ap);
		if (!*++ap) {
			putc('\n', tracefile);
			break;
		}
		putc(' ', tracefile);
	}
}

static void
opentrace(void)
{
	char s[100];
#ifdef O_APPEND
	int flags;
#endif

	if (debug != 1) {
		if (tracefile)
			fflush(tracefile);
		/* leave open because libedit might be using it */
		return;
	}
	strcpy(s, "./trace");
	if (tracefile) {
		if (!freopen(s, "a", tracefile)) {
			fprintf(stderr, "Can't re-open %s\n", s);
			debug = 0;
			return;
		}
	} else {
		tracefile = fopen(s, "a");
		if (tracefile == NULL) {
			fprintf(stderr, "Can't open %s\n", s);
			debug = 0;
			return;
		}
	}
#ifdef O_APPEND
	flags = fcntl(fileno(tracefile), F_GETFL);
	if (flags >= 0)
		fcntl(fileno(tracefile), F_SETFL, flags | O_APPEND);
#endif
	setlinebuf(tracefile);
	fputs("\nTracing started.\n", tracefile);
}

static void
indent(int amount, char *pfx, FILE *fp)
{
	int i;

	for (i = 0; i < amount; i++) {
		if (pfx && i == amount - 1)
			fputs(pfx, fp);
		putc('\t', fp);
	}
}

/* little circular references here... */
static void shtree(union node *n, int ind, char *pfx, FILE *fp);

static void
sharg(union node *arg, FILE *fp)
{
	char *p;
	struct nodelist *bqlist;
	int subtype;

	if (arg->type != NARG) {
		out1fmt("<node type %d>\n", arg->type);
		abort();
	}
	bqlist = arg->narg.backquote;
	for (p = arg->narg.text; *p; p++) {
		switch (*p) {
		case CTLESC:
			putc(*++p, fp);
			break;
		case CTLVAR:
			putc('$', fp);
			putc('{', fp);
			subtype = *++p;
			if (subtype == VSLENGTH)
				putc('#', fp);

			while (*p != '=')
				putc(*p++, fp);

			if (subtype & VSNUL)
				putc(':', fp);

			switch (subtype & VSTYPE) {
			case VSNORMAL:
				putc('}', fp);
				break;
			case VSMINUS:
				putc('-', fp);
				break;
			case VSPLUS:
				putc('+', fp);
				break;
			case VSQUESTION:
				putc('?', fp);
				break;
			case VSASSIGN:
				putc('=', fp);
				break;
			case VSTRIMLEFT:
				putc('#', fp);
				break;
			case VSTRIMLEFTMAX:
				putc('#', fp);
				putc('#', fp);
				break;
			case VSTRIMRIGHT:
				putc('%', fp);
				break;
			case VSTRIMRIGHTMAX:
				putc('%', fp);
				putc('%', fp);
				break;
			case VSLENGTH:
				break;
			default:
				out1fmt("<subtype %d>", subtype);
			}
			break;
		case CTLENDVAR:
			putc('}', fp);
			break;
		case CTLBACKQ:
		case CTLBACKQ|CTLQUOTE:
			putc('$', fp);
			putc('(', fp);
			shtree(bqlist->n, -1, NULL, fp);
			putc(')', fp);
			break;
		default:
			putc(*p, fp);
			break;
		}
	}
}

static void
shcmd(union node *cmd, FILE *fp)
{
	union node *np;
	int first;
	const char *s;
	int dftfd;

	first = 1;
	for (np = cmd->ncmd.args; np; np = np->narg.next) {
		if (!first)
			putc(' ', fp);
		sharg(np, fp);
		first = 0;
	}
	for (np = cmd->ncmd.redirect; np; np = np->nfile.next) {
		if (!first)
			putc(' ', fp);
		dftfd = 0;
		switch (np->nfile.type) {
		case NTO:      s = ">>"+1; dftfd = 1; break;
		case NCLOBBER: s = ">|"; dftfd = 1; break;
		case NAPPEND:  s = ">>"; dftfd = 1; break;
		case NTOFD:    s = ">&"; dftfd = 1; break;
		case NFROM:    s = "<";  break;
		case NFROMFD:  s = "<&"; break;
		case NFROMTO:  s = "<>"; break;
		default:       s = "*error*"; break;
		}
		if (np->nfile.fd != dftfd)
			fprintf(fp, "%d", np->nfile.fd);
		fputs(s, fp);
		if (np->nfile.type == NTOFD || np->nfile.type == NFROMFD) {
			fprintf(fp, "%d", np->ndup.dupfd);
		} else {
			sharg(np->nfile.fname, fp);
		}
		first = 0;
	}
}

static void
shtree(union node *n, int ind, char *pfx, FILE *fp)
{
	struct nodelist *lp;
	const char *s;

	if (n == NULL)
		return;

	indent(ind, pfx, fp);
	switch (n->type) {
	case NSEMI:
		s = "; ";
		goto binop;
	case NAND:
		s = " && ";
		goto binop;
	case NOR:
		s = " || ";
 binop:
		shtree(n->nbinary.ch1, ind, NULL, fp);
		/* if (ind < 0) */
			fputs(s, fp);
		shtree(n->nbinary.ch2, ind, NULL, fp);
		break;
	case NCMD:
		shcmd(n, fp);
		if (ind >= 0)
			putc('\n', fp);
		break;
	case NPIPE:
		for (lp = n->npipe.cmdlist; lp; lp = lp->next) {
			shcmd(lp->n, fp);
			if (lp->next)
				fputs(" | ", fp);
		}
		if (n->npipe.backgnd)
			fputs(" &", fp);
		if (ind >= 0)
			putc('\n', fp);
		break;
	default:
		fprintf(fp, "<node type %d>", n->type);
		if (ind >= 0)
			putc('\n', fp);
		break;
	}
}

static void
showtree(union node *n)
{
	trace_puts("showtree called\n");
	shtree(n, 1, NULL, stdout);
}

#define TRACE(param)    trace_printf param
#define TRACEV(param)   trace_vprintf param

#else

#define TRACE(param)
#define TRACEV(param)

#endif /* DEBUG */


/* ============ Parser data */

/*
 * ash_vmsg() needs parsefile->fd, hence parsefile definition is moved up.
 */
struct strlist {
	struct strlist *next;
	char *text;
};

#if ENABLE_ASH_ALIAS
struct alias;
#endif

struct strpush {
	struct strpush *prev;   /* preceding string on stack */
	char *prevstring;
	int prevnleft;
#if ENABLE_ASH_ALIAS
	struct alias *ap;       /* if push was associated with an alias */
#endif
	char *string;           /* remember the string since it may change */
};

struct parsefile {
	struct parsefile *prev; /* preceding file on stack */
	int linno;              /* current line */
	int fd;                 /* file descriptor (or -1 if string) */
	int nleft;              /* number of chars left in this line */
	int lleft;              /* number of chars left in this buffer */
	char *nextc;            /* next char in buffer */
	char *buf;              /* input buffer */
	struct strpush *strpush; /* for pushing strings at this level */
	struct strpush basestrpush; /* so pushing one is fast */
};

static struct parsefile basepf;         /* top level input file */
static struct parsefile *g_parsefile = &basepf;  /* current input file */
static int startlinno;                 /* line # where last token started */
static char *commandname;              /* currently executing command */
static struct strlist *cmdenviron;     /* environment for builtin command */
static int exitstatus;                 /* exit status of last command */


/* ============ Message printing */

static void
ash_vmsg(const char *msg, va_list ap)
{
	fprintf(stderr, "%s: ", arg0);
	if (commandname) {
		if (strcmp(arg0, commandname))
			fprintf(stderr, "%s: ", commandname);
		if (!iflag || g_parsefile->fd)
			fprintf(stderr, "line %d: ", startlinno);
	}
	vfprintf(stderr, msg, ap);
	outcslow('\n', stderr);
}

/*
 * Exverror is called to raise the error exception.  If the second argument
 * is not NULL then error prints an error message using printf style
 * formatting.  It then raises the error exception.
 */
static void ash_vmsg_and_raise(int, const char *, va_list) ATTRIBUTE_NORETURN;
static void
ash_vmsg_and_raise(int cond, const char *msg, va_list ap)
{
#if DEBUG
	if (msg) {
		TRACE(("ash_vmsg_and_raise(%d, \"", cond));
		TRACEV((msg, ap));
		TRACE(("\") pid=%d\n", getpid()));
	} else
		TRACE(("ash_vmsg_and_raise(%d, NULL) pid=%d\n", cond, getpid()));
	if (msg)
#endif
		ash_vmsg(msg, ap);

	flush_stdout_stderr();
	raise_exception(cond);
	/* NOTREACHED */
}

static void ash_msg_and_raise_error(const char *, ...) ATTRIBUTE_NORETURN;
static void
ash_msg_and_raise_error(const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	ash_vmsg_and_raise(EXERROR, msg, ap);
	/* NOTREACHED */
	va_end(ap);
}

static void ash_msg_and_raise(int, const char *, ...) ATTRIBUTE_NORETURN;
static void
ash_msg_and_raise(int cond, const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	ash_vmsg_and_raise(cond, msg, ap);
	/* NOTREACHED */
	va_end(ap);
}

/*
 * error/warning routines for external builtins
 */
static void
ash_msg(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	ash_vmsg(fmt, ap);
	va_end(ap);
}

/*
 * Return a string describing an error.  The returned string may be a
 * pointer to a static buffer that will be overwritten on the next call.
 * Action describes the operation that got the error.
 */
static const char *
errmsg(int e, const char *em)
{
	if (e == ENOENT || e == ENOTDIR) {
		return em;
	}
	return strerror(e);
}


/* ============ Memory allocation */

/*
 * It appears that grabstackstr() will barf with such alignments
 * because stalloc() will return a string allocated in a new stackblock.
 */
#define SHELL_ALIGN(nbytes) (((nbytes) + SHELL_SIZE) & ~SHELL_SIZE)
enum {
	/* Most machines require the value returned from malloc to be aligned
	 * in some way.  The following macro will get this right
	 * on many machines.  */
	SHELL_SIZE = sizeof(union {int i; char *cp; double d; }) - 1,
	/* Minimum size of a block */
	MINSIZE = SHELL_ALIGN(504),
};

struct stack_block {
	struct stack_block *prev;
	char space[MINSIZE];
};

struct stackmark {
	struct stack_block *stackp;
	char *stacknxt;
	size_t stacknleft;
	struct stackmark *marknext;
};


struct globals_memstack {
	struct stack_block *g_stackp; // = &stackbase;
	struct stackmark *markp;
	char *g_stacknxt; // = stackbase.space;
	char *sstrend; // = stackbase.space + MINSIZE;
	size_t g_stacknleft; // = MINSIZE;
	int    herefd; // = -1;
	struct stack_block stackbase;
};
extern struct globals_memstack *const ash_ptr_to_globals_memstack;
#define G_memstack (*ash_ptr_to_globals_memstack)
#define g_stackp     (G_memstack.g_stackp    )
#define markp        (G_memstack.markp       )
#define g_stacknxt   (G_memstack.g_stacknxt  )
#define sstrend      (G_memstack.sstrend     )
#define g_stacknleft (G_memstack.g_stacknleft)
#define herefd       (G_memstack.herefd      )
#define stackbase    (G_memstack.stackbase   )
#define INIT_G_memstack() do { \
	(*(struct globals_memstack**)&ash_ptr_to_globals_memstack) = xzalloc(sizeof(G_memstack)); \
	barrier(); \
	g_stackp = &stackbase; \
	g_stacknxt = stackbase.space; \
	g_stacknleft = MINSIZE; \
	sstrend = stackbase.space + MINSIZE; \
	herefd = -1; \
} while (0)

#define stackblock()     ((void *)g_stacknxt)
#define stackblocksize() g_stacknleft


static void *
ckrealloc(void * p, size_t nbytes)
{
	p = realloc(p, nbytes);
	if (!p)
		ash_msg_and_raise_error(bb_msg_memory_exhausted);
	return p;
}

static void *
ckmalloc(size_t nbytes)
{
	return ckrealloc(NULL, nbytes);
}

static void *
ckzalloc(size_t nbytes)
{
	return memset(ckmalloc(nbytes), 0, nbytes);
}

/*
 * Make a copy of a string in safe storage.
 */
static char *
ckstrdup(const char *s)
{
	char *p = strdup(s);
	if (!p)
		ash_msg_and_raise_error(bb_msg_memory_exhausted);
	return p;
}

/*
 * Parse trees for commands are allocated in lifo order, so we use a stack
 * to make this more efficient, and also to avoid all sorts of exception
 * handling code to handle interrupts in the middle of a parse.
 *
 * The size 504 was chosen because the Ultrix malloc handles that size
 * well.
 */
static void *
stalloc(size_t nbytes)
{
	char *p;
	size_t aligned;

	aligned = SHELL_ALIGN(nbytes);
	if (aligned > g_stacknleft) {
		size_t len;
		size_t blocksize;
		struct stack_block *sp;

		blocksize = aligned;
		if (blocksize < MINSIZE)
			blocksize = MINSIZE;
		len = sizeof(struct stack_block) - MINSIZE + blocksize;
		if (len < blocksize)
			ash_msg_and_raise_error(bb_msg_memory_exhausted);
		INT_OFF;
		sp = ckmalloc(len);
		sp->prev = g_stackp;
		g_stacknxt = sp->space;
		g_stacknleft = blocksize;
		sstrend = g_stacknxt + blocksize;
		g_stackp = sp;
		INT_ON;
	}
	p = g_stacknxt;
	g_stacknxt += aligned;
	g_stacknleft -= aligned;
	return p;
}

static void *
stzalloc(size_t nbytes)
{
	return memset(stalloc(nbytes), 0, nbytes);
}

static void
stunalloc(void *p)
{
#if DEBUG
	if (!p || (g_stacknxt < (char *)p) || ((char *)p < g_stackp->space)) {
		write(STDERR_FILENO, "stunalloc\n", 10);
		abort();
	}
#endif
	g_stacknleft += g_stacknxt - (char *)p;
	g_stacknxt = p;
}

/*
 * Like strdup but works with the ash stack.
 */
static char *
ststrdup(const char *p)
{
	size_t len = strlen(p) + 1;
	return memcpy(stalloc(len), p, len);
}

static void
setstackmark(struct stackmark *mark)
{
	mark->stackp = g_stackp;
	mark->stacknxt = g_stacknxt;
	mark->stacknleft = g_stacknleft;
	mark->marknext = markp;
	markp = mark;
}

static void
popstackmark(struct stackmark *mark)
{
	struct stack_block *sp;

	if (!mark->stackp)
		return;

	INT_OFF;
	markp = mark->marknext;
	while (g_stackp != mark->stackp) {
		sp = g_stackp;
		g_stackp = sp->prev;
		free(sp);
	}
	g_stacknxt = mark->stacknxt;
	g_stacknleft = mark->stacknleft;
	sstrend = mark->stacknxt + mark->stacknleft;
	INT_ON;
}

/*
 * When the parser reads in a string, it wants to stick the string on the
 * stack and only adjust the stack pointer when it knows how big the
 * string is.  Stackblock (defined in stack.h) returns a pointer to a block
 * of space on top of the stack and stackblocklen returns the length of
 * this block.  Growstackblock will grow this space by at least one byte,
 * possibly moving it (like realloc).  Grabstackblock actually allocates the
 * part of the block that has been used.
 */
static void
growstackblock(void)
{
	size_t newlen;

	newlen = g_stacknleft * 2;
	if (newlen < g_stacknleft)
		ash_msg_and_raise_error(bb_msg_memory_exhausted);
	if (newlen < 128)
		newlen += 128;

	if (g_stacknxt == g_stackp->space && g_stackp != &stackbase) {
		struct stack_block *oldstackp;
		struct stackmark *xmark;
		struct stack_block *sp;
		struct stack_block *prevstackp;
		size_t grosslen;

		INT_OFF;
		oldstackp = g_stackp;
		sp = g_stackp;
		prevstackp = sp->prev;
		grosslen = newlen + sizeof(struct stack_block) - MINSIZE;
		sp = ckrealloc(sp, grosslen);
		sp->prev = prevstackp;
		g_stackp = sp;
		g_stacknxt = sp->space;
		g_stacknleft = newlen;
		sstrend = sp->space + newlen;

		/*
		 * Stack marks pointing to the start of the old block
		 * must be relocated to point to the new block
		 */
		xmark = markp;
		while (xmark != NULL && xmark->stackp == oldstackp) {
			xmark->stackp = g_stackp;
			xmark->stacknxt = g_stacknxt;
			xmark->stacknleft = g_stacknleft;
			xmark = xmark->marknext;
		}
		INT_ON;
	} else {
		char *oldspace = g_stacknxt;
		size_t oldlen = g_stacknleft;
		char *p = stalloc(newlen);

		/* free the space we just allocated */
		g_stacknxt = memcpy(p, oldspace, oldlen);
		g_stacknleft += newlen;
	}
}

static void
grabstackblock(size_t len)
{
	len = SHELL_ALIGN(len);
	g_stacknxt += len;
	g_stacknleft -= len;
}

/*
 * The following routines are somewhat easier to use than the above.
 * The user declares a variable of type STACKSTR, which may be declared
 * to be a register.  The macro STARTSTACKSTR initializes things.  Then
 * the user uses the macro STPUTC to add characters to the string.  In
 * effect, STPUTC(c, p) is the same as *p++ = c except that the stack is
 * grown as necessary.  When the user is done, she can just leave the
 * string there and refer to it using stackblock().  Or she can allocate
 * the space for it using grabstackstr().  If it is necessary to allow
 * someone else to use the stack temporarily and then continue to grow
 * the string, the user should use grabstack to allocate the space, and
 * then call ungrabstr(p) to return to the previous mode of operation.
 *
 * USTPUTC is like STPUTC except that it doesn't check for overflow.
 * CHECKSTACKSPACE can be called before USTPUTC to ensure that there
 * is space for at least one character.
 */
static void *
growstackstr(void)
{
	size_t len = stackblocksize();
	if (herefd >= 0 && len >= 1024) {
		full_write(herefd, stackblock(), len);
		return stackblock();
	}
	growstackblock();
	return (char *)stackblock() + len;
}

/*
 * Called from CHECKSTRSPACE.
 */
static char *
makestrspace(size_t newlen, char *p)
{
	size_t len = p - g_stacknxt;
	size_t size = stackblocksize();

	for (;;) {
		size_t nleft;

		size = stackblocksize();
		nleft = size - len;
		if (nleft >= newlen)
			break;
		growstackblock();
	}
	return (char *)stackblock() + len;
}

static char *
stack_nputstr(const char *s, size_t n, char *p)
{
	p = makestrspace(n, p);
	p = (char *)memcpy(p, s, n) + n;
	return p;
}

static char *
stack_putstr(const char *s, char *p)
{
	return stack_nputstr(s, strlen(s), p);
}

static char *
_STPUTC(int c, char *p)
{
	if (p == sstrend)
		p = growstackstr();
	*p++ = c;
	return p;
}

#define STARTSTACKSTR(p)        ((p) = stackblock())
#define STPUTC(c, p)            ((p) = _STPUTC((c), (p)))
#define CHECKSTRSPACE(n, p) \
	do { \
		char *q = (p); \
		size_t l = (n); \
		size_t m = sstrend - q; \
		if (l > m) \
			(p) = makestrspace(l, q); \
	} while (0)
#define USTPUTC(c, p)           (*(p)++ = (c))
#define STACKSTRNUL(p) \
	do { \
		if ((p) == sstrend) \
			(p) = growstackstr(); \
		*(p) = '\0'; \
	} while (0)
#define STUNPUTC(p)             (--(p))
#define STTOPC(p)               ((p)[-1])
#define STADJUST(amount, p)     ((p) += (amount))

#define grabstackstr(p)         stalloc((char *)(p) - (char *)stackblock())
#define ungrabstackstr(s, p)    stunalloc(s)
#define stackstrend()           ((void *)sstrend)


/* ============ String helpers */

/*
 * prefix -- see if pfx is a prefix of string.
 */
static char *
prefix(const char *string, const char *pfx)
{
	while (*pfx) {
		if (*pfx++ != *string++)
			return NULL;
	}
	return (char *) string;
}

/*
 * Check for a valid number.  This should be elsewhere.
 */
static int
is_number(const char *p)
{
	do {
		if (!isdigit(*p))
			return 0;
	} while (*++p != '\0');
	return 1;
}

/*
 * Convert a string of digits to an integer, printing an error message on
 * failure.
 */
static int
number(const char *s)
{
	if (!is_number(s))
		ash_msg_and_raise_error(illnum, s);
	return atoi(s);
}

/*
 * Produce a possibly single quoted string suitable as input to the shell.
 * The return string is allocated on the stack.
 */
static char *
single_quote(const char *s)
{
	char *p;

	STARTSTACKSTR(p);

	do {
		char *q;
		size_t len;

		len = strchrnul(s, '\'') - s;

		q = p = makestrspace(len + 3, p);

		*q++ = '\'';
		q = (char *)memcpy(q, s, len) + len;
		*q++ = '\'';
		s += len;

		STADJUST(q - p, p);

		len = strspn(s, "'");
		if (!len)
			break;

		q = p = makestrspace(len + 3, p);

		*q++ = '"';
		q = (char *)memcpy(q, s, len) + len;
		*q++ = '"';
		s += len;

		STADJUST(q - p, p);
	} while (*s);

	USTPUTC(0, p);

	return stackblock();
}


/* ============ nextopt */

static char **argptr;                  /* argument list for builtin commands */
static char *optionarg;                /* set by nextopt (like getopt) */
static char *optptr;                   /* used by nextopt */

/*
 * XXX - should get rid of.  have all builtins use getopt(3).  the
 * library getopt must have the BSD extension static variable "optreset"
 * otherwise it can't be used within the shell safely.
 *
 * Standard option processing (a la getopt) for builtin routines.  The
 * only argument that is passed to nextopt is the option string; the
 * other arguments are unnecessary.  It return the character, or '\0' on
 * end of input.
 */
static int
nextopt(const char *optstring)
{
	char *p;
	const char *q;
	char c;

	p = optptr;
	if (p == NULL || *p == '\0') {
		p = *argptr;
		if (p == NULL || *p != '-' || *++p == '\0')
			return '\0';
		argptr++;
		if (LONE_DASH(p))        /* check for "--" */
			return '\0';
	}
	c = *p++;
	for (q = optstring; *q != c;) {
		if (*q == '\0')
			ash_msg_and_raise_error("illegal option -%c", c);
		if (*++q == ':')
			q++;
	}
	if (*++q == ':') {
		if (*p == '\0' && (p = *argptr++) == NULL)
			ash_msg_and_raise_error("no arg for -%c option", c);
		optionarg = p;
		p = NULL;
	}
	optptr = p;
	return c;
}


/* ============ Math support definitions */

#if ENABLE_ASH_MATH_SUPPORT_64
typedef int64_t arith_t;
#define arith_t_type long long
#else
typedef long arith_t;
#define arith_t_type long
#endif

#if ENABLE_ASH_MATH_SUPPORT
static arith_t dash_arith(const char *);
static arith_t arith(const char *expr, int *perrcode);
#endif

#if ENABLE_ASH_RANDOM_SUPPORT
static unsigned long rseed;
#ifndef DYNAMIC_VAR
#define DYNAMIC_VAR
#endif
#endif


/* ============ Shell variables */

/*
 * The parsefile structure pointed to by the global variable parsefile
 * contains information about the current file being read.
 */
struct redirtab {
	struct redirtab *next;
	int renamed[10];
	int nullredirs;
};

struct shparam {
	int nparam;             /* # of positional parameters (without $0) */
#if ENABLE_ASH_GETOPTS
	int optind;             /* next parameter to be processed by getopts */
	int optoff;             /* used by getopts */
#endif
	unsigned char malloced; /* if parameter list dynamically allocated */
	char **p;               /* parameter list */
};

/*
 * Free the list of positional parameters.
 */
static void
freeparam(volatile struct shparam *param)
{
	char **ap;

	if (param->malloced) {
		for (ap = param->p; *ap; ap++)
			free(*ap);
		free(param->p);
	}
}

#if ENABLE_ASH_GETOPTS
static void getoptsreset(const char *value);
#endif

struct var {
	struct var *next;               /* next entry in hash list */
	int flags;                      /* flags are defined above */
	const char *text;               /* name=value */
	void (*func)(const char *);     /* function to be called when  */
					/* the variable gets set/unset */
};

struct localvar {
	struct localvar *next;          /* next local variable in list */
	struct var *vp;                 /* the variable that was made local */
	int flags;                      /* saved flags */
	const char *text;               /* saved text */
};

/* flags */
#define VEXPORT         0x01    /* variable is exported */
#define VREADONLY       0x02    /* variable cannot be modified */
#define VSTRFIXED       0x04    /* variable struct is statically allocated */
#define VTEXTFIXED      0x08    /* text is statically allocated */
#define VSTACK          0x10    /* text is allocated on the stack */
#define VUNSET          0x20    /* the variable is not set */
#define VNOFUNC         0x40    /* don't call the callback function */
#define VNOSET          0x80    /* do not set variable - just readonly test */
#define VNOSAVE         0x100   /* when text is on the heap before setvareq */
#ifdef DYNAMIC_VAR
# define VDYNAMIC       0x200   /* dynamic variable */
#else
# define VDYNAMIC       0
#endif

#ifdef IFS_BROKEN
static const char defifsvar[] ALIGN1 = "IFS= \t\n";
#define defifs (defifsvar + 4)
#else
static const char defifs[] ALIGN1 = " \t\n";
#endif


/* Need to be before varinit_data[] */
#if ENABLE_LOCALE_SUPPORT
static void
change_lc_all(const char *value)
{
	if (value && *value != '\0')
		setlocale(LC_ALL, value);
}
static void
change_lc_ctype(const char *value)
{
	if (value && *value != '\0')
		setlocale(LC_CTYPE, value);
}
#endif
#if ENABLE_ASH_MAIL
static void chkmail(void);
static void changemail(const char *);
#endif
static void changepath(const char *);
#if ENABLE_ASH_RANDOM_SUPPORT
static void change_random(const char *);
#endif

static const struct {
	int flags;
	const char *text;
	void (*func)(const char *);
} varinit_data[] = {
#ifdef IFS_BROKEN
	{ VSTRFIXED|VTEXTFIXED       , defifsvar   , NULL            },
#else
	{ VSTRFIXED|VTEXTFIXED|VUNSET, "IFS\0"     , NULL            },
#endif
#if ENABLE_ASH_MAIL
	{ VSTRFIXED|VTEXTFIXED|VUNSET, "MAIL\0"    , changemail      },
	{ VSTRFIXED|VTEXTFIXED|VUNSET, "MAILPATH\0", changemail      },
#endif
	{ VSTRFIXED|VTEXTFIXED       , bb_PATH_root_path, changepath },
	{ VSTRFIXED|VTEXTFIXED       , "PS1=$ "    , NULL            },
	{ VSTRFIXED|VTEXTFIXED       , "PS2=> "    , NULL            },
	{ VSTRFIXED|VTEXTFIXED       , "PS4=+ "    , NULL            },
#if ENABLE_ASH_GETOPTS
	{ VSTRFIXED|VTEXTFIXED       , "OPTIND=1"  , getoptsreset    },
#endif
#if ENABLE_ASH_RANDOM_SUPPORT
	{ VSTRFIXED|VTEXTFIXED|VUNSET|VDYNAMIC, "RANDOM\0", change_random },
#endif
#if ENABLE_LOCALE_SUPPORT
	{ VSTRFIXED|VTEXTFIXED|VUNSET, "LC_ALL\0"  , change_lc_all   },
	{ VSTRFIXED|VTEXTFIXED|VUNSET, "LC_CTYPE\0", change_lc_ctype },
#endif
#if ENABLE_FEATURE_EDITING_SAVEHISTORY
	{ VSTRFIXED|VTEXTFIXED|VUNSET, "HISTFILE\0", NULL            },
#endif
};


struct globals_var {
	struct shparam shellparam;      /* $@ current positional parameters */
	struct redirtab *redirlist;
	int g_nullredirs;
	int preverrout_fd;   /* save fd2 before print debug if xflag is set. */
	struct var *vartab[VTABSIZE];
	struct var varinit[ARRAY_SIZE(varinit_data)];
};
extern struct globals_var *const ash_ptr_to_globals_var;
#define G_var (*ash_ptr_to_globals_var)
#define shellparam    (G_var.shellparam   )
#define redirlist     (G_var.redirlist    )
#define g_nullredirs  (G_var.g_nullredirs )
#define preverrout_fd (G_var.preverrout_fd)
#define vartab        (G_var.vartab       )
#define varinit       (G_var.varinit      )
#define INIT_G_var() do { \
	unsigned i; \
	(*(struct globals_var**)&ash_ptr_to_globals_var) = xzalloc(sizeof(G_var)); \
	barrier(); \
	for (i = 0; i < ARRAY_SIZE(varinit_data); i++) { \
		varinit[i].flags = varinit_data[i].flags; \
		varinit[i].text  = varinit_data[i].text; \
		varinit[i].func  = varinit_data[i].func; \
	} \
} while (0)

#define vifs      varinit[0]
#if ENABLE_ASH_MAIL
# define vmail    (&vifs)[1]
# define vmpath   (&vmail)[1]
# define vpath    (&vmpath)[1]
#else
# define vpath    (&vifs)[1]
#endif
#define vps1      (&vpath)[1]
#define vps2      (&vps1)[1]
#define vps4      (&vps2)[1]
#if ENABLE_ASH_GETOPTS
# define voptind  (&vps4)[1]
# if ENABLE_ASH_RANDOM_SUPPORT
#  define vrandom (&voptind)[1]
# endif
#else
# if ENABLE_ASH_RANDOM_SUPPORT
#  define vrandom (&vps4)[1]
# endif
#endif

/*
 * The following macros access the values of the above variables.
 * They have to skip over the name.  They return the null string
 * for unset variables.
 */
#define ifsval()        (vifs.text + 4)
#define ifsset()        ((vifs.flags & VUNSET) == 0)
#if ENABLE_ASH_MAIL
# define mailval()      (vmail.text + 5)
# define mpathval()     (vmpath.text + 9)
# define mpathset()     ((vmpath.flags & VUNSET) == 0)
#endif
#define pathval()       (vpath.text + 5)
#define ps1val()        (vps1.text + 4)
#define ps2val()        (vps2.text + 4)
#define ps4val()        (vps4.text + 4)
#if ENABLE_ASH_GETOPTS
# define optindval()    (voptind.text + 7)
#endif


#define is_name(c)      ((c) == '_' || isalpha((unsigned char)(c)))
#define is_in_name(c)   ((c) == '_' || isalnum((unsigned char)(c)))

#if ENABLE_ASH_GETOPTS
static void
getoptsreset(const char *value)
{
	shellparam.optind = number(value);
	shellparam.optoff = -1;
}
#endif

/*
 * Return of a legal variable name (a letter or underscore followed by zero or
 * more letters, underscores, and digits).
 */
static char *
endofname(const char *name)
{
	char *p;

	p = (char *) name;
	if (!is_name(*p))
		return p;
	while (*++p) {
		if (!is_in_name(*p))
			break;
	}
	return p;
}

/*
 * Compares two strings up to the first = or '\0'.  The first
 * string must be terminated by '='; the second may be terminated by
 * either '=' or '\0'.
 */
static int
varcmp(const char *p, const char *q)
{
	int c, d;

	while ((c = *p) == (d = *q)) {
		if (!c || c == '=')
			goto out;
		p++;
		q++;
	}
	if (c == '=')
		c = '\0';
	if (d == '=')
		d = '\0';
 out:
	return c - d;
}

static int
varequal(const char *a, const char *b)
{
	return !varcmp(a, b);
}

/*
 * Find the appropriate entry in the hash table from the name.
 */
static struct var **
hashvar(const char *p)
{
	unsigned hashval;

	hashval = ((unsigned char) *p) << 4;
	while (*p && *p != '=')
		hashval += (unsigned char) *p++;
	return &vartab[hashval % VTABSIZE];
}

static int
vpcmp(const void *a, const void *b)
{
	return varcmp(*(const char **)a, *(const char **)b);
}

/*
 * This routine initializes the builtin variables.
 */
static void
initvar(void)
{
	struct var *vp;
	struct var *end;
	struct var **vpp;

	/*
	 * PS1 depends on uid
	 */
#if ENABLE_FEATURE_EDITING && ENABLE_FEATURE_EDITING_FANCY_PROMPT
	vps1.text = "PS1=\\w \\$ ";
#else
	if (!geteuid())
		vps1.text = "PS1=# ";
#endif
	vp = varinit;
	end = vp + ARRAY_SIZE(varinit);
	do {
		vpp = hashvar(vp->text);
		vp->next = *vpp;
		*vpp = vp;
	} while (++vp < end);
}

static struct var **
findvar(struct var **vpp, const char *name)
{
	for (; *vpp; vpp = &(*vpp)->next) {
		if (varequal((*vpp)->text, name)) {
			break;
		}
	}
	return vpp;
}

/*
 * Find the value of a variable.  Returns NULL if not set.
 */
static char *
lookupvar(const char *name)
{
	struct var *v;

	v = *findvar(hashvar(name), name);
	if (v) {
#ifdef DYNAMIC_VAR
	/*
	 * Dynamic variables are implemented roughly the same way they are
	 * in bash. Namely, they're "special" so long as they aren't unset.
	 * As soon as they're unset, they're no longer dynamic, and dynamic
	 * lookup will no longer happen at that point. -- PFM.
	 */
		if ((v->flags & VDYNAMIC))
			(*v->func)(NULL);
#endif
		if (!(v->flags & VUNSET))
			return strchrnul(v->text, '=') + 1;
	}
	return NULL;
}

/*
 * Search the environment of a builtin command.
 */
static char *
bltinlookup(const char *name)
{
	struct strlist *sp;

	for (sp = cmdenviron; sp; sp = sp->next) {
		if (varequal(sp->text, name))
			return strchrnul(sp->text, '=') + 1;
	}
	return lookupvar(name);
}

/*
 * Same as setvar except that the variable and value are passed in
 * the first argument as name=value.  Since the first argument will
 * be actually stored in the table, it should not be a string that
 * will go away.
 * Called with interrupts off.
 */
static void
setvareq(char *s, int flags)
{
	struct var *vp, **vpp;

	vpp = hashvar(s);
	flags |= (VEXPORT & (((unsigned) (1 - aflag)) - 1));
	vp = *findvar(vpp, s);
	if (vp) {
		if ((vp->flags & (VREADONLY|VDYNAMIC)) == VREADONLY) {
			const char *n;

			if (flags & VNOSAVE)
				free(s);
			n = vp->text;
			ash_msg_and_raise_error("%.*s: is read only", strchrnul(n, '=') - n, n);
		}

		if (flags & VNOSET)
			return;

		if (vp->func && (flags & VNOFUNC) == 0)
			(*vp->func)(strchrnul(s, '=') + 1);

		if ((vp->flags & (VTEXTFIXED|VSTACK)) == 0)
			free((char*)vp->text);

		flags |= vp->flags & ~(VTEXTFIXED|VSTACK|VNOSAVE|VUNSET);
	} else {
		if (flags & VNOSET)
			return;
		/* not found */
		vp = ckzalloc(sizeof(*vp));
		vp->next = *vpp;
		/*vp->func = NULL; - ckzalloc did it */
		*vpp = vp;
	}
	if (!(flags & (VTEXTFIXED|VSTACK|VNOSAVE)))
		s = ckstrdup(s);
	vp->text = s;
	vp->flags = flags;
}

/*
 * Set the value of a variable.  The flags argument is ored with the
 * flags of the variable.  If val is NULL, the variable is unset.
 */
static void
setvar(const char *name, const char *val, int flags)
{
	char *p, *q;
	size_t namelen;
	char *nameeq;
	size_t vallen;

	q = endofname(name);
	p = strchrnul(q, '=');
	namelen = p - name;
	if (!namelen || p != q)
		ash_msg_and_raise_error("%.*s: bad variable name", namelen, name);
	vallen = 0;
	if (val == NULL) {
		flags |= VUNSET;
	} else {
		vallen = strlen(val);
	}
	INT_OFF;
	nameeq = ckmalloc(namelen + vallen + 2);
	p = (char *)memcpy(nameeq, name, namelen) + namelen;
	if (val) {
		*p++ = '=';
		p = (char *)memcpy(p, val, vallen) + vallen;
	}
	*p = '\0';
	setvareq(nameeq, flags | VNOSAVE);
	INT_ON;
}

#if ENABLE_ASH_GETOPTS
/*
 * Safe version of setvar, returns 1 on success 0 on failure.
 */
static int
setvarsafe(const char *name, const char *val, int flags)
{
	int err;
	volatile int saveint;
	struct jmploc *volatile savehandler = exception_handler;
	struct jmploc jmploc;

	SAVE_INT(saveint);
	if (setjmp(jmploc.loc))
		err = 1;
	else {
		exception_handler = &jmploc;
		setvar(name, val, flags);
		err = 0;
	}
	exception_handler = savehandler;
	RESTORE_INT(saveint);
	return err;
}
#endif

/*
 * Unset the specified variable.
 */
static int
unsetvar(const char *s)
{
	struct var **vpp;
	struct var *vp;
	int retval;

	vpp = findvar(hashvar(s), s);
	vp = *vpp;
	retval = 2;
	if (vp) {
		int flags = vp->flags;

		retval = 1;
		if (flags & VREADONLY)
			goto out;
#ifdef DYNAMIC_VAR
		vp->flags &= ~VDYNAMIC;
#endif
		if (flags & VUNSET)
			goto ok;
		if ((flags & VSTRFIXED) == 0) {
			INT_OFF;
			if ((flags & (VTEXTFIXED|VSTACK)) == 0)
				free((char*)vp->text);
			*vpp = vp->next;
			free(vp);
			INT_ON;
		} else {
			setvar(s, 0, 0);
			vp->flags &= ~VEXPORT;
		}
 ok:
		retval = 0;
	}
 out:
	return retval;
}

/*
 * Process a linked list of variable assignments.
 */
static void
listsetvar(struct strlist *list_set_var, int flags)
{
	struct strlist *lp = list_set_var;

	if (!lp)
		return;
	INT_OFF;
	do {
		setvareq(lp->text, flags);
		lp = lp->next;
	} while (lp);
	INT_ON;
}

/*
 * Generate a list of variables satisfying the given conditions.
 */
static char **
listvars(int on, int off, char ***end)
{
	struct var **vpp;
	struct var *vp;
	char **ep;
	int mask;

	STARTSTACKSTR(ep);
	vpp = vartab;
	mask = on | off;
	do {
		for (vp = *vpp; vp; vp = vp->next) {
			if ((vp->flags & mask) == on) {
				if (ep == stackstrend())
					ep = growstackstr();
				*ep++ = (char *) vp->text;
			}
		}
	} while (++vpp < vartab + VTABSIZE);
	if (ep == stackstrend())
		ep = growstackstr();
	if (end)
		*end = ep;
	*ep++ = NULL;
	return grabstackstr(ep);
}


/* ============ Path search helper
 *
 * The variable path (passed by reference) should be set to the start
 * of the path before the first call; padvance will update
 * this value as it proceeds.  Successive calls to padvance will return
 * the possible path expansions in sequence.  If an option (indicated by
 * a percent sign) appears in the path entry then the global variable
 * pathopt will be set to point to it; otherwise pathopt will be set to
 * NULL.
 */
static const char *pathopt;     /* set by padvance */

static char *
padvance(const char **path, const char *name)
{
	const char *p;
	char *q;
	const char *start;
	size_t len;

	if (*path == NULL)
		return NULL;
	start = *path;
	for (p = start; *p && *p != ':' && *p != '%'; p++)
		continue;
	len = p - start + strlen(name) + 2;     /* "2" is for '/' and '\0' */
	while (stackblocksize() < len)
		growstackblock();
	q = stackblock();
	if (p != start) {
		memcpy(q, start, p - start);
		q += p - start;
		*q++ = '/';
	}
	strcpy(q, name);
	pathopt = NULL;
	if (*p == '%') {
		pathopt = ++p;
		while (*p && *p != ':')
			p++;
	}
	if (*p == ':')
		*path = p + 1;
	else
		*path = NULL;
	return stalloc(len);
}


/* ============ Prompt */

static smallint doprompt;                   /* if set, prompt the user */
static smallint needprompt;                 /* true if interactive and at start of line */

#if ENABLE_FEATURE_EDITING
static line_input_t *line_input_state;
static const char *cmdedit_prompt;
static void
putprompt(const char *s)
{
	if (ENABLE_ASH_EXPAND_PRMT) {
		free((char*)cmdedit_prompt);
		cmdedit_prompt = ckstrdup(s);
		return;
	}
	cmdedit_prompt = s;
}
#else
static void
putprompt(const char *s)
{
	out2str(s);
}
#endif

#if ENABLE_ASH_EXPAND_PRMT
/* expandstr() needs parsing machinery, so it is far away ahead... */
static const char *expandstr(const char *ps);
#else
#define expandstr(s) s
#endif

static void
setprompt(int whichprompt)
{
	const char *prompt;
#if ENABLE_ASH_EXPAND_PRMT
	struct stackmark smark;
#endif

	needprompt = 0;

	switch (whichprompt) {
	case 1:
		prompt = ps1val();
		break;
	case 2:
		prompt = ps2val();
		break;
	default:                        /* 0 */
		prompt = nullstr;
	}
#if ENABLE_ASH_EXPAND_PRMT
	setstackmark(&smark);
	stalloc(stackblocksize());
#endif
	putprompt(expandstr(prompt));
#if ENABLE_ASH_EXPAND_PRMT
	popstackmark(&smark);
#endif
}


/* ============ The cd and pwd commands */

#define CD_PHYSICAL 1
#define CD_PRINT 2

static int docd(const char *, int);

static int
cdopt(void)
{
	int flags = 0;
	int i, j;

	j = 'L';
	while ((i = nextopt("LP"))) {
		if (i != j) {
			flags ^= CD_PHYSICAL;
			j = i;
		}
	}

	return flags;
}

/*
 * Update curdir (the name of the current directory) in response to a
 * cd command.
 */
static const char *
updatepwd(const char *dir)
{
	char *new;
	char *p;
	char *cdcomppath;
	const char *lim;

	cdcomppath = ststrdup(dir);
	STARTSTACKSTR(new);
	if (*dir != '/') {
		if (curdir == nullstr)
			return 0;
		new = stack_putstr(curdir, new);
	}
	new = makestrspace(strlen(dir) + 2, new);
	lim = (char *)stackblock() + 1;
	if (*dir != '/') {
		if (new[-1] != '/')
			USTPUTC('/', new);
		if (new > lim && *lim == '/')
			lim++;
	} else {
		USTPUTC('/', new);
		cdcomppath++;
		if (dir[1] == '/' && dir[2] != '/') {
			USTPUTC('/', new);
			cdcomppath++;
			lim++;
		}
	}
	p = strtok(cdcomppath, "/");
	while (p) {
		switch (*p) {
		case '.':
			if (p[1] == '.' && p[2] == '\0') {
				while (new > lim) {
					STUNPUTC(new);
					if (new[-1] == '/')
						break;
				}
				break;
			}
			if (p[1] == '\0')
				break;
			/* fall through */
		default:
			new = stack_putstr(p, new);
			USTPUTC('/', new);
		}
		p = strtok(0, "/");
	}
	if (new > lim)
		STUNPUTC(new);
	*new = 0;
	return stackblock();
}

/*
 * Find out what the current directory is. If we already know the current
 * directory, this routine returns immediately.
 */
static char *
getpwd(void)
{
	char *dir = getcwd(NULL, 0); /* huh, using glibc extension? */
	return dir ? dir : nullstr;
}

static void
setpwd(const char *val, int setold)
{
	char *oldcur, *dir;

	oldcur = dir = curdir;

	if (setold) {
		setvar("OLDPWD", oldcur, VEXPORT);
	}
	INT_OFF;
	if (physdir != nullstr) {
		if (physdir != oldcur)
			free(physdir);
		physdir = nullstr;
	}
	if (oldcur == val || !val) {
		char *s = getpwd();
		physdir = s;
		if (!val)
			dir = s;
	} else
		dir = ckstrdup(val);
	if (oldcur != dir && oldcur != nullstr) {
		free(oldcur);
	}
	curdir = dir;
	INT_ON;
	setvar("PWD", dir, VEXPORT);
}

static void hashcd(void);

/*
 * Actually do the chdir.  We also call hashcd to let the routines in exec.c
 * know that the current directory has changed.
 */
static int
docd(const char *dest, int flags)
{
	const char *dir = 0;
	int err;

	TRACE(("docd(\"%s\", %d) called\n", dest, flags));

	INT_OFF;
	if (!(flags & CD_PHYSICAL)) {
		dir = updatepwd(dest);
		if (dir)
			dest = dir;
	}
	err = chdir(dest);
	if (err)
		goto out;
	setpwd(dir, 1);
	hashcd();
 out:
	INT_ON;
	return err;
}

static int
cdcmd(int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED)
{
	const char *dest;
	const char *path;
	const char *p;
	char c;
	struct stat statb;
	int flags;

	flags = cdopt();
	dest = *argptr;
	if (!dest)
		dest = bltinlookup(homestr);
	else if (LONE_DASH(dest)) {
		dest = bltinlookup("OLDPWD");
		flags |= CD_PRINT;
	}
	if (!dest)
		dest = nullstr;
	if (*dest == '/')
		goto step7;
	if (*dest == '.') {
		c = dest[1];
 dotdot:
		switch (c) {
		case '\0':
		case '/':
			goto step6;
		case '.':
			c = dest[2];
			if (c != '.')
				goto dotdot;
		}
	}
	if (!*dest)
		dest = ".";
	path = bltinlookup("CDPATH");
	if (!path) {
 step6:
 step7:
		p = dest;
		goto docd;
	}
	do {
		c = *path;
		p = padvance(&path, dest);
		if (stat(p, &statb) >= 0 && S_ISDIR(statb.st_mode)) {
			if (c && c != ':')
				flags |= CD_PRINT;
 docd:
			if (!docd(p, flags))
				goto out;
			break;
		}
	} while (path);
	ash_msg_and_raise_error("can't cd to %s", dest);
	/* NOTREACHED */
 out:
	if (flags & CD_PRINT)
		out1fmt(snlfmt, curdir);
	return 0;
}

static int
pwdcmd(int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED)
{
	int flags;
	const char *dir = curdir;

	flags = cdopt();
	if (flags) {
		if (physdir == nullstr)
			setpwd(dir, 0);
		dir = physdir;
	}
	out1fmt(snlfmt, dir);
	return 0;
}


/* ============ ... */

#define IBUFSIZ COMMON_BUFSIZE
#define basebuf bb_common_bufsiz1       /* buffer for top level input file */

/* Syntax classes */
#define CWORD 0                 /* character is nothing special */
#define CNL 1                   /* newline character */
#define CBACK 2                 /* a backslash character */
#define CSQUOTE 3               /* single quote */
#define CDQUOTE 4               /* double quote */
#define CENDQUOTE 5             /* a terminating quote */
#define CBQUOTE 6               /* backwards single quote */
#define CVAR 7                  /* a dollar sign */
#define CENDVAR 8               /* a '}' character */
#define CLP 9                   /* a left paren in arithmetic */
#define CRP 10                  /* a right paren in arithmetic */
#define CENDFILE 11             /* end of file */
#define CCTL 12                 /* like CWORD, except it must be escaped */
#define CSPCL 13                /* these terminate a word */
#define CIGN 14                 /* character should be ignored */

#if ENABLE_ASH_ALIAS
#define SYNBASE 130
#define PEOF -130
#define PEOA -129
#define PEOA_OR_PEOF PEOA
#else
#define SYNBASE 129
#define PEOF -129
#define PEOA_OR_PEOF PEOF
#endif

/* number syntax index */
#define BASESYNTAX 0    /* not in quotes */
#define DQSYNTAX   1    /* in double quotes */
#define SQSYNTAX   2    /* in single quotes */
#define ARISYNTAX  3    /* in arithmetic */
#define PSSYNTAX   4    /* prompt */

#if ENABLE_ASH_OPTIMIZE_FOR_SIZE
#define USE_SIT_FUNCTION
#endif

#if ENABLE_ASH_MATH_SUPPORT
static const char S_I_T[][4] = {
#if ENABLE_ASH_ALIAS
	{ CSPCL, CIGN, CIGN, CIGN },            /* 0, PEOA */
#endif
	{ CSPCL, CWORD, CWORD, CWORD },         /* 1, ' ' */
	{ CNL, CNL, CNL, CNL },                 /* 2, \n */
	{ CWORD, CCTL, CCTL, CWORD },           /* 3, !*-/:=?[]~ */
	{ CDQUOTE, CENDQUOTE, CWORD, CWORD },   /* 4, '"' */
	{ CVAR, CVAR, CWORD, CVAR },            /* 5, $ */
	{ CSQUOTE, CWORD, CENDQUOTE, CWORD },   /* 6, "'" */
	{ CSPCL, CWORD, CWORD, CLP },           /* 7, ( */
	{ CSPCL, CWORD, CWORD, CRP },           /* 8, ) */
	{ CBACK, CBACK, CCTL, CBACK },          /* 9, \ */
	{ CBQUOTE, CBQUOTE, CWORD, CBQUOTE },   /* 10, ` */
	{ CENDVAR, CENDVAR, CWORD, CENDVAR },   /* 11, } */
#ifndef USE_SIT_FUNCTION
	{ CENDFILE, CENDFILE, CENDFILE, CENDFILE }, /* 12, PEOF */
	{ CWORD, CWORD, CWORD, CWORD },         /* 13, 0-9A-Za-z */
	{ CCTL, CCTL, CCTL, CCTL }              /* 14, CTLESC ... */
#endif
};
#else
static const char S_I_T[][3] = {
#if ENABLE_ASH_ALIAS
	{ CSPCL, CIGN, CIGN },                  /* 0, PEOA */
#endif
	{ CSPCL, CWORD, CWORD },                /* 1, ' ' */
	{ CNL, CNL, CNL },                      /* 2, \n */
	{ CWORD, CCTL, CCTL },                  /* 3, !*-/:=?[]~ */
	{ CDQUOTE, CENDQUOTE, CWORD },          /* 4, '"' */
	{ CVAR, CVAR, CWORD },                  /* 5, $ */
	{ CSQUOTE, CWORD, CENDQUOTE },          /* 6, "'" */
	{ CSPCL, CWORD, CWORD },                /* 7, ( */
	{ CSPCL, CWORD, CWORD },                /* 8, ) */
	{ CBACK, CBACK, CCTL },                 /* 9, \ */
	{ CBQUOTE, CBQUOTE, CWORD },            /* 10, ` */
	{ CENDVAR, CENDVAR, CWORD },            /* 11, } */
#ifndef USE_SIT_FUNCTION
	{ CENDFILE, CENDFILE, CENDFILE },       /* 12, PEOF */
	{ CWORD, CWORD, CWORD },                /* 13, 0-9A-Za-z */
	{ CCTL, CCTL, CCTL }                    /* 14, CTLESC ... */
#endif
};
#endif /* ASH_MATH_SUPPORT */

#ifdef USE_SIT_FUNCTION

static int
SIT(int c, int syntax)
{
	static const char spec_symbls[] ALIGN1 = "\t\n !\"$&'()*-/:;<=>?[\\]`|}~";
#if ENABLE_ASH_ALIAS
	static const char syntax_index_table[] ALIGN1 = {
		1, 2, 1, 3, 4, 5, 1, 6,         /* "\t\n !\"$&'" */
		7, 8, 3, 3, 3, 3, 1, 1,         /* "()*-/:;<" */
		3, 1, 3, 3, 9, 3, 10, 1,        /* "=>?[\\]`|" */
		11, 3                           /* "}~" */
	};
#else
	static const char syntax_index_table[] ALIGN1 = {
		0, 1, 0, 2, 3, 4, 0, 5,         /* "\t\n !\"$&'" */
		6, 7, 2, 2, 2, 2, 0, 0,         /* "()*-/:;<" */
		2, 0, 2, 2, 8, 2, 9, 0,         /* "=>?[\\]`|" */
		10, 2                           /* "}~" */
	};
#endif
	const char *s;
	int indx;

	if (c == PEOF)          /* 2^8+2 */
		return CENDFILE;
#if ENABLE_ASH_ALIAS
	if (c == PEOA)          /* 2^8+1 */
		indx = 0;
	else
#endif
#define U_C(c) ((unsigned char)(c))

	if ((unsigned char)c >= (unsigned char)(CTLESC)
	 && (unsigned char)c <= (unsigned char)(CTLQUOTEMARK)
	) {
		return CCTL;
	} else {
		s = strchrnul(spec_symbls, c);
		if (*s == '\0')
			return CWORD;
		indx = syntax_index_table[s - spec_symbls];
	}
	return S_I_T[indx][syntax];
}

#else   /* !USE_SIT_FUNCTION */

#if ENABLE_ASH_ALIAS
#define CSPCL_CIGN_CIGN_CIGN                     0
#define CSPCL_CWORD_CWORD_CWORD                  1
#define CNL_CNL_CNL_CNL                          2
#define CWORD_CCTL_CCTL_CWORD                    3
#define CDQUOTE_CENDQUOTE_CWORD_CWORD            4
#define CVAR_CVAR_CWORD_CVAR                     5
#define CSQUOTE_CWORD_CENDQUOTE_CWORD            6
#define CSPCL_CWORD_CWORD_CLP                    7
#define CSPCL_CWORD_CWORD_CRP                    8
#define CBACK_CBACK_CCTL_CBACK                   9
#define CBQUOTE_CBQUOTE_CWORD_CBQUOTE           10
#define CENDVAR_CENDVAR_CWORD_CENDVAR           11
#define CENDFILE_CENDFILE_CENDFILE_CENDFILE     12
#define CWORD_CWORD_CWORD_CWORD                 13
#define CCTL_CCTL_CCTL_CCTL                     14
#else
#define CSPCL_CWORD_CWORD_CWORD                  0
#define CNL_CNL_CNL_CNL                          1
#define CWORD_CCTL_CCTL_CWORD                    2
#define CDQUOTE_CENDQUOTE_CWORD_CWORD            3
#define CVAR_CVAR_CWORD_CVAR                     4
#define CSQUOTE_CWORD_CENDQUOTE_CWORD            5
#define CSPCL_CWORD_CWORD_CLP                    6
#define CSPCL_CWORD_CWORD_CRP                    7
#define CBACK_CBACK_CCTL_CBACK                   8
#define CBQUOTE_CBQUOTE_CWORD_CBQUOTE            9
#define CENDVAR_CENDVAR_CWORD_CENDVAR           10
#define CENDFILE_CENDFILE_CENDFILE_CENDFILE     11
#define CWORD_CWORD_CWORD_CWORD                 12
#define CCTL_CCTL_CCTL_CCTL                     13
#endif

static const char syntax_index_table[258] = {
	/* BASESYNTAX_DQSYNTAX_SQSYNTAX_ARISYNTAX */
	/*   0  PEOF */      CENDFILE_CENDFILE_CENDFILE_CENDFILE,
#if ENABLE_ASH_ALIAS
	/*   1  PEOA */      CSPCL_CIGN_CIGN_CIGN,
#endif
	/*   2  -128 0x80 */ CWORD_CWORD_CWORD_CWORD,
	/*   3  -127 CTLESC       */ CCTL_CCTL_CCTL_CCTL,
	/*   4  -126 CTLVAR       */ CCTL_CCTL_CCTL_CCTL,
	/*   5  -125 CTLENDVAR    */ CCTL_CCTL_CCTL_CCTL,
	/*   6  -124 CTLBACKQ     */ CCTL_CCTL_CCTL_CCTL,
	/*   7  -123 CTLQUOTE     */ CCTL_CCTL_CCTL_CCTL,
	/*   8  -122 CTLARI       */ CCTL_CCTL_CCTL_CCTL,
	/*   9  -121 CTLENDARI    */ CCTL_CCTL_CCTL_CCTL,
	/*  10  -120 CTLQUOTEMARK */ CCTL_CCTL_CCTL_CCTL,
	/*  11  -119      */ CWORD_CWORD_CWORD_CWORD,
	/*  12  -118      */ CWORD_CWORD_CWORD_CWORD,
	/*  13  -117      */ CWORD_CWORD_CWORD_CWORD,
	/*  14  -116      */ CWORD_CWORD_CWORD_CWORD,
	/*  15  -115      */ CWORD_CWORD_CWORD_CWORD,
	/*  16  -114      */ CWORD_CWORD_CWORD_CWORD,
	/*  17  -113      */ CWORD_CWORD_CWORD_CWORD,
	/*  18  -112      */ CWORD_CWORD_CWORD_CWORD,
	/*  19  -111      */ CWORD_CWORD_CWORD_CWORD,
	/*  20  -110      */ CWORD_CWORD_CWORD_CWORD,
	/*  21  -109      */ CWORD_CWORD_CWORD_CWORD,
	/*  22  -108      */ CWORD_CWORD_CWORD_CWORD,
	/*  23  -107      */ CWORD_CWORD_CWORD_CWORD,
	/*  24  -106      */ CWORD_CWORD_CWORD_CWORD,
	/*  25  -105      */ CWORD_CWORD_CWORD_CWORD,
	/*  26  -104      */ CWORD_CWORD_CWORD_CWORD,
	/*  27  -103      */ CWORD_CWORD_CWORD_CWORD,
	/*  28  -102      */ CWORD_CWORD_CWORD_CWORD,
	/*  29  -101      */ CWORD_CWORD_CWORD_CWORD,
	/*  30  -100      */ CWORD_CWORD_CWORD_CWORD,
	/*  31   -99      */ CWORD_CWORD_CWORD_CWORD,
	/*  32   -98      */ CWORD_CWORD_CWORD_CWORD,
	/*  33   -97      */ CWORD_CWORD_CWORD_CWORD,
	/*  34   -96      */ CWORD_CWORD_CWORD_CWORD,
	/*  35   -95      */ CWORD_CWORD_CWORD_CWORD,
	/*  36   -94      */ CWORD_CWORD_CWORD_CWORD,
	/*  37   -93      */ CWORD_CWORD_CWORD_CWORD,
	/*  38   -92      */ CWORD_CWORD_CWORD_CWORD,
	/*  39   -91      */ CWORD_CWORD_CWORD_CWORD,
	/*  40   -90      */ CWORD_CWORD_CWORD_CWORD,
	/*  41   -89      */ CWORD_CWORD_CWORD_CWORD,
	/*  42   -88      */ CWORD_CWORD_CWORD_CWORD,
	/*  43   -87      */ CWORD_CWORD_CWORD_CWORD,
	/*  44   -86      */ CWORD_CWORD_CWORD_CWORD,
	/*  45   -85      */ CWORD_CWORD_CWORD_CWORD,
	/*  46   -84      */ CWORD_CWORD_CWORD_CWORD,
	/*  47   -83      */ CWORD_CWORD_CWORD_CWORD,
	/*  48   -82      */ CWORD_CWORD_CWORD_CWORD,
	/*  49   -81      */ CWORD_CWORD_CWORD_CWORD,
	/*  50   -80      */ CWORD_CWORD_CWORD_CWORD,
	/*  51   -79      */ CWORD_CWORD_CWORD_CWORD,
	/*  52   -78      */ CWORD_CWORD_CWORD_CWORD,
	/*  53   -77      */ CWORD_CWORD_CWORD_CWORD,
	/*  54   -76      */ CWORD_CWORD_CWORD_CWORD,
	/*  55   -75      */ CWORD_CWORD_CWORD_CWORD,
	/*  56   -74      */ CWORD_CWORD_CWORD_CWORD,
	/*  57   -73      */ CWORD_CWORD_CWORD_CWORD,
	/*  58   -72      */ CWORD_CWORD_CWORD_CWORD,
	/*  59   -71      */ CWORD_CWORD_CWORD_CWORD,
	/*  60   -70      */ CWORD_CWORD_CWORD_CWORD,
	/*  61   -69      */ CWORD_CWORD_CWORD_CWORD,
	/*  62   -68      */ CWORD_CWORD_CWORD_CWORD,
	/*  63   -67      */ CWORD_CWORD_CWORD_CWORD,
	/*  64   -66      */ CWORD_CWORD_CWORD_CWORD,
	/*  65   -65      */ CWORD_CWORD_CWORD_CWORD,
	/*  66   -64      */ CWORD_CWORD_CWORD_CWORD,
	/*  67   -63      */ CWORD_CWORD_CWORD_CWORD,
	/*  68   -62      */ CWORD_CWORD_CWORD_CWORD,
	/*  69   -61      */ CWORD_CWORD_CWORD_CWORD,
	/*  70   -60      */ CWORD_CWORD_CWORD_CWORD,
	/*  71   -59      */ CWORD_CWORD_CWORD_CWORD,
	/*  72   -58      */ CWORD_CWORD_CWORD_CWORD,
	/*  73   -57      */ CWORD_CWORD_CWORD_CWORD,
	/*  74   -56      */ CWORD_CWORD_CWORD_CWORD,
	/*  75   -55      */ CWORD_CWORD_CWORD_CWORD,
	/*  76   -54      */ CWORD_CWORD_CWORD_CWORD,
	/*  77   -53      */ CWORD_CWORD_CWORD_CWORD,
	/*  78   -52      */ CWORD_CWORD_CWORD_CWORD,
	/*  79   -51      */ CWORD_CWORD_CWORD_CWORD,
	/*  80   -50      */ CWORD_CWORD_CWORD_CWORD,
	/*  81   -49      */ CWORD_CWORD_CWORD_CWORD,
	/*  82   -48      */ CWORD_CWORD_CWORD_CWORD,
	/*  83   -47      */ CWORD_CWORD_CWORD_CWORD,
	/*  84   -46      */ CWORD_CWORD_CWORD_CWORD,
	/*  85   -45      */ CWORD_CWORD_CWORD_CWORD,
	/*  86   -44      */ CWORD_CWORD_CWORD_CWORD,
	/*  87   -43      */ CWORD_CWORD_CWORD_CWORD,
	/*  88   -42      */ CWORD_CWORD_CWORD_CWORD,
	/*  89   -41      */ CWORD_CWORD_CWORD_CWORD,
	/*  90   -40      */ CWORD_CWORD_CWORD_CWORD,
	/*  91   -39      */ CWORD_CWORD_CWORD_CWORD,
	/*  92   -38      */ CWORD_CWORD_CWORD_CWORD,
	/*  93   -37      */ CWORD_CWORD_CWORD_CWORD,
	/*  94   -36      */ CWORD_CWORD_CWORD_CWORD,
	/*  95   -35      */ CWORD_CWORD_CWORD_CWORD,
	/*  96   -34      */ CWORD_CWORD_CWORD_CWORD,
	/*  97   -33      */ CWORD_CWORD_CWORD_CWORD,
	/*  98   -32      */ CWORD_CWORD_CWORD_CWORD,
	/*  99   -31      */ CWORD_CWORD_CWORD_CWORD,
	/* 100   -30      */ CWORD_CWORD_CWORD_CWORD,
	/* 101   -29      */ CWORD_CWORD_CWORD_CWORD,
	/* 102   -28      */ CWORD_CWORD_CWORD_CWORD,
	/* 103   -27      */ CWORD_CWORD_CWORD_CWORD,
	/* 104   -26      */ CWORD_CWORD_CWORD_CWORD,
	/* 105   -25      */ CWORD_CWORD_CWORD_CWORD,
	/* 106   -24      */ CWORD_CWORD_CWORD_CWORD,
	/* 107   -23      */ CWORD_CWORD_CWORD_CWORD,
	/* 108   -22      */ CWORD_CWORD_CWORD_CWORD,
	/* 109   -21      */ CWORD_CWORD_CWORD_CWORD,
	/* 110   -20      */ CWORD_CWORD_CWORD_CWORD,
	/* 111   -19      */ CWORD_CWORD_CWORD_CWORD,
	/* 112   -18      */ CWORD_CWORD_CWORD_CWORD,
	/* 113   -17      */ CWORD_CWORD_CWORD_CWORD,
	/* 114   -16      */ CWORD_CWORD_CWORD_CWORD,
	/* 115   -15      */ CWORD_CWORD_CWORD_CWORD,
	/* 116   -14      */ CWORD_CWORD_CWORD_CWORD,
	/* 117   -13      */ CWORD_CWORD_CWORD_CWORD,
	/* 118   -12      */ CWORD_CWORD_CWORD_CWORD,
	/* 119   -11      */ CWORD_CWORD_CWORD_CWORD,
	/* 120   -10      */ CWORD_CWORD_CWORD_CWORD,
	/* 121    -9      */ CWORD_CWORD_CWORD_CWORD,
	/* 122    -8      */ CWORD_CWORD_CWORD_CWORD,
	/* 123    -7      */ CWORD_CWORD_CWORD_CWORD,
	/* 124    -6      */ CWORD_CWORD_CWORD_CWORD,
	/* 125    -5      */ CWORD_CWORD_CWORD_CWORD,
	/* 126    -4      */ CWORD_CWORD_CWORD_CWORD,
	/* 127    -3      */ CWORD_CWORD_CWORD_CWORD,
	/* 128    -2      */ CWORD_CWORD_CWORD_CWORD,
	/* 129    -1      */ CWORD_CWORD_CWORD_CWORD,
	/* 130     0      */ CWORD_CWORD_CWORD_CWORD,
	/* 131     1      */ CWORD_CWORD_CWORD_CWORD,
	/* 132     2      */ CWORD_CWORD_CWORD_CWORD,
	/* 133     3      */ CWORD_CWORD_CWORD_CWORD,
	/* 134     4      */ CWORD_CWORD_CWORD_CWORD,
	/* 135     5      */ CWORD_CWORD_CWORD_CWORD,
	/* 136     6      */ CWORD_CWORD_CWORD_CWORD,
	/* 137     7      */ CWORD_CWORD_CWORD_CWORD,
	/* 138     8      */ CWORD_CWORD_CWORD_CWORD,
	/* 139     9 "\t" */ CSPCL_CWORD_CWORD_CWORD,
	/* 140    10 "\n" */ CNL_CNL_CNL_CNL,
	/* 141    11      */ CWORD_CWORD_CWORD_CWORD,
	/* 142    12      */ CWORD_CWORD_CWORD_CWORD,
	/* 143    13      */ CWORD_CWORD_CWORD_CWORD,
	/* 144    14      */ CWORD_CWORD_CWORD_CWORD,
	/* 145    15      */ CWORD_CWORD_CWORD_CWORD,
	/* 146    16      */ CWORD_CWORD_CWORD_CWORD,
	/* 147    17      */ CWORD_CWORD_CWORD_CWORD,
	/* 148    18      */ CWORD_CWORD_CWORD_CWORD,
	/* 149    19      */ CWORD_CWORD_CWORD_CWORD,
	/* 150    20      */ CWORD_CWORD_CWORD_CWORD,
	/* 151    21      */ CWORD_CWORD_CWORD_CWORD,
	/* 152    22      */ CWORD_CWORD_CWORD_CWORD,
	/* 153    23      */ CWORD_CWORD_CWORD_CWORD,
	/* 154    24      */ CWORD_CWORD_CWORD_CWORD,
	/* 155    25      */ CWORD_CWORD_CWORD_CWORD,
	/* 156    26      */ CWORD_CWORD_CWORD_CWORD,
	/* 157    27      */ CWORD_CWORD_CWORD_CWORD,
	/* 158    28      */ CWORD_CWORD_CWORD_CWORD,
	/* 159    29      */ CWORD_CWORD_CWORD_CWORD,
	/* 160    30      */ CWORD_CWORD_CWORD_CWORD,
	/* 161    31      */ CWORD_CWORD_CWORD_CWORD,
	/* 162    32  " " */ CSPCL_CWORD_CWORD_CWORD,
	/* 163    33  "!" */ CWORD_CCTL_CCTL_CWORD,
	/* 164    34  """ */ CDQUOTE_CENDQUOTE_CWORD_CWORD,
	/* 165    35  "#" */ CWORD_CWORD_CWORD_CWORD,
	/* 166    36  "$" */ CVAR_CVAR_CWORD_CVAR,
	/* 167    37  "%" */ CWORD_CWORD_CWORD_CWORD,
	/* 168    38  "&" */ CSPCL_CWORD_CWORD_CWORD,
	/* 169    39  "'" */ CSQUOTE_CWORD_CENDQUOTE_CWORD,
	/* 170    40  "(" */ CSPCL_CWORD_CWORD_CLP,
	/* 171    41  ")" */ CSPCL_CWORD_CWORD_CRP,
	/* 172    42  "*" */ CWORD_CCTL_CCTL_CWORD,
	/* 173    43  "+" */ CWORD_CWORD_CWORD_CWORD,
	/* 174    44  "," */ CWORD_CWORD_CWORD_CWORD,
	/* 175    45  "-" */ CWORD_CCTL_CCTL_CWORD,
	/* 176    46  "." */ CWORD_CWORD_CWORD_CWORD,
	/* 177    47  "/" */ CWORD_CCTL_CCTL_CWORD,
	/* 178    48  "0" */ CWORD_CWORD_CWORD_CWORD,
	/* 179    49  "1" */ CWORD_CWORD_CWORD_CWORD,
	/* 180    50  "2" */ CWORD_CWORD_CWORD_CWORD,
	/* 181    51  "3" */ CWORD_CWORD_CWORD_CWORD,
	/* 182    52  "4" */ CWORD_CWORD_CWORD_CWORD,
	/* 183    53  "5" */ CWORD_CWORD_CWORD_CWORD,
	/* 184    54  "6" */ CWORD_CWORD_CWORD_CWORD,
	/* 185    55  "7" */ CWORD_CWORD_CWORD_CWORD,
	/* 186    56  "8" */ CWORD_CWORD_CWORD_CWORD,
	/* 187    57  "9" */ CWORD_CWORD_CWORD_CWORD,
	/* 188    58  ":" */ CWORD_CCTL_CCTL_CWORD,
	/* 189    59  ";" */ CSPCL_CWORD_CWORD_CWORD,
	/* 190    60  "<" */ CSPCL_CWORD_CWORD_CWORD,
	/* 191    61  "=" */ CWORD_CCTL_CCTL_CWORD,
	/* 192    62  ">" */ CSPCL_CWORD_CWORD_CWORD,
	/* 193    63  "?" */ CWORD_CCTL_CCTL_CWORD,
	/* 194    64  "@" */ CWORD_CWORD_CWORD_CWORD,
	/* 195    65  "A" */ CWORD_CWORD_CWORD_CWORD,
	/* 196    66  "B" */ CWORD_CWORD_CWORD_CWORD,
	/* 197    67  "C" */ CWORD_CWORD_CWORD_CWORD,
	/* 198    68  "D" */ CWORD_CWORD_CWORD_CWORD,
	/* 199    69  "E" */ CWORD_CWORD_CWORD_CWORD,
	/* 200    70  "F" */ CWORD_CWORD_CWORD_CWORD,
	/* 201    71  "G" */ CWORD_CWORD_CWORD_CWORD,
	/* 202    72  "H" */ CWORD_CWORD_CWORD_CWORD,
	/* 203    73  "I" */ CWORD_CWORD_CWORD_CWORD,
	/* 204    74  "J" */ CWORD_CWORD_CWORD_CWORD,
	/* 205    75  "K" */ CWORD_CWORD_CWORD_CWORD,
	/* 206    76  "L" */ CWORD_CWORD_CWORD_CWORD,
	/* 207    77  "M" */ CWORD_CWORD_CWORD_CWORD,
	/* 208    78  "N" */ CWORD_CWORD_CWORD_CWORD,
	/* 209    79  "O" */ CWORD_CWORD_CWORD_CWORD,
	/* 210    80  "P" */ CWORD_CWORD_CWORD_CWORD,
	/* 211    81  "Q" */ CWORD_CWORD_CWORD_CWORD,
	/* 212    82  "R" */ CWORD_CWORD_CWORD_CWORD,
	/* 213    83  "S" */ CWORD_CWORD_CWORD_CWORD,
	/* 214    84  "T" */ CWORD_CWORD_CWORD_CWORD,
	/* 215    85  "U" */ CWORD_CWORD_CWORD_CWORD,
	/* 216    86  "V" */ CWORD_CWORD_CWORD_CWORD,
	/* 217    87  "W" */ CWORD_CWORD_CWORD_CWORD,
	/* 218    88  "X" */ CWORD_CWORD_CWORD_CWORD,
	/* 219    89  "Y" */ CWORD_CWORD_CWORD_CWORD,
	/* 220    90  "Z" */ CWORD_CWORD_CWORD_CWORD,
	/* 221    91  "[" */ CWORD_CCTL_CCTL_CWORD,
	/* 222    92  "\" */ CBACK_CBACK_CCTL_CBACK,
	/* 223    93  "]" */ CWORD_CCTL_CCTL_CWORD,
	/* 224    94  "^" */ CWORD_CWORD_CWORD_CWORD,
	/* 225    95  "_" */ CWORD_CWORD_CWORD_CWORD,
	/* 226    96  "`" */ CBQUOTE_CBQUOTE_CWORD_CBQUOTE,
	/* 227    97  "a" */ CWORD_CWORD_CWORD_CWORD,
	/* 228    98  "b" */ CWORD_CWORD_CWORD_CWORD,
	/* 229    99  "c" */ CWORD_CWORD_CWORD_CWORD,
	/* 230   100  "d" */ CWORD_CWORD_CWORD_CWORD,
	/* 231   101  "e" */ CWORD_CWORD_CWORD_CWORD,
	/* 232   102  "f" */ CWORD_CWORD_CWORD_CWORD,
	/* 233   103  "g" */ CWORD_CWORD_CWORD_CWORD,
	/* 234   104  "h" */ CWORD_CWORD_CWORD_CWORD,
	/* 235   105  "i" */ CWORD_CWORD_CWORD_CWORD,
	/* 236   106  "j" */ CWORD_CWORD_CWORD_CWORD,
	/* 237   107  "k" */ CWORD_CWORD_CWORD_CWORD,
	/* 238   108  "l" */ CWORD_CWORD_CWORD_CWORD,
	/* 239   109  "m" */ CWORD_CWORD_CWORD_CWORD,
	/* 240   110  "n" */ CWORD_CWORD_CWORD_CWORD,
	/* 241   111  "o" */ CWORD_CWORD_CWORD_CWORD,
	/* 242   112  "p" */ CWORD_CWORD_CWORD_CWORD,
	/* 243   113  "q" */ CWORD_CWORD_CWORD_CWORD,
	/* 244   114  "r" */ CWORD_CWORD_CWORD_CWORD,
	/* 245   115  "s" */ CWORD_CWORD_CWORD_CWORD,
	/* 246   116  "t" */ CWORD_CWORD_CWORD_CWORD,
	/* 247   117  "u" */ CWORD_CWORD_CWORD_CWORD,
	/* 248   118  "v" */ CWORD_CWORD_CWORD_CWORD,
	/* 249   119  "w" */ CWORD_CWORD_CWORD_CWORD,
	/* 250   120  "x" */ CWORD_CWORD_CWORD_CWORD,
	/* 251   121  "y" */ CWORD_CWORD_CWORD_CWORD,
	/* 252   122  "z" */ CWORD_CWORD_CWORD_CWORD,
	/* 253   123  "{" */ CWORD_CWORD_CWORD_CWORD,
	/* 254   124  "|" */ CSPCL_CWORD_CWORD_CWORD,
	/* 255   125  "}" */ CENDVAR_CENDVAR_CWORD_CENDVAR,
	/* 256   126  "~" */ CWORD_CCTL_CCTL_CWORD,
	/* 257   127      */ CWORD_CWORD_CWORD_CWORD,
};

#define SIT(c, syntax) (S_I_T[(int)syntax_index_table[((int)c)+SYNBASE]][syntax])

#endif  /* USE_SIT_FUNCTION */


/* ============ Alias handling */

#if ENABLE_ASH_ALIAS

#define ALIASINUSE 1
#define ALIASDEAD  2

struct alias {
	struct alias *next;
	char *name;
	char *val;
	int flag;
};


static struct alias **atab; // [ATABSIZE];
#define INIT_G_alias() do { \
	atab = xzalloc(ATABSIZE * sizeof(atab[0])); \
} while (0)


static struct alias **
__lookupalias(const char *name) {
	unsigned int hashval;
	struct alias **app;
	const char *p;
	unsigned int ch;

	p = name;

	ch = (unsigned char)*p;
	hashval = ch << 4;
	while (ch) {
		hashval += ch;
		ch = (unsigned char)*++p;
	}
	app = &atab[hashval % ATABSIZE];

	for (; *app; app = &(*app)->next) {
		if (strcmp(name, (*app)->name) == 0) {
			break;
		}
	}

	return app;
}

static struct alias *
lookupalias(const char *name, int check)
{
	struct alias *ap = *__lookupalias(name);

	if (check && ap && (ap->flag & ALIASINUSE))
		return NULL;
	return ap;
}

static struct alias *
freealias(struct alias *ap)
{
	struct alias *next;

	if (ap->flag & ALIASINUSE) {
		ap->flag |= ALIASDEAD;
		return ap;
	}

	next = ap->next;
	free(ap->name);
	free(ap->val);
	free(ap);
	return next;
}

static void
setalias(const char *name, const char *val)
{
	struct alias *ap, **app;

	app = __lookupalias(name);
	ap = *app;
	INT_OFF;
	if (ap) {
		if (!(ap->flag & ALIASINUSE)) {
			free(ap->val);
		}
		ap->val = ckstrdup(val);
		ap->flag &= ~ALIASDEAD;
	} else {
		/* not found */
		ap = ckzalloc(sizeof(struct alias));
		ap->name = ckstrdup(name);
		ap->val = ckstrdup(val);
		/*ap->flag = 0; - ckzalloc did it */
		/*ap->next = NULL;*/
		*app = ap;
	}
	INT_ON;
}

static int
unalias(const char *name)
{
	struct alias **app;

	app = __lookupalias(name);

	if (*app) {
		INT_OFF;
		*app = freealias(*app);
		INT_ON;
		return 0;
	}

	return 1;
}

static void
rmaliases(void)
{
	struct alias *ap, **app;
	int i;

	INT_OFF;
	for (i = 0; i < ATABSIZE; i++) {
		app = &atab[i];
		for (ap = *app; ap; ap = *app) {
			*app = freealias(*app);
			if (ap == *app) {
				app = &ap->next;
			}
		}
	}
	INT_ON;
}

static void
printalias(const struct alias *ap)
{
	out1fmt("%s=%s\n", ap->name, single_quote(ap->val));
}

/*
 * TODO - sort output
 */
static int
aliascmd(int argc ATTRIBUTE_UNUSED, char **argv)
{
	char *n, *v;
	int ret = 0;
	struct alias *ap;

	if (!argv[1]) {
		int i;

		for (i = 0; i < ATABSIZE; i++) {
			for (ap = atab[i]; ap; ap = ap->next) {
				printalias(ap);
			}
		}
		return 0;
	}
	while ((n = *++argv) != NULL) {
		v = strchr(n+1, '=');
		if (v == NULL) { /* n+1: funny ksh stuff */
			ap = *__lookupalias(n);
			if (ap == NULL) {
				fprintf(stderr, "%s: %s not found\n", "alias", n);
				ret = 1;
			} else
				printalias(ap);
		} else {
			*v++ = '\0';
			setalias(n, v);
		}
	}

	return ret;
}

static int
unaliascmd(int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED)
{
	int i;

	while ((i = nextopt("a")) != '\0') {
		if (i == 'a') {
			rmaliases();
			return 0;
		}
	}
	for (i = 0; *argptr; argptr++) {
		if (unalias(*argptr)) {
			fprintf(stderr, "%s: %s not found\n", "unalias", *argptr);
			i = 1;
		}
	}

	return i;
}

#endif /* ASH_ALIAS */


/* ============ jobs.c */

/* Mode argument to forkshell.  Don't change FORK_FG or FORK_BG. */
#define FORK_FG 0
#define FORK_BG 1
#define FORK_NOJOB 2

/* mode flags for showjob(s) */
#define SHOW_PGID       0x01    /* only show pgid - for jobs -p */
#define SHOW_PID        0x04    /* include process pid */
#define SHOW_CHANGED    0x08    /* only jobs whose state has changed */

/*
 * A job structure contains information about a job.  A job is either a
 * single process or a set of processes contained in a pipeline.  In the
 * latter case, pidlist will be non-NULL, and will point to a -1 terminated
 * array of pids.
 */

struct procstat {
	pid_t   pid;            /* process id */
	int     status;         /* last process status from wait() */
	char    *cmd;           /* text of command being run */
};

struct job {
	struct procstat ps0;    /* status of process */
	struct procstat *ps;    /* status or processes when more than one */
#if JOBS
	int stopstatus;         /* status of a stopped job */
#endif
	uint32_t
		nprocs: 16,     /* number of processes */
		state: 8,
#define JOBRUNNING      0       /* at least one proc running */
#define JOBSTOPPED      1       /* all procs are stopped */
#define JOBDONE         2       /* all procs are completed */
#if JOBS
		sigint: 1,      /* job was killed by SIGINT */
		jobctl: 1,      /* job running under job control */
#endif
		waited: 1,      /* true if this entry has been waited for */
		used: 1,        /* true if this entry is in used */
		changed: 1;     /* true if status has changed */
	struct job *prev_job;   /* previous job */
};

static pid_t backgndpid;        /* pid of last background process */
static smallint job_warning;    /* user was warned about stopped jobs (can be 2, 1 or 0). */

static struct job *makejob(/*union node *,*/ int);
#if !JOBS
#define forkshell(job, node, mode) forkshell(job, mode)
#endif
static int forkshell(struct job *, union node *, int);
static int waitforjob(struct job *);

#if !JOBS
enum { doing_jobctl = 0 };
#define setjobctl(on) do {} while (0)
#else
static smallint doing_jobctl;
static void setjobctl(int);
#endif

/*
 * Set the signal handler for the specified signal.  The routine figures
 * out what it should be set to.
 */
static void
setsignal(int signo)
{
	int action;
	char *t, tsig;
	struct sigaction act;

	t = trap[signo];
	action = S_IGN;
	if (t == NULL)
		action = S_DFL;
	else if (*t != '\0')
		action = S_CATCH;
	if (rootshell && action == S_DFL) {
		switch (signo) {
		case SIGINT:
			if (iflag || minusc || sflag == 0)
				action = S_CATCH;
			break;
		case SIGQUIT:
#if DEBUG
			if (debug)
				break;
#endif
			/* FALLTHROUGH */
		case SIGTERM:
			if (iflag)
				action = S_IGN;
			break;
#if JOBS
		case SIGTSTP:
		case SIGTTOU:
			if (mflag)
				action = S_IGN;
			break;
#endif
		}
	}

	t = &sigmode[signo - 1];
	tsig = *t;
	if (tsig == 0) {
		/*
		 * current setting unknown
		 */
		if (sigaction(signo, NULL, &act) == -1) {
			/*
			 * Pretend it worked; maybe we should give a warning
			 * here, but other shells don't. We don't alter
			 * sigmode, so that we retry every time.
			 */
			return;
		}
		tsig = S_RESET; /* force to be set */
		if (act.sa_handler == SIG_IGN) {
			tsig = S_HARD_IGN;
			if (mflag
			 && (signo == SIGTSTP || signo == SIGTTIN || signo == SIGTTOU)
			) {
				tsig = S_IGN;   /* don't hard ignore these */
			}
		}
	}
	if (tsig == S_HARD_IGN || tsig == action)
		return;
	act.sa_handler = SIG_DFL;
	switch (action) {
	case S_CATCH:
		act.sa_handler = onsig;
		break;
	case S_IGN:
		act.sa_handler = SIG_IGN;
		break;
	}
	*t = action;
	act.sa_flags = 0;
	sigfillset(&act.sa_mask);
	sigaction_set(signo, &act);
}

/* mode flags for set_curjob */
#define CUR_DELETE 2
#define CUR_RUNNING 1
#define CUR_STOPPED 0

/* mode flags for dowait */
#define DOWAIT_NONBLOCK WNOHANG
#define DOWAIT_BLOCK    0

#if JOBS
/* pgrp of shell on invocation */
static int initialpgrp;
static int ttyfd = -1;
#endif
/* array of jobs */
static struct job *jobtab;
/* size of array */
static unsigned njobs;
/* current job */
static struct job *curjob;
/* number of presumed living untracked jobs */
static int jobless;

static void
set_curjob(struct job *jp, unsigned mode)
{
	struct job *jp1;
	struct job **jpp, **curp;

	/* first remove from list */
	jpp = curp = &curjob;
	do {
		jp1 = *jpp;
		if (jp1 == jp)
			break;
		jpp = &jp1->prev_job;
	} while (1);
	*jpp = jp1->prev_job;

	/* Then re-insert in correct position */
	jpp = curp;
	switch (mode) {
	default:
#if DEBUG
		abort();
#endif
	case CUR_DELETE:
		/* job being deleted */
		break;
	case CUR_RUNNING:
		/* newly created job or backgrounded job,
		   put after all stopped jobs. */
		do {
			jp1 = *jpp;
#if JOBS
			if (!jp1 || jp1->state != JOBSTOPPED)
#endif
				break;
			jpp = &jp1->prev_job;
		} while (1);
		/* FALLTHROUGH */
#if JOBS
	case CUR_STOPPED:
#endif
		/* newly stopped job - becomes curjob */
		jp->prev_job = *jpp;
		*jpp = jp;
		break;
	}
}

#if JOBS || DEBUG
static int
jobno(const struct job *jp)
{
	return jp - jobtab + 1;
}
#endif

/*
 * Convert a job name to a job structure.
 */
#if !JOBS
#define getjob(name, getctl) getjob(name)
#endif
static struct job *
getjob(const char *name, int getctl)
{
	struct job *jp;
	struct job *found;
	const char *err_msg = "No such job: %s";
	unsigned num;
	int c;
	const char *p;
	char *(*match)(const char *, const char *);

	jp = curjob;
	p = name;
	if (!p)
		goto currentjob;

	if (*p != '%')
		goto err;

	c = *++p;
	if (!c)
		goto currentjob;

	if (!p[1]) {
		if (c == '+' || c == '%') {
 currentjob:
			err_msg = "No current job";
			goto check;
		}
		if (c == '-') {
			if (jp)
				jp = jp->prev_job;
			err_msg = "No previous job";
 check:
			if (!jp)
				goto err;
			goto gotit;
		}
	}

	if (is_number(p)) {
// TODO: number() instead? It does error checking...
		num = atoi(p);
		if (num < njobs) {
			jp = jobtab + num - 1;
			if (jp->used)
				goto gotit;
			goto err;
		}
	}

	match = prefix;
	if (*p == '?') {
		match = strstr;
		p++;
	}

	found = 0;
	while (1) {
		if (!jp)
			goto err;
		if (match(jp->ps[0].cmd, p)) {
			if (found)
				goto err;
			found = jp;
			err_msg = "%s: ambiguous";
		}
		jp = jp->prev_job;
	}

 gotit:
#if JOBS
	err_msg = "job %s not created under job control";
	if (getctl && jp->jobctl == 0)
		goto err;
#endif
	return jp;
 err:
	ash_msg_and_raise_error(err_msg, name);
}

/*
 * Mark a job structure as unused.
 */
static void
freejob(struct job *jp)
{
	struct procstat *ps;
	int i;

	INT_OFF;
	for (i = jp->nprocs, ps = jp->ps; --i >= 0; ps++) {
		if (ps->cmd != nullstr)
			free(ps->cmd);
	}
	if (jp->ps != &jp->ps0)
		free(jp->ps);
	jp->used = 0;
	set_curjob(jp, CUR_DELETE);
	INT_ON;
}

#if JOBS
static void
xtcsetpgrp(int fd, pid_t pgrp)
{
	if (tcsetpgrp(fd, pgrp))
		ash_msg_and_raise_error("cannot set tty process group (%m)");
}

/*
 * Turn job control on and off.
 *
 * Note:  This code assumes that the third arg to ioctl is a character
 * pointer, which is true on Berkeley systems but not System V.  Since
 * System V doesn't have job control yet, this isn't a problem now.
 *
 * Called with interrupts off.
 */
static void
setjobctl(int on)
{
	int fd;
	int pgrp;

	if (on == doing_jobctl || rootshell == 0)
		return;
	if (on) {
		int ofd;
		ofd = fd = open(_PATH_TTY, O_RDWR);
		if (fd < 0) {
	/* BTW, bash will try to open(ttyname(0)) if open("/dev/tty") fails.
	 * That sometimes helps to acquire controlling tty.
	 * Obviously, a workaround for bugs when someone
	 * failed to provide a controlling tty to bash! :) */
			fd = 2;
			while (!isatty(fd))
				if (--fd < 0)
					goto out;
		}
		fd = fcntl(fd, F_DUPFD, 10);
		if (ofd >= 0)
			close(ofd);
		if (fd < 0)
			goto out;
		/* fd is a tty at this point */
		close_on_exec_on(fd);
		do { /* while we are in the background */
			pgrp = tcgetpgrp(fd);
			if (pgrp < 0) {
 out:
				ash_msg("can't access tty; job control turned off");
				mflag = on = 0;
				goto close;
			}
			if (pgrp == getpgrp())
				break;
			killpg(0, SIGTTIN);
		} while (1);
		initialpgrp = pgrp;

		setsignal(SIGTSTP);
		setsignal(SIGTTOU);
		setsignal(SIGTTIN);
		pgrp = rootpid;
		setpgid(0, pgrp);
		xtcsetpgrp(fd, pgrp);
	} else {
		/* turning job control off */
		fd = ttyfd;
		pgrp = initialpgrp;
		/* was xtcsetpgrp, but this can make exiting ash
		 * loop forever if pty is already deleted */
		tcsetpgrp(fd, pgrp);
		setpgid(0, pgrp);
		setsignal(SIGTSTP);
		setsignal(SIGTTOU);
		setsignal(SIGTTIN);
 close:
		if (fd >= 0)
			close(fd);
		fd = -1;
	}
	ttyfd = fd;
	doing_jobctl = on;
}

static int
killcmd(int argc, char **argv)
{
	int i = 1;
	if (argv[1] && strcmp(argv[1], "-l") != 0) {
		do {
			if (argv[i][0] == '%') {
				struct job *jp = getjob(argv[i], 0);
				unsigned pid = jp->ps[0].pid;
				/* Enough space for ' -NNN<nul>' */
				argv[i] = alloca(sizeof(int)*3 + 3);
				/* kill_main has matching code to expect
				 * leading space. Needed to not confuse
				 * negative pids with "kill -SIGNAL_NO" syntax */
				sprintf(argv[i], " -%u", pid);
			}
		} while (argv[++i]);
	}
	return kill_main(argc, argv);
}

static void
showpipe(struct job *jp, FILE *out)
{
	struct procstat *sp;
	struct procstat *spend;

	spend = jp->ps + jp->nprocs;
	for (sp = jp->ps + 1; sp < spend; sp++)
		fprintf(out, " | %s", sp->cmd);
	outcslow('\n', out);
	flush_stdout_stderr();
}


static int
restartjob(struct job *jp, int mode)
{
	struct procstat *ps;
	int i;
	int status;
	pid_t pgid;

	INT_OFF;
	if (jp->state == JOBDONE)
		goto out;
	jp->state = JOBRUNNING;
	pgid = jp->ps->pid;
	if (mode == FORK_FG)
		xtcsetpgrp(ttyfd, pgid);
	killpg(pgid, SIGCONT);
	ps = jp->ps;
	i = jp->nprocs;
	do {
		if (WIFSTOPPED(ps->status)) {
			ps->status = -1;
		}
		ps++;
	} while (--i);
 out:
	status = (mode == FORK_FG) ? waitforjob(jp) : 0;
	INT_ON;
	return status;
}

static int
fg_bgcmd(int argc ATTRIBUTE_UNUSED, char **argv)
{
	struct job *jp;
	FILE *out;
	int mode;
	int retval;

	mode = (**argv == 'f') ? FORK_FG : FORK_BG;
	nextopt(nullstr);
	argv = argptr;
	out = stdout;
	do {
		jp = getjob(*argv, 1);
		if (mode == FORK_BG) {
			set_curjob(jp, CUR_RUNNING);
			fprintf(out, "[%d] ", jobno(jp));
		}
		outstr(jp->ps->cmd, out);
		showpipe(jp, out);
		retval = restartjob(jp, mode);
	} while (*argv && *++argv);
	return retval;
}
#endif

static int
sprint_status(char *s, int status, int sigonly)
{
	int col;
	int st;

	col = 0;
	if (!WIFEXITED(status)) {
#if JOBS
		if (WIFSTOPPED(status))
			st = WSTOPSIG(status);
		else
#endif
			st = WTERMSIG(status);
		if (sigonly) {
			if (st == SIGINT || st == SIGPIPE)
				goto out;
#if JOBS
			if (WIFSTOPPED(status))
				goto out;
#endif
		}
		st &= 0x7f;
		col = fmtstr(s, 32, strsignal(st));
		if (WCOREDUMP(status)) {
			col += fmtstr(s + col, 16, " (core dumped)");
		}
	} else if (!sigonly) {
		st = WEXITSTATUS(status);
		if (st)
			col = fmtstr(s, 16, "Done(%d)", st);
		else
			col = fmtstr(s, 16, "Done");
	}
 out:
	return col;
}

/*
 * Do a wait system call.  If job control is compiled in, we accept
 * stopped processes.  If block is zero, we return a value of zero
 * rather than blocking.
 *
 * System V doesn't have a non-blocking wait system call.  It does
 * have a SIGCLD signal that is sent to a process when one of it's
 * children dies.  The obvious way to use SIGCLD would be to install
 * a handler for SIGCLD which simply bumped a counter when a SIGCLD
 * was received, and have waitproc bump another counter when it got
 * the status of a process.  Waitproc would then know that a wait
 * system call would not block if the two counters were different.
 * This approach doesn't work because if a process has children that
 * have not been waited for, System V will send it a SIGCLD when it
 * installs a signal handler for SIGCLD.  What this means is that when
 * a child exits, the shell will be sent SIGCLD signals continuously
 * until is runs out of stack space, unless it does a wait call before
 * restoring the signal handler.  The code below takes advantage of
 * this (mis)feature by installing a signal handler for SIGCLD and
 * then checking to see whether it was called.  If there are any
 * children to be waited for, it will be.
 *
 * If neither SYSV nor BSD is defined, we don't implement nonblocking
 * waits at all.  In this case, the user will not be informed when
 * a background process until the next time she runs a real program
 * (as opposed to running a builtin command or just typing return),
 * and the jobs command may give out of date information.
 */
static int
waitproc(int wait_flags, int *status)
{
#if JOBS
	if (doing_jobctl)
		wait_flags |= WUNTRACED;
#endif
	/* NB: _not_ safe_waitpid, we need to detect EINTR */
	return waitpid(-1, status, wait_flags);
}

/*
 * Wait for a process to terminate.
 */
static int
dowait(int wait_flags, struct job *job)
{
	int pid;
	int status;
	struct job *jp;
	struct job *thisjob;
	int state;

	TRACE(("dowait(%d) called\n", wait_flags));
	pid = waitproc(wait_flags, &status);
	TRACE(("wait returns pid=%d, status=%d\n", pid, status));
	if (pid <= 0) {
		/* If we were doing blocking wait and (probably) got EINTR,
		 * check for pending sigs received while waiting.
		 * (NB: can be moved into callers if needed) */
		if (wait_flags == DOWAIT_BLOCK && pendingsig)
			raise_exception(EXSIG);
		return pid;
	}
	INT_OFF;
	thisjob = NULL;
	for (jp = curjob; jp; jp = jp->prev_job) {
		struct procstat *sp;
		struct procstat *spend;
		if (jp->state == JOBDONE)
			continue;
		state = JOBDONE;
		spend = jp->ps + jp->nprocs;
		sp = jp->ps;
		do {
			if (sp->pid == pid) {
				TRACE(("Job %d: changing status of proc %d "
					"from 0x%x to 0x%x\n",
					jobno(jp), pid, sp->status, status));
				sp->status = status;
				thisjob = jp;
			}
			if (sp->status == -1)
				state = JOBRUNNING;
#if JOBS
			if (state == JOBRUNNING)
				continue;
			if (WIFSTOPPED(sp->status)) {
				jp->stopstatus = sp->status;
				state = JOBSTOPPED;
			}
#endif
		} while (++sp < spend);
		if (thisjob)
			goto gotjob;
	}
#if JOBS
	if (!WIFSTOPPED(status))
#endif
		jobless--;
	goto out;

 gotjob:
	if (state != JOBRUNNING) {
		thisjob->changed = 1;

		if (thisjob->state != state) {
			TRACE(("Job %d: changing state from %d to %d\n",
				jobno(thisjob), thisjob->state, state));
			thisjob->state = state;
#if JOBS
			if (state == JOBSTOPPED) {
				set_curjob(thisjob, CUR_STOPPED);
			}
#endif
		}
	}

 out:
	INT_ON;

	if (thisjob && thisjob == job) {
		char s[48 + 1];
		int len;

		len = sprint_status(s, status, 1);
		if (len) {
			s[len] = '\n';
			s[len + 1] = '\0';
			out2str(s);
		}
	}
	return pid;
}

#if JOBS
static void
showjob(FILE *out, struct job *jp, int mode)
{
	struct procstat *ps;
	struct procstat *psend;
	int col;
	int indent_col;
	char s[80];

	ps = jp->ps;

	if (mode & SHOW_PGID) {
		/* just output process (group) id of pipeline */
		fprintf(out, "%d\n", ps->pid);
		return;
	}

	col = fmtstr(s, 16, "[%d]   ", jobno(jp));
	indent_col = col;

	if (jp == curjob)
		s[col - 2] = '+';
	else if (curjob && jp == curjob->prev_job)
		s[col - 2] = '-';

	if (mode & SHOW_PID)
		col += fmtstr(s + col, 16, "%d ", ps->pid);

	psend = ps + jp->nprocs;

	if (jp->state == JOBRUNNING) {
		strcpy(s + col, "Running");
		col += sizeof("Running") - 1;
	} else {
		int status = psend[-1].status;
		if (jp->state == JOBSTOPPED)
			status = jp->stopstatus;
		col += sprint_status(s + col, status, 0);
	}

	goto start;

	do {
		/* for each process */
		col = fmtstr(s, 48, " |\n%*c%d ", indent_col, ' ', ps->pid) - 3;
 start:
		fprintf(out, "%s%*c%s",
			s, 33 - col >= 0 ? 33 - col : 0, ' ', ps->cmd
		);
		if (!(mode & SHOW_PID)) {
			showpipe(jp, out);
			break;
		}
		if (++ps == psend) {
			outcslow('\n', out);
			break;
		}
	} while (1);

	jp->changed = 0;

	if (jp->state == JOBDONE) {
		TRACE(("showjob: freeing job %d\n", jobno(jp)));
		freejob(jp);
	}
}

/*
 * Print a list of jobs.  If "change" is nonzero, only print jobs whose
 * statuses have changed since the last call to showjobs.
 */
static void
showjobs(FILE *out, int mode)
{
	struct job *jp;

	TRACE(("showjobs(%x) called\n", mode));

	/* If not even one job changed, there is nothing to do */
	while (dowait(DOWAIT_NONBLOCK, NULL) > 0)
		continue;

	for (jp = curjob; jp; jp = jp->prev_job) {
		if (!(mode & SHOW_CHANGED) || jp->changed) {
			showjob(out, jp, mode);
		}
	}
}

static int
jobscmd(int argc ATTRIBUTE_UNUSED, char **argv)
{
	int mode, m;

	mode = 0;
	while ((m = nextopt("lp"))) {
		if (m == 'l')
			mode = SHOW_PID;
		else
			mode = SHOW_PGID;
	}

	argv = argptr;
	if (*argv) {
		do
			showjob(stdout, getjob(*argv,0), mode);
		while (*++argv);
	} else
		showjobs(stdout, mode);

	return 0;
}
#endif /* JOBS */

static int
getstatus(struct job *job)
{
	int status;
	int retval;

	status = job->ps[job->nprocs - 1].status;
	retval = WEXITSTATUS(status);
	if (!WIFEXITED(status)) {
#if JOBS
		retval = WSTOPSIG(status);
		if (!WIFSTOPPED(status))
#endif
		{
			/* XXX: limits number of signals */
			retval = WTERMSIG(status);
#if JOBS
			if (retval == SIGINT)
				job->sigint = 1;
#endif
		}
		retval += 128;
	}
	TRACE(("getstatus: job %d, nproc %d, status %x, retval %x\n",
		jobno(job), job->nprocs, status, retval));
	return retval;
}

static int
waitcmd(int argc ATTRIBUTE_UNUSED, char **argv)
{
	struct job *job;
	int retval;
	struct job *jp;

//	exsig++;
//	xbarrier();
	if (pendingsig)
		raise_exception(EXSIG);

	nextopt(nullstr);
	retval = 0;

	argv = argptr;
	if (!*argv) {
		/* wait for all jobs */
		for (;;) {
			jp = curjob;
			while (1) {
				if (!jp) /* no running procs */
					goto ret;
				if (jp->state == JOBRUNNING)
					break;
				jp->waited = 1;
				jp = jp->prev_job;
			}
			dowait(DOWAIT_BLOCK, NULL);
		}
	}

	retval = 127;
	do {
		if (**argv != '%') {
			pid_t pid = number(*argv);
			job = curjob;
			while (1) {
				if (!job)
					goto repeat;
				if (job->ps[job->nprocs - 1].pid == pid)
					break;
				job = job->prev_job;
			}
		} else
			job = getjob(*argv, 0);
		/* loop until process terminated or stopped */
		while (job->state == JOBRUNNING)
			dowait(DOWAIT_BLOCK, NULL);
		job->waited = 1;
		retval = getstatus(job);
 repeat:
		;
	} while (*++argv);

 ret:
	return retval;
}

static struct job *
growjobtab(void)
{
	size_t len;
	ptrdiff_t offset;
	struct job *jp, *jq;

	len = njobs * sizeof(*jp);
	jq = jobtab;
	jp = ckrealloc(jq, len + 4 * sizeof(*jp));

	offset = (char *)jp - (char *)jq;
	if (offset) {
		/* Relocate pointers */
		size_t l = len;

		jq = (struct job *)((char *)jq + l);
		while (l) {
			l -= sizeof(*jp);
			jq--;
#define joff(p) ((struct job *)((char *)(p) + l))
#define jmove(p) (p) = (void *)((char *)(p) + offset)
			if (joff(jp)->ps == &jq->ps0)
				jmove(joff(jp)->ps);
			if (joff(jp)->prev_job)
				jmove(joff(jp)->prev_job);
		}
		if (curjob)
			jmove(curjob);
#undef joff
#undef jmove
	}

	njobs += 4;
	jobtab = jp;
	jp = (struct job *)((char *)jp + len);
	jq = jp + 3;
	do {
		jq->used = 0;
	} while (--jq >= jp);
	return jp;
}

/*
 * Return a new job structure.
 * Called with interrupts off.
 */
static struct job *
makejob(/*union node *node,*/ int nprocs)
{
	int i;
	struct job *jp;

	for (i = njobs, jp = jobtab; ; jp++) {
		if (--i < 0) {
			jp = growjobtab();
			break;
		}
		if (jp->used == 0)
			break;
		if (jp->state != JOBDONE || !jp->waited)
			continue;
#if JOBS
		if (doing_jobctl)
			continue;
#endif
		freejob(jp);
		break;
	}
	memset(jp, 0, sizeof(*jp));
#if JOBS
	/* jp->jobctl is a bitfield.
	 * "jp->jobctl |= jobctl" likely to give awful code */
	if (doing_jobctl)
		jp->jobctl = 1;
#endif
	jp->prev_job = curjob;
	curjob = jp;
	jp->used = 1;
	jp->ps = &jp->ps0;
	if (nprocs > 1) {
		jp->ps = ckmalloc(nprocs * sizeof(struct procstat));
	}
	TRACE(("makejob(%d) returns %%%d\n", nprocs,
				jobno(jp)));
	return jp;
}

#if JOBS
/*
 * Return a string identifying a command (to be printed by the
 * jobs command).
 */
static char *cmdnextc;

static void
cmdputs(const char *s)
{
	static const char vstype[VSTYPE + 1][3] = {
		"", "}", "-", "+", "?", "=",
		"%", "%%", "#", "##"
		USE_ASH_BASH_COMPAT(, ":", "/", "//")
	};

	const char *p, *str;
	char c, cc[2] = " ";
	char *nextc;
	int subtype = 0;
	int quoted = 0;

	nextc = makestrspace((strlen(s) + 1) * 8, cmdnextc);
	p = s;
	while ((c = *p++) != 0) {
		str = NULL;
		switch (c) {
		case CTLESC:
			c = *p++;
			break;
		case CTLVAR:
			subtype = *p++;
			if ((subtype & VSTYPE) == VSLENGTH)
				str = "${#";
			else
				str = "${";
			if (!(subtype & VSQUOTE) == !(quoted & 1))
				goto dostr;
			quoted ^= 1;
			c = '"';
			break;
		case CTLENDVAR:
			str = "\"}" + !(quoted & 1);
			quoted >>= 1;
			subtype = 0;
			goto dostr;
		case CTLBACKQ:
			str = "$(...)";
			goto dostr;
		case CTLBACKQ+CTLQUOTE:
			str = "\"$(...)\"";
			goto dostr;
#if ENABLE_ASH_MATH_SUPPORT
		case CTLARI:
			str = "$((";
			goto dostr;
		case CTLENDARI:
			str = "))";
			goto dostr;
#endif
		case CTLQUOTEMARK:
			quoted ^= 1;
			c = '"';
			break;
		case '=':
			if (subtype == 0)
				break;
			if ((subtype & VSTYPE) != VSNORMAL)
				quoted <<= 1;
			str = vstype[subtype & VSTYPE];
			if (subtype & VSNUL)
				c = ':';
			else
				goto checkstr;
			break;
		case '\'':
		case '\\':
		case '"':
		case '$':
			/* These can only happen inside quotes */
			cc[0] = c;
			str = cc;
			c = '\\';
			break;
		default:
			break;
		}
		USTPUTC(c, nextc);
 checkstr:
		if (!str)
			continue;
 dostr:
		while ((c = *str++)) {
			USTPUTC(c, nextc);
		}
	}
	if (quoted & 1) {
		USTPUTC('"', nextc);
	}
	*nextc = 0;
	cmdnextc = nextc;
}

/* cmdtxt() and cmdlist() call each other */
static void cmdtxt(union node *n);

static void
cmdlist(union node *np, int sep)
{
	for (; np; np = np->narg.next) {
		if (!sep)
			cmdputs(" ");
		cmdtxt(np);
		if (sep && np->narg.next)
			cmdputs(" ");
	}
}

static void
cmdtxt(union node *n)
{
	union node *np;
	struct nodelist *lp;
	const char *p;
	char s[2];

	if (!n)
		return;
	switch (n->type) {
	default:
#if DEBUG
		abort();
#endif
	case NPIPE:
		lp = n->npipe.cmdlist;
		for (;;) {
			cmdtxt(lp->n);
			lp = lp->next;
			if (!lp)
				break;
			cmdputs(" | ");
		}
		break;
	case NSEMI:
		p = "; ";
		goto binop;
	case NAND:
		p = " && ";
		goto binop;
	case NOR:
		p = " || ";
 binop:
		cmdtxt(n->nbinary.ch1);
		cmdputs(p);
		n = n->nbinary.ch2;
		goto donode;
	case NREDIR:
	case NBACKGND:
		n = n->nredir.n;
		goto donode;
	case NNOT:
		cmdputs("!");
		n = n->nnot.com;
 donode:
		cmdtxt(n);
		break;
	case NIF:
		cmdputs("if ");
		cmdtxt(n->nif.test);
		cmdputs("; then ");
		n = n->nif.ifpart;
		if (n->nif.elsepart) {
			cmdtxt(n);
			cmdputs("; else ");
			n = n->nif.elsepart;
		}
		p = "; fi";
		goto dotail;
	case NSUBSHELL:
		cmdputs("(");
		n = n->nredir.n;
		p = ")";
		goto dotail;
	case NWHILE:
		p = "while ";
		goto until;
	case NUNTIL:
		p = "until ";
 until:
		cmdputs(p);
		cmdtxt(n->nbinary.ch1);
		n = n->nbinary.ch2;
		p = "; done";
 dodo:
		cmdputs("; do ");
 dotail:
		cmdtxt(n);
		goto dotail2;
	case NFOR:
		cmdputs("for ");
		cmdputs(n->nfor.var);
		cmdputs(" in ");
		cmdlist(n->nfor.args, 1);
		n = n->nfor.body;
		p = "; done";
		goto dodo;
	case NDEFUN:
		cmdputs(n->narg.text);
		p = "() { ... }";
		goto dotail2;
	case NCMD:
		cmdlist(n->ncmd.args, 1);
		cmdlist(n->ncmd.redirect, 0);
		break;
	case NARG:
		p = n->narg.text;
 dotail2:
		cmdputs(p);
		break;
	case NHERE:
	case NXHERE:
		p = "<<...";
		goto dotail2;
	case NCASE:
		cmdputs("case ");
		cmdputs(n->ncase.expr->narg.text);
		cmdputs(" in ");
		for (np = n->ncase.cases; np; np = np->nclist.next) {
			cmdtxt(np->nclist.pattern);
			cmdputs(") ");
			cmdtxt(np->nclist.body);
			cmdputs(";; ");
		}
		p = "esac";
		goto dotail2;
	case NTO:
		p = ">";
		goto redir;
	case NCLOBBER:
		p = ">|";
		goto redir;
	case NAPPEND:
		p = ">>";
		goto redir;
	case NTOFD:
		p = ">&";
		goto redir;
	case NFROM:
		p = "<";
		goto redir;
	case NFROMFD:
		p = "<&";
		goto redir;
	case NFROMTO:
		p = "<>";
 redir:
		s[0] = n->nfile.fd + '0';
		s[1] = '\0';
		cmdputs(s);
		cmdputs(p);
		if (n->type == NTOFD || n->type == NFROMFD) {
			s[0] = n->ndup.dupfd + '0';
			p = s;
			goto dotail2;
		}
		n = n->nfile.fname;
		goto donode;
	}
}

static char *
commandtext(union node *n)
{
	char *name;

	STARTSTACKSTR(cmdnextc);
	cmdtxt(n);
	name = stackblock();
	TRACE(("commandtext: name %p, end %p\n\t\"%s\"\n",
			name, cmdnextc, cmdnextc));
	return ckstrdup(name);
}
#endif /* JOBS */

/*
 * Fork off a subshell.  If we are doing job control, give the subshell its
 * own process group.  Jp is a job structure that the job is to be added to.
 * N is the command that will be evaluated by the child.  Both jp and n may
 * be NULL.  The mode parameter can be one of the following:
 *      FORK_FG - Fork off a foreground process.
 *      FORK_BG - Fork off a background process.
 *      FORK_NOJOB - Like FORK_FG, but don't give the process its own
 *                   process group even if job control is on.
 *
 * When job control is turned off, background processes have their standard
 * input redirected to /dev/null (except for the second and later processes
 * in a pipeline).
 *
 * Called with interrupts off.
 */
/*
 * Clear traps on a fork.
 */
static void
clear_traps(void)
{
	char **tp;

	for (tp = trap; tp < &trap[NSIG]; tp++) {
		if (*tp && **tp) {      /* trap not NULL or "" (SIG_IGN) */
			INT_OFF;
			free(*tp);
			*tp = NULL;
			if (tp != &trap[0])
				setsignal(tp - trap);
			INT_ON;
		}
	}
}

/* Lives far away from here, needed for forkchild */
static void closescript(void);

/* Called after fork(), in child */
static void
forkchild(struct job *jp, /*union node *n,*/ int mode)
{
	int oldlvl;

	TRACE(("Child shell %d\n", getpid()));
	oldlvl = shlvl;
	shlvl++;

	closescript();
	clear_traps();
#if JOBS
	/* do job control only in root shell */
	doing_jobctl = 0;
	if (mode != FORK_NOJOB && jp->jobctl && !oldlvl) {
		pid_t pgrp;

		if (jp->nprocs == 0)
			pgrp = getpid();
		else
			pgrp = jp->ps[0].pid;
		/* This can fail because we are doing it in the parent also */
		(void)setpgid(0, pgrp);
		if (mode == FORK_FG)
			xtcsetpgrp(ttyfd, pgrp);
		setsignal(SIGTSTP);
		setsignal(SIGTTOU);
	} else
#endif
	if (mode == FORK_BG) {
		ignoresig(SIGINT);
		ignoresig(SIGQUIT);
		if (jp->nprocs == 0) {
			close(0);
			if (open(bb_dev_null, O_RDONLY) != 0)
				ash_msg_and_raise_error("can't open %s", bb_dev_null);
		}
	}
	if (!oldlvl && iflag) {
		setsignal(SIGINT);
		setsignal(SIGQUIT);
		setsignal(SIGTERM);
	}
	for (jp = curjob; jp; jp = jp->prev_job)
		freejob(jp);
	jobless = 0;
}

/* Called after fork(), in parent */
#if !JOBS
#define forkparent(jp, n, mode, pid) forkparent(jp, mode, pid)
#endif
static void
forkparent(struct job *jp, union node *n, int mode, pid_t pid)
{
	TRACE(("In parent shell: child = %d\n", pid));
	if (!jp) {
		while (jobless && dowait(DOWAIT_NONBLOCK, NULL) > 0)
			continue;
		jobless++;
		return;
	}
#if JOBS
	if (mode != FORK_NOJOB && jp->jobctl) {
		int pgrp;

		if (jp->nprocs == 0)
			pgrp = pid;
		else
			pgrp = jp->ps[0].pid;
		/* This can fail because we are doing it in the child also */
		setpgid(pid, pgrp);
	}
#endif
	if (mode == FORK_BG) {
		backgndpid = pid;               /* set $! */
		set_curjob(jp, CUR_RUNNING);
	}
	if (jp) {
		struct procstat *ps = &jp->ps[jp->nprocs++];
		ps->pid = pid;
		ps->status = -1;
		ps->cmd = nullstr;
#if JOBS
		if (doing_jobctl && n)
			ps->cmd = commandtext(n);
#endif
	}
}

static int
forkshell(struct job *jp, union node *n, int mode)
{
	int pid;

	TRACE(("forkshell(%%%d, %p, %d) called\n", jobno(jp), n, mode));
	pid = fork();
	if (pid < 0) {
		TRACE(("Fork failed, errno=%d", errno));
		if (jp)
			freejob(jp);
		ash_msg_and_raise_error("cannot fork");
	}
	if (pid == 0)
		forkchild(jp, /*n,*/ mode);
	else
		forkparent(jp, n, mode, pid);
	return pid;
}

/*
 * Wait for job to finish.
 *
 * Under job control we have the problem that while a child process is
 * running interrupts generated by the user are sent to the child but not
 * to the shell.  This means that an infinite loop started by an inter-
 * active user may be hard to kill.  With job control turned off, an
 * interactive user may place an interactive program inside a loop.  If
 * the interactive program catches interrupts, the user doesn't want
 * these interrupts to also abort the loop.  The approach we take here
 * is to have the shell ignore interrupt signals while waiting for a
 * foreground process to terminate, and then send itself an interrupt
 * signal if the child process was terminated by an interrupt signal.
 * Unfortunately, some programs want to do a bit of cleanup and then
 * exit on interrupt; unless these processes terminate themselves by
 * sending a signal to themselves (instead of calling exit) they will
 * confuse this approach.
 *
 * Called with interrupts off.
 */
static int
waitforjob(struct job *jp)
{
	int st;

	TRACE(("waitforjob(%%%d) called\n", jobno(jp)));
	while (jp->state == JOBRUNNING) {
		dowait(DOWAIT_BLOCK, jp);
	}
	st = getstatus(jp);
#if JOBS
	if (jp->jobctl) {
		xtcsetpgrp(ttyfd, rootpid);
		/*
		 * This is truly gross.
		 * If we're doing job control, then we did a TIOCSPGRP which
		 * caused us (the shell) to no longer be in the controlling
		 * session -- so we wouldn't have seen any ^C/SIGINT.  So, we
		 * intuit from the subprocess exit status whether a SIGINT
		 * occurred, and if so interrupt ourselves.  Yuck.  - mycroft
		 */
		if (jp->sigint) /* TODO: do the same with all signals */
			raise(SIGINT); /* ... by raise(jp->sig) instead? */
	}
	if (jp->state == JOBDONE)
#endif
		freejob(jp);
	return st;
}

/*
 * return 1 if there are stopped jobs, otherwise 0
 */
static int
stoppedjobs(void)
{
	struct job *jp;
	int retval;

	retval = 0;
	if (job_warning)
		goto out;
	jp = curjob;
	if (jp && jp->state == JOBSTOPPED) {
		out2str("You have stopped jobs.\n");
		job_warning = 2;
		retval++;
	}
 out:
	return retval;
}


/* ============ redir.c
 *
 * Code for dealing with input/output redirection.
 */

#define EMPTY -2                /* marks an unused slot in redirtab */
#define CLOSED -3               /* marks a slot of previously-closed fd */
#ifndef PIPE_BUF
# define PIPESIZE 4096          /* amount of buffering in a pipe */
#else
# define PIPESIZE PIPE_BUF
#endif

/*
 * Open a file in noclobber mode.
 * The code was copied from bash.
 */
static int
noclobberopen(const char *fname)
{
	int r, fd;
	struct stat finfo, finfo2;

	/*
	 * If the file exists and is a regular file, return an error
	 * immediately.
	 */
	r = stat(fname, &finfo);
	if (r == 0 && S_ISREG(finfo.st_mode)) {
		errno = EEXIST;
		return -1;
	}

	/*
	 * If the file was not present (r != 0), make sure we open it
	 * exclusively so that if it is created before we open it, our open
	 * will fail.  Make sure that we do not truncate an existing file.
	 * Note that we don't turn on O_EXCL unless the stat failed -- if the
	 * file was not a regular file, we leave O_EXCL off.
	 */
	if (r != 0)
		return open(fname, O_WRONLY|O_CREAT|O_EXCL, 0666);
	fd = open(fname, O_WRONLY|O_CREAT, 0666);

	/* If the open failed, return the file descriptor right away. */
	if (fd < 0)
		return fd;

	/*
	 * OK, the open succeeded, but the file may have been changed from a
	 * non-regular file to a regular file between the stat and the open.
	 * We are assuming that the O_EXCL open handles the case where FILENAME
	 * did not exist and is symlinked to an existing file between the stat
	 * and open.
	 */

	/*
	 * If we can open it and fstat the file descriptor, and neither check
	 * revealed that it was a regular file, and the file has not been
	 * replaced, return the file descriptor.
	 */
	if (fstat(fd, &finfo2) == 0 && !S_ISREG(finfo2.st_mode)
	 && finfo.st_dev == finfo2.st_dev && finfo.st_ino == finfo2.st_ino)
		return fd;

	/* The file has been replaced.  badness. */
	close(fd);
	errno = EEXIST;
	return -1;
}

/*
 * Handle here documents.  Normally we fork off a process to write the
 * data to a pipe.  If the document is short, we can stuff the data in
 * the pipe without forking.
 */
/* openhere needs this forward reference */
static void expandhere(union node *arg, int fd);
static int
openhere(union node *redir)
{
	int pip[2];
	size_t len = 0;

	if (pipe(pip) < 0)
		ash_msg_and_raise_error("pipe call failed");
	if (redir->type == NHERE) {
		len = strlen(redir->nhere.doc->narg.text);
		if (len <= PIPESIZE) {
			full_write(pip[1], redir->nhere.doc->narg.text, len);
			goto out;
		}
	}
	if (forkshell((struct job *)NULL, (union node *)NULL, FORK_NOJOB) == 0) {
		close(pip[0]);
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		signal(SIGHUP, SIG_IGN);
#ifdef SIGTSTP
		signal(SIGTSTP, SIG_IGN);
#endif
		signal(SIGPIPE, SIG_DFL);
		if (redir->type == NHERE)
			full_write(pip[1], redir->nhere.doc->narg.text, len);
		else
			expandhere(redir->nhere.doc, pip[1]);
		_exit(EXIT_SUCCESS);
	}
 out:
	close(pip[1]);
	return pip[0];
}

static int
openredirect(union node *redir)
{
	char *fname;
	int f;

	switch (redir->nfile.type) {
	case NFROM:
		fname = redir->nfile.expfname;
		f = open(fname, O_RDONLY);
		if (f < 0)
			goto eopen;
		break;
	case NFROMTO:
		fname = redir->nfile.expfname;
		f = open(fname, O_RDWR|O_CREAT|O_TRUNC, 0666);
		if (f < 0)
			goto ecreate;
		break;
	case NTO:
		/* Take care of noclobber mode. */
		if (Cflag) {
			fname = redir->nfile.expfname;
			f = noclobberopen(fname);
			if (f < 0)
				goto ecreate;
			break;
		}
		/* FALLTHROUGH */
	case NCLOBBER:
		fname = redir->nfile.expfname;
		f = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0666);
		if (f < 0)
			goto ecreate;
		break;
	case NAPPEND:
		fname = redir->nfile.expfname;
		f = open(fname, O_WRONLY|O_CREAT|O_APPEND, 0666);
		if (f < 0)
			goto ecreate;
		break;
	default:
#if DEBUG
		abort();
#endif
		/* Fall through to eliminate warning. */
	case NTOFD:
	case NFROMFD:
		f = -1;
		break;
	case NHERE:
	case NXHERE:
		f = openhere(redir);
		break;
	}

	return f;
 ecreate:
	ash_msg_and_raise_error("cannot create %s: %s", fname, errmsg(errno, "nonexistent directory"));
 eopen:
	ash_msg_and_raise_error("cannot open %s: %s", fname, errmsg(errno, "no such file"));
}

/*
 * Copy a file descriptor to be >= to.  Returns -1
 * if the source file descriptor is closed, EMPTY if there are no unused
 * file descriptors left.
 */
static int
copyfd(int from, int to)
{
	int newfd;

	newfd = fcntl(from, F_DUPFD, to);
	if (newfd < 0) {
		if (errno == EMFILE)
			return EMPTY;
		ash_msg_and_raise_error("%d: %m", from);
	}
	return newfd;
}

static void
dupredirect(union node *redir, int f)
{
	int fd = redir->nfile.fd;

	if (redir->nfile.type == NTOFD || redir->nfile.type == NFROMFD) {
		if (redir->ndup.dupfd >= 0) {   /* if not ">&-" */
			copyfd(redir->ndup.dupfd, fd);
		}
		return;
	}

	if (f != fd) {
		copyfd(f, fd);
		close(f);
	}
}

/*
 * Process a list of redirection commands.  If the REDIR_PUSH flag is set,
 * old file descriptors are stashed away so that the redirection can be
 * undone by calling popredir.  If the REDIR_BACKQ flag is set, then the
 * standard output, and the standard error if it becomes a duplicate of
 * stdout, is saved in memory.
 */
/* flags passed to redirect */
#define REDIR_PUSH    01        /* save previous values of file descriptors */
#define REDIR_SAVEFD2 03        /* set preverrout */
static void
redirect(union node *redir, int flags)
{
	union node *n;
	struct redirtab *sv;
	int i;
	int fd;
	int newfd;

	g_nullredirs++;
	if (!redir) {
		return;
	}
	sv = NULL;
	INT_OFF;
	if (flags & REDIR_PUSH) {
		sv = ckmalloc(sizeof(*sv));
		sv->next = redirlist;
		redirlist = sv;
		sv->nullredirs = g_nullredirs - 1;
		for (i = 0; i < 10; i++)
			sv->renamed[i] = EMPTY;
		g_nullredirs = 0;
	}
	n = redir;
	do {
		fd = n->nfile.fd;
		if ((n->nfile.type == NTOFD || n->nfile.type == NFROMFD)
		 && n->ndup.dupfd == fd)
			continue; /* redirect from/to same file descriptor */

		newfd = openredirect(n);
		if (fd == newfd) {
			/* Descriptor wasn't open before redirect.
			 * Mark it for close in the future */
			if (sv && sv->renamed[fd] == EMPTY)
				sv->renamed[fd] = CLOSED;
			continue;
		}
		if (sv && sv->renamed[fd] == EMPTY) {
			i = fcntl(fd, F_DUPFD, 10);

			if (i == -1) {
				i = errno;
				if (i != EBADF) {
					close(newfd);
					errno = i;
					ash_msg_and_raise_error("%d: %m", fd);
					/* NOTREACHED */
				}
			} else {
				sv->renamed[fd] = i;
				close(fd);
			}
		} else {
			close(fd);
		}
		dupredirect(n, newfd);
	} while ((n = n->nfile.next));
	INT_ON;
	if ((flags & REDIR_SAVEFD2) && sv && sv->renamed[2] >= 0)
		preverrout_fd = sv->renamed[2];
}

/*
 * Undo the effects of the last redirection.
 */
static void
popredir(int drop)
{
	struct redirtab *rp;
	int i;

	if (--g_nullredirs >= 0)
		return;
	INT_OFF;
	rp = redirlist;
	for (i = 0; i < 10; i++) {
		if (rp->renamed[i] == CLOSED) {
			if (!drop)
				close(i);
			continue;
		}
		if (rp->renamed[i] != EMPTY) {
			if (!drop) {
				close(i);
				copyfd(rp->renamed[i], i);
			}
			close(rp->renamed[i]);
		}
	}
	redirlist = rp->next;
	g_nullredirs = rp->nullredirs;
	free(rp);
	INT_ON;
}

/*
 * Undo all redirections.  Called on error or interrupt.
 */

/*
 * Discard all saved file descriptors.
 */
static void
clearredir(int drop)
{
	for (;;) {
		g_nullredirs = 0;
		if (!redirlist)
			break;
		popredir(drop);
	}
}

static int
redirectsafe(union node *redir, int flags)
{
	int err;
	volatile int saveint;
	struct jmploc *volatile savehandler = exception_handler;
	struct jmploc jmploc;

	SAVE_INT(saveint);
	err = setjmp(jmploc.loc) * 2;
	if (!err) {
		exception_handler = &jmploc;
		redirect(redir, flags);
	}
	exception_handler = savehandler;
	if (err && exception != EXERROR)
		longjmp(exception_handler->loc, 1);
	RESTORE_INT(saveint);
	return err;
}


/* ============ Routines to expand arguments to commands
 *
 * We have to deal with backquotes, shell variables, and file metacharacters.
 */

/*
 * expandarg flags
 */
#define EXP_FULL        0x1     /* perform word splitting & file globbing */
#define EXP_TILDE       0x2     /* do normal tilde expansion */
#define EXP_VARTILDE    0x4     /* expand tildes in an assignment */
#define EXP_REDIR       0x8     /* file glob for a redirection (1 match only) */
#define EXP_CASE        0x10    /* keeps quotes around for CASE pattern */
#define EXP_RECORD      0x20    /* need to record arguments for ifs breakup */
#define EXP_VARTILDE2   0x40    /* expand tildes after colons only */
#define EXP_WORD        0x80    /* expand word in parameter expansion */
#define EXP_QWORD       0x100   /* expand word in quoted parameter expansion */
/*
 * _rmescape() flags
 */
#define RMESCAPE_ALLOC  0x1     /* Allocate a new string */
#define RMESCAPE_GLOB   0x2     /* Add backslashes for glob */
#define RMESCAPE_QUOTED 0x4     /* Remove CTLESC unless in quotes */
#define RMESCAPE_GROW   0x8     /* Grow strings instead of stalloc */
#define RMESCAPE_HEAP   0x10    /* Malloc strings instead of stalloc */

/*
 * Structure specifying which parts of the string should be searched
 * for IFS characters.
 */
struct ifsregion {
	struct ifsregion *next; /* next region in list */
	int begoff;             /* offset of start of region */
	int endoff;             /* offset of end of region */
	int nulonly;            /* search for nul bytes only */
};

struct arglist {
	struct strlist *list;
	struct strlist **lastp;
};

/* output of current string */
static char *expdest;
/* list of back quote expressions */
static struct nodelist *argbackq;
/* first struct in list of ifs regions */
static struct ifsregion ifsfirst;
/* last struct in list */
static struct ifsregion *ifslastp;
/* holds expanded arg list */
static struct arglist exparg;

/*
 * Our own itoa().
 */
static int
cvtnum(arith_t num)
{
	int len;

	expdest = makestrspace(32, expdest);
#if ENABLE_ASH_MATH_SUPPORT_64
	len = fmtstr(expdest, 32, "%lld", (long long) num);
#else
	len = fmtstr(expdest, 32, "%ld", num);
#endif
	STADJUST(len, expdest);
	return len;
}

static size_t
esclen(const char *start, const char *p)
{
	size_t esc = 0;

	while (p > start && *--p == CTLESC) {
		esc++;
	}
	return esc;
}

/*
 * Remove any CTLESC characters from a string.
 */
static char *
_rmescapes(char *str, int flag)
{
	static const char qchars[] ALIGN1 = { CTLESC, CTLQUOTEMARK, '\0' };

	char *p, *q, *r;
	unsigned inquotes;
	int notescaped;
	int globbing;

	p = strpbrk(str, qchars);
	if (!p) {
		return str;
	}
	q = p;
	r = str;
	if (flag & RMESCAPE_ALLOC) {
		size_t len = p - str;
		size_t fulllen = len + strlen(p) + 1;

		if (flag & RMESCAPE_GROW) {
			r = makestrspace(fulllen, expdest);
		} else if (flag & RMESCAPE_HEAP) {
			r = ckmalloc(fulllen);
		} else {
			r = stalloc(fulllen);
		}
		q = r;
		if (len > 0) {
			q = (char *)memcpy(q, str, len) + len;
		}
	}
	inquotes = (flag & RMESCAPE_QUOTED) ^ RMESCAPE_QUOTED;
	globbing = flag & RMESCAPE_GLOB;
	notescaped = globbing;
	while (*p) {
		if (*p == CTLQUOTEMARK) {
			inquotes = ~inquotes;
			p++;
			notescaped = globbing;
			continue;
		}
		if (*p == '\\') {
			/* naked back slash */
			notescaped = 0;
			goto copy;
		}
		if (*p == CTLESC) {
			p++;
			if (notescaped && inquotes && *p != '/') {
				*q++ = '\\';
			}
		}
		notescaped = globbing;
 copy:
		*q++ = *p++;
	}
	*q = '\0';
	if (flag & RMESCAPE_GROW) {
		expdest = r;
		STADJUST(q - r + 1, expdest);
	}
	return r;
}
#define rmescapes(p) _rmescapes((p), 0)

#define pmatch(a, b) !fnmatch((a), (b), 0)

/*
 * Prepare a pattern for a expmeta (internal glob(3)) call.
 *
 * Returns an stalloced string.
 */
static char *
preglob(const char *pattern, int quoted, int flag)
{
	flag |= RMESCAPE_GLOB;
	if (quoted) {
		flag |= RMESCAPE_QUOTED;
	}
	return _rmescapes((char *)pattern, flag);
}

/*
 * Put a string on the stack.
 */
static void
memtodest(const char *p, size_t len, int syntax, int quotes)
{
	char *q = expdest;

	q = makestrspace(len * 2, q);

	while (len--) {
		int c = signed_char2int(*p++);
		if (!c)
			continue;
		if (quotes && (SIT(c, syntax) == CCTL || SIT(c, syntax) == CBACK))
			USTPUTC(CTLESC, q);
		USTPUTC(c, q);
	}

	expdest = q;
}

static void
strtodest(const char *p, int syntax, int quotes)
{
	memtodest(p, strlen(p), syntax, quotes);
}

/*
 * Record the fact that we have to scan this region of the
 * string for IFS characters.
 */
static void
recordregion(int start, int end, int nulonly)
{
	struct ifsregion *ifsp;

	if (ifslastp == NULL) {
		ifsp = &ifsfirst;
	} else {
		INT_OFF;
		ifsp = ckzalloc(sizeof(*ifsp));
		/*ifsp->next = NULL; - ckzalloc did it */
		ifslastp->next = ifsp;
		INT_ON;
	}
	ifslastp = ifsp;
	ifslastp->begoff = start;
	ifslastp->endoff = end;
	ifslastp->nulonly = nulonly;
}

static void
removerecordregions(int endoff)
{
	if (ifslastp == NULL)
		return;

	if (ifsfirst.endoff > endoff) {
		while (ifsfirst.next != NULL) {
			struct ifsregion *ifsp;
			INT_OFF;
			ifsp = ifsfirst.next->next;
			free(ifsfirst.next);
			ifsfirst.next = ifsp;
			INT_ON;
		}
		if (ifsfirst.begoff > endoff)
			ifslastp = NULL;
		else {
			ifslastp = &ifsfirst;
			ifsfirst.endoff = endoff;
		}
		return;
	}

	ifslastp = &ifsfirst;
	while (ifslastp->next && ifslastp->next->begoff < endoff)
		ifslastp=ifslastp->next;
	while (ifslastp->next != NULL) {
		struct ifsregion *ifsp;
		INT_OFF;
		ifsp = ifslastp->next->next;
		free(ifslastp->next);
		ifslastp->next = ifsp;
		INT_ON;
	}
	if (ifslastp->endoff > endoff)
		ifslastp->endoff = endoff;
}

static char *
exptilde(char *startp, char *p, int flag)
{
	char c;
	char *name;
	struct passwd *pw;
	const char *home;
	int quotes = flag & (EXP_FULL | EXP_CASE);
	int startloc;

	name = p + 1;

	while ((c = *++p) != '\0') {
		switch (c) {
		case CTLESC:
			return startp;
		case CTLQUOTEMARK:
			return startp;
		case ':':
			if (flag & EXP_VARTILDE)
				goto done;
			break;
		case '/':
		case CTLENDVAR:
			goto done;
		}
	}
 done:
	*p = '\0';
	if (*name == '\0') {
		home = lookupvar(homestr);
	} else {
		pw = getpwnam(name);
		if (pw == NULL)
			goto lose;
		home = pw->pw_dir;
	}
	if (!home || !*home)
		goto lose;
	*p = c;
	startloc = expdest - (char *)stackblock();
	strtodest(home, SQSYNTAX, quotes);
	recordregion(startloc, expdest - (char *)stackblock(), 0);
	return p;
 lose:
	*p = c;
	return startp;
}

/*
 * Execute a command inside back quotes.  If it's a builtin command, we
 * want to save its output in a block obtained from malloc.  Otherwise
 * we fork off a subprocess and get the output of the command via a pipe.
 * Should be called with interrupts off.
 */
struct backcmd {                /* result of evalbackcmd */
	int fd;                 /* file descriptor to read from */
	int nleft;              /* number of chars in buffer */
	char *buf;              /* buffer */
	struct job *jp;         /* job structure for command */
};

/* These forward decls are needed to use "eval" code for backticks handling: */
static smalluint back_exitstatus; /* exit status of backquoted command */
#define EV_EXIT 01              /* exit after evaluating tree */
static void evaltree(union node *, int);

static void
evalbackcmd(union node *n, struct backcmd *result)
{
	int saveherefd;

	result->fd = -1;
	result->buf = NULL;
	result->nleft = 0;
	result->jp = NULL;
	if (n == NULL) {
		goto out;
	}

	saveherefd = herefd;
	herefd = -1;

	{
		int pip[2];
		struct job *jp;

		if (pipe(pip) < 0)
			ash_msg_and_raise_error("pipe call failed");
		jp = makejob(/*n,*/ 1);
		if (forkshell(jp, n, FORK_NOJOB) == 0) {
			FORCE_INT_ON;
			close(pip[0]);
			if (pip[1] != 1) {
				close(1);
				copyfd(pip[1], 1);
				close(pip[1]);
			}
			eflag = 0;
			evaltree(n, EV_EXIT); /* actually evaltreenr... */
			/* NOTREACHED */
		}
		close(pip[1]);
		result->fd = pip[0];
		result->jp = jp;
	}
	herefd = saveherefd;
 out:
	TRACE(("evalbackcmd done: fd=%d buf=0x%x nleft=%d jp=0x%x\n",
		result->fd, result->buf, result->nleft, result->jp));
}

/*
 * Expand stuff in backwards quotes.
 */
static void
expbackq(union node *cmd, int quoted, int quotes)
{
	struct backcmd in;
	int i;
	char buf[128];
	char *p;
	char *dest;
	int startloc;
	int syntax = quoted ? DQSYNTAX : BASESYNTAX;
	struct stackmark smark;

	INT_OFF;
	setstackmark(&smark);
	dest = expdest;
	startloc = dest - (char *)stackblock();
	grabstackstr(dest);
	evalbackcmd(cmd, &in);
	popstackmark(&smark);

	p = in.buf;
	i = in.nleft;
	if (i == 0)
		goto read;
	for (;;) {
		memtodest(p, i, syntax, quotes);
 read:
		if (in.fd < 0)
			break;
		i = nonblock_safe_read(in.fd, buf, sizeof(buf));
		TRACE(("expbackq: read returns %d\n", i));
		if (i <= 0)
			break;
		p = buf;
	}

	free(in.buf);
	if (in.fd >= 0) {
		close(in.fd);
		back_exitstatus = waitforjob(in.jp);
	}
	INT_ON;

	/* Eat all trailing newlines */
	dest = expdest;
	for (; dest > (char *)stackblock() && dest[-1] == '\n';)
		STUNPUTC(dest);
	expdest = dest;

	if (quoted == 0)
		recordregion(startloc, dest - (char *)stackblock(), 0);
	TRACE(("evalbackq: size=%d: \"%.*s\"\n",
		(dest - (char *)stackblock()) - startloc,
		(dest - (char *)stackblock()) - startloc,
		stackblock() + startloc));
}

#if ENABLE_ASH_MATH_SUPPORT
/*
 * Expand arithmetic expression.  Backup to start of expression,
 * evaluate, place result in (backed up) result, adjust string position.
 */
static void
expari(int quotes)
{
	char *p, *start;
	int begoff;
	int flag;
	int len;

	/*      ifsfree(); */

	/*
	 * This routine is slightly over-complicated for
	 * efficiency.  Next we scan backwards looking for the
	 * start of arithmetic.
	 */
	start = stackblock();
	p = expdest - 1;
	*p = '\0';
	p--;
	do {
		int esc;

		while (*p != CTLARI) {
			p--;
#if DEBUG
			if (p < start) {
				ash_msg_and_raise_error("missing CTLARI (shouldn't happen)");
			}
#endif
		}

		esc = esclen(start, p);
		if (!(esc % 2)) {
			break;
		}

		p -= esc + 1;
	} while (1);

	begoff = p - start;

	removerecordregions(begoff);

	flag = p[1];

	expdest = p;

	if (quotes)
		rmescapes(p + 2);

	len = cvtnum(dash_arith(p + 2));

	if (flag != '"')
		recordregion(begoff, begoff + len, 0);
}
#endif

/* argstr needs it */
static char *evalvar(char *p, int flag, struct strlist *var_str_list);

/*
 * Perform variable and command substitution.  If EXP_FULL is set, output CTLESC
 * characters to allow for further processing.  Otherwise treat
 * $@ like $* since no splitting will be performed.
 *
 * var_str_list (can be NULL) is a list of "VAR=val" strings which take precedence
 * over shell varables. Needed for "A=a B=$A; echo $B" case - we use it
 * for correct expansion of "B=$A" word.
 */
static void
argstr(char *p, int flag, struct strlist *var_str_list)
{
	static const char spclchars[] ALIGN1 = {
		'=',
		':',
		CTLQUOTEMARK,
		CTLENDVAR,
		CTLESC,
		CTLVAR,
		CTLBACKQ,
		CTLBACKQ | CTLQUOTE,
#if ENABLE_ASH_MATH_SUPPORT
		CTLENDARI,
#endif
		0
	};
	const char *reject = spclchars;
	int c;
	int quotes = flag & (EXP_FULL | EXP_CASE);      /* do CTLESC */
	int breakall = flag & EXP_WORD;
	int inquotes;
	size_t length;
	int startloc;

	if (!(flag & EXP_VARTILDE)) {
		reject += 2;
	} else if (flag & EXP_VARTILDE2) {
		reject++;
	}
	inquotes = 0;
	length = 0;
	if (flag & EXP_TILDE) {
		char *q;

		flag &= ~EXP_TILDE;
 tilde:
		q = p;
		if (*q == CTLESC && (flag & EXP_QWORD))
			q++;
		if (*q == '~')
			p = exptilde(p, q, flag);
	}
 start:
	startloc = expdest - (char *)stackblock();
	for (;;) {
		length += strcspn(p + length, reject);
		c = p[length];
		if (c && (!(c & 0x80)
#if ENABLE_ASH_MATH_SUPPORT
					|| c == CTLENDARI
#endif
		   )) {
			/* c == '=' || c == ':' || c == CTLENDARI */
			length++;
		}
		if (length > 0) {
			int newloc;
			expdest = stack_nputstr(p, length, expdest);
			newloc = expdest - (char *)stackblock();
			if (breakall && !inquotes && newloc > startloc) {
				recordregion(startloc, newloc, 0);
			}
			startloc = newloc;
		}
		p += length + 1;
		length = 0;

		switch (c) {
		case '\0':
			goto breakloop;
		case '=':
			if (flag & EXP_VARTILDE2) {
				p--;
				continue;
			}
			flag |= EXP_VARTILDE2;
			reject++;
			/* fall through */
		case ':':
			/*
			 * sort of a hack - expand tildes in variable
			 * assignments (after the first '=' and after ':'s).
			 */
			if (*--p == '~') {
				goto tilde;
			}
			continue;
		}

		switch (c) {
		case CTLENDVAR: /* ??? */
			goto breakloop;
		case CTLQUOTEMARK:
			/* "$@" syntax adherence hack */
			if (
				!inquotes &&
				!memcmp(p, dolatstr, 4) &&
				(p[4] == CTLQUOTEMARK || (
					p[4] == CTLENDVAR &&
					p[5] == CTLQUOTEMARK
				))
			) {
				p = evalvar(p + 1, flag, /* var_str_list: */ NULL) + 1;
				goto start;
			}
			inquotes = !inquotes;
 addquote:
			if (quotes) {
				p--;
				length++;
				startloc++;
			}
			break;
		case CTLESC:
			startloc++;
			length++;
			goto addquote;
		case CTLVAR:
			p = evalvar(p, flag, var_str_list);
			goto start;
		case CTLBACKQ:
			c = 0;
		case CTLBACKQ|CTLQUOTE:
			expbackq(argbackq->n, c, quotes);
			argbackq = argbackq->next;
			goto start;
#if ENABLE_ASH_MATH_SUPPORT
		case CTLENDARI:
			p--;
			expari(quotes);
			goto start;
#endif
		}
	}
 breakloop:
	;
}

static char *
scanleft(char *startp, char *rmesc, char *rmescend ATTRIBUTE_UNUSED, char *str, int quotes,
	int zero)
{
// This commented out code was added by James Simmons <jsimmons@infradead.org>
// as part of a larger change when he added support for ${var/a/b}.
// However, it broke # and % operators:
//
//var=ababcdcd
//                 ok       bad
//echo ${var#ab}   abcdcd   abcdcd
//echo ${var##ab}  abcdcd   abcdcd
//echo ${var#a*b}  abcdcd   ababcdcd  (!)
//echo ${var##a*b} cdcd     cdcd
//echo ${var#?}    babcdcd  ababcdcd  (!)
//echo ${var##?}   babcdcd  babcdcd
//echo ${var#*}    ababcdcd babcdcd   (!)
//echo ${var##*}
//echo ${var%cd}   ababcd   ababcd
//echo ${var%%cd}  ababcd   abab      (!)
//echo ${var%c*d}  ababcd   ababcd
//echo ${var%%c*d} abab     ababcdcd  (!)
//echo ${var%?}    ababcdc  ababcdc
//echo ${var%%?}   ababcdc  ababcdcd  (!)
//echo ${var%*}    ababcdcd ababcdcd
//echo ${var%%*}
//
// Commenting it back out helped. Remove it completely if it really
// is not needed.

	char *loc, *loc2; //, *full;
	char c;

	loc = startp;
	loc2 = rmesc;
	do {
		int match; // = strlen(str);
		const char *s = loc2;

		c = *loc2;
		if (zero) {
			*loc2 = '\0';
			s = rmesc;
		}
		match = pmatch(str, s); // this line was deleted

//		// chop off end if its '*'
//		full = strrchr(str, '*');
//		if (full && full != str)
//			match--;
//
//		// If str starts with '*' replace with s.
//		if ((*str == '*') && strlen(s) >= match) {
//			full = xstrdup(s);
//			strncpy(full+strlen(s)-match+1, str+1, match-1);
//		} else
//			full = xstrndup(str, match);
//		match = strncmp(s, full, strlen(full));
//		free(full);
//
		*loc2 = c;
		if (match) // if (!match)
			return loc;
		if (quotes && *loc == CTLESC)
			loc++;
		loc++;
		loc2++;
	} while (c);
	return 0;
}

static char *
scanright(char *startp, char *rmesc, char *rmescend, char *str, int quotes,
	int zero)
{
	int esc = 0;
	char *loc;
	char *loc2;

	for (loc = str - 1, loc2 = rmescend; loc >= startp; loc2--) {
		int match;
		char c = *loc2;
		const char *s = loc2;
		if (zero) {
			*loc2 = '\0';
			s = rmesc;
		}
		match = pmatch(str, s);
		*loc2 = c;
		if (match)
			return loc;
		loc--;
		if (quotes) {
			if (--esc < 0) {
				esc = esclen(startp, loc);
			}
			if (esc % 2) {
				esc--;
				loc--;
			}
		}
	}
	return 0;
}

static void varunset(const char *, const char *, const char *, int) ATTRIBUTE_NORETURN;
static void
varunset(const char *end, const char *var, const char *umsg, int varflags)
{
	const char *msg;
	const char *tail;

	tail = nullstr;
	msg = "parameter not set";
	if (umsg) {
		if (*end == CTLENDVAR) {
			if (varflags & VSNUL)
				tail = " or null";
		} else
			msg = umsg;
	}
	ash_msg_and_raise_error("%.*s: %s%s", end - var - 1, var, msg, tail);
}

#if ENABLE_ASH_BASH_COMPAT
static char *
parse_sub_pattern(char *arg, int inquotes)
{
	char *idx, *repl = NULL;
	unsigned char c;

	idx = arg;
	while (1) {
		c = *arg;
		if (!c)
			break;
		if (c == '/') {
			/* Only the first '/' seen is our separator */
			if (!repl) {
				repl = idx + 1;
				c = '\0';
			}
		}
		*idx++ = c;
		if (!inquotes && c == '\\' && arg[1] == '\\')
			arg++; /* skip both \\, not just first one */
		arg++;
	}
	*idx = c; /* NUL */

	return repl;
}
#endif /* ENABLE_ASH_BASH_COMPAT */

static const char *
subevalvar(char *p, char *str, int strloc, int subtype,
		int startloc, int varflags, int quotes, struct strlist *var_str_list)
{
	struct nodelist *saveargbackq = argbackq;
	char *startp;
	char *loc;
	char *rmesc, *rmescend;
	USE_ASH_BASH_COMPAT(char *repl = NULL;)
	USE_ASH_BASH_COMPAT(char null = '\0';)
	USE_ASH_BASH_COMPAT(int pos, len, orig_len;)
	int saveherefd = herefd;
	int amount, workloc, resetloc;
	int zero;
	char *(*scan)(char*, char*, char*, char*, int, int);

	herefd = -1;
	argstr(p, (subtype != VSASSIGN && subtype != VSQUESTION) ? EXP_CASE : 0,
			var_str_list);
	STPUTC('\0', expdest);
	herefd = saveherefd;
	argbackq = saveargbackq;
	startp = (char *)stackblock() + startloc;

	switch (subtype) {
	case VSASSIGN:
		setvar(str, startp, 0);
		amount = startp - expdest;
		STADJUST(amount, expdest);
		return startp;

#if ENABLE_ASH_BASH_COMPAT
	case VSSUBSTR:
		loc = str = stackblock() + strloc;
// TODO: number() instead? It does error checking...
		pos = atoi(loc);
		len = str - startp - 1;

		/* *loc != '\0', guaranteed by parser */
		if (quotes) {
			char *ptr;

			/* We must adjust the length by the number of escapes we find. */
			for (ptr = startp; ptr < (str - 1); ptr++) {
				if(*ptr == CTLESC) {
					len--;
					ptr++;
				}
			}
		}
		orig_len = len;

		if (*loc++ == ':') {
// TODO: number() instead? It does error checking...
			len = atoi(loc);
		} else {
			len = orig_len;
			while (*loc && *loc != ':')
				loc++;
			if (*loc++ == ':')
// TODO: number() instead? It does error checking...
				len = atoi(loc);
		}
		if (pos >= orig_len) {
			pos = 0;
			len = 0;
		}
		if (len > (orig_len - pos))
			len = orig_len - pos;

		for (str = startp; pos; str++, pos--) {
			if (quotes && *str == CTLESC)
				str++;
		}
		for (loc = startp; len; len--) {
			if (quotes && *str == CTLESC)
				*loc++ = *str++;
			*loc++ = *str++;
		}
		*loc = '\0';
		amount = loc - expdest;
		STADJUST(amount, expdest);
		return loc;
#endif

	case VSQUESTION:
		varunset(p, str, startp, varflags);
		/* NOTREACHED */
	}
	resetloc = expdest - (char *)stackblock();

	/* We'll comeback here if we grow the stack while handling
	 * a VSREPLACE or VSREPLACEALL, since our pointers into the
	 * stack will need rebasing, and we'll need to remove our work
	 * areas each time
	 */
 USE_ASH_BASH_COMPAT(restart:)

	amount = expdest - ((char *)stackblock() + resetloc);
	STADJUST(-amount, expdest);
	startp = (char *)stackblock() + startloc;

	rmesc = startp;
	rmescend = (char *)stackblock() + strloc;
	if (quotes) {
		rmesc = _rmescapes(startp, RMESCAPE_ALLOC | RMESCAPE_GROW);
		if (rmesc != startp) {
			rmescend = expdest;
			startp = (char *)stackblock() + startloc;
		}
	}
	rmescend--;
	str = (char *)stackblock() + strloc;
	preglob(str, varflags & VSQUOTE, 0);
	workloc = expdest - (char *)stackblock();

#if ENABLE_ASH_BASH_COMPAT
	if (subtype == VSREPLACE || subtype == VSREPLACEALL) {
		char *idx, *end, *restart_detect;

		if(!repl) {
			repl = parse_sub_pattern(str, varflags & VSQUOTE);
			if (!repl)
				repl = &null;
		}

		/* If there's no pattern to match, return the expansion unmolested */
		if (*str == '\0')
			return 0;

		len = 0;
		idx = startp;
		end = str - 1;
		while (idx < end) {
			loc = scanright(idx, rmesc, rmescend, str, quotes, 1);
			if (!loc) {
				/* No match, advance */
				restart_detect = stackblock();
				STPUTC(*idx, expdest);
				if (quotes && *idx == CTLESC) {
					idx++;
					len++;
					STPUTC(*idx, expdest);
				}
				if (stackblock() != restart_detect)
					goto restart;
				idx++;
				len++;
				rmesc++;
				continue;
			}

			if (subtype == VSREPLACEALL) {
				while (idx < loc) {
					if (quotes && *idx == CTLESC)
						idx++;
					idx++;
					rmesc++;
				}
			} else
				idx = loc;

			for (loc = repl; *loc; loc++) {
				restart_detect = stackblock();
				STPUTC(*loc, expdest);
				if (stackblock() != restart_detect)
					goto restart;
				len++;
			}

			if (subtype == VSREPLACE) {
				while (*idx) {
					restart_detect = stackblock();
					STPUTC(*idx, expdest);
					if (stackblock() != restart_detect)
						goto restart;
					len++;
					idx++;
				}
				break;
			}
		}

		/* We've put the replaced text into a buffer at workloc, now
		 * move it to the right place and adjust the stack.
		 */
		startp = stackblock() + startloc;
		STPUTC('\0', expdest);
		memmove(startp, stackblock() + workloc, len);
		startp[len++] = '\0';
		amount = expdest - ((char *)stackblock() + startloc + len - 1);
		STADJUST(-amount, expdest);
		return startp;
	}
#endif /* ENABLE_ASH_BASH_COMPAT */

	subtype -= VSTRIMRIGHT;
#if DEBUG
	if (subtype < 0 || subtype > 7)
		abort();
#endif
	/* zero = subtype == VSTRIMLEFT || subtype == VSTRIMLEFTMAX */
	zero = subtype >> 1;
	/* VSTRIMLEFT/VSTRIMRIGHTMAX -> scanleft */
	scan = (subtype & 1) ^ zero ? scanleft : scanright;

	loc = scan(startp, rmesc, rmescend, str, quotes, zero);
	if (loc) {
		if (zero) {
			memmove(startp, loc, str - loc);
			loc = startp + (str - loc) - 1;
		}
		*loc = '\0';
		amount = loc - expdest;
		STADJUST(amount, expdest);
	}
	return loc;
}

/*
 * Add the value of a specialized variable to the stack string.
 */
static ssize_t
varvalue(char *name, int varflags, int flags, struct strlist *var_str_list)
{
	int num;
	char *p;
	int i;
	int sep = 0;
	int sepq = 0;
	ssize_t len = 0;
	char **ap;
	int syntax;
	int quoted = varflags & VSQUOTE;
	int subtype = varflags & VSTYPE;
	int quotes = flags & (EXP_FULL | EXP_CASE);

	if (quoted && (flags & EXP_FULL))
		sep = 1 << CHAR_BIT;

	syntax = quoted ? DQSYNTAX : BASESYNTAX;
	switch (*name) {
	case '$':
		num = rootpid;
		goto numvar;
	case '?':
		num = exitstatus;
		goto numvar;
	case '#':
		num = shellparam.nparam;
		goto numvar;
	case '!':
		num = backgndpid;
		if (num == 0)
			return -1;
 numvar:
		len = cvtnum(num);
		break;
	case '-':
		p = makestrspace(NOPTS, expdest);
		for (i = NOPTS - 1; i >= 0; i--) {
			if (optlist[i]) {
				USTPUTC(optletters(i), p);
				len++;
			}
		}
		expdest = p;
		break;
	case '@':
		if (sep)
			goto param;
		/* fall through */
	case '*':
		sep = ifsset() ? signed_char2int(ifsval()[0]) : ' ';
		if (quotes && (SIT(sep, syntax) == CCTL || SIT(sep, syntax) == CBACK))
			sepq = 1;
 param:
		ap = shellparam.p;
		if (!ap)
			return -1;
		while ((p = *ap++)) {
			size_t partlen;

			partlen = strlen(p);
			len += partlen;

			if (!(subtype == VSPLUS || subtype == VSLENGTH))
				memtodest(p, partlen, syntax, quotes);

			if (*ap && sep) {
				char *q;

				len++;
				if (subtype == VSPLUS || subtype == VSLENGTH) {
					continue;
				}
				q = expdest;
				if (sepq)
					STPUTC(CTLESC, q);
				STPUTC(sep, q);
				expdest = q;
			}
		}
		return len;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
// TODO: number() instead? It does error checking...
		num = atoi(name);
		if (num < 0 || num > shellparam.nparam)
			return -1;
		p = num ? shellparam.p[num - 1] : arg0;
		goto value;
	default:
		/* NB: name has form "VAR=..." */

		/* "A=a B=$A" case: var_str_list is a list of "A=a" strings
		 * which should be considered before we check variables. */
		if (var_str_list) {
			unsigned name_len = (strchrnul(name, '=') - name) + 1;
			p = NULL;
			do {
				char *str, *eq;
				str = var_str_list->text;
				eq = strchr(str, '=');
				if (!eq) /* stop at first non-assignment */
					break;
				eq++;
				if (name_len == (unsigned)(eq - str)
				 && strncmp(str, name, name_len) == 0) {
					p = eq;
					/* goto value; - WRONG! */
					/* think "A=1 A=2 B=$A" */
				}
				var_str_list = var_str_list->next;
			} while (var_str_list);
			if (p)
				goto value;
		}
		p = lookupvar(name);
 value:
		if (!p)
			return -1;

		len = strlen(p);
		if (!(subtype == VSPLUS || subtype == VSLENGTH))
			memtodest(p, len, syntax, quotes);
		return len;
	}

	if (subtype == VSPLUS || subtype == VSLENGTH)
		STADJUST(-len, expdest);
	return len;
}

/*
 * Expand a variable, and return a pointer to the next character in the
 * input string.
 */
static char *
evalvar(char *p, int flag, struct strlist *var_str_list)
{
	char varflags;
	char subtype;
	char quoted;
	char easy;
	char *var;
	int patloc;
	int startloc;
	ssize_t varlen;

	varflags = *p++;
	subtype = varflags & VSTYPE;
	quoted = varflags & VSQUOTE;
	var = p;
	easy = (!quoted || (*var == '@' && shellparam.nparam));
	startloc = expdest - (char *)stackblock();
	p = strchr(p, '=') + 1;

 again:
	varlen = varvalue(var, varflags, flag, var_str_list);
	if (varflags & VSNUL)
		varlen--;

	if (subtype == VSPLUS) {
		varlen = -1 - varlen;
		goto vsplus;
	}

	if (subtype == VSMINUS) {
 vsplus:
		if (varlen < 0) {
			argstr(
				p, flag | EXP_TILDE |
					(quoted ?  EXP_QWORD : EXP_WORD),
				var_str_list
			);
			goto end;
		}
		if (easy)
			goto record;
		goto end;
	}

	if (subtype == VSASSIGN || subtype == VSQUESTION) {
		if (varlen < 0) {
			if (subevalvar(p, var, /* strloc: */ 0,
					subtype, startloc, varflags,
					/* quotes: */ 0,
					var_str_list)
			) {
				varflags &= ~VSNUL;
				/*
				 * Remove any recorded regions beyond
				 * start of variable
				 */
				removerecordregions(startloc);
				goto again;
			}
			goto end;
		}
		if (easy)
			goto record;
		goto end;
	}

	if (varlen < 0 && uflag)
		varunset(p, var, 0, 0);

	if (subtype == VSLENGTH) {
		cvtnum(varlen > 0 ? varlen : 0);
		goto record;
	}

	if (subtype == VSNORMAL) {
		if (easy)
			goto record;
		goto end;
	}

#if DEBUG
	switch (subtype) {
	case VSTRIMLEFT:
	case VSTRIMLEFTMAX:
	case VSTRIMRIGHT:
	case VSTRIMRIGHTMAX:
#if ENABLE_ASH_BASH_COMPAT
	case VSSUBSTR:
	case VSREPLACE:
	case VSREPLACEALL:
#endif
		break;
	default:
		abort();
	}
#endif

	if (varlen >= 0) {
		/*
		 * Terminate the string and start recording the pattern
		 * right after it
		 */
		STPUTC('\0', expdest);
		patloc = expdest - (char *)stackblock();
		if (0 == subevalvar(p, /* str: */ NULL, patloc, subtype,
				startloc, varflags,
				/* quotes: */ flag & (EXP_FULL | EXP_CASE),
				var_str_list)
		) {
			int amount = expdest - (
				(char *)stackblock() + patloc - 1
			);
			STADJUST(-amount, expdest);
		}
		/* Remove any recorded regions beyond start of variable */
		removerecordregions(startloc);
 record:
		recordregion(startloc, expdest - (char *)stackblock(), quoted);
	}

 end:
	if (subtype != VSNORMAL) {      /* skip to end of alternative */
		int nesting = 1;
		for (;;) {
			char c = *p++;
			if (c == CTLESC)
				p++;
			else if (c == CTLBACKQ || c == (CTLBACKQ|CTLQUOTE)) {
				if (varlen >= 0)
					argbackq = argbackq->next;
			} else if (c == CTLVAR) {
				if ((*p++ & VSTYPE) != VSNORMAL)
					nesting++;
			} else if (c == CTLENDVAR) {
				if (--nesting == 0)
					break;
			}
		}
	}
	return p;
}

/*
 * Break the argument string into pieces based upon IFS and add the
 * strings to the argument list.  The regions of the string to be
 * searched for IFS characters have been stored by recordregion.
 */
static void
ifsbreakup(char *string, struct arglist *arglist)
{
	struct ifsregion *ifsp;
	struct strlist *sp;
	char *start;
	char *p;
	char *q;
	const char *ifs, *realifs;
	int ifsspc;
	int nulonly;

	start = string;
	if (ifslastp != NULL) {
		ifsspc = 0;
		nulonly = 0;
		realifs = ifsset() ? ifsval() : defifs;
		ifsp = &ifsfirst;
		do {
			p = string + ifsp->begoff;
			nulonly = ifsp->nulonly;
			ifs = nulonly ? nullstr : realifs;
			ifsspc = 0;
			while (p < string + ifsp->endoff) {
				q = p;
				if (*p == CTLESC)
					p++;
				if (!strchr(ifs, *p)) {
					p++;
					continue;
				}
				if (!nulonly)
					ifsspc = (strchr(defifs, *p) != NULL);
				/* Ignore IFS whitespace at start */
				if (q == start && ifsspc) {
					p++;
					start = p;
					continue;
				}
				*q = '\0';
				sp = stzalloc(sizeof(*sp));
				sp->text = start;
				*arglist->lastp = sp;
				arglist->lastp = &sp->next;
				p++;
				if (!nulonly) {
					for (;;) {
						if (p >= string + ifsp->endoff) {
							break;
						}
						q = p;
						if (*p == CTLESC)
							p++;
						if (strchr(ifs, *p) == NULL) {
							p = q;
							break;
						}
						if (strchr(defifs, *p) == NULL) {
							if (ifsspc) {
								p++;
								ifsspc = 0;
							} else {
								p = q;
								break;
							}
						} else
							p++;
					}
				}
				start = p;
			} /* while */
			ifsp = ifsp->next;
		} while (ifsp != NULL);
		if (nulonly)
			goto add;
	}

	if (!*start)
		return;

 add:
	sp = stzalloc(sizeof(*sp));
	sp->text = start;
	*arglist->lastp = sp;
	arglist->lastp = &sp->next;
}

static void
ifsfree(void)
{
	struct ifsregion *p;

	INT_OFF;
	p = ifsfirst.next;
	do {
		struct ifsregion *ifsp;
		ifsp = p->next;
		free(p);
		p = ifsp;
	} while (p);
	ifslastp = NULL;
	ifsfirst.next = NULL;
	INT_ON;
}

/*
 * Add a file name to the list.
 */
static void
addfname(const char *name)
{
	struct strlist *sp;

	sp = stzalloc(sizeof(*sp));
	sp->text = ststrdup(name);
	*exparg.lastp = sp;
	exparg.lastp = &sp->next;
}

static char *expdir;

/*
 * Do metacharacter (i.e. *, ?, [...]) expansion.
 */
static void
expmeta(char *enddir, char *name)
{
	char *p;
	const char *cp;
	char *start;
	char *endname;
	int metaflag;
	struct stat statb;
	DIR *dirp;
	struct dirent *dp;
	int atend;
	int matchdot;

	metaflag = 0;
	start = name;
	for (p = name; *p; p++) {
		if (*p == '*' || *p == '?')
			metaflag = 1;
		else if (*p == '[') {
			char *q = p + 1;
			if (*q == '!')
				q++;
			for (;;) {
				if (*q == '\\')
					q++;
				if (*q == '/' || *q == '\0')
					break;
				if (*++q == ']') {
					metaflag = 1;
					break;
				}
			}
		} else if (*p == '\\')
			p++;
		else if (*p == '/') {
			if (metaflag)
				goto out;
			start = p + 1;
		}
	}
 out:
	if (metaflag == 0) {    /* we've reached the end of the file name */
		if (enddir != expdir)
			metaflag++;
		p = name;
		do {
			if (*p == '\\')
				p++;
			*enddir++ = *p;
		} while (*p++);
		if (metaflag == 0 || lstat(expdir, &statb) >= 0)
			addfname(expdir);
		return;
	}
	endname = p;
	if (name < start) {
		p = name;
		do {
			if (*p == '\\')
				p++;
			*enddir++ = *p++;
		} while (p < start);
	}
	if (enddir == expdir) {
		cp = ".";
	} else if (enddir == expdir + 1 && *expdir == '/') {
		cp = "/";
	} else {
		cp = expdir;
		enddir[-1] = '\0';
	}
	dirp = opendir(cp);
	if (dirp == NULL)
		return;
	if (enddir != expdir)
		enddir[-1] = '/';
	if (*endname == 0) {
		atend = 1;
	} else {
		atend = 0;
		*endname++ = '\0';
	}
	matchdot = 0;
	p = start;
	if (*p == '\\')
		p++;
	if (*p == '.')
		matchdot++;
	while (!intpending && (dp = readdir(dirp)) != NULL) {
		if (dp->d_name[0] == '.' && ! matchdot)
			continue;
		if (pmatch(start, dp->d_name)) {
			if (atend) {
				strcpy(enddir, dp->d_name);
				addfname(expdir);
			} else {
				for (p = enddir, cp = dp->d_name; (*p++ = *cp++) != '\0';)
					continue;
				p[-1] = '/';
				expmeta(p, endname);
			}
		}
	}
	closedir(dirp);
	if (! atend)
		endname[-1] = '/';
}

static struct strlist *
msort(struct strlist *list, int len)
{
	struct strlist *p, *q = NULL;
	struct strlist **lpp;
	int half;
	int n;

	if (len <= 1)
		return list;
	half = len >> 1;
	p = list;
	for (n = half; --n >= 0;) {
		q = p;
		p = p->next;
	}
	q->next = NULL;                 /* terminate first half of list */
	q = msort(list, half);          /* sort first half of list */
	p = msort(p, len - half);               /* sort second half */
	lpp = &list;
	for (;;) {
#if ENABLE_LOCALE_SUPPORT
		if (strcoll(p->text, q->text) < 0)
#else
		if (strcmp(p->text, q->text) < 0)
#endif
						{
			*lpp = p;
			lpp = &p->next;
			p = *lpp;
			if (p == NULL) {
				*lpp = q;
				break;
			}
		} else {
			*lpp = q;
			lpp = &q->next;
			q = *lpp;
			if (q == NULL) {
				*lpp = p;
				break;
			}
		}
	}
	return list;
}

/*
 * Sort the results of file name expansion.  It calculates the number of
 * strings to sort and then calls msort (short for merge sort) to do the
 * work.
 */
static struct strlist *
expsort(struct strlist *str)
{
	int len;
	struct strlist *sp;

	len = 0;
	for (sp = str; sp; sp = sp->next)
		len++;
	return msort(str, len);
}

static void
expandmeta(struct strlist *str /*, int flag*/)
{
	static const char metachars[] ALIGN1 = {
		'*', '?', '[', 0
	};
	/* TODO - EXP_REDIR */

	while (str) {
		struct strlist **savelastp;
		struct strlist *sp;
		char *p;

		if (fflag)
			goto nometa;
		if (!strpbrk(str->text, metachars))
			goto nometa;
		savelastp = exparg.lastp;

		INT_OFF;
		p = preglob(str->text, 0, RMESCAPE_ALLOC | RMESCAPE_HEAP);
		{
			int i = strlen(str->text);
			expdir = ckmalloc(i < 2048 ? 2048 : i); /* XXX */
		}

		expmeta(expdir, p);
		free(expdir);
		if (p != str->text)
			free(p);
		INT_ON;
		if (exparg.lastp == savelastp) {
			/*
			 * no matches
			 */
 nometa:
			*exparg.lastp = str;
			rmescapes(str->text);
			exparg.lastp = &str->next;
		} else {
			*exparg.lastp = NULL;
			*savelastp = sp = expsort(*savelastp);
			while (sp->next != NULL)
				sp = sp->next;
			exparg.lastp = &sp->next;
		}
		str = str->next;
	}
}

/*
 * Perform variable substitution and command substitution on an argument,
 * placing the resulting list of arguments in arglist.  If EXP_FULL is true,
 * perform splitting and file name expansion.  When arglist is NULL, perform
 * here document expansion.
 */
static void
expandarg(union node *arg, struct arglist *arglist, int flag)
{
	struct strlist *sp;
	char *p;

	argbackq = arg->narg.backquote;
	STARTSTACKSTR(expdest);
	ifsfirst.next = NULL;
	ifslastp = NULL;
	argstr(arg->narg.text, flag,
			/* var_str_list: */ arglist ? arglist->list : NULL);
	p = _STPUTC('\0', expdest);
	expdest = p - 1;
	if (arglist == NULL) {
		return;                 /* here document expanded */
	}
	p = grabstackstr(p);
	exparg.lastp = &exparg.list;
	/*
	 * TODO - EXP_REDIR
	 */
	if (flag & EXP_FULL) {
		ifsbreakup(p, &exparg);
		*exparg.lastp = NULL;
		exparg.lastp = &exparg.list;
		expandmeta(exparg.list /*, flag*/);
	} else {
		if (flag & EXP_REDIR) /*XXX - for now, just remove escapes */
			rmescapes(p);
		sp = stzalloc(sizeof(*sp));
		sp->text = p;
		*exparg.lastp = sp;
		exparg.lastp = &sp->next;
	}
	if (ifsfirst.next)
		ifsfree();
	*exparg.lastp = NULL;
	if (exparg.list) {
		*arglist->lastp = exparg.list;
		arglist->lastp = exparg.lastp;
	}
}

/*
 * Expand shell variables and backquotes inside a here document.
 */
static void
expandhere(union node *arg, int fd)
{
	herefd = fd;
	expandarg(arg, (struct arglist *)NULL, 0);
	full_write(fd, stackblock(), expdest - (char *)stackblock());
}

/*
 * Returns true if the pattern matches the string.
 */
static int
patmatch(char *pattern, const char *string)
{
	return pmatch(preglob(pattern, 0, 0), string);
}

/*
 * See if a pattern matches in a case statement.
 */
static int
casematch(union node *pattern, char *val)
{
	struct stackmark smark;
	int result;

	setstackmark(&smark);
	argbackq = pattern->narg.backquote;
	STARTSTACKSTR(expdest);
	ifslastp = NULL;
	argstr(pattern->narg.text, EXP_TILDE | EXP_CASE,
			/* var_str_list: */ NULL);
	STACKSTRNUL(expdest);
	result = patmatch(stackblock(), val);
	popstackmark(&smark);
	return result;
}


/* ============ find_command */

struct builtincmd {
	const char *name;
	int (*builtin)(int, char **);
	/* unsigned flags; */
};
#define IS_BUILTIN_SPECIAL(b) ((b)->name[0] & 1)
/* "regular" builtins always take precedence over commands,
 * regardless of PATH=....%builtin... position */
#define IS_BUILTIN_REGULAR(b) ((b)->name[0] & 2)
#define IS_BUILTIN_ASSIGN(b)  ((b)->name[0] & 4)

struct cmdentry {
	smallint cmdtype;       /* CMDxxx */
	union param {
		int index;
		/* index >= 0 for commands without path (slashes) */
		/* (TODO: what exactly does the value mean? PATH position?) */
		/* index == -1 for commands with slashes */
		/* index == (-2 - applet_no) for NOFORK applets */
		const struct builtincmd *cmd;
		struct funcnode *func;
	} u;
};
/* values of cmdtype */
#define CMDUNKNOWN      -1      /* no entry in table for command */
#define CMDNORMAL       0       /* command is an executable program */
#define CMDFUNCTION     1       /* command is a shell function */
#define CMDBUILTIN      2       /* command is a shell builtin */

/* action to find_command() */
#define DO_ERR          0x01    /* prints errors */
#define DO_ABS          0x02    /* checks absolute paths */
#define DO_NOFUNC       0x04    /* don't return shell functions, for command */
#define DO_ALTPATH      0x08    /* using alternate path */
#define DO_ALTBLTIN     0x20    /* %builtin in alt. path */

static void find_command(char *, struct cmdentry *, int, const char *);


/* ============ Hashing commands */

/*
 * When commands are first encountered, they are entered in a hash table.
 * This ensures that a full path search will not have to be done for them
 * on each invocation.
 *
 * We should investigate converting to a linear search, even though that
 * would make the command name "hash" a misnomer.
 */

struct tblentry {
	struct tblentry *next;  /* next entry in hash chain */
	union param param;      /* definition of builtin function */
	smallint cmdtype;       /* CMDxxx */
	char rehash;            /* if set, cd done since entry created */
	char cmdname[1];        /* name of command */
};

static struct tblentry **cmdtable;
#define INIT_G_cmdtable() do { \
	cmdtable = xzalloc(CMDTABLESIZE * sizeof(cmdtable[0])); \
} while (0)

static int builtinloc = -1;     /* index in path of %builtin, or -1 */


static void
tryexec(USE_FEATURE_SH_STANDALONE(int applet_no,) char *cmd, char **argv, char **envp)
{
	int repeated = 0;

#if ENABLE_FEATURE_SH_STANDALONE
	if (applet_no >= 0) {
		if (APPLET_IS_NOEXEC(applet_no))
			run_applet_no_and_exit(applet_no, argv);
		/* re-exec ourselves with the new arguments */
		execve(bb_busybox_exec_path, argv, envp);
		/* If they called chroot or otherwise made the binary no longer
		 * executable, fall through */
	}
#endif

 repeat:
#ifdef SYSV
	do {
		execve(cmd, argv, envp);
	} while (errno == EINTR);
#else
	execve(cmd, argv, envp);
#endif
	if (repeated) {
		free(argv);
		return;
	}
	if (errno == ENOEXEC) {
		char **ap;
		char **new;

		for (ap = argv; *ap; ap++)
			continue;
		ap = new = ckmalloc((ap - argv + 2) * sizeof(ap[0]));
		ap[1] = cmd;
		ap[0] = cmd = (char *)DEFAULT_SHELL;
		ap += 2;
		argv++;
		while ((*ap++ = *argv++) != NULL)
			continue;
		argv = new;
		repeated++;
		goto repeat;
	}
}

/*
 * Exec a program.  Never returns.  If you change this routine, you may
 * have to change the find_command routine as well.
 */
static void shellexec(char **, const char *, int) ATTRIBUTE_NORETURN;
static void
shellexec(char **argv, const char *path, int idx)
{
	char *cmdname;
	int e;
	char **envp;
	int exerrno;
#if ENABLE_FEATURE_SH_STANDALONE
	int applet_no = -1;
#endif

	clearredir(1);
	envp = listvars(VEXPORT, VUNSET, 0);
	if (strchr(argv[0], '/') != NULL
#if ENABLE_FEATURE_SH_STANDALONE
	 || (applet_no = find_applet_by_name(argv[0])) >= 0
#endif
	) {
		tryexec(USE_FEATURE_SH_STANDALONE(applet_no,) argv[0], argv, envp);
		e = errno;
	} else {
		e = ENOENT;
		while ((cmdname = padvance(&path, argv[0])) != NULL) {
			if (--idx < 0 && pathopt == NULL) {
				tryexec(USE_FEATURE_SH_STANDALONE(-1,) cmdname, argv, envp);
				if (errno != ENOENT && errno != ENOTDIR)
					e = errno;
			}
			stunalloc(cmdname);
		}
	}

	/* Map to POSIX errors */
	switch (e) {
	case EACCES:
		exerrno = 126;
		break;
	case ENOENT:
		exerrno = 127;
		break;
	default:
		exerrno = 2;
		break;
	}
	exitstatus = exerrno;
	TRACE(("shellexec failed for %s, errno %d, suppressint %d\n",
		argv[0], e, suppressint));
	ash_msg_and_raise(EXEXEC, "%s: %s", argv[0], errmsg(e, "not found"));
	/* NOTREACHED */
}

static void
printentry(struct tblentry *cmdp)
{
	int idx;
	const char *path;
	char *name;

	idx = cmdp->param.index;
	path = pathval();
	do {
		name = padvance(&path, cmdp->cmdname);
		stunalloc(name);
	} while (--idx >= 0);
	out1fmt("%s%s\n", name, (cmdp->rehash ? "*" : nullstr));
}

/*
 * Clear out command entries.  The argument specifies the first entry in
 * PATH which has changed.
 */
static void
clearcmdentry(int firstchange)
{
	struct tblentry **tblp;
	struct tblentry **pp;
	struct tblentry *cmdp;

	INT_OFF;
	for (tblp = cmdtable; tblp < &cmdtable[CMDTABLESIZE]; tblp++) {
		pp = tblp;
		while ((cmdp = *pp) != NULL) {
			if ((cmdp->cmdtype == CMDNORMAL &&
			     cmdp->param.index >= firstchange)
			 || (cmdp->cmdtype == CMDBUILTIN &&
			     builtinloc >= firstchange)
			) {
				*pp = cmdp->next;
				free(cmdp);
			} else {
				pp = &cmdp->next;
			}
		}
	}
	INT_ON;
}

/*
 * Locate a command in the command hash table.  If "add" is nonzero,
 * add the command to the table if it is not already present.  The
 * variable "lastcmdentry" is set to point to the address of the link
 * pointing to the entry, so that delete_cmd_entry can delete the
 * entry.
 *
 * Interrupts must be off if called with add != 0.
 */
static struct tblentry **lastcmdentry;

static struct tblentry *
cmdlookup(const char *name, int add)
{
	unsigned int hashval;
	const char *p;
	struct tblentry *cmdp;
	struct tblentry **pp;

	p = name;
	hashval = (unsigned char)*p << 4;
	while (*p)
		hashval += (unsigned char)*p++;
	hashval &= 0x7FFF;
	pp = &cmdtable[hashval % CMDTABLESIZE];
	for (cmdp = *pp; cmdp; cmdp = cmdp->next) {
		if (strcmp(cmdp->cmdname, name) == 0)
			break;
		pp = &cmdp->next;
	}
	if (add && cmdp == NULL) {
		cmdp = *pp = ckzalloc(sizeof(struct tblentry)
				+ strlen(name)
				/* + 1 - already done because
				 * tblentry::cmdname is char[1] */);
		/*cmdp->next = NULL; - ckzalloc did it */
		cmdp->cmdtype = CMDUNKNOWN;
		strcpy(cmdp->cmdname, name);
	}
	lastcmdentry = pp;
	return cmdp;
}

/*
 * Delete the command entry returned on the last lookup.
 */
static void
delete_cmd_entry(void)
{
	struct tblentry *cmdp;

	INT_OFF;
	cmdp = *lastcmdentry;
	*lastcmdentry = cmdp->next;
	if (cmdp->cmdtype == CMDFUNCTION)
		freefunc(cmdp->param.func);
	free(cmdp);
	INT_ON;
}

/*
 * Add a new command entry, replacing any existing command entry for
 * the same name - except special builtins.
 */
static void
addcmdentry(char *name, struct cmdentry *entry)
{
	struct tblentry *cmdp;

	cmdp = cmdlookup(name, 1);
	if (cmdp->cmdtype == CMDFUNCTION) {
		freefunc(cmdp->param.func);
	}
	cmdp->cmdtype = entry->cmdtype;
	cmdp->param = entry->u;
	cmdp->rehash = 0;
}

static int
hashcmd(int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED)
{
	struct tblentry **pp;
	struct tblentry *cmdp;
	int c;
	struct cmdentry entry;
	char *name;

	if (nextopt("r") != '\0') {
		clearcmdentry(0);
		return 0;
	}

	if (*argptr == NULL) {
		for (pp = cmdtable; pp < &cmdtable[CMDTABLESIZE]; pp++) {
			for (cmdp = *pp; cmdp; cmdp = cmdp->next) {
				if (cmdp->cmdtype == CMDNORMAL)
					printentry(cmdp);
			}
		}
		return 0;
	}

	c = 0;
	while ((name = *argptr) != NULL) {
		cmdp = cmdlookup(name, 0);
		if (cmdp != NULL
		 && (cmdp->cmdtype == CMDNORMAL
		     || (cmdp->cmdtype == CMDBUILTIN && builtinloc >= 0))
		) {
			delete_cmd_entry();
		}
		find_command(name, &entry, DO_ERR, pathval());
		if (entry.cmdtype == CMDUNKNOWN)
			c = 1;
		argptr++;
	}
	return c;
}

/*
 * Called when a cd is done.  Marks all commands so the next time they
 * are executed they will be rehashed.
 */
static void
hashcd(void)
{
	struct tblentry **pp;
	struct tblentry *cmdp;

	for (pp = cmdtable; pp < &cmdtable[CMDTABLESIZE]; pp++) {
		for (cmdp = *pp; cmdp; cmdp = cmdp->next) {
			if (cmdp->cmdtype == CMDNORMAL
			 || (cmdp->cmdtype == CMDBUILTIN
			     &&	!IS_BUILTIN_REGULAR(cmdp->param.cmd)
			     && builtinloc > 0)
			) {
				cmdp->rehash = 1;
			}
		}
	}
}

/*
 * Fix command hash table when PATH changed.
 * Called before PATH is changed.  The argument is the new value of PATH;
 * pathval() still returns the old value at this point.
 * Called with interrupts off.
 */
static void
changepath(const char *new)
{
	const char *old;
	int firstchange;
	int idx;
	int idx_bltin;

	old = pathval();
	firstchange = 9999;     /* assume no change */
	idx = 0;
	idx_bltin = -1;
	for (;;) {
		if (*old != *new) {
			firstchange = idx;
			if ((*old == '\0' && *new == ':')
			 || (*old == ':' && *new == '\0'))
				firstchange++;
			old = new;      /* ignore subsequent differences */
		}
		if (*new == '\0')
			break;
		if (*new == '%' && idx_bltin < 0 && prefix(new + 1, "builtin"))
			idx_bltin = idx;
		if (*new == ':')
			idx++;
		new++, old++;
	}
	if (builtinloc < 0 && idx_bltin >= 0)
		builtinloc = idx_bltin;             /* zap builtins */
	if (builtinloc >= 0 && idx_bltin < 0)
		firstchange = 0;
	clearcmdentry(firstchange);
	builtinloc = idx_bltin;
}

#define TEOF 0
#define TNL 1
#define TREDIR 2
#define TWORD 3
#define TSEMI 4
#define TBACKGND 5
#define TAND 6
#define TOR 7
#define TPIPE 8
#define TLP 9
#define TRP 10
#define TENDCASE 11
#define TENDBQUOTE 12
#define TNOT 13
#define TCASE 14
#define TDO 15
#define TDONE 16
#define TELIF 17
#define TELSE 18
#define TESAC 19
#define TFI 20
#define TFOR 21
#define TIF 22
#define TIN 23
#define TTHEN 24
#define TUNTIL 25
#define TWHILE 26
#define TBEGIN 27
#define TEND 28
typedef smallint token_id_t;

/* first char is indicating which tokens mark the end of a list */
static const char *const tokname_array[] = {
	"\1end of file",
	"\0newline",
	"\0redirection",
	"\0word",
	"\0;",
	"\0&",
	"\0&&",
	"\0||",
	"\0|",
	"\0(",
	"\1)",
	"\1;;",
	"\1`",
#define KWDOFFSET 13
	/* the following are keywords */
	"\0!",
	"\0case",
	"\1do",
	"\1done",
	"\1elif",
	"\1else",
	"\1esac",
	"\1fi",
	"\0for",
	"\0if",
	"\0in",
	"\1then",
	"\0until",
	"\0while",
	"\0{",
	"\1}",
};

static const char *
tokname(int tok)
{
	static char buf[16];

//try this:
//if (tok < TSEMI) return tokname_array[tok] + 1;
//sprintf(buf, "\"%s\"", tokname_array[tok] + 1);
//return buf;

	if (tok >= TSEMI)
		buf[0] = '"';
	sprintf(buf + (tok >= TSEMI), "%s%c",
			tokname_array[tok] + 1, (tok >= TSEMI ? '"' : 0));
	return buf;
}

/* Wrapper around strcmp for qsort/bsearch/... */
static int
pstrcmp(const void *a, const void *b)
{
	return strcmp((char*) a, (*(char**) b) + 1);
}

static const char *const *
findkwd(const char *s)
{
	return bsearch(s, tokname_array + KWDOFFSET,
			ARRAY_SIZE(tokname_array) - KWDOFFSET,
			sizeof(tokname_array[0]), pstrcmp);
}

/*
 * Locate and print what a word is...
 */
static int
describe_command(char *command, int describe_command_verbose)
{
	struct cmdentry entry;
	struct tblentry *cmdp;
#if ENABLE_ASH_ALIAS
	const struct alias *ap;
#endif
	const char *path = pathval();

	if (describe_command_verbose) {
		out1str(command);
	}

	/* First look at the keywords */
	if (findkwd(command)) {
		out1str(describe_command_verbose ? " is a shell keyword" : command);
		goto out;
	}

#if ENABLE_ASH_ALIAS
	/* Then look at the aliases */
	ap = lookupalias(command, 0);
	if (ap != NULL) {
		if (!describe_command_verbose) {
			out1str("alias ");
			printalias(ap);
			return 0;
		}
		out1fmt(" is an alias for %s", ap->val);
		goto out;
	}
#endif
	/* Then check if it is a tracked alias */
	cmdp = cmdlookup(command, 0);
	if (cmdp != NULL) {
		entry.cmdtype = cmdp->cmdtype;
		entry.u = cmdp->param;
	} else {
		/* Finally use brute force */
		find_command(command, &entry, DO_ABS, path);
	}

	switch (entry.cmdtype) {
	case CMDNORMAL: {
		int j = entry.u.index;
		char *p;
		if (j < 0) {
			p = command;
		} else {
			do {
				p = padvance(&path, command);
				stunalloc(p);
			} while (--j >= 0);
		}
		if (describe_command_verbose) {
			out1fmt(" is%s %s",
				(cmdp ? " a tracked alias for" : nullstr), p
			);
		} else {
			out1str(p);
		}
		break;
	}

	case CMDFUNCTION:
		if (describe_command_verbose) {
			out1str(" is a shell function");
		} else {
			out1str(command);
		}
		break;

	case CMDBUILTIN:
		if (describe_command_verbose) {
			out1fmt(" is a %sshell builtin",
				IS_BUILTIN_SPECIAL(entry.u.cmd) ?
					"special " : nullstr
			);
		} else {
			out1str(command);
		}
		break;

	default:
		if (describe_command_verbose) {
			out1str(": not found\n");
		}
		return 127;
	}
 out:
	outstr("\n", stdout);
	return 0;
}

static int
typecmd(int argc ATTRIBUTE_UNUSED, char **argv)
{
	int i = 1;
	int err = 0;
	int verbose = 1;

	/* type -p ... ? (we don't bother checking for 'p') */
	if (argv[1] && argv[1][0] == '-') {
		i++;
		verbose = 0;
	}
	while (argv[i]) {
		err |= describe_command(argv[i++], verbose);
	}
	return err;
}

#if ENABLE_ASH_CMDCMD
static int
commandcmd(int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED)
{
	int c;
	enum {
		VERIFY_BRIEF = 1,
		VERIFY_VERBOSE = 2,
	} verify = 0;

	while ((c = nextopt("pvV")) != '\0')
		if (c == 'V')
			verify |= VERIFY_VERBOSE;
		else if (c == 'v')
			verify |= VERIFY_BRIEF;
#if DEBUG
		else if (c != 'p')
			abort();
#endif
	if (verify)
		return describe_command(*argptr, verify - VERIFY_BRIEF);

	return 0;
}
#endif


/* ============ eval.c */

static int funcblocksize;          /* size of structures in function */
static int funcstringsize;         /* size of strings in node */
static void *funcblock;            /* block to allocate function from */
static char *funcstring;           /* block to allocate strings from */

/* flags in argument to evaltree */
#define EV_EXIT 01              /* exit after evaluating tree */
#define EV_TESTED 02            /* exit status is checked; ignore -e flag */
#define EV_BACKCMD 04           /* command executing within back quotes */

static const short nodesize[26] = {
	SHELL_ALIGN(sizeof(struct ncmd)),
	SHELL_ALIGN(sizeof(struct npipe)),
	SHELL_ALIGN(sizeof(struct nredir)),
	SHELL_ALIGN(sizeof(struct nredir)),
	SHELL_ALIGN(sizeof(struct nredir)),
	SHELL_ALIGN(sizeof(struct nbinary)),
	SHELL_ALIGN(sizeof(struct nbinary)),
	SHELL_ALIGN(sizeof(struct nbinary)),
	SHELL_ALIGN(sizeof(struct nif)),
	SHELL_ALIGN(sizeof(struct nbinary)),
	SHELL_ALIGN(sizeof(struct nbinary)),
	SHELL_ALIGN(sizeof(struct nfor)),
	SHELL_ALIGN(sizeof(struct ncase)),
	SHELL_ALIGN(sizeof(struct nclist)),
	SHELL_ALIGN(sizeof(struct narg)),
	SHELL_ALIGN(sizeof(struct narg)),
	SHELL_ALIGN(sizeof(struct nfile)),
	SHELL_ALIGN(sizeof(struct nfile)),
	SHELL_ALIGN(sizeof(struct nfile)),
	SHELL_ALIGN(sizeof(struct nfile)),
	SHELL_ALIGN(sizeof(struct nfile)),
	SHELL_ALIGN(sizeof(struct ndup)),
	SHELL_ALIGN(sizeof(struct ndup)),
	SHELL_ALIGN(sizeof(struct nhere)),
	SHELL_ALIGN(sizeof(struct nhere)),
	SHELL_ALIGN(sizeof(struct nnot)),
};

static void calcsize(union node *n);

static void
sizenodelist(struct nodelist *lp)
{
	while (lp) {
		funcblocksize += SHELL_ALIGN(sizeof(struct nodelist));
		calcsize(lp->n);
		lp = lp->next;
	}
}

static void
calcsize(union node *n)
{
	if (n == NULL)
		return;
	funcblocksize += nodesize[n->type];
	switch (n->type) {
	case NCMD:
		calcsize(n->ncmd.redirect);
		calcsize(n->ncmd.args);
		calcsize(n->ncmd.assign);
		break;
	case NPIPE:
		sizenodelist(n->npipe.cmdlist);
		break;
	case NREDIR:
	case NBACKGND:
	case NSUBSHELL:
		calcsize(n->nredir.redirect);
		calcsize(n->nredir.n);
		break;
	case NAND:
	case NOR:
	case NSEMI:
	case NWHILE:
	case NUNTIL:
		calcsize(n->nbinary.ch2);
		calcsize(n->nbinary.ch1);
		break;
	case NIF:
		calcsize(n->nif.elsepart);
		calcsize(n->nif.ifpart);
		calcsize(n->nif.test);
		break;
	case NFOR:
		funcstringsize += strlen(n->nfor.var) + 1;
		calcsize(n->nfor.body);
		calcsize(n->nfor.args);
		break;
	case NCASE:
		calcsize(n->ncase.cases);
		calcsize(n->ncase.expr);
		break;
	case NCLIST:
		calcsize(n->nclist.body);
		calcsize(n->nclist.pattern);
		calcsize(n->nclist.next);
		break;
	case NDEFUN:
	case NARG:
		sizenodelist(n->narg.backquote);
		funcstringsize += strlen(n->narg.text) + 1;
		calcsize(n->narg.next);
		break;
	case NTO:
	case NCLOBBER:
	case NFROM:
	case NFROMTO:
	case NAPPEND:
		calcsize(n->nfile.fname);
		calcsize(n->nfile.next);
		break;
	case NTOFD:
	case NFROMFD:
		calcsize(n->ndup.vname);
		calcsize(n->ndup.next);
	break;
	case NHERE:
	case NXHERE:
		calcsize(n->nhere.doc);
		calcsize(n->nhere.next);
		break;
	case NNOT:
		calcsize(n->nnot.com);
		break;
	};
}

static char *
nodeckstrdup(char *s)
{
	char *rtn = funcstring;

	strcpy(funcstring, s);
	funcstring += strlen(s) + 1;
	return rtn;
}

static union node *copynode(union node *);

static struct nodelist *
copynodelist(struct nodelist *lp)
{
	struct nodelist *start;
	struct nodelist **lpp;

	lpp = &start;
	while (lp) {
		*lpp = funcblock;
		funcblock = (char *) funcblock + SHELL_ALIGN(sizeof(struct nodelist));
		(*lpp)->n = copynode(lp->n);
		lp = lp->next;
		lpp = &(*lpp)->next;
	}
	*lpp = NULL;
	return start;
}

static union node *
copynode(union node *n)
{
	union node *new;

	if (n == NULL)
		return NULL;
	new = funcblock;
	funcblock = (char *) funcblock + nodesize[n->type];

	switch (n->type) {
	case NCMD:
		new->ncmd.redirect = copynode(n->ncmd.redirect);
		new->ncmd.args = copynode(n->ncmd.args);
		new->ncmd.assign = copynode(n->ncmd.assign);
		break;
	case NPIPE:
		new->npipe.cmdlist = copynodelist(n->npipe.cmdlist);
		new->npipe.backgnd = n->npipe.backgnd;
		break;
	case NREDIR:
	case NBACKGND:
	case NSUBSHELL:
		new->nredir.redirect = copynode(n->nredir.redirect);
		new->nredir.n = copynode(n->nredir.n);
		break;
	case NAND:
	case NOR:
	case NSEMI:
	case NWHILE:
	case NUNTIL:
		new->nbinary.ch2 = copynode(n->nbinary.ch2);
		new->nbinary.ch1 = copynode(n->nbinary.ch1);
		break;
	case NIF:
		new->nif.elsepart = copynode(n->nif.elsepart);
		new->nif.ifpart = copynode(n->nif.ifpart);
		new->nif.test = copynode(n->nif.test);
		break;
	case NFOR:
		new->nfor.var = nodeckstrdup(n->nfor.var);
		new->nfor.body = copynode(n->nfor.body);
		new->nfor.args = copynode(n->nfor.args);
		break;
	case NCASE:
		new->ncase.cases = copynode(n->ncase.cases);
		new->ncase.expr = copynode(n->ncase.expr);
		break;
	case NCLIST:
		new->nclist.body = copynode(n->nclist.body);
		new->nclist.pattern = copynode(n->nclist.pattern);
		new->nclist.next = copynode(n->nclist.next);
		break;
	case NDEFUN:
	case NARG:
		new->narg.backquote = copynodelist(n->narg.backquote);
		new->narg.text = nodeckstrdup(n->narg.text);
		new->narg.next = copynode(n->narg.next);
		break;
	case NTO:
	case NCLOBBER:
	case NFROM:
	case NFROMTO:
	case NAPPEND:
		new->nfile.fname = copynode(n->nfile.fname);
		new->nfile.fd = n->nfile.fd;
		new->nfile.next = copynode(n->nfile.next);
		break;
	case NTOFD:
	case NFROMFD:
		new->ndup.vname = copynode(n->ndup.vname);
		new->ndup.dupfd = n->ndup.dupfd;
		new->ndup.fd = n->ndup.fd;
		new->ndup.next = copynode(n->ndup.next);
		break;
	case NHERE:
	case NXHERE:
		new->nhere.doc = copynode(n->nhere.doc);
		new->nhere.fd = n->nhere.fd;
		new->nhere.next = copynode(n->nhere.next);
		break;
	case NNOT:
		new->nnot.com = copynode(n->nnot.com);
		break;
	};
	new->type = n->type;
	return new;
}

/*
 * Make a copy of a parse tree.
 */
static struct funcnode *
copyfunc(union node *n)
{
	struct funcnode *f;
	size_t blocksize;

	funcblocksize = offsetof(struct funcnode, n);
	funcstringsize = 0;
	calcsize(n);
	blocksize = funcblocksize;
	f = ckmalloc(blocksize + funcstringsize);
	funcblock = (char *) f + offsetof(struct funcnode, n);
	funcstring = (char *) f + blocksize;
	copynode(n);
	f->count = 0;
	return f;
}

/*
 * Define a shell function.
 */
static void
defun(char *name, union node *func)
{
	struct cmdentry entry;

	INT_OFF;
	entry.cmdtype = CMDFUNCTION;
	entry.u.func = copyfunc(func);
	addcmdentry(name, &entry);
	INT_ON;
}

static int evalskip;            /* set if we are skipping commands */
/* reasons for skipping commands (see comment on breakcmd routine) */
#define SKIPBREAK      (1 << 0)
#define SKIPCONT       (1 << 1)
#define SKIPFUNC       (1 << 2)
#define SKIPFILE       (1 << 3)
#define SKIPEVAL       (1 << 4)
static int skipcount;           /* number of levels to skip */
static int funcnest;            /* depth of function calls */
static int loopnest;            /* current loop nesting level */

/* forward decl way out to parsing code - dotrap needs it */
static int evalstring(char *s, int mask);

/*
 * Called to execute a trap.  Perhaps we should avoid entering new trap
 * handlers while we are executing a trap handler.
 */
static int
dotrap(void)
{
	char *p;
	char *q;
	int i;
	int savestatus;
	int skip;

	savestatus = exitstatus;
	pendingsig = 0;
	xbarrier();

	for (i = 0, q = gotsig; i < NSIG - 1; i++, q++) {
		if (!*q)
			continue;
		*q = '\0';

		p = trap[i + 1];
		if (!p)
			continue;
		skip = evalstring(p, SKIPEVAL);
		exitstatus = savestatus;
		if (skip)
			return skip;
	}

	return 0;
}

/* forward declarations - evaluation is fairly recursive business... */
static void evalloop(union node *, int);
static void evalfor(union node *, int);
static void evalcase(union node *, int);
static void evalsubshell(union node *, int);
static void expredir(union node *);
static void evalpipe(union node *, int);
static void evalcommand(union node *, int);
static int evalbltin(const struct builtincmd *, int, char **);
static void prehash(union node *);

/*
 * Evaluate a parse tree.  The value is left in the global variable
 * exitstatus.
 */
static void
evaltree(union node *n, int flags)
{
	int checkexit = 0;
	void (*evalfn)(union node *, int);
	unsigned isor;
	int status;
	if (n == NULL) {
		TRACE(("evaltree(NULL) called\n"));
		goto out;
	}
	TRACE(("pid %d, evaltree(%p: %d, %d) called\n",
			getpid(), n, n->type, flags));
	switch (n->type) {
	default:
#if DEBUG
		out1fmt("Node type = %d\n", n->type);
		fflush(stdout);
		break;
#endif
	case NNOT:
		evaltree(n->nnot.com, EV_TESTED);
		status = !exitstatus;
		goto setstatus;
	case NREDIR:
		expredir(n->nredir.redirect);
		status = redirectsafe(n->nredir.redirect, REDIR_PUSH);
		if (!status) {
			evaltree(n->nredir.n, flags & EV_TESTED);
			status = exitstatus;
		}
		popredir(0);
		goto setstatus;
	case NCMD:
		evalfn = evalcommand;
 checkexit:
		if (eflag && !(flags & EV_TESTED))
			checkexit = ~0;
		goto calleval;
	case NFOR:
		evalfn = evalfor;
		goto calleval;
	case NWHILE:
	case NUNTIL:
		evalfn = evalloop;
		goto calleval;
	case NSUBSHELL:
	case NBACKGND:
		evalfn = evalsubshell;
		goto calleval;
	case NPIPE:
		evalfn = evalpipe;
		goto checkexit;
	case NCASE:
		evalfn = evalcase;
		goto calleval;
	case NAND:
	case NOR:
	case NSEMI:
#if NAND + 1 != NOR
#error NAND + 1 != NOR
#endif
#if NOR + 1 != NSEMI
#error NOR + 1 != NSEMI
#endif
		isor = n->type - NAND;
		evaltree(
			n->nbinary.ch1,
			(flags | ((isor >> 1) - 1)) & EV_TESTED
		);
		if (!exitstatus == isor)
			break;
		if (!evalskip) {
			n = n->nbinary.ch2;
 evaln:
			evalfn = evaltree;
 calleval:
			evalfn(n, flags);
			break;
		}
		break;
	case NIF:
		evaltree(n->nif.test, EV_TESTED);
		if (evalskip)
			break;
		if (exitstatus == 0) {
			n = n->nif.ifpart;
			goto evaln;
		} else if (n->nif.elsepart) {
			n = n->nif.elsepart;
			goto evaln;
		}
		goto success;
	case NDEFUN:
		defun(n->narg.text, n->narg.next);
 success:
		status = 0;
 setstatus:
		exitstatus = status;
		break;
	}
 out:
	if ((checkexit & exitstatus))
		evalskip |= SKIPEVAL;
	else if (pendingsig && dotrap())
		goto exexit;

	if (flags & EV_EXIT) {
 exexit:
		raise_exception(EXEXIT);
	}
}

#if !defined(__alpha__) || (defined(__GNUC__) && __GNUC__ >= 3)
static
#endif
void evaltreenr(union node *, int) __attribute__ ((alias("evaltree"),__noreturn__));

static void
evalloop(union node *n, int flags)
{
	int status;

	loopnest++;
	status = 0;
	flags &= EV_TESTED;
	for (;;) {
		int i;

		evaltree(n->nbinary.ch1, EV_TESTED);
		if (evalskip) {
 skipping:
			if (evalskip == SKIPCONT && --skipcount <= 0) {
				evalskip = 0;
				continue;
			}
			if (evalskip == SKIPBREAK && --skipcount <= 0)
				evalskip = 0;
			break;
		}
		i = exitstatus;
		if (n->type != NWHILE)
			i = !i;
		if (i != 0)
			break;
		evaltree(n->nbinary.ch2, flags);
		status = exitstatus;
		if (evalskip)
			goto skipping;
	}
	loopnest--;
	exitstatus = status;
}

static void
evalfor(union node *n, int flags)
{
	struct arglist arglist;
	union node *argp;
	struct strlist *sp;
	struct stackmark smark;

	setstackmark(&smark);
	arglist.list = NULL;
	arglist.lastp = &arglist.list;
	for (argp = n->nfor.args; argp; argp = argp->narg.next) {
		expandarg(argp, &arglist, EXP_FULL | EXP_TILDE | EXP_RECORD);
		/* XXX */
		if (evalskip)
			goto out;
	}
	*arglist.lastp = NULL;

	exitstatus = 0;
	loopnest++;
	flags &= EV_TESTED;
	for (sp = arglist.list; sp; sp = sp->next) {
		setvar(n->nfor.var, sp->text, 0);
		evaltree(n->nfor.body, flags);
		if (evalskip) {
			if (evalskip == SKIPCONT && --skipcount <= 0) {
				evalskip = 0;
				continue;
			}
			if (evalskip == SKIPBREAK && --skipcount <= 0)
				evalskip = 0;
			break;
		}
	}
	loopnest--;
 out:
	popstackmark(&smark);
}

static void
evalcase(union node *n, int flags)
{
	union node *cp;
	union node *patp;
	struct arglist arglist;
	struct stackmark smark;

	setstackmark(&smark);
	arglist.list = NULL;
	arglist.lastp = &arglist.list;
	expandarg(n->ncase.expr, &arglist, EXP_TILDE);
	exitstatus = 0;
	for (cp = n->ncase.cases; cp && evalskip == 0; cp = cp->nclist.next) {
		for (patp = cp->nclist.pattern; patp; patp = patp->narg.next) {
			if (casematch(patp, arglist.list->text)) {
				if (evalskip == 0) {
					evaltree(cp->nclist.body, flags);
				}
				goto out;
			}
		}
	}
 out:
	popstackmark(&smark);
}

/*
 * Kick off a subshell to evaluate a tree.
 */
static void
evalsubshell(union node *n, int flags)
{
	struct job *jp;
	int backgnd = (n->type == NBACKGND);
	int status;

	expredir(n->nredir.redirect);
	if (!backgnd && flags & EV_EXIT && !trap[0])
		goto nofork;
	INT_OFF;
	jp = makejob(/*n,*/ 1);
	if (forkshell(jp, n, backgnd) == 0) {
		INT_ON;
		flags |= EV_EXIT;
		if (backgnd)
			flags &=~ EV_TESTED;
 nofork:
		redirect(n->nredir.redirect, 0);
		evaltreenr(n->nredir.n, flags);
		/* never returns */
	}
	status = 0;
	if (! backgnd)
		status = waitforjob(jp);
	exitstatus = status;
	INT_ON;
}

/*
 * Compute the names of the files in a redirection list.
 */
static void fixredir(union node *, const char *, int);
static void
expredir(union node *n)
{
	union node *redir;

	for (redir = n; redir; redir = redir->nfile.next) {
		struct arglist fn;

		fn.list = NULL;
		fn.lastp = &fn.list;
		switch (redir->type) {
		case NFROMTO:
		case NFROM:
		case NTO:
		case NCLOBBER:
		case NAPPEND:
			expandarg(redir->nfile.fname, &fn, EXP_TILDE | EXP_REDIR);
			redir->nfile.expfname = fn.list->text;
			break;
		case NFROMFD:
		case NTOFD:
			if (redir->ndup.vname) {
				expandarg(redir->ndup.vname, &fn, EXP_FULL | EXP_TILDE);
				if (fn.list == NULL)
					ash_msg_and_raise_error("redir error");
				fixredir(redir, fn.list->text, 1);
			}
			break;
		}
	}
}

/*
 * Evaluate a pipeline.  All the processes in the pipeline are children
 * of the process creating the pipeline.  (This differs from some versions
 * of the shell, which make the last process in a pipeline the parent
 * of all the rest.)
 */
static void
evalpipe(union node *n, int flags)
{
	struct job *jp;
	struct nodelist *lp;
	int pipelen;
	int prevfd;
	int pip[2];

	TRACE(("evalpipe(0x%lx) called\n", (long)n));
	pipelen = 0;
	for (lp = n->npipe.cmdlist; lp; lp = lp->next)
		pipelen++;
	flags |= EV_EXIT;
	INT_OFF;
	jp = makejob(/*n,*/ pipelen);
	prevfd = -1;
	for (lp = n->npipe.cmdlist; lp; lp = lp->next) {
		prehash(lp->n);
		pip[1] = -1;
		if (lp->next) {
			if (pipe(pip) < 0) {
				close(prevfd);
				ash_msg_and_raise_error("pipe call failed");
			}
		}
		if (forkshell(jp, lp->n, n->npipe.backgnd) == 0) {
			INT_ON;
			if (pip[1] >= 0) {
				close(pip[0]);
			}
			if (prevfd > 0) {
				dup2(prevfd, 0);
				close(prevfd);
			}
			if (pip[1] > 1) {
				dup2(pip[1], 1);
				close(pip[1]);
			}
			evaltreenr(lp->n, flags);
			/* never returns */
		}
		if (prevfd >= 0)
			close(prevfd);
		prevfd = pip[0];
		close(pip[1]);
	}
	if (n->npipe.backgnd == 0) {
		exitstatus = waitforjob(jp);
		TRACE(("evalpipe:  job done exit status %d\n", exitstatus));
	}
	INT_ON;
}

/*
 * Controls whether the shell is interactive or not.
 */
static void
setinteractive(int on)
{
	static smallint is_interactive;

	if (++on == is_interactive)
		return;
	is_interactive = on;
	setsignal(SIGINT);
	setsignal(SIGQUIT);
	setsignal(SIGTERM);
#if !ENABLE_FEATURE_SH_EXTRA_QUIET
	if (is_interactive > 1) {
		/* Looks like they want an interactive shell */
		static smallint did_banner;

		if (!did_banner) {
			out1fmt(
				"\n\n"
				"%s built-in shell (ash)\n"
				"Enter 'help' for a list of built-in commands."
				"\n\n",
				bb_banner);
			did_banner = 1;
		}
	}
#endif
}

static void
optschanged(void)
{
#if DEBUG
	opentrace();
#endif
	setinteractive(iflag);
	setjobctl(mflag);
#if ENABLE_FEATURE_EDITING_VI
	if (viflag)
		line_input_state->flags |= VI_MODE;
	else
		line_input_state->flags &= ~VI_MODE;
#else
	viflag = 0; /* forcibly keep the option off */
#endif
}

static struct localvar *localvars;

/*
 * Called after a function returns.
 * Interrupts must be off.
 */
static void
poplocalvars(void)
{
	struct localvar *lvp;
	struct var *vp;

	while ((lvp = localvars) != NULL) {
		localvars = lvp->next;
		vp = lvp->vp;
		TRACE(("poplocalvar %s", vp ? vp->text : "-"));
		if (vp == NULL) {       /* $- saved */
			memcpy(optlist, lvp->text, sizeof(optlist));
			free((char*)lvp->text);
			optschanged();
		} else if ((lvp->flags & (VUNSET|VSTRFIXED)) == VUNSET) {
			unsetvar(vp->text);
		} else {
			if (vp->func)
				(*vp->func)(strchrnul(lvp->text, '=') + 1);
			if ((vp->flags & (VTEXTFIXED|VSTACK)) == 0)
				free((char*)vp->text);
			vp->flags = lvp->flags;
			vp->text = lvp->text;
		}
		free(lvp);
	}
}

static int
evalfun(struct funcnode *func, int argc, char **argv, int flags)
{
	volatile struct shparam saveparam;
	struct localvar *volatile savelocalvars;
	struct jmploc *volatile savehandler;
	struct jmploc jmploc;
	int e;

	saveparam = shellparam;
	savelocalvars = localvars;
	e = setjmp(jmploc.loc);
	if (e) {
		goto funcdone;
	}
	INT_OFF;
	savehandler = exception_handler;
	exception_handler = &jmploc;
	localvars = NULL;
	shellparam.malloced = 0;
	func->count++;
	funcnest++;
	INT_ON;
	shellparam.nparam = argc - 1;
	shellparam.p = argv + 1;
#if ENABLE_ASH_GETOPTS
	shellparam.optind = 1;
	shellparam.optoff = -1;
#endif
	evaltree(&func->n, flags & EV_TESTED);
 funcdone:
	INT_OFF;
	funcnest--;
	freefunc(func);
	poplocalvars();
	localvars = savelocalvars;
	freeparam(&shellparam);
	shellparam = saveparam;
	exception_handler = savehandler;
	INT_ON;
	evalskip &= ~SKIPFUNC;
	return e;
}

#if ENABLE_ASH_CMDCMD
static char **
parse_command_args(char **argv, const char **path)
{
	char *cp, c;

	for (;;) {
		cp = *++argv;
		if (!cp)
			return 0;
		if (*cp++ != '-')
			break;
		c = *cp++;
		if (!c)
			break;
		if (c == '-' && !*cp) {
			argv++;
			break;
		}
		do {
			switch (c) {
			case 'p':
				*path = bb_default_path;
				break;
			default:
				/* run 'typecmd' for other options */
				return 0;
			}
			c = *cp++;
		} while (c);
	}
	return argv;
}
#endif

/*
 * Make a variable a local variable.  When a variable is made local, it's
 * value and flags are saved in a localvar structure.  The saved values
 * will be restored when the shell function returns.  We handle the name
 * "-" as a special case.
 */
static void
mklocal(char *name)
{
	struct localvar *lvp;
	struct var **vpp;
	struct var *vp;

	INT_OFF;
	lvp = ckzalloc(sizeof(struct localvar));
	if (LONE_DASH(name)) {
		char *p;
		p = ckmalloc(sizeof(optlist));
		lvp->text = memcpy(p, optlist, sizeof(optlist));
		vp = NULL;
	} else {
		char *eq;

		vpp = hashvar(name);
		vp = *findvar(vpp, name);
		eq = strchr(name, '=');
		if (vp == NULL) {
			if (eq)
				setvareq(name, VSTRFIXED);
			else
				setvar(name, NULL, VSTRFIXED);
			vp = *vpp;      /* the new variable */
			lvp->flags = VUNSET;
		} else {
			lvp->text = vp->text;
			lvp->flags = vp->flags;
			vp->flags |= VSTRFIXED|VTEXTFIXED;
			if (eq)
				setvareq(name, 0);
		}
	}
	lvp->vp = vp;
	lvp->next = localvars;
	localvars = lvp;
	INT_ON;
}

/*
 * The "local" command.
 */
static int
localcmd(int argc ATTRIBUTE_UNUSED, char **argv)
{
	char *name;

	argv = argptr;
	while ((name = *argv++) != NULL) {
		mklocal(name);
	}
	return 0;
}

static int
falsecmd(int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED)
{
	return 1;
}

static int
truecmd(int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED)
{
	return 0;
}

static int
execcmd(int argc ATTRIBUTE_UNUSED, char **argv)
{
	if (argv[1]) {
		iflag = 0;              /* exit on error */
		mflag = 0;
		optschanged();
		shellexec(argv + 1, pathval(), 0);
	}
	return 0;
}

/*
 * The return command.
 */
static int
returncmd(int argc ATTRIBUTE_UNUSED, char **argv)
{
	/*
	 * If called outside a function, do what ksh does;
	 * skip the rest of the file.
	 */
	evalskip = funcnest ? SKIPFUNC : SKIPFILE;
	return argv[1] ? number(argv[1]) : exitstatus;
}

/* Forward declarations for builtintab[] */
static int breakcmd(int, char **);
static int dotcmd(int, char **);
static int evalcmd(int, char **);
static int exitcmd(int, char **);
static int exportcmd(int, char **);
#if ENABLE_ASH_GETOPTS
static int getoptscmd(int, char **);
#endif
#if !ENABLE_FEATURE_SH_EXTRA_QUIET
static int helpcmd(int, char **);
#endif
#if ENABLE_ASH_MATH_SUPPORT
static int letcmd(int, char **);
#endif
static int readcmd(int, char **);
static int setcmd(int, char **);
static int shiftcmd(int, char **);
static int timescmd(int, char **);
static int trapcmd(int, char **);
static int umaskcmd(int, char **);
static int unsetcmd(int, char **);
static int ulimitcmd(int, char **);

#define BUILTIN_NOSPEC          "0"
#define BUILTIN_SPECIAL         "1"
#define BUILTIN_REGULAR         "2"
#define BUILTIN_SPEC_REG        "3"
#define BUILTIN_ASSIGN          "4"
#define BUILTIN_SPEC_ASSG       "5"
#define BUILTIN_REG_ASSG        "6"
#define BUILTIN_SPEC_REG_ASSG   "7"

/* We do not handle [[ expr ]] bashism bash-compatibly,
 * we make it a synonym of [ expr ].
 * Basically, word splitting and pathname expansion should NOT be performed
 * Examples:
 * no word splitting:     a="a b"; [[ $a = "a b" ]]; echo $? should print "0"
 * no pathname expansion: [[ /bin/m* = "/bin/m*" ]]; echo $? should print "0"
 * Additional operators:
 * || and && should work as -o and -a
 * =~ regexp match
 * Apart from the above, [[ expr ]] should work as [ expr ]
 */

#define echocmd   echo_main
#define printfcmd printf_main
#define testcmd   test_main

/* Keep these in proper order since it is searched via bsearch() */
static const struct builtincmd builtintab[] = {
	{ BUILTIN_SPEC_REG      ".", dotcmd },
	{ BUILTIN_SPEC_REG      ":", truecmd },
#if ENABLE_ASH_BUILTIN_TEST
	{ BUILTIN_REGULAR       "[", testcmd },
#if ENABLE_ASH_BASH_COMPAT
	{ BUILTIN_REGULAR       "[[", testcmd },
#endif
#endif
#if ENABLE_ASH_ALIAS
	{ BUILTIN_REG_ASSG      "alias", aliascmd },
#endif
#if JOBS
	{ BUILTIN_REGULAR       "bg", fg_bgcmd },
#endif
	{ BUILTIN_SPEC_REG      "break", breakcmd },
	{ BUILTIN_REGULAR       "cd", cdcmd },
	{ BUILTIN_NOSPEC        "chdir", cdcmd },
#if ENABLE_ASH_CMDCMD
	{ BUILTIN_REGULAR       "command", commandcmd },
#endif
	{ BUILTIN_SPEC_REG      "continue", breakcmd },
#if ENABLE_ASH_BUILTIN_ECHO
	{ BUILTIN_REGULAR       "echo", echocmd },
#endif
	{ BUILTIN_SPEC_REG      "eval", evalcmd },
	{ BUILTIN_SPEC_REG      "exec", execcmd },
	{ BUILTIN_SPEC_REG      "exit", exitcmd },
	{ BUILTIN_SPEC_REG_ASSG "export", exportcmd },
	{ BUILTIN_REGULAR       "false", falsecmd },
#if JOBS
	{ BUILTIN_REGULAR       "fg", fg_bgcmd },
#endif
#if ENABLE_ASH_GETOPTS
	{ BUILTIN_REGULAR       "getopts", getoptscmd },
#endif
	{ BUILTIN_NOSPEC        "hash", hashcmd },
#if !ENABLE_FEATURE_SH_EXTRA_QUIET
	{ BUILTIN_NOSPEC        "help", helpcmd },
#endif
#if JOBS
	{ BUILTIN_REGULAR       "jobs", jobscmd },
	{ BUILTIN_REGULAR       "kill", killcmd },
#endif
#if ENABLE_ASH_MATH_SUPPORT
	{ BUILTIN_NOSPEC        "let", letcmd },
#endif
	{ BUILTIN_ASSIGN        "local", localcmd },
#if ENABLE_ASH_BUILTIN_PRINTF
	{ BUILTIN_REGULAR       "printf", printfcmd },
#endif
	{ BUILTIN_NOSPEC        "pwd", pwdcmd },
	{ BUILTIN_REGULAR       "read", readcmd },
	{ BUILTIN_SPEC_REG_ASSG "readonly", exportcmd },
	{ BUILTIN_SPEC_REG      "return", returncmd },
	{ BUILTIN_SPEC_REG      "set", setcmd },
	{ BUILTIN_SPEC_REG      "shift", shiftcmd },
	{ BUILTIN_SPEC_REG      "source", dotcmd },
#if ENABLE_ASH_BUILTIN_TEST
	{ BUILTIN_REGULAR       "test", testcmd },
#endif
	{ BUILTIN_SPEC_REG      "times", timescmd },
	{ BUILTIN_SPEC_REG      "trap", trapcmd },
	{ BUILTIN_REGULAR       "true", truecmd },
	{ BUILTIN_NOSPEC        "type", typecmd },
	{ BUILTIN_NOSPEC        "ulimit", ulimitcmd },
	{ BUILTIN_REGULAR       "umask", umaskcmd },
#if ENABLE_ASH_ALIAS
	{ BUILTIN_REGULAR       "unalias", unaliascmd },
#endif
	{ BUILTIN_SPEC_REG      "unset", unsetcmd },
	{ BUILTIN_REGULAR       "wait", waitcmd },
};

/* Should match the above table! */
#define COMMANDCMD (builtintab + \
	2 + \
	1 * ENABLE_ASH_BUILTIN_TEST + \
	1 * ENABLE_ASH_BUILTIN_TEST * ENABLE_ASH_BASH_COMPAT + \
	1 * ENABLE_ASH_ALIAS + \
	1 * ENABLE_ASH_JOB_CONTROL + \
	3)
#define EXECCMD (builtintab + \
	2 + \
	1 * ENABLE_ASH_BUILTIN_TEST + \
	1 * ENABLE_ASH_BUILTIN_TEST * ENABLE_ASH_BASH_COMPAT + \
	1 * ENABLE_ASH_ALIAS + \
	1 * ENABLE_ASH_JOB_CONTROL + \
	3 + \
	1 * ENABLE_ASH_CMDCMD + \
	1 + \
	ENABLE_ASH_BUILTIN_ECHO + \
	1)

/*
 * Search the table of builtin commands.
 */
static struct builtincmd *
find_builtin(const char *name)
{
	struct builtincmd *bp;

	bp = bsearch(
		name, builtintab, ARRAY_SIZE(builtintab), sizeof(builtintab[0]),
		pstrcmp
	);
	return bp;
}

/*
 * Execute a simple command.
 */
static int
isassignment(const char *p)
{
	const char *q = endofname(p);
	if (p == q)
		return 0;
	return *q == '=';
}
static int
bltincmd(int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED)
{
	/* Preserve exitstatus of a previous possible redirection
	 * as POSIX mandates */
	return back_exitstatus;
}
static void
evalcommand(union node *cmd, int flags)
{
	static const struct builtincmd null_bltin = {
		"\0\0", bltincmd /* why three NULs? */
	};
	struct stackmark smark;
	union node *argp;
	struct arglist arglist;
	struct arglist varlist;
	char **argv;
	int argc;
	const struct strlist *sp;
	struct cmdentry cmdentry;
	struct job *jp;
	char *lastarg;
	const char *path;
	int spclbltin;
	int cmd_is_exec;
	int status;
	char **nargv;
	struct builtincmd *bcmd;
	int pseudovarflag = 0;

	/* First expand the arguments. */
	TRACE(("evalcommand(0x%lx, %d) called\n", (long)cmd, flags));
	setstackmark(&smark);
	back_exitstatus = 0;

	cmdentry.cmdtype = CMDBUILTIN;
	cmdentry.u.cmd = &null_bltin;
	varlist.lastp = &varlist.list;
	*varlist.lastp = NULL;
	arglist.lastp = &arglist.list;
	*arglist.lastp = NULL;

	argc = 0;
	if (cmd->ncmd.args) {
		bcmd = find_builtin(cmd->ncmd.args->narg.text);
		pseudovarflag = bcmd && IS_BUILTIN_ASSIGN(bcmd);
	}

	for (argp = cmd->ncmd.args; argp; argp = argp->narg.next) {
		struct strlist **spp;

		spp = arglist.lastp;
		if (pseudovarflag && isassignment(argp->narg.text))
			expandarg(argp, &arglist, EXP_VARTILDE);
		else
			expandarg(argp, &arglist, EXP_FULL | EXP_TILDE);

		for (sp = *spp; sp; sp = sp->next)
			argc++;
	}

	argv = nargv = stalloc(sizeof(char *) * (argc + 1));
	for (sp = arglist.list; sp; sp = sp->next) {
		TRACE(("evalcommand arg: %s\n", sp->text));
		*nargv++ = sp->text;
	}
	*nargv = NULL;

	lastarg = NULL;
	if (iflag && funcnest == 0 && argc > 0)
		lastarg = nargv[-1];

	preverrout_fd = 2;
	expredir(cmd->ncmd.redirect);
	status = redirectsafe(cmd->ncmd.redirect, REDIR_PUSH | REDIR_SAVEFD2);

	path = vpath.text;
	for (argp = cmd->ncmd.assign; argp; argp = argp->narg.next) {
		struct strlist **spp;
		char *p;

		spp = varlist.lastp;
		expandarg(argp, &varlist, EXP_VARTILDE);

		/*
		 * Modify the command lookup path, if a PATH= assignment
		 * is present
		 */
		p = (*spp)->text;
		if (varequal(p, path))
			path = p;
	}

	/* Print the command if xflag is set. */
	if (xflag) {
		int n;
		const char *p = " %s";

		p++;
		fdprintf(preverrout_fd, p, expandstr(ps4val()));

		sp = varlist.list;
		for (n = 0; n < 2; n++) {
			while (sp) {
				fdprintf(preverrout_fd, p, sp->text);
				sp = sp->next;
				if (*p == '%') {
					p--;
				}
			}
			sp = arglist.list;
		}
		safe_write(preverrout_fd, "\n", 1);
	}

	cmd_is_exec = 0;
	spclbltin = -1;

	/* Now locate the command. */
	if (argc) {
		const char *oldpath;
		int cmd_flag = DO_ERR;

		path += 5;
		oldpath = path;
		for (;;) {
			find_command(argv[0], &cmdentry, cmd_flag, path);
			if (cmdentry.cmdtype == CMDUNKNOWN) {
				status = 127;
				flush_stderr();
				goto bail;
			}

			/* implement bltin and command here */
			if (cmdentry.cmdtype != CMDBUILTIN)
				break;
			if (spclbltin < 0)
				spclbltin = IS_BUILTIN_SPECIAL(cmdentry.u.cmd);
			if (cmdentry.u.cmd == EXECCMD)
				cmd_is_exec++;
#if ENABLE_ASH_CMDCMD
			if (cmdentry.u.cmd == COMMANDCMD) {
				path = oldpath;
				nargv = parse_command_args(argv, &path);
				if (!nargv)
					break;
				argc -= nargv - argv;
				argv = nargv;
				cmd_flag |= DO_NOFUNC;
			} else
#endif
				break;
		}
	}

	if (status) {
		/* We have a redirection error. */
		if (spclbltin > 0)
			raise_exception(EXERROR);
 bail:
		exitstatus = status;
		goto out;
	}

	/* Execute the command. */
	switch (cmdentry.cmdtype) {
	default:
#if ENABLE_FEATURE_SH_NOFORK
	{
		/* find_command() encodes applet_no as (-2 - applet_no) */
		int applet_no = (- cmdentry.u.index - 2);
		if (applet_no >= 0 && APPLET_IS_NOFORK(applet_no)) {
			listsetvar(varlist.list, VEXPORT|VSTACK);
			/* run <applet>_main() */
			exitstatus = run_nofork_applet(applet_no, argv);
			break;
		}
	}
#endif

		/* Fork off a child process if necessary. */
		if (!(flags & EV_EXIT) || trap[0]) {
			INT_OFF;
			jp = makejob(/*cmd,*/ 1);
			if (forkshell(jp, cmd, FORK_FG) != 0) {
				exitstatus = waitforjob(jp);
				INT_ON;
				break;
			}
			FORCE_INT_ON;
		}
		listsetvar(varlist.list, VEXPORT|VSTACK);
		shellexec(argv, path, cmdentry.u.index);
		/* NOTREACHED */

	case CMDBUILTIN:
		cmdenviron = varlist.list;
		if (cmdenviron) {
			struct strlist *list = cmdenviron;
			int i = VNOSET;
			if (spclbltin > 0 || argc == 0) {
				i = 0;
				if (cmd_is_exec && argc > 1)
					i = VEXPORT;
			}
			listsetvar(list, i);
		}
		if (evalbltin(cmdentry.u.cmd, argc, argv)) {
			int exit_status;
			int i = exception;
			if (i == EXEXIT)
				goto raise;
			exit_status = 2;
			if (i == EXINT)
				exit_status = 128 + SIGINT;
			if (i == EXSIG)
				exit_status = 128 + pendingsig;
			exitstatus = exit_status;
			if (i == EXINT || spclbltin > 0) {
 raise:
				longjmp(exception_handler->loc, 1);
			}
			FORCE_INT_ON;
		}
		break;

	case CMDFUNCTION:
		listsetvar(varlist.list, 0);
		if (evalfun(cmdentry.u.func, argc, argv, flags))
			goto raise;
		break;
	}

 out:
	popredir(cmd_is_exec);
	if (lastarg)
		/* dsl: I think this is intended to be used to support
		 * '_' in 'vi' command mode during line editing...
		 * However I implemented that within libedit itself.
		 */
		setvar("_", lastarg, 0);
	popstackmark(&smark);
}

static int
evalbltin(const struct builtincmd *cmd, int argc, char **argv)
{
	char *volatile savecmdname;
	struct jmploc *volatile savehandler;
	struct jmploc jmploc;
	int i;

	savecmdname = commandname;
	i = setjmp(jmploc.loc);
	if (i)
		goto cmddone;
	savehandler = exception_handler;
	exception_handler = &jmploc;
	commandname = argv[0];
	argptr = argv + 1;
	optptr = NULL;                  /* initialize nextopt */
	exitstatus = (*cmd->builtin)(argc, argv);
	flush_stdout_stderr();
 cmddone:
	exitstatus |= ferror(stdout);
	clearerr(stdout);
	commandname = savecmdname;
//	exsig = 0;
	exception_handler = savehandler;

	return i;
}

static int
goodname(const char *p)
{
	return !*endofname(p);
}


/*
 * Search for a command.  This is called before we fork so that the
 * location of the command will be available in the parent as well as
 * the child.  The check for "goodname" is an overly conservative
 * check that the name will not be subject to expansion.
 */
static void
prehash(union node *n)
{
	struct cmdentry entry;

	if (n->type == NCMD && n->ncmd.args && goodname(n->ncmd.args->narg.text))
		find_command(n->ncmd.args->narg.text, &entry, 0, pathval());
}


/* ============ Builtin commands
 *
 * Builtin commands whose functions are closely tied to evaluation
 * are implemented here.
 */

/*
 * Handle break and continue commands.  Break, continue, and return are
 * all handled by setting the evalskip flag.  The evaluation routines
 * above all check this flag, and if it is set they start skipping
 * commands rather than executing them.  The variable skipcount is
 * the number of loops to break/continue, or the number of function
 * levels to return.  (The latter is always 1.)  It should probably
 * be an error to break out of more loops than exist, but it isn't
 * in the standard shell so we don't make it one here.
 */
static int
breakcmd(int argc ATTRIBUTE_UNUSED, char **argv)
{
	int n = argv[1] ? number(argv[1]) : 1;

	if (n <= 0)
		ash_msg_and_raise_error(illnum, argv[1]);
	if (n > loopnest)
		n = loopnest;
	if (n > 0) {
		evalskip = (**argv == 'c') ? SKIPCONT : SKIPBREAK;
		skipcount = n;
	}
	return 0;
}


/* ============ input.c
 *
 * This implements the input routines used by the parser.
 */

#define EOF_NLEFT -99           /* value of parsenleft when EOF pushed back */

enum {
	INPUT_PUSH_FILE = 1,
	INPUT_NOFILE_OK = 2,
};

static int plinno = 1;                  /* input line number */
/* number of characters left in input buffer */
static int parsenleft;                  /* copy of parsefile->nleft */
static int parselleft;                  /* copy of parsefile->lleft */
/* next character in input buffer */
static char *parsenextc;                /* copy of parsefile->nextc */

static smallint checkkwd;
/* values of checkkwd variable */
#define CHKALIAS        0x1
#define CHKKWD          0x2
#define CHKNL           0x4

static void
popstring(void)
{
	struct strpush *sp = g_parsefile->strpush;

	INT_OFF;
#if ENABLE_ASH_ALIAS
	if (sp->ap) {
		if (parsenextc[-1] == ' ' || parsenextc[-1] == '\t') {
			checkkwd |= CHKALIAS;
		}
		if (sp->string != sp->ap->val) {
			free(sp->string);
		}
		sp->ap->flag &= ~ALIASINUSE;
		if (sp->ap->flag & ALIASDEAD) {
			unalias(sp->ap->name);
		}
	}
#endif
	parsenextc = sp->prevstring;
	parsenleft = sp->prevnleft;
/*dprintf("*** calling popstring: restoring to '%s'\n", parsenextc);*/
	g_parsefile->strpush = sp->prev;
	if (sp != &(g_parsefile->basestrpush))
		free(sp);
	INT_ON;
}

static int
preadfd(void)
{
	int nr;
	char *buf =  g_parsefile->buf;
	parsenextc = buf;

#if ENABLE_FEATURE_EDITING
 retry:
	if (!iflag || g_parsefile->fd)
		nr = nonblock_safe_read(g_parsefile->fd, buf, BUFSIZ - 1);
	else {
#if ENABLE_FEATURE_TAB_COMPLETION
		line_input_state->path_lookup = pathval();
#endif
		nr = read_line_input(cmdedit_prompt, buf, BUFSIZ, line_input_state);
		if (nr == 0) {
			/* Ctrl+C pressed */
			if (trap[SIGINT]) {
				buf[0] = '\n';
				buf[1] = '\0';
				raise(SIGINT);
				return 1;
			}
			goto retry;
		}
		if (nr < 0 && errno == 0) {
			/* Ctrl+D pressed */
			nr = 0;
		}
	}
#else
	nr = nonblock_safe_read(g_parsefile->fd, buf, BUFSIZ - 1);
#endif

#if 0
/* nonblock_safe_read() handles this problem */
	if (nr < 0) {
		if (parsefile->fd == 0 && errno == EWOULDBLOCK) {
			int flags = fcntl(0, F_GETFL);
			if (flags >= 0 && (flags & O_NONBLOCK)) {
				flags &= ~O_NONBLOCK;
				if (fcntl(0, F_SETFL, flags) >= 0) {
					out2str("sh: turning off NDELAY mode\n");
					goto retry;
				}
			}
		}
	}
#endif
	return nr;
}

/*
 * Refill the input buffer and return the next input character:
 *
 * 1) If a string was pushed back on the input, pop it;
 * 2) If an EOF was pushed back (parsenleft == EOF_NLEFT) or we are reading
 *    from a string so we can't refill the buffer, return EOF.
 * 3) If the is more stuff in this buffer, use it else call read to fill it.
 * 4) Process input up to the next newline, deleting nul characters.
 */
static int
preadbuffer(void)
{
	char *q;
	int more;
	char savec;

	while (g_parsefile->strpush) {
#if ENABLE_ASH_ALIAS
		if (parsenleft == -1 && g_parsefile->strpush->ap &&
			parsenextc[-1] != ' ' && parsenextc[-1] != '\t') {
			return PEOA;
		}
#endif
		popstring();
		if (--parsenleft >= 0)
			return signed_char2int(*parsenextc++);
	}
	if (parsenleft == EOF_NLEFT || g_parsefile->buf == NULL)
		return PEOF;
	flush_stdout_stderr();

	more = parselleft;
	if (more <= 0) {
 again:
		more = preadfd();
		if (more <= 0) {
			parselleft = parsenleft = EOF_NLEFT;
			return PEOF;
		}
	}

	q = parsenextc;

	/* delete nul characters */
	for (;;) {
		int c;

		more--;
		c = *q;

		if (!c)
			memmove(q, q + 1, more);
		else {
			q++;
			if (c == '\n') {
				parsenleft = q - parsenextc - 1;
				break;
			}
		}

		if (more <= 0) {
			parsenleft = q - parsenextc - 1;
			if (parsenleft < 0)
				goto again;
			break;
		}
	}
	parselleft = more;

	savec = *q;
	*q = '\0';

	if (vflag) {
		out2str(parsenextc);
	}

	*q = savec;

	return signed_char2int(*parsenextc++);
}

#define pgetc_as_macro() (--parsenleft >= 0? signed_char2int(*parsenextc++) : preadbuffer())
static int
pgetc(void)
{
	return pgetc_as_macro();
}

#if ENABLE_ASH_OPTIMIZE_FOR_SIZE
#define pgetc_macro() pgetc()
#else
#define pgetc_macro() pgetc_as_macro()
#endif

/*
 * Same as pgetc(), but ignores PEOA.
 */
#if ENABLE_ASH_ALIAS
static int
pgetc2(void)
{
	int c;

	do {
		c = pgetc_macro();
	} while (c == PEOA);
	return c;
}
#else
static int
pgetc2(void)
{
	return pgetc_macro();
}
#endif

/*
 * Read a line from the script.
 */
static char *
pfgets(char *line, int len)
{
	char *p = line;
	int nleft = len;
	int c;

	while (--nleft > 0) {
		c = pgetc2();
		if (c == PEOF) {
			if (p == line)
				return NULL;
			break;
		}
		*p++ = c;
		if (c == '\n')
			break;
	}
	*p = '\0';
	return line;
}

/*
 * Undo the last call to pgetc.  Only one character may be pushed back.
 * PEOF may be pushed back.
 */
static void
pungetc(void)
{
	parsenleft++;
	parsenextc--;
}

/*
 * Push a string back onto the input at this current parsefile level.
 * We handle aliases this way.
 */
#if !ENABLE_ASH_ALIAS
#define pushstring(s, ap) pushstring(s)
#endif
static void
pushstring(char *s, struct alias *ap)
{
	struct strpush *sp;
	size_t len;

	len = strlen(s);
	INT_OFF;
/*dprintf("*** calling pushstring: %s, %d\n", s, len);*/
	if (g_parsefile->strpush) {
		sp = ckzalloc(sizeof(struct strpush));
		sp->prev = g_parsefile->strpush;
		g_parsefile->strpush = sp;
	} else
		sp = g_parsefile->strpush = &(g_parsefile->basestrpush);
	sp->prevstring = parsenextc;
	sp->prevnleft = parsenleft;
#if ENABLE_ASH_ALIAS
	sp->ap = ap;
	if (ap) {
		ap->flag |= ALIASINUSE;
		sp->string = s;
	}
#endif
	parsenextc = s;
	parsenleft = len;
	INT_ON;
}

/*
 * To handle the "." command, a stack of input files is used.  Pushfile
 * adds a new entry to the stack and popfile restores the previous level.
 */
static void
pushfile(void)
{
	struct parsefile *pf;

	g_parsefile->nleft = parsenleft;
	g_parsefile->lleft = parselleft;
	g_parsefile->nextc = parsenextc;
	g_parsefile->linno = plinno;
	pf = ckzalloc(sizeof(*pf));
	pf->prev = g_parsefile;
	pf->fd = -1;
	/*pf->strpush = NULL; - ckzalloc did it */
	/*pf->basestrpush.prev = NULL;*/
	g_parsefile = pf;
}

static void
popfile(void)
{
	struct parsefile *pf = g_parsefile;

	INT_OFF;
	if (pf->fd >= 0)
		close(pf->fd);
	free(pf->buf);
	while (pf->strpush)
		popstring();
	g_parsefile = pf->prev;
	free(pf);
	parsenleft = g_parsefile->nleft;
	parselleft = g_parsefile->lleft;
	parsenextc = g_parsefile->nextc;
	plinno = g_parsefile->linno;
	INT_ON;
}

/*
 * Return to top level.
 */
static void
popallfiles(void)
{
	while (g_parsefile != &basepf)
		popfile();
}

/*
 * Close the file(s) that the shell is reading commands from.  Called
 * after a fork is done.
 */
static void
closescript(void)
{
	popallfiles();
	if (g_parsefile->fd > 0) {
		close(g_parsefile->fd);
		g_parsefile->fd = 0;
	}
}

/*
 * Like setinputfile, but takes an open file descriptor.  Call this with
 * interrupts off.
 */
static void
setinputfd(int fd, int push)
{
	close_on_exec_on(fd);
	if (push) {
		pushfile();
		g_parsefile->buf = 0;
	}
	g_parsefile->fd = fd;
	if (g_parsefile->buf == NULL)
		g_parsefile->buf = ckmalloc(IBUFSIZ);
	parselleft = parsenleft = 0;
	plinno = 1;
}

/*
 * Set the input to take input from a file.  If push is set, push the
 * old input onto the stack first.
 */
static int
setinputfile(const char *fname, int flags)
{
	int fd;
	int fd2;

	INT_OFF;
	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		if (flags & INPUT_NOFILE_OK)
			goto out;
		ash_msg_and_raise_error("can't open %s", fname);
	}
	if (fd < 10) {
		fd2 = copyfd(fd, 10);
		close(fd);
		if (fd2 < 0)
			ash_msg_and_raise_error("out of file descriptors");
		fd = fd2;
	}
	setinputfd(fd, flags & INPUT_PUSH_FILE);
 out:
	INT_ON;
	return fd;
}

/*
 * Like setinputfile, but takes input from a string.
 */
static void
setinputstring(char *string)
{
	INT_OFF;
	pushfile();
	parsenextc = string;
	parsenleft = strlen(string);
	g_parsefile->buf = NULL;
	plinno = 1;
	INT_ON;
}


/* ============ mail.c
 *
 * Routines to check for mail.
 */

#if ENABLE_ASH_MAIL

#define MAXMBOXES 10

/* times of mailboxes */
static time_t mailtime[MAXMBOXES];
/* Set if MAIL or MAILPATH is changed. */
static smallint mail_var_path_changed;

/*
 * Print appropriate message(s) if mail has arrived.
 * If mail_var_path_changed is set,
 * then the value of MAIL has mail_var_path_changed,
 * so we just update the values.
 */
static void
chkmail(void)
{
	const char *mpath;
	char *p;
	char *q;
	time_t *mtp;
	struct stackmark smark;
	struct stat statb;

	setstackmark(&smark);
	mpath = mpathset() ? mpathval() : mailval();
	for (mtp = mailtime; mtp < mailtime + MAXMBOXES; mtp++) {
		p = padvance(&mpath, nullstr);
		if (p == NULL)
			break;
		if (*p == '\0')
			continue;
		for (q = p; *q; q++)
			continue;
#if DEBUG
		if (q[-1] != '/')
			abort();
#endif
		q[-1] = '\0';                   /* delete trailing '/' */
		if (stat(p, &statb) < 0) {
			*mtp = 0;
			continue;
		}
		if (!mail_var_path_changed && statb.st_mtime != *mtp) {
			fprintf(
				stderr, snlfmt,
				pathopt ? pathopt : "you have mail"
			);
		}
		*mtp = statb.st_mtime;
	}
	mail_var_path_changed = 0;
	popstackmark(&smark);
}

static void
changemail(const char *val ATTRIBUTE_UNUSED)
{
	mail_var_path_changed = 1;
}

#endif /* ASH_MAIL */


/* ============ ??? */

/*
 * Set the shell parameters.
 */
static void
setparam(char **argv)
{
	char **newparam;
	char **ap;
	int nparam;

	for (nparam = 0; argv[nparam]; nparam++)
		continue;
	ap = newparam = ckmalloc((nparam + 1) * sizeof(*ap));
	while (*argv) {
		*ap++ = ckstrdup(*argv++);
	}
	*ap = NULL;
	freeparam(&shellparam);
	shellparam.malloced = 1;
	shellparam.nparam = nparam;
	shellparam.p = newparam;
#if ENABLE_ASH_GETOPTS
	shellparam.optind = 1;
	shellparam.optoff = -1;
#endif
}

/*
 * Process shell options.  The global variable argptr contains a pointer
 * to the argument list; we advance it past the options.
 *
 * SUSv3 section 2.8.1 "Consequences of Shell Errors" says:
 * For a non-interactive shell, an error condition encountered
 * by a special built-in ... shall cause the shell to write a diagnostic message
 * to standard error and exit as shown in the following table:
 * Error                                           Special Built-In
 * ...
 * Utility syntax error (option or operand error)  Shall exit
 * ...
 * However, in bug 1142 (http://busybox.net/bugs/view.php?id=1142)
 * we see that bash does not do that (set "finishes" with error code 1 instead,
 * and shell continues), and people rely on this behavior!
 * Testcase:
 * set -o barfoo 2>/dev/null
 * echo $?
 *
 * Oh well. Let's mimic that.
 */
static int
plus_minus_o(char *name, int val)
{
	int i;

	if (name) {
		for (i = 0; i < NOPTS; i++) {
			if (strcmp(name, optnames(i)) == 0) {
				optlist[i] = val;
				return 0;
			}
		}
		ash_msg("illegal option %co %s", val ? '-' : '+', name);
		return 1;
	}
	for (i = 0; i < NOPTS; i++) {
		if (val) {
			out1fmt("%-16s%s\n", optnames(i), optlist[i] ? "on" : "off");
		} else {
			out1fmt("set %co %s\n", optlist[i] ? '-' : '+', optnames(i));
		}
	}
	return 0;
}
static void
setoption(int flag, int val)
{
	int i;

	for (i = 0; i < NOPTS; i++) {
		if (optletters(i) == flag) {
			optlist[i] = val;
			return;
		}
	}
	ash_msg_and_raise_error("illegal option %c%c", val ? '-' : '+', flag);
	/* NOTREACHED */
}
static int
options(int cmdline)
{
	char *p;
	int val;
	int c;

	if (cmdline)
		minusc = NULL;
	while ((p = *argptr) != NULL) {
		c = *p++;
		if (c != '-' && c != '+')
			break;
		argptr++;
		val = 0; /* val = 0 if c == '+' */
		if (c == '-') {
			val = 1;
			if (p[0] == '\0' || LONE_DASH(p)) {
				if (!cmdline) {
					/* "-" means turn off -x and -v */
					if (p[0] == '\0')
						xflag = vflag = 0;
					/* "--" means reset params */
					else if (*argptr == NULL)
						setparam(argptr);
				}
				break;    /* "-" or  "--" terminates options */
			}
		}
		/* first char was + or - */
		while ((c = *p++) != '\0') {
			/* bash 3.2 indeed handles -c CMD and +c CMD the same */
			if (c == 'c' && cmdline) {
				minusc = p;     /* command is after shell args */
			} else if (c == 'o') {
				if (plus_minus_o(*argptr, val)) {
					/* it already printed err message */
					return 1; /* error */
				}
				if (*argptr)
					argptr++;
			} else if (cmdline && (c == 'l')) { /* -l or +l == --login */
				isloginsh = 1;
			/* bash does not accept +-login, we also won't */
			} else if (cmdline && val && (c == '-')) { /* long options */
				if (strcmp(p, "login") == 0)
					isloginsh = 1;
				break;
			} else {
				setoption(c, val);
			}
		}
	}
	return 0;
}

/*
 * The shift builtin command.
 */
static int
shiftcmd(int argc ATTRIBUTE_UNUSED, char **argv)
{
	int n;
	char **ap1, **ap2;

	n = 1;
	if (argv[1])
		n = number(argv[1]);
	if (n > shellparam.nparam)
		n = shellparam.nparam;
	INT_OFF;
	shellparam.nparam -= n;
	for (ap1 = shellparam.p; --n >= 0; ap1++) {
		if (shellparam.malloced)
			free(*ap1);
	}
	ap2 = shellparam.p;
	while ((*ap2++ = *ap1++) != NULL)
		continue;
#if ENABLE_ASH_GETOPTS
	shellparam.optind = 1;
	shellparam.optoff = -1;
#endif
	INT_ON;
	return 0;
}

/*
 * POSIX requires that 'set' (but not export or readonly) output the
 * variables in lexicographic order - by the locale's collating order (sigh).
 * Maybe we could keep them in an ordered balanced binary tree
 * instead of hashed lists.
 * For now just roll 'em through qsort for printing...
 */
static int
showvars(const char *sep_prefix, int on, int off)
{
	const char *sep;
	char **ep, **epend;

	ep = listvars(on, off, &epend);
	qsort(ep, epend - ep, sizeof(char *), vpcmp);

	sep = *sep_prefix ? " " : sep_prefix;

	for (; ep < epend; ep++) {
		const char *p;
		const char *q;

		p = strchrnul(*ep, '=');
		q = nullstr;
		if (*p)
			q = single_quote(++p);
		out1fmt("%s%s%.*s%s\n", sep_prefix, sep, (int)(p - *ep), *ep, q);
	}
	return 0;
}

/*
 * The set command builtin.
 */
static int
setcmd(int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED)
{
	int retval;

	if (!argv[1])
		return showvars(nullstr, 0, VUNSET);
	INT_OFF;
	retval = 1;
	if (!options(0)) { /* if no parse error... */
		retval = 0;
		optschanged();
		if (*argptr != NULL) {
			setparam(argptr);
		}
	}
	INT_ON;
	return retval;
}

#if ENABLE_ASH_RANDOM_SUPPORT
/* Roughly copied from bash.. */
static void
change_random(const char *value)
{
	if (value == NULL) {
		/* "get", generate */
		char buf[16];

		rseed = rseed * 1103515245 + 12345;
		sprintf(buf, "%d", (unsigned int)((rseed & 32767)));
		/* set without recursion */
		setvar(vrandom.text, buf, VNOFUNC);
		vrandom.flags &= ~VNOFUNC;
	} else {
		/* set/reset */
		rseed = strtoul(value, (char **)NULL, 10);
	}
}
#endif

#if ENABLE_ASH_GETOPTS
static int
getopts(char *optstr, char *optvar, char **optfirst, int *param_optind, int *optoff)
{
	char *p, *q;
	char c = '?';
	int done = 0;
	int err = 0;
	char s[12];
	char **optnext;

	if (*param_optind < 1)
		return 1;
	optnext = optfirst + *param_optind - 1;

	if (*param_optind <= 1 || *optoff < 0 || (int)strlen(optnext[-1]) < *optoff)
		p = NULL;
	else
		p = optnext[-1] + *optoff;
	if (p == NULL || *p == '\0') {
		/* Current word is done, advance */
		p = *optnext;
		if (p == NULL || *p != '-' || *++p == '\0') {
 atend:
			p = NULL;
			done = 1;
			goto out;
		}
		optnext++;
		if (LONE_DASH(p))        /* check for "--" */
			goto atend;
	}

	c = *p++;
	for (q = optstr; *q != c;) {
		if (*q == '\0') {
			if (optstr[0] == ':') {
				s[0] = c;
				s[1] = '\0';
				err |= setvarsafe("OPTARG", s, 0);
			} else {
				fprintf(stderr, "Illegal option -%c\n", c);
				unsetvar("OPTARG");
			}
			c = '?';
			goto out;
		}
		if (*++q == ':')
			q++;
	}

	if (*++q == ':') {
		if (*p == '\0' && (p = *optnext) == NULL) {
			if (optstr[0] == ':') {
				s[0] = c;
				s[1] = '\0';
				err |= setvarsafe("OPTARG", s, 0);
				c = ':';
			} else {
				fprintf(stderr, "No arg for -%c option\n", c);
				unsetvar("OPTARG");
				c = '?';
			}
			goto out;
		}

		if (p == *optnext)
			optnext++;
		err |= setvarsafe("OPTARG", p, 0);
		p = NULL;
	} else
		err |= setvarsafe("OPTARG", nullstr, 0);
 out:
	*optoff = p ? p - *(optnext - 1) : -1;
	*param_optind = optnext - optfirst + 1;
	fmtstr(s, sizeof(s), "%d", *param_optind);
	err |= setvarsafe("OPTIND", s, VNOFUNC);
	s[0] = c;
	s[1] = '\0';
	err |= setvarsafe(optvar, s, 0);
	if (err) {
		*param_optind = 1;
		*optoff = -1;
		flush_stdout_stderr();
		raise_exception(EXERROR);
	}
	return done;
}

/*
 * The getopts builtin.  Shellparam.optnext points to the next argument
 * to be processed.  Shellparam.optptr points to the next character to
 * be processed in the current argument.  If shellparam.optnext is NULL,
 * then it's the first time getopts has been called.
 */
static int
getoptscmd(int argc, char **argv)
{
	char **optbase;

	if (argc < 3)
		ash_msg_and_raise_error("usage: getopts optstring var [arg]");
	if (argc == 3) {
		optbase = shellparam.p;
		if (shellparam.optind > shellparam.nparam + 1) {
			shellparam.optind = 1;
			shellparam.optoff = -1;
		}
	} else {
		optbase = &argv[3];
		if (shellparam.optind > argc - 2) {
			shellparam.optind = 1;
			shellparam.optoff = -1;
		}
	}

	return getopts(argv[1], argv[2], optbase, &shellparam.optind,
			&shellparam.optoff);
}
#endif /* ASH_GETOPTS */


/* ============ Shell parser */

struct heredoc {
	struct heredoc *next;   /* next here document in list */
	union node *here;       /* redirection node */
	char *eofmark;          /* string indicating end of input */
	smallint striptabs;     /* if set, strip leading tabs */
};

static smallint tokpushback;           /* last token pushed back */
static smallint parsebackquote;        /* nonzero if we are inside backquotes */
static smallint quoteflag;             /* set if (part of) last token was quoted */
static token_id_t lasttoken;           /* last token read (integer id Txxx) */
static struct heredoc *heredoclist;    /* list of here documents to read */
static char *wordtext;                 /* text of last word returned by readtoken */
static struct nodelist *backquotelist;
static union node *redirnode;
static struct heredoc *heredoc;
/*
 * NEOF is returned by parsecmd when it encounters an end of file.  It
 * must be distinct from NULL, so we use the address of a variable that
 * happens to be handy.
 */
#define NEOF ((union node *)&tokpushback)

static void raise_error_syntax(const char *) ATTRIBUTE_NORETURN;
static void
raise_error_syntax(const char *msg)
{
	ash_msg_and_raise_error("syntax error: %s", msg);
	/* NOTREACHED */
}

/*
 * Called when an unexpected token is read during the parse.  The argument
 * is the token that is expected, or -1 if more than one type of token can
 * occur at this point.
 */
static void raise_error_unexpected_syntax(int) ATTRIBUTE_NORETURN;
static void
raise_error_unexpected_syntax(int token)
{
	char msg[64];
	int l;

	l = sprintf(msg, "%s unexpected", tokname(lasttoken));
	if (token >= 0)
		sprintf(msg + l, " (expecting %s)", tokname(token));
	raise_error_syntax(msg);
	/* NOTREACHED */
}

#define EOFMARKLEN 79

/* parsing is heavily cross-recursive, need these forward decls */
static union node *andor(void);
static union node *pipeline(void);
static union node *parse_command(void);
static void parseheredoc(void);
static char peektoken(void);
static int readtoken(void);

static union node *
list(int nlflag)
{
	union node *n1, *n2, *n3;
	int tok;

	checkkwd = CHKNL | CHKKWD | CHKALIAS;
	if (nlflag == 2 && peektoken())
		return NULL;
	n1 = NULL;
	for (;;) {
		n2 = andor();
		tok = readtoken();
		if (tok == TBACKGND) {
			if (n2->type == NPIPE) {
				n2->npipe.backgnd = 1;
			} else {
				if (n2->type != NREDIR) {
					n3 = stzalloc(sizeof(struct nredir));
					n3->nredir.n = n2;
					/*n3->nredir.redirect = NULL; - stzalloc did it */
					n2 = n3;
				}
				n2->type = NBACKGND;
			}
		}
		if (n1 == NULL) {
			n1 = n2;
		} else {
			n3 = stzalloc(sizeof(struct nbinary));
			n3->type = NSEMI;
			n3->nbinary.ch1 = n1;
			n3->nbinary.ch2 = n2;
			n1 = n3;
		}
		switch (tok) {
		case TBACKGND:
		case TSEMI:
			tok = readtoken();
			/* fall through */
		case TNL:
			if (tok == TNL) {
				parseheredoc();
				if (nlflag == 1)
					return n1;
			} else {
				tokpushback = 1;
			}
			checkkwd = CHKNL | CHKKWD | CHKALIAS;
			if (peektoken())
				return n1;
			break;
		case TEOF:
			if (heredoclist)
				parseheredoc();
			else
				pungetc();              /* push back EOF on input */
			return n1;
		default:
			if (nlflag == 1)
				raise_error_unexpected_syntax(-1);
			tokpushback = 1;
			return n1;
		}
	}
}

static union node *
andor(void)
{
	union node *n1, *n2, *n3;
	int t;

	n1 = pipeline();
	for (;;) {
		t = readtoken();
		if (t == TAND) {
			t = NAND;
		} else if (t == TOR) {
			t = NOR;
		} else {
			tokpushback = 1;
			return n1;
		}
		checkkwd = CHKNL | CHKKWD | CHKALIAS;
		n2 = pipeline();
		n3 = stzalloc(sizeof(struct nbinary));
		n3->type = t;
		n3->nbinary.ch1 = n1;
		n3->nbinary.ch2 = n2;
		n1 = n3;
	}
}

static union node *
pipeline(void)
{
	union node *n1, *n2, *pipenode;
	struct nodelist *lp, *prev;
	int negate;

	negate = 0;
	TRACE(("pipeline: entered\n"));
	if (readtoken() == TNOT) {
		negate = !negate;
		checkkwd = CHKKWD | CHKALIAS;
	} else
		tokpushback = 1;
	n1 = parse_command();
	if (readtoken() == TPIPE) {
		pipenode = stzalloc(sizeof(struct npipe));
		pipenode->type = NPIPE;
		/*pipenode->npipe.backgnd = 0; - stzalloc did it */
		lp = stzalloc(sizeof(struct nodelist));
		pipenode->npipe.cmdlist = lp;
		lp->n = n1;
		do {
			prev = lp;
			lp = stzalloc(sizeof(struct nodelist));
			checkkwd = CHKNL | CHKKWD | CHKALIAS;
			lp->n = parse_command();
			prev->next = lp;
		} while (readtoken() == TPIPE);
		lp->next = NULL;
		n1 = pipenode;
	}
	tokpushback = 1;
	if (negate) {
		n2 = stzalloc(sizeof(struct nnot));
		n2->type = NNOT;
		n2->nnot.com = n1;
		return n2;
	}
	return n1;
}

static union node *
makename(void)
{
	union node *n;

	n = stzalloc(sizeof(struct narg));
	n->type = NARG;
	/*n->narg.next = NULL; - stzalloc did it */
	n->narg.text = wordtext;
	n->narg.backquote = backquotelist;
	return n;
}

static void
fixredir(union node *n, const char *text, int err)
{
	TRACE(("Fix redir %s %d\n", text, err));
	if (!err)
		n->ndup.vname = NULL;

	if (isdigit(text[0]) && text[1] == '\0')
		n->ndup.dupfd = text[0] - '0';
	else if (LONE_DASH(text))
		n->ndup.dupfd = -1;
	else {
		if (err)
			raise_error_syntax("Bad fd number");
		n->ndup.vname = makename();
	}
}

/*
 * Returns true if the text contains nothing to expand (no dollar signs
 * or backquotes).
 */
static int
noexpand(char *text)
{
	char *p;
	char c;

	p = text;
	while ((c = *p++) != '\0') {
		if (c == CTLQUOTEMARK)
			continue;
		if (c == CTLESC)
			p++;
		else if (SIT(c, BASESYNTAX) == CCTL)
			return 0;
	}
	return 1;
}

static void
parsefname(void)
{
	union node *n = redirnode;

	if (readtoken() != TWORD)
		raise_error_unexpected_syntax(-1);
	if (n->type == NHERE) {
		struct heredoc *here = heredoc;
		struct heredoc *p;
		int i;

		if (quoteflag == 0)
			n->type = NXHERE;
		TRACE(("Here document %d\n", n->type));
		if (!noexpand(wordtext) || (i = strlen(wordtext)) == 0 || i > EOFMARKLEN)
			raise_error_syntax("Illegal eof marker for << redirection");
		rmescapes(wordtext);
		here->eofmark = wordtext;
		here->next = NULL;
		if (heredoclist == NULL)
			heredoclist = here;
		else {
			for (p = heredoclist; p->next; p = p->next)
				continue;
			p->next = here;
		}
	} else if (n->type == NTOFD || n->type == NFROMFD) {
		fixredir(n, wordtext, 0);
	} else {
		n->nfile.fname = makename();
	}
}

static union node *
simplecmd(void)
{
	union node *args, **app;
	union node *n = NULL;
	union node *vars, **vpp;
	union node **rpp, *redir;
	int savecheckkwd;
#if ENABLE_ASH_BASH_COMPAT
	smallint double_brackets_flag = 0;
#endif

	args = NULL;
	app = &args;
	vars = NULL;
	vpp = &vars;
	redir = NULL;
	rpp = &redir;

	savecheckkwd = CHKALIAS;
	for (;;) {
		int t;
		checkkwd = savecheckkwd;
		t = readtoken();
		switch (t) {
#if ENABLE_ASH_BASH_COMPAT
		case TAND: /* "&&" */
		case TOR: /* "||" */
			if (!double_brackets_flag) {
				tokpushback = 1;
				goto out;
			}
			wordtext = (char *) (t == TAND ? "-a" : "-o");
#endif
		case TWORD:
			n = stzalloc(sizeof(struct narg));
			n->type = NARG;
			/*n->narg.next = NULL; - stzalloc did it */
			n->narg.text = wordtext;
#if ENABLE_ASH_BASH_COMPAT
			if (strcmp("[[", wordtext) == 0)
				double_brackets_flag = 1;
			else if (strcmp("]]", wordtext) == 0)
				double_brackets_flag = 0;
#endif
			n->narg.backquote = backquotelist;
			if (savecheckkwd && isassignment(wordtext)) {
				*vpp = n;
				vpp = &n->narg.next;
			} else {
				*app = n;
				app = &n->narg.next;
				savecheckkwd = 0;
			}
			break;
		case TREDIR:
			*rpp = n = redirnode;
			rpp = &n->nfile.next;
			parsefname();   /* read name of redirection file */
			break;
		case TLP:
			if (args && app == &args->narg.next
			 && !vars && !redir
			) {
				struct builtincmd *bcmd;
				const char *name;

				/* We have a function */
				if (readtoken() != TRP)
					raise_error_unexpected_syntax(TRP);
				name = n->narg.text;
				if (!goodname(name)
				 || ((bcmd = find_builtin(name)) && IS_BUILTIN_SPECIAL(bcmd))
				) {
					raise_error_syntax("Bad function name");
				}
				n->type = NDEFUN;
				checkkwd = CHKNL | CHKKWD | CHKALIAS;
				n->narg.next = parse_command();
				return n;
			}
			/* fall through */
		default:
			tokpushback = 1;
			goto out;
		}
	}
 out:
	*app = NULL;
	*vpp = NULL;
	*rpp = NULL;
	n = stzalloc(sizeof(struct ncmd));
	n->type = NCMD;
	n->ncmd.args = args;
	n->ncmd.assign = vars;
	n->ncmd.redirect = redir;
	return n;
}

static union node *
parse_command(void)
{
	union node *n1, *n2;
	union node *ap, **app;
	union node *cp, **cpp;
	union node *redir, **rpp;
	union node **rpp2;
	int t;

	redir = NULL;
	rpp2 = &redir;

	switch (readtoken()) {
	default:
		raise_error_unexpected_syntax(-1);
		/* NOTREACHED */
	case TIF:
		n1 = stzalloc(sizeof(struct nif));
		n1->type = NIF;
		n1->nif.test = list(0);
		if (readtoken() != TTHEN)
			raise_error_unexpected_syntax(TTHEN);
		n1->nif.ifpart = list(0);
		n2 = n1;
		while (readtoken() == TELIF) {
			n2->nif.elsepart = stzalloc(sizeof(struct nif));
			n2 = n2->nif.elsepart;
			n2->type = NIF;
			n2->nif.test = list(0);
			if (readtoken() != TTHEN)
				raise_error_unexpected_syntax(TTHEN);
			n2->nif.ifpart = list(0);
		}
		if (lasttoken == TELSE)
			n2->nif.elsepart = list(0);
		else {
			n2->nif.elsepart = NULL;
			tokpushback = 1;
		}
		t = TFI;
		break;
	case TWHILE:
	case TUNTIL: {
		int got;
		n1 = stzalloc(sizeof(struct nbinary));
		n1->type = (lasttoken == TWHILE) ? NWHILE : NUNTIL;
		n1->nbinary.ch1 = list(0);
		got = readtoken();
		if (got != TDO) {
			TRACE(("expecting DO got %s %s\n", tokname(got),
					got == TWORD ? wordtext : ""));
			raise_error_unexpected_syntax(TDO);
		}
		n1->nbinary.ch2 = list(0);
		t = TDONE;
		break;
	}
	case TFOR:
		if (readtoken() != TWORD || quoteflag || ! goodname(wordtext))
			raise_error_syntax("Bad for loop variable");
		n1 = stzalloc(sizeof(struct nfor));
		n1->type = NFOR;
		n1->nfor.var = wordtext;
		checkkwd = CHKKWD | CHKALIAS;
		if (readtoken() == TIN) {
			app = &ap;
			while (readtoken() == TWORD) {
				n2 = stzalloc(sizeof(struct narg));
				n2->type = NARG;
				/*n2->narg.next = NULL; - stzalloc did it */
				n2->narg.text = wordtext;
				n2->narg.backquote = backquotelist;
				*app = n2;
				app = &n2->narg.next;
			}
			*app = NULL;
			n1->nfor.args = ap;
			if (lasttoken != TNL && lasttoken != TSEMI)
				raise_error_unexpected_syntax(-1);
		} else {
			n2 = stzalloc(sizeof(struct narg));
			n2->type = NARG;
			/*n2->narg.next = NULL; - stzalloc did it */
			n2->narg.text = (char *)dolatstr;
			/*n2->narg.backquote = NULL;*/
			n1->nfor.args = n2;
			/*
			 * Newline or semicolon here is optional (but note
			 * that the original Bourne shell only allowed NL).
			 */
			if (lasttoken != TNL && lasttoken != TSEMI)
				tokpushback = 1;
		}
		checkkwd = CHKNL | CHKKWD | CHKALIAS;
		if (readtoken() != TDO)
			raise_error_unexpected_syntax(TDO);
		n1->nfor.body = list(0);
		t = TDONE;
		break;
	case TCASE:
		n1 = stzalloc(sizeof(struct ncase));
		n1->type = NCASE;
		if (readtoken() != TWORD)
			raise_error_unexpected_syntax(TWORD);
		n1->ncase.expr = n2 = stzalloc(sizeof(struct narg));
		n2->type = NARG;
		/*n2->narg.next = NULL; - stzalloc did it */
		n2->narg.text = wordtext;
		n2->narg.backquote = backquotelist;
		do {
			checkkwd = CHKKWD | CHKALIAS;
		} while (readtoken() == TNL);
		if (lasttoken != TIN)
			raise_error_unexpected_syntax(TIN);
		cpp = &n1->ncase.cases;
 next_case:
		checkkwd = CHKNL | CHKKWD;
		t = readtoken();
		while (t != TESAC) {
			if (lasttoken == TLP)
				readtoken();
			*cpp = cp = stzalloc(sizeof(struct nclist));
			cp->type = NCLIST;
			app = &cp->nclist.pattern;
			for (;;) {
				*app = ap = stzalloc(sizeof(struct narg));
				ap->type = NARG;
				/*ap->narg.next = NULL; - stzalloc did it */
				ap->narg.text = wordtext;
				ap->narg.backquote = backquotelist;
				if (readtoken() != TPIPE)
					break;
				app = &ap->narg.next;
				readtoken();
			}
			//ap->narg.next = NULL;
			if (lasttoken != TRP)
				raise_error_unexpected_syntax(TRP);
			cp->nclist.body = list(2);

			cpp = &cp->nclist.next;

			checkkwd = CHKNL | CHKKWD;
			t = readtoken();
			if (t != TESAC) {
				if (t != TENDCASE)
					raise_error_unexpected_syntax(TENDCASE);
				goto next_case;
			}
		}
		*cpp = NULL;
		goto redir;
	case TLP:
		n1 = stzalloc(sizeof(struct nredir));
		n1->type = NSUBSHELL;
		n1->nredir.n = list(0);
		/*n1->nredir.redirect = NULL; - stzalloc did it */
		t = TRP;
		break;
	case TBEGIN:
		n1 = list(0);
		t = TEND;
		break;
	case TWORD:
	case TREDIR:
		tokpushback = 1;
		return simplecmd();
	}

	if (readtoken() != t)
		raise_error_unexpected_syntax(t);

 redir:
	/* Now check for redirection which may follow command */
	checkkwd = CHKKWD | CHKALIAS;
	rpp = rpp2;
	while (readtoken() == TREDIR) {
		*rpp = n2 = redirnode;
		rpp = &n2->nfile.next;
		parsefname();
	}
	tokpushback = 1;
	*rpp = NULL;
	if (redir) {
		if (n1->type != NSUBSHELL) {
			n2 = stzalloc(sizeof(struct nredir));
			n2->type = NREDIR;
			n2->nredir.n = n1;
			n1 = n2;
		}
		n1->nredir.redirect = redir;
	}
	return n1;
}

#if ENABLE_ASH_BASH_COMPAT
static int decode_dollar_squote(void)
{
	static const char C_escapes[] ALIGN1 = "nrbtfav""x\\01234567";
	int c, cnt;
	char *p;
	char buf[4];

	c = pgetc();
	p = strchr(C_escapes, c);
	if (p) {
		buf[0] = c;
		p = buf;
		cnt = 3;
		if ((unsigned char)(c - '0') <= 7) { /* \ooo */
			do {
				c = pgetc();
				*++p = c;
			} while ((unsigned char)(c - '0') <= 7 && --cnt);
			pungetc();
		} else if (c == 'x') { /* \xHH */
			do {
				c = pgetc();
				*++p = c;
			} while (isxdigit(c) && --cnt);
			pungetc();
			if (cnt == 3) { /* \x but next char is "bad" */
				c = 'x';
				goto unrecognized;
			}
		} else { /* simple seq like \\ or \t */
			p++;
		}
		*p = '\0';
		p = buf;
		c = bb_process_escape_sequence((void*)&p);
	} else { /* unrecognized "\z": print both chars unless ' or " */
		if (c != '\'' && c != '"') {
 unrecognized:
			c |= 0x100; /* "please encode \, then me" */
		}
	}
	return c;
}
#endif

/*
 * If eofmark is NULL, read a word or a redirection symbol.  If eofmark
 * is not NULL, read a here document.  In the latter case, eofmark is the
 * word which marks the end of the document and striptabs is true if
 * leading tabs should be stripped from the document.  The argument firstc
 * is the first character of the input token or document.
 *
 * Because C does not have internal subroutines, I have simulated them
 * using goto's to implement the subroutine linkage.  The following macros
 * will run code that appears at the end of readtoken1.
 */
#define CHECKEND()      {goto checkend; checkend_return:;}
#define PARSEREDIR()    {goto parseredir; parseredir_return:;}
#define PARSESUB()      {goto parsesub; parsesub_return:;}
#define PARSEBACKQOLD() {oldstyle = 1; goto parsebackq; parsebackq_oldreturn:;}
#define PARSEBACKQNEW() {oldstyle = 0; goto parsebackq; parsebackq_newreturn:;}
#define PARSEARITH()    {goto parsearith; parsearith_return:;}
static int
readtoken1(int firstc, int syntax, char *eofmark, int striptabs)
{
	/* NB: syntax parameter fits into smallint */
	int c = firstc;
	char *out;
	int len;
	char line[EOFMARKLEN + 1];
	struct nodelist *bqlist;
	smallint quotef;
	smallint dblquote;
	smallint oldstyle;
	smallint prevsyntax; /* syntax before arithmetic */
#if ENABLE_ASH_EXPAND_PRMT
	smallint pssyntax;   /* we are expanding a prompt string */
#endif
	int varnest;         /* levels of variables expansion */
	int arinest;         /* levels of arithmetic expansion */
	int parenlevel;      /* levels of parens in arithmetic */
	int dqvarnest;       /* levels of variables expansion within double quotes */

	USE_ASH_BASH_COMPAT(smallint bash_dollar_squote = 0;)

#if __GNUC__
	/* Avoid longjmp clobbering */
	(void) &out;
	(void) &quotef;
	(void) &dblquote;
	(void) &varnest;
	(void) &arinest;
	(void) &parenlevel;
	(void) &dqvarnest;
	(void) &oldstyle;
	(void) &prevsyntax;
	(void) &syntax;
#endif
	startlinno = plinno;
	bqlist = NULL;
	quotef = 0;
	oldstyle = 0;
	prevsyntax = 0;
#if ENABLE_ASH_EXPAND_PRMT
	pssyntax = (syntax == PSSYNTAX);
	if (pssyntax)
		syntax = DQSYNTAX;
#endif
	dblquote = (syntax == DQSYNTAX);
	varnest = 0;
	arinest = 0;
	parenlevel = 0;
	dqvarnest = 0;

	STARTSTACKSTR(out);
	loop: { /* for each line, until end of word */
		CHECKEND();     /* set c to PEOF if at end of here document */
		for (;;) {      /* until end of line or end of word */
			CHECKSTRSPACE(4, out);  /* permit 4 calls to USTPUTC */
			switch (SIT(c, syntax)) {
			case CNL:       /* '\n' */
				if (syntax == BASESYNTAX)
					goto endword;   /* exit outer loop */
				USTPUTC(c, out);
				plinno++;
				if (doprompt)
					setprompt(2);
				c = pgetc();
				goto loop;              /* continue outer loop */
			case CWORD:
				USTPUTC(c, out);
				break;
			case CCTL:
				if (eofmark == NULL || dblquote)
					USTPUTC(CTLESC, out);
#if ENABLE_ASH_BASH_COMPAT
				if (c == '\\' && bash_dollar_squote) {
					c = decode_dollar_squote();
					if (c & 0x100) {
						USTPUTC('\\', out);
						c = (unsigned char)c;
					}
				}
#endif
				USTPUTC(c, out);
				break;
			case CBACK:     /* backslash */
				c = pgetc2();
				if (c == PEOF) {
					USTPUTC(CTLESC, out);
					USTPUTC('\\', out);
					pungetc();
				} else if (c == '\n') {
					if (doprompt)
						setprompt(2);
				} else {
#if ENABLE_ASH_EXPAND_PRMT
					if (c == '$' && pssyntax) {
						USTPUTC(CTLESC, out);
						USTPUTC('\\', out);
					}
#endif
					if (dblquote &&	c != '\\'
					 && c != '`' &&	c != '$'
					 && (c != '"' || eofmark != NULL)
					) {
						USTPUTC(CTLESC, out);
						USTPUTC('\\', out);
					}
					if (SIT(c, SQSYNTAX) == CCTL)
						USTPUTC(CTLESC, out);
					USTPUTC(c, out);
					quotef = 1;
				}
				break;
			case CSQUOTE:
				syntax = SQSYNTAX;
 quotemark:
				if (eofmark == NULL) {
					USTPUTC(CTLQUOTEMARK, out);
				}
				break;
			case CDQUOTE:
				syntax = DQSYNTAX;
				dblquote = 1;
				goto quotemark;
			case CENDQUOTE:
				USE_ASH_BASH_COMPAT(bash_dollar_squote = 0;)
				if (eofmark != NULL && arinest == 0
				 && varnest == 0
				) {
					USTPUTC(c, out);
				} else {
					if (dqvarnest == 0) {
						syntax = BASESYNTAX;
						dblquote = 0;
					}
					quotef = 1;
					goto quotemark;
				}
				break;
			case CVAR:      /* '$' */
				PARSESUB();             /* parse substitution */
				break;
			case CENDVAR:   /* '}' */
				if (varnest > 0) {
					varnest--;
					if (dqvarnest > 0) {
						dqvarnest--;
					}
					USTPUTC(CTLENDVAR, out);
				} else {
					USTPUTC(c, out);
				}
				break;
#if ENABLE_ASH_MATH_SUPPORT
			case CLP:       /* '(' in arithmetic */
				parenlevel++;
				USTPUTC(c, out);
				break;
			case CRP:       /* ')' in arithmetic */
				if (parenlevel > 0) {
					USTPUTC(c, out);
					--parenlevel;
				} else {
					if (pgetc() == ')') {
						if (--arinest == 0) {
							USTPUTC(CTLENDARI, out);
							syntax = prevsyntax;
							dblquote = (syntax == DQSYNTAX);
						} else
							USTPUTC(')', out);
					} else {
						/*
						 * unbalanced parens
						 *  (don't 2nd guess - no error)
						 */
						pungetc();
						USTPUTC(')', out);
					}
				}
				break;
#endif
			case CBQUOTE:   /* '`' */
				PARSEBACKQOLD();
				break;
			case CENDFILE:
				goto endword;           /* exit outer loop */
			case CIGN:
				break;
			default:
				if (varnest == 0)
					goto endword;   /* exit outer loop */
#if ENABLE_ASH_ALIAS
				if (c != PEOA)
#endif
					USTPUTC(c, out);

			}
			c = pgetc_macro();
		} /* for(;;) */
	}
 endword:
#if ENABLE_ASH_MATH_SUPPORT
	if (syntax == ARISYNTAX)
		raise_error_syntax("Missing '))'");
#endif
	if (syntax != BASESYNTAX && !parsebackquote && eofmark == NULL)
		raise_error_syntax("Unterminated quoted string");
	if (varnest != 0) {
		startlinno = plinno;
		/* { */
		raise_error_syntax("Missing '}'");
	}
	USTPUTC('\0', out);
	len = out - (char *)stackblock();
	out = stackblock();
	if (eofmark == NULL) {
		if ((c == '>' || c == '<')
		 && quotef == 0
		 && len <= 2
		 && (*out == '\0' || isdigit(*out))
		) {
			PARSEREDIR();
			lasttoken = TREDIR;
			return lasttoken;
		}
		pungetc();
	}
	quoteflag = quotef;
	backquotelist = bqlist;
	grabstackblock(len);
	wordtext = out;
	lasttoken = TWORD;
	return lasttoken;
/* end of readtoken routine */

/*
 * Check to see whether we are at the end of the here document.  When this
 * is called, c is set to the first character of the next input line.  If
 * we are at the end of the here document, this routine sets the c to PEOF.
 */
checkend: {
	if (eofmark) {
#if ENABLE_ASH_ALIAS
		if (c == PEOA) {
			c = pgetc2();
		}
#endif
		if (striptabs) {
			while (c == '\t') {
				c = pgetc2();
			}
		}
		if (c == *eofmark) {
			if (pfgets(line, sizeof(line)) != NULL) {
				char *p, *q;

				p = line;
				for (q = eofmark + 1; *q && *p == *q; p++, q++)
					continue;
				if (*p == '\n' && *q == '\0') {
					c = PEOF;
					plinno++;
					needprompt = doprompt;
				} else {
					pushstring(line, NULL);
				}
			}
		}
	}
	goto checkend_return;
}

/*
 * Parse a redirection operator.  The variable "out" points to a string
 * specifying the fd to be redirected.  The variable "c" contains the
 * first character of the redirection operator.
 */
parseredir: {
	char fd = *out;
	union node *np;

	np = stzalloc(sizeof(struct nfile));
	if (c == '>') {
		np->nfile.fd = 1;
		c = pgetc();
		if (c == '>')
			np->type = NAPPEND;
		else if (c == '|')
			np->type = NCLOBBER;
		else if (c == '&')
			np->type = NTOFD;
		else {
			np->type = NTO;
			pungetc();
		}
	} else {        /* c == '<' */
		/*np->nfile.fd = 0; - stzalloc did it */
		c = pgetc();
		switch (c) {
		case '<':
			if (sizeof(struct nfile) != sizeof(struct nhere)) {
				np = stzalloc(sizeof(struct nhere));
				/*np->nfile.fd = 0; - stzalloc did it */
			}
			np->type = NHERE;
			heredoc = stzalloc(sizeof(struct heredoc));
			heredoc->here = np;
			c = pgetc();
			if (c == '-') {
				heredoc->striptabs = 1;
			} else {
				/*heredoc->striptabs = 0; - stzalloc did it */
				pungetc();
			}
			break;

		case '&':
			np->type = NFROMFD;
			break;

		case '>':
			np->type = NFROMTO;
			break;

		default:
			np->type = NFROM;
			pungetc();
			break;
		}
	}
	if (fd != '\0')
		np->nfile.fd = fd - '0';
	redirnode = np;
	goto parseredir_return;
}

/*
 * Parse a substitution.  At this point, we have read the dollar sign
 * and nothing else.
 */

/* is_special(c) evaluates to 1 for c in "!#$*-0123456789?@"; 0 otherwise
 * (assuming ascii char codes, as the original implementation did) */
#define is_special(c) \
	(((unsigned)(c) - 33 < 32) \
			&& ((0xc1ff920dU >> ((unsigned)(c) - 33)) & 1))
parsesub: {
	int subtype;
	int typeloc;
	int flags;
	char *p;
	static const char types[] ALIGN1 = "}-+?=";

	c = pgetc();
	if (c <= PEOA_OR_PEOF
	 || (c != '(' && c != '{' && !is_name(c) && !is_special(c))
	) {
#if ENABLE_ASH_BASH_COMPAT
		if (c == '\'')
			bash_dollar_squote = 1;
		else
#endif
			USTPUTC('$', out);
		pungetc();
	} else if (c == '(') {  /* $(command) or $((arith)) */
		if (pgetc() == '(') {
#if ENABLE_ASH_MATH_SUPPORT
			PARSEARITH();
#else
			raise_error_syntax("you disabled math support for $((arith)) syntax");
#endif
		} else {
			pungetc();
			PARSEBACKQNEW();
		}
	} else {
		USTPUTC(CTLVAR, out);
		typeloc = out - (char *)stackblock();
		USTPUTC(VSNORMAL, out);
		subtype = VSNORMAL;
		if (c == '{') {
			c = pgetc();
			if (c == '#') {
				c = pgetc();
				if (c == '}')
					c = '#';
				else
					subtype = VSLENGTH;
			} else
				subtype = 0;
		}
		if (c > PEOA_OR_PEOF && is_name(c)) {
			do {
				STPUTC(c, out);
				c = pgetc();
			} while (c > PEOA_OR_PEOF && is_in_name(c));
		} else if (isdigit(c)) {
			do {
				STPUTC(c, out);
				c = pgetc();
			} while (isdigit(c));
		} else if (is_special(c)) {
			USTPUTC(c, out);
			c = pgetc();
		} else
 badsub:		raise_error_syntax("Bad substitution");

		STPUTC('=', out);
		flags = 0;
		if (subtype == 0) {
			switch (c) {
			case ':':
				c = pgetc();
#if ENABLE_ASH_BASH_COMPAT
				if (c == ':' || c == '$' || isdigit(c)) {
					pungetc();
					subtype = VSSUBSTR;
					break;
				}
#endif
				flags = VSNUL;
				/*FALLTHROUGH*/
			default:
				p = strchr(types, c);
				if (p == NULL)
					goto badsub;
				subtype = p - types + VSNORMAL;
				break;
			case '%':
			case '#': {
				int cc = c;
				subtype = c == '#' ? VSTRIMLEFT : VSTRIMRIGHT;
				c = pgetc();
				if (c == cc)
					subtype++;
				else
					pungetc();
				break;
			}
#if ENABLE_ASH_BASH_COMPAT
			case '/':
				subtype = VSREPLACE;
				c = pgetc();
				if (c == '/')
					subtype++; /* VSREPLACEALL */
				else
					pungetc();
				break;
#endif
			}
		} else {
			pungetc();
		}
		if (dblquote || arinest)
			flags |= VSQUOTE;
		*((char *)stackblock() + typeloc) = subtype | flags;
		if (subtype != VSNORMAL) {
			varnest++;
			if (dblquote || arinest) {
				dqvarnest++;
			}
		}
	}
	goto parsesub_return;
}

/*
 * Called to parse command substitutions.  Newstyle is set if the command
 * is enclosed inside $(...); nlpp is a pointer to the head of the linked
 * list of commands (passed by reference), and savelen is the number of
 * characters on the top of the stack which must be preserved.
 */
parsebackq: {
	struct nodelist **nlpp;
	smallint savepbq;
	union node *n;
	char *volatile str;
	struct jmploc jmploc;
	struct jmploc *volatile savehandler;
	size_t savelen;
	smallint saveprompt = 0;

#ifdef __GNUC__
	(void) &saveprompt;
#endif
	savepbq = parsebackquote;
	if (setjmp(jmploc.loc)) {
		free(str);
		parsebackquote = 0;
		exception_handler = savehandler;
		longjmp(exception_handler->loc, 1);
	}
	INT_OFF;
	str = NULL;
	savelen = out - (char *)stackblock();
	if (savelen > 0) {
		str = ckmalloc(savelen);
		memcpy(str, stackblock(), savelen);
	}
	savehandler = exception_handler;
	exception_handler = &jmploc;
	INT_ON;
	if (oldstyle) {
		/* We must read until the closing backquote, giving special
		   treatment to some slashes, and then push the string and
		   reread it as input, interpreting it normally.  */
		char *pout;
		int pc;
		size_t psavelen;
		char *pstr;


		STARTSTACKSTR(pout);
		for (;;) {
			if (needprompt) {
				setprompt(2);
			}
			pc = pgetc();
			switch (pc) {
			case '`':
				goto done;

			case '\\':
				pc = pgetc();
				if (pc == '\n') {
					plinno++;
					if (doprompt)
						setprompt(2);
					/*
					 * If eating a newline, avoid putting
					 * the newline into the new character
					 * stream (via the STPUTC after the
					 * switch).
					 */
					continue;
				}
				if (pc != '\\' && pc != '`' && pc != '$'
				 && (!dblquote || pc != '"'))
					STPUTC('\\', pout);
				if (pc > PEOA_OR_PEOF) {
					break;
				}
				/* fall through */

			case PEOF:
#if ENABLE_ASH_ALIAS
			case PEOA:
#endif
				startlinno = plinno;
				raise_error_syntax("EOF in backquote substitution");

			case '\n':
				plinno++;
				needprompt = doprompt;
				break;

			default:
				break;
			}
			STPUTC(pc, pout);
		}
 done:
		STPUTC('\0', pout);
		psavelen = pout - (char *)stackblock();
		if (psavelen > 0) {
			pstr = grabstackstr(pout);
			setinputstring(pstr);
		}
	}
	nlpp = &bqlist;
	while (*nlpp)
		nlpp = &(*nlpp)->next;
	*nlpp = stzalloc(sizeof(**nlpp));
	/* (*nlpp)->next = NULL; - stzalloc did it */
	parsebackquote = oldstyle;

	if (oldstyle) {
		saveprompt = doprompt;
		doprompt = 0;
	}

	n = list(2);

	if (oldstyle)
		doprompt = saveprompt;
	else if (readtoken() != TRP)
		raise_error_unexpected_syntax(TRP);

	(*nlpp)->n = n;
	if (oldstyle) {
		/*
		 * Start reading from old file again, ignoring any pushed back
		 * tokens left from the backquote parsing
		 */
		popfile();
		tokpushback = 0;
	}
	while (stackblocksize() <= savelen)
		growstackblock();
	STARTSTACKSTR(out);
	if (str) {
		memcpy(out, str, savelen);
		STADJUST(savelen, out);
		INT_OFF;
		free(str);
		str = NULL;
		INT_ON;
	}
	parsebackquote = savepbq;
	exception_handler = savehandler;
	if (arinest || dblquote)
		USTPUTC(CTLBACKQ | CTLQUOTE, out);
	else
		USTPUTC(CTLBACKQ, out);
	if (oldstyle)
		goto parsebackq_oldreturn;
	goto parsebackq_newreturn;
}

#if ENABLE_ASH_MATH_SUPPORT
/*
 * Parse an arithmetic expansion (indicate start of one and set state)
 */
parsearith: {
	if (++arinest == 1) {
		prevsyntax = syntax;
		syntax = ARISYNTAX;
		USTPUTC(CTLARI, out);
		if (dblquote)
			USTPUTC('"', out);
		else
			USTPUTC(' ', out);
	} else {
		/*
		 * we collapse embedded arithmetic expansion to
		 * parenthesis, which should be equivalent
		 */
		USTPUTC('(', out);
	}
	goto parsearith_return;
}
#endif

} /* end of readtoken */

/*
 * Read the next input token.
 * If the token is a word, we set backquotelist to the list of cmds in
 *      backquotes.  We set quoteflag to true if any part of the word was
 *      quoted.
 * If the token is TREDIR, then we set redirnode to a structure containing
 *      the redirection.
 * In all cases, the variable startlinno is set to the number of the line
 *      on which the token starts.
 *
 * [Change comment:  here documents and internal procedures]
 * [Readtoken shouldn't have any arguments.  Perhaps we should make the
 *  word parsing code into a separate routine.  In this case, readtoken
 *  doesn't need to have any internal procedures, but parseword does.
 *  We could also make parseoperator in essence the main routine, and
 *  have parseword (readtoken1?) handle both words and redirection.]
 */
#define NEW_xxreadtoken
#ifdef NEW_xxreadtoken
/* singles must be first! */
static const char xxreadtoken_chars[7] ALIGN1 = {
	'\n', '(', ')', '&', '|', ';', 0
};

static const char xxreadtoken_tokens[] ALIGN1 = {
	TNL, TLP, TRP,          /* only single occurrence allowed */
	TBACKGND, TPIPE, TSEMI, /* if single occurrence */
	TEOF,                   /* corresponds to trailing nul */
	TAND, TOR, TENDCASE     /* if double occurrence */
};

#define xxreadtoken_doubles \
	(sizeof(xxreadtoken_tokens) - sizeof(xxreadtoken_chars))
#define xxreadtoken_singles \
	(sizeof(xxreadtoken_chars) - xxreadtoken_doubles - 1)

static int
xxreadtoken(void)
{
	int c;

	if (tokpushback) {
		tokpushback = 0;
		return lasttoken;
	}
	if (needprompt) {
		setprompt(2);
	}
	startlinno = plinno;
	for (;;) {                      /* until token or start of word found */
		c = pgetc_macro();

		if ((c != ' ') && (c != '\t')
#if ENABLE_ASH_ALIAS
		 && (c != PEOA)
#endif
		) {
			if (c == '#') {
				while ((c = pgetc()) != '\n' && c != PEOF)
					continue;
				pungetc();
			} else if (c == '\\') {
				if (pgetc() != '\n') {
					pungetc();
					goto READTOKEN1;
				}
				startlinno = ++plinno;
				if (doprompt)
					setprompt(2);
			} else {
				const char *p
					= xxreadtoken_chars + sizeof(xxreadtoken_chars) - 1;

				if (c != PEOF) {
					if (c == '\n') {
						plinno++;
						needprompt = doprompt;
					}

					p = strchr(xxreadtoken_chars, c);
					if (p == NULL) {
 READTOKEN1:
						return readtoken1(c, BASESYNTAX, (char *) NULL, 0);
					}

					if ((size_t)(p - xxreadtoken_chars) >= xxreadtoken_singles) {
						if (pgetc() == *p) {    /* double occurrence? */
							p += xxreadtoken_doubles + 1;
						} else {
							pungetc();
						}
					}
				}
				lasttoken = xxreadtoken_tokens[p - xxreadtoken_chars];
				return lasttoken;
			}
		}
	} /* for */
}
#else
#define RETURN(token)   return lasttoken = token
static int
xxreadtoken(void)
{
	int c;

	if (tokpushback) {
		tokpushback = 0;
		return lasttoken;
	}
	if (needprompt) {
		setprompt(2);
	}
	startlinno = plinno;
	for (;;) {      /* until token or start of word found */
		c = pgetc_macro();
		switch (c) {
		case ' ': case '\t':
#if ENABLE_ASH_ALIAS
		case PEOA:
#endif
			continue;
		case '#':
			while ((c = pgetc()) != '\n' && c != PEOF)
				continue;
			pungetc();
			continue;
		case '\\':
			if (pgetc() == '\n') {
				startlinno = ++plinno;
				if (doprompt)
					setprompt(2);
				continue;
			}
			pungetc();
			goto breakloop;
		case '\n':
			plinno++;
			needprompt = doprompt;
			RETURN(TNL);
		case PEOF:
			RETURN(TEOF);
		case '&':
			if (pgetc() == '&')
				RETURN(TAND);
			pungetc();
			RETURN(TBACKGND);
		case '|':
			if (pgetc() == '|')
				RETURN(TOR);
			pungetc();
			RETURN(TPIPE);
		case ';':
			if (pgetc() == ';')
				RETURN(TENDCASE);
			pungetc();
			RETURN(TSEMI);
		case '(':
			RETURN(TLP);
		case ')':
			RETURN(TRP);
		default:
			goto breakloop;
		}
	}
 breakloop:
	return readtoken1(c, BASESYNTAX, (char *)NULL, 0);
#undef RETURN
}
#endif /* NEW_xxreadtoken */

static int
readtoken(void)
{
	int t;
#if DEBUG
	smallint alreadyseen = tokpushback;
#endif

#if ENABLE_ASH_ALIAS
 top:
#endif

	t = xxreadtoken();

	/*
	 * eat newlines
	 */
	if (checkkwd & CHKNL) {
		while (t == TNL) {
			parseheredoc();
			t = xxreadtoken();
		}
	}

	if (t != TWORD || quoteflag) {
		goto out;
	}

	/*
	 * check for keywords
	 */
	if (checkkwd & CHKKWD) {
		const char *const *pp;

		pp = findkwd(wordtext);
		if (pp) {
			lasttoken = t = pp - tokname_array;
			TRACE(("keyword %s recognized\n", tokname(t)));
			goto out;
		}
	}

	if (checkkwd & CHKALIAS) {
#if ENABLE_ASH_ALIAS
		struct alias *ap;
		ap = lookupalias(wordtext, 1);
		if (ap != NULL) {
			if (*ap->val) {
				pushstring(ap->val, ap);
			}
			goto top;
		}
#endif
	}
 out:
	checkkwd = 0;
#if DEBUG
	if (!alreadyseen)
		TRACE(("token %s %s\n", tokname(t), t == TWORD ? wordtext : ""));
	else
		TRACE(("reread token %s %s\n", tokname(t), t == TWORD ? wordtext : ""));
#endif
	return t;
}

static char
peektoken(void)
{
	int t;

	t = readtoken();
	tokpushback = 1;
	return tokname_array[t][0];
}

/*
 * Read and parse a command.  Returns NEOF on end of file.  (NULL is a
 * valid parse tree indicating a blank line.)
 */
static union node *
parsecmd(int interact)
{
	int t;

	tokpushback = 0;
	doprompt = interact;
	if (doprompt)
		setprompt(doprompt);
	needprompt = 0;
	t = readtoken();
	if (t == TEOF)
		return NEOF;
	if (t == TNL)
		return NULL;
	tokpushback = 1;
	return list(1);
}

/*
 * Input any here documents.
 */
static void
parseheredoc(void)
{
	struct heredoc *here;
	union node *n;

	here = heredoclist;
	heredoclist = NULL;

	while (here) {
		if (needprompt) {
			setprompt(2);
		}
		readtoken1(pgetc(), here->here->type == NHERE? SQSYNTAX : DQSYNTAX,
				here->eofmark, here->striptabs);
		n = stzalloc(sizeof(struct narg));
		n->narg.type = NARG;
		/*n->narg.next = NULL; - stzalloc did it */
		n->narg.text = wordtext;
		n->narg.backquote = backquotelist;
		here->here->nhere.doc = n;
		here = here->next;
	}
}


/*
 * called by editline -- any expansions to the prompt should be added here.
 */
#if ENABLE_ASH_EXPAND_PRMT
static const char *
expandstr(const char *ps)
{
	union node n;

	/* XXX Fix (char *) cast. */
	setinputstring((char *)ps);
	readtoken1(pgetc(), PSSYNTAX, nullstr, 0);
	popfile();

	n.narg.type = NARG;
	n.narg.next = NULL;
	n.narg.text = wordtext;
	n.narg.backquote = backquotelist;

	expandarg(&n, NULL, 0);
	return stackblock();
}
#endif

/*
 * Execute a command or commands contained in a string.
 */
static int
evalstring(char *s, int mask)
{
	union node *n;
	struct stackmark smark;
	int skip;

	setinputstring(s);
	setstackmark(&smark);

	skip = 0;
	while ((n = parsecmd(0)) != NEOF) {
		evaltree(n, 0);
		popstackmark(&smark);
		skip = evalskip;
		if (skip)
			break;
	}
	popfile();

	skip &= mask;
	evalskip = skip;
	return skip;
}

/*
 * The eval command.
 */
static int
evalcmd(int argc ATTRIBUTE_UNUSED, char **argv)
{
	char *p;
	char *concat;

	if (argv[1]) {
		p = argv[1];
		argv += 2;
		if (argv[0]) {
			STARTSTACKSTR(concat);
			for (;;) {
				concat = stack_putstr(p, concat);
				p = *argv++;
				if (p == NULL)
					break;
				STPUTC(' ', concat);
			}
			STPUTC('\0', concat);
			p = grabstackstr(concat);
		}
		evalstring(p, ~SKIPEVAL);

	}
	return exitstatus;
}

/*
 * Read and execute commands.  "Top" is nonzero for the top level command
 * loop; it turns on prompting if the shell is interactive.
 */
static int
cmdloop(int top)
{
	union node *n;
	struct stackmark smark;
	int inter;
	int numeof = 0;

	TRACE(("cmdloop(%d) called\n", top));
	for (;;) {
		int skip;

		setstackmark(&smark);
#if JOBS
		if (doing_jobctl)
			showjobs(stderr, SHOW_CHANGED);
#endif
		inter = 0;
		if (iflag && top) {
			inter++;
#if ENABLE_ASH_MAIL
			chkmail();
#endif
		}
		n = parsecmd(inter);
		/* showtree(n); DEBUG */
		if (n == NEOF) {
			if (!top || numeof >= 50)
				break;
			if (!stoppedjobs()) {
				if (!Iflag)
					break;
				out2str("\nUse \"exit\" to leave shell.\n");
			}
			numeof++;
		} else if (nflag == 0) {
			/* job_warning can only be 2,1,0. Here 2->1, 1/0->0 */
			job_warning >>= 1;
			numeof = 0;
			evaltree(n, 0);
		}
		popstackmark(&smark);
		skip = evalskip;

		if (skip) {
			evalskip = 0;
			return skip & SKIPEVAL;
		}
	}
	return 0;
}

/*
 * Take commands from a file.  To be compatible we should do a path
 * search for the file, which is necessary to find sub-commands.
 */
static char *
find_dot_file(char *name)
{
	char *fullname;
	const char *path = pathval();
	struct stat statb;

	/* don't try this for absolute or relative paths */
	if (strchr(name, '/'))
		return name;

	while ((fullname = padvance(&path, name)) != NULL) {
		if ((stat(fullname, &statb) == 0) && S_ISREG(statb.st_mode)) {
			/*
			 * Don't bother freeing here, since it will
			 * be freed by the caller.
			 */
			return fullname;
		}
		stunalloc(fullname);
	}

	/* not found in the PATH */
	ash_msg_and_raise_error("%s: not found", name);
	/* NOTREACHED */
}

static int
dotcmd(int argc, char **argv)
{
	struct strlist *sp;
	volatile struct shparam saveparam;
	int status = 0;

	for (sp = cmdenviron; sp; sp = sp->next)
		setvareq(ckstrdup(sp->text), VSTRFIXED | VTEXTFIXED);

	if (argv[1]) {        /* That's what SVR2 does */
		char *fullname = find_dot_file(argv[1]);
		argv += 2;
		argc -= 2;
		if (argc) { /* argc > 0, argv[0] != NULL */
			saveparam = shellparam;
			shellparam.malloced = 0;
			shellparam.nparam = argc;
			shellparam.p = argv;
		};

		setinputfile(fullname, INPUT_PUSH_FILE);
		commandname = fullname;
		cmdloop(0);
		popfile();

		if (argc) {
			freeparam(&shellparam);
			shellparam = saveparam;
		};
		status = exitstatus;
	}
	return status;
}

static int
exitcmd(int argc ATTRIBUTE_UNUSED, char **argv)
{
	if (stoppedjobs())
		return 0;
	if (argv[1])
		exitstatus = number(argv[1]);
	raise_exception(EXEXIT);
	/* NOTREACHED */
}

/*
 * Read a file containing shell functions.
 */
static void
readcmdfile(char *name)
{
	setinputfile(name, INPUT_PUSH_FILE);
	cmdloop(0);
	popfile();
}


/* ============ find_command inplementation */

/*
 * Resolve a command name.  If you change this routine, you may have to
 * change the shellexec routine as well.
 */
static void
find_command(char *name, struct cmdentry *entry, int act, const char *path)
{
	struct tblentry *cmdp;
	int idx;
	int prev;
	char *fullname;
	struct stat statb;
	int e;
	int updatetbl;
	struct builtincmd *bcmd;

	/* If name contains a slash, don't use PATH or hash table */
	if (strchr(name, '/') != NULL) {
		entry->u.index = -1;
		if (act & DO_ABS) {
			while (stat(name, &statb) < 0) {
#ifdef SYSV
				if (errno == EINTR)
					continue;
#endif
				entry->cmdtype = CMDUNKNOWN;
				return;
			}
		}
		entry->cmdtype = CMDNORMAL;
		return;
	}

/* #if ENABLE_FEATURE_SH_STANDALONE... moved after builtin check */

	updatetbl = (path == pathval());
	if (!updatetbl) {
		act |= DO_ALTPATH;
		if (strstr(path, "%builtin") != NULL)
			act |= DO_ALTBLTIN;
	}

	/* If name is in the table, check answer will be ok */
	cmdp = cmdlookup(name, 0);
	if (cmdp != NULL) {
		int bit;

		switch (cmdp->cmdtype) {
		default:
#if DEBUG
			abort();
#endif
		case CMDNORMAL:
			bit = DO_ALTPATH;
			break;
		case CMDFUNCTION:
			bit = DO_NOFUNC;
			break;
		case CMDBUILTIN:
			bit = DO_ALTBLTIN;
			break;
		}
		if (act & bit) {
			updatetbl = 0;
			cmdp = NULL;
		} else if (cmdp->rehash == 0)
			/* if not invalidated by cd, we're done */
			goto success;
	}

	/* If %builtin not in path, check for builtin next */
	bcmd = find_builtin(name);
	if (bcmd) {
		if (IS_BUILTIN_REGULAR(bcmd))
			goto builtin_success;
		if (act & DO_ALTPATH) {
			if (!(act & DO_ALTBLTIN))
				goto builtin_success;
		} else if (builtinloc <= 0) {
			goto builtin_success;
		}
	}

#if ENABLE_FEATURE_SH_STANDALONE
	{
		int applet_no = find_applet_by_name(name);
		if (applet_no >= 0) {
			entry->cmdtype = CMDNORMAL;
			entry->u.index = -2 - applet_no;
			return;
		}
	}
#endif

	/* We have to search path. */
	prev = -1;              /* where to start */
	if (cmdp && cmdp->rehash) {     /* doing a rehash */
		if (cmdp->cmdtype == CMDBUILTIN)
			prev = builtinloc;
		else
			prev = cmdp->param.index;
	}

	e = ENOENT;
	idx = -1;
 loop:
	while ((fullname = padvance(&path, name)) != NULL) {
		stunalloc(fullname);
		/* NB: code below will still use fullname
		 * despite it being "unallocated" */
		idx++;
		if (pathopt) {
			if (prefix(pathopt, "builtin")) {
				if (bcmd)
					goto builtin_success;
				continue;
			}
			if ((act & DO_NOFUNC)
			 || !prefix(pathopt, "func")
			) {	/* ignore unimplemented options */
				continue;
			}
		}
		/* if rehash, don't redo absolute path names */
		if (fullname[0] == '/' && idx <= prev) {
			if (idx < prev)
				continue;
			TRACE(("searchexec \"%s\": no change\n", name));
			goto success;
		}
		while (stat(fullname, &statb) < 0) {
#ifdef SYSV
			if (errno == EINTR)
				continue;
#endif
			if (errno != ENOENT && errno != ENOTDIR)
				e = errno;
			goto loop;
		}
		e = EACCES;     /* if we fail, this will be the error */
		if (!S_ISREG(statb.st_mode))
			continue;
		if (pathopt) {          /* this is a %func directory */
			stalloc(strlen(fullname) + 1);
			/* NB: stalloc will return space pointed by fullname
			 * (because we don't have any intervening allocations
			 * between stunalloc above and this stalloc) */
			readcmdfile(fullname);
			cmdp = cmdlookup(name, 0);
			if (cmdp == NULL || cmdp->cmdtype != CMDFUNCTION)
				ash_msg_and_raise_error("%s not defined in %s", name, fullname);
			stunalloc(fullname);
			goto success;
		}
		TRACE(("searchexec \"%s\" returns \"%s\"\n", name, fullname));
		if (!updatetbl) {
			entry->cmdtype = CMDNORMAL;
			entry->u.index = idx;
			return;
		}
		INT_OFF;
		cmdp = cmdlookup(name, 1);
		cmdp->cmdtype = CMDNORMAL;
		cmdp->param.index = idx;
		INT_ON;
		goto success;
	}

	/* We failed.  If there was an entry for this command, delete it */
	if (cmdp && updatetbl)
		delete_cmd_entry();
	if (act & DO_ERR)
		ash_msg("%s: %s", name, errmsg(e, "not found"));
	entry->cmdtype = CMDUNKNOWN;
	return;

 builtin_success:
	if (!updatetbl) {
		entry->cmdtype = CMDBUILTIN;
		entry->u.cmd = bcmd;
		return;
	}
	INT_OFF;
	cmdp = cmdlookup(name, 1);
	cmdp->cmdtype = CMDBUILTIN;
	cmdp->param.cmd = bcmd;
	INT_ON;
 success:
	cmdp->rehash = 0;
	entry->cmdtype = cmdp->cmdtype;
	entry->u = cmdp->param;
}


/* ============ trap.c */

/*
 * The trap builtin.
 */
static int
trapcmd(int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED)
{
	char *action;
	char **ap;
	int signo;

	nextopt(nullstr);
	ap = argptr;
	if (!*ap) {
		for (signo = 0; signo < NSIG; signo++) {
			if (trap[signo] != NULL) {
				const char *sn;

				sn = get_signame(signo);
				out1fmt("trap -- %s %s\n",
					single_quote(trap[signo]), sn);
			}
		}
		return 0;
	}
	if (!ap[1])
		action = NULL;
	else
		action = *ap++;
	while (*ap) {
		signo = get_signum(*ap);
		if (signo < 0)
			ash_msg_and_raise_error("%s: bad trap", *ap);
		INT_OFF;
		if (action) {
			if (LONE_DASH(action))
				action = NULL;
			else
				action = ckstrdup(action);
		}
		free(trap[signo]);
		trap[signo] = action;
		if (signo != 0)
			setsignal(signo);
		INT_ON;
		ap++;
	}
	return 0;
}


/* ============ Builtins */

#if !ENABLE_FEATURE_SH_EXTRA_QUIET
/*
 * Lists available builtins
 */
static int
helpcmd(int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED)
{
	unsigned col;
	unsigned i;

	out1fmt("\nBuilt-in commands:\n-------------------\n");
	for (col = 0, i = 0; i < ARRAY_SIZE(builtintab); i++) {
		col += out1fmt("%c%s", ((col == 0) ? '\t' : ' '),
					builtintab[i].name + 1);
		if (col > 60) {
			out1fmt("\n");
			col = 0;
		}
	}
#if ENABLE_FEATURE_SH_STANDALONE
	{
		const char *a = applet_names;
		while (*a) {
			col += out1fmt("%c%s", ((col == 0) ? '\t' : ' '), a);
			if (col > 60) {
				out1fmt("\n");
				col = 0;
			}
			a += strlen(a) + 1;
		}
	}
#endif
	out1fmt("\n\n");
	return EXIT_SUCCESS;
}
#endif /* FEATURE_SH_EXTRA_QUIET */

/*
 * The export and readonly commands.
 */
static int
exportcmd(int argc ATTRIBUTE_UNUSED, char **argv)
{
	struct var *vp;
	char *name;
	const char *p;
	char **aptr;
	int flag = argv[0][0] == 'r'? VREADONLY : VEXPORT;

	if (nextopt("p") != 'p') {
		aptr = argptr;
		name = *aptr;
		if (name) {
			do {
				p = strchr(name, '=');
				if (p != NULL) {
					p++;
				} else {
					vp = *findvar(hashvar(name), name);
					if (vp) {
						vp->flags |= flag;
						continue;
					}
				}
				setvar(name, p, flag);
			} while ((name = *++aptr) != NULL);
			return 0;
		}
	}
	showvars(argv[0], flag, 0);
	return 0;
}

/*
 * Delete a function if it exists.
 */
static void
unsetfunc(const char *name)
{
	struct tblentry *cmdp;

	cmdp = cmdlookup(name, 0);
	if (cmdp!= NULL && cmdp->cmdtype == CMDFUNCTION)
		delete_cmd_entry();
}

/*
 * The unset builtin command.  We unset the function before we unset the
 * variable to allow a function to be unset when there is a readonly variable
 * with the same name.
 */
static int
unsetcmd(int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED)
{
	char **ap;
	int i;
	int flag = 0;
	int ret = 0;

	while ((i = nextopt("vf")) != '\0') {
		flag = i;
	}

	for (ap = argptr; *ap; ap++) {
		if (flag != 'f') {
			i = unsetvar(*ap);
			ret |= i;
			if (!(i & 2))
				continue;
		}
		if (flag != 'v')
			unsetfunc(*ap);
	}
	return ret & 1;
}


/*      setmode.c      */

#include <sys/times.h>

static const unsigned char timescmd_str[] ALIGN1 = {
	' ',  offsetof(struct tms, tms_utime),
	'\n', offsetof(struct tms, tms_stime),
	' ',  offsetof(struct tms, tms_cutime),
	'\n', offsetof(struct tms, tms_cstime),
	0
};

static int
timescmd(int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED)
{
	long clk_tck, s, t;
	const unsigned char *p;
	struct tms buf;

	clk_tck = sysconf(_SC_CLK_TCK);
	times(&buf);

	p = timescmd_str;
	do {
		t = *(clock_t *)(((char *) &buf) + p[1]);
		s = t / clk_tck;
		out1fmt("%ldm%ld.%.3lds%c",
			s/60, s%60,
			((t - s * clk_tck) * 1000) / clk_tck,
			p[0]);
	} while (*(p += 2));

	return 0;
}

#if ENABLE_ASH_MATH_SUPPORT
static arith_t
dash_arith(const char *s)
{
	arith_t result;
	int errcode = 0;

	INT_OFF;
	result = arith(s, &errcode);
	if (errcode < 0) {
		if (errcode == -3)
			ash_msg_and_raise_error("exponent less than 0");
		if (errcode == -2)
			ash_msg_and_raise_error("divide by zero");
		if (errcode == -5)
			ash_msg_and_raise_error("expression recursion loop detected");
		raise_error_syntax(s);
	}
	INT_ON;

	return result;
}

/*
 *  The let builtin. partial stolen from GNU Bash, the Bourne Again SHell.
 *  Copyright (C) 1987, 1989, 1991 Free Software Foundation, Inc.
 *
 *  Copyright (C) 2003 Vladimir Oleynik <dzo@simtreas.ru>
 */
static int
letcmd(int argc ATTRIBUTE_UNUSED, char **argv)
{
	arith_t i;

	argv++;
	if (!*argv)
		ash_msg_and_raise_error("expression expected");
	do {
		i = dash_arith(*argv);
	} while (*++argv);

	return !i;
}
#endif /* ASH_MATH_SUPPORT */


/* ============ miscbltin.c
 *
 * Miscellaneous builtins.
 */

#undef rflag

#if defined(__GLIBC__) && __GLIBC__ == 2 && __GLIBC_MINOR__ < 1
typedef enum __rlimit_resource rlim_t;
#endif

/*
 * The read builtin. Options:
 *      -r              Do not interpret '\' specially
 *      -s              Turn off echo (tty only)
 *      -n NCHARS       Read NCHARS max
 *      -p PROMPT       Display PROMPT on stderr (if input is from tty)
 *      -t SECONDS      Timeout after SECONDS (tty or pipe only)
 *      -u FD           Read from given FD instead of fd 0
 * This uses unbuffered input, which may be avoidable in some cases.
 * TODO: bash also has:
 *      -a ARRAY        Read into array[0],[1],etc
 *      -d DELIM        End on DELIM char, not newline
 *      -e              Use line editing (tty only)
 */
static int
readcmd(int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED)
{
	static const char *const arg_REPLY[] = { "REPLY", NULL };

	char **ap;
	int backslash;
	char c;
	int rflag;
	char *prompt;
	const char *ifs;
	char *p;
	int startword;
	int status;
	int i;
	int fd = 0;
#if ENABLE_ASH_READ_NCHARS
	int nchars = 0; /* if != 0, -n is in effect */
	int silent = 0;
	struct termios tty, old_tty;
#endif
#if ENABLE_ASH_READ_TIMEOUT
	unsigned end_ms = 0;
	unsigned timeout = 0;
#endif

	rflag = 0;
	prompt = NULL;
	while ((i = nextopt("p:u:r"
		USE_ASH_READ_TIMEOUT("t:")
		USE_ASH_READ_NCHARS("n:s")
	)) != '\0') {
		switch (i) {
		case 'p':
			prompt = optionarg;
			break;
#if ENABLE_ASH_READ_NCHARS
		case 'n':
			nchars = bb_strtou(optionarg, NULL, 10);
			if (nchars < 0 || errno)
				ash_msg_and_raise_error("invalid count");
			/* nchars == 0: off (bash 3.2 does this too) */
			break;
		case 's':
			silent = 1;
			break;
#endif
#if ENABLE_ASH_READ_TIMEOUT
		case 't':
			timeout = bb_strtou(optionarg, NULL, 10);
			if (errno || timeout > UINT_MAX / 2048)
				ash_msg_and_raise_error("invalid timeout");
			timeout *= 1000;
#if 0 /* even bash have no -t N.NNN support */
			ts.tv_sec = bb_strtou(optionarg, &p, 10);
			ts.tv_usec = 0;
			/* EINVAL means number is ok, but not terminated by NUL */
			if (*p == '.' && errno == EINVAL) {
				char *p2;
				if (*++p) {
					int scale;
					ts.tv_usec = bb_strtou(p, &p2, 10);
					if (errno)
						ash_msg_and_raise_error("invalid timeout");
					scale = p2 - p;
					/* normalize to usec */
					if (scale > 6)
						ash_msg_and_raise_error("invalid timeout");
					while (scale++ < 6)
						ts.tv_usec *= 10;
				}
			} else if (ts.tv_sec < 0 || errno) {
				ash_msg_and_raise_error("invalid timeout");
			}
			if (!(ts.tv_sec | ts.tv_usec)) { /* both are 0? */
				ash_msg_and_raise_error("invalid timeout");
			}
#endif /* if 0 */
			break;
#endif
		case 'r':
			rflag = 1;
			break;
		case 'u':
			fd = bb_strtou(optionarg, NULL, 10);
			if (fd < 0 || errno)
				ash_msg_and_raise_error("invalid file descriptor");
			break;
		default:
			break;
		}
	}
	if (prompt && isatty(fd)) {
		out2str(prompt);
	}
	ap = argptr;
	if (*ap == NULL)
		ap = (char**)arg_REPLY;
	ifs = bltinlookup("IFS");
	if (ifs == NULL)
		ifs = defifs;
#if ENABLE_ASH_READ_NCHARS
	tcgetattr(fd, &tty);
	old_tty = tty;
	if (nchars || silent) {
		if (nchars) {
			tty.c_lflag &= ~ICANON;
			tty.c_cc[VMIN] = nchars < 256 ? nchars : 255;
		}
		if (silent) {
			tty.c_lflag &= ~(ECHO | ECHOK | ECHONL);
		}
		/* if tcgetattr failed, tcsetattr will fail too.
		 * Ignoring, it's harmless. */
		tcsetattr(fd, TCSANOW, &tty);
	}
#endif

	status = 0;
	startword = 1;
	backslash = 0;
#if ENABLE_ASH_READ_TIMEOUT
	if (timeout) /* NB: ensuring end_ms is nonzero */
		end_ms = ((unsigned)(monotonic_us() / 1000) + timeout) | 1;
#endif
	STARTSTACKSTR(p);
	do {
#if ENABLE_ASH_READ_TIMEOUT
		if (end_ms) {
			struct pollfd pfd[1];
			pfd[0].fd = fd;
			pfd[0].events = POLLIN;
			timeout = end_ms - (unsigned)(monotonic_us() / 1000);
			if ((int)timeout <= 0 /* already late? */
			 || safe_poll(pfd, 1, timeout) != 1 /* no? wait... */
			) { /* timed out! */
#if ENABLE_ASH_READ_NCHARS
				tcsetattr(fd, TCSANOW, &old_tty);
#endif
				return 1;
			}
		}
#endif
		if (nonblock_safe_read(fd, &c, 1) != 1) {
			status = 1;
			break;
		}
		if (c == '\0')
			continue;
		if (backslash) {
			backslash = 0;
			if (c != '\n')
				goto put;
			continue;
		}
		if (!rflag && c == '\\') {
			backslash++;
			continue;
		}
		if (c == '\n')
			break;
		if (startword && *ifs == ' ' && strchr(ifs, c)) {
			continue;
		}
		startword = 0;
		if (ap[1] != NULL && strchr(ifs, c) != NULL) {
			STACKSTRNUL(p);
			setvar(*ap, stackblock(), 0);
			ap++;
			startword = 1;
			STARTSTACKSTR(p);
		} else {
 put:
			STPUTC(c, p);
		}
	}
/* end of do {} while: */
#if ENABLE_ASH_READ_NCHARS
	while (--nchars);
#else
	while (1);
#endif

#if ENABLE_ASH_READ_NCHARS
	tcsetattr(fd, TCSANOW, &old_tty);
#endif

	STACKSTRNUL(p);
	/* Remove trailing blanks */
	while ((char *)stackblock() <= --p && strchr(ifs, *p) != NULL)
		*p = '\0';
	setvar(*ap, stackblock(), 0);
	while (*++ap != NULL)
		setvar(*ap, nullstr, 0);
	return status;
}

static int
umaskcmd(int argc ATTRIBUTE_UNUSED, char **argv)
{
	static const char permuser[3] ALIGN1 = "ugo";
	static const char permmode[3] ALIGN1 = "rwx";
	static const short permmask[] ALIGN2 = {
		S_IRUSR, S_IWUSR, S_IXUSR,
		S_IRGRP, S_IWGRP, S_IXGRP,
		S_IROTH, S_IWOTH, S_IXOTH
	};

	char *ap;
	mode_t mask;
	int i;
	int symbolic_mode = 0;

	while (nextopt("S") != '\0') {
		symbolic_mode = 1;
	}

	INT_OFF;
	mask = umask(0);
	umask(mask);
	INT_ON;

	ap = *argptr;
	if (ap == NULL) {
		if (symbolic_mode) {
			char buf[18];
			char *p = buf;

			for (i = 0; i < 3; i++) {
				int j;

				*p++ = permuser[i];
				*p++ = '=';
				for (j = 0; j < 3; j++) {
					if ((mask & permmask[3 * i + j]) == 0) {
						*p++ = permmode[j];
					}
				}
				*p++ = ',';
			}
			*--p = 0;
			puts(buf);
		} else {
			out1fmt("%.4o\n", mask);
		}
	} else {
		if (isdigit((unsigned char) *ap)) {
			mask = 0;
			do {
				if (*ap >= '8' || *ap < '0')
					ash_msg_and_raise_error(illnum, argv[1]);
				mask = (mask << 3) + (*ap - '0');
			} while (*++ap != '\0');
			umask(mask);
		} else {
			mask = ~mask & 0777;
			if (!bb_parse_mode(ap, &mask)) {
				ash_msg_and_raise_error("illegal mode: %s", ap);
			}
			umask(~mask & 0777);
		}
	}
	return 0;
}

/*
 * ulimit builtin
 *
 * This code, originally by Doug Gwyn, Doug Kingston, Eric Gisin, and
 * Michael Rendell was ripped from pdksh 5.0.8 and hacked for use with
 * ash by J.T. Conklin.
 *
 * Public domain.
 */

struct limits {
	uint8_t cmd;          /* RLIMIT_xxx fit into it */
	uint8_t factor_shift; /* shift by to get rlim_{cur,max} values */
	char    option;
};

static const struct limits limits_tbl[] = {
#ifdef RLIMIT_CPU
	{ RLIMIT_CPU,        0, 't' },
#endif
#ifdef RLIMIT_FSIZE
	{ RLIMIT_FSIZE,      9, 'f' },
#endif
#ifdef RLIMIT_DATA
	{ RLIMIT_DATA,      10, 'd' },
#endif
#ifdef RLIMIT_STACK
	{ RLIMIT_STACK,     10, 's' },
#endif
#ifdef RLIMIT_CORE
	{ RLIMIT_CORE,       9, 'c' },
#endif
#ifdef RLIMIT_RSS
	{ RLIMIT_RSS,       10, 'm' },
#endif
#ifdef RLIMIT_MEMLOCK
	{ RLIMIT_MEMLOCK,   10, 'l' },
#endif
#ifdef RLIMIT_NPROC
	{ RLIMIT_NPROC,      0, 'p' },
#endif
#ifdef RLIMIT_NOFILE
	{ RLIMIT_NOFILE,     0, 'n' },
#endif
#ifdef RLIMIT_AS
	{ RLIMIT_AS,        10, 'v' },
#endif
#ifdef RLIMIT_LOCKS
	{ RLIMIT_LOCKS,      0, 'w' },
#endif
};
static const char limits_name[] =
#ifdef RLIMIT_CPU
	"time(seconds)" "\0"
#endif
#ifdef RLIMIT_FSIZE
	"file(blocks)" "\0"
#endif
#ifdef RLIMIT_DATA
	"data(kb)" "\0"
#endif
#ifdef RLIMIT_STACK
	"stack(kb)" "\0"
#endif
#ifdef RLIMIT_CORE
	"coredump(blocks)" "\0"
#endif
#ifdef RLIMIT_RSS
	"memory(kb)" "\0"
#endif
#ifdef RLIMIT_MEMLOCK
	"locked memory(kb)" "\0"
#endif
#ifdef RLIMIT_NPROC
	"process" "\0"
#endif
#ifdef RLIMIT_NOFILE
	"nofiles" "\0"
#endif
#ifdef RLIMIT_AS
	"vmemory(kb)" "\0"
#endif
#ifdef RLIMIT_LOCKS
	"locks" "\0"
#endif
;

enum limtype { SOFT = 0x1, HARD = 0x2 };

static void
printlim(enum limtype how, const struct rlimit *limit,
			const struct limits *l)
{
	rlim_t val;

	val = limit->rlim_max;
	if (how & SOFT)
		val = limit->rlim_cur;

	if (val == RLIM_INFINITY)
		out1fmt("unlimited\n");
	else {
		val >>= l->factor_shift;
		out1fmt("%lld\n", (long long) val);
	}
}

static int
ulimitcmd(int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED)
{
	int c;
	rlim_t val = 0;
	enum limtype how = SOFT | HARD;
	const struct limits *l;
	int set, all = 0;
	int optc, what;
	struct rlimit limit;

	what = 'f';
	while ((optc = nextopt("HSa"
#ifdef RLIMIT_CPU
				"t"
#endif
#ifdef RLIMIT_FSIZE
				"f"
#endif
#ifdef RLIMIT_DATA
				"d"
#endif
#ifdef RLIMIT_STACK
				"s"
#endif
#ifdef RLIMIT_CORE
				"c"
#endif
#ifdef RLIMIT_RSS
				"m"
#endif
#ifdef RLIMIT_MEMLOCK
				"l"
#endif
#ifdef RLIMIT_NPROC
				"p"
#endif
#ifdef RLIMIT_NOFILE
				"n"
#endif
#ifdef RLIMIT_AS
				"v"
#endif
#ifdef RLIMIT_LOCKS
				"w"
#endif
					)) != '\0')
		switch (optc) {
		case 'H':
			how = HARD;
			break;
		case 'S':
			how = SOFT;
			break;
		case 'a':
			all = 1;
			break;
		default:
			what = optc;
		}

	for (l = limits_tbl; l->option != what; l++)
		continue;

	set = *argptr ? 1 : 0;
	if (set) {
		char *p = *argptr;

		if (all || argptr[1])
			ash_msg_and_raise_error("too many arguments");
		if (strncmp(p, "unlimited\n", 9) == 0)
			val = RLIM_INFINITY;
		else {
			val = (rlim_t) 0;

			while ((c = *p++) >= '0' && c <= '9') {
				val = (val * 10) + (long)(c - '0');
				// val is actually 'unsigned long int' and can't get < 0
				if (val < (rlim_t) 0)
					break;
			}
			if (c)
				ash_msg_and_raise_error("bad number");
			val <<= l->factor_shift;
		}
	}
	if (all) {
		const char *lname = limits_name;
		for (l = limits_tbl; l != &limits_tbl[ARRAY_SIZE(limits_tbl)]; l++) {
			getrlimit(l->cmd, &limit);
			out1fmt("%-20s ", lname);
			lname += strlen(lname) + 1;
			printlim(how, &limit, l);
		}
		return 0;
	}

	getrlimit(l->cmd, &limit);
	if (set) {
		if (how & HARD)
			limit.rlim_max = val;
		if (how & SOFT)
			limit.rlim_cur = val;
		if (setrlimit(l->cmd, &limit) < 0)
			ash_msg_and_raise_error("error setting limit (%m)");
	} else {
		printlim(how, &limit, l);
	}
	return 0;
}


/* ============ Math support */

#if ENABLE_ASH_MATH_SUPPORT

/* Copyright (c) 2001 Aaron Lehmann <aaronl@vitelus.com>

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/* This is my infix parser/evaluator. It is optimized for size, intended
 * as a replacement for yacc-based parsers. However, it may well be faster
 * than a comparable parser written in yacc. The supported operators are
 * listed in #defines below. Parens, order of operations, and error handling
 * are supported. This code is thread safe. The exact expression format should
 * be that which POSIX specifies for shells. */

/* The code uses a simple two-stack algorithm. See
 * http://www.onthenet.com.au/~grahamis/int2008/week02/lect02.html
 * for a detailed explanation of the infix-to-postfix algorithm on which
 * this is based (this code differs in that it applies operators immediately
 * to the stack instead of adding them to a queue to end up with an
 * expression). */

/* To use the routine, call it with an expression string and error return
 * pointer */

/*
 * Aug 24, 2001              Manuel Novoa III
 *
 * Reduced the generated code size by about 30% (i386) and fixed several bugs.
 *
 * 1) In arith_apply():
 *    a) Cached values of *numptr and &(numptr[-1]).
 *    b) Removed redundant test for zero denominator.
 *
 * 2) In arith():
 *    a) Eliminated redundant code for processing operator tokens by moving
 *       to a table-based implementation.  Also folded handling of parens
 *       into the table.
 *    b) Combined all 3 loops which called arith_apply to reduce generated
 *       code size at the cost of speed.
 *
 * 3) The following expressions were treated as valid by the original code:
 *       1()  ,    0!  ,    1 ( *3 )   .
 *    These bugs have been fixed by internally enclosing the expression in
 *    parens and then checking that all binary ops and right parens are
 *    preceded by a valid expression (NUM_TOKEN).
 *
 * Note: It may be desirable to replace Aaron's test for whitespace with
 * ctype's isspace() if it is used by another busybox applet or if additional
 * whitespace chars should be considered.  Look below the "#include"s for a
 * precompiler test.
 */

/*
 * Aug 26, 2001              Manuel Novoa III
 *
 * Return 0 for null expressions.  Pointed out by Vladimir Oleynik.
 *
 * Merge in Aaron's comments previously posted to the busybox list,
 * modified slightly to take account of my changes to the code.
 *
 */

/*
 *  (C) 2003 Vladimir Oleynik <dzo@simtreas.ru>
 *
 * - allow access to variable,
 *   used recursive find value indirection (c=2*2; a="c"; $((a+=2)) produce 6)
 * - realize assign syntax (VAR=expr, +=, *= etc)
 * - realize exponentiation (** operator)
 * - realize comma separated - expr, expr
 * - realise ++expr --expr expr++ expr--
 * - realise expr ? expr : expr (but, second expr calculate always)
 * - allow hexadecimal and octal numbers
 * - was restored loses XOR operator
 * - remove one goto label, added three ;-)
 * - protect $((num num)) as true zero expr (Manuel`s error)
 * - always use special isspace(), see comment from bash ;-)
 */

#define arith_isspace(arithval) \
	(arithval == ' ' || arithval == '\n' || arithval == '\t')

typedef unsigned char operator;

/* An operator's token id is a bit of a bitfield. The lower 5 bits are the
 * precedence, and 3 high bits are an ID unique across operators of that
 * precedence. The ID portion is so that multiple operators can have the
 * same precedence, ensuring that the leftmost one is evaluated first.
 * Consider * and /. */

#define tok_decl(prec,id) (((id)<<5)|(prec))
#define PREC(op) ((op) & 0x1F)

#define TOK_LPAREN tok_decl(0,0)

#define TOK_COMMA tok_decl(1,0)

#define TOK_ASSIGN tok_decl(2,0)
#define TOK_AND_ASSIGN tok_decl(2,1)
#define TOK_OR_ASSIGN tok_decl(2,2)
#define TOK_XOR_ASSIGN tok_decl(2,3)
#define TOK_PLUS_ASSIGN tok_decl(2,4)
#define TOK_MINUS_ASSIGN tok_decl(2,5)
#define TOK_LSHIFT_ASSIGN tok_decl(2,6)
#define TOK_RSHIFT_ASSIGN tok_decl(2,7)

#define TOK_MUL_ASSIGN tok_decl(3,0)
#define TOK_DIV_ASSIGN tok_decl(3,1)
#define TOK_REM_ASSIGN tok_decl(3,2)

/* all assign is right associativity and precedence eq, but (7+3)<<5 > 256 */
#define convert_prec_is_assing(prec) do { if (prec == 3) prec = 2; } while (0)

/* conditional is right associativity too */
#define TOK_CONDITIONAL tok_decl(4,0)
#define TOK_CONDITIONAL_SEP tok_decl(4,1)

#define TOK_OR tok_decl(5,0)

#define TOK_AND tok_decl(6,0)

#define TOK_BOR tok_decl(7,0)

#define TOK_BXOR tok_decl(8,0)

#define TOK_BAND tok_decl(9,0)

#define TOK_EQ tok_decl(10,0)
#define TOK_NE tok_decl(10,1)

#define TOK_LT tok_decl(11,0)
#define TOK_GT tok_decl(11,1)
#define TOK_GE tok_decl(11,2)
#define TOK_LE tok_decl(11,3)

#define TOK_LSHIFT tok_decl(12,0)
#define TOK_RSHIFT tok_decl(12,1)

#define TOK_ADD tok_decl(13,0)
#define TOK_SUB tok_decl(13,1)

#define TOK_MUL tok_decl(14,0)
#define TOK_DIV tok_decl(14,1)
#define TOK_REM tok_decl(14,2)

/* exponent is right associativity */
#define TOK_EXPONENT tok_decl(15,1)

/* For now unary operators. */
#define UNARYPREC 16
#define TOK_BNOT tok_decl(UNARYPREC,0)
#define TOK_NOT tok_decl(UNARYPREC,1)

#define TOK_UMINUS tok_decl(UNARYPREC+1,0)
#define TOK_UPLUS tok_decl(UNARYPREC+1,1)

#define PREC_PRE (UNARYPREC+2)

#define TOK_PRE_INC tok_decl(PREC_PRE, 0)
#define TOK_PRE_DEC tok_decl(PREC_PRE, 1)

#define PREC_POST (UNARYPREC+3)

#define TOK_POST_INC tok_decl(PREC_POST, 0)
#define TOK_POST_DEC tok_decl(PREC_POST, 1)

#define SPEC_PREC (UNARYPREC+4)

#define TOK_NUM tok_decl(SPEC_PREC, 0)
#define TOK_RPAREN tok_decl(SPEC_PREC, 1)

#define NUMPTR (*numstackptr)

static int
tok_have_assign(operator op)
{
	operator prec = PREC(op);

	convert_prec_is_assing(prec);
	return (prec == PREC(TOK_ASSIGN) ||
			prec == PREC_PRE || prec == PREC_POST);
}

static int
is_right_associativity(operator prec)
{
	return (prec == PREC(TOK_ASSIGN) || prec == PREC(TOK_EXPONENT)
	        || prec == PREC(TOK_CONDITIONAL));
}

typedef struct {
	arith_t val;
	arith_t contidional_second_val;
	char contidional_second_val_initialized;
	char *var;      /* if NULL then is regular number,
			   else is variable name */
} v_n_t;

typedef struct chk_var_recursive_looped_t {
	const char *var;
	struct chk_var_recursive_looped_t *next;
} chk_var_recursive_looped_t;

static chk_var_recursive_looped_t *prev_chk_var_recursive;

static int
arith_lookup_val(v_n_t *t)
{
	if (t->var) {
		const char * p = lookupvar(t->var);

		if (p) {
			int errcode;

			/* recursive try as expression */
			chk_var_recursive_looped_t *cur;
			chk_var_recursive_looped_t cur_save;

			for (cur = prev_chk_var_recursive; cur; cur = cur->next) {
				if (strcmp(cur->var, t->var) == 0) {
					/* expression recursion loop detected */
					return -5;
				}
			}
			/* save current lookuped var name */
			cur = prev_chk_var_recursive;
			cur_save.var = t->var;
			cur_save.next = cur;
			prev_chk_var_recursive = &cur_save;

			t->val = arith (p, &errcode);
			/* restore previous ptr after recursiving */
			prev_chk_var_recursive = cur;
			return errcode;
		}
		/* allow undefined var as 0 */
		t->val = 0;
	}
	return 0;
}

/* "applying" a token means performing it on the top elements on the integer
 * stack. For a unary operator it will only change the top element, but a
 * binary operator will pop two arguments and push a result */
static int
arith_apply(operator op, v_n_t *numstack, v_n_t **numstackptr)
{
	v_n_t *numptr_m1;
	arith_t numptr_val, rez;
	int ret_arith_lookup_val;

	/* There is no operator that can work without arguments */
	if (NUMPTR == numstack) goto err;
	numptr_m1 = NUMPTR - 1;

	/* check operand is var with noninteger value */
	ret_arith_lookup_val = arith_lookup_val(numptr_m1);
	if (ret_arith_lookup_val)
		return ret_arith_lookup_val;

	rez = numptr_m1->val;
	if (op == TOK_UMINUS)
		rez *= -1;
	else if (op == TOK_NOT)
		rez = !rez;
	else if (op == TOK_BNOT)
		rez = ~rez;
	else if (op == TOK_POST_INC || op == TOK_PRE_INC)
		rez++;
	else if (op == TOK_POST_DEC || op == TOK_PRE_DEC)
		rez--;
	else if (op != TOK_UPLUS) {
		/* Binary operators */

		/* check and binary operators need two arguments */
		if (numptr_m1 == numstack) goto err;

		/* ... and they pop one */
		--NUMPTR;
		numptr_val = rez;
		if (op == TOK_CONDITIONAL) {
			if (! numptr_m1->contidional_second_val_initialized) {
				/* protect $((expr1 ? expr2)) without ": expr" */
				goto err;
			}
			rez = numptr_m1->contidional_second_val;
		} else if (numptr_m1->contidional_second_val_initialized) {
			/* protect $((expr1 : expr2)) without "expr ? " */
			goto err;
		}
		numptr_m1 = NUMPTR - 1;
		if (op != TOK_ASSIGN) {
			/* check operand is var with noninteger value for not '=' */
			ret_arith_lookup_val = arith_lookup_val(numptr_m1);
			if (ret_arith_lookup_val)
				return ret_arith_lookup_val;
		}
		if (op == TOK_CONDITIONAL) {
			numptr_m1->contidional_second_val = rez;
		}
		rez = numptr_m1->val;
		if (op == TOK_BOR || op == TOK_OR_ASSIGN)
			rez |= numptr_val;
		else if (op == TOK_OR)
			rez = numptr_val || rez;
		else if (op == TOK_BAND || op == TOK_AND_ASSIGN)
			rez &= numptr_val;
		else if (op == TOK_BXOR || op == TOK_XOR_ASSIGN)
			rez ^= numptr_val;
		else if (op == TOK_AND)
			rez = rez && numptr_val;
		else if (op == TOK_EQ)
			rez = (rez == numptr_val);
		else if (op == TOK_NE)
			rez = (rez != numptr_val);
		else if (op == TOK_GE)
			rez = (rez >= numptr_val);
		else if (op == TOK_RSHIFT || op == TOK_RSHIFT_ASSIGN)
			rez >>= numptr_val;
		else if (op == TOK_LSHIFT || op == TOK_LSHIFT_ASSIGN)
			rez <<= numptr_val;
		else if (op == TOK_GT)
			rez = (rez > numptr_val);
		else if (op == TOK_LT)
			rez = (rez < numptr_val);
		else if (op == TOK_LE)
			rez = (rez <= numptr_val);
		else if (op == TOK_MUL || op == TOK_MUL_ASSIGN)
			rez *= numptr_val;
		else if (op == TOK_ADD || op == TOK_PLUS_ASSIGN)
			rez += numptr_val;
		else if (op == TOK_SUB || op == TOK_MINUS_ASSIGN)
			rez -= numptr_val;
		else if (op == TOK_ASSIGN || op == TOK_COMMA)
			rez = numptr_val;
		else if (op == TOK_CONDITIONAL_SEP) {
			if (numptr_m1 == numstack) {
				/* protect $((expr : expr)) without "expr ? " */
				goto err;
			}
			numptr_m1->contidional_second_val_initialized = op;
			numptr_m1->contidional_second_val = numptr_val;
		} else if (op == TOK_CONDITIONAL) {
			rez = rez ?
				numptr_val : numptr_m1->contidional_second_val;
		} else if (op == TOK_EXPONENT) {
			if (numptr_val < 0)
				return -3;      /* exponent less than 0 */
			else {
				arith_t c = 1;

				if (numptr_val)
					while (numptr_val--)
						c *= rez;
				rez = c;
			}
		} else if (numptr_val==0)          /* zero divisor check */
			return -2;
		else if (op == TOK_DIV || op == TOK_DIV_ASSIGN)
			rez /= numptr_val;
		else if (op == TOK_REM || op == TOK_REM_ASSIGN)
			rez %= numptr_val;
	}
	if (tok_have_assign(op)) {
		char buf[sizeof(arith_t_type)*3 + 2];

		if (numptr_m1->var == NULL) {
			/* Hmm, 1=2 ? */
			goto err;
		}
		/* save to shell variable */
#if ENABLE_ASH_MATH_SUPPORT_64
		snprintf(buf, sizeof(buf), "%lld", (arith_t_type) rez);
#else
		snprintf(buf, sizeof(buf), "%ld", (arith_t_type) rez);
#endif
		setvar(numptr_m1->var, buf, 0);
		/* after saving, make previous value for v++ or v-- */
		if (op == TOK_POST_INC)
			rez--;
		else if (op == TOK_POST_DEC)
			rez++;
	}
	numptr_m1->val = rez;
	/* protect geting var value, is number now */
	numptr_m1->var = NULL;
	return 0;
 err:
	return -1;
}

/* longest must be first */
static const char op_tokens[] ALIGN1 = {
	'<','<','=',0, TOK_LSHIFT_ASSIGN,
	'>','>','=',0, TOK_RSHIFT_ASSIGN,
	'<','<',    0, TOK_LSHIFT,
	'>','>',    0, TOK_RSHIFT,
	'|','|',    0, TOK_OR,
	'&','&',    0, TOK_AND,
	'!','=',    0, TOK_NE,
	'<','=',    0, TOK_LE,
	'>','=',    0, TOK_GE,
	'=','=',    0, TOK_EQ,
	'|','=',    0, TOK_OR_ASSIGN,
	'&','=',    0, TOK_AND_ASSIGN,
	'*','=',    0, TOK_MUL_ASSIGN,
	'/','=',    0, TOK_DIV_ASSIGN,
	'%','=',    0, TOK_REM_ASSIGN,
	'+','=',    0, TOK_PLUS_ASSIGN,
	'-','=',    0, TOK_MINUS_ASSIGN,
	'-','-',    0, TOK_POST_DEC,
	'^','=',    0, TOK_XOR_ASSIGN,
	'+','+',    0, TOK_POST_INC,
	'*','*',    0, TOK_EXPONENT,
	'!',        0, TOK_NOT,
	'<',        0, TOK_LT,
	'>',        0, TOK_GT,
	'=',        0, TOK_ASSIGN,
	'|',        0, TOK_BOR,
	'&',        0, TOK_BAND,
	'*',        0, TOK_MUL,
	'/',        0, TOK_DIV,
	'%',        0, TOK_REM,
	'+',        0, TOK_ADD,
	'-',        0, TOK_SUB,
	'^',        0, TOK_BXOR,
	/* uniq */
	'~',        0, TOK_BNOT,
	',',        0, TOK_COMMA,
	'?',        0, TOK_CONDITIONAL,
	':',        0, TOK_CONDITIONAL_SEP,
	')',        0, TOK_RPAREN,
	'(',        0, TOK_LPAREN,
	0
};
/* ptr to ")" */
#define endexpression (&op_tokens[sizeof(op_tokens)-7])

static arith_t
arith(const char *expr, int *perrcode)
{
	char arithval; /* Current character under analysis */
	operator lasttok, op;
	operator prec;
	operator *stack, *stackptr;
	const char *p = endexpression;
	int errcode;
	v_n_t *numstack, *numstackptr;
	unsigned datasizes = strlen(expr) + 2;

	/* Stack of integers */
	/* The proof that there can be no more than strlen(startbuf)/2+1 integers
	 * in any given correct or incorrect expression is left as an exercise to
	 * the reader. */
	numstackptr = numstack = alloca((datasizes / 2) * sizeof(numstack[0]));
	/* Stack of operator tokens */
	stackptr = stack = alloca(datasizes * sizeof(stack[0]));

	*stackptr++ = lasttok = TOK_LPAREN;     /* start off with a left paren */
	*perrcode = errcode = 0;

	while (1) {
		arithval = *expr;
		if (arithval == 0) {
			if (p == endexpression) {
				/* Null expression. */
				return 0;
			}

			/* This is only reached after all tokens have been extracted from the
			 * input stream. If there are still tokens on the operator stack, they
			 * are to be applied in order. At the end, there should be a final
			 * result on the integer stack */

			if (expr != endexpression + 1) {
				/* If we haven't done so already, */
				/* append a closing right paren */
				expr = endexpression;
				/* and let the loop process it. */
				continue;
			}
			/* At this point, we're done with the expression. */
			if (numstackptr != numstack+1) {
				/* ... but if there isn't, it's bad */
 err:
				*perrcode = -1;
				return *perrcode;
			}
			if (numstack->var) {
				/* expression is $((var)) only, lookup now */
				errcode = arith_lookup_val(numstack);
			}
 ret:
			*perrcode = errcode;
			return numstack->val;
		}

		/* Continue processing the expression. */
		if (arith_isspace(arithval)) {
			/* Skip whitespace */
			goto prologue;
		}
		p = endofname(expr);
		if (p != expr) {
			size_t var_name_size = (p-expr) + 1;  /* trailing zero */

			numstackptr->var = alloca(var_name_size);
			safe_strncpy(numstackptr->var, expr, var_name_size);
			expr = p;
 num:
			numstackptr->contidional_second_val_initialized = 0;
			numstackptr++;
			lasttok = TOK_NUM;
			continue;
		}
		if (isdigit(arithval)) {
			numstackptr->var = NULL;
#if ENABLE_ASH_MATH_SUPPORT_64
			numstackptr->val = strtoll(expr, (char **) &expr, 0);
#else
			numstackptr->val = strtol(expr, (char **) &expr, 0);
#endif
			goto num;
		}
		for (p = op_tokens; ; p++) {
			const char *o;

			if (*p == 0) {
				/* strange operator not found */
				goto err;
			}
			for (o = expr; *p && *o == *p; p++)
				o++;
			if (! *p) {
				/* found */
				expr = o - 1;
				break;
			}
			/* skip tail uncompared token */
			while (*p)
				p++;
			/* skip zero delim */
			p++;
		}
		op = p[1];

		/* post grammar: a++ reduce to num */
		if (lasttok == TOK_POST_INC || lasttok == TOK_POST_DEC)
			lasttok = TOK_NUM;

		/* Plus and minus are binary (not unary) _only_ if the last
		 * token was as number, or a right paren (which pretends to be
		 * a number, since it evaluates to one). Think about it.
		 * It makes sense. */
		if (lasttok != TOK_NUM) {
			switch (op) {
			case TOK_ADD:
				op = TOK_UPLUS;
				break;
			case TOK_SUB:
				op = TOK_UMINUS;
				break;
			case TOK_POST_INC:
				op = TOK_PRE_INC;
				break;
			case TOK_POST_DEC:
				op = TOK_PRE_DEC;
				break;
			}
		}
		/* We don't want a unary operator to cause recursive descent on the
		 * stack, because there can be many in a row and it could cause an
		 * operator to be evaluated before its argument is pushed onto the
		 * integer stack. */
		/* But for binary operators, "apply" everything on the operator
		 * stack until we find an operator with a lesser priority than the
		 * one we have just extracted. */
		/* Left paren is given the lowest priority so it will never be
		 * "applied" in this way.
		 * if associativity is right and priority eq, applied also skip
		 */
		prec = PREC(op);
		if ((prec > 0 && prec < UNARYPREC) || prec == SPEC_PREC) {
			/* not left paren or unary */
			if (lasttok != TOK_NUM) {
				/* binary op must be preceded by a num */
				goto err;
			}
			while (stackptr != stack) {
				if (op == TOK_RPAREN) {
					/* The algorithm employed here is simple: while we don't
					 * hit an open paren nor the bottom of the stack, pop
					 * tokens and apply them */
					if (stackptr[-1] == TOK_LPAREN) {
						--stackptr;
						/* Any operator directly after a */
						lasttok = TOK_NUM;
						/* close paren should consider itself binary */
						goto prologue;
					}
				} else {
					operator prev_prec = PREC(stackptr[-1]);

					convert_prec_is_assing(prec);
					convert_prec_is_assing(prev_prec);
					if (prev_prec < prec)
						break;
					/* check right assoc */
					if (prev_prec == prec && is_right_associativity(prec))
						break;
				}
				errcode = arith_apply(*--stackptr, numstack, &numstackptr);
				if (errcode) goto ret;
			}
			if (op == TOK_RPAREN) {
				goto err;
			}
		}

		/* Push this operator to the stack and remember it. */
		*stackptr++ = lasttok = op;
 prologue:
		++expr;
	} /* while */
}
#endif /* ASH_MATH_SUPPORT */


/* ============ main() and helpers */

/*
 * Called to exit the shell.
 */
static void exitshell(void) ATTRIBUTE_NORETURN;
static void
exitshell(void)
{
	struct jmploc loc;
	char *p;
	int status;

	status = exitstatus;
	TRACE(("pid %d, exitshell(%d)\n", getpid(), status));
	if (setjmp(loc.loc)) {
		if (exception == EXEXIT)
/* dash bug: it just does _exit(exitstatus) here
 * but we have to do setjobctl(0) first!
 * (bug is still not fixed in dash-0.5.3 - if you run dash
 * under Midnight Commander, on exit from dash MC is backgrounded) */
			status = exitstatus;
		goto out;
	}
	exception_handler = &loc;
	p = trap[0];
	if (p) {
		trap[0] = NULL;
		evalstring(p, 0);
	}
	flush_stdout_stderr();
 out:
	setjobctl(0);
	_exit(status);
	/* NOTREACHED */
}

static void
init(void)
{
	/* from input.c: */
	basepf.nextc = basepf.buf = basebuf;

	/* from trap.c: */
	signal(SIGCHLD, SIG_DFL);

	/* from var.c: */
	{
		char **envp;
		char ppid[sizeof(int)*3 + 1];
		const char *p;
		struct stat st1, st2;

		initvar();
		for (envp = environ; envp && *envp; envp++) {
			if (strchr(*envp, '=')) {
				setvareq(*envp, VEXPORT|VTEXTFIXED);
			}
		}

		snprintf(ppid, sizeof(ppid), "%u", (unsigned) getppid());
		setvar("PPID", ppid, 0);

		p = lookupvar("PWD");
		if (p)
			if (*p != '/' || stat(p, &st1) || stat(".", &st2)
			 || st1.st_dev != st2.st_dev || st1.st_ino != st2.st_ino)
				p = '\0';
		setpwd(p, 0);
	}
}

/*
 * Process the shell command line arguments.
 */
static void
procargs(char **argv)
{
	int i;
	const char *xminusc;
	char **xargv;

	xargv = argv;
	arg0 = xargv[0];
	/* if (xargv[0]) - mmm, this is always true! */
		xargv++;
	for (i = 0; i < NOPTS; i++)
		optlist[i] = 2;
	argptr = xargv;
	if (options(1)) {
		/* it already printed err message */
		raise_exception(EXERROR);
	}
	xargv = argptr;
	xminusc = minusc;
	if (*xargv == NULL) {
		if (xminusc)
			ash_msg_and_raise_error(bb_msg_requires_arg, "-c");
		sflag = 1;
	}
	if (iflag == 2 && sflag == 1 && isatty(0) && isatty(1))
		iflag = 1;
	if (mflag == 2)
		mflag = iflag;
	for (i = 0; i < NOPTS; i++)
		if (optlist[i] == 2)
			optlist[i] = 0;
#if DEBUG == 2
	debug = 1;
#endif
	/* POSIX 1003.2: first arg after -c cmd is $0, remainder $1... */
	if (xminusc) {
		minusc = *xargv++;
		if (*xargv)
			goto setarg0;
	} else if (!sflag) {
		setinputfile(*xargv, 0);
 setarg0:
		arg0 = *xargv++;
		commandname = arg0;
	}

	shellparam.p = xargv;
#if ENABLE_ASH_GETOPTS
	shellparam.optind = 1;
	shellparam.optoff = -1;
#endif
	/* assert(shellparam.malloced == 0 && shellparam.nparam == 0); */
	while (*xargv) {
		shellparam.nparam++;
		xargv++;
	}
	optschanged();
}

/*
 * Read /etc/profile or .profile.
 */
static void
read_profile(const char *name)
{
	int skip;

	if (setinputfile(name, INPUT_PUSH_FILE | INPUT_NOFILE_OK) < 0)
		return;
	skip = cmdloop(0);
	popfile();
	if (skip)
		exitshell();
}

/*
 * This routine is called when an error or an interrupt occurs in an
 * interactive shell and control is returned to the main command loop.
 */
static void
reset(void)
{
	/* from eval.c: */
	evalskip = 0;
	loopnest = 0;
	/* from input.c: */
	parselleft = parsenleft = 0;      /* clear input buffer */
	popallfiles();
	/* from parser.c: */
	tokpushback = 0;
	checkkwd = 0;
	/* from redir.c: */
	clearredir(0);
}

#if PROFILE
static short profile_buf[16384];
extern int etext();
#endif

/*
 * Main routine.  We initialize things, parse the arguments, execute
 * profiles if we're a login shell, and then call cmdloop to execute
 * commands.  The setjmp call sets up the location to jump to when an
 * exception occurs.  When an exception occurs the variable "state"
 * is used to figure out how far we had gotten.
 */
int ash_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int ash_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	char *shinit;
	volatile int state;
	struct jmploc jmploc;
	struct stackmark smark;

	/* Initialize global data */
	INIT_G_misc();
	INIT_G_memstack();
	INIT_G_var();
#if ENABLE_ASH_ALIAS
	INIT_G_alias();
#endif
	INIT_G_cmdtable();

#if PROFILE
	monitor(4, etext, profile_buf, sizeof(profile_buf), 50);
#endif

#if ENABLE_FEATURE_EDITING
	line_input_state = new_line_input_t(FOR_SHELL | WITH_PATH_LOOKUP);
#endif
	state = 0;
	if (setjmp(jmploc.loc)) {
		int e;
		int s;

		reset();

		e = exception;
		if (e == EXERROR)
			exitstatus = 2;
		s = state;
		if (e == EXEXIT || s == 0 || iflag == 0 || shlvl)
			exitshell();

		if (e == EXINT) {
			outcslow('\n', stderr);
		}
		popstackmark(&smark);
		FORCE_INT_ON; /* enable interrupts */
		if (s == 1)
			goto state1;
		if (s == 2)
			goto state2;
		if (s == 3)
			goto state3;
		goto state4;
	}
	exception_handler = &jmploc;
#if DEBUG
	opentrace();
	trace_puts("Shell args: ");
	trace_puts_args(argv);
#endif
	rootpid = getpid();

#if ENABLE_ASH_RANDOM_SUPPORT
	rseed = rootpid + time(NULL);
#endif
	init();
	setstackmark(&smark);
	procargs(argv);

#if ENABLE_FEATURE_EDITING_SAVEHISTORY
	if (iflag) {
		const char *hp = lookupvar("HISTFILE");

		if (hp == NULL) {
			hp = lookupvar("HOME");
			if (hp != NULL) {
				char *defhp = concat_path_file(hp, ".ash_history");
				setvar("HISTFILE", defhp, 0);
				free(defhp);
			}
		}
	}
#endif
	if (argv[0] && argv[0][0] == '-')
		isloginsh = 1;
	if (isloginsh) {
		state = 1;
		read_profile("/etc/profile");
 state1:
		state = 2;
		read_profile(".profile");
	}
 state2:
	state = 3;
	if (
#ifndef linux
	 getuid() == geteuid() && getgid() == getegid() &&
#endif
	 iflag
	) {
		shinit = lookupvar("ENV");
		if (shinit != NULL && *shinit != '\0') {
			read_profile(shinit);
		}
	}
 state3:
	state = 4;
	if (minusc)
		evalstring(minusc, 0);

	if (sflag || minusc == NULL) {
#if ENABLE_FEATURE_EDITING_SAVEHISTORY
		if (iflag) {
			const char *hp = lookupvar("HISTFILE");

			if (hp != NULL)
				line_input_state->hist_file = hp;
		}
#endif
 state4: /* XXX ??? - why isn't this before the "if" statement */
		cmdloop(1);
	}
#if PROFILE
	monitor(0);
#endif
#ifdef GPROF
	{
		extern void _mcleanup(void);
		_mcleanup();
	}
#endif
	exitshell();
	/* NOTREACHED */
}

#if DEBUG
const char *applet_name = "debug stuff usage";
int main(int argc, char **argv)
{
	return ash_main(argc, argv);
}
#endif


/*-
 * Copyright (c) 1989, 1991, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
