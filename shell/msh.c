/* vi: set sw=4 ts=4: */
/*
 * Minix shell port for busybox
 *
 * This version of the Minix shell was adapted for use in busybox
 * by Erik Andersen <andersen@codepoet.org>
 *
 * - backtick expansion did not work properly
 *   Jonas Holmberg <jonas.holmberg@axis.com>
 *   Robert Schwebel <r.schwebel@pengutronix.de>
 *   Erik Andersen <andersen@codepoet.org>
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
 * Original copyright notice is retained at the end of this file.
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "cmdedit.h"
#include "busybox.h"


/* -------- sh.h -------- */
/*
 * shell
 */

#define	LINELIM	2100
#define	NPUSH	8	/* limit to input nesting */

#undef NOFILE
#define	NOFILE	20	/* Number of open files */
#define	NUFILE	10	/* Number of user-accessible files */
#define	FDBASE	10	/* First file usable by Shell */

/*
 * values returned by wait
 */
#define	WAITSIG(s) ((s)&0177)
#define	WAITVAL(s) (((s)>>8)&0377)
#define	WAITCORE(s) (((s)&0200)!=0)

/*
 * library and system defintions
 */
typedef void xint;	/* base type of jmp_buf, for not broken compilers */

/*
 * shell components
 */

#define	QUOTE	0200

#define	NOBLOCK	((struct op *)NULL)
#define	NOWORD	((char *)NULL)
#define	NOWORDS	((char **)NULL)
#define	NOPIPE	((int *)NULL)

/*
 * Description of a command or an operation on commands.
 * Might eventually use a union.
 */
struct op {
	int	type;	/* operation type, see below */
	char	**words;	/* arguments to a command */
	struct	ioword	**ioact;	/* IO actions (eg, < > >>) */
	struct op *left;
	struct op *right;
	char	*str;	/* identifier for case and for */
};

#define	TCOM	1	/* command */
#define	TPAREN	2	/* (c-list) */
#define	TPIPE	3	/* a | b */
#define	TLIST	4	/* a [&;] b */
#define	TOR	5	/* || */
#define	TAND	6	/* && */
#define	TFOR	7
#define	TDO	8
#define	TCASE	9
#define	TIF	10
#define	TWHILE	11
#define	TUNTIL	12
#define	TELIF	13
#define	TPAT	14	/* pattern in case */
#define	TBRACE	15	/* {c-list} */
#define	TASYNC	16	/* c & */

/*
 * actions determining the environment of a process
 */
#define	BIT(i)	(1<<(i))
#define	FEXEC	BIT(0)	/* execute without forking */

/*
 * flags to control evaluation of words
 */
#define	DOSUB	1	/* interpret $, `, and quotes */
#define	DOBLANK	2	/* perform blank interpretation */
#define	DOGLOB	4	/* interpret [?* */
#define	DOKEY	8	/* move words with `=' to 2nd arg. list */
#define	DOTRIM	16	/* trim resulting string */

#define	DOALL	(DOSUB|DOBLANK|DOGLOB|DOKEY|DOTRIM)

static	char	**dolv;
static	int	dolc;
static	int	exstat;
static  char	gflg;
static  int	interactive;	/* Is this an interactive shell */
static  int	execflg;
static  int	multiline;	/* \n changed to ; */
static  struct	op	*outtree;	/* result from parser */

static	xint	*failpt;
static	xint	*errpt;
static	struct brkcon	*brklist;
static	int	isbreak;
static int newfile(char *s);
static char *findeq(char *cp);
static char *cclass(char *p, int sub);
static void initarea(void);
extern int msh_main(int argc, char **argv);


struct	brkcon {
	jmp_buf	brkpt;
	struct	brkcon	*nextlev;
} ;

/*
 * redirection
 */
struct ioword {
	short	io_unit;	/* unit affected */
	short	io_flag;	/* action (below) */
	char	*io_name;	/* file name */
};
#define	IOREAD	1	/* < */
#define	IOHERE	2	/* << (here file) */
#define	IOWRITE	4	/* > */
#define	IOCAT	8	/* >> */
#define	IOXHERE	16	/* ${}, ` in << */
#define	IODUP	32	/* >&digit */
#define	IOCLOSE	64	/* >&- */

#define	IODEFAULT (-1)	/* token for default IO unit */

static	struct	wdblock	*wdlist;
static	struct	wdblock	*iolist;

/*
 * parsing & execution environment
 */
static struct	env {
	char	*linep;
	struct	io	*iobase;
	struct	io	*iop;
	xint	*errpt;
	int	iofd;
	struct	env	*oenv;
} e;

/*
 * flags:
 * -e: quit on error
 * -k: look for name=value everywhere on command line
 * -n: no execution
 * -t: exit after reading and executing one command
 * -v: echo as read
 * -x: trace
 * -u: unset variables net diagnostic
 */
static	char	*flag;

static	char	*null;	/* null value for variable */
static	int	intr;	/* interrupt pending */

static	char	*trap[_NSIG+1];
static	char	ourtrap[_NSIG+1];
static	int	trapset;	/* trap pending */

static	int	heedint;	/* heed interrupt signals */

static	int	yynerrs;	/* yacc */

static	char	line[LINELIM];
static	char	*elinep;

/*
 * other functions
 */
static int(*inbuilt(char *s))(struct op *);


static char *rexecve (char *c , char **v, char **envp );
static char *space (int n );
static char *strsave (char *s, int a );
static char *evalstr (char *cp, int f );
static char *putn (int n );
static char *itoa (int n );
static char *unquote (char *as );
static struct var *lookup (char *n );
static int rlookup (char *n );
static struct wdblock *glob (char *cp, struct wdblock *wb );
static int my_getc( int ec);
static int subgetc (int ec, int quoted );
static char **makenv (void);
static char **eval (char **ap, int f );
static int setstatus (int s );
static int waitfor (int lastpid, int canintr );

static void onintr (int s ); /* SIGINT handler */

static int newenv (int f );
static void quitenv (void);
static void err (char *s );
static int anys (char *s1, char *s2 );
static int any (int c, char *s );
static void next (int f );
static void setdash (void);
static void onecommand (void);
static void runtrap (int i );
static int gmatch (char *s, char *p );

/*
 * error handling
 */
static void leave (void); /* abort shell (or fail in subshell) */
static void fail (void);	 /* fail but return to process next command */
static void warn (char *s );
static void sig (int i );	 /* default signal handler */



/* -------- area stuff -------- */

#define	REGSIZE		sizeof(struct region)
#define GROWBY		256
//#define	SHRINKBY   64
#undef	SHRINKBY
#define FREE 32767
#define BUSY 0
#define	ALIGN (sizeof(int)-1)


struct region {
	struct	region *next;
	int	area;
};



/* -------- grammar stuff -------- */
typedef union {
	char	*cp;
	char	**wp;
	int	i;
	struct	op *o;
} YYSTYPE;
#define	WORD	256
#define	LOGAND	257
#define	LOGOR	258
#define	BREAK	259
#define	IF	260
#define	THEN	261
#define	ELSE	262
#define	ELIF	263
#define	FI	264
#define	CASE	265
#define	ESAC	266
#define	FOR	267
#define	WHILE	268
#define	UNTIL	269
#define	DO	270
#define	DONE	271
#define	IN	272
#define	YYERRCODE 300

/* flags to yylex */
#define	CONTIN	01	/* skip new lines to complete command */

#define	SYNTAXERR	zzerr()
static struct op *pipeline(int cf );
static struct op *andor(void);
static struct op *c_list(void);
static int synio(int cf );
static void musthave (int c, int cf );
static struct op *simple(void);
static struct op *nested(int type, int mark );
static struct op *command(int cf );
static struct op *dogroup(int onlydone );
static struct op *thenpart(void);
static struct op *elsepart(void);
static struct op *caselist(void);
static struct op *casepart(void);
static char **pattern(void);
static char **wordlist(void);
static struct op *list(struct op *t1, struct op *t2 );
static struct op *block(int type, struct op *t1, struct op *t2, char **wp );
static struct op *newtp(void);
static struct op *namelist(struct op *t );
static char **copyw(void);
static void word(char *cp );
static struct ioword **copyio(void);
static struct ioword *io (int u, int f, char *cp );
static void zzerr(void);
static void yyerror(char *s );
static int yylex(int cf );
static int collect(int c, int c1 );
static int dual(int c );
static void diag(int ec );
static char *tree(unsigned size );

/* -------- var.h -------- */

struct	var {
	char	*value;
	char	*name;
	struct	var	*next;
	char	status;
};
#define	COPYV	1	/* flag to setval, suggesting copy */
#define	RONLY	01	/* variable is read-only */
#define	EXPORT	02	/* variable is to be exported */
#define	GETCELL	04	/* name & value space was got with getcell */

static	struct	var	*vlist;		/* dictionary */

static	struct	var	*homedir;	/* home directory */
static	struct	var	*prompt;	/* main prompt */
static	struct	var	*cprompt;	/* continuation prompt */
static	struct	var	*path;		/* search path for commands */
static	struct	var	*shell;		/* shell to interpret command files */
static	struct	var	*ifs;		/* field separators */

static int yyparse (void);
static struct var *lookup (char *n );
static void setval (struct var *vp, char *val );
static void nameval (struct var *vp, char *val, char *name );
static void export (struct var *vp );
static void ronly (struct var *vp );
static int isassign (char *s );
static int checkname (char *cp );
static int assign (char *s, int cf );
static void putvlist (int f, int out );
static int eqname (char *n1, char *n2 );

static int execute (struct op *t, int *pin, int *pout, int act );

/* -------- io.h -------- */
/* io buffer */
struct iobuf {
  unsigned id;				/* buffer id */
  char buf[512];			/* buffer */
  char *bufp;				/* pointer into buffer */
  char *ebufp;				/* pointer to end of buffer */
};

/* possible arguments to an IO function */
struct ioarg {
	char	*aword;
	char	**awordlist;
	int	afile;		/* file descriptor */
	unsigned afid;		/* buffer id */
	long	afpos;		/* file position */
	struct iobuf *afbuf;	/* buffer for this file */
};
//static struct ioarg ioargstack[NPUSH];
#define AFID_NOBUF	(~0)
#define AFID_ID		0

/* an input generator's state */
struct	io {
	int	(*iofn)(struct  ioarg *, struct io *);
	struct	ioarg	*argp;
	int	peekc;
	char	prev;		/* previous character read by readc() */
	char	nlcount;	/* for `'s */
	char	xchar;		/* for `'s */
	char	task;		/* reason for pushed IO */
};
//static	struct	io	iostack[NPUSH];
#define	XOTHER	0	/* none of the below */
#define	XDOLL	1	/* expanding ${} */
#define	XGRAVE	2	/* expanding `'s */
#define	XIO	3	/* file IO */

/* in substitution */
#define	INSUB()	(e.iop->task == XGRAVE || e.iop->task == XDOLL)

/*
 * input generators for IO structure
 */
static int nlchar (struct ioarg *ap );
static int strchar (struct ioarg *ap );
static int qstrchar (struct ioarg *ap );
static int filechar (struct ioarg *ap );
static int herechar (struct ioarg *ap );
static int linechar (struct ioarg *ap );
static int gravechar (struct ioarg *ap, struct io *iop );
static int qgravechar (struct ioarg *ap, struct io *iop );
static int dolchar (struct ioarg *ap );
static int wdchar (struct ioarg *ap );
static void scraphere (void);
static void freehere (int area );
static void gethere (void);
static void markhere (char *s, struct ioword *iop );
static int herein (char *hname, int xdoll );
static int run (struct ioarg *argp, int (*f)(struct ioarg *));

/*
 * IO functions
 */
static int eofc (void);
static int readc (void);
static void unget (int c );
static void ioecho (int c );
static void prs (char *s );
static void prn (unsigned u );
static void closef (int i );
static void closeall (void);

/*
 * IO control
 */
static void pushio (struct ioarg *argp, int (*f)(struct ioarg *));
static int remap (int fd );
static int openpipe (int *pv );
static void closepipe (int *pv );
static struct io *setbase (struct io *ip );

static	struct	ioarg	temparg;	/* temporary for PUSHIO */
#define	PUSHIO(what,arg,gen) ((temparg.what = (arg)),pushio(&temparg,(gen)))
#define	RUN(what,arg,gen) ((temparg.what = (arg)), run(&temparg,(gen)))

/* -------- word.h -------- */

#define	NSTART	16	/* default number of words to allow for initially */

struct	wdblock {
	short	w_bsize;
	short	w_nword;
	/* bounds are arbitrary */
	char	*w_words[1];
};

static struct wdblock *addword (char *wd, struct wdblock *wb );
static struct wdblock *newword (int nw );
static char **getwords (struct wdblock *wb );

/* -------- area.h -------- */

/*
 * storage allocation
 */
static char *getcell (unsigned nbytes );
static void garbage (void);
static void setarea (char *cp, int a );
static int getarea (char *cp );
static void freearea (int a );
static void freecell (char *cp );
static	int	areanum;	/* current allocation area */

#define	NEW(type) (type *)getcell(sizeof(type))
#define	DELETE(obj)	freecell((char *)obj)


/* -------- misc stuff -------- */

static int forkexec (struct op *t, int *pin, int *pout, int act, char **wp, int *pforked );
static int iosetup (struct ioword *iop, int pipein, int pipeout );
static void echo(char **wp );
static struct op **find1case (struct op *t, char *w );
static struct op *findcase (struct op *t, char *w );
static void brkset(struct brkcon *bc );
static int dolabel(struct op *t );
static int dohelp(struct op *t );
static int dochdir(struct op *t );
static int doshift(struct op *t );
static int dologin(struct op *t );
static int doumask(struct op *t );
static int doexec(struct op *t );
static int dodot(struct op *t );
static int dowait(struct op *t );
static int doread(struct op *t );
static int doeval(struct op *t );
static int dotrap(struct op *t );
static int getsig(char *s );
static void setsig (int n, sighandler_t f);
static int getn(char *as );
static int dobreak(struct op *t );
static int docontinue(struct op *t );
static int brkcontin (char *cp, int val );
static int doexit(struct op *t );
static int doexport(struct op *t );
static int doreadonly(struct op *t );
static void rdexp (char **wp, void (*f)(struct var *), int key);
static void badid(char *s );
static int doset(struct op *t );
static void varput (char *s, int out );
static int dotimes(struct op *t );
static int expand (char *cp, struct wdblock **wbp, int f );
static char *blank(int f );
static int dollar(int quoted );
static int grave(int quoted );
static void globname (char *we, char *pp );
static char *generate (char *start1, char *end1, char *middle, char *end );
static int anyspcl(struct wdblock *wb );
static int xstrcmp (char *p1, char *p2 );
static void glob0 (char *a0, unsigned int a1, int a2, int (*a3)(char *, char *));
static void glob1 (char *base, char *lim );
static void glob2 (char *i, char *j );
static void glob3 (char *i, char *j, char *k );
static void readhere (char **name, char *s, int ec );
static void pushio (struct ioarg *argp, int (*f)(struct ioarg *));
static int xxchar(struct ioarg *ap );

struct	here {
	char	*h_tag;
	int	h_dosub;
	struct	ioword *h_iop;
	struct	here	*h_next;
};

static	char	*signame[] = {
	"Signal 0",
	"Hangup",
	(char *)NULL,	/* interrupt */
	"Quit",
	"Illegal instruction",
	"Trace/BPT trap",
	"Abort",
	"Bus error",
	"Floating Point Exception",
	"Killed",
	"SIGUSR1",
	"SIGSEGV",
	"SIGUSR2",
	(char *)NULL,	/* broken pipe */
	"Alarm clock",
	"Terminated",
};
#define	NSIGNAL (sizeof(signame)/sizeof(signame[0]))

struct res {
	char *r_name;
	int	  r_val;
};
static struct res restab[] = {
    {"for",		FOR},
    {"case",	CASE},
    {"esac",	ESAC},
    {"while",	WHILE},
    {"do",		DO},
    {"done",	DONE},
    {"if",		IF},
    {"in",		IN},
    {"then",	THEN},
    {"else",	ELSE},
    {"elif",	ELIF},
    {"until",	UNTIL},
    {"fi",		FI},

    {";;",		BREAK},
    {"||",		LOGOR},
    {"&&",		LOGAND},
    {"{",		'{'},
    {"}",		'}'},
    {0,		0},
};


struct builtincmd {
	const char *name;
	int (*builtinfunc)(struct op *t);
};
static const struct	builtincmd	builtincmds[] = {
    {".",		dodot},
    {":",		dolabel},
    {"break",	dobreak},
    {"cd",		dochdir},
    {"continue",docontinue},
    {"eval",	doeval},
    {"exec",	doexec},
    {"exit",	doexit},
    {"export",	doexport},
    {"help",	dohelp},
    {"login",	dologin},
    {"newgrp",	dologin},
    {"read",	doread},
    {"readonly",doreadonly},
    {"set",		doset},
    {"shift",	doshift},
    {"times",	dotimes},
    {"trap",	dotrap},
    {"umask",	doumask},
    {"wait",	dowait},
    {0,0}
};

/* Globals */
extern	char	**environ;	/* environment pointer */
static char	**dolv;
static int	dolc;
static int	exstat;
static char	gflg;
static int	interactive;	/* Is this an interactive shell */
static int	execflg;
static int	multiline;	/* \n changed to ; */
static struct	op	*outtree;	/* result from parser */
static xint	*failpt;
static xint	*errpt;
static struct	brkcon	*brklist;
static int	isbreak;
static struct	wdblock	*wdlist;
static struct	wdblock	*iolist;
static char	*trap[_NSIG+1];
static char	ourtrap[_NSIG+1];
static int	trapset;	/* trap pending */
static int	yynerrs;	/* yacc */
static char	line[LINELIM];
static struct	var	*vlist;		/* dictionary */
static struct	var	*homedir;	/* home directory */
static struct	var	*prompt;	/* main prompt */
static struct	var	*cprompt;	/* continuation prompt */
static struct	var	*path;		/* search path for commands */
static struct	var	*shell;		/* shell to interpret command files */
static struct	var	*ifs;		/* field separators */
static struct	ioarg ioargstack[NPUSH];
static struct	io	iostack[NPUSH];
static int	areanum;	/* current allocation area */
static int	intr;
static int	inparse;
static char	flags['z'-'a'+1];
static char	*flag = flags-'a';
static char	*elinep = line+sizeof(line)-5;
static char	*null	= "";
static int	heedint =1;
static struct env e ={line, iostack, iostack-1, (xint *)NULL, FDBASE, (struct env *)NULL};
static void (*qflag)(int) = SIG_IGN;
static	int	startl;
static	int	peeksym;
static	int	nlseen;
static	int	iounit = IODEFAULT;
static	YYSTYPE	yylval;
static struct iobuf sharedbuf = {AFID_NOBUF};
static struct iobuf mainbuf = {AFID_NOBUF};
static unsigned bufid = AFID_ID;	/* buffer id counter */
static struct ioarg temparg = {0, 0, 0, AFID_NOBUF, 0};
static	struct here *inhere;		/* list of hear docs while parsing */
static	struct here *acthere;		/* list of active here documents */
static	struct region *areabot;		/* bottom of area */
static	struct region *areatop;		/* top of area */
static	struct region *areanxt;		/* starting point of scan */
static void * brktop;
static void * brkaddr;


#ifdef CONFIG_FEATURE_COMMAND_EDITING
static char * current_prompt;
#endif

/* -------- sh.c -------- */
/*
 * shell
 */


extern int msh_main(int argc, char **argv)
{
	register int f;
	register char *s;
	int cflag;
	char *name, **ap;
	int (*iof)(struct ioarg *);

	initarea();
	if ((ap = environ) != NULL) {
		while (*ap)
			assign(*ap++, !COPYV);
		for (ap = environ; *ap;)
			export(lookup(*ap++));
	}
	closeall();
	areanum = 1;

	shell = lookup("SHELL");
	if (shell->value == null)
		setval(shell, DEFAULT_SHELL);
	export(shell);

	homedir = lookup("HOME");
	if (homedir->value == null)
		setval(homedir, "/");
	export(homedir);

	setval(lookup("$"), putn(getpid()));

	path = lookup("PATH");
	if (path->value == null) {
		if (geteuid() == 0)
			setval(path, "/sbin:/bin:/usr/sbin:/usr/bin");
		else
			setval(path, "/bin:/usr/bin");
	}
	export(path);

	ifs = lookup("IFS");
	if (ifs->value == null)
		setval(ifs, " \t\n");

	prompt = lookup("PS1");
#ifdef CONFIG_FEATURE_SH_FANCY_PROMPT
	if (prompt->value == null)
#endif
		setval(prompt, "$ ");
	if (geteuid() == 0) {
		setval(prompt, "# ");
		prompt->status &= ~EXPORT;
	}
	cprompt = lookup("PS2");
#ifdef CONFIG_FEATURE_SH_FANCY_PROMPT
	if (cprompt->value == null)
#endif
		setval(cprompt, "> ");

	iof = filechar;
	cflag = 0;
	name = *argv++;
	if (--argc >= 1) {
		if(argv[0][0] == '-' && argv[0][1] != '\0') {
			for (s = argv[0]+1; *s; s++)
				switch (*s) {
				case 'c':
					prompt->status &= ~EXPORT;
					cprompt->status &= ~EXPORT;
					setval(prompt, "");
					setval(cprompt, "");
					cflag = 1;
					if (--argc > 0)
						PUSHIO(aword, *++argv, iof = nlchar);
					break;
	
				case 'q':
					qflag = SIG_DFL;
					break;

				case 's':
					/* standard input */
					break;

				case 't':
					prompt->status &= ~EXPORT;
					setval(prompt, "");
					iof = linechar;
					break;
	
				case 'i':
					interactive++;
				default:
					if (*s>='a' && *s<='z')
						flag[(int)*s]++;
				}
		} else {
			argv--;
			argc++;
		}
		if (iof == filechar && --argc > 0) {
			setval(prompt, "");
			setval(cprompt, "");
			prompt->status &= ~EXPORT;
			cprompt->status &= ~EXPORT;
			if (newfile(name = *++argv))
				exit(1);
		}
	}
	setdash();
	if (e.iop < iostack) {
		PUSHIO(afile, 0, iof);
		if (isatty(0) && isatty(1) && !cflag) {
			interactive++;
#ifndef CONFIG_FEATURE_SH_EXTRA_QUIET 
			printf( "\n\n" BB_BANNER " Built-in shell (msh)\n");
			printf( "Enter 'help' for a list of built-in commands.\n\n");
#endif
		}
	}
	signal(SIGQUIT, qflag);
	if (name && name[0] == '-') {
		interactive++;
		if ((f = open(".profile", 0)) >= 0)
			next(remap(f));
		if ((f = open("/etc/profile", 0)) >= 0)
			next(remap(f));
	}
	if (interactive)
		signal(SIGTERM, sig);
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		signal(SIGINT, onintr);
	dolv = argv;
	dolc = argc;
	dolv[0] = name;
	if (dolc > 1) {
		for (ap = ++argv; --argc > 0;) {
			if (assign(*ap = *argv++, !COPYV)) {
				dolc--;	/* keyword */
			} else {
				ap++;
			}
		}
	}	
	setval(lookup("#"), putn((--dolc < 0) ? (dolc = 0) : dolc));

	for (;;) {
		if (interactive && e.iop <= iostack) {
#ifdef CONFIG_FEATURE_COMMAND_EDITING
			current_prompt=prompt->value;
#else
			prs(prompt->value);
#endif
		}
		onecommand();
		/* Ensure that getenv("PATH") stays current */
		setenv("PATH", path->value, 1);
	}
}

static void
setdash()
{
	register char *cp;
	register int c;
	char m['z'-'a'+1];

	cp = m;
	for (c='a'; c<='z'; c++)
		if (flag[(int)c])
			*cp++ = c;
	*cp = 0;
	setval(lookup("-"), m);
}

static int
newfile(s)
register char *s;
{
	register int f;

	if (strcmp(s, "-") != 0) {
		f = open(s, 0);
		if (f < 0) {
			prs(s);
			err(": cannot open");
			return(1);
		}
	} else
		f = 0;
	next(remap(f));
	return(0);
}

static void
onecommand()
{
	register int i;
	jmp_buf m1;

	while (e.oenv)
		quitenv();
	areanum = 1;
	freehere(areanum);
	freearea(areanum);
	garbage();
	wdlist = 0;
	iolist = 0;
	e.errpt = 0;
	e.linep = line;
	yynerrs = 0;
	multiline = 0;
	inparse = 1;
	intr = 0;
	execflg = 0;
	setjmp(failpt = m1);	/* Bruce Evans' fix */
	if (setjmp(failpt = m1) || yyparse() || intr) {
		while (e.oenv)
			quitenv();
		scraphere();
		if (!interactive && intr)
			leave();
		inparse = 0;
		intr = 0;
		return;
	}
	inparse = 0;
	brklist = 0;
	intr = 0;
	execflg = 0;
	if (!flag['n'])
		execute(outtree, NOPIPE, NOPIPE, 0);
	if (!interactive && intr) {
		execflg = 0;
		leave();
	}
	if ((i = trapset) != 0) {
		trapset = 0;
		runtrap(i);
	}
}

static void
fail()
{
	longjmp(failpt, 1);
	/* NOTREACHED */
}

static void
leave()
{
	if (execflg)
		fail();
	scraphere();
	freehere(1);
	runtrap(0);
	exit(exstat);
	/* NOTREACHED */
}

static void
warn(s)
register char *s;
{
	if(*s) {
		prs(s);
		exstat = -1;
	}
	prs("\n");
	if (flag['e'])
		leave();
}

static void
err(s)
char *s;
{
	warn(s);
	if (flag['n'])
		return;
	if (!interactive)
		leave();
	if (e.errpt)
		longjmp(e.errpt, 1);
	closeall();
	e.iop = e.iobase = iostack;
}

static int
newenv(f)
int f;
{
	register struct env *ep;

	if (f) {
		quitenv();
		return(1);
	}
	ep = (struct env *) space(sizeof(*ep));
	if (ep == NULL) {
		while (e.oenv)
			quitenv();
		fail();
	}
	*ep = e;
	e.oenv = ep;
	e.errpt = errpt;
	return(0);
}

static void
quitenv()
{
	register struct env *ep;
	register int fd;

	if ((ep = e.oenv) != NULL) {
		fd = e.iofd;
		e = *ep;
		/* should close `'d files */
		DELETE(ep);
		while (--fd >= e.iofd)
			close(fd);
	}
}

/*
 * Is any character from s1 in s2?
 */
static int
anys(s1, s2)
register char *s1, *s2;
{
	while (*s1)
		if (any(*s1++, s2))
			return(1);
	return(0);
}

/*
 * Is character c in s?
 */
static int
any(c, s)
register int c;
register char *s;
{
	while (*s)
		if (*s++ == c)
			return(1);
	return(0);
}

static char *
putn(n)
register int n;
{
	return(itoa(n));
}

static char *
itoa(n)
register int n;
{
	static char s[20];
	snprintf(s, sizeof(s), "%u", n);
	return(s);
}

static void
next(int f)
{
	PUSHIO(afile, f, filechar);
}

static void
onintr(s)
int s;				/* ANSI C requires a parameter */
{
	signal(SIGINT, onintr);
	intr = 1;
	if (interactive) {
		if (inparse) {
			prs("\n");
			fail();
		}
	}
	else if (heedint) {
		execflg = 0;
		leave();
	}
}

static char *
space(n)
int n;
{
	register char *cp;

	if ((cp = getcell(n)) == 0)
		err("out of string space");
	return(cp);
}

static char *
strsave(s, a)
register char *s;
int a;
{
	register char *cp, *xp;

	if ((cp = space(strlen(s)+1)) != NULL) {
		setarea((char *)cp, a);
		for (xp = cp; (*xp++ = *s++) != '\0';)
			;
		return(cp);
	}
	return("");
}

/*
 * trap handling
 */
static void
sig(i)
register int i;
{
	trapset = i;
	signal(i, sig);
}

static void runtrap(i)
int i;
{
	char *trapstr;

	if ((trapstr = trap[i]) == NULL)
		return;
	if (i == 0)
		trap[i] = 0;
	RUN(aword, trapstr, nlchar);
}

/* -------- var.c -------- */

/*
 * Find the given name in the dictionary
 * and return its value.  If the name was
 * not previously there, enter it now and
 * return a null value.
 */
static struct var *
lookup(n)
register char *n;
{
	register struct var *vp;
	register char *cp;
	register int c;
	static struct var dummy;

	if (isdigit(*n)) {
		dummy.name = n;
		for (c = 0; isdigit(*n) && c < 1000; n++)
			c = c*10 + *n-'0';
		dummy.status = RONLY;
		dummy.value = c <= dolc? dolv[c]: null;
		return(&dummy);
	}
	for (vp = vlist; vp; vp = vp->next)
		if (eqname(vp->name, n))
			return(vp);
	cp = findeq(n);
	vp = (struct var *)space(sizeof(*vp));
	if (vp == 0 || (vp->name = space((int)(cp-n)+2)) == 0) {
		dummy.name = dummy.value = "";
		return(&dummy);
	}
	for (cp = vp->name; (*cp = *n++) && *cp != '='; cp++)
		;
	if (*cp == 0)
		*cp = '=';
	*++cp = 0;
	setarea((char *)vp, 0);
	setarea((char *)vp->name, 0);
	vp->value = null;
	vp->next = vlist;
	vp->status = GETCELL;
	vlist = vp;
	return(vp);
}

/*
 * give variable at `vp' the value `val'.
 */
static void
setval(vp, val)
struct var *vp;
char *val;
{
	nameval(vp, val, (char *)NULL);
}

/*
 * if name is not NULL, it must be
 * a prefix of the space `val',
 * and end with `='.
 * this is all so that exporting
 * values is reasonably painless.
 */
static void
nameval(vp, val, name)
register struct var *vp;
char *val, *name;
{
	register char *cp, *xp;
	char *nv;
	int fl;

	if (vp->status & RONLY) {
		for (xp = vp->name; *xp && *xp != '=';)
			putc(*xp++, stderr);
		err(" is read-only");
		return;
	}
	fl = 0;
	if (name == NULL) {
		xp = space(strlen(vp->name)+strlen(val)+2);
		if (xp == 0)
			return;
		/* make string:  name=value */
		setarea((char *)xp, 0);
		name = xp;
		for (cp = vp->name; (*xp = *cp++) && *xp!='='; xp++)
			;
		if (*xp++ == 0)
			xp[-1] = '=';
		nv = xp;
		for (cp = val; (*xp++ = *cp++) != '\0';)
			;
		val = nv;
		fl = GETCELL;
	}
	if (vp->status & GETCELL)
		freecell(vp->name);    /* form new string `name=value' */
	vp->name = name;
	vp->value = val;
	vp->status |= fl;
}

static void
export(vp)
struct var *vp;
{
	vp->status |= EXPORT;
}

static void
ronly(vp)
struct var *vp;
{
	if (isalpha(vp->name[0]) || vp->name[0] == '_')	/* not an internal symbol */
		vp->status |= RONLY;
}

static int
isassign(s)
register char *s;
{
	if (!isalpha((int)*s) && *s != '_')
		return(0);
	for (; *s != '='; s++)
		if (*s == 0 || (!isalnum(*s) && *s != '_'))
			return(0);
	return(1);
}

static int
assign(s, cf)
register char *s;
int cf;
{
	register char *cp;
	struct var *vp;

	if (!isalpha(*s) && *s != '_')
		return(0);
	for (cp = s; *cp != '='; cp++)
		if (*cp == 0 || (!isalnum(*cp) && *cp != '_'))
			return(0);
	vp = lookup(s);
	nameval(vp, ++cp, cf == COPYV? (char *)NULL: s);
	if (cf != COPYV)
		vp->status &= ~GETCELL;
	return(1);
}

static int
checkname(cp)
register char *cp;
{
	if (!isalpha(*cp++) && *(cp-1) != '_')
		return(0);
	while (*cp)
		if (!isalnum(*cp++) && *(cp-1) != '_')
			return(0);
	return(1);
}

static void
putvlist(f, out)
register int f, out;
{
	register struct var *vp;

	for (vp = vlist; vp; vp = vp->next)
		if (vp->status & f && (isalpha(*vp->name) || *vp->name == '_')) {
			if (vp->status & EXPORT)
				write(out, "export ", 7);
			if (vp->status & RONLY)
				write(out, "readonly ", 9);
			write(out, vp->name, (int)(findeq(vp->name) - vp->name));
			write(out, "\n", 1);
		}
}

static int
eqname(n1, n2)
register char *n1, *n2;
{
	for (; *n1 != '=' && *n1 != 0; n1++)
		if (*n2++ != *n1)
			return(0);
	return(*n2 == 0 || *n2 == '=');
}

static char *
findeq(cp)
register char *cp;
{
	while (*cp != '\0' && *cp != '=')
		cp++;
	return(cp);
}

/* -------- gmatch.c -------- */
/*
 * int gmatch(string, pattern)
 * char *string, *pattern;
 *
 * Match a pattern as in sh(1).
 */

#define	CMASK	0377
#define	QUOTE	0200
#define	QMASK	(CMASK&~QUOTE)
#define	NOT	'!'	/* might use ^ */

static int
gmatch(s, p)
register char *s, *p;
{
	register int sc, pc;

	if (s == NULL || p == NULL)
		return(0);
	while ((pc = *p++ & CMASK) != '\0') {
		sc = *s++ & QMASK;
		switch (pc) {
		case '[':
			if ((p = cclass(p, sc)) == NULL)
				return(0);
			break;

		case '?':
			if (sc == 0)
				return(0);
			break;

		case '*':
			s--;
			do {
				if (*p == '\0' || gmatch(s, p))
					return(1);
			} while (*s++ != '\0');
			return(0);

		default:
			if (sc != (pc&~QUOTE))
				return(0);
		}
	}
	return(*s == 0);
}

static char *
cclass(p, sub)
register char *p;
register int sub;
{
	register int c, d, not, found;

	if ((not = *p == NOT) != 0)
		p++;
	found = not;
	do {
		if (*p == '\0')
			return((char *)NULL);
		c = *p & CMASK;
		if (p[1] == '-' && p[2] != ']') {
			d = p[2] & CMASK;
			p++;
		} else
			d = c;
		if (c == sub || (c <= sub && sub <= d))
			found = !not;
	} while (*++p != ']');
	return(found? p+1: (char *)NULL);
}


/* -------- area.c -------- */

/*
 * All memory between (char *)areabot and (char *)(areatop+1) is
 * exclusively administered by the area management routines.
 * It is assumed that sbrk() and brk() manipulate the high end.
 */

#define sbrk(X) ({ void * __q = (void *)-1; if (brkaddr + (int)(X) < brktop) { __q = brkaddr; brkaddr+=(int)(X); } __q;})

static void
initarea()
{
	brkaddr = malloc(65000);
	brktop = brkaddr + 65000;

	while ((int)sbrk(0) & ALIGN)
		sbrk(1);
	areabot = (struct region *)sbrk(REGSIZE);

	areabot->next = areabot;
	areabot->area = BUSY;
	areatop = areabot;
	areanxt = areabot;
}

char *
getcell(nbytes)
unsigned nbytes;
{
	register int nregio;
	register struct region *p, *q;
	register int i;

	if (nbytes == 0) {
		puts("getcell(0)");
		abort();
	}	/* silly and defeats the algorithm */
	/*
	 * round upwards and add administration area
	 */
	nregio = (nbytes+(REGSIZE-1))/REGSIZE + 1;
	for (p = areanxt;;) {
		if (p->area > areanum) {
			/*
			 * merge free cells
			 */
			while ((q = p->next)->area > areanum && q != areanxt)
				p->next = q->next;
			/*
			 * exit loop if cell big enough
			 */
			if (q >= p + nregio)
				goto found;
		}
		p = p->next;
		if (p == areanxt)
			break;
	}
	i = nregio >= GROWBY ? nregio : GROWBY;
	p = (struct region *)sbrk(i * REGSIZE);
	if (p == (struct region *)-1)
		return((char *)NULL);
	p--;
	if (p != areatop) {
		puts("not contig");
		abort();	/* allocated areas are contiguous */
	}
	q = p + i;
	p->next = q;
	p->area = FREE;
	q->next = areabot;
	q->area = BUSY;
	areatop = q;
found:
	/*
	 * we found a FREE area big enough, pointed to by 'p', and up to 'q'
	 */
	areanxt = p + nregio;
	if (areanxt < q) {
		/*
		 * split into requested area and rest
		 */
		if (areanxt+1 > q) {
			puts("OOM");
			abort();	/* insufficient space left for admin */
		}
		areanxt->next = q;
		areanxt->area = FREE;
		p->next = areanxt;
	}
	p->area = areanum;
	return((char *)(p+1));
}

static void
freecell(cp)
char *cp;
{
	register struct region *p;

	if ((p = (struct region *)cp) != NULL) {
		p--;
		if (p < areanxt)
			areanxt = p;
		p->area = FREE;
	}
}

static void
freearea(a)
register int a;
{
	register struct region *p, *top;

	top = areatop;
	for (p = areabot; p != top; p = p->next)
		if (p->area >= a)
			p->area = FREE;
}

static void
setarea(cp,a)
char *cp;
int a;
{
	register struct region *p;

	if ((p = (struct region *)cp) != NULL)
		(p-1)->area = a;
}

int
getarea(cp)
char *cp;
{
	return ((struct region*)cp-1)->area;
}

static void
garbage()
{
	register struct region *p, *q, *top;

	top = areatop;
	for (p = areabot; p != top; p = p->next) {
		if (p->area > areanum) {
			while ((q = p->next)->area > areanum)
				p->next = q->next;
			areanxt = p;
		}
	}
#ifdef SHRINKBY
	if (areatop >= q + SHRINKBY && q->area > areanum) {
		brk((char *)(q+1));
		q->next = areabot;
		q->area = BUSY;
		areatop = q;
	}
#endif
}

/* -------- csyn.c -------- */
/*
 * shell: syntax (C version)
 */


int
yyparse()
{
	startl  = 1;
	peeksym = 0;
	yynerrs = 0;
	outtree = c_list();
	musthave('\n', 0);
	return(yynerrs!=0);
}

static struct op *
pipeline(cf)
int cf;
{
	register struct op *t, *p;
	register int c;

	t = command(cf);
	if (t != NULL) {
		while ((c = yylex(0)) == '|') {
			if ((p = command(CONTIN)) == NULL)
				SYNTAXERR;
			if (t->type != TPAREN && t->type != TCOM) {
				/* shell statement */
				t = block(TPAREN, t, NOBLOCK, NOWORDS);
			}
			t = block(TPIPE, t, p, NOWORDS);
		}
		peeksym = c;
	}
	return(t);
}

static struct op *
andor()
{
	register struct op *t, *p;
	register int c;

	t = pipeline(0);
	if (t != NULL) {
		while ((c = yylex(0)) == LOGAND || c == LOGOR) {
			if ((p = pipeline(CONTIN)) == NULL)
				SYNTAXERR;
			t = block(c == LOGAND? TAND: TOR, t, p, NOWORDS);
		}
		peeksym = c;
	}
	return(t);
}

static struct op *
c_list()
{
	register struct op *t, *p;
	register int c;

	t = andor();
	if (t != NULL) {
		if((peeksym = yylex(0)) == '&')
			t = block(TASYNC, t, NOBLOCK, NOWORDS);
		while ((c = yylex(0)) == ';' || c == '&' || (multiline && c == '\n')) {
			if ((p = andor()) == NULL)
				return(t);
			if((peeksym = yylex(0)) == '&')
				p = block(TASYNC, p, NOBLOCK, NOWORDS);
			t = list(t, p);
		}
		peeksym = c;
	}
	return(t);
}


static int
synio(cf)
int cf;
{
	register struct ioword *iop;
	register int i;
	register int c;

	if ((c = yylex(cf)) != '<' && c != '>') {
		peeksym = c;
		return(0);
	}
	i = yylval.i;
	musthave(WORD, 0);
	iop = io(iounit, i, yylval.cp);
	iounit = IODEFAULT;
	if (i & IOHERE)
		markhere(yylval.cp, iop);
	return(1);
}

static void
musthave(c, cf)
int c, cf;
{
	if ((peeksym = yylex(cf)) != c)
		SYNTAXERR;
	peeksym = 0;
}

static struct op *
simple()
{
	register struct op *t;

	t = NULL;
	for (;;) {
		switch (peeksym = yylex(0)) {
		case '<':
		case '>':
			(void) synio(0);
			break;

		case WORD:
			if (t == NULL) {
				t = newtp();
				t->type = TCOM;
			}
			peeksym = 0;
			word(yylval.cp);
			break;

		default:
			return(t);
		}
	}
}

static struct op *
nested(type, mark)
int type, mark;
{
	register struct op *t;

	multiline++;
	t = c_list();
	musthave(mark, 0);
	multiline--;
	return(block(type, t, NOBLOCK, NOWORDS));
}

static struct op *
command(cf)
int cf;
{
	register struct op *t;
	struct wdblock *iosave;
	register int c;

	iosave = iolist;
	iolist = NULL;
	if (multiline)
		cf |= CONTIN;
	while (synio(cf))
		cf = 0;
	switch (c = yylex(cf)) {
	default:
		peeksym = c;
		if ((t = simple()) == NULL) {
			if (iolist == NULL)
				return((struct op *)NULL);
			t = newtp();
			t->type = TCOM;
		}
		break;

	case '(':
		t = nested(TPAREN, ')');
		break;

	case '{':
		t = nested(TBRACE, '}');
		break;

	case FOR:
		t = newtp();
		t->type = TFOR;
		musthave(WORD, 0);
		startl = 1;
		t->str = yylval.cp;
		multiline++;
		t->words = wordlist();
		if ((c = yylex(0)) != '\n' && c != ';')
			peeksym = c;
		t->left = dogroup(0);
		multiline--;
		break;

	case WHILE:
	case UNTIL:
		multiline++;
		t = newtp();
		t->type = c == WHILE? TWHILE: TUNTIL;
		t->left = c_list();
		t->right = dogroup(1);
		t->words = NULL;
		multiline--;
		break;

	case CASE:
		t = newtp();
		t->type = TCASE;
		musthave(WORD, 0);
		t->str = yylval.cp;
		startl++;
		multiline++;
		musthave(IN, CONTIN);
		startl++;
		t->left = caselist();
		musthave(ESAC, 0);
		multiline--;
		break;

	case IF:
		multiline++;
		t = newtp();
		t->type = TIF;
		t->left = c_list();
		t->right = thenpart();
		musthave(FI, 0);
		multiline--;
		break;
	}
	while (synio(0))
		;
	t = namelist(t);
	iolist = iosave;
	return(t);
}

static struct op *
dogroup(onlydone)
int onlydone;
{
	register int c;
	register struct op *mylist;

	c = yylex(CONTIN);
	if (c == DONE && onlydone)
		return((struct op *)NULL);
	if (c != DO)
		SYNTAXERR;
	mylist = c_list();
	musthave(DONE, 0);
	return(mylist);
}

static struct op *
thenpart()
{
	register int c;
	register struct op *t;

	if ((c = yylex(0)) != THEN) {
		peeksym = c;
		return((struct op *)NULL);
	}
	t = newtp();
	t->type = 0;
	t->left = c_list();
	if (t->left == NULL)
		SYNTAXERR;
	t->right = elsepart();
	return(t);
}

static struct op *
elsepart()
{
	register int c;
	register struct op *t;

	switch (c = yylex(0)) {
	case ELSE:
		if ((t = c_list()) == NULL)
			SYNTAXERR;
		return(t);

	case ELIF:
		t = newtp();
		t->type = TELIF;
		t->left = c_list();
		t->right = thenpart();
		return(t);

	default:
		peeksym = c;
		return((struct op *)NULL);
	}
}

static struct op *
caselist()
{
	register struct op *t;

	t = NULL;
	while ((peeksym = yylex(CONTIN)) != ESAC)
		t = list(t, casepart());
	return(t);
}

static struct op *
casepart()
{
	register struct op *t;

	t = newtp();
	t->type = TPAT;
	t->words = pattern();
	musthave(')', 0);
	t->left = c_list();
	if ((peeksym = yylex(CONTIN)) != ESAC)
		musthave(BREAK, CONTIN);
	return(t);
}

static char **
pattern()
{
	register int c, cf;

	cf = CONTIN;
	do {
		musthave(WORD, cf);
		word(yylval.cp);
		cf = 0;
	} while ((c = yylex(0)) == '|');
	peeksym = c;
	word(NOWORD);
	return(copyw());
}

static char **
wordlist()
{
	register int c;

	if ((c = yylex(0)) != IN) {
		peeksym = c;
		return((char **)NULL);
	}
	startl = 0;
	while ((c = yylex(0)) == WORD)
		word(yylval.cp);
	word(NOWORD);
	peeksym = c;
	return(copyw());
}

/*
 * supporting functions
 */
static struct op *
list(t1, t2)
register struct op *t1, *t2;
{
	if (t1 == NULL)
		return(t2);
	if (t2 == NULL)
		return(t1);
	return(block(TLIST, t1, t2, NOWORDS));
}

static struct op *
block(type, t1, t2, wp)
int type;
struct op *t1, *t2;
char **wp;
{
	register struct op *t;

	t = newtp();
	t->type = type;
	t->left = t1;
	t->right = t2;
	t->words = wp;
	return(t);
}

static int
rlookup(n)
register char *n;
{
	register struct res *rp;

	for (rp = restab; rp->r_name; rp++)
		if (strcmp(rp->r_name, n) == 0)
			return(rp->r_val);
	return(0);
}

static struct op *
newtp()
{
	register struct op *t;

	t = (struct op *)tree(sizeof(*t));
	t->type = 0;
	t->words = NULL;
	t->ioact = NULL;
	t->left = NULL;
	t->right = NULL;
	t->str = NULL;
	return(t);
}

static struct op *
namelist(t)
register struct op *t;
{
	if (iolist) {
		iolist = addword((char *)NULL, iolist);
		t->ioact = copyio();
	} else
		t->ioact = NULL;
	if (t->type != TCOM) {
		if (t->type != TPAREN && t->ioact != NULL) {
			t = block(TPAREN, t, NOBLOCK, NOWORDS);
			t->ioact = t->left->ioact;
			t->left->ioact = NULL;
		}
		return(t);
	}
	word(NOWORD);
	t->words = copyw();
	return(t);
}

static char **
copyw()
{
	register char **wd;

	wd = getwords(wdlist);
	wdlist = 0;
	return(wd);
}

static void
word(cp)
char *cp;
{
	wdlist = addword(cp, wdlist);
}

static struct ioword **
copyio()
{
	register struct ioword **iop;

	iop = (struct ioword **) getwords(iolist);
	iolist = 0;
	return(iop);
}

static struct ioword *
io(u, f, cp)
int u;
int f;
char *cp;
{
	register struct ioword *iop;

	iop = (struct ioword *) tree(sizeof(*iop));
	iop->io_unit = u;
	iop->io_flag = f;
	iop->io_name = cp;
	iolist = addword((char *)iop, iolist);
	return(iop);
}

static void
zzerr()
{
	yyerror("syntax error");
}

static void
yyerror(s)
char *s;
{
	yynerrs++;
	if (interactive && e.iop <= iostack) {
		multiline = 0;
		while (eofc() == 0 && yylex(0) != '\n')
			;
	}
	err(s);
	fail();
}

static int
yylex(cf)
int cf;
{
	register int c, c1;
	int atstart;

	if ((c = peeksym) > 0) {
		peeksym = 0;
		if (c == '\n')
			startl = 1;
		return(c);
	}
	nlseen = 0;
	e.linep = line;
	atstart = startl;
	startl = 0;
	yylval.i = 0;

loop:
	while ((c = my_getc(0)) == ' ' || c == '\t')
		;
	switch (c) {
	default:
		if (any(c, "0123456789")) {
			unget(c1 = my_getc(0));
			if (c1 == '<' || c1 == '>') {
				iounit = c - '0';
				goto loop;
			}
			*e.linep++ = c;
			c = c1;
		}
		break;

	case '#':
		while ((c = my_getc(0)) != 0 && c != '\n')
			;
		unget(c);
		goto loop;

	case 0:
		return(c);

	case '$':
		*e.linep++ = c;
		if ((c = my_getc(0)) == '{') {
			if ((c = collect(c, '}')) != '\0')
				return(c);
			goto pack;
		}
		break;

	case '`':
	case '\'':
	case '"':
		if ((c = collect(c, c)) != '\0')
			return(c);
		goto pack;

	case '|':
	case '&':
	case ';':
		if ((c1 = dual(c)) != '\0') {
			startl = 1;
			return(c1);
		}
		startl = 1;
		return(c);
	case '^':
		startl = 1;
		return('|');
	case '>':
	case '<':
		diag(c);
		return(c);

	case '\n':
		nlseen++;
		gethere();
		startl = 1;
		if (multiline || cf & CONTIN) {
			if (interactive && e.iop <= iostack) {
#ifdef CONFIG_FEATURE_COMMAND_EDITING
			current_prompt=cprompt->value;
#else
			prs(cprompt->value);
#endif
			}
			if (cf & CONTIN)
				goto loop;
		}
		return(c);

	case '(':
	case ')':
		startl = 1;
		return(c);
	}

	unget(c);

pack:
	while ((c = my_getc(0)) != 0 && !any(c, "`$ '\"\t;&<>()|^\n"))
		if (e.linep >= elinep)
			err("word too long");
		else
			*e.linep++ = c;
	unget(c);
	if(any(c, "\"'`$"))
		goto loop;
	*e.linep++ = '\0';
	if (atstart && (c = rlookup(line))!=0) {
		startl = 1;
		return(c);
	}
	yylval.cp = strsave(line, areanum);
	return(WORD);
}

static int
collect(c, c1)
register int c, c1;
{
	char s[2];

	*e.linep++ = c;
	while ((c = my_getc(c1)) != c1) {
		if (c == 0) {
			unget(c);
			s[0] = c1;
			s[1] = 0;
			prs("no closing "); yyerror(s);
			return(YYERRCODE);
		}
		if (interactive && c == '\n' && e.iop <= iostack) {
#ifdef CONFIG_FEATURE_COMMAND_EDITING
		    current_prompt=cprompt->value;
#else
		    prs(cprompt->value);
#endif
		}
		*e.linep++ = c;
	}
	*e.linep++ = c;
	return(0);
}

static int
dual(c)
register int c;
{
	char s[3];
	register char *cp = s;

	*cp++ = c;
	*cp++ = my_getc(0);
	*cp = 0;
	if ((c = rlookup(s)) == 0)
		unget(*--cp);
	return(c);
}

static void
diag(ec)
register int ec;
{
	register int c;

	c = my_getc(0);
	if (c == '>' || c == '<') {
		if (c != ec)
			zzerr();
		yylval.i = ec == '>'? IOWRITE|IOCAT: IOHERE;
		c = my_getc(0);
	} else
		yylval.i = ec == '>'? IOWRITE: IOREAD;
	if (c != '&' || yylval.i == IOHERE)
		unget(c);
	else
		yylval.i |= IODUP;
}

static char *
tree(size)
unsigned size;
{
	register char *t;

	if ((t = getcell(size)) == NULL) {
		prs("command line too complicated\n");
		fail();
		/* NOTREACHED */
	}
	return(t);
}

/* VARARGS1 */
/* ARGSUSED */

/* -------- exec.c -------- */

/*
 * execute tree
 */


static int
execute(t, pin, pout, act)
register struct op *t;
int *pin, *pout;
int act;
{
	register struct op *t1;
	volatile int i, rv, a;
	char *cp, **wp, **wp2;
	struct var *vp;
	struct brkcon bc;

#if __GNUC__
	/* Avoid longjmp clobbering */
	(void) &wp;
#endif	


	if (t == NULL)
		return(0);
	rv = 0;
	a = areanum++;
	wp = (wp2 = t->words) != NULL
	     ? eval(wp2, t->type == TCOM ? DOALL : DOALL & ~DOKEY)
	     : NULL;

	switch(t->type) {
	case TPAREN:
		rv = execute(t->left, pin, pout, 0);
		break;
			
	case TCOM:
		{
			int child;
			rv = forkexec(t, pin, pout, act, wp, &child);
			if (child) {
				exstat = rv;
				leave();
			}
		}
		break;

	case TPIPE:
		{
		    int pv[2];
		    if ((rv = openpipe(pv)) < 0)
			break;
		    pv[0] = remap(pv[0]);
		    pv[1] = remap(pv[1]);
		    (void) execute(t->left, pin, pv, 0);
		    rv = execute(t->right, pv, pout, 0);
		}
		break;

	case TLIST:
		(void) execute(t->left, pin, pout, 0);
		rv = execute(t->right, pin, pout, 0);
		break;

	case TASYNC:
	{
		int hinteractive = interactive;

		i = vfork();
		if (i != 0) {
			interactive = hinteractive;
			if (i != -1) {
				setval(lookup("!"), putn(i));
				if (pin != NULL)
					closepipe(pin);
				if (interactive) {
					prs(putn(i));
					prs("\n");
				}
			} else
				rv = -1;
			setstatus(rv);
		} else {
			signal(SIGINT, SIG_IGN);
			signal(SIGQUIT, SIG_IGN);
			if (interactive)
				signal(SIGTERM, SIG_DFL);
			interactive = 0;
			if (pin == NULL) {
				close(0);
				open("/dev/null", 0);
			}
			exit(execute(t->left, pin, pout, FEXEC));
		}
	}
		break;

	case TOR:
	case TAND:
		rv = execute(t->left, pin, pout, 0);
		if ((t1 = t->right)!=NULL && (rv == 0) == (t->type == TAND))
			rv = execute(t1, pin, pout, 0);
		break;

	case TFOR:
		if (wp == NULL) {
			wp = dolv+1;
			if ((i = dolc) < 0)
				i = 0;
		} else {
			i = -1;
			while (*wp++ != NULL)
				;			
		}
		vp = lookup(t->str);
		while (setjmp(bc.brkpt))
			if (isbreak)
				goto broken;
		brkset(&bc);
		for (t1 = t->left; i-- && *wp != NULL;) {
			setval(vp, *wp++);
			rv = execute(t1, pin, pout, 0);
		}
		brklist = brklist->nextlev;
		break;

	case TWHILE:
	case TUNTIL:
		while (setjmp(bc.brkpt))
			if (isbreak)
				goto broken;
		brkset(&bc);
		t1 = t->left;
		while ((execute(t1, pin, pout, 0) == 0) == (t->type == TWHILE))
			rv = execute(t->right, pin, pout, 0);
		brklist = brklist->nextlev;
		break;

	case TIF:
	case TELIF:
	 	if (t->right != NULL) {
		rv = !execute(t->left, pin, pout, 0) ?
			execute(t->right->left, pin, pout, 0):
			execute(t->right->right, pin, pout, 0);
		}
		break;

	case TCASE:
		if ((cp = evalstr(t->str, DOSUB|DOTRIM)) == 0)
			cp = "";
		if ((t1 = findcase(t->left, cp)) != NULL)
			rv = execute(t1, pin, pout, 0);
		break;

	case TBRACE:
/*
		if (iopp = t->ioact)
			while (*iopp)
				if (iosetup(*iopp++, pin!=NULL, pout!=NULL)) {
					rv = -1;
					break;
				}
*/
		if (rv >= 0 && (t1 = t->left))
			rv = execute(t1, pin, pout, 0);
		break;
	}

broken:
	t->words = wp2;
	isbreak = 0;
	freehere(areanum);
	freearea(areanum);
	areanum = a;
	if (interactive && intr) {
		closeall();
		fail();
	}
	if ((i = trapset) != 0) {
		trapset = 0;
		runtrap(i);
	}
	return(rv);
}

static int
forkexec( register struct op *t, int *pin, int *pout, int act, char **wp, int *pforked)
{
	int i, rv;
	int (*shcom)(struct op *) = NULL;
	register int f;
	char *cp = NULL;
	struct ioword **iopp;
	int resetsig;
	char **owp;

	int *hpin = pin;
	int *hpout = pout;
	int hforked;
	char *hwp;
	int hinteractive;
	int hintr;
	struct brkcon * hbrklist;
	int hexecflg;

#if __GNUC__
	/* Avoid longjmp clobbering */
	(void) &pin;
	(void) &pout;
	(void) &wp;
	(void) &shcom;
	(void) &cp;
	(void) &resetsig;
	(void) &owp;
#endif	

	owp = wp;
	resetsig = 0;
	*pforked = 0;
	rv = -1;	/* system-detected error */
	if (t->type == TCOM) {
		while ((cp = *wp++) != NULL)
			;
		cp = *wp;

		/* strip all initial assignments */
		/* not correct wrt PATH=yyy command  etc */
		if (flag['x'])
			echo (cp ? wp: owp);
		if (cp == NULL && t->ioact == NULL) {
			while ((cp = *owp++) != NULL && assign(cp, COPYV))
				;
			return(setstatus(0));
		}
		else if (cp != NULL)
			shcom = inbuilt(cp);
	}
	t->words = wp;
	f = act;
	if (shcom == NULL && (f & FEXEC) == 0) {

		hpin = pin;
		hpout = pout;
		hforked = *pforked;
		hwp = *wp;
		hinteractive = interactive;
		hintr = intr;
		hbrklist = brklist;
		hexecflg = execflg;
	
		i = vfork();
		if (i != 0) {
                	/* who wrote this crappy non vfork safe shit? */
			pin = hpin;
			pout = hpout;
			*pforked = hforked;
			*wp = hwp;
			interactive = hinteractive;
			intr = hintr;
			brklist = hbrklist;
			execflg = hexecflg;

			*pforked = 0;
			if (i == -1)
				return(rv);
			if (pin != NULL)
				closepipe(pin);
			return(pout==NULL? setstatus(waitfor(i,0)): 0);
		}

		if (interactive) {
			signal(SIGINT, SIG_IGN);
			signal(SIGQUIT, SIG_IGN);
			resetsig = 1;
		}
		interactive = 0;
		intr = 0;
		(*pforked)++;
		brklist = 0;
		execflg = 0;
	}	
	if (owp != NULL)
		while ((cp = *owp++) != NULL && assign(cp, COPYV))
			if (shcom == NULL)
				export(lookup(cp));
#ifdef COMPIPE
	if ((pin != NULL || pout != NULL) && shcom != NULL && shcom != doexec) {
		err("piping to/from shell builtins not yet done");
		return(-1);
	}
#endif
	if (pin != NULL) {
		dup2(pin[0], 0);
		closepipe(pin);
	}
	if (pout != NULL) {
		dup2(pout[1], 1);
		closepipe(pout);
	}
	if ((iopp = t->ioact) != NULL) {
		if (shcom != NULL && shcom != doexec) {
			prs(cp);
			err(": cannot redirect shell command");
			return(-1);
		}
		while (*iopp)
			if (iosetup(*iopp++, pin!=NULL, pout!=NULL))
				return(rv);
	}
	if (shcom)
		return(setstatus((*shcom)(t)));
	/* should use FIOCEXCL */
	for (i=FDBASE; i<NOFILE; i++)
		close(i);
	if (resetsig) {
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
	}
	if (t->type == TPAREN)
		exit(execute(t->left, NOPIPE, NOPIPE, FEXEC));
	if (wp[0] == NULL)
		exit(0);

	cp = rexecve(wp[0], wp, makenv());
	prs(wp[0]); prs(": "); warn(cp);
	if (!execflg)
		trap[0] = NULL;
	leave();
	/* NOTREACHED */
	exit(1);
}

/*
 * 0< 1> are ignored as required
 * within pipelines.
 */
static int
iosetup(iop, pipein, pipeout)
register struct ioword *iop;
int pipein, pipeout;
{
	register int u = -1;
	char *cp=NULL, *msg;

	if (iop->io_unit == IODEFAULT)	/* take default */
		iop->io_unit = iop->io_flag&(IOREAD|IOHERE)? 0: 1;
	if (pipein && iop->io_unit == 0)
		return(0);
	if (pipeout && iop->io_unit == 1)
		return(0);
	msg = iop->io_flag&(IOREAD|IOHERE)? "open": "create";
	if ((iop->io_flag & IOHERE) == 0) {
		cp = iop->io_name;
		if ((cp = evalstr(cp, DOSUB|DOTRIM)) == NULL)
			return(1);
	}
	if (iop->io_flag & IODUP) {
		if (cp[1] || (!isdigit(*cp) && *cp != '-')) {
			prs(cp);
			err(": illegal >& argument");
			return(1);
		}
		if (*cp == '-')
			iop->io_flag = IOCLOSE;
		iop->io_flag &= ~(IOREAD|IOWRITE);
	}
	switch (iop->io_flag) {
	case IOREAD:
		u = open(cp, 0);
		break;

	case IOHERE:
	case IOHERE|IOXHERE:
		u = herein(iop->io_name, iop->io_flag&IOXHERE);
		cp = "here file";
		break;

	case IOWRITE|IOCAT:
		if ((u = open(cp, 1)) >= 0) {
			lseek(u, (long)0, 2);
			break;
		}
	case IOWRITE:
		u = creat(cp, 0666);
		break;

	case IODUP:
		u = dup2(*cp-'0', iop->io_unit);
		break;

	case IOCLOSE:
		close(iop->io_unit);
		return(0);
	}
	if (u < 0) {
		prs(cp);
		prs(": cannot ");
		warn(msg);
		return(1);
	} else {
		if (u != iop->io_unit) {
			dup2(u, iop->io_unit);
			close(u);
		}
	}
	return(0);
}

static void
echo(wp)
register char **wp;
{
	register int i;

	prs("+");
	for (i=0; wp[i]; i++) {
		if (i)
			prs(" ");
		prs(wp[i]);
	}
	prs("\n");
}

static struct op **
find1case(t, w)
struct op *t;
char *w;
{
	register struct op *t1;
	struct op **tp;
	register char **wp, *cp;

	if (t == NULL)
		return((struct op **)NULL);
	if (t->type == TLIST) {
		if ((tp = find1case(t->left, w)) != NULL)
			return(tp);
		t1 = t->right;	/* TPAT */
	} else
		t1 = t;
	for (wp = t1->words; *wp;)
		if ((cp = evalstr(*wp++, DOSUB)) && gmatch(w, cp))
			return(&t1->left);
	return((struct op **)NULL);
}

static struct op *
findcase(t, w)
struct op *t;
char *w;
{
	register struct op **tp;

	return((tp = find1case(t, w)) != NULL? *tp: (struct op *)NULL);
}

/*
 * Enter a new loop level (marked for break/continue).
 */
static void
brkset(bc)
struct brkcon *bc;
{
	bc->nextlev = brklist;
	brklist = bc;
}

/*
 * Wait for the last process created.
 * Print a message for each process found
 * that was killed by a signal.
 * Ignore interrupt signals while waiting
 * unless `canintr' is true.
 */
static int
waitfor(lastpid, canintr)
register int lastpid;
int canintr;
{
	register int pid, rv;
	int s;
	int oheedint = heedint;

	heedint = 0;
	rv = 0;
	do {
		pid = wait(&s);
		if (pid == -1) {
			if (errno != EINTR || canintr)
				break;
		} else {
			if ((rv = WAITSIG(s)) != 0) {
				if (rv < NSIGNAL) {
					if (signame[rv] != NULL) {
						if (pid != lastpid) {
							prn(pid);
							prs(": ");
						}
						prs(signame[rv]);
					}
				} else {
					if (pid != lastpid) {
						prn(pid);
						prs(": ");
					}
					prs("Signal "); prn(rv); prs(" ");
				}
				if (WAITCORE(s))
					prs(" - core dumped");
				if (rv >= NSIGNAL || signame[rv])
					prs("\n");
				rv = -1;
			} else
				rv = WAITVAL(s);
		}
	} while (pid != lastpid);
	heedint = oheedint;
	if (intr) {
		if (interactive) {
			if (canintr)
				intr = 0;
		} else {
			if (exstat == 0) exstat = rv;
			onintr(0);
		}
	}
	return(rv);
}

static int
setstatus(s)
register int s;
{
	exstat = s;
	setval(lookup("?"), putn(s));
	return(s);
}

/*
 * PATH-searching interface to execve.
 * If getenv("PATH") were kept up-to-date,
 * execvp might be used.
 */
static char *
rexecve(c, v, envp)
char *c, **v, **envp;
{
	register int i;
	register char *sp, *tp;
	int eacces = 0, asis = 0;

#ifdef CONFIG_FEATURE_SH_STANDALONE_SHELL
	char *name = c;
#ifdef CONFIG_FEATURE_SH_APPLETS_ALWAYS_WIN
	name = bb_get_last_path_component(name);
#endif
	optind = 1;
	if (find_applet_by_name(name)) {
		/* We have to exec here since we vforked.  Running 
		 * run_applet_by_name() won't work and bad things
		 * will happen. */
		execve("/proc/self/exe", v, envp);
		execve("busybox", v, envp);
	}
#endif

	sp = any('/', c)? "": path->value;
	asis = *sp == '\0';
	while (asis || *sp != '\0') {
		asis = 0;
		tp = e.linep;
		for (; *sp != '\0'; tp++)
			if ((*tp = *sp++) == ':') {
				asis = *sp == '\0';
				break;
			}
		if (tp != e.linep)
			*tp++ = '/';
		for (i = 0; (*tp++ = c[i++]) != '\0';)
			;

		execve(e.linep, v, envp);
		switch (errno) {
		case ENOEXEC:
			*v = e.linep;
			tp = *--v;
			*v = e.linep;
			execve(DEFAULT_SHELL, v, envp);
			*v = tp;
			return("no Shell");

		case ENOMEM:
			return((char*)bb_msg_memory_exhausted);

		case E2BIG:
			return("argument list too long");

		case EACCES:
			eacces++;
			break;
		}
	}
	return(errno==ENOENT ? "not found" : "cannot execute");
}

/*
 * Run the command produced by generator `f'
 * applied to stream `arg'.
 */
static int
run(struct ioarg *argp, int (*f)(struct ioarg *))
{
	struct op *otree;
	struct wdblock *swdlist;
	struct wdblock *siolist;
	jmp_buf ev, rt;
	xint *ofail;
	int rv;

#if __GNUC__
	/* Avoid longjmp clobbering */
	(void) &rv;
#endif

	areanum++;
	swdlist = wdlist;
	siolist = iolist;
	otree = outtree;
	ofail = failpt;
	rv = -1;
	if (newenv(setjmp(errpt = ev)) == 0) {
		wdlist = 0;
		iolist = 0;
		pushio(argp, f);
		e.iobase = e.iop;
		yynerrs = 0;
		if (setjmp(failpt = rt) == 0 && yyparse() == 0)
			rv = execute(outtree, NOPIPE, NOPIPE, 0);
		quitenv();
	}
	wdlist = swdlist;
	iolist = siolist;
	failpt = ofail;
	outtree = otree;
	freearea(areanum--);
	return(rv);
}

/* -------- do.c -------- */

/*
 * built-in commands: doX
 */

static int dohelp(struct op *t )
{
	int col;
	const struct builtincmd *x;

	printf("\nBuilt-in commands:\n");
	printf("-------------------\n");

	for (col=0, x = builtincmds; x->builtinfunc != NULL; x++) {
		if (!x->name)
			continue;
		col += printf("%s%s", ((col == 0) ? "\t" : " "), x->name);
		if (col > 60) {
			printf("\n");
			col = 0;
		}
	}
#ifdef CONFIG_FEATURE_SH_STANDALONE_SHELL
	{
		int i;
		const struct BB_applet *applet;
		extern const struct BB_applet applets[];
		extern const size_t NUM_APPLETS;

		for (i=0, applet = applets; i < NUM_APPLETS; applet++, i++) {
			if (!applet->name)
				continue;
		
			col += printf("%s%s", ((col == 0) ? "\t" : " "), 
					applet->name);
			if (col > 60) {
				printf("\n");
				col = 0;
			}
		}
	}
#endif
	printf("\n\n");
	return EXIT_SUCCESS;
}



static int dolabel(struct op *t )
{
	return(0);
}

static int
dochdir(t)
register struct op *t;
{
	register char *cp, *er;

	if ((cp = t->words[1]) == NULL && (cp = homedir->value) == NULL)
		er = ": no home directory";
	else if(chdir(cp) < 0)
		er = ": bad directory";
	else
		return(0);
	prs(cp != NULL? cp: "cd");
	err(er);
	return(1);
}

static int
doshift(t)
register struct op *t;
{
	register int n;

	n = t->words[1]? getn(t->words[1]): 1;
	if(dolc < n) {
		err("nothing to shift");
		return(1);
	}
	dolv[n] = dolv[0];
	dolv += n;
	dolc -= n;
	setval(lookup("#"), putn(dolc));
	return(0);
}

/*
 * execute login and newgrp directly
 */
static int
dologin(t)
struct op *t;
{
	register char *cp;

	if (interactive) {
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
	}
	cp = rexecve(t->words[0], t->words, makenv());
	prs(t->words[0]); prs(": "); err(cp);
	return(1);
}

static int
doumask(t)
register struct op *t;
{
	register int i, n;
	register char *cp;

	if ((cp = t->words[1]) == NULL) {
		i = umask(0);
		umask(i);
		for (n=3*4; (n-=3) >= 0;)
			putc('0'+((i>>n)&07), stderr);
		putc('\n', stderr);
	} else {
		for (n=0; *cp>='0' && *cp<='9'; cp++)
			n = n*8 + (*cp-'0');
		umask(n);
	}
	return(0);
}

static int
doexec(t)
register struct op *t;
{
	register int i;
	jmp_buf ex;
	xint *ofail;

	t->ioact = NULL;
	for(i = 0; (t->words[i]=t->words[i+1]) != NULL; i++)
		;
	if (i == 0)
		return(1);
	execflg = 1;
	ofail = failpt;
	if (setjmp(failpt = ex) == 0)
		execute(t, NOPIPE, NOPIPE, FEXEC);
	failpt = ofail;
	execflg = 0;
	return(1);
}

static int
dodot(t)
struct op *t;
{
	register int i;
	register char *sp, *tp;
	char *cp;

	if ((cp = t->words[1]) == NULL)
		return(0);
	sp = any('/', cp)? ":": path->value;
	while (*sp) {
		tp = e.linep;
		while (*sp && (*tp = *sp++) != ':')
			tp++;
		if (tp != e.linep)
			*tp++ = '/';
		for (i = 0; (*tp++ = cp[i++]) != '\0';)
			;
		if ((i = open(e.linep, 0)) >= 0) {
			exstat = 0;
			next(remap(i));
			return(exstat);
		}
	}
	prs(cp);
	err(": not found");
	return(-1);
}

static int
dowait(t)
struct op *t;
{
	register int i;
	register char *cp;

	if ((cp = t->words[1]) != NULL) {
		i = getn(cp);
		if (i == 0)
			return(0);
	} else
		i = -1;
	setstatus(waitfor(i, 1));
	return(0);
}

static int
doread(t)
struct op *t;
{
	register char *cp, **wp;
	register int nb = 0;
	register int  nl = 0;

	if (t->words[1] == NULL) {
		err("Usage: read name ...");
		return(1);
	}
	for (wp = t->words+1; *wp; wp++) {
		for (cp = e.linep; !nl && cp < elinep-1; cp++)
			if ((nb = read(0, cp, sizeof(*cp))) != sizeof(*cp) ||
			    (nl = (*cp == '\n')) ||
			    (wp[1] && any(*cp, ifs->value)))
				break;
		*cp = 0;
		if (nb <= 0)
			break;
		setval(lookup(*wp), e.linep);
	}
	return(nb <= 0);
}

static int
doeval(t)
register struct op *t;
{
	return(RUN(awordlist, t->words+1, wdchar));
}

static int
dotrap(t)
register struct op *t;
{
	register int  n, i;
	register int  resetsig;

	if (t->words[1] == NULL) {
		for (i=0; i<=_NSIG; i++)
			if (trap[i]) {
				prn(i);
				prs(": ");
				prs(trap[i]);
				prs("\n");
			}
		return(0);
	}
	resetsig = isdigit(*t->words[1]);
	for (i = resetsig ? 1 : 2; t->words[i] != NULL; ++i) {
		n = getsig(t->words[i]);
		freecell(trap[n]);
		trap[n] = 0;
		if (!resetsig) {
			if (*t->words[1] != '\0') {
				trap[n] = strsave(t->words[1], 0);
				setsig(n, sig);
			} else
				setsig(n, SIG_IGN);
		} else {
			if (interactive)
				if (n == SIGINT)
					setsig(n, onintr);
				else
					setsig(n, n == SIGQUIT ? SIG_IGN 
							       : SIG_DFL);
			else
				setsig(n, SIG_DFL);
		}
	}
	return(0);
}

static int
getsig(s)
char *s;
{
	register int n;

	if ((n = getn(s)) < 0 || n > _NSIG) {
		err("trap: bad signal number");
		n = 0;
	}
	return(n);
}

static void
setsig( register int n, sighandler_t f)
{
	if (n == 0)
		return;
	if (signal(n, SIG_IGN) != SIG_IGN || ourtrap[n]) {
		ourtrap[n] = 1;
		signal(n, f);
	}
}

static int
getn(as)
char *as;
{
	register char *s;
	register int n, m;

	s = as;
	m = 1;
	if (*s == '-') {
		m = -1;
		s++;
	}
	for (n = 0; isdigit(*s); s++)
		n = (n*10) + (*s-'0');
	if (*s) {
		prs(as);
		err(": bad number");
	}
	return(n*m);
}

static int
dobreak(t)
struct op *t;
{
	return(brkcontin(t->words[1], 1));
}

static int
docontinue(t)
struct op *t;
{
	return(brkcontin(t->words[1], 0));
}

static int
brkcontin(cp, val)
register char *cp;
int val;
{
	register struct brkcon *bc;
	register int nl;

	nl = cp == NULL? 1: getn(cp);
	if (nl <= 0)
		nl = 999;
	do {
		if ((bc = brklist) == NULL)
			break;
		brklist = bc->nextlev;
	} while (--nl);
	if (nl) {
		err("bad break/continue level");
		return(1);
	}
	isbreak = val;
	longjmp(bc->brkpt, 1);
	/* NOTREACHED */
}

static int
doexit(t)
struct op *t;
{
	register char *cp;

	execflg = 0;
	if ((cp = t->words[1]) != NULL)
		setstatus(getn(cp));
	leave();
	/* NOTREACHED */
	return(0);
}

static int
doexport(t)
struct op *t;
{
	rdexp(t->words+1, export, EXPORT);
	return(0);
}

static int
doreadonly(t)
struct op *t;
{
	rdexp(t->words+1, ronly, RONLY);
	return(0);
}

static void rdexp (char **wp, void (*f)(struct var *), int key)
{
	if (*wp != NULL) {
		for (; *wp != NULL; wp++) {
			if (isassign(*wp)) {
				char *cp;
				assign(*wp, COPYV);
				for (cp = *wp; *cp != '='; cp++)
					;
				*cp = '\0';
			}
			if (checkname(*wp))
				(*f)(lookup(*wp));
			else
				badid(*wp);
		}
	} else
		putvlist(key, 1);
}

static void
badid(s)
register char *s;
{
	prs(s);
	err(": bad identifier");
}

static int
doset(t)
register struct op *t;
{
	register struct var *vp;
	register char *cp;
	register int n;

	if ((cp = t->words[1]) == NULL) {
		for (vp = vlist; vp; vp = vp->next)
			varput(vp->name, 1);
		return(0);
	}
	if (*cp == '-') {
		/* bad: t->words++; */
		for(n = 0; (t->words[n]=t->words[n+1]) != NULL; n++)
			;
		if (*++cp == 0)
			flag['x'] = flag['v'] = 0;
		else
			for (; *cp; cp++)
				switch (*cp) {
				case 'e':
					if (!interactive)
						flag['e']++;
					break;

				default:
					if (*cp>='a' && *cp<='z')
						flag[(int)*cp]++;
					break;
				}
		setdash();
	}
	if (t->words[1]) {
		t->words[0] = dolv[0];
		for (n=1; t->words[n]; n++)
			setarea((char *)t->words[n], 0);
		dolc = n-1;
		dolv = t->words;
		setval(lookup("#"), putn(dolc));
		setarea((char *)(dolv-1), 0);
	}
	return(0);
}

static void
varput(s, out)
register char *s;
int out;
{
	if (isalnum(*s) || *s == '_') {
		write(out, s, strlen(s));
		write(out, "\n", 1);
	}
}


/*
 * Copyright (c) 1999 Herbert Xu <herbert@debian.org>
 * This file contains code for the times builtin.
 */
static int dotimes(struct op *t )
{
	struct tms buf;
	long int clk_tck = sysconf(_SC_CLK_TCK);

	times(&buf);
	printf("%dm%fs %dm%fs\n%dm%fs %dm%fs\n",
	       (int) (buf.tms_utime / clk_tck / 60),
	       ((double) buf.tms_utime) / clk_tck,
	       (int) (buf.tms_stime / clk_tck / 60),
	       ((double) buf.tms_stime) / clk_tck,
	       (int) (buf.tms_cutime / clk_tck / 60),
	       ((double) buf.tms_cutime) / clk_tck,
	       (int) (buf.tms_cstime / clk_tck / 60),
	       ((double) buf.tms_cstime) / clk_tck);
	return 0;
}


static int(*inbuilt(char *s))(struct op *)
{
	const struct builtincmd *bp;

	for (bp = builtincmds; bp->name != NULL; bp++)
		if (strcmp(bp->name, s) == 0)
			return(bp->builtinfunc);

	return(NULL);
}

/* -------- eval.c -------- */

/*
 * ${}
 * `command`
 * blank interpretation
 * quoting
 * glob
 */

static char ** eval( char **ap, int f)
{
	struct wdblock *wb;
	char **wp;
	char **wf;
	jmp_buf ev;

#if __GNUC__
	/* Avoid longjmp clobbering */
	(void) &wp;
	(void) &ap;
#endif
	wp = NULL;
	wb = NULL;
	wf = NULL;
	if (newenv(setjmp(errpt = ev)) == 0) {
		while (*ap && isassign(*ap))
			expand(*ap++, &wb, f & ~DOGLOB);
		if (flag['k']) {
			for (wf = ap; *wf; wf++) {
				if (isassign(*wf))
					expand(*wf, &wb, f & ~DOGLOB);
			}
		}
		for (wb = addword((char *)0, wb); *ap; ap++) {
			if (!flag['k'] || !isassign(*ap))
				expand(*ap, &wb, f & ~DOKEY);
		}
		wb = addword((char *)0, wb);
		wp = getwords(wb);
		quitenv();
	} else
		gflg = 1;
	return(gflg? (char **)NULL: wp);
}

/*
 * Make the exported environment from the exported
 * names in the dictionary. Keyword assignments
 * will already have been done.
 */
static char **
makenv()

{
	register struct wdblock *wb;
	register struct var *vp;

	wb = NULL;
	for (vp = vlist; vp; vp = vp->next)
		if (vp->status & EXPORT)
			wb = addword(vp->name, wb);
	wb = addword((char *)0, wb);
	return(getwords(wb));
}

static char *
evalstr(cp, f)
register char *cp;
int f;
{
	struct wdblock *wb;

	wb = NULL;
	if (expand(cp, &wb, f)) {
		if (wb == NULL || wb->w_nword == 0 || (cp = wb->w_words[0]) == NULL)
			cp = "";
		DELETE(wb);
	} else
		cp = NULL;
	return(cp);
}

static int
expand( char *cp, register struct wdblock **wbp, int f)
{
	jmp_buf ev;

#if __GNUC__
	/* Avoid longjmp clobbering */
	(void) &cp;
#endif
	gflg = 0;
	if (cp == NULL)
		return(0);
	if (!anys("$`'\"", cp) &&
	    !anys(ifs->value, cp) &&
	    ((f&DOGLOB)==0 || !anys("[*?", cp))) {
		cp = strsave(cp, areanum);
		if (f & DOTRIM)
			unquote(cp);
		*wbp = addword(cp, *wbp);
		return(1);
	}
	if (newenv(setjmp(errpt = ev)) == 0) {
		PUSHIO(aword, cp, strchar);
		e.iobase = e.iop;
		while ((cp = blank(f)) && gflg == 0) {
			e.linep = cp;
			cp = strsave(cp, areanum);
			if ((f&DOGLOB) == 0) {
				if (f & DOTRIM)
					unquote(cp);
				*wbp = addword(cp, *wbp);
			} else
				*wbp = glob(cp, *wbp);
		}
		quitenv();
	} else
		gflg = 1;
	return(gflg == 0);
}

/*
 * Blank interpretation and quoting
 */
static char *
blank(f)
int f;
{
	register int c, c1;
	register char *sp;
	int scanequals, foundequals;

	sp = e.linep;
	scanequals = f & DOKEY;
	foundequals = 0;

loop:
	switch (c = subgetc('"', foundequals)) {
	case 0:
		if (sp == e.linep)
			return(0);
		*e.linep++ = 0;
		return(sp);

	default:
		if (f & DOBLANK && any(c, ifs->value))
			goto loop;
		break;

	case '"':
	case '\'':
		scanequals = 0;
		if (INSUB())
			break;
		for (c1 = c; (c = subgetc(c1, 1)) != c1;) {
			if (c == 0)
				break;
			if (c == '\'' || !any(c, "$`\""))
				c |= QUOTE;
			*e.linep++ = c;
		}
		c = 0;
	}
	unget(c);
	if (!isalpha(c) && c != '_')
		scanequals = 0;
	for (;;) {
		c = subgetc('"', foundequals);
		if (c == 0 ||
		    f & (DOBLANK && any(c, ifs->value)) ||
		    (!INSUB() && any(c, "\"'"))) {
		        scanequals = 0;
			unget(c);
			if (any(c, "\"'"))
				goto loop;
			break;
		}
		if (scanequals) {
			if (c == '=') {
				foundequals = 1;
				scanequals  = 0;
			}
			else if (!isalnum(c) && c != '_')
				scanequals = 0;
		}
		*e.linep++ = c;
	}
	*e.linep++ = 0;
	return(sp);
}

/*
 * Get characters, substituting for ` and $
 */
static int
subgetc(ec, quoted)
register char ec;
int quoted;
{
	register char c;

again:
	c = my_getc(ec);
	if (!INSUB() && ec != '\'') {
		if (c == '`') {
			if (grave(quoted) == 0)
				return(0);
			e.iop->task = XGRAVE;
			goto again;
		}
		if (c == '$' && (c = dollar(quoted)) == 0) {
			e.iop->task = XDOLL;
			goto again;
		}
	}
	return(c);
}

/*
 * Prepare to generate the string returned by ${} substitution.
 */
static int
dollar(quoted)
int quoted;
{
	int otask;
	struct io *oiop;
	char *dolp;
	register char *s, c, *cp=NULL;
	struct var *vp;

	c = readc();
	s = e.linep;
	if (c != '{') {
		*e.linep++ = c;
		if (isalpha(c) || c == '_') {
			while ((c = readc())!=0 && (isalnum(c) || c == '_'))
				if (e.linep < elinep)
					*e.linep++ = c;
			unget(c);
		}
		c = 0;
	} else {
		oiop = e.iop;
		otask = e.iop->task;
		e.iop->task = XOTHER;
		while ((c = subgetc('"', 0))!=0 && c!='}' && c!='\n')
			if (e.linep < elinep)
				*e.linep++ = c;
		if (oiop == e.iop)
			e.iop->task = otask;
		if (c != '}') {
			err("unclosed ${");
			gflg++;
			return(c);
		}
	}
	if (e.linep >= elinep) {
		err("string in ${} too long");
		gflg++;
		e.linep -= 10;
	}
	*e.linep = 0;
	if (*s)
		for (cp = s+1; *cp; cp++)
			if (any(*cp, "=-+?")) {
				c = *cp;
				*cp++ = 0;
				break;
			}
	if (s[1] == 0 && (*s == '*' || *s == '@')) {
		if (dolc > 1) {
			/* currently this does not distinguish $* and $@ */
			/* should check dollar */
			e.linep = s;
			PUSHIO(awordlist, dolv+1, dolchar);
			return(0);
		} else {	/* trap the nasty ${=} */
			s[0] = '1';
			s[1] = 0;
		}
	}
	vp = lookup(s);
	if ((dolp = vp->value) == null) {
		switch (c) {
		case '=':
			if (isdigit(*s)) {
				err("cannot use ${...=...} with $n");
				gflg++;
				break;
			}
			setval(vp, cp);
			dolp = vp->value;
			break;

		case '-':
			dolp = strsave(cp, areanum);
			break;

		case '?':
			if (*cp == 0) {
				prs("missing value for ");
				err(s);
			} else
				err(cp);
			gflg++;
			break;
		}
	} else if (c == '+')
		dolp = strsave(cp, areanum);
	if (flag['u'] && dolp == null) {
		prs("unset variable: ");
		err(s);
		gflg++;
	}
	e.linep = s;
	PUSHIO(aword, dolp, quoted ? qstrchar : strchar);
	return(0);
}

/*
 * Run the command in `...` and read its output.
 */

static int
grave(quoted)
int quoted;
{
	char *cp;
	register int i;
	int j;
	int pf[2];
	static char child_cmd[LINELIM];
	char *src;
	char *dest;
	int count;
	int ignore;
	int ignore_once;
	char *argument_list[4];

#if __GNUC__
	/* Avoid longjmp clobbering */
	(void) &cp;
#endif
	
	for (cp = e.iop->argp->aword; *cp != '`'; cp++)
		if (*cp == 0) {
			err("no closing `");
			return(0);
		}

	/* string copy with dollar expansion */
	src = e.iop->argp->aword;
	dest = child_cmd;
	count = 0;
	ignore = 0;
	ignore_once = 0;
	while ((*src != '`') && (count < LINELIM)) {
		if (*src == '\'')
			ignore = !ignore;
		if (*src == '\\')
			ignore_once = 1;
		if (*src == '$' && !ignore && !ignore_once) {
			struct var *vp;
			char var_name[LINELIM];
			char alt_value[LINELIM];
			int var_index = 0;
			int alt_index = 0;
			char operator = 0;
			int braces = 0;
			char *value;

			src++;
			if (*src == '{') {
				braces = 1;
				src++;
			}

			var_name[var_index++] = *src++;
			while (isalnum(*src))
				var_name[var_index++] = *src++;
			var_name[var_index] = 0;

			if (braces) {
				switch (*src) {
				case '}':
					break;
				case '-':
				case '=':
				case '+':
				case '?':
					operator = *src;
					break;
				default:
					err("unclosed ${\n");
					return(0);
				}
				if (operator) {	
					src++;
					while (*src && (*src != '}')) {
						alt_value[alt_index++] = *src++;
					}
					alt_value[alt_index] = 0;
					if (*src != '}') {
						err("unclosed ${\n");
						return(0);
					}
				}
				src++;
			}

			vp = lookup(var_name);
			if (vp->value != null)
				value = (operator == '+')? alt_value : vp->value;
			else if (operator == '?') {
				err(alt_value);
				return(0);
			} else if (alt_index && (operator != '+')) {
				value = alt_value;
				if (operator == '=')
					setval(vp, value);
			} else
				continue;

			while (*value && (count < LINELIM)) {
				*dest++ = *value++;
				count++;
			}
		} else {
			*dest++ = *src++;
			count++;
			ignore_once = 0;
		}
	}
	*dest = '\0';
	
	if (openpipe(pf) < 0)
		return(0);
	while ((i = vfork()) == -1 && errno == EAGAIN)
		;
	if (i < 0) {
		closepipe(pf);
		err((char*)bb_msg_memory_exhausted);
		return(0);
	}
	if (i != 0) {
		waitpid(i, NULL, 0);
		e.iop->argp->aword = ++cp;
		close(pf[1]);
		PUSHIO(afile, remap(pf[0]), (int(*)(struct ioarg *))((quoted)? qgravechar: gravechar));
		return(1);
	}
	/* allow trapped signals */
	/* XXX - Maybe this signal stuff should go as well? */
	for (j=0; j<=_NSIG; j++)
		if (ourtrap[j] && signal(j, SIG_IGN) != SIG_IGN)
			signal(j, SIG_DFL);
	
	dup2(pf[1], 1);
	closepipe(pf);

	argument_list[0] = (char *)DEFAULT_SHELL;
	argument_list[1] = "-c";
	argument_list[2] = child_cmd;
	argument_list[3] = 0;

	prs(rexecve(argument_list[0], argument_list, makenv()));
	_exit(1);
}


static char *
unquote(as)
register char *as;
{
	register char *s;

	if ((s = as) != NULL)
		while (*s)
			*s++ &= ~QUOTE;
	return(as);
}

/* -------- glob.c -------- */

/*
 * glob
 */

#define	scopy(x) strsave((x), areanum)
#define	BLKSIZ	512
#define	NDENT	((BLKSIZ+sizeof(struct dirent)-1)/sizeof(struct dirent))

static	struct wdblock	*cl, *nl;
static	char	spcl[] = "[?*";

static struct wdblock *
glob(cp, wb)
char *cp;
struct wdblock *wb;
{
	register int i;
	register char *pp;

	if (cp == 0)
		return(wb);
	i = 0;
	for (pp = cp; *pp; pp++)
		if (any(*pp, spcl))
			i++;
		else if (!any(*pp & ~QUOTE, spcl))
			*pp &= ~QUOTE;
	if (i != 0) {
		for (cl = addword(scopy(cp), (struct wdblock *)0); anyspcl(cl); cl = nl) {
			nl = newword(cl->w_nword*2);
			for(i=0; i<cl->w_nword; i++) { /* for each argument */
				for (pp = cl->w_words[i]; *pp; pp++)
					if (any(*pp, spcl)) {
						globname(cl->w_words[i], pp);
						break;
					}
				if (*pp == '\0')
					nl = addword(scopy(cl->w_words[i]), nl);
			}
			for(i=0; i<cl->w_nword; i++)
				DELETE(cl->w_words[i]);
			DELETE(cl);
		}
		for(i=0; i<cl->w_nword; i++)
			unquote(cl->w_words[i]);
		glob0((char *)cl->w_words, cl->w_nword, sizeof(char *), xstrcmp);
		if (cl->w_nword) {
			for (i=0; i<cl->w_nword; i++)
				wb = addword(cl->w_words[i], wb);
			DELETE(cl);
			return(wb);
		}
	}
	wb = addword(unquote(cp), wb);
	return(wb);
}

static void
globname(we, pp)
char *we;
register char *pp;
{
	register char *np, *cp;
	char *name, *gp, *dp;
	int k;
	DIR *dirp;
	struct dirent *de;
	char dname[NAME_MAX+1];
	struct stat dbuf;

	for (np = we; np != pp; pp--)
		if (pp[-1] == '/')
			break;
	for (dp = cp = space((int)(pp-np)+3); np < pp;)
		*cp++ = *np++;
	*cp++ = '.';
	*cp = '\0';
	for (gp = cp = space(strlen(pp)+1); *np && *np != '/';)
		*cp++ = *np++;
	*cp = '\0';
	dirp = opendir(dp);
	if (dirp == 0) {
		DELETE(dp);
		DELETE(gp);
		return;
	}
	dname[NAME_MAX] = '\0';
	while ((de=readdir(dirp))!=NULL) {
		/* XXX Hmmm... What this could be? (abial) */
		/*
		if (ent[j].d_ino == 0)
			continue;
		*/
		strncpy(dname, de->d_name, NAME_MAX);
			if (dname[0] == '.')
				if (*gp != '.')
					continue;
			for(k=0; k<NAME_MAX; k++)
				if (any(dname[k], spcl))
					dname[k] |= QUOTE;
			if (gmatch(dname, gp)) {
				name = generate(we, pp, dname, np);
				if (*np && !anys(np, spcl)) {
					if (stat(name,&dbuf)) {
						DELETE(name);
						continue;
					}
				}
				nl = addword(name, nl);
			}
	}
	closedir(dirp);
	DELETE(dp);
	DELETE(gp);
}

/*
 * generate a pathname as below.
 * start..end1 / middle end
 * the slashes come for free
 */
static char *
generate(start1, end1, middle, end)
char *start1;
register char *end1;
char *middle, *end;
{
	char *p;
	register char *op, *xp;

	p = op = space((int)(end1-start1)+strlen(middle)+strlen(end)+2);
	for (xp = start1; xp != end1;)
		*op++ = *xp++;
	for (xp = middle; (*op++ = *xp++) != '\0';)
		;
	op--;
	for (xp = end; (*op++ = *xp++) != '\0';)
		;
	return(p);
}

static int
anyspcl(wb)
register struct wdblock *wb;
{
	register int i;
	register char **wd;

	wd = wb->w_words;
	for (i=0; i<wb->w_nword; i++)
		if (anys(spcl, *wd++))
			return(1);
	return(0);
}

static int
xstrcmp(p1, p2)
char *p1, *p2;
{
	return(strcmp(*(char **)p1, *(char **)p2));
}

/* -------- word.c -------- */

static struct wdblock *
newword(nw)
register int nw;
{
	register struct wdblock *wb;

	wb = (struct wdblock *) space(sizeof(*wb) + nw*sizeof(char *));
	wb->w_bsize = nw;
	wb->w_nword = 0;
	return(wb);
}

static struct wdblock *
addword(wd, wb)
char *wd;
register struct wdblock *wb;
{
	register struct wdblock *wb2;
	register int nw;

	if (wb == NULL)
		wb = newword(NSTART);
	if ((nw = wb->w_nword) >= wb->w_bsize) {
		wb2 = newword(nw * 2);
		memcpy((char *)wb2->w_words, (char *)wb->w_words, nw*sizeof(char *));
		wb2->w_nword = nw;
		DELETE(wb);
		wb = wb2;
	}
	wb->w_words[wb->w_nword++] = wd;
	return(wb);
}
static 
char **
getwords(wb)
register struct wdblock *wb;
{
	register char **wd;
	register int nb;

	if (wb == NULL)
		return((char **)NULL);
	if (wb->w_nword == 0) {
		DELETE(wb);
		return((char **)NULL);
	}
	wd = (char **) space(nb = sizeof(*wd) * wb->w_nword);
	memcpy((char *)wd, (char *)wb->w_words, nb);
	DELETE(wb);	/* perhaps should done by caller */
	return(wd);
}

int (*func)(char *, char *);
int	globv;

static void
glob0(a0, a1, a2, a3)
char *a0;
unsigned a1;
int a2;
int (*a3) (char *, char *);
{
	func = a3;
	globv = a2;
	glob1(a0, a0 + a1 * a2);
}

static void
glob1(base, lim)
char *base, *lim;
{
	register char *i, *j;
	int v2;
	char *lptr, *hptr;
	int c;
	unsigned n;


	v2 = globv;

top:
	if ((n=(int)(lim-base)) <= v2)
		return;
	n = v2 * (n / (2*v2));
	hptr = lptr = base+n;
	i = base;
	j = lim-v2;
	for(;;) {
		if (i < lptr) {
			if ((c = (*func)(i, lptr)) == 0) {
				glob2(i, lptr -= v2);
				continue;
			}
			if (c < 0) {
				i += v2;
				continue;
			}
		}

begin:
		if (j > hptr) {
			if ((c = (*func)(hptr, j)) == 0) {
				glob2(hptr += v2, j);
				goto begin;
			}
			if (c > 0) {
				if (i == lptr) {
					glob3(i, hptr += v2, j);
					i = lptr += v2;
					goto begin;
				}
				glob2(i, j);
				j -= v2;
				i += v2;
				continue;
			}
			j -= v2;
			goto begin;
		}


		if (i == lptr) {
			if (lptr-base >= lim-hptr) {
				glob1(hptr+v2, lim);
				lim = lptr;
			} else {
				glob1(base, lptr);
				base = hptr+v2;
			}
			goto top;
		}


		glob3(j, lptr -= v2, i);
		j = hptr -= v2;
	}
}

static void
glob2(i, j)
char *i, *j;
{
	register char *index1, *index2, c;
	int m;

	m = globv;
	index1 = i;
	index2 = j;
	do {
		c = *index1;
		*index1++ = *index2;
		*index2++ = c;
	} while(--m);
}

static void
glob3(i, j, k)
char *i, *j, *k;
{
	register char *index1, *index2, *index3;
	int c;
	int m;

	m = globv;
	index1 = i;
	index2 = j;
	index3 = k;
	do {
		c = *index1;
		*index1++ = *index3;
		*index3++ = *index2;
		*index2++ = c;
	} while(--m);
}

/* -------- io.c -------- */

/*
 * shell IO
 */

static int my_getc( int ec)
{
	register int c;

	if(e.linep > elinep) {
		while((c=readc()) != '\n' && c)
			;
		err("input line too long");
		gflg++;
		return(c);
	}
	c = readc();
	if ((ec != '\'') && (ec != '`') && (e.iop->task != XGRAVE)) {
		if(c == '\\') {
			c = readc();
			if (c == '\n' && ec != '\"')
				return(my_getc(ec));
			c |= QUOTE;
		}
	}
	return(c);
}

static void
unget(c)
int c;
{
	if (e.iop >= e.iobase)
		e.iop->peekc = c;
}

static int
eofc()

{
  return e.iop < e.iobase || (e.iop->peekc == 0 && e.iop->prev == 0);
}

static int
readc()
{
	register int c;

	for (; e.iop >= e.iobase; e.iop--)
		if ((c = e.iop->peekc) != '\0') {
			e.iop->peekc = 0;
			return(c);
		}
		else {
		    if (e.iop->prev != 0) {
		        if ((c = (*e.iop->iofn)(e.iop->argp, e.iop)) != '\0') {
			        if (c == -1) {
				        e.iop++;
				        continue;
			        }
			        if (e.iop == iostack)
				        ioecho(c);
			        return(e.iop->prev = c);
		        }
		        else if (e.iop->task == XIO && e.iop->prev != '\n') {
			        e.iop->prev = 0;
				if (e.iop == iostack)
					ioecho('\n');
			        return '\n';
		        }
		    }
		    if (e.iop->task == XIO) {
			if (multiline)
			    return e.iop->prev = 0;
			if (interactive && e.iop == iostack+1) {
#ifdef CONFIG_FEATURE_COMMAND_EDITING
			    current_prompt=prompt->value;
#else
			    prs(prompt->value);
#endif
			}
		    }
		}
	if (e.iop >= iostack)
		return(0);
	leave();
	/* NOTREACHED */
	return(0);
}

static void
ioecho(c)
char c;
{
	if (flag['v'])
		write(2, &c, sizeof c);
}

static void
pushio(struct ioarg *argp, int (*fn)(struct ioarg *))
{
	if (++e.iop >= &iostack[NPUSH]) {
		e.iop--;
		err("Shell input nested too deeply");
		gflg++;
		return;
	}
	e.iop->iofn = (int (*)(struct  ioarg *, struct io *))fn;

	if (argp->afid != AFID_NOBUF)
	  e.iop->argp = argp;
	else {
	  e.iop->argp  = ioargstack + (e.iop - iostack);
	  *e.iop->argp = *argp;
	  e.iop->argp->afbuf = e.iop == &iostack[0] ? &mainbuf : &sharedbuf;
	  if (isatty(e.iop->argp->afile) == 0 &&
	      (e.iop == &iostack[0] ||
	       lseek(e.iop->argp->afile, 0L, 1) != -1)) {
	    if (++bufid == AFID_NOBUF)
	      bufid = AFID_ID;
	    e.iop->argp->afid  = bufid;
	  }
	}

	e.iop->prev  = ~'\n';
	e.iop->peekc = 0;
	e.iop->xchar = 0;
	e.iop->nlcount = 0;
	if (fn == filechar || fn == linechar)
		e.iop->task = XIO;
	else if (fn == (int(*)(struct ioarg *))gravechar || fn == (int(*)(struct ioarg *))qgravechar)
		e.iop->task = XGRAVE;
	else
		e.iop->task = XOTHER;
}

static struct io *
setbase(ip)
struct io *ip;
{
	register struct io *xp;

	xp = e.iobase;
	e.iobase = ip;
	return(xp);
}

/*
 * Input generating functions
 */

/*
 * Produce the characters of a string, then a newline, then EOF.
 */
static int
nlchar(ap)
register struct ioarg *ap;
{
	register int c;

	if (ap->aword == NULL)
		return(0);
	if ((c = *ap->aword++) == 0) {
		ap->aword = NULL;
		return('\n');
	}
	return(c);
}

/*
 * Given a list of words, produce the characters
 * in them, with a space after each word.
 */
static int
wdchar(ap)
register struct ioarg *ap;
{
	register char c;
	register char **wl;

	if ((wl = ap->awordlist) == NULL)
		return(0);
	if (*wl != NULL) {
		if ((c = *(*wl)++) != 0)
			return(c & 0177);
		ap->awordlist++;
		return(' ');
	}
	ap->awordlist = NULL;
	return('\n');
}

/*
 * Return the characters of a list of words,
 * producing a space between them.
 */
static int
dolchar(ap)
register struct ioarg *ap;
{
	register char *wp;

	if ((wp = *ap->awordlist++) != NULL) {
		PUSHIO(aword, wp, *ap->awordlist == NULL? strchar: xxchar);
		return(-1);
	}
	return(0);
}

static int
xxchar(ap)
register struct ioarg *ap;
{
	register int c;

	if (ap->aword == NULL)
		return(0);
	if ((c = *ap->aword++) == '\0') {
		ap->aword = NULL;
		return(' ');
	}
	return(c);
}

/*
 * Produce the characters from a single word (string).
 */
static int
strchar(ap)
register struct ioarg *ap;
{
	register int c;

	if (ap->aword == NULL || (c = *ap->aword++) == 0)
		return(0);
	return(c);
}

/*
 * Produce quoted characters from a single word (string).
 */
static int
qstrchar(ap)
register struct ioarg *ap;
{
	register int c;

	if (ap->aword == NULL || (c = *ap->aword++) == 0)
		return(0);
	return(c|QUOTE);
}

/*
 * Return the characters from a file.
 */
static int
filechar(ap)
register struct ioarg *ap;
{
	register int i;
	char c;
	struct iobuf *bp = ap->afbuf;

	if (ap->afid != AFID_NOBUF) {
	  if ((i = ap->afid != bp->id) || bp->bufp == bp->ebufp) {
	    if (i)
	      lseek(ap->afile, ap->afpos, 0);
	    i = safe_read(ap->afile, bp->buf, sizeof(bp->buf));
	    if (i <= 0) {
	      closef(ap->afile);
	      return 0;
	    }
	    bp->id = ap->afid;
	    bp->ebufp = (bp->bufp  = bp->buf) + i;
	  }
	  ap->afpos++;
	  return *bp->bufp++ & 0177;
	}

#ifdef CONFIG_FEATURE_COMMAND_EDITING
	if (interactive && isatty(ap->afile)) {
	    static char mycommand[BUFSIZ];
	    static int position = 0, size = 0;

	    while (size == 0 || position >= size) {
		cmdedit_read_input(current_prompt, mycommand);
		size = strlen(mycommand);
		position = 0;
	    }
	    c = mycommand[position];
	    position++;
	    return(c);
	} else 
#endif
	{
		i = safe_read(ap->afile, &c, sizeof(c));
		return(i == sizeof(c)? c&0177: (closef(ap->afile), 0));
	}
}

/*
 * Return the characters from a here temp file.
 */
static int
herechar(ap)
register struct ioarg *ap;
{
	char c;


	if (read(ap->afile, &c, sizeof(c)) != sizeof(c)) {
		close(ap->afile);
		c = 0;
	}
	return (c);

}

/*
 * Return the characters produced by a process (`...`).
 * Quote them if required, and remove any trailing newline characters.
 */
static int
gravechar(ap, iop)
struct ioarg *ap;
struct io *iop;
{
	register int c;

	if ((c = qgravechar(ap, iop)&~QUOTE) == '\n')
		c = ' ';
	return(c);
}

static int
qgravechar(ap, iop)
register struct ioarg *ap;
struct io *iop;
{
	register int c;

	if (iop->xchar) {
		if (iop->nlcount) {
			iop->nlcount--;
			return('\n'|QUOTE);
		}
		c = iop->xchar;
		iop->xchar = 0;
	} else if ((c = filechar(ap)) == '\n') {
		iop->nlcount = 1;
		while ((c = filechar(ap)) == '\n')
			iop->nlcount++;
		iop->xchar = c;
		if (c == 0)
			return(c);
		iop->nlcount--;
		c = '\n';
	}
	return(c!=0? c|QUOTE: 0);
}

/*
 * Return a single command (usually the first line) from a file.
 */
static int
linechar(ap)
register struct ioarg *ap;
{
	register int c;

	if ((c = filechar(ap)) == '\n') {
		if (!multiline) {
			closef(ap->afile);
			ap->afile = -1;	/* illegal value */
		}
	}
	return(c);
}

static void
prs(s)
register char *s;
{
	if (*s)
		write(2, s, strlen(s));
}

static void
prn(u)
unsigned u;
{
	prs(itoa(u));
}

static void
closef(i)
register int i;
{
	if (i > 2)
		close(i);
}

static void
closeall()
{
	register int u;

	for (u=NUFILE; u<NOFILE;)
		close(u++);
}

/*
 * remap fd into Shell's fd space
 */
static int
remap(fd)
register int fd;
{
	register int i;
	int map[NOFILE];

	if (fd < e.iofd) {
		for (i=0; i<NOFILE; i++)
			map[i] = 0;
		do {
			map[fd] = 1;
			fd = dup(fd);
		} while (fd >= 0 && fd < e.iofd);
		for (i=0; i<NOFILE; i++)
			if (map[i])
				close(i);
		if (fd < 0)
			err("too many files open in shell");
	}
	return(fd);
}

static int
openpipe(pv)
register int *pv;
{
	register int i;

	if ((i = pipe(pv)) < 0)
		err("can't create pipe - try again");
	return(i);
}

static void
closepipe(pv)
register int *pv;
{
	if (pv != NULL) {
		close(*pv++);
		close(*pv);
	}
}

/* -------- here.c -------- */

/*
 * here documents
 */

static void
markhere(s, iop)
register char *s;
struct ioword *iop;
{
	register struct here *h, *lh;

	h = (struct here *) space(sizeof(struct here));
	if (h == 0)
		return;
	h->h_tag = evalstr(s, DOSUB);
	if (h->h_tag == 0)
		return;
	h->h_iop = iop;
	iop->io_name = 0;
	h->h_next = NULL;
	if (inhere == 0)
		inhere = h;
	else
		for (lh = inhere; lh!=NULL; lh = lh->h_next)
			if (lh->h_next == 0) {
				lh->h_next = h;
				break;
			}
	iop->io_flag |= IOHERE|IOXHERE;
	for (s = h->h_tag; *s; s++)
		if (*s & QUOTE) {
			iop->io_flag &= ~ IOXHERE;
			*s &= ~ QUOTE;
		}
	h->h_dosub = iop->io_flag & IOXHERE;
}

static void
gethere()
{
	register struct here *h, *hp;

	/* Scan here files first leaving inhere list in place */
	for (hp = h = inhere; h != NULL; hp = h, h = h->h_next)
	  readhere(&h->h_iop->io_name, h->h_tag, h->h_dosub? 0: '\'');

	/* Make inhere list active - keep list intact for scraphere */
	if (hp != NULL) {
	  hp->h_next = acthere;
	  acthere    = inhere;
	  inhere     = NULL;
	}
}

static void
readhere(name, s, ec)
char **name;
register char *s;
int ec;
{
	int tf;
	char tname[30] = ".msh_XXXXXX";
	register int c;
	jmp_buf ev;
	char myline [LINELIM+1];
	char *thenext;

	tf = mkstemp(tname);
	if (tf < 0)
		return;
	*name = strsave(tname, areanum);
	if (newenv(setjmp(errpt = ev)) != 0)
		unlink(tname);
	else {
		pushio(e.iop->argp, (int(*)(struct ioarg *))e.iop->iofn);
		e.iobase = e.iop;
		for (;;) {
		    if (interactive && e.iop <= iostack) {
#ifdef CONFIG_FEATURE_COMMAND_EDITING
			    current_prompt=cprompt->value;
#else
			    prs(cprompt->value);
#endif
			}
			thenext = myline;
			while ((c = my_getc(ec)) != '\n' && c) {
				if (ec == '\'')
					c &= ~ QUOTE;
				if (thenext >= &myline[LINELIM]) {
					c = 0;
					break;
				}
				*thenext++ = c;
			}
			*thenext = 0;
			if (strcmp(s, myline) == 0 || c == 0)
				break;
			*thenext++ = '\n';
			write (tf, myline, (int)(thenext-myline));
		}
		if (c == 0) {
			prs("here document `"); prs(s); err("' unclosed");
		}
		quitenv();
	}
	close(tf);
}

/*
 * open here temp file.
 * if unquoted here, expand here temp file into second temp file.
 */
static int
herein(hname, xdoll)
char *hname;
int xdoll;
{
	register int hf;
	int tf;

#if __GNUC__
	/* Avoid longjmp clobbering */
	(void) &tf;
#endif
	if (hname == 0)
		return(-1);
	hf = open(hname, 0);
	if (hf < 0)
		return (-1);
	if (xdoll) {
		char c;
		char tname[30] = ".msh_XXXXXX";
		jmp_buf ev;
	
		tf = mkstemp(tname);
		if (tf < 0)
			return (-1);
		if (newenv(setjmp(errpt = ev)) == 0) {
			PUSHIO(afile, hf, herechar);
			setbase(e.iop);
			while ((c = subgetc(0, 0)) != 0) {
				c &= ~ QUOTE;
				write(tf, &c, sizeof c);
			}
			quitenv();
		} else
			unlink(tname);
		close(tf);
		tf = open(tname, 0);
		unlink(tname);
		return (tf);
	} else
		return (hf);
}

static void
scraphere()
{
	register struct here *h;

	for (h = inhere; h != NULL; h = h->h_next) {
		if (h->h_iop && h->h_iop->io_name)
		  unlink(h->h_iop->io_name);
	}
	inhere = NULL;
}

/* unlink here temp files before a freearea(area) */
static void
freehere(area)
int area;
{
	register struct here *h, *hl;

	hl = NULL;
	for (h = acthere; h != NULL; h = h->h_next)
		if (getarea((char *) h) >= area) {
			if (h->h_iop->io_name != NULL)
				unlink(h->h_iop->io_name);
			if (hl == NULL)
				acthere = h->h_next;
			else
				hl->h_next = h->h_next;
		} else
			hl = h;
}



/*
 * Copyright (c) 1987,1997, Prentice Hall
 * All rights reserved.
 * 
 * Redistribution and use of the MINIX operating system in source and
 * binary forms, with or without modification, are permitted provided
 * that the following conditions are met:
 * 
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 * 
 * Neither the name of Prentice Hall nor the names of the software
 * authors or contributors may be used to endorse or promote
 * products derived from this software without specific prior
 * written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS, AUTHORS, AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL PRENTICE HALL OR ANY AUTHORS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

