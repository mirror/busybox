/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#undef _GNU_SOURCE
#undef ASH_TYPE
#undef ASH_GETOPTS
#undef ASH_MATH_SUPPORT

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/cdefs.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/wait.h>


#if !defined(FNMATCH_BROKEN)
#include <fnmatch.h>
#endif
#if !defined(GLOB_BROKEN)
#include <glob.h>
#endif

#if JOBS
#include <termios.h>
#undef CEOF			/* syntax.h redefines this */
#endif

#include "cmdedit.h"
#include "busybox.h"
#include "ash.h"


#define _DIAGASSERT(x)

#define ATABSIZE 39

#define S_DFL 1			/* default signal handling (SIG_DFL) */
#define S_CATCH 2		/* signal is caught */
#define S_IGN 3			/* signal is ignored (SIG_IGN) */
#define S_HARD_IGN 4		/* signal is ignored permenantly */
#define S_RESET 5		/* temporary - to reset a hard ignored sig */



struct alias *atab[ATABSIZE];

static void setalias __P((char *, char *));
static struct alias **hashalias __P((const char *));
static struct alias *freealias __P((struct alias *));
static struct alias **__lookupalias __P((const char *));
static char *trap[NSIG];		/* trap handler commands */
static char sigmode[NSIG - 1];	/* current value of signal */
static char gotsig[NSIG - 1];		/* indicates specified signal received */
static int pendingsigs;			/* indicates some signal received */


static void
setalias(name, val)
	char *name, *val;
{
	struct alias *ap, **app;

	app = __lookupalias(name);
	ap = *app;
	INTOFF;
	if (ap) {
		if (!(ap->flag & ALIASINUSE)) {
			ckfree(ap->val);
		}
		ap->val	= savestr(val);
		ap->flag &= ~ALIASDEAD;
	} else {
		/* not found */
		ap = ckmalloc(sizeof (struct alias));
		ap->name = savestr(name);
		ap->val = savestr(val);
		ap->flag = 0;
		ap->next = 0;
		*app = ap;
	}
	INTON;
}

static int
unalias(name)
	char *name;
	{
	struct alias **app;

	app = __lookupalias(name);

	if (*app) {
		INTOFF;
		*app = freealias(*app);
		INTON;
		return (0);
	}

	return (1);
}

#ifdef mkinit
static void rmaliases __P((void));

SHELLPROC {
	rmaliases();
}
#endif

static void
rmaliases() {
	struct alias *ap, **app;
	int i;

	INTOFF;
	for (i = 0; i < ATABSIZE; i++) {
		app = &atab[i];
		for (ap = *app; ap; ap = *app) {
			*app = freealias(*app);
			if (ap == *app) {
				app = &ap->next;
			}
		}
	}
	INTON;
}

struct alias *
lookupalias(name, check)
	const char *name;
	int check;
{
	struct alias *ap = *__lookupalias(name);

	if (check && ap && (ap->flag & ALIASINUSE))
		return (NULL);
	return (ap);
}


/*
 * TODO - sort output
 */
static int
aliascmd(argc, argv)
	int argc;
	char **argv;
{
	char *n, *v;
	int ret = 0;
	struct alias *ap;

	if (argc == 1) {
		int i;

		for (i = 0; i < ATABSIZE; i++)
			for (ap = atab[i]; ap; ap = ap->next) {
				printalias(ap);
			}
		return (0);
	}
	while ((n = *++argv) != NULL) {
		if ((v = strchr(n+1, '=')) == NULL) { /* n+1: funny ksh stuff */
			if ((ap = *__lookupalias(n)) == NULL) {
				outfmt(out2, "%s: %s not found\n", "alias", n);
				ret = 1;
			} else
				printalias(ap);
		}
		else {
			*v++ = '\0';
			setalias(n, v);
		}
	}

	return (ret);
}

static int
unaliascmd(argc, argv)
	int argc;
	char **argv;
{
	int i;

	while ((i = nextopt("a")) != '\0') {
		if (i == 'a') {
			rmaliases();
			return (0);
		}
	}
	for (i = 0; *argptr; argptr++) {
		if (unalias(*argptr)) {
			outfmt(out2, "%s: %s not found\n", "unalias", *argptr);
			i = 1;
		}
	}

	return (i);
}

static struct alias **
hashalias(p)
	const char *p;
	{
	unsigned int hashval;

	hashval = *p << 4;
	while (*p)
		hashval+= *p++;
	return &atab[hashval % ATABSIZE];
}

static struct alias *
freealias(struct alias *ap) {
	struct alias *next;

	if (ap->flag & ALIASINUSE) {
		ap->flag |= ALIASDEAD;
		return ap;
	}

	next = ap->next;
	ckfree(ap->name);
	ckfree(ap->val);
	ckfree(ap);
	return next;
}

static void
printalias(const struct alias *ap) {
	char *p;

	p = single_quote(ap->val);
	out1fmt("alias %s=%s\n", ap->name, p);
	stunalloc(p);
}

static struct alias **
__lookupalias(const char *name) {
	struct alias **app = hashalias(name);

	for (; *app; app = &(*app)->next) {
		if (equal(name, (*app)->name)) {
			break;
		}
	}

	return app;
}

#ifdef ASH_MATH_SUPPORT
/* The generated file arith.c has been snipped.  If you want this
 * stuff back in, feel free to add it to your own copy.  */
#endif

/*
 * This file was generated by the mkbuiltins program.
 */

static int bgcmd __P((int, char **));
static int breakcmd __P((int, char **));
static int cdcmd __P((int, char **));
static int commandcmd __P((int, char **));
static int dotcmd __P((int, char **));
static int evalcmd __P((int, char **));
static int execcmd __P((int, char **));
static int exitcmd __P((int, char **));
static int exportcmd __P((int, char **));
static int histcmd __P((int, char **));
static int fgcmd __P((int, char **));
static int hashcmd __P((int, char **));
static int jobscmd __P((int, char **));
static int killcmd __P((int, char **));
static int localcmd __P((int, char **));
static int pwdcmd __P((int, char **));
static int readcmd __P((int, char **));
static int returncmd __P((int, char **));
static int setcmd __P((int, char **));
static int setvarcmd __P((int, char **));
static int shiftcmd __P((int, char **));
static int trapcmd __P((int, char **));
static int umaskcmd __P((int, char **));
static int unaliascmd __P((int, char **));
static int unsetcmd __P((int, char **));
static int waitcmd __P((int, char **));
static int aliascmd __P((int, char **));
static int ulimitcmd __P((int, char **));
static int timescmd __P((int, char **));
#ifdef ASH_MATH_SUPPORT
static int expcmd __P((int, char **));
#endif
#ifdef ASH_TYPE
static int typecmd __P((int, char **));
#endif
#ifdef ASH_GETOPTS
static int getoptscmd __P((int, char **));
#endif
#ifndef BB_TRUE_FALSE
static int true_main __P((int, char **));
static int false_main __P((int, char **));
#endif

static struct builtincmd *DOTCMD;
static struct builtincmd *BLTINCMD;
static struct builtincmd *COMMANDCMD;
static struct builtincmd *EXECCMD;
static struct builtincmd *EVALCMD;

/* It is CRUCIAL that this listing be kept in ascii order, otherwise
 * the binary search in find_builtin() will stop working. If you value
 * your kneecaps, you'll be sure to *make sure* that any changes made
 * to this array result in the listing remaining in ascii order. You
 * have been warned.
 */
static const struct builtincmd builtincmds[] = {
	{ ".", dotcmd, 1 },
	{ ":", true_main, 1 },
	{ "alias", aliascmd, 6 },
	{ "bg", bgcmd, 2 },
	{ "break", breakcmd, 1 },
	{ "builtin", bltincmd, 1 },
	{ "cd", cdcmd, 2 },
	{ "chdir", cdcmd, 0 },
	{ "command", commandcmd, 2 },
	{ "continue", breakcmd, 1 },
	{ "eval", evalcmd, 1 },
	{ "exec", execcmd, 1 },
	{ "exit", exitcmd, 1 },
#ifdef ASH_MATH_SUPPORT
	{ "exp", expcmd, 0 },
#endif
	{ "export", exportcmd, 5 },
	{ "false", false_main, 2 },
	{ "fc", histcmd, 2 },
	{ "fg", fgcmd, 2 },
#ifdef ASH_GETOPTS
	{ "getopts", getoptscmd, 2 },
#endif	
	{ "hash", hashcmd, 0 },
	{ "jobs", jobscmd, 2 },
	{ "kill", killcmd, 2 },
#ifdef ASH_MATH_SUPPORT
	{ "let", expcmd, 0 },
#endif
	{ "local", localcmd, 4 },
	{ "pwd", pwdcmd, 0 },
	{ "read", readcmd, 2 },
	{ "readonly", exportcmd, 5 },
	{ "return", returncmd, 1 },
	{ "set", setcmd, 1 },
	{ "setvar", setvarcmd, 0 },
	{ "shift", shiftcmd, 1 },
	{ "times", timescmd, 1 },
	{ "trap", trapcmd, 1 },
	{ "true", true_main, 2 },
#ifdef ASH_TYPE
	{ "type", typecmd, 0 },
#endif
	{ "ulimit", ulimitcmd, 0 },
	{ "umask", umaskcmd, 2 },
	{ "unalias", unaliascmd, 2 },
	{ "unset", unsetcmd, 1 },
	{ "wait", waitcmd, 2 },
};
#define NUMBUILTINS  (sizeof (builtincmds) / sizeof (struct builtincmd) )


/*	$NetBSD: cd.c,v 1.27 1999/07/09 03:05:49 christos Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

static int docd __P((char *, int));
static char *getcomponent __P((void));
static void updatepwd __P((char *));
static void getpwd __P((void));

static char *curdir = nullstr;		/* current working directory */
static char *cdcomppath;

#ifdef mkinit
INCLUDE "cd.h"
INIT {
	setpwd(0, 0);
}
#endif

static int
cdcmd(argc, argv)
	int argc;
	char **argv;
{
	const char *dest;
	const char *path;
	char *p;
	struct stat statb;
	int print = 0;

	nextopt(nullstr);
	if ((dest = *argptr) == NULL && (dest = bltinlookup("HOME")) == NULL)
		error("HOME not set");
	if (*dest == '\0')
	        dest = ".";
	if (dest[0] == '-' && dest[1] == '\0') {
		dest = bltinlookup("OLDPWD");
		if (!dest || !*dest) {
			dest = curdir;
		}
		print = 1;
		if (dest)
		        print = 1;
		else
		        dest = ".";
	}
	if (*dest == '/' || (path = bltinlookup("CDPATH")) == NULL)
		path = nullstr;
	while ((p = padvance(&path, dest)) != NULL) {
		if (stat(p, &statb) >= 0 && S_ISDIR(statb.st_mode)) {
			if (!print) {
				/*
				 * XXX - rethink
				 */
				if (p[0] == '.' && p[1] == '/' && p[2] != '\0')
					p += 2;
				print = strcmp(p, dest);
			}
			if (docd(p, print) >= 0)
				return 0;

		}
	}
	error("can't cd to %s", dest);
	/* NOTREACHED */
}


/*
 * Actually do the chdir.  In an interactive shell, print the
 * directory name if "print" is nonzero.
 */

static int
docd(dest, print)
	char *dest;
	int print;
{
	char *p;
	char *q;
	char *component;
	struct stat statb;
	int first;
	int badstat;

	TRACE(("docd(\"%s\", %d) called\n", dest, print));

	/*
	 *  Check each component of the path. If we find a symlink or
	 *  something we can't stat, clear curdir to force a getcwd()
	 *  next time we get the value of the current directory.
	 */
	badstat = 0;
	cdcomppath = sstrdup(dest);
	STARTSTACKSTR(p);
	if (*dest == '/') {
		STPUTC('/', p);
		cdcomppath++;
	}
	first = 1;
	while ((q = getcomponent()) != NULL) {
		if (q[0] == '\0' || (q[0] == '.' && q[1] == '\0'))
			continue;
		if (! first)
			STPUTC('/', p);
		first = 0;
		component = q;
		while (*q)
			STPUTC(*q++, p);
		if (equal(component, ".."))
			continue;
		STACKSTRNUL(p);
		if ((lstat(stackblock(), &statb) < 0)
		    || (S_ISLNK(statb.st_mode)))  {
			/* print = 1; */
			badstat = 1;
			break;
		}
	}

	INTOFF;
	if (chdir(dest) < 0) {
		INTON;
		return -1;
	}
	updatepwd(badstat ? NULL : dest);
	INTON;
	if (print && iflag)
		out1fmt(snlfmt, curdir);
	return 0;
}


/*
 * Get the next component of the path name pointed to by cdcomppath.
 * This routine overwrites the string pointed to by cdcomppath.
 */

static char *
getcomponent() {
	char *p;
	char *start;

	if ((p = cdcomppath) == NULL)
		return NULL;
	start = cdcomppath;
	while (*p != '/' && *p != '\0')
		p++;
	if (*p == '\0') {
		cdcomppath = NULL;
	} else {
		*p++ = '\0';
		cdcomppath = p;
	}
	return start;
}



/*
 * Update curdir (the name of the current directory) in response to a
 * cd command.  We also call hashcd to let the routines in exec.c know
 * that the current directory has changed.
 */

static void
updatepwd(dir)
	char *dir;
	{
	char *new;
	char *p;
	size_t len;

	hashcd();				/* update command hash table */

	/*
	 * If our argument is NULL, we don't know the current directory
	 * any more because we traversed a symbolic link or something
	 * we couldn't stat().
	 */
	if (dir == NULL || curdir == nullstr)  {
		setpwd(0, 1);
		return;
	}
	len = strlen(dir);
	cdcomppath = sstrdup(dir);
	STARTSTACKSTR(new);
	if (*dir != '/') {
		p = curdir;
		while (*p)
			STPUTC(*p++, new);
		if (p[-1] == '/')
			STUNPUTC(new);
	}
	while ((p = getcomponent()) != NULL) {
		if (equal(p, "..")) {
			while (new > stackblock() && (STUNPUTC(new), *new) != '/');
		} else if (*p != '\0' && ! equal(p, ".")) {
			STPUTC('/', new);
			while (*p)
				STPUTC(*p++, new);
		}
	}
	if (new == stackblock())
		STPUTC('/', new);
	STACKSTRNUL(new);
	setpwd(stackblock(), 1);
}



static int
pwdcmd(argc, argv)
	int argc;
	char **argv;
{
	out1fmt(snlfmt, curdir);
	return 0;
}




#define MAXPWD 256

/*
 * Find out what the current directory is. If we already know the current
 * directory, this routine returns immediately.
 */
static void
getpwd()
{
	char buf[MAXPWD];

	/*
	 * Things are a bit complicated here; we could have just used
	 * getcwd, but traditionally getcwd is implemented using popen
	 * to /bin/pwd. This creates a problem for us, since we cannot
	 * keep track of the job if it is being ran behind our backs.
	 * So we re-implement getcwd(), and we suppress interrupts
	 * throughout the process. This is not completely safe, since
	 * the user can still break out of it by killing the pwd program.
	 * We still try to use getcwd for systems that we know have a
	 * c implementation of getcwd, that does not open a pipe to
	 * /bin/pwd.
	 */
#if defined(__NetBSD__) || defined(__SVR4) || defined(__GLIBC__)
		
	if (getcwd(buf, sizeof(buf)) == NULL) {
		char *pwd = getenv("PWD");
		struct stat stdot, stpwd;

		if (pwd && *pwd == '/' && stat(".", &stdot) != -1 &&
		    stat(pwd, &stpwd) != -1 &&
		    stdot.st_dev == stpwd.st_dev &&
		    stdot.st_ino == stpwd.st_ino) {
			curdir = savestr(pwd);
			return;
		}
		error("getcwd() failed: %s", strerror(errno));
	}
	curdir = savestr(buf);
#else
	{
		char *p;
		int i;
		int status;
		struct job *jp;
		int pip[2];

		if (pipe(pip) < 0)
			error("Pipe call failed");
		jp = makejob((union node *)NULL, 1);
		if (forkshell(jp, (union node *)NULL, FORK_NOJOB) == 0) {
			(void) close(pip[0]);
			if (pip[1] != 1) {
				close(1);
				dup_as_newfd(pip[1], 1);
				close(pip[1]);
			}
			(void) execl("/bin/pwd", "pwd", (char *)0);
			error("Cannot exec /bin/pwd");
		}
		(void) close(pip[1]);
		pip[1] = -1;
		p = buf;
		while ((i = read(pip[0], p, buf + MAXPWD - p)) > 0
		     || (i == -1 && errno == EINTR)) {
			if (i > 0)
				p += i;
		}
		(void) close(pip[0]);
		pip[0] = -1;
		status = waitforjob(jp);
		if (status != 0)
			error((char *)0);
		if (i < 0 || p == buf || p[-1] != '\n')
			error("pwd command failed");
		p[-1] = '\0';
	}
	curdir = savestr(buf);
#endif
}

static void
setpwd(const char *val, int setold)
{
	if (setold) {
		setvar("OLDPWD", curdir, VEXPORT);
	}
	INTOFF;
	if (curdir != nullstr) {
		free(curdir);
		curdir = nullstr;
	}
	if (!val) {
		getpwd();
	} else {
		curdir = savestr(val);
	}
	INTON;
	setvar("PWD", curdir, VEXPORT);
}

/*	$NetBSD: error.c,v 1.23 2000/07/03 03:26:19 matt Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

/*
 * Errors and exceptions.
 */

/*
 * Code to handle exceptions in C.
 */

struct jmploc *handler;
static int exception;
volatile int suppressint;
volatile int intpending;


static void exverror __P((int, const char *, va_list))
    __attribute__((__noreturn__));

/*
 * Called to raise an exception.  Since C doesn't include exceptions, we
 * just do a longjmp to the exception handler.  The type of exception is
 * stored in the global variable "exception".
 */

static void
exraise(e)
	int e;
{
#ifdef DEBUG
	if (handler == NULL)
		abort();
#endif
	exception = e;
	longjmp(handler->loc, 1);
}


/*
 * Called from trap.c when a SIGINT is received.  (If the user specifies
 * that SIGINT is to be trapped or ignored using the trap builtin, then
 * this routine is not called.)  Suppressint is nonzero when interrupts
 * are held using the INTOFF macro.  The call to _exit is necessary because
 * there is a short period after a fork before the signal handlers are
 * set to the appropriate value for the child.  (The test for iflag is
 * just defensive programming.)
 */

static void
onint() {
	sigset_t mysigset;

	if (suppressint) {
		intpending++;
		return;
	}
	intpending = 0;
	sigemptyset(&mysigset);
	sigprocmask(SIG_SETMASK, &mysigset, NULL);
	if (rootshell && iflag)
		exraise(EXINT);
	else {
		signal(SIGINT, SIG_DFL);
		raise(SIGINT);
	}
	/* NOTREACHED */
}


/*
 * Exverror is called to raise the error exception.  If the first argument
 * is not NULL then error prints an error message using printf style
 * formatting.  It then raises the error exception.
 */
static void
exverror(cond, msg, ap)
	int cond;
	const char *msg;
	va_list ap;
{
	CLEAR_PENDING_INT;
	INTOFF;

#ifdef DEBUG
	if (msg)
		TRACE(("exverror(%d, \"%s\") pid=%d\n", cond, msg, getpid()));
	else
		TRACE(("exverror(%d, NULL) pid=%d\n", cond, getpid()));
#endif
	if (msg) {
		if (commandname)
			outfmt(&errout, "%s: ", commandname);
		doformat(&errout, msg, ap);
#if FLUSHERR
		outc('\n', &errout);
#else
		outcslow('\n', &errout);
#endif
	}
	flushall();
	exraise(cond);
	/* NOTREACHED */
}


#ifdef __STDC__
static void
error(const char *msg, ...)
#else
static void
error(va_alist)
	va_dcl
#endif
{
#ifndef __STDC__
	const char *msg;
#endif
	va_list ap;
#ifdef __STDC__
	va_start(ap, msg);
#else
	va_start(ap);
	msg = va_arg(ap, const char *);
#endif
	exverror(EXERROR, msg, ap);
	/* NOTREACHED */
	va_end(ap);
}


#ifdef __STDC__
static void
exerror(int cond, const char *msg, ...)
#else
static void
exerror(va_alist)
	va_dcl
#endif
{
#ifndef __STDC__
	int cond;
	const char *msg;
#endif
	va_list ap;
#ifdef __STDC__
	va_start(ap, msg);
#else
	va_start(ap);
	cond = va_arg(ap, int);
	msg = va_arg(ap, const char *);
#endif
	exverror(cond, msg, ap);
	/* NOTREACHED */
	va_end(ap);
}



/*
 * Table of error messages.
 */

struct errname {
	short errcode;		/* error number */
	short action;		/* operation which encountered the error */
	const char *msg;	/* text describing the error */
};


#define ALL (E_OPEN|E_CREAT|E_EXEC)

static const struct errname errormsg[] = {
	{ EINTR,	ALL,	"interrupted" },
	{ EACCES,	ALL,	"permission denied" },
	{ EIO,		ALL,	"I/O error" },
	{ ENOENT,	E_OPEN,	"no such file" },
	{ ENOENT,	E_CREAT,"directory nonexistent" },
	{ ENOENT,	E_EXEC,	"not found" },
	{ ENOTDIR,	E_OPEN,	"no such file" },
	{ ENOTDIR,	E_CREAT,"directory nonexistent" },
	{ ENOTDIR,	E_EXEC,	"not found" },
	{ EISDIR,	ALL,	"is a directory" },
	{ EEXIST,	E_CREAT,"file exists" },
#ifdef notdef
	{ EMFILE,	ALL,	"too many open files" },
#endif
	{ ENFILE,	ALL,	"file table overflow" },
	{ ENOSPC,	ALL,	"file system full" },
#ifdef EDQUOT
	{ EDQUOT,	ALL,	"disk quota exceeded" },
#endif
#ifdef ENOSR
	{ ENOSR,	ALL,	"no streams resources" },
#endif
	{ ENXIO,	ALL,	"no such device or address" },
	{ EROFS,	ALL,	"read-only file system" },
	{ ETXTBSY,	ALL,	"text busy" },
#ifdef SYSV
	{ EAGAIN,	E_EXEC,	"not enough memory" },
#endif
	{ ENOMEM,	ALL,	"not enough memory" },
#ifdef ENOLINK
	{ ENOLINK,	ALL,	"remote access failed" },
#endif
#ifdef EMULTIHOP
	{ EMULTIHOP,	ALL,	"remote access failed" },
#endif
#ifdef ECOMM
	{ ECOMM,	ALL,	"remote access failed" },
#endif
#ifdef ESTALE
	{ ESTALE,	ALL,	"remote access failed" },
#endif
#ifdef ETIMEDOUT
	{ ETIMEDOUT,	ALL,	"remote access failed" },
#endif
#ifdef ELOOP
	{ ELOOP,	ALL,	"symbolic link loop" },
#endif
	{ E2BIG,	E_EXEC,	"argument list too long" },
#ifdef ELIBACC
	{ ELIBACC,	E_EXEC,	"shared library missing" },
#endif
	{ 0,		0,	NULL },
};


/*
 * Return a string describing an error.  The returned string may be a
 * pointer to a static buffer that will be overwritten on the next call.
 * Action describes the operation that got the error.
 */

static const char *
errmsg(e, action)
	int e;
	int action;
{
	struct errname const *ep;
	static char buf[12];

	for (ep = errormsg ; ep->errcode ; ep++) {
		if (ep->errcode == e && (ep->action & action) != 0)
			return ep->msg;
	}
	fmtstr(buf, sizeof buf, "error %d", e);
	return buf;
}


#ifdef REALLY_SMALL
static void
__inton() {
	if (--suppressint == 0 && intpending) {
		onint();
	}
}
#endif
/*	$NetBSD: eval.c,v 1.57 2001/02/04 19:52:06 christos Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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


/* flags in argument to evaltree */
#define EV_EXIT 01		/* exit after evaluating tree */
#define EV_TESTED 02		/* exit status is checked; ignore -e flag */
#define EV_BACKCMD 04		/* command executing within back quotes */

static int evalskip;			/* set if we are skipping commands */
static int skipcount;		/* number of levels to skip */
static int loopnest;		/* current loop nesting level */
static int funcnest;			/* depth of function calls */


static char *commandname;
struct strlist *cmdenviron;
static int exitstatus;			/* exit status of last command */
static int oexitstatus;		/* saved exit status */


static void evalloop __P((union node *, int));
static void evalfor __P((union node *, int));
static void evalcase __P((union node *, int));
static void evalsubshell __P((union node *, int));
static void expredir __P((union node *));
static void evalpipe __P((union node *));
#ifdef notyet
static void evalcommand __P((union node *, int, struct backcmd *));
#else
static void evalcommand __P((union node *, int));
#endif
static void prehash __P((union node *));
static void eprintlist __P((struct strlist *));


/*
 * Called to reset things after an exception.
 */

#ifdef mkinit
INCLUDE "eval.h"

RESET {
	evalskip = 0;
	loopnest = 0;
	funcnest = 0;
}

SHELLPROC {
	exitstatus = 0;
}
#endif



/*
 * The eval commmand.
 */

static int
evalcmd(argc, argv)
	int argc;
	char **argv;
{
        char *p;
        char *concat;
        char **ap;

        if (argc > 1) {
                p = argv[1];
                if (argc > 2) {
                        STARTSTACKSTR(concat);
                        ap = argv + 2;
                        for (;;) {
                                while (*p)
                                        STPUTC(*p++, concat);
                                if ((p = *ap++) == NULL)
                                        break;
                                STPUTC(' ', concat);
                        }
                        STPUTC('\0', concat);
                        p = grabstackstr(concat);
                }
                evalstring(p, EV_TESTED);
        }
        return exitstatus;
}


/*
 * Execute a command or commands contained in a string.
 */

static void
evalstring(s, flag)
	char *s;
	int flag;
	{
	union node *n;
	struct stackmark smark;

	setstackmark(&smark);
	setinputstring(s);
	while ((n = parsecmd(0)) != NEOF) {
		evaltree(n, flag);
		popstackmark(&smark);
	}
	popfile();
	popstackmark(&smark);
}



/*
 * Evaluate a parse tree.  The value is left in the global variable
 * exitstatus.
 */

static void
evaltree(n, flags)
	union node *n;
	int flags;
{
	int checkexit = 0;
	if (n == NULL) {
		TRACE(("evaltree(NULL) called\n"));
		goto out;
	}
	TRACE(("evaltree(0x%lx: %d) called\n", (long)n, n->type));
	switch (n->type) {
	case NSEMI:
		evaltree(n->nbinary.ch1, flags & EV_TESTED);
		if (evalskip)
			goto out;
		evaltree(n->nbinary.ch2, flags);
		break;
	case NAND:
		evaltree(n->nbinary.ch1, EV_TESTED);
		if (evalskip || exitstatus != 0)
			goto out;
		evaltree(n->nbinary.ch2, flags);
		break;
	case NOR:
		evaltree(n->nbinary.ch1, EV_TESTED);
		if (evalskip || exitstatus == 0)
			goto out;
		evaltree(n->nbinary.ch2, flags);
		break;
	case NREDIR:
		expredir(n->nredir.redirect);
		redirect(n->nredir.redirect, REDIR_PUSH);
		evaltree(n->nredir.n, flags);
		popredir();
		break;
	case NSUBSHELL:
		evalsubshell(n, flags);
		break;
	case NBACKGND:
		evalsubshell(n, flags);
		break;
	case NIF: {
		evaltree(n->nif.test, EV_TESTED);
		if (evalskip)
			goto out;
		if (exitstatus == 0)
			evaltree(n->nif.ifpart, flags);
		else if (n->nif.elsepart)
			evaltree(n->nif.elsepart, flags);
		else
			exitstatus = 0;
		break;
	}
	case NWHILE:
	case NUNTIL:
		evalloop(n, flags);
		break;
	case NFOR:
		evalfor(n, flags);
		break;
	case NCASE:
		evalcase(n, flags);
		break;
	case NDEFUN: {
		struct builtincmd *bcmd;
		if (
			(bcmd = find_builtin(n->narg.text)) &&
			bcmd->flags & BUILTIN_SPECIAL
		) {
			outfmt(out2, "%s is a special built-in\n", n->narg.text);
			exitstatus = 1;
			break;
		}
		defun(n->narg.text, n->narg.next);
		exitstatus = 0;
		break;
	}
	case NNOT:
		evaltree(n->nnot.com, EV_TESTED);
		exitstatus = !exitstatus;
		break;

	case NPIPE:
		evalpipe(n);
		checkexit = 1;
		break;
	case NCMD:
#ifdef notyet
		evalcommand(n, flags, (struct backcmd *)NULL);
#else
		evalcommand(n, flags);
#endif
		checkexit = 1;
		break;
#ifdef DEBUG
	default:
		out1fmt("Node type = %d\n", n->type);
#ifndef USE_GLIBC_STDIO
		flushout(out1);
#endif
		break;
#endif
	}
out:
	if (pendingsigs)
		dotrap();
	if (
		flags & EV_EXIT ||
		(checkexit && eflag && exitstatus && !(flags & EV_TESTED))
	)
		exitshell(exitstatus);
}


static void
evalloop(n, flags)
	union node *n;
	int flags;
{
	int status;

	loopnest++;
	status = 0;
	for (;;) {
		evaltree(n->nbinary.ch1, EV_TESTED);
		if (evalskip) {
skipping:	  if (evalskip == SKIPCONT && --skipcount <= 0) {
				evalskip = 0;
				continue;
			}
			if (evalskip == SKIPBREAK && --skipcount <= 0)
				evalskip = 0;
			break;
		}
		if (n->type == NWHILE) {
			if (exitstatus != 0)
				break;
		} else {
			if (exitstatus == 0)
				break;
		}
		evaltree(n->nbinary.ch2, flags & EV_TESTED);
		status = exitstatus;
		if (evalskip)
			goto skipping;
	}
	loopnest--;
	exitstatus = status;
}



static void
evalfor(n, flags)
    union node *n;
    int flags;
{
	struct arglist arglist;
	union node *argp;
	struct strlist *sp;
	struct stackmark smark;

	setstackmark(&smark);
	arglist.lastp = &arglist.list;
	for (argp = n->nfor.args ; argp ; argp = argp->narg.next) {
		oexitstatus = exitstatus;
		expandarg(argp, &arglist, EXP_FULL | EXP_TILDE | EXP_RECORD);
		if (evalskip)
			goto out;
	}
	*arglist.lastp = NULL;

	exitstatus = 0;
	loopnest++;
	for (sp = arglist.list ; sp ; sp = sp->next) {
		setvar(n->nfor.var, sp->text, 0);
		evaltree(n->nfor.body, flags & EV_TESTED);
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
evalcase(n, flags)
	union node *n;
	int flags;
{
	union node *cp;
	union node *patp;
	struct arglist arglist;
	struct stackmark smark;

	setstackmark(&smark);
	arglist.lastp = &arglist.list;
	oexitstatus = exitstatus;
	expandarg(n->ncase.expr, &arglist, EXP_TILDE);
	for (cp = n->ncase.cases ; cp && evalskip == 0 ; cp = cp->nclist.next) {
		for (patp = cp->nclist.pattern ; patp ; patp = patp->narg.next) {
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
evalsubshell(n, flags)
	union node *n;
	int flags;
{
	struct job *jp;
	int backgnd = (n->type == NBACKGND);

	expredir(n->nredir.redirect);
	jp = makejob(n, 1);
	if (forkshell(jp, n, backgnd) == 0) {
		if (backgnd)
			flags &=~ EV_TESTED;
		redirect(n->nredir.redirect, 0);
		evaltree(n->nredir.n, flags | EV_EXIT);	/* never returns */
	}
	if (! backgnd) {
		INTOFF;
		exitstatus = waitforjob(jp);
		INTON;
	}
}



/*
 * Compute the names of the files in a redirection list.
 */

static void
expredir(n)
	union node *n;
{
	union node *redir;

	for (redir = n ; redir ; redir = redir->nfile.next) {
		struct arglist fn;
		fn.lastp = &fn.list;
		oexitstatus = exitstatus;
		switch (redir->type) {
		case NFROMTO:
		case NFROM:
		case NTO:
		case NAPPEND:
		case NTOOV:
			expandarg(redir->nfile.fname, &fn, EXP_TILDE | EXP_REDIR);
			redir->nfile.expfname = fn.list->text;
			break;
		case NFROMFD:
		case NTOFD:
			if (redir->ndup.vname) {
				expandarg(redir->ndup.vname, &fn, EXP_FULL | EXP_TILDE);
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
evalpipe(n)
	union node *n;
{
	struct job *jp;
	struct nodelist *lp;
	int pipelen;
	int prevfd;
	int pip[2];

	TRACE(("evalpipe(0x%lx) called\n", (long)n));
	pipelen = 0;
	for (lp = n->npipe.cmdlist ; lp ; lp = lp->next)
		pipelen++;
	INTOFF;
	jp = makejob(n, pipelen);
	prevfd = -1;
	for (lp = n->npipe.cmdlist ; lp ; lp = lp->next) {
		prehash(lp->n);
		pip[1] = -1;
		if (lp->next) {
			if (pipe(pip) < 0) {
				close(prevfd);
				error("Pipe call failed");
			}
		}
		if (forkshell(jp, lp->n, n->npipe.backgnd) == 0) {
			INTON;
			if (prevfd > 0) {
				close(0);
				dup_as_newfd(prevfd, 0);
				close(prevfd);
				if (pip[0] == 0) {
					pip[0] = -1;
				}
			}
			if (pip[1] >= 0) {
				if (pip[0] >= 0) {
					close(pip[0]);
				}
				if (pip[1] != 1) {
					close(1);
					dup_as_newfd(pip[1], 1);
					close(pip[1]);
				}
			}
			evaltree(lp->n, EV_EXIT);
		}
		if (prevfd >= 0)
			close(prevfd);
		prevfd = pip[0];
		close(pip[1]);
	}
	INTON;
	if (n->npipe.backgnd == 0) {
		INTOFF;
		exitstatus = waitforjob(jp);
		TRACE(("evalpipe:  job done exit status %d\n", exitstatus));
		INTON;
	}
}



/*
 * Execute a command inside back quotes.  If it's a builtin command, we
 * want to save its output in a block obtained from malloc.  Otherwise
 * we fork off a subprocess and get the output of the command via a pipe.
 * Should be called with interrupts off.
 */

static void
evalbackcmd(n, result)
	union node *n;
	struct backcmd *result;
{
	int pip[2];
	struct job *jp;
	struct stackmark smark;		/* unnecessary */

	setstackmark(&smark);
	result->fd = -1;
	result->buf = NULL;
	result->nleft = 0;
	result->jp = NULL;
	if (n == NULL) {
		exitstatus = 0;
		goto out;
	}
#ifdef notyet
	/*
	 * For now we disable executing builtins in the same
	 * context as the shell, because we are not keeping
	 * enough state to recover from changes that are
	 * supposed only to affect subshells. eg. echo "`cd /`"
	 */
	if (n->type == NCMD) {
		exitstatus = oexitstatus;
		evalcommand(n, EV_BACKCMD, result);
	} else
#endif
	{
		exitstatus = 0;
		if (pipe(pip) < 0)
			error("Pipe call failed");
		jp = makejob(n, 1);
		if (forkshell(jp, n, FORK_NOJOB) == 0) {
			FORCEINTON;
			close(pip[0]);
			if (pip[1] != 1) {
				close(1);
				dup_as_newfd(pip[1], 1);
				close(pip[1]);
			}
			eflag = 0;
			evaltree(n, EV_EXIT);
		}
		close(pip[1]);
		result->fd = pip[0];
		result->jp = jp;
	}
out:
	popstackmark(&smark);
	TRACE(("evalbackcmd done: fd=%d buf=0x%x nleft=%d jp=0x%x\n",
		result->fd, result->buf, result->nleft, result->jp));
}



/*
 * Execute a simple command.
 */

static void
#ifdef notyet
evalcommand(cmd, flags, backcmd)
	union node *cmd;
	int flags;
	struct backcmd *backcmd;
#else
evalcommand(cmd, flags)
	union node *cmd;
	int flags;
#endif
{
	struct stackmark smark;
	union node *argp;
	struct arglist arglist;
	struct arglist varlist;
	char **argv;
	int argc;
	char **envp;
	struct strlist *sp;
	int mode;
#ifdef notyet
	int pip[2];
#endif
	struct cmdentry cmdentry;
	struct job *jp;
	char *volatile savecmdname;
	volatile struct shparam saveparam;
	struct localvar *volatile savelocalvars;
	volatile int e;
	char *lastarg;
	const char *path;
	const struct builtincmd *firstbltin;
	struct jmploc *volatile savehandler;
	struct jmploc jmploc;
#if __GNUC__
	/* Avoid longjmp clobbering */
	(void) &argv;
	(void) &argc;
	(void) &lastarg;
	(void) &flags;
#endif

	/* First expand the arguments. */
	TRACE(("evalcommand(0x%lx, %d) called\n", (long)cmd, flags));
	setstackmark(&smark);
	arglist.lastp = &arglist.list;
	varlist.lastp = &varlist.list;
	arglist.list = 0;
	oexitstatus = exitstatus;
	exitstatus = 0;
	path = pathval();
	for (argp = cmd->ncmd.assign; argp; argp = argp->narg.next) {
		expandarg(argp, &varlist, EXP_VARTILDE);
	}
	for (
		argp = cmd->ncmd.args; argp && !arglist.list;
		argp = argp->narg.next
	) {
		expandarg(argp, &arglist, EXP_FULL | EXP_TILDE);
	}
	if (argp) {
		struct builtincmd *bcmd;
		bool pseudovarflag;
		bcmd = find_builtin(arglist.list->text);
		pseudovarflag = bcmd && bcmd->flags & BUILTIN_ASSIGN;
		for (; argp; argp = argp->narg.next) {
			if (pseudovarflag && isassignment(argp->narg.text)) {
				expandarg(argp, &arglist, EXP_VARTILDE);
				continue;
			}
			expandarg(argp, &arglist, EXP_FULL | EXP_TILDE);
		}
	}
	*arglist.lastp = NULL;
	*varlist.lastp = NULL;
	expredir(cmd->ncmd.redirect);
	argc = 0;
	for (sp = arglist.list ; sp ; sp = sp->next)
		argc++;
	argv = stalloc(sizeof (char *) * (argc + 1));

	for (sp = arglist.list ; sp ; sp = sp->next) {
		TRACE(("evalcommand arg: %s\n", sp->text));
		*argv++ = sp->text;
	}
	*argv = NULL;
	lastarg = NULL;
	if (iflag && funcnest == 0 && argc > 0)
		lastarg = argv[-1];
	argv -= argc;

	/* Print the command if xflag is set. */
	if (xflag) {
#ifdef FLUSHERR
		outc('+', &errout);
#else
		outcslow('+', &errout);
#endif
		eprintlist(varlist.list);
		eprintlist(arglist.list);
#ifdef FLUSHERR
		outc('\n', &errout);
		flushout(&errout);
#else
		outcslow('\n', &errout);
#endif
	}

	/* Now locate the command. */
	if (argc == 0) {
		cmdentry.cmdtype = CMDBUILTIN;
		firstbltin = cmdentry.u.cmd = BLTINCMD;
	} else {
		const char *oldpath;
		int findflag = DO_ERR;
		int oldfindflag;

		/*
		 * Modify the command lookup path, if a PATH= assignment
		 * is present
		 */
		for (sp = varlist.list ; sp ; sp = sp->next)
			if (varequal(sp->text, defpathvar)) {
				path = sp->text + 5;
				findflag |= DO_BRUTE;
			}
		oldpath = path;
		oldfindflag = findflag;
		firstbltin = 0;
		for(;;) {
			find_command(argv[0], &cmdentry, findflag, path);
			if (cmdentry.cmdtype == CMDUNKNOWN) {	/* command not found */
				exitstatus = 127;
#ifdef FLUSHERR
				flushout(&errout);
#endif
				goto out;
			}
			/* implement bltin and command here */
			if (cmdentry.cmdtype != CMDBUILTIN) {
				break;
			}
			if (!firstbltin) {
				firstbltin = cmdentry.u.cmd;
			}
			if (cmdentry.u.cmd == BLTINCMD) {
				for(;;) {
					struct builtincmd *bcmd;

					argv++;
					if (--argc == 0)
						goto found;
					if (!(bcmd = find_builtin(*argv))) {
						outfmt(&errout, "%s: not found\n", *argv);
						exitstatus = 127;
#ifdef FLUSHERR
						flushout(&errout);
#endif
						goto out;
					}
					cmdentry.u.cmd = bcmd;
					if (bcmd != BLTINCMD)
						break;
				}
			}
			if (cmdentry.u.cmd == COMMANDCMD) {
				argv++;
				if (--argc == 0) {
					goto found;
				}
				if (*argv[0] == '-') {
					if (!equal(argv[0], "-p")) {
						argv--;
						argc++;
						break;
					}
					argv++;
					if (--argc == 0) {
						goto found;
					}
					path = defpath;
					findflag |= DO_BRUTE;
				} else {
					path = oldpath;
					findflag = oldfindflag;
				}
				findflag |= DO_NOFUN;
				continue;
			}
found:
			break;
		}
	}

	/* Fork off a child process if necessary. */
	if (cmd->ncmd.backgnd
	 || (cmdentry.cmdtype == CMDNORMAL && (flags & EV_EXIT) == 0)
#ifdef notyet
	 || ((flags & EV_BACKCMD) != 0
	    && (cmdentry.cmdtype != CMDBUILTIN
		 || cmdentry.u.bcmd == DOTCMD
		 || cmdentry.u.bcmd == EVALCMD))
#endif
	) {
		jp = makejob(cmd, 1);
		mode = cmd->ncmd.backgnd;
#ifdef notyet
		if (flags & EV_BACKCMD) {
			mode = FORK_NOJOB;
			if (pipe(pip) < 0)
				error("Pipe call failed");
		}
#endif
		if (forkshell(jp, cmd, mode) != 0)
			goto parent;	/* at end of routine */
#ifdef notyet
		if (flags & EV_BACKCMD) {
			FORCEINTON;
			close(pip[0]);
			if (pip[1] != 1) {
				close(1);
				dup_as_newfd(pip[1], 1);
				close(pip[1]);
			}
		}
#endif
		flags |= EV_EXIT;
	}

	/* This is the child process if a fork occurred. */
	/* Execute the command. */
	if (cmdentry.cmdtype == CMDFUNCTION) {
#ifdef DEBUG
		trputs("Shell function:  ");  trargs(argv);
#endif
		exitstatus = oexitstatus;
		redirect(cmd->ncmd.redirect, REDIR_PUSH);
		saveparam = shellparam;
		shellparam.malloc = 0;
		shellparam.nparam = argc - 1;
		shellparam.p = argv + 1;
		INTOFF;
		savelocalvars = localvars;
		localvars = NULL;
		INTON;
		if (setjmp(jmploc.loc)) {
			if (exception == EXSHELLPROC) {
				freeparam((volatile struct shparam *)
				    &saveparam);
			} else {
				saveparam.optind = shellparam.optind;
				saveparam.optoff = shellparam.optoff;
				freeparam(&shellparam);
				shellparam = saveparam;
			}
			poplocalvars();
			localvars = savelocalvars;
			handler = savehandler;
			longjmp(handler->loc, 1);
		}
		savehandler = handler;
		handler = &jmploc;
		for (sp = varlist.list ; sp ; sp = sp->next)
			mklocal(sp->text);
		funcnest++;
		evaltree(cmdentry.u.func, flags & EV_TESTED);
		funcnest--;
		INTOFF;
		poplocalvars();
		localvars = savelocalvars;
		saveparam.optind = shellparam.optind;
		saveparam.optoff = shellparam.optoff;
		freeparam(&shellparam);
		shellparam = saveparam;
		handler = savehandler;
		popredir();
		INTON;
		if (evalskip == SKIPFUNC) {
			evalskip = 0;
			skipcount = 0;
		}
		if (flags & EV_EXIT)
			exitshell(exitstatus);
	} else if (cmdentry.cmdtype == CMDBUILTIN) {
#ifdef DEBUG
		trputs("builtin command:  ");  trargs(argv);
#endif
		mode = (cmdentry.u.cmd == EXECCMD)? 0 : REDIR_PUSH;
#ifdef notyet
		if (flags == EV_BACKCMD) {
#ifdef USE_GLIBC_STDIO
			openmemout();
#else
			memout.nleft = 0;
			memout.nextc = memout.buf;
			memout.bufsize = 64;
#endif
			mode |= REDIR_BACKQ;
		}
#endif
		redirect(cmd->ncmd.redirect, mode);
		savecmdname = commandname;
		if (firstbltin->flags & BUILTIN_SPECIAL) {
			listsetvar(varlist.list);
		} else {
			cmdenviron = varlist.list;
		}
		e = -1;
		if (setjmp(jmploc.loc)) {
			e = exception;
			exitstatus = (e == EXINT)? SIGINT+128 : 2;
			goto cmddone;
		}
		savehandler = handler;
		handler = &jmploc;
		commandname = argv[0];
		argptr = argv + 1;
		optptr = NULL;			/* initialize nextopt */
		exitstatus = (*cmdentry.u.cmd->builtinfunc)(argc, argv);
		flushall();
cmddone:
		exitstatus |= outerr(out1);
		out1 = &output;
		out2 = &errout;
		freestdout();
		cmdenviron = NULL;
		if (e != EXSHELLPROC) {
			commandname = savecmdname;
			if (flags & EV_EXIT)
				exitshell(exitstatus);
		}
		handler = savehandler;
		if (e != -1) {
			if ((e != EXERROR && e != EXEXEC)
			   || cmdentry.u.cmd == BLTINCMD
			   || cmdentry.u.cmd == DOTCMD
			   || cmdentry.u.cmd == EVALCMD
			   || cmdentry.u.cmd == EXECCMD)
				exraise(e);
			FORCEINTON;
		}
		if (cmdentry.u.cmd != EXECCMD)
			popredir();
#ifdef notyet
		if (flags == EV_BACKCMD) {
			INTOFF;
#ifdef USE_GLIBC_STDIO
			if (__closememout()) {
				error(
					"__closememout() failed: %s",
					strerror(errno)
				);
			}
#endif
			backcmd->buf = memout.buf;
#ifdef USE_GLIBC_STDIO
			backcmd->nleft = memout.bufsize;
#else
			backcmd->nleft = memout.nextc - memout.buf;
#endif
			memout.buf = NULL;
			INTON;
		}
#endif
	} else {
#ifdef DEBUG
		trputs("normal command:  ");  trargs(argv);
#endif
		redirect(cmd->ncmd.redirect, 0);
		clearredir();
		for (sp = varlist.list ; sp ; sp = sp->next)
			setvareq(sp->text, VEXPORT|VSTACK);
		envp = environment();
		shellexec(argv, envp, path, cmdentry.u.index);
	}
	goto out;

parent:	/* parent process gets here (if we forked) */
	if (mode == 0) {	/* argument to fork */
		INTOFF;
		exitstatus = waitforjob(jp);
		INTON;
#ifdef notyet
	} else if (mode == 2) {
		backcmd->fd = pip[0];
		close(pip[1]);
		backcmd->jp = jp;
#endif
	}

out:
	if (lastarg)
		setvar("_", lastarg, 0);
	popstackmark(&smark);
}



/*
 * Search for a command.  This is called before we fork so that the
 * location of the command will be available in the parent as well as
 * the child.  The check for "goodname" is an overly conservative
 * check that the name will not be subject to expansion.
 */

static void
prehash(n)
	union node *n;
{
	struct cmdentry entry;

	if (n->type == NCMD && n->ncmd.args)
		if (goodname(n->ncmd.args->narg.text))
			find_command(n->ncmd.args->narg.text, &entry, 0,
				     pathval());
}



/*
 * Builtin commands.  Builtin commands whose functions are closely
 * tied to evaluation are implemented here.
 */

/*
 * No command given, or a bltin command with no arguments.  Set the
 * specified variables.
 */

int
bltincmd(argc, argv)
	int argc;
	char **argv;
{
	/*
	 * Preserve exitstatus of a previous possible redirection
	 * as POSIX mandates
	 */
	return exitstatus;
}


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
breakcmd(argc, argv)
	int argc;
	char **argv;
{
	int n = argc > 1 ? number(argv[1]) : 1;

	if (n > loopnest)
		n = loopnest;
	if (n > 0) {
		evalskip = (**argv == 'c')? SKIPCONT : SKIPBREAK;
		skipcount = n;
	}
	return 0;
}


/*
 * The return command.
 */

static int
returncmd(argc, argv)
	int argc;
	char **argv;
{
	int ret = argc > 1 ? number(argv[1]) : oexitstatus;

	if (funcnest) {
		evalskip = SKIPFUNC;
		skipcount = 1;
		return ret;
	}
	else {
		/* Do what ksh does; skip the rest of the file */
		evalskip = SKIPFILE;
		skipcount = 1;
		return ret;
	}
}


#ifndef BB_TRUE_FALSE
static int
false_main(argc, argv)
	int argc;
	char **argv;
{
	return 1;
}


static int
true_main(argc, argv)
	int argc;
	char **argv;
{
	return 0;
}
#endif

static int
execcmd(argc, argv)
	int argc;
	char **argv;
{
	if (argc > 1) {
		struct strlist *sp;

		iflag = 0;		/* exit on error */
		mflag = 0;
		optschanged();
		for (sp = cmdenviron; sp ; sp = sp->next)
			setvareq(sp->text, VEXPORT|VSTACK);
		shellexec(argv + 1, environment(), pathval(), 0);
	}
	return 0;
}

static void
eprintlist(struct strlist *sp)
{
	for (; sp; sp = sp->next) {
		outfmt(&errout, " %s",sp->text);
	}
}
/*	$NetBSD: exec.c,v 1.32 2001/02/04 19:52:06 christos Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

/*
 * When commands are first encountered, they are entered in a hash table.
 * This ensures that a full path search will not have to be done for them
 * on each invocation.
 *
 * We should investigate converting to a linear search, even though that
 * would make the command name "hash" a misnomer.
 */
#define CMDTABLESIZE 31		/* should be prime */
#define ARB 1			/* actual size determined at run time */



struct tblentry {
	struct tblentry *next;	/* next entry in hash chain */
	union param param;	/* definition of builtin function */
	short cmdtype;		/* index identifying command */
	char rehash;		/* if set, cd done since entry created */
	char cmdname[ARB];	/* name of command */
};


static struct tblentry *cmdtable[CMDTABLESIZE];
static int builtinloc = -1;		/* index in path of %builtin, or -1 */
static int exerrno = 0;			/* Last exec error */


static void tryexec __P((char *, char **, char **));
#if !defined(BSD) && !defined(linux)
static void execinterp __P((char **, char **));
#endif
static void printentry __P((struct tblentry *, int));
static void clearcmdentry __P((int));
static struct tblentry *cmdlookup __P((char *, int));
static void delete_cmd_entry __P((void));
#ifdef ASH_TYPE
static int describe_command __P((char *, int));
#endif
static int path_change __P((const char *, int *));


/*
 * Exec a program.  Never returns.  If you change this routine, you may
 * have to change the find_command routine as well.
 */

static void
shellexec(argv, envp, path, idx)
	char **argv, **envp;
	const char *path;
	int idx;
{
	char *cmdname;
	int e;

	if (strchr(argv[0], '/') != NULL) {
		tryexec(argv[0], argv, envp);
		e = errno;
	} else {
		e = ENOENT;
		while ((cmdname = padvance(&path, argv[0])) != NULL) {
			if (--idx < 0 && pathopt == NULL) {
				tryexec(cmdname, argv, envp);
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
	exerror(EXEXEC, "%s: %s", argv[0], errmsg(e, E_EXEC));
	/* NOTREACHED */
}


static void
tryexec(cmd, argv, envp)
	char *cmd;
	char **argv;
	char **envp;
	{
	int e;
#if !defined(BSD) && !defined(linux)
	char *p;
#endif

#ifdef SYSV
	do {
		execve(cmd, argv, envp);
	} while (errno == EINTR);
#else
	execve(cmd, argv, envp);
#endif
	e = errno;
	if (e == ENOEXEC) {
		INTOFF;
		initshellproc();
		setinputfile(cmd, 0);
		commandname = arg0 = savestr(argv[0]);
#if !defined(BSD) && !defined(linux)
		INTON;
		pgetc(); pungetc();		/* fill up input buffer */
		p = parsenextc;
		if (parsenleft > 2 && p[0] == '#' && p[1] == '!') {
			argv[0] = cmd;
			execinterp(argv, envp);
		}
		INTOFF;
#endif
		setparam(argv + 1);
		exraise(EXSHELLPROC);
	}
	errno = e;
}


#if !defined(BSD) && !defined(linux)
/*
 * Execute an interpreter introduced by "#!", for systems where this
 * feature has not been built into the kernel.  If the interpreter is
 * the shell, return (effectively ignoring the "#!").  If the execution
 * of the interpreter fails, exit.
 *
 * This code peeks inside the input buffer in order to avoid actually
 * reading any input.  It would benefit from a rewrite.
 */

#define NEWARGS 5

static void
execinterp(argv, envp)
	char **argv, **envp;
	{
	int n;
	char *inp;
	char *outp;
	char c;
	char *p;
	char **ap;
	char *newargs[NEWARGS];
	int i;
	char **ap2;
	char **new;

	n = parsenleft - 2;
	inp = parsenextc + 2;
	ap = newargs;
	for (;;) {
		while (--n >= 0 && (*inp == ' ' || *inp == '\t'))
			inp++;
		if (n < 0)
			goto bad;
		if ((c = *inp++) == '\n')
			break;
		if (ap == &newargs[NEWARGS])
bad:		  error("Bad #! line");
		STARTSTACKSTR(outp);
		do {
			STPUTC(c, outp);
		} while (--n >= 0 && (c = *inp++) != ' ' && c != '\t' && c != '\n');
		STPUTC('\0', outp);
		n++, inp--;
		*ap++ = grabstackstr(outp);
	}
	if (ap == newargs + 1) {	/* if no args, maybe no exec is needed */
		p = newargs[0];
		for (;;) {
			if (equal(p, "sh") || equal(p, "ash")) {
				return;
			}
			while (*p != '/') {
				if (*p == '\0')
					goto break2;
				p++;
			}
			p++;
		}
break2:;
	}
	i = (char *)ap - (char *)newargs;		/* size in bytes */
	if (i == 0)
		error("Bad #! line");
	for (ap2 = argv ; *ap2++ != NULL ; );
	new = ckmalloc(i + ((char *)ap2 - (char *)argv));
	ap = newargs, ap2 = new;
	while ((i -= sizeof (char **)) >= 0)
		*ap2++ = *ap++;
	ap = argv;
	while (*ap2++ = *ap++);
	shellexec(new, envp, pathval(), 0);
	/* NOTREACHED */
}
#endif



/*
 * Do a path search.  The variable path (passed by reference) should be
 * set to the start of the path before the first call; padvance will update
 * this value as it proceeds.  Successive calls to padvance will return
 * the possible path expansions in sequence.  If an option (indicated by
 * a percent sign) appears in the path entry then the global variable
 * pathopt will be set to point to it; otherwise pathopt will be set to
 * NULL.
 */

static const char *pathopt;

static char *
padvance(path, name)
	const char **path;
	const char *name;
	{
	const char *p;
	char *q;
	const char *start;
	int len;

	if (*path == NULL)
		return NULL;
	start = *path;
	for (p = start ; *p && *p != ':' && *p != '%' ; p++);
	len = p - start + strlen(name) + 2;	/* "2" is for '/' and '\0' */
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
		while (*p && *p != ':')  p++;
	}
	if (*p == ':')
		*path = p + 1;
	else
		*path = NULL;
	return stalloc(len);
}



/*** Command hashing code ***/


static int
hashcmd(argc, argv)
	int argc;
	char **argv;
{
	struct tblentry **pp;
	struct tblentry *cmdp;
	int c;
	int verbose;
	struct cmdentry entry;
	char *name;

	verbose = 0;
	while ((c = nextopt("rv")) != '\0') {
		if (c == 'r') {
			clearcmdentry(0);
			return 0;
		} else if (c == 'v') {
			verbose++;
		}
	}
	if (*argptr == NULL) {
		for (pp = cmdtable ; pp < &cmdtable[CMDTABLESIZE] ; pp++) {
			for (cmdp = *pp ; cmdp ; cmdp = cmdp->next) {
				if (cmdp->cmdtype != CMDBUILTIN) {
					printentry(cmdp, verbose);
				}
			}
		}
		return 0;
	}
	c = 0;
	while ((name = *argptr) != NULL) {
		if ((cmdp = cmdlookup(name, 0)) != NULL
		 && (cmdp->cmdtype == CMDNORMAL
		     || (cmdp->cmdtype == CMDBUILTIN && builtinloc >= 0)))
			delete_cmd_entry();
		find_command(name, &entry, DO_ERR, pathval());
		if (entry.cmdtype == CMDUNKNOWN) c = 1;
		else if (verbose) {
			cmdp = cmdlookup(name, 0);
			if (cmdp) printentry(cmdp, verbose);
			flushall();
		}
		argptr++;
	}
	return c;
}


static void
printentry(cmdp, verbose)
	struct tblentry *cmdp;
	int verbose;
	{
	int idx;
	const char *path;
	char *name;

	if (cmdp->cmdtype == CMDNORMAL) {
		idx = cmdp->param.index;
		path = pathval();
		do {
			name = padvance(&path, cmdp->cmdname);
			stunalloc(name);
		} while (--idx >= 0);
		out1str(name);
	} else if (cmdp->cmdtype == CMDBUILTIN) {
		out1fmt("builtin %s", cmdp->cmdname);
	} else if (cmdp->cmdtype == CMDFUNCTION) {
		out1fmt("function %s", cmdp->cmdname);
		if (verbose) {
			INTOFF;
			name = commandtext(cmdp->param.func);
			out1fmt(" %s", name);
			ckfree(name);
			INTON;
		}
#ifdef DEBUG
	} else {
		error("internal error: cmdtype %d", cmdp->cmdtype);
#endif
	}
	out1fmt(snlfmt, cmdp->rehash ? "*" : nullstr);
}



/*
 * Resolve a command name.  If you change this routine, you may have to
 * change the shellexec routine as well.
 */

static void
find_command(name, entry, act, path)
	char *name;
	struct cmdentry *entry;
	int act;
	const char *path;
{
	struct tblentry *cmdp;
	int idx;
	int prev;
	char *fullname;
	struct stat statb;
	int e;
	int bltin;
	int firstchange;
	int updatetbl;
	bool regular;
	struct builtincmd *bcmd;

	/* If name contains a slash, don't use the hash table */
	if (strchr(name, '/') != NULL) {
		if (act & DO_ABS) {
			while (stat(name, &statb) < 0) {
	#ifdef SYSV
				if (errno == EINTR)
					continue;
	#endif
				if (errno != ENOENT && errno != ENOTDIR)
					e = errno;
				entry->cmdtype = CMDUNKNOWN;
				entry->u.index = -1;
				return;
			}
			entry->cmdtype = CMDNORMAL;
			entry->u.index = -1;
			return;
		}
		entry->cmdtype = CMDNORMAL;
		entry->u.index = 0;
		return;
	}

	updatetbl = 1;
	if (act & DO_BRUTE) {
		firstchange = path_change(path, &bltin);
	} else {
		bltin = builtinloc;
		firstchange = 9999;
	}

	/* If name is in the table, and not invalidated by cd, we're done */
	if ((cmdp = cmdlookup(name, 0)) != NULL && cmdp->rehash == 0) {
		if (cmdp->cmdtype == CMDFUNCTION) {
			if (act & DO_NOFUN) {
				updatetbl = 0;
			} else {
				goto success;
			}
		} else if (act & DO_BRUTE) {
			if ((cmdp->cmdtype == CMDNORMAL &&
			     cmdp->param.index >= firstchange) ||
			    (cmdp->cmdtype == CMDBUILTIN &&
			     ((builtinloc < 0 && bltin >= 0) ?
			      bltin : builtinloc) >= firstchange)) {
				/* need to recompute the entry */
			} else {
				goto success;
			}
		} else {
			goto success;
		}
	}

	bcmd = find_builtin(name);
	regular = bcmd && bcmd->flags & BUILTIN_REGULAR;

	if (regular) {
		if (cmdp && (cmdp->cmdtype == CMDBUILTIN)) {
		    	goto success;
		}
	} else if (act & DO_BRUTE) {
		if (firstchange == 0) {
			updatetbl = 0;
		}
	}

	/* If %builtin not in path, check for builtin next */
	if (regular || (bltin < 0 && bcmd)) {
builtin:
		if (!updatetbl) {
			entry->cmdtype = CMDBUILTIN;
			entry->u.cmd = bcmd;
			return;
		}
		INTOFF;
		cmdp = cmdlookup(name, 1);
		cmdp->cmdtype = CMDBUILTIN;
		cmdp->param.cmd = bcmd;
		INTON;
		goto success;
	}

	/* We have to search path. */
	prev = -1;		/* where to start */
	if (cmdp && cmdp->rehash) {	/* doing a rehash */
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
		idx++;
		if (idx >= firstchange) {
			updatetbl = 0;
		}
		if (pathopt) {
			if (prefix("builtin", pathopt)) {
				if ((bcmd = find_builtin(name))) {
					goto builtin;
				}
				continue;
			} else if (!(act & DO_NOFUN) &&
				   prefix("func", pathopt)) {
				/* handled below */
			} else {
				continue;	/* ignore unimplemented options */
			}
		}
		/* if rehash, don't redo absolute path names */
		if (fullname[0] == '/' && idx <= prev &&
		    idx < firstchange) {
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
		e = EACCES;	/* if we fail, this will be the error */
		if (!S_ISREG(statb.st_mode))
			continue;
		if (pathopt) {		/* this is a %func directory */
			stalloc(strlen(fullname) + 1);
			readcmdfile(fullname);
			if ((cmdp = cmdlookup(name, 0)) == NULL || cmdp->cmdtype != CMDFUNCTION)
				error("%s not defined in %s", name, fullname);
			stunalloc(fullname);
			goto success;
		}
#ifdef notdef
		if (statb.st_uid == geteuid()) {
			if ((statb.st_mode & 0100) == 0)
				goto loop;
		} else if (statb.st_gid == getegid()) {
			if ((statb.st_mode & 010) == 0)
				goto loop;
		} else {
			if ((statb.st_mode & 01) == 0)
				goto loop;
		}
#endif
		TRACE(("searchexec \"%s\" returns \"%s\"\n", name, fullname));
		/* If we aren't called with DO_BRUTE and cmdp is set, it must
		   be a function and we're being called with DO_NOFUN */
		if (!updatetbl) {
			entry->cmdtype = CMDNORMAL;
			entry->u.index = idx;
			return;
		}
		INTOFF;
		cmdp = cmdlookup(name, 1);
		cmdp->cmdtype = CMDNORMAL;
		cmdp->param.index = idx;
		INTON;
		goto success;
	}

	/* We failed.  If there was an entry for this command, delete it */
	if (cmdp && updatetbl)
		delete_cmd_entry();
	if (act & DO_ERR)
		outfmt(out2, "%s: %s\n", name, errmsg(e, E_EXEC));
	entry->cmdtype = CMDUNKNOWN;
	return;

success:
	cmdp->rehash = 0;
	entry->cmdtype = cmdp->cmdtype;
	entry->u = cmdp->param;
}



/*
 * Search the table of builtin commands.
 */

struct builtincmd *
find_builtin(name)
	char *name;
{
	struct builtincmd *bp;

	bp = bsearch( &name, builtincmds, NUMBUILTINS, sizeof(struct builtincmd),
		pstrcmp
	);
	return bp;
}


/*
 * Called when a cd is done.  Marks all commands so the next time they
 * are executed they will be rehashed.
 */

static void
hashcd() {
	struct tblentry **pp;
	struct tblentry *cmdp;

	for (pp = cmdtable ; pp < &cmdtable[CMDTABLESIZE] ; pp++) {
		for (cmdp = *pp ; cmdp ; cmdp = cmdp->next) {
			if (cmdp->cmdtype == CMDNORMAL
			 || (cmdp->cmdtype == CMDBUILTIN && builtinloc >= 0))
				cmdp->rehash = 1;
		}
	}
}



/*
 * Called before PATH is changed.  The argument is the new value of PATH;
 * pathval() still returns the old value at this point.  Called with
 * interrupts off.
 */

static void
changepath(newval)
	const char *newval;
{
	int firstchange;
	int bltin;

	firstchange = path_change(newval, &bltin);
	if (builtinloc < 0 && bltin >= 0)
		builtinloc = bltin;		/* zap builtins */
	clearcmdentry(firstchange);
	builtinloc = bltin;
}


/*
 * Clear out command entries.  The argument specifies the first entry in
 * PATH which has changed.
 */

static void
clearcmdentry(firstchange)
	int firstchange;
{
	struct tblentry **tblp;
	struct tblentry **pp;
	struct tblentry *cmdp;

	INTOFF;
	for (tblp = cmdtable ; tblp < &cmdtable[CMDTABLESIZE] ; tblp++) {
		pp = tblp;
		while ((cmdp = *pp) != NULL) {
			if ((cmdp->cmdtype == CMDNORMAL &&
			     cmdp->param.index >= firstchange)
			 || (cmdp->cmdtype == CMDBUILTIN &&
			     builtinloc >= firstchange)) {
				*pp = cmdp->next;
				ckfree(cmdp);
			} else {
				pp = &cmdp->next;
			}
		}
	}
	INTON;
}


/*
 * Delete all functions.
 */

#ifdef mkinit
static void deletefuncs __P((void));

SHELLPROC {
	deletefuncs();
}
#endif

static void
deletefuncs() {
	struct tblentry **tblp;
	struct tblentry **pp;
	struct tblentry *cmdp;

	INTOFF;
	for (tblp = cmdtable ; tblp < &cmdtable[CMDTABLESIZE] ; tblp++) {
		pp = tblp;
		while ((cmdp = *pp) != NULL) {
			if (cmdp->cmdtype == CMDFUNCTION) {
				*pp = cmdp->next;
				freefunc(cmdp->param.func);
				ckfree(cmdp);
			} else {
				pp = &cmdp->next;
			}
		}
	}
	INTON;
}



/*
 * Locate a command in the command hash table.  If "add" is nonzero,
 * add the command to the table if it is not already present.  The
 * variable "lastcmdentry" is set to point to the address of the link
 * pointing to the entry, so that delete_cmd_entry can delete the
 * entry.
 */

struct tblentry **lastcmdentry;


static struct tblentry *
cmdlookup(name, add)
	char *name;
	int add;
{
	int hashval;
	char *p;
	struct tblentry *cmdp;
	struct tblentry **pp;

	p = name;
	hashval = *p << 4;
	while (*p)
		hashval += *p++;
	hashval &= 0x7FFF;
	pp = &cmdtable[hashval % CMDTABLESIZE];
	for (cmdp = *pp ; cmdp ; cmdp = cmdp->next) {
		if (equal(cmdp->cmdname, name))
			break;
		pp = &cmdp->next;
	}
	if (add && cmdp == NULL) {
		INTOFF;
		cmdp = *pp = ckmalloc(sizeof (struct tblentry) - ARB
					+ strlen(name) + 1);
		cmdp->next = NULL;
		cmdp->cmdtype = CMDUNKNOWN;
		cmdp->rehash = 0;
		strcpy(cmdp->cmdname, name);
		INTON;
	}
	lastcmdentry = pp;
	return cmdp;
}

/*
 * Delete the command entry returned on the last lookup.
 */

static void
delete_cmd_entry() {
	struct tblentry *cmdp;

	INTOFF;
	cmdp = *lastcmdentry;
	*lastcmdentry = cmdp->next;
	ckfree(cmdp);
	INTON;
}



#ifdef notdef
static void
getcmdentry(name, entry)
	char *name;
	struct cmdentry *entry;
	{
	struct tblentry *cmdp = cmdlookup(name, 0);

	if (cmdp) {
		entry->u = cmdp->param;
		entry->cmdtype = cmdp->cmdtype;
	} else {
		entry->cmdtype = CMDUNKNOWN;
		entry->u.index = 0;
	}
}
#endif


/*
 * Add a new command entry, replacing any existing command entry for
 * the same name.
 */

static void
addcmdentry(name, entry)
	char *name;
	struct cmdentry *entry;
	{
	struct tblentry *cmdp;

	INTOFF;
	cmdp = cmdlookup(name, 1);
	if (cmdp->cmdtype == CMDFUNCTION) {
		freefunc(cmdp->param.func);
	}
	cmdp->cmdtype = entry->cmdtype;
	cmdp->param = entry->u;
	INTON;
}


/*
 * Define a shell function.
 */

static void
defun(name, func)
	char *name;
	union node *func;
	{
	struct cmdentry entry;

	entry.cmdtype = CMDFUNCTION;
	entry.u.func = copyfunc(func);
	addcmdentry(name, &entry);
}


/*
 * Delete a function if it exists.
 */

static void
unsetfunc(name)
	char *name;
	{
	struct tblentry *cmdp;

	if ((cmdp = cmdlookup(name, 0)) != NULL && cmdp->cmdtype == CMDFUNCTION) {
		freefunc(cmdp->param.func);
		delete_cmd_entry();
	}
}

#ifdef ASH_TYPE
/*
 * Locate and print what a word is...
 */

static int
typecmd(argc, argv)
	int argc;
	char **argv;
{
	int i;
	int err = 0;

	for (i = 1; i < argc; i++) {
		err |= describe_command(argv[i], 1);
	}
	return err;
}

static int
describe_command(command, verbose)
	char *command;
	int verbose;
{
	struct cmdentry entry;
	struct tblentry *cmdp;
	const struct alias *ap;
	const char *path = pathval();

	if (verbose) {
		out1str(command);
	}

	/* First look at the keywords */
	if (findkwd(command)) {
		out1str(verbose ? " is a shell keyword" : command);
		goto out;
	}

	/* Then look at the aliases */
	if ((ap = lookupalias(command, 0)) != NULL) {
		if (verbose) {
			out1fmt(" is an alias for %s", ap->val);
		} else {
			printalias(ap);
		}
		goto out;
	}

	/* Then check if it is a tracked alias */
	if ((cmdp = cmdlookup(command, 0)) != NULL) {
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
		if (j == -1) {
			p = command;
		} else {
			do {
				p = padvance(&path, command);
				stunalloc(p);
			} while (--j >= 0);
		}
		if (verbose) {
			out1fmt(
				" is%s %s",
				cmdp ? " a tracked alias for" : nullstr, p
			);
		} else {
			out1str(p);
		}
		break;
	}

	case CMDFUNCTION:
		if (verbose) {
			out1str(" is a shell function");
		} else {
			out1str(command);
		}
		break;

	case CMDBUILTIN:
		if (verbose) {
			out1fmt(
				" is a %sshell builtin",
				entry.u.cmd->flags & BUILTIN_SPECIAL ?
					"special " : nullstr
			);
		} else {
			out1str(command);
		}
		break;

	default:
		if (verbose) {
			out1str(": not found\n");
		}
		return 127;
	}

out:
	out1c('\n');
	return 0;
}
#endif	

static int
commandcmd(argc, argv)
	int argc;
	char **argv;
{
	int c;
	int default_path = 0;
	int verify_only = 0;
	int verbose_verify_only = 0;

	while ((c = nextopt("pvV")) != '\0')
		switch (c) {
		case 'p':
			default_path = 1;
			break;
		case 'v':
			verify_only = 1;
			break;
		case 'V':
			verbose_verify_only = 1;
			break;
		default:
			outfmt(out2,
"command: nextopt returned character code 0%o\n", c);
			return EX_SOFTWARE;
		}

	if (default_path + verify_only + verbose_verify_only > 1 ||
	    !*argptr) {
			outfmt(out2,
"command [-p] command [arg ...]\n");
			outfmt(out2,
"command {-v|-V} command\n");
			return EX_USAGE;
	}

#ifdef ASH_TYPE
	if (verify_only || verbose_verify_only) {
		return describe_command(*argptr, verbose_verify_only);
	}
#endif	

	return 0;
}

static int
path_change(newval, bltin)
	const char *newval;
	int *bltin;
{
	const char *old, *new;
	int idx;
	int firstchange;

	old = pathval();
	new = newval;
	firstchange = 9999;	/* assume no change */
	idx = 0;
	*bltin = -1;
	for (;;) {
		if (*old != *new) {
			firstchange = idx;
			if ((*old == '\0' && *new == ':')
			 || (*old == ':' && *new == '\0'))
				firstchange++;
			old = new;	/* ignore subsequent differences */
		}
		if (*new == '\0')
			break;
		if (*new == '%' && *bltin < 0 && prefix("builtin", new + 1))
			*bltin = idx;
		if (*new == ':') {
			idx++;
		}
		new++, old++;
	}
	if (builtinloc >= 0 && *bltin < 0)
		firstchange = 0;
	return firstchange;
}
/*	$NetBSD: expand.c,v 1.50 2001/02/04 19:52:06 christos Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

/*
 * Routines to expand arguments to commands.  We have to deal with
 * backquotes, shell variables, and file metacharacters.
 */
/*
 * _rmescape() flags
 */
#define RMESCAPE_ALLOC	0x1	/* Allocate a new string */
#define RMESCAPE_GLOB	0x2	/* Add backslashes for glob */

/*
 * Structure specifying which parts of the string should be searched
 * for IFS characters.
 */

struct ifsregion {
	struct ifsregion *next;	/* next region in list */
	int begoff;		/* offset of start of region */
	int endoff;		/* offset of end of region */
	int nulonly;		/* search for nul bytes only */
};


static char *expdest;			/* output of current string */
struct nodelist *argbackq;	/* list of back quote expressions */
struct ifsregion ifsfirst;	/* first struct in list of ifs regions */
struct ifsregion *ifslastp;	/* last struct in list */
struct arglist exparg;		/* holds expanded arg list */

static void argstr __P((char *, int));
static char *exptilde __P((char *, int));
static void expbackq __P((union node *, int, int));
static int subevalvar __P((char *, char *, int, int, int, int, int));
static char *evalvar __P((char *, int));
static int varisset __P((char *, int));
static void strtodest __P((const char *, const char *, int));
static void varvalue __P((char *, int, int));
static void recordregion __P((int, int, int));
static void removerecordregions __P((int)); 
static void ifsbreakup __P((char *, struct arglist *));
static void ifsfree __P((void));
static void expandmeta __P((struct strlist *, int));
#if defined(__GLIBC__) && !defined(FNMATCH_BROKEN)
#define preglob(p) _rmescapes((p), RMESCAPE_ALLOC | RMESCAPE_GLOB)
#if !defined(GLOB_BROKEN)
static void addglob __P((const glob_t *));
#endif
#endif
#if !(defined(__GLIBC__) && !defined(FNMATCH_BROKEN) && !defined(GLOB_BROKEN))
static void expmeta __P((char *, char *));
#endif
static void addfname __P((char *));
#if !(defined(__GLIBC__) && !defined(FNMATCH_BROKEN) && !defined(GLOB_BROKEN))
static struct strlist *expsort __P((struct strlist *));
static struct strlist *msort __P((struct strlist *, int));
#endif
#if defined(__GLIBC__) && !defined(FNMATCH_BROKEN)
static int patmatch __P((char *, char *, int));
static int patmatch2 __P((char *, char *, int));
#else
static int pmatch __P((char *, char *, int));
#define patmatch2 patmatch
#endif
static char *cvtnum __P((int, char *));

extern int oexitstatus;

/*
 * Expand shell variables and backquotes inside a here document.
 */

static void
expandhere(arg, fd)
	union node *arg;	/* the document */
	int fd;			/* where to write the expanded version */
	{
	herefd = fd;
	expandarg(arg, (struct arglist *)NULL, 0);
	xwrite(fd, stackblock(), expdest - stackblock());
}


/*
 * Perform variable substitution and command substitution on an argument,
 * placing the resulting list of arguments in arglist.  If EXP_FULL is true,
 * perform splitting and file name expansion.  When arglist is NULL, perform
 * here document expansion.
 */

static void
expandarg(arg, arglist, flag)
	union node *arg;
	struct arglist *arglist;
	int flag;
{
	struct strlist *sp;
	char *p;

	argbackq = arg->narg.backquote;
	STARTSTACKSTR(expdest);
	ifsfirst.next = NULL;
	ifslastp = NULL;
	argstr(arg->narg.text, flag);
	if (arglist == NULL) {
		return;			/* here document expanded */
	}
	STPUTC('\0', expdest);
	p = grabstackstr(expdest);
	exparg.lastp = &exparg.list;
	/*
	 * TODO - EXP_REDIR
	 */
	if (flag & EXP_FULL) {
		ifsbreakup(p, &exparg);
		*exparg.lastp = NULL;
		exparg.lastp = &exparg.list;
		expandmeta(exparg.list, flag);
	} else {
		if (flag & EXP_REDIR) /*XXX - for now, just remove escapes */
			rmescapes(p);
		sp = (struct strlist *)stalloc(sizeof (struct strlist));
		sp->text = p;
		*exparg.lastp = sp;
		exparg.lastp = &sp->next;
	}
	ifsfree();
	*exparg.lastp = NULL;
	if (exparg.list) {
		*arglist->lastp = exparg.list;
		arglist->lastp = exparg.lastp;
	}
}



/*
 * Perform variable and command substitution.  If EXP_FULL is set, output CTLESC
 * characters to allow for further processing.  Otherwise treat
 * $@ like $* since no splitting will be performed.
 */

static void
argstr(p, flag)
	char *p;
	int flag;
{
	char c;
	int quotes = flag & (EXP_FULL | EXP_CASE);	/* do CTLESC */
	int firsteq = 1;

	if (*p == '~' && (flag & (EXP_TILDE | EXP_VARTILDE)))
		p = exptilde(p, flag);
	for (;;) {
		switch (c = *p++) {
		case '\0':
		case CTLENDVAR: /* ??? */
			goto breakloop;
		case CTLQUOTEMARK:
			/* "$@" syntax adherence hack */
			if (p[0] == CTLVAR && p[2] == '@' && p[3] == '=')
				break;
			if ((flag & EXP_FULL) != 0)
				STPUTC(c, expdest);
			break;
		case CTLESC:
			if (quotes)
				STPUTC(c, expdest);
			c = *p++;
			STPUTC(c, expdest);
			break;
		case CTLVAR:
			p = evalvar(p, flag);
			break;
		case CTLBACKQ:
		case CTLBACKQ|CTLQUOTE:
			expbackq(argbackq->n, c & CTLQUOTE, flag);
			argbackq = argbackq->next;
			break;
#ifdef ASH_MATH_SUPPORT
		case CTLENDARI:
			expari(flag);
			break;
#endif	
		case ':':
		case '=':
			/*
			 * sort of a hack - expand tildes in variable
			 * assignments (after the first '=' and after ':'s).
			 */
			STPUTC(c, expdest);
			if (flag & EXP_VARTILDE && *p == '~') {
				if (c == '=') {
					if (firsteq)
						firsteq = 0;
					else
						break;
				}
				p = exptilde(p, flag);
			}
			break;
		default:
			STPUTC(c, expdest);
		}
	}
breakloop:;
	return;
}

static char *
exptilde(p, flag)
	char *p;
	int flag;
{
	char c, *startp = p;
	struct passwd *pw;
	const char *home;
	int quotes = flag & (EXP_FULL | EXP_CASE);

	while ((c = *p) != '\0') {
		switch(c) {
		case CTLESC:
			return (startp);
		case CTLQUOTEMARK:
			return (startp);
		case ':':
			if (flag & EXP_VARTILDE)
				goto done;
			break;
		case '/':
			goto done;
		}
		p++;
	}
done:
	*p = '\0';
	if (*(startp+1) == '\0') {
		if ((home = lookupvar("HOME")) == NULL)
			goto lose;
	} else {
		if ((pw = getpwnam(startp+1)) == NULL)
			goto lose;
		home = pw->pw_dir;
	}
	if (*home == '\0')
		goto lose;
	*p = c;
	strtodest(home, SQSYNTAX, quotes);
	return (p);
lose:
	*p = c;
	return (startp);
}


static void 
removerecordregions(endoff)
	int endoff;
{
	if (ifslastp == NULL)
		return;

	if (ifsfirst.endoff > endoff) {
		while (ifsfirst.next != NULL) {
			struct ifsregion *ifsp;
			INTOFF;
			ifsp = ifsfirst.next->next;
			ckfree(ifsfirst.next);
			ifsfirst.next = ifsp;
			INTON;
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
		INTOFF;
		ifsp = ifslastp->next->next;
		ckfree(ifslastp->next);
		ifslastp->next = ifsp;
		INTON;
	}
	if (ifslastp->endoff > endoff)
		ifslastp->endoff = endoff;
}


#ifdef ASH_MATH_SUPPORT
/*
 * Expand arithmetic expression.  Backup to start of expression,
 * evaluate, place result in (backed up) result, adjust string position.
 */
static void
expari(flag)
	int flag;
{
	char *p, *start;
	int result;
	int begoff;
	int quotes = flag & (EXP_FULL | EXP_CASE);
	int quoted;

	/*	ifsfree(); */

	/*
	 * This routine is slightly over-complicated for
	 * efficiency.  First we make sure there is
	 * enough space for the result, which may be bigger
	 * than the expression if we add exponentation.  Next we
	 * scan backwards looking for the start of arithmetic.  If the
	 * next previous character is a CTLESC character, then we
	 * have to rescan starting from the beginning since CTLESC
	 * characters have to be processed left to right.
	 */
	CHECKSTRSPACE(10, expdest);
	USTPUTC('\0', expdest);
	start = stackblock();
	p = expdest - 1;
	while (*p != CTLARI && p >= start)
		--p;
	if (*p != CTLARI)
		error("missing CTLARI (shouldn't happen)");
	if (p > start && *(p-1) == CTLESC)
		for (p = start; *p != CTLARI; p++)
			if (*p == CTLESC)
				p++;

	if (p[1] == '"')
		quoted=1;
	else
		quoted=0;
	begoff = p - start;
	removerecordregions(begoff);
	if (quotes)
		rmescapes(p+2);
	result = arith(p+2);
	fmtstr(p, 12, "%d", result);

	while (*p++)
		;

	if (quoted == 0)
		recordregion(begoff, p - 1 - start, 0);
	result = expdest - p + 1;
	STADJUST(-result, expdest);
}
#endif	


/*
 * Expand stuff in backwards quotes.
 */

static void
expbackq(cmd, quoted, flag)
	union node *cmd;
	int quoted;
	int flag;
{
	volatile struct backcmd in;
	int i;
	char buf[128];
	char *p;
	char *dest = expdest;
	volatile struct ifsregion saveifs;
	struct ifsregion *volatile savelastp;
	struct nodelist *volatile saveargbackq;
	char lastc;
	int startloc = dest - stackblock();
	char const *syntax = quoted? DQSYNTAX : BASESYNTAX;
	volatile int saveherefd;
	int quotes = flag & (EXP_FULL | EXP_CASE);
	struct jmploc jmploc;
	struct jmploc *volatile savehandler;
	int ex;

#if __GNUC__
	/* Avoid longjmp clobbering */
	(void) &dest;
	(void) &syntax;
#endif

	in.fd = -1;
	in.buf = 0;
	in.jp = 0;

	INTOFF;
	saveifs = ifsfirst;
	savelastp = ifslastp;
	saveargbackq = argbackq;
	saveherefd = herefd;
	herefd = -1;
	if ((ex = setjmp(jmploc.loc))) {
		goto err1;
	}
	savehandler = handler;
	handler = &jmploc;
	INTON;
	p = grabstackstr(dest);
	evalbackcmd(cmd, (struct backcmd *) &in);
	ungrabstackstr(p, dest);
err1:
	INTOFF;
	ifsfirst = saveifs;
	ifslastp = savelastp;
	argbackq = saveargbackq;
	herefd = saveherefd;
	if (ex) {
		goto err2;
	}

	p = in.buf;
	lastc = '\0';
	for (;;) {
		if (--in.nleft < 0) {
			if (in.fd < 0)
				break;
			while ((i = read(in.fd, buf, sizeof buf)) < 0 && errno == EINTR);
			TRACE(("expbackq: read returns %d\n", i));
			if (i <= 0)
				break;
			p = buf;
			in.nleft = i - 1;
		}
		lastc = *p++;
		if (lastc != '\0') {
			if (quotes && syntax[(int)lastc] == CCTL)
				STPUTC(CTLESC, dest);
			STPUTC(lastc, dest);
		}
	}

	/* Eat all trailing newlines */
	for (; dest > stackblock() && dest[-1] == '\n';)
		STUNPUTC(dest);

err2:
	if (in.fd >= 0)
		close(in.fd);
	if (in.buf)
		ckfree(in.buf);
	if (in.jp)
		exitstatus = waitforjob(in.jp);
	handler = savehandler;
	if (ex) {
		longjmp(handler->loc, 1);
	}
	if (quoted == 0)
		recordregion(startloc, dest - stackblock(), 0);
	TRACE(("evalbackq: size=%d: \"%.*s\"\n",
		(dest - stackblock()) - startloc,
		(dest - stackblock()) - startloc,
		stackblock() + startloc));
	expdest = dest;
	INTON;
}



static int
subevalvar(p, str, strloc, subtype, startloc, varflags, quotes)
	char *p;
	char *str;
	int strloc;
	int subtype;
	int startloc;
	int varflags;
	int quotes;
{
	char *startp;
	char *loc = NULL;
	char *q;
	int c = 0;
	int saveherefd = herefd;
	struct nodelist *saveargbackq = argbackq;
	int amount;

	herefd = -1;
	argstr(p, subtype != VSASSIGN && subtype != VSQUESTION ? EXP_CASE : 0);
	STACKSTRNUL(expdest);
	herefd = saveherefd;
	argbackq = saveargbackq;
	startp = stackblock() + startloc;
	if (str == NULL)
	    str = stackblock() + strloc;

	switch (subtype) {
	case VSASSIGN:
		setvar(str, startp, 0);
		amount = startp - expdest;
		STADJUST(amount, expdest);
		varflags &= ~VSNUL;
		if (c != 0)
			*loc = c;
		return 1;

	case VSQUESTION:
		if (*p != CTLENDVAR) {
			outfmt(&errout, snlfmt, startp);
			error((char *)NULL);
		}
		error("%.*s: parameter %snot set", p - str - 1,
		      str, (varflags & VSNUL) ? "null or "
					      : nullstr);
		/* NOTREACHED */

	case VSTRIMLEFT:
		for (loc = startp; loc < str; loc++) {
			c = *loc;
			*loc = '\0';
			if (patmatch2(str, startp, quotes))
				goto recordleft;
			*loc = c;
			if (quotes && *loc == CTLESC)
			        loc++;
		}
		return 0;

	case VSTRIMLEFTMAX:
		for (loc = str - 1; loc >= startp;) {
			c = *loc;
			*loc = '\0';
			if (patmatch2(str, startp, quotes))
				goto recordleft;
			*loc = c;
			loc--;
			if (quotes && loc > startp && *(loc - 1) == CTLESC) {
				for (q = startp; q < loc; q++)
					if (*q == CTLESC)
						q++;
				if (q > loc)
					loc--;
			}
		}
		return 0;

	case VSTRIMRIGHT:
	        for (loc = str - 1; loc >= startp;) {
			if (patmatch2(str, loc, quotes))
				goto recordright;
			loc--;
			if (quotes && loc > startp && *(loc - 1) == CTLESC) { 
				for (q = startp; q < loc; q++)
					if (*q == CTLESC)
						q++;
				if (q > loc)
					loc--;
			}
		}
		return 0;

	case VSTRIMRIGHTMAX:
		for (loc = startp; loc < str - 1; loc++) {
			if (patmatch2(str, loc, quotes))
				goto recordright;
			if (quotes && *loc == CTLESC)
			        loc++;
		}
		return 0;

#ifdef DEBUG
	default:
		abort();
#endif
	}

recordleft:
	*loc = c;
	amount = ((str - 1) - (loc - startp)) - expdest;
	STADJUST(amount, expdest);
	while (loc != str - 1)
		*startp++ = *loc++;
	return 1;

recordright:
	amount = loc - expdest;
	STADJUST(amount, expdest);
	STPUTC('\0', expdest);
	STADJUST(-1, expdest);
	return 1;
}


/*
 * Expand a variable, and return a pointer to the next character in the
 * input string.
 */

static char *
evalvar(p, flag)
	char *p;
	int flag;
{
	int subtype;
	int varflags;
	char *var;
	char *val;
	int patloc;
	int c;
	int set;
	int special;
	int startloc;
	int varlen;
	int easy;
	int quotes = flag & (EXP_FULL | EXP_CASE);

	varflags = *p++;
	subtype = varflags & VSTYPE;
	var = p;
	special = 0;
	if (! is_name(*p))
		special = 1;
	p = strchr(p, '=') + 1;
again: /* jump here after setting a variable with ${var=text} */
	if (special) {
		set = varisset(var, varflags & VSNUL);
		val = NULL;
	} else {
		val = lookupvar(var);
		if (val == NULL || ((varflags & VSNUL) && val[0] == '\0')) {
			val = NULL;
			set = 0;
		} else
			set = 1;
	}
	varlen = 0;
	startloc = expdest - stackblock();
	if (set && subtype != VSPLUS) {
		/* insert the value of the variable */
		if (special) {
			varvalue(var, varflags & VSQUOTE, flag);
			if (subtype == VSLENGTH) {
				varlen = expdest - stackblock() - startloc;
				STADJUST(-varlen, expdest);
			}
		} else {
			if (subtype == VSLENGTH) {
				varlen = strlen(val);
			} else {
				strtodest(
					val,
					varflags & VSQUOTE ?
						DQSYNTAX : BASESYNTAX,
					quotes
				);
			}
		}
	}

	if (subtype == VSPLUS)
		set = ! set;

	easy = ((varflags & VSQUOTE) == 0 ||
		(*var == '@' && shellparam.nparam != 1));


	switch (subtype) {
	case VSLENGTH:
		expdest = cvtnum(varlen, expdest);
		goto record;

	case VSNORMAL:
		if (!easy)
			break;
record:
		recordregion(startloc, expdest - stackblock(),
			     varflags & VSQUOTE);
		break;

	case VSPLUS:
	case VSMINUS:
		if (!set) {
		        argstr(p, flag);
			break;
		}
		if (easy)
			goto record;
		break;

	case VSTRIMLEFT:
	case VSTRIMLEFTMAX:
	case VSTRIMRIGHT:
	case VSTRIMRIGHTMAX:
		if (!set)
			break;
		/*
		 * Terminate the string and start recording the pattern
		 * right after it
		 */
		STPUTC('\0', expdest);
		patloc = expdest - stackblock();
		if (subevalvar(p, NULL, patloc, subtype,
			       startloc, varflags, quotes) == 0) {
			int amount = (expdest - stackblock() - patloc) + 1;
			STADJUST(-amount, expdest);
		}
		/* Remove any recorded regions beyond start of variable */
		removerecordregions(startloc);
		goto record;

	case VSASSIGN:
	case VSQUESTION:
		if (!set) {
			if (subevalvar(p, var, 0, subtype, startloc,
				       varflags, quotes)) {
				varflags &= ~VSNUL;
				/* 
				 * Remove any recorded regions beyond 
				 * start of variable 
				 */
				removerecordregions(startloc);
				goto again;
			}
			break;
		}
		if (easy)
			goto record;
		break;

#ifdef DEBUG
	default:
		abort();
#endif
	}

	if (subtype != VSNORMAL) {	/* skip to end of alternative */
		int nesting = 1;
		for (;;) {
			if ((c = *p++) == CTLESC)
				p++;
			else if (c == CTLBACKQ || c == (CTLBACKQ|CTLQUOTE)) {
				if (set)
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
 * Test whether a specialized variable is set.
 */

static int
varisset(name, nulok)
	char *name;
	int nulok;
{
	if (*name == '!')
		return backgndpid != -1;
	else if (*name == '@' || *name == '*') {
		if (*shellparam.p == NULL)
			return 0;

		if (nulok) {
			char **av;

			for (av = shellparam.p; *av; av++)
				if (**av != '\0')
					return 1;
			return 0;
		}
	} else if (is_digit(*name)) {
		char *ap;
		int num = atoi(name);

		if (num > shellparam.nparam)
			return 0;

		if (num == 0)
			ap = arg0;
		else
			ap = shellparam.p[num - 1];

		if (nulok && (ap == NULL || *ap == '\0'))
			return 0;
	}
	return 1;
}



/*
 * Put a string on the stack.
 */

static void
strtodest(p, syntax, quotes)
	const char *p;
	const char *syntax;
	int quotes;
{
	while (*p) {
		if (quotes && syntax[(int) *p] == CCTL)
			STPUTC(CTLESC, expdest);
		STPUTC(*p++, expdest);
	}
}



/*
 * Add the value of a specialized variable to the stack string.
 */

static void
varvalue(name, quoted, flags)
	char *name;
	int quoted;
	int flags;
{
	int num;
	char *p;
	int i;
	int sep;
	int sepq = 0;
	char **ap;
	char const *syntax;
	int allow_split = flags & EXP_FULL;
	int quotes = flags & (EXP_FULL | EXP_CASE);

	syntax = quoted ? DQSYNTAX : BASESYNTAX;
	switch (*name) {
	case '$':
		num = rootpid;
		goto numvar;
	case '?':
		num = oexitstatus;
		goto numvar;
	case '#':
		num = shellparam.nparam;
		goto numvar;
	case '!':
		num = backgndpid;
numvar:
		expdest = cvtnum(num, expdest);
		break;
	case '-':
		for (i = 0 ; i < NOPTS ; i++) {
			if (optlist[i].val)
				STPUTC(optlist[i].letter, expdest);
		}
		break;
	case '@':
		if (allow_split && quoted) {
			sep = 1 << CHAR_BIT;
			goto param;
		}
		/* fall through */
	case '*':
		sep = ifsset() ? ifsval()[0] : ' ';
		if (quotes) {
			sepq = syntax[(int) sep] == CCTL;
		}
param:
		for (ap = shellparam.p ; (p = *ap++) != NULL ; ) {
			strtodest(p, syntax, quotes);
			if (*ap && sep) {
				if (sepq)
					STPUTC(CTLESC, expdest);
				STPUTC(sep, expdest);
			}
		}
		break;
	case '0':
		strtodest(arg0, syntax, quotes);
		break;
	default:
		num = atoi(name);
		if (num > 0 && num <= shellparam.nparam) {
			strtodest(shellparam.p[num - 1], syntax, quotes);
		}
		break;
	}
}



/*
 * Record the fact that we have to scan this region of the
 * string for IFS characters.
 */

static void
recordregion(start, end, nulonly)
	int start;
	int end;
	int nulonly;
{
	struct ifsregion *ifsp;

	if (ifslastp == NULL) {
		ifsp = &ifsfirst;
	} else {
		INTOFF;
		ifsp = (struct ifsregion *)ckmalloc(sizeof (struct ifsregion));
		ifsp->next = NULL;
		ifslastp->next = ifsp;
		INTON;
	}
	ifslastp = ifsp;
	ifslastp->begoff = start;
	ifslastp->endoff = end;
	ifslastp->nulonly = nulonly;
}



/*
 * Break the argument string into pieces based upon IFS and add the
 * strings to the argument list.  The regions of the string to be
 * searched for IFS characters have been stored by recordregion.
 */
static void
ifsbreakup(string, arglist)
	char *string;
	struct arglist *arglist;
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
	ifsspc = 0;
	nulonly = 0;
	realifs = ifsset() ? ifsval() : defifs;
	if (ifslastp != NULL) {
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
				if (strchr(ifs, *p)) {
					if (!nulonly)
						ifsspc = (strchr(defifs, *p) != NULL);
					/* Ignore IFS whitespace at start */
					if (q == start && ifsspc) {
						p++;
						start = p;
						continue;
					}
					*q = '\0';
					sp = (struct strlist *)stalloc(sizeof *sp);
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
							if (strchr(ifs, *p) == NULL ) {
								p = q;
								break;
							} else if (strchr(defifs, *p) == NULL) {
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
				} else
					p++;
			}
		} while ((ifsp = ifsp->next) != NULL);
		if (!(*start || (!ifsspc && start > string && nulonly))) {
			return;
		}
	}

	sp = (struct strlist *)stalloc(sizeof *sp);
	sp->text = start;
	*arglist->lastp = sp;
	arglist->lastp = &sp->next;
}

static void
ifsfree()
{
	while (ifsfirst.next != NULL) {
		struct ifsregion *ifsp;
		INTOFF;
		ifsp = ifsfirst.next->next;
		ckfree(ifsfirst.next);
		ifsfirst.next = ifsp;
		INTON;
	}
	ifslastp = NULL;
	ifsfirst.next = NULL;
}



/*
 * Expand shell metacharacters.  At this point, the only control characters
 * should be escapes.  The results are stored in the list exparg.
 */

#if defined(__GLIBC__) && !defined(FNMATCH_BROKEN) && !defined(GLOB_BROKEN)
static void
expandmeta(str, flag)
	struct strlist *str;
	int flag;
{
	const char *p;
	glob_t pglob;
	/* TODO - EXP_REDIR */

	while (str) {
		if (fflag)
			goto nometa;
		p = preglob(str->text);
		INTOFF;
		switch (glob(p, GLOB_NOMAGIC, 0, &pglob)) {
		case 0:
			if (!(pglob.gl_flags & GLOB_MAGCHAR))
				goto nometa2;
			addglob(&pglob);
			globfree(&pglob);
			INTON;
			break;
		case GLOB_NOMATCH:
nometa2:
			globfree(&pglob);
			INTON;
nometa:
			*exparg.lastp = str;
			rmescapes(str->text);
			exparg.lastp = &str->next;
			break;
		default:	/* GLOB_NOSPACE */
			error("Out of space");
		}
		str = str->next;
	}
}


/*
 * Add the result of glob(3) to the list.
 */

static void
addglob(pglob)
	const glob_t *pglob;
{
	char **p = pglob->gl_pathv;

	do {
		addfname(*p);
	} while (*++p);
}


#else	/* defined(__GLIBC__) && !defined(FNMATCH_BROKEN) && !defined(GLOB_BROKEN) */
static char *expdir;


static void
expandmeta(str, flag)
	struct strlist *str;
	int flag;
{
	char *p;
	struct strlist **savelastp;
	struct strlist *sp;
	char c;
	/* TODO - EXP_REDIR */

	while (str) {
		if (fflag)
			goto nometa;
		p = str->text;
		for (;;) {			/* fast check for meta chars */
			if ((c = *p++) == '\0')
				goto nometa;
			if (c == '*' || c == '?' || c == '[' || c == '!')
				break;
		}
		savelastp = exparg.lastp;
		INTOFF;
		if (expdir == NULL) {
			int i = strlen(str->text);
			expdir = ckmalloc(i < 2048 ? 2048 : i); /* XXX */
		}

		expmeta(expdir, str->text);
		ckfree(expdir);
		expdir = NULL;
		INTON;
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
 * Do metacharacter (i.e. *, ?, [...]) expansion.
 */

static void
expmeta(enddir, name)
	char *enddir;
	char *name;
	{
	char *p;
	const char *cp;
	char *q;
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
	for (p = name ; ; p++) {
		if (*p == '*' || *p == '?')
			metaflag = 1;
		else if (*p == '[') {
			q = p + 1;
			if (*q == '!')
				q++;
			for (;;) {
				while (*q == CTLQUOTEMARK)
					q++;
				if (*q == CTLESC)
					q++;
				if (*q == '/' || *q == '\0')
					break;
				if (*++q == ']') {
					metaflag = 1;
					break;
				}
			}
		} else if (*p == '!' && p[1] == '!'	&& (p == name || p[-1] == '/')) {
			metaflag = 1;
		} else if (*p == '\0')
			break;
		else if (*p == CTLQUOTEMARK)
			continue;
		else if (*p == CTLESC)
			p++;
		if (*p == '/') {
			if (metaflag)
				break;
			start = p + 1;
		}
	}
	if (metaflag == 0) {	/* we've reached the end of the file name */
		if (enddir != expdir)
			metaflag++;
		for (p = name ; ; p++) {
			if (*p == CTLQUOTEMARK)
				continue;
			if (*p == CTLESC)
				p++;
			*enddir++ = *p;
			if (*p == '\0')
				break;
		}
		if (metaflag == 0 || lstat(expdir, &statb) >= 0)
			addfname(expdir);
		return;
	}
	endname = p;
	if (start != name) {
		p = name;
		while (p < start) {
			while (*p == CTLQUOTEMARK)
				p++;
			if (*p == CTLESC)
				p++;
			*enddir++ = *p++;
		}
	}
	if (enddir == expdir) {
		cp = ".";
	} else if (enddir == expdir + 1 && *expdir == '/') {
		cp = "/";
	} else {
		cp = expdir;
		enddir[-1] = '\0';
	}
	if ((dirp = opendir(cp)) == NULL)
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
	while (*p == CTLQUOTEMARK)
		p++;
	if (*p == CTLESC)
		p++;
	if (*p == '.')
		matchdot++;
	while (! int_pending() && (dp = readdir(dirp)) != NULL) {
		if (dp->d_name[0] == '.' && ! matchdot)
			continue;
		if (patmatch(start, dp->d_name, 0)) {
			if (atend) {
				scopy(dp->d_name, enddir);
				addfname(expdir);
			} else {
				for (p = enddir, cp = dp->d_name;
				     (*p++ = *cp++) != '\0';)
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
#endif	/* defined(__GLIBC__) && !defined(FNMATCH_BROKEN) && !defined(GLOB_BROKEN) */


/*
 * Add a file name to the list.
 */

static void
addfname(name)
	char *name;
	{
	char *p;
	struct strlist *sp;

	p = sstrdup(name);
	sp = (struct strlist *)stalloc(sizeof *sp);
	sp->text = p;
	*exparg.lastp = sp;
	exparg.lastp = &sp->next;
}


#if !(defined(__GLIBC__) && !defined(FNMATCH_BROKEN) && !defined(GLOB_BROKEN))
/*
 * Sort the results of file name expansion.  It calculates the number of
 * strings to sort and then calls msort (short for merge sort) to do the
 * work.
 */

static struct strlist *
expsort(str)
	struct strlist *str;
	{
	int len;
	struct strlist *sp;

	len = 0;
	for (sp = str ; sp ; sp = sp->next)
		len++;
	return msort(str, len);
}


static struct strlist *
msort(list, len)
	struct strlist *list;
	int len;
{
	struct strlist *p, *q = NULL;
	struct strlist **lpp;
	int half;
	int n;

	if (len <= 1)
		return list;
	half = len >> 1;
	p = list;
	for (n = half ; --n >= 0 ; ) {
		q = p;
		p = p->next;
	}
	q->next = NULL;			/* terminate first half of list */
	q = msort(list, half);		/* sort first half of list */
	p = msort(p, len - half);		/* sort second half */
	lpp = &list;
	for (;;) {
		if (strcmp(p->text, q->text) < 0) {
			*lpp = p;
			lpp = &p->next;
			if ((p = *lpp) == NULL) {
				*lpp = q;
				break;
			}
		} else {
			*lpp = q;
			lpp = &q->next;
			if ((q = *lpp) == NULL) {
				*lpp = p;
				break;
			}
		}
	}
	return list;
}
#endif



/*
 * Returns true if the pattern matches the string.
 */

#if defined(__GLIBC__) && !defined(FNMATCH_BROKEN)
static int
patmatch(pattern, string, squoted)
	char *pattern;
	char *string;
	int squoted;	/* string might have quote chars */
	{
	const char *p;
	char *q;

	p = preglob(pattern);
	q = squoted ? _rmescapes(string, RMESCAPE_ALLOC) : string;

	return !fnmatch(p, q, 0);
}


static int
patmatch2(pattern, string, squoted)
	char *pattern;
	char *string;
	int squoted;	/* string might have quote chars */
	{
	char *p;
	int res;

	sstrnleft--;
	p = grabstackstr(expdest);
	res = patmatch(pattern, string, squoted);
	ungrabstackstr(p, expdest);
	return res;
}
#else
static int
patmatch(pattern, string, squoted)
	char *pattern;
	char *string;
	int squoted;	/* string might have quote chars */
	{
#ifdef notdef
	if (pattern[0] == '!' && pattern[1] == '!')
		return 1 - pmatch(pattern + 2, string);
	else
#endif
		return pmatch(pattern, string, squoted);
}


static int
pmatch(pattern, string, squoted)
	char *pattern;
	char *string;
	int squoted;
	{
	char *p, *q;
	char c;

	p = pattern;
	q = string;
	for (;;) {
		switch (c = *p++) {
		case '\0':
			goto breakloop;
		case CTLESC:
			if (squoted && *q == CTLESC)
				q++;
			if (*q++ != *p++)
				return 0;
			break;
		case CTLQUOTEMARK:
			continue;
		case '?':
			if (squoted && *q == CTLESC)
				q++;
			if (*q++ == '\0')
				return 0;
			break;
		case '*':
			c = *p;
			while (c == CTLQUOTEMARK || c == '*')
				c = *++p;
			if (c != CTLESC &&  c != CTLQUOTEMARK &&
			    c != '?' && c != '*' && c != '[') {
				while (*q != c) {
					if (squoted && *q == CTLESC &&
					    q[1] == c)
						break;
					if (*q == '\0')
						return 0;
					if (squoted && *q == CTLESC)
						q++;
					q++;
				}
			}
			do {
				if (pmatch(p, q, squoted))
					return 1;
				if (squoted && *q == CTLESC)
					q++;
			} while (*q++ != '\0');
			return 0;
		case '[': {
			char *endp;
			int invert, found;
			char chr;

			endp = p;
			if (*endp == '!')
				endp++;
			for (;;) {
				while (*endp == CTLQUOTEMARK)
					endp++;
				if (*endp == '\0')
					goto dft;		/* no matching ] */
				if (*endp == CTLESC)
					endp++;
				if (*++endp == ']')
					break;
			}
			invert = 0;
			if (*p == '!') {
				invert++;
				p++;
			}
			found = 0;
			chr = *q++;
			if (squoted && chr == CTLESC)
				chr = *q++;
			if (chr == '\0')
				return 0;
			c = *p++;
			do {
				if (c == CTLQUOTEMARK)
					continue;
				if (c == CTLESC)
					c = *p++;
				if (*p == '-' && p[1] != ']') {
					p++;
					while (*p == CTLQUOTEMARK)
						p++;
					if (*p == CTLESC)
						p++;
					if (chr >= c && chr <= *p)
						found = 1;
					p++;
				} else {
					if (chr == c)
						found = 1;
				}
			} while ((c = *p++) != ']');
			if (found == invert)
				return 0;
			break;
		}
dft:	        default:
			if (squoted && *q == CTLESC)
				q++;
			if (*q++ != c)
				return 0;
			break;
		}
	}
breakloop:
	if (*q != '\0')
		return 0;
	return 1;
}
#endif



/*
 * Remove any CTLESC characters from a string.
 */

#if defined(__GLIBC__) && !defined(FNMATCH_BROKEN)
static char *
_rmescapes(str, flag)
	char *str;
	int flag;
{
	char *p, *q, *r;
	static const char qchars[] = { CTLESC, CTLQUOTEMARK, 0 };

	p = strpbrk(str, qchars);
	if (!p) {
		return str;
	}
	q = p;
	r = str;
	if (flag & RMESCAPE_ALLOC) {
		size_t len = p - str;
		q = r = stalloc(strlen(p) + len + 1);
		if (len > 0) {
#ifdef _GNU_SOURCE
			q = mempcpy(q, str, len);
#else
			memcpy(q, str, len);
			q += len;
#endif
		}
	}
	while (*p) {
		if (*p == CTLQUOTEMARK) {
			p++;
			continue;
		}
		if (*p == CTLESC) {
			p++;
			if (flag & RMESCAPE_GLOB && *p != '/') {
				*q++ = '\\';
			}
		}
		*q++ = *p++;
	}
	*q = '\0';
	return r;
}
#else
static void
rmescapes(str)
	char *str;
{
	char *p, *q;

	p = str;
	while (*p != CTLESC && *p != CTLQUOTEMARK) {
		if (*p++ == '\0')
			return;
	}
	q = p;
	while (*p) {
		if (*p == CTLQUOTEMARK) {
			p++;
			continue;
		}
		if (*p == CTLESC)
			p++;
		*q++ = *p++;
	}
	*q = '\0';
}
#endif



/*
 * See if a pattern matches in a case statement.
 */

static int
casematch(pattern, val)
	union node *pattern;
	char *val;
	{
	struct stackmark smark;
	int result;
	char *p;

	setstackmark(&smark);
	argbackq = pattern->narg.backquote;
	STARTSTACKSTR(expdest);
	ifslastp = NULL;
	argstr(pattern->narg.text, EXP_TILDE | EXP_CASE);
	STPUTC('\0', expdest);
	p = grabstackstr(expdest);
	result = patmatch(p, val, 0);
	popstackmark(&smark);
	return result;
}

/*
 * Our own itoa().
 */

static char *
cvtnum(num, buf)
	int num;
	char *buf;
	{
	int len;

	CHECKSTRSPACE(32, buf);
	len = sprintf(buf, "%d", num);
	STADJUST(len, buf);
	return buf;
}
/*	$NetBSD: histedit.c,v 1.25 2001/02/04 19:52:06 christos Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

/*
 * Editline and history functions (and glue).
 */
static int histcmd(argc, argv)
	int argc;
	char **argv;
{
	error("not compiled with history support");
	/* NOTREACHED */
}


/*
 * This file was generated by the mkinit program.
 */

extern void rmaliases __P((void));

extern int loopnest;		/* current loop nesting level */

extern void deletefuncs __P((void));

struct strpush {
	struct strpush *prev;	/* preceding string on stack */
	char *prevstring;
	int prevnleft;
	struct alias *ap;	/* if push was associated with an alias */
	char *string;		/* remember the string since it may change */
};

struct parsefile {
	struct parsefile *prev;	/* preceding file on stack */
	int linno;		/* current line */
	int fd;			/* file descriptor (or -1 if string) */
	int nleft;		/* number of chars left in this line */
	int lleft;		/* number of chars left in this buffer */
	char *nextc;		/* next char in buffer */
	char *buf;		/* input buffer */
	struct strpush *strpush; /* for pushing strings at this level */
	struct strpush basestrpush; /* so pushing one is fast */
};

extern int parselleft;		/* copy of parsefile->lleft */
extern struct parsefile basepf;	/* top level input file */
extern char basebuf[BUFSIZ];	/* buffer for top level input file */

extern short backgndpid;	/* pid of last background process */
extern int jobctl;

extern int tokpushback;		/* last token pushed back */
extern int checkkwd;            /* 1 == check for kwds, 2 == also eat newlines */

struct redirtab {
	struct redirtab *next;
	short renamed[10];
};

extern struct redirtab *redirlist;

extern char sigmode[NSIG - 1];	/* current value of signal */

extern char **environ;



/*
 * Initialization code.
 */

static void
init() {

      /* from cd.c: */
      {
	      setpwd(0, 0);
      }

      /* from input.c: */
      {
	      basepf.nextc = basepf.buf = basebuf;
      }

      /* from output.c: */
      {
#ifdef USE_GLIBC_STDIO
	      initstreams();
#endif
      }

      /* from var.c: */
      {
	      char **envp;
	      char ppid[32];

	      initvar();
	      for (envp = environ ; *envp ; envp++) {
		      if (strchr(*envp, '=')) {
			      setvareq(*envp, VEXPORT|VTEXTFIXED);
		      }
	      }

	      fmtstr(ppid, sizeof(ppid), "%d", (int) getppid());
	      setvar("PPID", ppid, 0);
      }
}



/*
 * This routine is called when an error or an interrupt occurs in an
 * interactive shell and control is returned to the main command loop.
 */

static void
reset() {

      /* from eval.c: */
      {
	      evalskip = 0;
	      loopnest = 0;
	      funcnest = 0;
      }

      /* from input.c: */
      {
	      if (exception != EXSHELLPROC)
		      parselleft = parsenleft = 0;	/* clear input buffer */
	      popallfiles();
      }

      /* from parser.c: */
      {
	      tokpushback = 0;
	      checkkwd = 0;
	      checkalias = 0;
      }

      /* from redir.c: */
      {
	      while (redirlist)
		      popredir();
      }

      /* from output.c: */
      {
	      out1 = &output;
	      out2 = &errout;
#ifdef USE_GLIBC_STDIO
	      if (memout.stream != NULL)
		      __closememout();
#endif
	      if (memout.buf != NULL) {
		      ckfree(memout.buf);
		      memout.buf = NULL;
	      }
      }
}



/*
 * This routine is called to initialize the shell to run a shell procedure.
 */

static void
initshellproc() {

      /* from alias.c: */
      {
	      rmaliases();
      }

      /* from eval.c: */
      {
	      exitstatus = 0;
      }

      /* from exec.c: */
      {
	      deletefuncs();
      }

      /* from jobs.c: */
      {
	      backgndpid = -1;
#if JOBS
	      jobctl = 0;
#endif
      }

      /* from options.c: */
      {
	      int i;

	      for (i = 0; i < NOPTS; i++)
		      optlist[i].val = 0;
	      optschanged();

      }

      /* from redir.c: */
      {
	      clearredir();
      }

      /* from trap.c: */
      {
	      char *sm;

	      clear_traps();
	      for (sm = sigmode ; sm < sigmode + NSIG - 1; sm++) {
		      if (*sm == S_IGN)
			      *sm = S_HARD_IGN;
	      }
      }

      /* from var.c: */
      {
	      shprocvar();
      }
}
/*	$NetBSD: input.c,v 1.35 2001/02/04 19:52:06 christos Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

/*
 * This file implements the input routines used by the parser.
 */

#ifdef BB_FEATURE_COMMAND_EDITING
unsigned int shell_context;
static const char * cmdedit_prompt;
static inline void putprompt(const char *s) {
    cmdedit_prompt = s;
}
#else
static inline void putprompt(const char *s) {
    out2str(s);
}
#endif

#define EOF_NLEFT -99		/* value of parsenleft when EOF pushed back */

static int plinno = 1;			/* input line number */
static int parsenleft;			/* copy of parsefile->nleft */
static int parselleft;		/* copy of parsefile->lleft */
static char *parsenextc;		/* copy of parsefile->nextc */
struct parsefile basepf;	/* top level input file */
static char basebuf[BUFSIZ];	/* buffer for top level input file */
struct parsefile *parsefile = &basepf;	/* current input file */
static int whichprompt;		/* 1 == PS1, 2 == PS2 */

static void pushfile __P((void));
static int preadfd __P((void));

#ifdef mkinit
INCLUDE <stdio.h>
INCLUDE "input.h"
INCLUDE "error.h"

INIT {
	basepf.nextc = basepf.buf = basebuf;
}

RESET {
	if (exception != EXSHELLPROC)
		parselleft = parsenleft = 0;	/* clear input buffer */
	popallfiles();
}
#endif


/*
 * Read a line from the script.
 */

static char *
pfgets(line, len)
	char *line;
	int len;
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
 * Read a character from the script, returning PEOF on end of file.
 * Nul characters in the input are silently discarded.
 */

static int
pgetc()
{
	return pgetc_macro();
}


/*
 * Same as pgetc(), but ignores PEOA.
 */

static int
pgetc2()
{
	int c;
	do {
		c = pgetc_macro();
	} while (c == PEOA);
	return c;
}


static int
preadfd()
{
    int nr;
    char *buf =  parsefile->buf;
    parsenextc = buf;

retry:
#ifdef BB_FEATURE_COMMAND_EDITING
	{
	    if (parsefile->fd)
		nr = read(parsefile->fd, buf, BUFSIZ - 1);
	    else { 
		do {
		    cmdedit_read_input((char*)cmdedit_prompt, buf);
		    nr = strlen(buf);
		} while (nr <=0 || shell_context);
		cmdedit_terminate();
	    }
	}
#else
	nr = read(parsefile->fd, buf, BUFSIZ - 1);
#endif

	if (nr < 0) {
		if (errno == EINTR)
			goto retry;
		if (parsefile->fd == 0 && errno == EWOULDBLOCK) {
			int flags = fcntl(0, F_GETFL, 0);
			if (flags >= 0 && flags & O_NONBLOCK) {
				flags &=~ O_NONBLOCK;
				if (fcntl(0, F_SETFL, flags) >= 0) {
					out2str("sh: turning off NDELAY mode\n");
					goto retry;
				}
			}
		}
	}
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
preadbuffer()
{
	char *p, *q;
	int more;
	char savec;

	while (parsefile->strpush) {
		if (
			parsenleft == -1 && parsefile->strpush->ap &&
			parsenextc[-1] != ' ' && parsenextc[-1] != '\t'
		) {
			return PEOA;
		}
		popstring();
		if (--parsenleft >= 0)
			return (*parsenextc++);
	}
	if (parsenleft == EOF_NLEFT || parsefile->buf == NULL)
		return PEOF;
	flushout(&output);
#ifdef FLUSHERR
	flushout(&errout);
#endif

again:
	if (parselleft <= 0) {
		if ((parselleft = preadfd()) <= 0) {
			parselleft = parsenleft = EOF_NLEFT;
			return PEOF;
		}
	}

	q = p = parsenextc;

	/* delete nul characters */
	for (more = 1; more;) {
		switch (*p) {
		case '\0':
			p++;	/* Skip nul */
			goto check;


		case '\n':
			parsenleft = q - parsenextc;
			more = 0; /* Stop processing here */
			break;
		}

		*q++ = *p++;
check:
		if (--parselleft <= 0 && more) {
			parsenleft = q - parsenextc - 1;
			if (parsenleft < 0)
				goto again;
			more = 0;
		}
	}

	savec = *q;
	*q = '\0';

	if (vflag) {
		out2str(parsenextc);
#ifdef FLUSHERR
		flushout(out2);
#endif
	}

	*q = savec;

	return *parsenextc++;
}

/*
 * Undo the last call to pgetc.  Only one character may be pushed back.
 * PEOF may be pushed back.
 */

static void
pungetc() {
	parsenleft++;
	parsenextc--;
}

/*
 * Push a string back onto the input at this current parsefile level.
 * We handle aliases this way.
 */
static void
pushstring(s, len, ap)
	char *s;
	int len;
	void *ap;
	{
	struct strpush *sp;

	INTOFF;
/*dprintf("*** calling pushstring: %s, %d\n", s, len);*/
	if (parsefile->strpush) {
		sp = ckmalloc(sizeof (struct strpush));
		sp->prev = parsefile->strpush;
		parsefile->strpush = sp;
	} else
		sp = parsefile->strpush = &(parsefile->basestrpush);
	sp->prevstring = parsenextc;
	sp->prevnleft = parsenleft;
	sp->ap = (struct alias *)ap;
	if (ap) {
		((struct alias *)ap)->flag |= ALIASINUSE;
		sp->string = s;
	}
	parsenextc = s;
	parsenleft = len;
	INTON;
}

static void
popstring()
{
	struct strpush *sp = parsefile->strpush;

	INTOFF;
	if (sp->ap) {
		if (parsenextc[-1] == ' ' || parsenextc[-1] == '\t') {
			if (!checkalias) {
				checkalias = 1;
			}
		}
		if (sp->string != sp->ap->val) {
			ckfree(sp->string);
		}
		sp->ap->flag &= ~ALIASINUSE;
		if (sp->ap->flag & ALIASDEAD) {
			unalias(sp->ap->name);
		}
	}
	parsenextc = sp->prevstring;
	parsenleft = sp->prevnleft;
/*dprintf("*** calling popstring: restoring to '%s'\n", parsenextc);*/
	parsefile->strpush = sp->prev;
	if (sp != &(parsefile->basestrpush))
		ckfree(sp);
	INTON;
}

/*
 * Set the input to take input from a file.  If push is set, push the
 * old input onto the stack first.
 */

static void
setinputfile(fname, push)
	const char *fname;
	int push;
{
	int fd;
	int myfileno2;

	INTOFF;
	if ((fd = open(fname, O_RDONLY)) < 0)
		error("Can't open %s", fname);
	if (fd < 10) {
		myfileno2 = dup_as_newfd(fd, 10);
		close(fd);
		if (myfileno2 < 0)
			error("Out of file descriptors");
		fd = myfileno2;
	}
	setinputfd(fd, push);
	INTON;
}


/*
 * Like setinputfile, but takes an open file descriptor.  Call this with
 * interrupts off.
 */

static void
setinputfd(fd, push)
	int fd, push;
{
	(void) fcntl(fd, F_SETFD, FD_CLOEXEC);
	if (push) {
		pushfile();
		parsefile->buf = 0;
	} else {
		closescript();
		while (parsefile->strpush)
			popstring();
	}
	parsefile->fd = fd;
	if (parsefile->buf == NULL)
		parsefile->buf = ckmalloc(BUFSIZ);
	parselleft = parsenleft = 0;
	plinno = 1;
}


/*
 * Like setinputfile, but takes input from a string.
 */

static void
setinputstring(string)
	char *string;
	{
	INTOFF;
	pushfile();
	parsenextc = string;
	parsenleft = strlen(string);
	parsefile->buf = NULL;
	plinno = 1;
	INTON;
}



/*
 * To handle the "." command, a stack of input files is used.  Pushfile
 * adds a new entry to the stack and popfile restores the previous level.
 */

static void
pushfile() {
	struct parsefile *pf;

	parsefile->nleft = parsenleft;
	parsefile->lleft = parselleft;
	parsefile->nextc = parsenextc;
	parsefile->linno = plinno;
	pf = (struct parsefile *)ckmalloc(sizeof (struct parsefile));
	pf->prev = parsefile;
	pf->fd = -1;
	pf->strpush = NULL;
	pf->basestrpush.prev = NULL;
	parsefile = pf;
}


static void
popfile() {
	struct parsefile *pf = parsefile;

	INTOFF;
	if (pf->fd >= 0)
		close(pf->fd);
	if (pf->buf)
		ckfree(pf->buf);
	while (pf->strpush)
		popstring();
	parsefile = pf->prev;
	ckfree(pf);
	parsenleft = parsefile->nleft;
	parselleft = parsefile->lleft;
	parsenextc = parsefile->nextc;
	plinno = parsefile->linno;
	INTON;
}


/*
 * Return to top level.
 */

static void
popallfiles() {
	while (parsefile != &basepf)
		popfile();
}



/*
 * Close the file(s) that the shell is reading commands from.  Called
 * after a fork is done.
 */

static void
closescript() {
	popallfiles();
	if (parsefile->fd > 0) {
		close(parsefile->fd);
		parsefile->fd = 0;
	}
}
/*	$NetBSD: jobs.c,v 1.36 2000/05/22 10:18:47 elric Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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


struct job *jobtab;		/* array of jobs */
static int njobs;			/* size of array */
short backgndpid = -1;	/* pid of last background process */
#if JOBS
static int initialpgrp;		/* pgrp of shell on invocation */
short curjob;			/* current job */
#endif
static int intreceived;

static void restartjob __P((struct job *));
static void freejob __P((struct job *));
static struct job *getjob __P((char *));
static int dowait __P((int, struct job *));
#ifdef SYSV
static int onsigchild __P((void));
#endif
static int waitproc __P((int, int *));
static void cmdtxt __P((union node *));
static void cmdputs __P((const char *));
static void waitonint(int);


#if JOBS
/*
 * Turn job control on and off.
 *
 * Note:  This code assumes that the third arg to ioctl is a character
 * pointer, which is true on Berkeley systems but not System V.  Since
 * System V doesn't have job control yet, this isn't a problem now.
 */

static int jobctl;

static void setjobctl(int enable)
{
#ifdef OLD_TTY_DRIVER
	int ldisc;
#endif

	if (enable == jobctl || rootshell == 0)
		return;
	if (enable) {
		do { /* while we are in the background */
#ifdef OLD_TTY_DRIVER
			if (ioctl(fileno2, TIOCGPGRP, (char *)&initialpgrp) < 0) {
#else
			initialpgrp = tcgetpgrp(fileno2);
			if (initialpgrp < 0) {
#endif
				out2str("sh: can't access tty; job cenabletrol turned off\n");
				mflag = 0;
				return;
			}
			if (initialpgrp == -1)
				initialpgrp = getpgrp();
			else if (initialpgrp != getpgrp()) {
				killpg(initialpgrp, SIGTTIN);
				continue;
			}
		} while (0);
#ifdef OLD_TTY_DRIVER
		if (ioctl(fileno2, TIOCGETD, (char *)&ldisc) < 0 || ldisc != NTTYDISC) {
			out2str("sh: need new tty driver to run job cenabletrol; job cenabletrol turned off\n");
			mflag = 0;
			return;
		}
#endif
		setsignal(SIGTSTP);
		setsignal(SIGTTOU);
		setsignal(SIGTTIN);
		setpgid(0, rootpid);
#ifdef OLD_TTY_DRIVER
		ioctl(fileno2, TIOCSPGRP, (char *)&rootpid);
#else
		tcsetpgrp(fileno2, rootpid);
#endif
	} else { /* turning job cenabletrol off */
		setpgid(0, initialpgrp);
#ifdef OLD_TTY_DRIVER
		ioctl(fileno2, TIOCSPGRP, (char *)&initialpgrp);
#else
		tcsetpgrp(fileno2, initialpgrp);
#endif
		setsignal(SIGTSTP);
		setsignal(SIGTTOU);
		setsignal(SIGTTIN);
	}
	jobctl = enable;
}
#endif


#ifdef mkinit
INCLUDE <stdlib.h>

SHELLPROC {
	backgndpid = -1;
#if JOBS
	jobctl = 0;
#endif
}

#endif


/* This file was automatically created by ./mksignames.
   Do not edit.  Edit support/mksignames.c instead. */

/* A translation list so we can be polite to our users. */
static char *signal_names[NSIG + 2] = {
    "EXIT",
    "SIGHUP",
    "SIGINT",
    "SIGQUIT",
    "SIGILL",
    "SIGTRAP",
    "SIGABRT",
    "SIGBUS",
    "SIGFPE",
    "SIGKILL",
    "SIGUSR1",
    "SIGSEGV",
    "SIGUSR2",
    "SIGPIPE",
    "SIGALRM",
    "SIGTERM",
    "SIGJUNK(16)",
    "SIGCHLD",
    "SIGCONT",
    "SIGSTOP",
    "SIGTSTP",
    "SIGTTIN",
    "SIGTTOU",
    "SIGURG",
    "SIGXCPU",
    "SIGXFSZ",
    "SIGVTALRM",
    "SIGPROF",
    "SIGWINCH",
    "SIGIO",
    "SIGPWR",
    "SIGSYS",
    "SIGRTMIN",
    "SIGRTMIN+1",
    "SIGRTMIN+2",
    "SIGRTMIN+3",
    "SIGRTMIN+4",
    "SIGRTMIN+5",
    "SIGRTMIN+6",
    "SIGRTMIN+7",
    "SIGRTMIN+8",
    "SIGRTMIN+9",
    "SIGRTMIN+10",
    "SIGRTMIN+11",
    "SIGRTMIN+12",
    "SIGRTMIN+13",
    "SIGRTMIN+14",
    "SIGRTMIN+15",
    "SIGRTMAX-15",
    "SIGRTMAX-14",
    "SIGRTMAX-13",
    "SIGRTMAX-12",
    "SIGRTMAX-11",
    "SIGRTMAX-10",
    "SIGRTMAX-9",
    "SIGRTMAX-8",
    "SIGRTMAX-7",
    "SIGRTMAX-6",
    "SIGRTMAX-5",
    "SIGRTMAX-4",
    "SIGRTMAX-3",
    "SIGRTMAX-2",
    "SIGRTMAX-1",
    "SIGRTMAX",
    "DEBUG",
    (char *)0x0,
};



#if JOBS
static int
killcmd(argc, argv)
	int argc;
	char **argv;
{
	int signo = -1;
	int list = 0;
	int i;
	pid_t pid;
	struct job *jp;

	if (argc <= 1) {
usage:
		error(
"Usage: kill [-s sigspec | -signum | -sigspec] [pid | job]... or\n"
"kill -l [exitstatus]"
		);
	}

	if (*argv[1] == '-') {
		signo = decode_signal(argv[1] + 1, 1);
		if (signo < 0) {
			int c;

			while ((c = nextopt("ls:")) != '\0')
				switch (c) {
				case 'l':
					list = 1;
					break;
				case 's':
					signo = decode_signal(optionarg, 1);
					if (signo < 0) {
						error(
							"invalid signal number or name: %s",
							optionarg
						);
					}
		                        break;
#ifdef DEBUG
				default:
					error(
	"nextopt returned character code 0%o", c);
#endif
			}
		} else
			argptr++;
	}

	if (!list && signo < 0)
		signo = SIGTERM;

	if ((signo < 0 || !*argptr) ^ list) {
		goto usage;
	}

	if (list) {
		if (!*argptr) {
			out1str("0\n");
			for (i = 1; i < NSIG; i++) {
				out1fmt(snlfmt, signal_names[i] + 3);
			}
			return 0;
		}
		signo = atoi(*argptr);
		if (signo > 128)
			signo -= 128;
		if (0 < signo && signo < NSIG)
				out1fmt(snlfmt, signal_names[signo] + 3);
		else
			error("invalid signal number or exit status: %s",
			      *argptr);
		return 0;
	}

	do {
		if (**argptr == '%') {
			jp = getjob(*argptr);
			if (jp->jobctl == 0)
				error("job %s not created under job control",
				      *argptr);
			pid = -jp->ps[0].pid;
		} else
			pid = atoi(*argptr);
		if (kill(pid, signo) != 0)
			error("%s: %s", *argptr, strerror(errno));
	} while (*++argptr);

	return 0;
}

static int
fgcmd(argc, argv)
	int argc;
	char **argv;
{
	struct job *jp;
	int pgrp;
	int status;

	jp = getjob(argv[1]);
	if (jp->jobctl == 0)
		error("job not created under job control");
	pgrp = jp->ps[0].pid;
#ifdef OLD_TTY_DRIVER
	ioctl(fileno2, TIOCSPGRP, (char *)&pgrp);
#else
	tcsetpgrp(fileno2, pgrp);
#endif
	restartjob(jp);
	INTOFF;
	status = waitforjob(jp);
	INTON;
	return status;
}


static int
bgcmd(argc, argv)
	int argc;
	char **argv;
{
	struct job *jp;

	do {
		jp = getjob(*++argv);
		if (jp->jobctl == 0)
			error("job not created under job control");
		restartjob(jp);
	} while (--argc > 1);
	return 0;
}


static void
restartjob(jp)
	struct job *jp;
{
	struct procstat *ps;
	int i;

	if (jp->state == JOBDONE)
		return;
	INTOFF;
	killpg(jp->ps[0].pid, SIGCONT);
	for (ps = jp->ps, i = jp->nprocs ; --i >= 0 ; ps++) {
		if (WIFSTOPPED(ps->status)) {
			ps->status = -1;
			jp->state = 0;
		}
	}
	INTON;
}
#endif


static int
jobscmd(argc, argv)
	int argc;
	char **argv;
{
	showjobs(0);
	return 0;
}


/*
 * Print a list of jobs.  If "change" is nonzero, only print jobs whose
 * statuses have changed since the last call to showjobs.
 *
 * If the shell is interrupted in the process of creating a job, the
 * result may be a job structure containing zero processes.  Such structures
 * will be freed here.
 */

static void
showjobs(change)
	int change;
{
	int jobno;
	int procno;
	int i;
	struct job *jp;
	struct procstat *ps;
	int col;
	char s[64];

	TRACE(("showjobs(%d) called\n", change));
	while (dowait(0, (struct job *)NULL) > 0);
	for (jobno = 1, jp = jobtab ; jobno <= njobs ; jobno++, jp++) {
		if (! jp->used)
			continue;
		if (jp->nprocs == 0) {
			freejob(jp);
			continue;
		}
		if (change && ! jp->changed)
			continue;
		procno = jp->nprocs;
		for (ps = jp->ps ; ; ps++) {	/* for each process */
			if (ps == jp->ps)
				fmtstr(s, 64, "[%d] %ld ", jobno, 
				    (long)ps->pid);
			else
				fmtstr(s, 64, "    %ld ", 
				    (long)ps->pid);
			out1str(s);
			col = strlen(s);
			s[0] = '\0';
			if (ps->status == -1) {
				/* don't print anything */
			} else if (WIFEXITED(ps->status)) {
				fmtstr(s, 64, "Exit %d", 
				       WEXITSTATUS(ps->status));
			} else {
#if JOBS
				if (WIFSTOPPED(ps->status)) 
					i = WSTOPSIG(ps->status);
				else /* WIFSIGNALED(ps->status) */
#endif
					i = WTERMSIG(ps->status);
				if ((i & 0x7F) < NSIG && sys_siglist[i & 0x7F])
					scopy(sys_siglist[i & 0x7F], s);
				else
					fmtstr(s, 64, "Signal %d", i & 0x7F);
				if (WCOREDUMP(ps->status))
					strcat(s, " (core dumped)");
			}
			out1str(s);
			col += strlen(s);
			out1fmt(
				"%*c%s\n", 30 - col >= 0 ? 30 - col : 0, ' ',
				ps->cmd
			);
			if (--procno <= 0)
				break;
		}
		jp->changed = 0;
		if (jp->state == JOBDONE) {
			freejob(jp);
		}
	}
}


/*
 * Mark a job structure as unused.
 */

static void
freejob(jp)
	struct job *jp;
	{
	struct procstat *ps;
	int i;

	INTOFF;
	for (i = jp->nprocs, ps = jp->ps ; --i >= 0 ; ps++) {
		if (ps->cmd != nullstr)
			ckfree(ps->cmd);
	}
	if (jp->ps != &jp->ps0)
		ckfree(jp->ps);
	jp->used = 0;
#if JOBS
	if (curjob == jp - jobtab + 1)
		curjob = 0;
#endif
	INTON;
}



static int
waitcmd(argc, argv)
	int argc;
	char **argv;
{
	struct job *job;
	int status, retval;
	struct job *jp;

	if (--argc > 0) {
start:
		job = getjob(*++argv);
	} else {
		job = NULL;
	}
	for (;;) {	/* loop until process terminated or stopped */
		if (job != NULL) {
			if (job->state) {
				status = job->ps[job->nprocs - 1].status;
				if (! iflag)
					freejob(job);
				if (--argc) {
					goto start;
				}
				if (WIFEXITED(status))
					retval = WEXITSTATUS(status);
#if JOBS
				else if (WIFSTOPPED(status))
					retval = WSTOPSIG(status) + 128;
#endif
				else {
					/* XXX: limits number of signals */
					retval = WTERMSIG(status) + 128;
				}
				return retval;
			}
		} else {
			for (jp = jobtab ; ; jp++) {
				if (jp >= jobtab + njobs) {	/* no running procs */
					return 0;
				}
				if (jp->used && jp->state == 0)
					break;
			}
		}
		if (dowait(2, 0) < 0 && errno == EINTR) {
			return 129;
		}
	}
}



/*
 * Convert a job name to a job structure.
 */

static struct job *
getjob(name)
	char *name;
	{
	int jobno;
	struct job *jp;
	int pid;
	int i;

	if (name == NULL) {
#if JOBS
currentjob:
		if ((jobno = curjob) == 0 || jobtab[jobno - 1].used == 0)
			error("No current job");
		return &jobtab[jobno - 1];
#else
		error("No current job");
#endif
	} else if (name[0] == '%') {
		if (is_digit(name[1])) {
			jobno = number(name + 1);
			if (jobno > 0 && jobno <= njobs
			 && jobtab[jobno - 1].used != 0)
				return &jobtab[jobno - 1];
#if JOBS
		} else if (name[1] == '%' && name[2] == '\0') {
			goto currentjob;
#endif
		} else {
			struct job *found = NULL;
			for (jp = jobtab, i = njobs ; --i >= 0 ; jp++) {
				if (jp->used && jp->nprocs > 0
				 && prefix(name + 1, jp->ps[0].cmd)) {
					if (found)
						error("%s: ambiguous", name);
					found = jp;
				}
			}
			if (found)
				return found;
		}
	} else if (is_number(name)) {
		pid = number(name);
		for (jp = jobtab, i = njobs ; --i >= 0 ; jp++) {
			if (jp->used && jp->nprocs > 0
			 && jp->ps[jp->nprocs - 1].pid == pid)
				return jp;
		}
	}
	error("No such job: %s", name);
	/* NOTREACHED */
}



/*
 * Return a new job structure,
 */

struct job *
makejob(node, nprocs)
	union node *node;
	int nprocs;
{
	int i;
	struct job *jp;

	for (i = njobs, jp = jobtab ; ; jp++) {
		if (--i < 0) {
			INTOFF;
			if (njobs == 0) {
				jobtab = ckmalloc(4 * sizeof jobtab[0]);
			} else {
				jp = ckmalloc((njobs + 4) * sizeof jobtab[0]);
				memcpy(jp, jobtab, njobs * sizeof jp[0]);
				/* Relocate `ps' pointers */
				for (i = 0; i < njobs; i++)
					if (jp[i].ps == &jobtab[i].ps0)
						jp[i].ps = &jp[i].ps0;
				ckfree(jobtab);
				jobtab = jp;
			}
			jp = jobtab + njobs;
			for (i = 4 ; --i >= 0 ; jobtab[njobs++].used = 0);
			INTON;
			break;
		}
		if (jp->used == 0)
			break;
	}
	INTOFF;
	jp->state = 0;
	jp->used = 1;
	jp->changed = 0;
	jp->nprocs = 0;
#if JOBS
	jp->jobctl = jobctl;
#endif
	if (nprocs > 1) {
		jp->ps = ckmalloc(nprocs * sizeof (struct procstat));
	} else {
		jp->ps = &jp->ps0;
	}
	INTON;
	TRACE(("makejob(0x%lx, %d) returns %%%d\n", (long)node, nprocs,
	    jp - jobtab + 1));
	return jp;
}


/*
 * Fork of a subshell.  If we are doing job control, give the subshell its
 * own process group.  Jp is a job structure that the job is to be added to.
 * N is the command that will be evaluated by the child.  Both jp and n may
 * be NULL.  The mode parameter can be one of the following:
 *	FORK_FG - Fork off a foreground process.
 *	FORK_BG - Fork off a background process.
 *	FORK_NOJOB - Like FORK_FG, but don't give the process its own
 *		     process group even if job control is on.
 *
 * When job control is turned off, background processes have their standard
 * input redirected to /dev/null (except for the second and later processes
 * in a pipeline).
 */

static int
forkshell(jp, n, mode)
	union node *n;
	struct job *jp;
	int mode;
{
	int pid;
	int pgrp;
	const char *devnull = _PATH_DEVNULL;
	const char *nullerr = "Can't open %s";

	TRACE(("forkshell(%%%d, 0x%lx, %d) called\n", jp - jobtab, (long)n,
	    mode));
	INTOFF;
	pid = fork();
	if (pid == -1) {
		TRACE(("Fork failed, errno=%d\n", errno));
		INTON;
		error("Cannot fork");
	}
	if (pid == 0) {
		struct job *p;
		int wasroot;
		int i;

		TRACE(("Child shell %d\n", getpid()));
		wasroot = rootshell;
		rootshell = 0;
		closescript();
		INTON;
		clear_traps();
#if JOBS
		jobctl = 0;		/* do job control only in root shell */
		if (wasroot && mode != FORK_NOJOB && mflag) {
			if (jp == NULL || jp->nprocs == 0)
				pgrp = getpid();
			else
				pgrp = jp->ps[0].pid;
			setpgid(0, pgrp);
			if (mode == FORK_FG) {
				/*** this causes superfluous TIOCSPGRPS ***/
#ifdef OLD_TTY_DRIVER
				if (ioctl(fileno2, TIOCSPGRP, (char *)&pgrp) < 0)
					error("TIOCSPGRP failed, errno=%d", errno);
#else
				if (tcsetpgrp(fileno2, pgrp) < 0)
					error("tcsetpgrp failed, errno=%d", errno);
#endif
			}
			setsignal(SIGTSTP);
			setsignal(SIGTTOU);
		} else if (mode == FORK_BG) {
			ignoresig(SIGINT);
			ignoresig(SIGQUIT);
			if ((jp == NULL || jp->nprocs == 0) &&
			    ! fd0_redirected_p ()) {
				close(0);
				if (open(devnull, O_RDONLY) != 0)
					error(nullerr, devnull);
			}
		}
#else
		if (mode == FORK_BG) {
			ignoresig(SIGINT);
			ignoresig(SIGQUIT);
			if ((jp == NULL || jp->nprocs == 0) &&
			    ! fd0_redirected_p ()) {
				close(0);
				if (open(devnull, O_RDONLY) != 0)
					error(nullerr, devnull);
			}
		}
#endif
		for (i = njobs, p = jobtab ; --i >= 0 ; p++)
			if (p->used)
				freejob(p);
		if (wasroot && iflag) {
			setsignal(SIGINT);
			setsignal(SIGQUIT);
			setsignal(SIGTERM);
		}
		return pid;
	}
	if (rootshell && mode != FORK_NOJOB && mflag) {
		if (jp == NULL || jp->nprocs == 0)
			pgrp = pid;
		else
			pgrp = jp->ps[0].pid;
		setpgid(pid, pgrp);
	}
	if (mode == FORK_BG)
		backgndpid = pid;		/* set $! */
	if (jp) {
		struct procstat *ps = &jp->ps[jp->nprocs++];
		ps->pid = pid;
		ps->status = -1;
		ps->cmd = nullstr;
		if (iflag && rootshell && n)
			ps->cmd = commandtext(n);
	}
	INTON;
	TRACE(("In parent shell:  child = %d\n", pid));
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
 * forground process to terminate, and then send itself an interrupt
 * signal if the child process was terminated by an interrupt signal.
 * Unfortunately, some programs want to do a bit of cleanup and then
 * exit on interrupt; unless these processes terminate themselves by
 * sending a signal to themselves (instead of calling exit) they will
 * confuse this approach.
 */

static int
waitforjob(jp)
	struct job *jp;
	{
#if JOBS
	int mypgrp = getpgrp();
#endif
	int status;
	int st;
	struct sigaction act, oact;

	INTOFF;
	intreceived = 0;
#if JOBS
	if (!jobctl) {
#else
	if (!iflag) {
#endif
		sigaction(SIGINT, 0, &act);
		act.sa_handler = waitonint;
		sigaction(SIGINT, &act, &oact);
	}
	TRACE(("waitforjob(%%%d) called\n", jp - jobtab + 1));
	while (jp->state == 0) {
		dowait(1, jp);
	}
#if JOBS
	if (!jobctl) {
#else
	if (!iflag) {
#endif
		sigaction(SIGINT, &oact, 0);
		if (intreceived && trap[SIGINT]) kill(getpid(), SIGINT);
	}
#if JOBS
	if (jp->jobctl) {
#ifdef OLD_TTY_DRIVER
		if (ioctl(fileno2, TIOCSPGRP, (char *)&mypgrp) < 0)
			error("TIOCSPGRP failed, errno=%d\n", errno);
#else
		if (tcsetpgrp(fileno2, mypgrp) < 0)
			error("tcsetpgrp failed, errno=%d\n", errno);
#endif
	}
	if (jp->state == JOBSTOPPED)
		curjob = jp - jobtab + 1;
#endif
	status = jp->ps[jp->nprocs - 1].status;
	/* convert to 8 bits */
	if (WIFEXITED(status))
		st = WEXITSTATUS(status);
#if JOBS
	else if (WIFSTOPPED(status))
		st = WSTOPSIG(status) + 128;
#endif
	else
		st = WTERMSIG(status) + 128;
#if JOBS
	if (jp->jobctl) {
		/*
		 * This is truly gross.
		 * If we're doing job control, then we did a TIOCSPGRP which
		 * caused us (the shell) to no longer be in the controlling
		 * session -- so we wouldn't have seen any ^C/SIGINT.  So, we
		 * intuit from the subprocess exit status whether a SIGINT
		 * occured, and if so interrupt ourselves.  Yuck.  - mycroft
		 */
		if (WIFSIGNALED(status) && WTERMSIG(status) == SIGINT)
			raise(SIGINT);
	}
#endif
	if (! JOBS || jp->state == JOBDONE)
		freejob(jp);
	INTON;
	return st;
}



/*
 * Wait for a process to terminate.
 */

static int
dowait(block, job)
	int block;
	struct job *job;
{
	int pid;
	int status;
	struct procstat *sp;
	struct job *jp;
	struct job *thisjob;
	int done;
	int stopped;
	int core;
	int sig;

	TRACE(("dowait(%d) called\n", block));
	do {
		pid = waitproc(block, &status);
		TRACE(("wait returns %d, status=%d\n", pid, status));
	} while (!(block & 2) && pid == -1 && errno == EINTR);
	if (pid <= 0)
		return pid;
	INTOFF;
	thisjob = NULL;
	for (jp = jobtab ; jp < jobtab + njobs ; jp++) {
		if (jp->used) {
			done = 1;
			stopped = 1;
			for (sp = jp->ps ; sp < jp->ps + jp->nprocs ; sp++) {
				if (sp->pid == -1)
					continue;
				if (sp->pid == pid) {
					TRACE(("Changing status of proc %d from 0x%x to 0x%x\n", pid, sp->status, status));
					sp->status = status;
					thisjob = jp;
				}
				if (sp->status == -1)
					stopped = 0;
				else if (WIFSTOPPED(sp->status))
					done = 0;
			}
			if (stopped) {		/* stopped or done */
				int state = done? JOBDONE : JOBSTOPPED;
				if (jp->state != state) {
					TRACE(("Job %d: changing state from %d to %d\n", jp - jobtab + 1, jp->state, state));
					jp->state = state;
#if JOBS
					if (done && curjob == jp - jobtab + 1)
						curjob = 0;		/* no current job */
#endif
				}
			}
		}
	}
	INTON;
	if (! rootshell || ! iflag || (job && thisjob == job)) {
		core = WCOREDUMP(status);
#if JOBS
		if (WIFSTOPPED(status)) sig = WSTOPSIG(status);
		else
#endif
		if (WIFEXITED(status)) sig = 0;
		else sig = WTERMSIG(status);

		if (sig != 0 && sig != SIGINT && sig != SIGPIPE) {
			if (thisjob != job)
				outfmt(out2, "%d: ", pid);
#if JOBS
			if (sig == SIGTSTP && rootshell && iflag)
				outfmt(out2, "%%%ld ",
				    (long)(job - jobtab + 1));
#endif
			if (sig < NSIG && sys_siglist[sig])
				out2str(sys_siglist[sig]);
			else
				outfmt(out2, "Signal %d", sig);
			if (core)
				out2str(" - core dumped");
			out2c('\n');
#ifdef FLUSHERR
			flushout(&errout);
#endif
		} else {
			TRACE(("Not printing status: status=%d, sig=%d\n", 
			       status, sig));
		}
	} else {
		TRACE(("Not printing status, rootshell=%d, job=0x%x\n", rootshell, job));
		if (thisjob)
			thisjob->changed = 1;
	}
	return pid;
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

#ifdef SYSV
static int gotsigchild;

static int onsigchild() {
	gotsigchild = 1;
}
#endif


static int
waitproc(block, status)
	int block;
	int *status;
{
#ifdef BSD
	int flags;

	flags = 0;
#if JOBS
	if (jobctl)
		flags |= WUNTRACED;
#endif
	if (block == 0)
		flags |= WNOHANG;
	return wait3(status, flags, (struct rusage *)NULL);
#else
#ifdef SYSV
	int (*save)();

	if (block == 0) {
		gotsigchild = 0;
		save = signal(SIGCLD, onsigchild);
		signal(SIGCLD, save);
		if (gotsigchild == 0)
			return 0;
	}
	return wait(status);
#else
	if (block == 0)
		return 0;
	return wait(status);
#endif
#endif
}

/*
 * return 1 if there are stopped jobs, otherwise 0
 */
static int job_warning = 0;
static int
stoppedjobs()
{
	int jobno;
	struct job *jp;

	if (job_warning)
		return (0);
	for (jobno = 1, jp = jobtab; jobno <= njobs; jobno++, jp++) {
		if (jp->used == 0)
			continue;
		if (jp->state == JOBSTOPPED) {
			out2str("You have stopped jobs.\n");
			job_warning = 2;
			return (1);
		}
	}

	return (0);
}

/*
 * Return a string identifying a command (to be printed by the
 * jobs command.
 */

static char *cmdnextc;
static int cmdnleft;
#define MAXCMDTEXT	200

static char *
commandtext(n)
	union node *n;
	{
	char *name;

	cmdnextc = name = ckmalloc(MAXCMDTEXT);
	cmdnleft = MAXCMDTEXT - 4;
	cmdtxt(n);
	*cmdnextc = '\0';
	return name;
}


static void
cmdtxt(n)
	union node *n;
	{
	union node *np;
	struct nodelist *lp;
	const char *p;
	int i;
	char s[2];

	if (n == NULL)
		return;
	switch (n->type) {
	case NSEMI:
		cmdtxt(n->nbinary.ch1);
		cmdputs("; ");
		cmdtxt(n->nbinary.ch2);
		break;
	case NAND:
		cmdtxt(n->nbinary.ch1);
		cmdputs(" && ");
		cmdtxt(n->nbinary.ch2);
		break;
	case NOR:
		cmdtxt(n->nbinary.ch1);
		cmdputs(" || ");
		cmdtxt(n->nbinary.ch2);
		break;
	case NPIPE:
		for (lp = n->npipe.cmdlist ; lp ; lp = lp->next) {
			cmdtxt(lp->n);
			if (lp->next)
				cmdputs(" | ");
		}
		break;
	case NSUBSHELL:
		cmdputs("(");
		cmdtxt(n->nredir.n);
		cmdputs(")");
		break;
	case NREDIR:
	case NBACKGND:
		cmdtxt(n->nredir.n);
		break;
	case NIF:
		cmdputs("if ");
		cmdtxt(n->nif.test);
		cmdputs("; then ");
		cmdtxt(n->nif.ifpart);
		cmdputs("...");
		break;
	case NWHILE:
		cmdputs("while ");
		goto until;
	case NUNTIL:
		cmdputs("until ");
until:
		cmdtxt(n->nbinary.ch1);
		cmdputs("; do ");
		cmdtxt(n->nbinary.ch2);
		cmdputs("; done");
		break;
	case NFOR:
		cmdputs("for ");
		cmdputs(n->nfor.var);
		cmdputs(" in ...");
		break;
	case NCASE:
		cmdputs("case ");
		cmdputs(n->ncase.expr->narg.text);
		cmdputs(" in ...");
		break;
	case NDEFUN:
		cmdputs(n->narg.text);
		cmdputs("() ...");
		break;
	case NCMD:
		for (np = n->ncmd.args ; np ; np = np->narg.next) {
			cmdtxt(np);
			if (np->narg.next)
				cmdputs(spcstr);
		}
		for (np = n->ncmd.redirect ; np ; np = np->nfile.next) {
			cmdputs(spcstr);
			cmdtxt(np);
		}
		break;
	case NARG:
		cmdputs(n->narg.text);
		break;
	case NTO:
		p = ">";  i = 1;  goto redir;
	case NAPPEND:
		p = ">>";  i = 1;  goto redir;
	case NTOFD:
		p = ">&";  i = 1;  goto redir;
	case NTOOV:
		p = ">|";  i = 1;  goto redir;
	case NFROM:
		p = "<";  i = 0;  goto redir;
	case NFROMFD:
		p = "<&";  i = 0;  goto redir;
	case NFROMTO:
		p = "<>";  i = 0;  goto redir;
redir:
		if (n->nfile.fd != i) {
			s[0] = n->nfile.fd + '0';
			s[1] = '\0';
			cmdputs(s);
		}
		cmdputs(p);
		if (n->type == NTOFD || n->type == NFROMFD) {
			s[0] = n->ndup.dupfd + '0';
			s[1] = '\0';
			cmdputs(s);
		} else {
			cmdtxt(n->nfile.fname);
		}
		break;
	case NHERE:
	case NXHERE:
		cmdputs("<<...");
		break;
	default:
		cmdputs("???");
		break;
	}
}



static void
cmdputs(s)
	const char *s;
	{
	const char *p;
	char *q;
	char c;
	int subtype = 0;

	if (cmdnleft <= 0)
		return;
	p = s;
	q = cmdnextc;
	while ((c = *p++) != '\0') {
		if (c == CTLESC)
			*q++ = *p++;
		else if (c == CTLVAR) {
			*q++ = '$';
			if (--cmdnleft > 0)
				*q++ = '{';
			subtype = *p++;
		} else if (c == '=' && subtype != 0) {
			*q++ = "}-+?="[(subtype & VSTYPE) - VSNORMAL];
			subtype = 0;
		} else if (c == CTLENDVAR) {
			*q++ = '}';
		} else if (c == CTLBACKQ || c == CTLBACKQ+CTLQUOTE)
			cmdnleft++;		/* ignore it */
		else
			*q++ = c;
		if (--cmdnleft <= 0) {
			*q++ = '.';
			*q++ = '.';
			*q++ = '.';
			break;
		}
	}
	cmdnextc = q;
}

static void waitonint(int sig) {
	intreceived = 1;
	return;
}
/*	$NetBSD: mail.c,v 1.14 2000/07/03 03:26:19 matt Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

/*
 * Routines to check for mail.  (Perhaps make part of main.c?)
 */


#define MAXMBOXES 10


static int nmboxes;			/* number of mailboxes */
static time_t mailtime[MAXMBOXES];	/* times of mailboxes */



/*
 * Print appropriate message(s) if mail has arrived.  If the argument is
 * nozero, then the value of MAIL has changed, so we just update the
 * values.
 */

static void
chkmail(silent)
	int silent;
{
	int i;
	const char *mpath;
	char *p;
	char *q;
	struct stackmark smark;
	struct stat statb;

	if (silent)
		nmboxes = 10;
	if (nmboxes == 0)
		return;
	setstackmark(&smark);
	mpath = mpathset()? mpathval() : mailval();
	for (i = 0 ; i < nmboxes ; i++) {
		p = padvance(&mpath, nullstr);
		if (p == NULL)
			break;
		if (*p == '\0')
			continue;
		for (q = p ; *q ; q++);
#ifdef DEBUG
		if (q[-1] != '/')
			abort();
#endif
		q[-1] = '\0';			/* delete trailing '/' */
#ifdef notdef /* this is what the System V shell claims to do (it lies) */
		if (stat(p, &statb) < 0)
			statb.st_mtime = 0;
		if (statb.st_mtime > mailtime[i] && ! silent) {
			outfmt(
				&errout, snlfmt,
				pathopt? pathopt : "you have mail"
			);
		}
		mailtime[i] = statb.st_mtime;
#else /* this is what it should do */
		if (stat(p, &statb) < 0)
			statb.st_size = 0;
		if (statb.st_size > mailtime[i] && ! silent) {
			outfmt(
				&errout, snlfmt,
				pathopt? pathopt : "you have mail"
			);
		}
		mailtime[i] = statb.st_size;
#endif
	}
	nmboxes = i;
	popstackmark(&smark);
}
/*	$NetBSD: main.c,v 1.40 2001/02/04 19:52:06 christos Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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


#define PROFILE 0

static int rootpid;
static int rootshell;
#if PROFILE
short profile_buf[16384];
extern int etext();
#endif

static void read_profile __P((const char *));
static char *find_dot_file __P((char *));
int shell_main __P((int, char **));

extern int oexitstatus;
/*
 * Main routine.  We initialize things, parse the arguments, execute
 * profiles if we're a login shell, and then call cmdloop to execute
 * commands.  The setjmp call sets up the location to jump to when an
 * exception occurs.  When an exception occurs the variable "state"
 * is used to figure out how far we had gotten.
 */

int
shell_main(argc, argv)
	int argc;
	char **argv;
{
	struct jmploc jmploc;
	struct stackmark smark;
	volatile int state;
	char *shinit;

	DOTCMD = find_builtin(".");
	BLTINCMD = find_builtin("builtin");
	COMMANDCMD = find_builtin("command");
	EXECCMD = find_builtin("exec");
	EVALCMD = find_builtin("eval");

#if PROFILE
	monitor(4, etext, profile_buf, sizeof profile_buf, 50);
#endif
#if defined(linux) || defined(__GNU__)
	signal(SIGCHLD, SIG_DFL);
#endif
	state = 0;
	if (setjmp(jmploc.loc)) {
		INTOFF;
		/*
		 * When a shell procedure is executed, we raise the
		 * exception EXSHELLPROC to clean up before executing
		 * the shell procedure.
		 */
		switch (exception) {
		case EXSHELLPROC:
			rootpid = getpid();
			rootshell = 1;
			minusc = NULL;
			state = 3;
			break;

		case EXEXEC:
			exitstatus = exerrno;
			break;

		case EXERROR:
			exitstatus = 2;
			break;

		default:
			break;
		}

		if (exception != EXSHELLPROC) {
		    if (state == 0 || iflag == 0 || ! rootshell)
			    exitshell(exitstatus);
		}
		reset();
		if (exception == EXINT
#if ATTY
		 && (! attyset() || equal(termval(), "emacs"))
#endif
		 ) {
			out2c('\n');
#ifdef FLUSHERR
			flushout(out2);
#endif
		}
		popstackmark(&smark);
		FORCEINTON;				/* enable interrupts */
		if (state == 1)
			goto state1;
		else if (state == 2)
			goto state2;
		else if (state == 3)
			goto state3;
		else
			goto state4;
	}
	handler = &jmploc;
#ifdef DEBUG
	opentrace();
	trputs("Shell args:  ");  trargs(argv);
#endif
	rootpid = getpid();
	rootshell = 1;
	init();
	setstackmark(&smark);
	procargs(argc, argv);
	if (argv[0] && argv[0][0] == '-') {
		state = 1;
		read_profile("/etc/profile");
state1:
		state = 2;
		read_profile(".profile");
	}
state2:
	state = 3;
#ifndef linux
	if (getuid() == geteuid() && getgid() == getegid()) {
#endif
		if ((shinit = lookupvar("ENV")) != NULL && *shinit != '\0') {
			state = 3;
			read_profile(shinit);
		}
#ifndef linux
	}
#endif
state3:
	state = 4;
	if (sflag == 0 || minusc) {
		static int sigs[] =  {
		    SIGINT, SIGQUIT, SIGHUP, 
#ifdef SIGTSTP
		    SIGTSTP,
#endif
		    SIGPIPE
		};
#define SIGSSIZE (sizeof(sigs)/sizeof(sigs[0]))
		int i;

		for (i = 0; i < SIGSSIZE; i++)
		    setsignal(sigs[i]);
	}

	if (minusc)
		evalstring(minusc, 0);

	if (sflag || minusc == NULL) {
state4:	/* XXX ??? - why isn't this before the "if" statement */
		cmdloop(1);
	}
#if PROFILE
	monitor(0);
#endif
	exitshell(exitstatus);
	/* NOTREACHED */
}


/*
 * Read and execute commands.  "Top" is nonzero for the top level command
 * loop; it turns on prompting if the shell is interactive.
 */

static void
cmdloop(top)
	int top;
{
	union node *n;
	struct stackmark smark;
	int inter;
	int numeof = 0;

	TRACE(("cmdloop(%d) called\n", top));
	setstackmark(&smark);
	for (;;) {
		if (pendingsigs)
			dotrap();
		inter = 0;
		if (iflag && top) {
			inter++;
			showjobs(1);
			chkmail(0);
			flushout(&output);
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
		} else if (n != NULL && nflag == 0) {
			job_warning = (job_warning == 2) ? 1 : 0;
			numeof = 0;
			evaltree(n, 0);
		}
		popstackmark(&smark);
		setstackmark(&smark);
		if (evalskip == SKIPFILE) {
			evalskip = 0;
			break;
		}
	}
	popstackmark(&smark);
}



/*
 * Read /etc/profile or .profile.  Return on error.
 */

static void
read_profile(name)
	const char *name;
{
	int fd;
	int xflag_set = 0;
	int vflag_set = 0;

	INTOFF;
	if ((fd = open(name, O_RDONLY)) >= 0)
		setinputfd(fd, 1);
	INTON;
	if (fd < 0)
		return;
	/* -q turns off -x and -v just when executing init files */
	if (qflag)  {
	    if (xflag)
		    xflag = 0, xflag_set = 1;
	    if (vflag)
		    vflag = 0, vflag_set = 1;
	}
	cmdloop(0);
	if (qflag)  {
	    if (xflag_set)
		    xflag = 1;
	    if (vflag_set)
		    vflag = 1;
	}
	popfile();
}



/*
 * Read a file containing shell functions.
 */

static void
readcmdfile(name)
	char *name;
{
	int fd;

	INTOFF;
	if ((fd = open(name, O_RDONLY)) >= 0)
		setinputfd(fd, 1);
	else
		error("Can't open %s", name);
	INTON;
	cmdloop(0);
	popfile();
}



/*
 * Take commands from a file.  To be compatable we should do a path
 * search for the file, which is necessary to find sub-commands.
 */


static char *
find_dot_file(mybasename)
	char *mybasename;
{
	char *fullname;
	const char *path = pathval();
	struct stat statb;

	/* don't try this for absolute or relative paths */
	if (strchr(mybasename, '/'))
		return mybasename;

	while ((fullname = padvance(&path, mybasename)) != NULL) {
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
	error("%s: not found", mybasename);
	/* NOTREACHED */
}

static int
dotcmd(argc, argv)
	int argc;
	char **argv;
{
	struct strlist *sp;
	exitstatus = 0;

	for (sp = cmdenviron; sp ; sp = sp->next)
		setvareq(savestr(sp->text), VSTRFIXED|VTEXTFIXED);

	if (argc >= 2) {		/* That's what SVR2 does */
		char *fullname;
		struct stackmark smark;

		setstackmark(&smark);
		fullname = find_dot_file(argv[1]);
		setinputfile(fullname, 1);
		commandname = fullname;
		cmdloop(0);
		popfile();
		popstackmark(&smark);
	}
	return exitstatus;
}


static int
exitcmd(argc, argv)
	int argc;
	char **argv;
{
	if (stoppedjobs())
		return 0;
	if (argc > 1)
		exitstatus = number(argv[1]);
	else
		exitstatus = oexitstatus;
	exitshell(exitstatus);
	/* NOTREACHED */
}
/*	$NetBSD: memalloc.c,v 1.23 2000/11/01 19:56:01 christos Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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


/*
 * Parse trees for commands are allocated in lifo order, so we use a stack
 * to make this more efficient, and also to avoid all sorts of exception
 * handling code to handle interrupts in the middle of a parse.
 *
 * The size 504 was chosen because the Ultrix malloc handles that size
 * well.
 */

#define MINSIZE 504		/* minimum size of a block */


struct stack_block {
	struct stack_block *prev;
	char space[MINSIZE];
};

struct stack_block stackbase;
struct stack_block *stackp = &stackbase;
struct stackmark *markp;
static char *stacknxt = stackbase.space;
static int stacknleft = MINSIZE;
static int sstrnleft;
static int herefd = -1;



pointer
stalloc(nbytes)
	int nbytes;
{
	char *p;

	nbytes = ALIGN(nbytes);
	if (nbytes > stacknleft) {
		int blocksize;
		struct stack_block *sp;

		blocksize = nbytes;
		if (blocksize < MINSIZE)
			blocksize = MINSIZE;
		INTOFF;
		sp = ckmalloc(sizeof(struct stack_block) - MINSIZE + blocksize);
		sp->prev = stackp;
		stacknxt = sp->space;
		stacknleft = blocksize;
		stackp = sp;
		INTON;
	}
	p = stacknxt;
	stacknxt += nbytes;
	stacknleft -= nbytes;
	return p;
}


static void
stunalloc(p)
	pointer p;
	{
#ifdef DEBUG
	if (p == NULL) {		/*DEBUG */
		write(2, "stunalloc\n", 10);
		abort();
	}
#endif
	if (!(stacknxt >= (char *)p && (char *)p >= stackp->space)) {
		p = stackp->space;
	}
	stacknleft += stacknxt - (char *)p;
	stacknxt = p;
}



static void
setstackmark(mark)
	struct stackmark *mark;
	{
	mark->stackp = stackp;
	mark->stacknxt = stacknxt;
	mark->stacknleft = stacknleft;
	mark->marknext = markp;
	markp = mark;
}


static void
popstackmark(mark)
	struct stackmark *mark;
	{
	struct stack_block *sp;

	INTOFF;
	markp = mark->marknext;
	while (stackp != mark->stackp) {
		sp = stackp;
		stackp = sp->prev;
		ckfree(sp);
	}
	stacknxt = mark->stacknxt;
	stacknleft = mark->stacknleft;
	INTON;
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
growstackblock() {
	char *p;
	int newlen = ALIGN(stacknleft * 2 + 100);
	char *oldspace = stacknxt;
	int oldlen = stacknleft;
	struct stack_block *sp;
	struct stack_block *oldstackp;

	if (stacknxt == stackp->space && stackp != &stackbase) {
		INTOFF;
		oldstackp = stackp;
		sp = stackp;
		stackp = sp->prev;
		sp = ckrealloc((pointer)sp, sizeof(struct stack_block) - MINSIZE + newlen);
		sp->prev = stackp;
		stackp = sp;
		stacknxt = sp->space;
		stacknleft = newlen;
		{
		  /* Stack marks pointing to the start of the old block
		   * must be relocated to point to the new block 
		   */
		  struct stackmark *xmark;
		  xmark = markp;
		  while (xmark != NULL && xmark->stackp == oldstackp) {
		    xmark->stackp = stackp;
		    xmark->stacknxt = stacknxt;
		    xmark->stacknleft = stacknleft;
		    xmark = xmark->marknext;
		  }
		}
		INTON;
	} else {
		p = stalloc(newlen);
		memcpy(p, oldspace, oldlen);
		stacknxt = p;			/* free the space */
		stacknleft += newlen;		/* we just allocated */
	}
}



static void
grabstackblock(len)
	int len;
{
	len = ALIGN(len);
	stacknxt += len;
	stacknleft -= len;
}



/*
 * The following routines are somewhat easier to use that the above.
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


static char *
growstackstr() {
	int len = stackblocksize();
	if (herefd >= 0 && len >= 1024) {
		xwrite(herefd, stackblock(), len);
		sstrnleft = len - 1;
		return stackblock();
	}
	growstackblock();
	sstrnleft = stackblocksize() - len - 1;
	return stackblock() + len;
}


/*
 * Called from CHECKSTRSPACE.
 */

static char *
makestrspace(size_t newlen) {
	int len = stackblocksize() - sstrnleft;
	do {
		growstackblock();
		sstrnleft = stackblocksize() - len;
	} while (sstrnleft < newlen);
	return stackblock() + len;
}



static void
ungrabstackstr(s, p)
	char *s;
	char *p;
	{
	stacknleft += stacknxt - s;
	stacknxt = s;
	sstrnleft = stacknleft - (p - s);
}
/*	$NetBSD: miscbltin.c,v 1.30 2001/02/04 19:52:06 christos Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

/*
 * Miscelaneous builtins.
 */


#undef rflag

#ifdef __GLIBC__
mode_t getmode(const void *, mode_t);
static void *setmode(const char *);

#if !defined(__GLIBC__) || __GLIBC__ == 2 && __GLIBC_MINOR__ < 1
typedef enum __rlimit_resource rlim_t;
#endif
#endif



/*
 * The read builtin.  The -e option causes backslashes to escape the
 * following character.
 *
 * This uses unbuffered input, which may be avoidable in some cases.
 */

static int
readcmd(argc, argv)
	int argc;
	char **argv;
{
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

	rflag = 0;
	prompt = NULL;
	while ((i = nextopt("p:r")) != '\0') {
		if (i == 'p')
			prompt = optionarg;
		else
			rflag = 1;
	}
	if (prompt && isatty(0)) {
		putprompt(prompt);
		flushall();
	}
	if (*(ap = argptr) == NULL)
		error("arg count");
	if ((ifs = bltinlookup("IFS")) == NULL)
		ifs = defifs;
	status = 0;
	startword = 1;
	backslash = 0;
	STARTSTACKSTR(p);
	for (;;) {
		if (read(0, &c, 1) != 1) {
			status = 1;
			break;
		}
		if (c == '\0')
			continue;
		if (backslash) {
			backslash = 0;
			if (c != '\n')
				STPUTC(c, p);
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
		if (backslash && c == '\\') {
			if (read(0, &c, 1) != 1) {
				status = 1;
				break;
			}
			STPUTC(c, p);
		} else if (ap[1] != NULL && strchr(ifs, c) != NULL) {
			STACKSTRNUL(p);
			setvar(*ap, stackblock(), 0);
			ap++;
			startword = 1;
			STARTSTACKSTR(p);
		} else {
			STPUTC(c, p);
		}
	}
	STACKSTRNUL(p);
	/* Remove trailing blanks */
	while (stackblock() <= --p && strchr(ifs, *p) != NULL)
		*p = '\0';
	setvar(*ap, stackblock(), 0);
	while (*++ap != NULL)
		setvar(*ap, nullstr, 0);
	return status;
}



static int
umaskcmd(argc, argv)
	int argc;
	char **argv;
{
	char *ap;
	int mask;
	int i;
	int symbolic_mode = 0;

	while ((i = nextopt("S")) != '\0') {
		symbolic_mode = 1;
	}

	INTOFF;
	mask = umask(0);
	umask(mask);
	INTON;

	if ((ap = *argptr) == NULL) {
		if (symbolic_mode) {
			char u[4], g[4], o[4];

			i = 0;
			if ((mask & S_IRUSR) == 0)
				u[i++] = 'r';
			if ((mask & S_IWUSR) == 0)
				u[i++] = 'w';
			if ((mask & S_IXUSR) == 0)
				u[i++] = 'x';
			u[i] = '\0';

			i = 0;
			if ((mask & S_IRGRP) == 0)
				g[i++] = 'r';
			if ((mask & S_IWGRP) == 0)
				g[i++] = 'w';
			if ((mask & S_IXGRP) == 0)
				g[i++] = 'x';
			g[i] = '\0';

			i = 0;
			if ((mask & S_IROTH) == 0)
				o[i++] = 'r';
			if ((mask & S_IWOTH) == 0)
				o[i++] = 'w';
			if ((mask & S_IXOTH) == 0)
				o[i++] = 'x';
			o[i] = '\0';

			out1fmt("u=%s,g=%s,o=%s\n", u, g, o);
		} else {
			out1fmt("%.4o\n", mask);
		}
	} else {
		if (isdigit((unsigned char)*ap)) {
			mask = 0;
			do {
				if (*ap >= '8' || *ap < '0')
					error("Illegal number: %s", argv[1]);
				mask = (mask << 3) + (*ap - '0');
			} while (*++ap != '\0');
			umask(mask);
		} else {
			void *set;

			INTOFF;
			if ((set = setmode(ap)) != 0) {
				mask = getmode(set, ~mask & 0777);
				ckfree(set);
			}
			INTON;
			if (!set)
				error("Illegal mode: %s", ap);

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
	const char *name;
	int	cmd;
	int	factor;	/* multiply by to get rlim_{cur,max} values */
	char	option;
};

static const struct limits limits[] = {
#ifdef RLIMIT_CPU
	{ "time(seconds)",		RLIMIT_CPU,	   1, 't' },
#endif
#ifdef RLIMIT_FSIZE
	{ "file(blocks)",		RLIMIT_FSIZE,	 512, 'f' },
#endif
#ifdef RLIMIT_DATA
	{ "data(kbytes)",		RLIMIT_DATA,	1024, 'd' },
#endif
#ifdef RLIMIT_STACK
	{ "stack(kbytes)",		RLIMIT_STACK,	1024, 's' },
#endif
#ifdef  RLIMIT_CORE
	{ "coredump(blocks)",		RLIMIT_CORE,	 512, 'c' },
#endif
#ifdef RLIMIT_RSS
	{ "memory(kbytes)",		RLIMIT_RSS,	1024, 'm' },
#endif
#ifdef RLIMIT_MEMLOCK
	{ "locked memory(kbytes)",	RLIMIT_MEMLOCK, 1024, 'l' },
#endif
#ifdef RLIMIT_NPROC
	{ "process(processes)",		RLIMIT_NPROC,      1, 'p' },
#endif
#ifdef RLIMIT_NOFILE
	{ "nofiles(descriptors)",	RLIMIT_NOFILE,     1, 'n' },
#endif
#ifdef RLIMIT_VMEM
	{ "vmemory(kbytes)",		RLIMIT_VMEM,	1024, 'v' },
#endif
#ifdef RLIMIT_SWAP
	{ "swap(kbytes)",		RLIMIT_SWAP,	1024, 'w' },
#endif
	{ (char *) 0,			0,		   0,  '\0' }
};

static int
ulimitcmd(argc, argv)
	int argc;
	char **argv;
{
	int	c;
	rlim_t val = 0;
	enum { SOFT = 0x1, HARD = 0x2 }
			how = SOFT | HARD;
	const struct limits	*l;
	int		set, all = 0;
	int		optc, what;
	struct rlimit	limit;

	what = 'f';
	while ((optc = nextopt("HSatfdsmcnpl")) != '\0')
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

	for (l = limits; l->name && l->option != what; l++)
		;
	if (!l->name)
		error("internal error (%c)", what);

	set = *argptr ? 1 : 0;
	if (set) {
		char *p = *argptr;

		if (all || argptr[1])
			error("too many arguments");
		if (strcmp(p, "unlimited") == 0)
			val = RLIM_INFINITY;
		else {
			val = (rlim_t) 0;

			while ((c = *p++) >= '0' && c <= '9')
			{
				val = (val * 10) + (long)(c - '0');
				if (val < (rlim_t) 0)
					break;
			}
			if (c)
				error("bad number");
			val *= l->factor;
		}
	}
	if (all) {
		for (l = limits; l->name; l++) {
			getrlimit(l->cmd, &limit);
			if (how & SOFT)
				val = limit.rlim_cur;
			else if (how & HARD)
				val = limit.rlim_max;

			out1fmt("%-20s ", l->name);
			if (val == RLIM_INFINITY)
				out1fmt("unlimited\n");
			else
			{
				val /= l->factor;
#ifdef BSD4_4
				out1fmt("%lld\n", (long long) val);
#else
				out1fmt("%ld\n", (long) val);
#endif
			}
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
			error("error setting limit (%s)", strerror(errno));
	} else {
		if (how & SOFT)
			val = limit.rlim_cur;
		else if (how & HARD)
			val = limit.rlim_max;

		if (val == RLIM_INFINITY)
			out1fmt("unlimited\n");
		else
		{
			val /= l->factor;
#ifdef BSD4_4
			out1fmt("%lld\n", (long long) val);
#else
			out1fmt("%ld\n", (long) val);
#endif
		}
	}
	return 0;
}
/*	$NetBSD: mystring.c,v 1.14 1999/07/09 03:05:50 christos Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

/*
 * String functions.
 *
 *	equal(s1, s2)		Return true if strings are equal.
 *	scopy(from, to)		Copy a string.
 *	scopyn(from, to, n)	Like scopy, but checks for overflow.
 *	number(s)		Convert a string of digits to an integer.
 *	is_number(s)		Return true if s is a string of digits.
 */

static char nullstr[1];		/* zero length string */
static const char spcstr[] = " ";
static const char snlfmt[] = "%s\n";

/*
 * equal - #defined in mystring.h
 */

/*
 * scopy - #defined in mystring.h
 */


#if 0
/*
 * scopyn - copy a string from "from" to "to", truncating the string
 *		if necessary.  "To" is always nul terminated, even if
 *		truncation is performed.  "Size" is the size of "to".
 */

static void
scopyn(from, to, size)
	char const *from;
	char *to;
	int size;
	{

	while (--size > 0) {
		if ((*to++ = *from++) == '\0')
			return;
	}
	*to = '\0';
}
#endif


/*
 * prefix -- see if pfx is a prefix of string.
 */

static int
prefix(pfx, string)
	char const *pfx;
	char const *string;
	{
	while (*pfx) {
		if (*pfx++ != *string++)
			return 0;
	}
	return 1;
}


/*
 * Convert a string of digits to an integer, printing an error message on
 * failure.
 */

static int
number(s)
	const char *s;
	{

	if (! is_number(s))
		error("Illegal number: %s", s);
	return atoi(s);
}



/*
 * Check for a valid number.  This should be elsewhere.
 */

static int
is_number(p)
	const char *p;
	{
	do {
		if (! is_digit(*p))
			return 0;
	} while (*++p != '\0');
	return 1;
}


/*
 * Produce a possibly single quoted string suitable as input to the shell.
 * The return string is allocated on the stack.
 */

static char *
single_quote(const char *s) {
	char *p;

	STARTSTACKSTR(p);

	do {
		char *q = p;
		size_t len1, len1p, len2, len2p;

		len1 = strcspn(s, "'");
		len2 = strspn(s + len1, "'");

		len1p = len1 ? len1 + 2 : len1;
		switch (len2) {
		case 0:
			len2p = 0;
			break;
		case 1:
			len2p = 2;
			break;
		default:
			len2p = len2 + 2;
		}

		CHECKSTRSPACE(len1p + len2p + 1, p);

		if (len1) {
			*p = '\'';
#ifdef _GNU_SOURCE
			q = mempcpy(p + 1, s, len1);
#else
			q = p + 1 + len1;
			memcpy(p + 1, s, len1);
#endif
			*q++ = '\'';
			s += len1;
		}

		switch (len2) {
		case 0:
			break;
		case 1:
			*q++ = '\\';
			*q = '\'';
			s++;
			break;
		default:
			*q = '"';
#ifdef _GNU_SOURCE
			*(char *) mempcpy(q + 1, s, len2) = '"';
#else
			q += 1 + len2;
			memcpy(q + 1, s, len2);
			*q = '"';
#endif
			s += len2;
		}

		STADJUST(len1p + len2p, p);
	} while (*s);

	USTPUTC(0, p);

	return grabstackstr(p);
}

/*
 * Like strdup but works with the ash stack.
 */

static char *
sstrdup(const char *p)
{
	size_t len = strlen(p) + 1;
	return memcpy(stalloc(len), p, len);
}

/*
 * Wrapper around strcmp for qsort/bsearch/...
 */
static int
pstrcmp(const void *a, const void *b)
{
	return strcmp(*(const char *const *) a, *(const char *const *) b);
}

/*
 * Find a string is in a sorted array.
 */
static const char *const *
findstring(const char *s, const char *const *array, size_t nmemb)
{
	return bsearch(&s, array, nmemb, sizeof(const char *), pstrcmp);
}
/*
 * This file was generated by the mknodes program.
 */

/*	$NetBSD: nodes.c.pat,v 1.8 1997/04/11 23:03:09 christos Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)nodes.c.pat	8.2 (Berkeley) 5/4/95
 */

/*
 * Routine for dealing with parsed shell commands.
 */


static int     funcblocksize;		/* size of structures in function */
static int     funcstringsize;		/* size of strings in node */
pointer funcblock;		/* block to allocate function from */
static char   *funcstring;		/* block to allocate strings from */

static const short nodesize[26] = {
      ALIGN(sizeof (struct nbinary)),
      ALIGN(sizeof (struct ncmd)),
      ALIGN(sizeof (struct npipe)),
      ALIGN(sizeof (struct nredir)),
      ALIGN(sizeof (struct nredir)),
      ALIGN(sizeof (struct nredir)),
      ALIGN(sizeof (struct nbinary)),
      ALIGN(sizeof (struct nbinary)),
      ALIGN(sizeof (struct nif)),
      ALIGN(sizeof (struct nbinary)),
      ALIGN(sizeof (struct nbinary)),
      ALIGN(sizeof (struct nfor)),
      ALIGN(sizeof (struct ncase)),
      ALIGN(sizeof (struct nclist)),
      ALIGN(sizeof (struct narg)),
      ALIGN(sizeof (struct narg)),
      ALIGN(sizeof (struct nfile)),
      ALIGN(sizeof (struct nfile)),
      ALIGN(sizeof (struct nfile)),
      ALIGN(sizeof (struct nfile)),
      ALIGN(sizeof (struct nfile)),
      ALIGN(sizeof (struct ndup)),
      ALIGN(sizeof (struct ndup)),
      ALIGN(sizeof (struct nhere)),
      ALIGN(sizeof (struct nhere)),
      ALIGN(sizeof (struct nnot)),
};


static void calcsize __P((union node *));
static void sizenodelist __P((struct nodelist *));
static union node *copynode __P((union node *));
static struct nodelist *copynodelist __P((struct nodelist *));
static char *nodesavestr __P((char *));



/*
 * Make a copy of a parse tree.
 */

union node *
copyfunc(n)
	union node *n;
{
	if (n == NULL)
		return NULL;
	funcblocksize = 0;
	funcstringsize = 0;
	calcsize(n);
	funcblock = ckmalloc(funcblocksize + funcstringsize);
	funcstring = (char *) funcblock + funcblocksize;
	return copynode(n);
}



static void
calcsize(n)
	union node *n;
{
      if (n == NULL)
	    return;
      funcblocksize += nodesize[n->type];
      switch (n->type) {
      case NSEMI:
      case NAND:
      case NOR:
      case NWHILE:
      case NUNTIL:
	    calcsize(n->nbinary.ch2);
	    calcsize(n->nbinary.ch1);
	    break;
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
      case NFROM:
      case NFROMTO:
      case NAPPEND:
      case NTOOV:
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



static void
sizenodelist(lp)
	struct nodelist *lp;
{
	while (lp) {
		funcblocksize += ALIGN(sizeof(struct nodelist));
		calcsize(lp->n);
		lp = lp->next;
	}
}



static union node *
copynode(n)
	union node *n;
{
	union node *new;

      if (n == NULL)
	    return NULL;
      new = funcblock;
      funcblock = (char *) funcblock + nodesize[n->type];
      switch (n->type) {
      case NSEMI:
      case NAND:
      case NOR:
      case NWHILE:
      case NUNTIL:
	    new->nbinary.ch2 = copynode(n->nbinary.ch2);
	    new->nbinary.ch1 = copynode(n->nbinary.ch1);
	    break;
      case NCMD:
	    new->ncmd.redirect = copynode(n->ncmd.redirect);
	    new->ncmd.args = copynode(n->ncmd.args);
	    new->ncmd.assign = copynode(n->ncmd.assign);
	    new->ncmd.backgnd = n->ncmd.backgnd;
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
      case NIF:
	    new->nif.elsepart = copynode(n->nif.elsepart);
	    new->nif.ifpart = copynode(n->nif.ifpart);
	    new->nif.test = copynode(n->nif.test);
	    break;
      case NFOR:
	    new->nfor.var = nodesavestr(n->nfor.var);
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
	    new->narg.text = nodesavestr(n->narg.text);
	    new->narg.next = copynode(n->narg.next);
	    break;
      case NTO:
      case NFROM:
      case NFROMTO:
      case NAPPEND:
      case NTOOV:
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


static struct nodelist *
copynodelist(lp)
	struct nodelist *lp;
{
	struct nodelist *start;
	struct nodelist **lpp;

	lpp = &start;
	while (lp) {
		*lpp = funcblock;
		funcblock = (char *) funcblock + ALIGN(sizeof(struct nodelist));
		(*lpp)->n = copynode(lp->n);
		lp = lp->next;
		lpp = &(*lpp)->next;
	}
	*lpp = NULL;
	return start;
}



static char *
nodesavestr(s)
	char   *s;
{
#ifdef _GNU_SOURCE
	char   *rtn = funcstring;

	funcstring = stpcpy(funcstring, s) + 1;
	return rtn;
#else
	register char *p = s;
	register char *q = funcstring;
	char   *rtn = funcstring;

	while ((*q++ = *p++) != '\0')
		continue;
	funcstring = q;
	return rtn;
#endif
}



/*
 * Free a parse tree.
 */

static void
freefunc(n)
	union node *n;
{
	if (n)
		ckfree(n);
}
/*	$NetBSD: options.c,v 1.31 2001/02/26 13:06:43 wiz Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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


struct optent optlist[NOPTS] = {
	{ "errexit",	'e',	0 },
	{ "noglob",	'f',	0 },
	{ "ignoreeof",	'I',	0 },
	{ "interactive",'i',	0 },
	{ "monitor",	'm',	0 },
	{ "noexec",	'n',	0 },
	{ "stdin",	's',	0 },
	{ "xtrace",	'x',	0 },
	{ "verbose",	'v',	0 },
	{ "vi",		'V',	0 },
	{ "emacs",	'E',	0 },
	{ "noclobber",	'C',	0 },
	{ "allexport",	'a',	0 },
	{ "notify",	'b',	0 },
	{ "nounset",	'u',	0 },
	{ "quietprofile", 'q',	0 },
};
static char *arg0;			/* value of $0 */
struct shparam shellparam;	/* current positional parameters */
static char **argptr;			/* argument list for builtin commands */
static char *optionarg;		/* set by nextopt (like getopt) */
static char *optptr;			/* used by nextopt */

static char *minusc;			/* argument to -c option */


static void options __P((int));
static void minus_o __P((char *, int));
static void setoption __P((int, int));
#ifdef ASH_GETOPTS
static int getopts __P((char *, char *, char **, int *, int *));
#endif


/*
 * Process the shell command line arguments.
 */

static void
procargs(argc, argv)
	int argc;
	char **argv;
{
	int i;

	argptr = argv;
	if (argc > 0)
		argptr++;
	for (i = 0; i < NOPTS; i++)
		optlist[i].val = 2;
	options(1);
	if (*argptr == NULL && minusc == NULL)
		sflag = 1;
	if (iflag == 2 && sflag == 1 && isatty(0) && isatty(1))
		iflag = 1;
	if (mflag == 2)
		mflag = iflag;
	for (i = 0; i < NOPTS; i++)
		if (optlist[i].val == 2)
			optlist[i].val = 0;
	arg0 = argv[0];
	if (sflag == 0 && minusc == NULL) {
		commandname = argv[0];
		arg0 = *argptr++;
		setinputfile(arg0, 0);
		commandname = arg0;
	}
	/* POSIX 1003.2: first arg after -c cmd is $0, remainder $1... */
	if (argptr && minusc && *argptr)
	        arg0 = *argptr++;

	shellparam.p = argptr;
	shellparam.optind = 1;
	shellparam.optoff = -1;
	/* assert(shellparam.malloc == 0 && shellparam.nparam == 0); */
	while (*argptr) {
		shellparam.nparam++;
		argptr++;
	}
	optschanged();
}


static void
optschanged()
{
	setinteractive(iflag);
	setjobctl(mflag);
}

/*
 * Process shell options.  The global variable argptr contains a pointer
 * to the argument list; we advance it past the options.
 */

static void
options(cmdline)
	int cmdline;
{
	char *p;
	int val;
	int c;

	if (cmdline)
		minusc = NULL;
	while ((p = *argptr) != NULL) {
		argptr++;
		if ((c = *p++) == '-') {
			val = 1;
                        if (p[0] == '\0' || (p[0] == '-' && p[1] == '\0')) {
                                if (!cmdline) {
                                        /* "-" means turn off -x and -v */
                                        if (p[0] == '\0')
                                                xflag = vflag = 0;
                                        /* "--" means reset params */
                                        else if (*argptr == NULL)
						setparam(argptr);
                                }
				break;	  /* "-" or  "--" terminates options */
			}
		} else if (c == '+') {
			val = 0;
		} else {
			argptr--;
			break;
		}
		while ((c = *p++) != '\0') {
			if (c == 'c' && cmdline) {
				char *q;
#ifdef NOHACK	/* removing this code allows sh -ce 'foo' for compat */
				if (*p == '\0')
#endif
					q = *argptr++;
				if (q == NULL || minusc != NULL)
					error("Bad -c option");
				minusc = q;
#ifdef NOHACK
				break;
#endif
			} else if (c == 'o') {
				minus_o(*argptr, val);
				if (*argptr)
					argptr++;
			} else {
				setoption(c, val);
			}
		}
	}
}

static void
minus_o(name, val)
	char *name;
	int val;
{
	int i;

	if (name == NULL) {
		out1str("Current option settings\n");
		for (i = 0; i < NOPTS; i++)
			out1fmt("%-16s%s\n", optlist[i].name,
				optlist[i].val ? "on" : "off");
	} else {
		for (i = 0; i < NOPTS; i++)
			if (equal(name, optlist[i].name)) {
				setoption(optlist[i].letter, val);
				return;
			}
		error("Illegal option -o %s", name);
	}
}


static void
setoption(flag, val)
	char flag;
	int val;
	{
	int i;

	for (i = 0; i < NOPTS; i++)
		if (optlist[i].letter == flag) {
			optlist[i].val = val;
			if (val) {
				/* #%$ hack for ksh semantics */
				if (flag == 'V')
					Eflag = 0;
				else if (flag == 'E')
					Vflag = 0;
			}
			return;
		}
	error("Illegal option -%c", flag);
	/* NOTREACHED */
}



#ifdef mkinit
SHELLPROC {
	int i;

	for (i = 0; i < NOPTS; i++)
		optlist[i].val = 0;
	optschanged();

}
#endif


/*
 * Set the shell parameters.
 */

static void
setparam(argv)
	char **argv;
	{
	char **newparam;
	char **ap;
	int nparam;

	for (nparam = 0 ; argv[nparam] ; nparam++);
	ap = newparam = ckmalloc((nparam + 1) * sizeof *ap);
	while (*argv) {
		*ap++ = savestr(*argv++);
	}
	*ap = NULL;
	freeparam(&shellparam);
	shellparam.malloc = 1;
	shellparam.nparam = nparam;
	shellparam.p = newparam;
	shellparam.optind = 1;
	shellparam.optoff = -1;
}


/*
 * Free the list of positional parameters.
 */

static void
freeparam(param)
	volatile struct shparam *param;
	{
	char **ap;

	if (param->malloc) {
		for (ap = param->p ; *ap ; ap++)
			ckfree(*ap);
		ckfree(param->p);
	}
}



/*
 * The shift builtin command.
 */

static int
shiftcmd(argc, argv)
	int argc;
	char **argv;
{
	int n;
	char **ap1, **ap2;

	n = 1;
	if (argc > 1)
		n = number(argv[1]);
	if (n > shellparam.nparam)
		error("can't shift that many");
	INTOFF;
	shellparam.nparam -= n;
	for (ap1 = shellparam.p ; --n >= 0 ; ap1++) {
		if (shellparam.malloc)
			ckfree(*ap1);
	}
	ap2 = shellparam.p;
	while ((*ap2++ = *ap1++) != NULL);
	shellparam.optind = 1;
	shellparam.optoff = -1;
	INTON;
	return 0;
}



/*
 * The set command builtin.
 */

static int
setcmd(argc, argv)
	int argc;
	char **argv;
{
	if (argc == 1)
		return showvarscmd(argc, argv);
	INTOFF;
	options(0);
	optschanged();
	if (*argptr != NULL) {
		setparam(argptr);
	}
	INTON;
	return 0;
}


static void
getoptsreset(value)
	const char *value;
{
	shellparam.optind = number(value);
	shellparam.optoff = -1;
}

#ifdef ASH_GETOPTS
/*
 * The getopts builtin.  Shellparam.optnext points to the next argument
 * to be processed.  Shellparam.optptr points to the next character to
 * be processed in the current argument.  If shellparam.optnext is NULL,
 * then it's the first time getopts has been called.
 */

static int
getoptscmd(argc, argv)
	int argc;
	char **argv;
{
	char **optbase;

	if (argc < 3)
		error("Usage: getopts optstring var [arg]");
	else if (argc == 3) {
		optbase = shellparam.p;
		if (shellparam.optind > shellparam.nparam + 1) {
			shellparam.optind = 1;
			shellparam.optoff = -1;
		}
	}
	else {
		optbase = &argv[3];
		if (shellparam.optind > argc - 2) {
			shellparam.optind = 1;
			shellparam.optoff = -1;
		}
	}

	return getopts(argv[1], argv[2], optbase, &shellparam.optind,
		       &shellparam.optoff);
}

/*
 * Safe version of setvar, returns 1 on success 0 on failure.
 */

static int
setvarsafe(name, val, flags)
	const char *name, *val;
	int flags;
{
	struct jmploc jmploc;
	struct jmploc *volatile savehandler = handler;
	int err = 0;
#ifdef __GNUC__
	(void) &err;
#endif

	if (setjmp(jmploc.loc))
		err = 1;
	else {
		handler = &jmploc;
		setvar(name, val, flags);
	}
	handler = savehandler;
	return err;
}

static int
getopts(optstr, optvar, optfirst, myoptind, optoff)
	char *optstr;
	char *optvar;
	char **optfirst;
	int *myoptind;
	int *optoff;
{
	char *p, *q;
	char c = '?';
	int done = 0;
	int err = 0;
	char s[10];
	char **optnext = optfirst + *myoptind - 1;

	if (*myoptind <= 1 || *optoff < 0 || !(*(optnext - 1)) ||
	    strlen(*(optnext - 1)) < *optoff)
		p = NULL;
	else
		p = *(optnext - 1) + *optoff;
	if (p == NULL || *p == '\0') {
		/* Current word is done, advance */
		if (optnext == NULL)
			return 1;
		p = *optnext;
		if (p == NULL || *p != '-' || *++p == '\0') {
atend:
			*myoptind = optnext - optfirst + 1;
			p = NULL;
			done = 1;
			goto out;
		}
		optnext++;
		if (p[0] == '-' && p[1] == '\0')	/* check for "--" */
			goto atend;
	}

	c = *p++;
	for (q = optstr; *q != c; ) {
		if (*q == '\0') {
			if (optstr[0] == ':') {
				s[0] = c;
				s[1] = '\0';
				err |= setvarsafe("OPTARG", s, 0);
			}
			else {
				outfmt(&errout, "Illegal option -%c\n", c);
				(void) unsetvar("OPTARG");
			}
			c = '?';
			goto bad;
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
			}
			else {
				outfmt(&errout, "No arg for -%c option\n", c);
				(void) unsetvar("OPTARG");
				c = '?';
			}
			goto bad;
		}

		if (p == *optnext)
			optnext++;
		setvarsafe("OPTARG", p, 0);
		p = NULL;
	}
	else
		setvarsafe("OPTARG", "", 0);
	*myoptind = optnext - optfirst + 1;
	goto out;

bad:
	*myoptind = 1;
	p = NULL;
out:
	*optoff = p ? p - *(optnext - 1) : -1;
	fmtstr(s, sizeof(s), "%d", *myoptind);
	err |= setvarsafe("OPTIND", s, VNOFUNC);
	s[0] = c;
	s[1] = '\0';
	err |= setvarsafe(optvar, s, 0);
	if (err) {
		*myoptind = 1;
		*optoff = -1;
		flushall();
		exraise(EXERROR);
	}
	return done;
}
#endif	

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
nextopt(optstring)
	const char *optstring;
	{
	char *p;
	const char *q;
	char c;

	if ((p = optptr) == NULL || *p == '\0') {
		p = *argptr;
		if (p == NULL || *p != '-' || *++p == '\0')
			return '\0';
		argptr++;
		if (p[0] == '-' && p[1] == '\0')	/* check for "--" */
			return '\0';
	}
	c = *p++;
	for (q = optstring ; *q != c ; ) {
		if (*q == '\0')
			error("Illegal option -%c", c);
		if (*++q == ':')
			q++;
	}
	if (*++q == ':') {
		if (*p == '\0' && (p = *argptr++) == NULL)
			error("No arg for -%c option", c);
		optionarg = p;
		p = NULL;
	}
	optptr = p;
	return c;
}


/*	$NetBSD: output.c,v 1.23 2001/01/07 23:39:07 lukem Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

/*
 * Shell output routines.  We use our own output routines because:
 *	When a builtin command is interrupted we have to discard
 *		any pending output.
 *	When a builtin command appears in back quotes, we want to
 *		save the output of the command in a region obtained
 *		via malloc, rather than doing a fork and reading the
 *		output of the command via a pipe.
 *	Our output routines may be smaller than the stdio routines.
 */


#define OUTBUFSIZ BUFSIZ
#define MEM_OUT -3		/* output to dynamically allocated memory */


#ifdef USE_GLIBC_STDIO
struct output output = {NULL, NULL, 0, NULL, 0, 1, 0};
struct output errout = {NULL, NULL, 0, NULL, 0, 2, 0};
struct output memout = {NULL, NULL, 0, NULL, 0, MEM_OUT, 0};
#else
struct output output = {NULL, 0, NULL, OUTBUFSIZ, 1, 0};
struct output errout = {NULL, 0, NULL, 0, 2, 0};
struct output memout = {NULL, 0, NULL, 0, MEM_OUT, 0};
#endif
struct output *out1 = &output;
struct output *out2 = &errout;


#ifndef USE_GLIBC_STDIO
static void __outstr __P((const char *, size_t, struct output*));
#endif


#ifdef mkinit

INCLUDE "output.h"
INCLUDE "memalloc.h"

INIT {
#ifdef USE_GLIBC_STDIO
	initstreams();
#endif
}

RESET {
	out1 = &output;
	out2 = &errout;
#ifdef USE_GLIBC_STDIO
	if (memout.stream != NULL)
		__closememout();
#endif
	if (memout.buf != NULL) {
		ckfree(memout.buf);
		memout.buf = NULL;
	}
}

#endif


#ifndef USE_GLIBC_STDIO
static void
__outstr(const char *p, size_t len, struct output *dest) {
	if (!dest->bufsize) {
		dest->nleft = 0;
	} else if (dest->buf == NULL) {
		if (len > dest->bufsize && dest->fd == MEM_OUT) {
			dest->bufsize = len;
		}
		INTOFF;
		dest->buf = ckmalloc(dest->bufsize);
		dest->nextc = dest->buf;
		dest->nleft = dest->bufsize;
		INTON;
	} else if (dest->fd == MEM_OUT) {
		int offset;

		offset = dest->bufsize;
		INTOFF;
		if (dest->bufsize >= len) {
			dest->bufsize <<= 1;
		} else {
			dest->bufsize += len;
		}
		dest->buf = ckrealloc(dest->buf, dest->bufsize);
		dest->nleft = dest->bufsize - offset;
		dest->nextc = dest->buf + offset;
		INTON;
	} else {
		flushout(dest);
	}

	if (len < dest->nleft) {
		dest->nleft -= len;
		memcpy(dest->nextc, p, len);
		dest->nextc += len;
		return;
	}

	if (xwrite(dest->fd, p, len) < len)
		dest->flags |= OUTPUT_ERR;
}
#endif


static void
outstr(p, file)
	const char *p;
	struct output *file;
	{
#ifdef USE_GLIBC_STDIO
	INTOFF;
	fputs(p, file->stream);
	INTON;
#else
	size_t len;

	if (!*p) {
		return;
	}
	len = strlen(p);
	if ((file->nleft -= len) > 0) {
		memcpy(file->nextc, p, len);
		file->nextc += len;
		return;
	}
	__outstr(p, len, file);
#endif
}


#ifndef USE_GLIBC_STDIO


static void
outcslow(c, dest)
	char c;
	struct output *dest;
	{
	__outstr(&c, 1, dest);
}
#endif


static void
flushall() {
	flushout(&output);
#ifdef FLUSHERR
	flushout(&errout);
#endif
}


static void
flushout(dest)
	struct output *dest;
	{
#ifdef USE_GLIBC_STDIO
	INTOFF;
	fflush(dest->stream);
	INTON;
#else
	size_t len;

	len = dest->nextc - dest->buf;
	if (dest->buf == NULL || !len || dest->fd < 0)
		return;
	dest->nextc = dest->buf;
	dest->nleft = dest->bufsize;
	if (xwrite(dest->fd, dest->buf, len) < len)
		dest->flags |= OUTPUT_ERR;
#endif
}


static void
freestdout() {
	if (output.buf) {
		INTOFF;
		ckfree(output.buf);
		output.buf = NULL;
		output.nleft = 0;
		INTON;
	}
	output.flags = 0;
}


static void
#ifdef __STDC__
outfmt(struct output *file, const char *fmt, ...)
#else
static void
outfmt(va_alist)
	va_dcl
#endif
{
	va_list ap;
#ifndef __STDC__
	struct output *file;
	const char *fmt;

	va_start(ap);
	file = va_arg(ap, struct output *);
	fmt = va_arg(ap, const char *);
#else
	va_start(ap, fmt);
#endif
	doformat(file, fmt, ap);
	va_end(ap);
}


static void
#ifdef __STDC__
out1fmt(const char *fmt, ...)
#else
out1fmt(va_alist)
	va_dcl
#endif
{
	va_list ap;
#ifndef __STDC__
	const char *fmt;

	va_start(ap);
	fmt = va_arg(ap, const char *);
#else
	va_start(ap, fmt);
#endif
	doformat(out1, fmt, ap);
	va_end(ap);
}

static void
#ifdef __STDC__
fmtstr(char *outbuf, size_t length, const char *fmt, ...)
#else
fmtstr(va_alist)
	va_dcl
#endif
{
	va_list ap;
#ifndef __STDC__
	char *outbuf;
	size_t length;
	const char *fmt;

	va_start(ap);
	outbuf = va_arg(ap, char *);
	length = va_arg(ap, size_t);
	fmt = va_arg(ap, const char *);
#else
	va_start(ap, fmt);
#endif
	INTOFF;
	vsnprintf(outbuf, length, fmt, ap);
	INTON;
}

#ifndef USE_GLIBC_STDIO
/*
 * Formatted output.  This routine handles a subset of the printf formats:
 * - Formats supported: d, u, o, p, X, s, and c.
 * - The x format is also accepted but is treated like X.
 * - The l, ll and q modifiers are accepted.
 * - The - and # flags are accepted; # only works with the o format.
 * - Width and precision may be specified with any format except c.
 * - An * may be given for the width or precision.
 * - The obsolete practice of preceding the width with a zero to get
 *   zero padding is not supported; use the precision field.
 * - A % may be printed by writing %% in the format string.
 */

#define TEMPSIZE 24

#ifdef BSD4_4
#define HAVE_VASPRINTF 1
#endif

#if	!HAVE_VASPRINTF
static const char digit[] = "0123456789ABCDEF";
#endif


static void
doformat(dest, f, ap)
	struct output *dest;
	const char *f;		/* format string */
	va_list ap;
{
#if	HAVE_VASPRINTF
	char *s, *t;
	int len;

	INTOFF;
	len = vasprintf(&t, f, ap);
	if (len < 0) {
		return;
	}
	s = stalloc(++len);
	memcpy(s, t, len);
	free(t);
	INTON;
	outstr(s, dest);
	stunalloc(s);
#else	/* !HAVE_VASPRINTF */
	char c;
	char temp[TEMPSIZE];
	int flushleft;
	int sharp;
	int width;
	int prec;
	int islong;
	int isquad;
	char *p;
	int sign;
#ifdef BSD4_4
	quad_t l;
	u_quad_t num;
#else
	long l;
	u_long num;
#endif
	unsigned base;
	int len;
	int size;
	int pad;

	while ((c = *f++) != '\0') {
		if (c != '%') {
			outc(c, dest);
			continue;
		}
		flushleft = 0;
		sharp = 0;
		width = 0;
		prec = -1;
		islong = 0;
		isquad = 0;
		for (;;) {
			if (*f == '-')
				flushleft++;
			else if (*f == '#')
				sharp++;
			else
				break;
			f++;
		}
		if (*f == '*') {
			width = va_arg(ap, int);
			f++;
		} else {
			while (is_digit(*f)) {
				width = 10 * width + digit_val(*f++);
			}
		}
		if (*f == '.') {
			if (*++f == '*') {
				prec = va_arg(ap, int);
				f++;
			} else {
				prec = 0;
				while (is_digit(*f)) {
					prec = 10 * prec + digit_val(*f++);
				}
			}
		}
		if (*f == 'l') {
			f++;
			if (*f == 'l') {
				isquad++;
				f++;
			} else
				islong++;
		} else if (*f == 'q') {
			isquad++;
			f++;
		}
		switch (*f) {
		case 'd':
#ifdef BSD4_4
			if (isquad)
				l = va_arg(ap, quad_t);
			else
#endif
			if (islong)
				l = va_arg(ap, long);
			else
				l = va_arg(ap, int);
			sign = 0;
			num = l;
			if (l < 0) {
				num = -l;
				sign = 1;
			}
			base = 10;
			goto number;
		case 'u':
			base = 10;
			goto uns_number;
		case 'o':
			base = 8;
			goto uns_number;
		case 'p':
			outc('0', dest);
			outc('x', dest);
			/*FALLTHROUGH*/
		case 'x':
			/* we don't implement 'x'; treat like 'X' */
		case 'X':
			base = 16;
uns_number:	  /* an unsigned number */
			sign = 0;
#ifdef BSD4_4
			if (isquad)
				num = va_arg(ap, u_quad_t);
			else
#endif
			if (islong)
				num = va_arg(ap, unsigned long);
			else
				num = va_arg(ap, unsigned int);
number:		  /* process a number */
			p = temp + TEMPSIZE - 1;
			*p = '\0';
			while (num) {
				*--p = digit[num % base];
				num /= base;
			}
			len = (temp + TEMPSIZE - 1) - p;
			if (prec < 0)
				prec = 1;
			if (sharp && *f == 'o' && prec <= len)
				prec = len + 1;
			pad = 0;
			if (width) {
				size = len;
				if (size < prec)
					size = prec;
				size += sign;
				pad = width - size;
				if (flushleft == 0) {
					while (--pad >= 0)
						outc(' ', dest);
				}
			}
			if (sign)
				outc('-', dest);
			prec -= len;
			while (--prec >= 0)
				outc('0', dest);
			while (*p)
				outc(*p++, dest);
			while (--pad >= 0)
				outc(' ', dest);
			break;
		case 's':
			p = va_arg(ap, char *);
			pad = 0;
			if (width) {
				len = strlen(p);
				if (prec >= 0 && len > prec)
					len = prec;
				pad = width - len;
				if (flushleft == 0) {
					while (--pad >= 0)
						outc(' ', dest);
				}
			}
			prec++;
			while (--prec != 0 && *p)
				outc(*p++, dest);
			while (--pad >= 0)
				outc(' ', dest);
			break;
		case 'c':
			c = va_arg(ap, int);
			outc(c, dest);
			break;
		default:
			outc(*f, dest);
			break;
		}
		f++;
	}
#endif	/* !HAVE_VASPRINTF */
}
#endif



/*
 * Version of write which resumes after a signal is caught.
 */

static int
xwrite(fd, buf, nbytes)
	int fd;
	const char *buf;
	int nbytes;
	{
	int ntry;
	int i;
	int n;

	n = nbytes;
	ntry = 0;
	for (;;) {
		i = write(fd, buf, n);
		if (i > 0) {
			if ((n -= i) <= 0)
				return nbytes;
			buf += i;
			ntry = 0;
		} else if (i == 0) {
			if (++ntry > 10)
				return nbytes - n;
		} else if (errno != EINTR) {
			return -1;
		}
	}
}


#ifdef notdef
/*
 * Version of ioctl that retries after a signal is caught.
 * XXX unused function
 */

static int
xioctl(fd, request, arg)
	int fd;
	unsigned long request;
	char * arg;
{
	int i;

	while ((i = ioctl(fd, request, arg)) == -1 && errno == EINTR);
	return i;
}
#endif


#ifdef USE_GLIBC_STDIO
static void initstreams() {
	output.stream = stdout;
	errout.stream = stderr;
}


static void
openmemout() {
	INTOFF;
	memout.stream = open_memstream(&memout.buf, &memout.bufsize);
	INTON;
}


static int
__closememout() {
	int error;
	error = fclose(memout.stream);
	memout.stream = NULL;
	return error;
}
#endif
/*	$NetBSD: parser.c,v 1.46 2001/02/04 19:52:06 christos Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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


/*
 * Shell command parser.
 */

#define EOFMARKLEN 79



struct heredoc {
	struct heredoc *next;	/* next here document in list */
	union node *here;		/* redirection node */
	char *eofmark;		/* string indicating end of input */
	int striptabs;		/* if set, strip leading tabs */
};



struct heredoc *heredoclist;	/* list of here documents to read */
static int parsebackquote;		/* nonzero if we are inside backquotes */
static int doprompt;			/* if set, prompt the user */
static int needprompt;			/* true if interactive and at start of line */
static int lasttoken;			/* last token read */
static int tokpushback;		/* last token pushed back */
static char *wordtext;			/* text of last word returned by readtoken */
static int checkkwd;            /* 1 == check for kwds, 2 == also eat newlines */
/* 1 == check for aliases, 2 == also check for assignments */
static int checkalias;
struct nodelist *backquotelist;
union node *redirnode;
struct heredoc *heredoc;
static int quoteflag;			/* set if (part of) last token was quoted */
static int startlinno;			/* line # where last token started */


static union node *list __P((int));
static union node *andor __P((void));
static union node *pipeline __P((void));
static union node *command __P((void));
static union node *simplecmd __P((void));
static union node *makename __P((void));
static void parsefname __P((void));
static void parseheredoc __P((void));
static int peektoken __P((void));
static int readtoken __P((void));
static int xxreadtoken __P((void));
static int readtoken1 __P((int, char const *, char *, int));
static int noexpand __P((char *));
static void synexpect __P((int)) __attribute__((noreturn));
static void synerror __P((const char *)) __attribute__((noreturn));
static void setprompt __P((int));


/*
 * Read and parse a command.  Returns NEOF on end of file.  (NULL is a
 * valid parse tree indicating a blank line.)
 */

union node *
parsecmd(int interact)
{
	int t;

	tokpushback = 0;
	doprompt = interact;
	if (doprompt)
		setprompt(1);
	else
		setprompt(0);
	needprompt = 0;
	t = readtoken();
	if (t == TEOF)
		return NEOF;
	if (t == TNL)
		return NULL;
	tokpushback++;
	return list(1);
}


static union node *
list(nlflag)
	int nlflag;
{
	union node *n1, *n2, *n3;
	int tok;

	checkkwd = 2;
	if (nlflag == 0 && tokendlist[peektoken()])
		return NULL;
	n1 = NULL;
	for (;;) {
		n2 = andor();
		tok = readtoken();
		if (tok == TBACKGND) {
			if (n2->type == NCMD || n2->type == NPIPE) {
				n2->ncmd.backgnd = 1;
			} else if (n2->type == NREDIR) {
				n2->type = NBACKGND;
			} else {
				n3 = (union node *)stalloc(sizeof (struct nredir));
				n3->type = NBACKGND;
				n3->nredir.n = n2;
				n3->nredir.redirect = NULL;
				n2 = n3;
			}
		}
		if (n1 == NULL) {
			n1 = n2;
		}
		else {
			n3 = (union node *)stalloc(sizeof (struct nbinary));
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
				if (nlflag)
					return n1;
			} else {
				tokpushback++;
			}
			checkkwd = 2;
			if (tokendlist[peektoken()])
				return n1;
			break;
		case TEOF:
			if (heredoclist)
				parseheredoc();
			else
				pungetc();		/* push back EOF on input */
			return n1;
		default:
			if (nlflag)
				synexpect(-1);
			tokpushback++;
			return n1;
		}
	}
}



static union node *
andor() {
	union node *n1, *n2, *n3;
	int t;

	checkkwd = 1;
	n1 = pipeline();
	for (;;) {
		if ((t = readtoken()) == TAND) {
			t = NAND;
		} else if (t == TOR) {
			t = NOR;
		} else {
			tokpushback++;
			return n1;
		}
		checkkwd = 2;
		n2 = pipeline();
		n3 = (union node *)stalloc(sizeof (struct nbinary));
		n3->type = t;
		n3->nbinary.ch1 = n1;
		n3->nbinary.ch2 = n2;
		n1 = n3;
	}
}



static union node *
pipeline() {
	union node *n1, *n2, *pipenode;
	struct nodelist *lp, *prev;
	int negate;

	negate = 0;
	TRACE(("pipeline: entered\n"));
	if (readtoken() == TNOT) {
		negate = !negate;
		checkkwd = 1;
	} else
		tokpushback++;
	n1 = command();
	if (readtoken() == TPIPE) {
		pipenode = (union node *)stalloc(sizeof (struct npipe));
		pipenode->type = NPIPE;
		pipenode->npipe.backgnd = 0;
		lp = (struct nodelist *)stalloc(sizeof (struct nodelist));
		pipenode->npipe.cmdlist = lp;
		lp->n = n1;
		do {
			prev = lp;
			lp = (struct nodelist *)stalloc(sizeof (struct nodelist));
			checkkwd = 2;
			lp->n = command();
			prev->next = lp;
		} while (readtoken() == TPIPE);
		lp->next = NULL;
		n1 = pipenode;
	}
	tokpushback++;
	if (negate) {
		n2 = (union node *)stalloc(sizeof (struct nnot));
		n2->type = NNOT;
		n2->nnot.com = n1;
		return n2;
	} else
		return n1;
}



static union node *
command() {
	union node *n1, *n2;
	union node *ap, **app;
	union node *cp, **cpp;
	union node *redir, **rpp;
	int t;

	redir = NULL;
	n1 = NULL;
	rpp = &redir;

	switch (readtoken()) {
	case TIF:
		n1 = (union node *)stalloc(sizeof (struct nif));
		n1->type = NIF;
		n1->nif.test = list(0);
		if (readtoken() != TTHEN)
			synexpect(TTHEN);
		n1->nif.ifpart = list(0);
		n2 = n1;
		while (readtoken() == TELIF) {
			n2->nif.elsepart = (union node *)stalloc(sizeof (struct nif));
			n2 = n2->nif.elsepart;
			n2->type = NIF;
			n2->nif.test = list(0);
			if (readtoken() != TTHEN)
				synexpect(TTHEN);
			n2->nif.ifpart = list(0);
		}
		if (lasttoken == TELSE)
			n2->nif.elsepart = list(0);
		else {
			n2->nif.elsepart = NULL;
			tokpushback++;
		}
		if (readtoken() != TFI)
			synexpect(TFI);
		checkkwd = 1;
		break;
	case TWHILE:
	case TUNTIL: {
		int got;
		n1 = (union node *)stalloc(sizeof (struct nbinary));
		n1->type = (lasttoken == TWHILE)? NWHILE : NUNTIL;
		n1->nbinary.ch1 = list(0);
		if ((got=readtoken()) != TDO) {
TRACE(("expecting DO got %s %s\n", tokname[got], got == TWORD ? wordtext : ""));
			synexpect(TDO);
		}
		n1->nbinary.ch2 = list(0);
		if (readtoken() != TDONE)
			synexpect(TDONE);
		checkkwd = 1;
		break;
	}
	case TFOR:
		if (readtoken() != TWORD || quoteflag || ! goodname(wordtext))
			synerror("Bad for loop variable");
		n1 = (union node *)stalloc(sizeof (struct nfor));
		n1->type = NFOR;
		n1->nfor.var = wordtext;
		checkkwd = 1;
		if (readtoken() == TIN) {
			app = &ap;
			while (readtoken() == TWORD) {
				n2 = (union node *)stalloc(sizeof (struct narg));
				n2->type = NARG;
				n2->narg.text = wordtext;
				n2->narg.backquote = backquotelist;
				*app = n2;
				app = &n2->narg.next;
			}
			*app = NULL;
			n1->nfor.args = ap;
			if (lasttoken != TNL && lasttoken != TSEMI)
				synexpect(-1);
		} else {
			static char argvars[5] = {CTLVAR, VSNORMAL|VSQUOTE,
								   '@', '=', '\0'};
			n2 = (union node *)stalloc(sizeof (struct narg));
			n2->type = NARG;
			n2->narg.text = argvars;
			n2->narg.backquote = NULL;
			n2->narg.next = NULL;
			n1->nfor.args = n2;
			/*
			 * Newline or semicolon here is optional (but note
			 * that the original Bourne shell only allowed NL).
			 */
			if (lasttoken != TNL && lasttoken != TSEMI)
				tokpushback++;
		}
		checkkwd = 2;
		if (readtoken() != TDO)
			synexpect(TDO);
		n1->nfor.body = list(0);
		if (readtoken() != TDONE)
			synexpect(TDONE);
		checkkwd = 1;
		break;
	case TCASE:
		n1 = (union node *)stalloc(sizeof (struct ncase));
		n1->type = NCASE;
		if (readtoken() != TWORD)
			synexpect(TWORD);
		n1->ncase.expr = n2 = (union node *)stalloc(sizeof (struct narg));
		n2->type = NARG;
		n2->narg.text = wordtext;
		n2->narg.backquote = backquotelist;
		n2->narg.next = NULL;
		do {
			checkkwd = 1;
		} while (readtoken() == TNL);
		if (lasttoken != TIN)
			synerror("expecting \"in\"");
		cpp = &n1->ncase.cases;
		checkkwd = 2, readtoken();
		do {
			if (lasttoken == TLP)
				readtoken();
			*cpp = cp = (union node *)stalloc(sizeof (struct nclist));
			cp->type = NCLIST;
			app = &cp->nclist.pattern;
			for (;;) {
				*app = ap = (union node *)stalloc(sizeof (struct narg));
				ap->type = NARG;
				ap->narg.text = wordtext;
				ap->narg.backquote = backquotelist;
				if (checkkwd = 2, readtoken() != TPIPE)
					break;
				app = &ap->narg.next;
				readtoken();
			}
			ap->narg.next = NULL;
			if (lasttoken != TRP)
				synexpect(TRP);
			cp->nclist.body = list(0);

			checkkwd = 2;
			if ((t = readtoken()) != TESAC) {
				if (t != TENDCASE)
					synexpect(TENDCASE);
				else
					checkkwd = 2, readtoken();
			}
			cpp = &cp->nclist.next;
		} while(lasttoken != TESAC);
		*cpp = NULL;
		checkkwd = 1;
		break;
	case TLP:
		n1 = (union node *)stalloc(sizeof (struct nredir));
		n1->type = NSUBSHELL;
		n1->nredir.n = list(0);
		n1->nredir.redirect = NULL;
		if (readtoken() != TRP)
			synexpect(TRP);
		checkkwd = 1;
		break;
	case TBEGIN:
		n1 = list(0);
		if (readtoken() != TEND)
			synexpect(TEND);
		checkkwd = 1;
		break;
	/* Handle an empty command like other simple commands.  */
	case TSEMI:
	case TAND:
	case TOR:
	case TNL:
	case TEOF:
	case TRP:
	case TBACKGND:
		/*
		 * An empty command before a ; doesn't make much sense, and
		 * should certainly be disallowed in the case of `if ;'.
		 */
		if (!redir)
			synexpect(-1);
	case TWORD:
	case TREDIR:
		tokpushback++;
		n1 = simplecmd();
		return n1;
	default:
		synexpect(-1);
		/* NOTREACHED */
	}

	/* Now check for redirection which may follow command */
	while (readtoken() == TREDIR) {
		*rpp = n2 = redirnode;
		rpp = &n2->nfile.next;
		parsefname();
	}
	tokpushback++;
	*rpp = NULL;
	if (redir) {
		if (n1->type != NSUBSHELL) {
			n2 = (union node *)stalloc(sizeof (struct nredir));
			n2->type = NREDIR;
			n2->nredir.n = n1;
			n1 = n2;
		}
		n1->nredir.redirect = redir;
	}

	return n1;
}


static union node *
simplecmd() {
	union node *args, **app;
	union node *n = NULL;
	union node *vars, **vpp;
	union node **rpp, *redir;

	args = NULL;
	app = &args;
	vars = NULL;
	vpp = &vars;
	redir = NULL;
	rpp = &redir;

	checkalias = 2;
	for (;;) {
		switch (readtoken()) {
		case TWORD:
		case TASSIGN:
			n = (union node *)stalloc(sizeof (struct narg));
			n->type = NARG;
			n->narg.text = wordtext;
			n->narg.backquote = backquotelist;
			if (lasttoken == TWORD) {
				*app = n;
				app = &n->narg.next;
			} else {
				*vpp = n;
				vpp = &n->narg.next;
			}
			break;
		case TREDIR:
			*rpp = n = redirnode;
			rpp = &n->nfile.next;
			parsefname();	/* read name of redirection file */
			break;
		case TLP:
			if (
				args && app == &args->narg.next &&
				!vars && !redir
			) {
				/* We have a function */
				if (readtoken() != TRP)
					synexpect(TRP);
#ifdef notdef
				if (! goodname(n->narg.text))
					synerror("Bad function name");
#endif
				n->type = NDEFUN;
				checkkwd = 2;
				n->narg.next = command();
				return n;
			}
			/* fall through */
		default:
			tokpushback++;
			goto out;
		}
	}
out:
	*app = NULL;
	*vpp = NULL;
	*rpp = NULL;
	n = (union node *)stalloc(sizeof (struct ncmd));
	n->type = NCMD;
	n->ncmd.backgnd = 0;
	n->ncmd.args = args;
	n->ncmd.assign = vars;
	n->ncmd.redirect = redir;
	return n;
}

static union node *
makename() {
	union node *n;

	n = (union node *)stalloc(sizeof (struct narg));
	n->type = NARG;
	n->narg.next = NULL;
	n->narg.text = wordtext;
	n->narg.backquote = backquotelist;
	return n;
}

static void fixredir(union node *n, const char *text, int err)
	{
	TRACE(("Fix redir %s %d\n", text, err));
	if (!err)
		n->ndup.vname = NULL;

	if (is_digit(text[0]) && text[1] == '\0')
		n->ndup.dupfd = digit_val(text[0]);
	else if (text[0] == '-' && text[1] == '\0')
		n->ndup.dupfd = -1;
	else {

		if (err)
			synerror("Bad fd number");
		else
			n->ndup.vname = makename();
	}
}


static void
parsefname() {
	union node *n = redirnode;

	if (readtoken() != TWORD)
		synexpect(-1);
	if (n->type == NHERE) {
		struct heredoc *here = heredoc;
		struct heredoc *p;
		int i;

		if (quoteflag == 0)
			n->type = NXHERE;
		TRACE(("Here document %d\n", n->type));
		if (here->striptabs) {
			while (*wordtext == '\t')
				wordtext++;
		}
		if (! noexpand(wordtext) || (i = strlen(wordtext)) == 0 || i > EOFMARKLEN)
			synerror("Illegal eof marker for << redirection");
		rmescapes(wordtext);
		here->eofmark = wordtext;
		here->next = NULL;
		if (heredoclist == NULL)
			heredoclist = here;
		else {
			for (p = heredoclist ; p->next ; p = p->next);
			p->next = here;
		}
	} else if (n->type == NTOFD || n->type == NFROMFD) {
		fixredir(n, wordtext, 0);
	} else {
		n->nfile.fname = makename();
	}
}


/*
 * Input any here documents.
 */

static void
parseheredoc() {
	struct heredoc *here;
	union node *n;

	while (heredoclist) {
		here = heredoclist;
		heredoclist = here->next;
		if (needprompt) {
			setprompt(2);
			needprompt = 0;
		}
		readtoken1(pgetc(), here->here->type == NHERE? SQSYNTAX : DQSYNTAX,
				here->eofmark, here->striptabs);
		n = (union node *)stalloc(sizeof (struct narg));
		n->narg.type = NARG;
		n->narg.next = NULL;
		n->narg.text = wordtext;
		n->narg.backquote = backquotelist;
		here->here->nhere.doc = n;
	}
}

static int
peektoken() {
	int t;

	t = readtoken();
	tokpushback++;
	return (t);
}

static int
readtoken() {
	int t;
	int savecheckkwd = checkkwd;
	int savecheckalias = checkalias;
	struct alias *ap;
#ifdef DEBUG
	int alreadyseen = tokpushback;
#endif

top:
	t = xxreadtoken();
	checkalias = savecheckalias;

	if (checkkwd) {
		/*
		 * eat newlines
		 */
		if (checkkwd == 2) {
			checkkwd = 0;
			while (t == TNL) {
				parseheredoc();
				t = xxreadtoken();
			}
		}
		checkkwd = 0;
		/*
		 * check for keywords
		 */
		if (t == TWORD && !quoteflag)
		{
			const char *const *pp;

			if ((pp = findkwd(wordtext))) {
				lasttoken = t = pp - parsekwd + KWDOFFSET;
				TRACE(("keyword %s recognized\n", tokname[t]));
				goto out;
			}
		}
	}

	if (t != TWORD) {
		if (t != TREDIR) {
			checkalias = 0;
		}
	} else if (checkalias == 2 && isassignment(wordtext)) {
		lasttoken = t = TASSIGN;
	} else if (checkalias) {
		if (!quoteflag && (ap = lookupalias(wordtext, 1)) != NULL) {
			if (*ap->val) {
				pushstring(ap->val, strlen(ap->val), ap);
			}
			checkkwd = savecheckkwd;
			goto top;
		}
		checkalias = 0;
	}
out:
#ifdef DEBUG
	if (!alreadyseen)
	    TRACE(("token %s %s\n", tokname[t], t == TWORD || t == TASSIGN ? wordtext : ""));
	else
	    TRACE(("reread token %s %s\n", tokname[t], t == TWORD || t == TASSIGN ? wordtext : ""));
#endif
	return (t);
}


/*
 * Read the next input token.
 * If the token is a word, we set backquotelist to the list of cmds in
 *	backquotes.  We set quoteflag to true if any part of the word was
 *	quoted.
 * If the token is TREDIR, then we set redirnode to a structure containing
 *	the redirection.
 * In all cases, the variable startlinno is set to the number of the line
 *	on which the token starts.
 *
 * [Change comment:  here documents and internal procedures]
 * [Readtoken shouldn't have any arguments.  Perhaps we should make the
 *  word parsing code into a separate routine.  In this case, readtoken
 *  doesn't need to have any internal procedures, but parseword does.
 *  We could also make parseoperator in essence the main routine, and
 *  have parseword (readtoken1?) handle both words and redirection.]
 */

#define RETURN(token)	return lasttoken = token

static int
xxreadtoken() {
	int c;

	if (tokpushback) {
		tokpushback = 0;
		return lasttoken;
	}
	if (needprompt) {
		setprompt(2);
		needprompt = 0;
	}
	startlinno = plinno;
	for (;;) {	/* until token or start of word found */
		c = pgetc_macro();
		switch (c) {
		case ' ': case '\t':
		case PEOA:
			continue;
		case '#':
			while ((c = pgetc()) != '\n' && c != PEOF);
			pungetc();
			continue;
		case '\\':
			if (pgetc() == '\n') {
				startlinno = ++plinno;
				if (doprompt)
					setprompt(2);
				else
					setprompt(0);
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

#define CHECKEND()	{goto checkend; checkend_return:;}
#define PARSEREDIR()	{goto parseredir; parseredir_return:;}
#define PARSESUB()	{goto parsesub; parsesub_return:;}
#define PARSEBACKQOLD()	{oldstyle = 1; goto parsebackq; parsebackq_oldreturn:;}
#define PARSEBACKQNEW()	{oldstyle = 0; goto parsebackq; parsebackq_newreturn:;}
#define	PARSEARITH()	{goto parsearith; parsearith_return:;}

static int
readtoken1(firstc, syntax, eofmark, striptabs)
	int firstc;
	char const *syntax;
	char *eofmark;
	int striptabs;
	{
	int c = firstc;
	char *out;
	int len;
	char line[EOFMARKLEN + 1];
	struct nodelist *bqlist;
	int quotef;
	int dblquote;
	int varnest;	/* levels of variables expansion */
	int arinest;	/* levels of arithmetic expansion */
	int parenlevel;	/* levels of parens in arithmetic */
	int dqvarnest;	/* levels of variables expansion within double quotes */
	int oldstyle;
	char const *prevsyntax;	/* syntax before arithmetic */
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
	dblquote = 0;
	if (syntax == DQSYNTAX)
		dblquote = 1;
	quotef = 0;
	bqlist = NULL;
	varnest = 0;
	arinest = 0;
	parenlevel = 0;
	dqvarnest = 0;

	STARTSTACKSTR(out);
	loop: {	/* for each line, until end of word */
#if ATTY
		if (c == '\034' && doprompt
		 && attyset() && ! equal(termval(), "emacs")) {
			attyline();
			if (syntax == BASESYNTAX)
				return readtoken();
			c = pgetc();
			goto loop;
		}
#endif
		CHECKEND();	/* set c to PEOF if at end of here document */
		for (;;) {	/* until end of line or end of word */
			CHECKSTRSPACE(3, out);	/* permit 3 calls to USTPUTC */
			switch(syntax[c]) {
			case CNL:	/* '\n' */
				if (syntax == BASESYNTAX)
					goto endword;	/* exit outer loop */
				USTPUTC(c, out);
				plinno++;
				if (doprompt)
					setprompt(2);
				else
					setprompt(0);
				c = pgetc();
				goto loop;		/* continue outer loop */
			case CWORD:
				USTPUTC(c, out);
				break;
			case CCTL:
				if ((eofmark == NULL || dblquote) &&
				    dqvarnest == 0)
					USTPUTC(CTLESC, out);
				USTPUTC(c, out);
				break;
			case CBACK:	/* backslash */
				c = pgetc2();
				if (c == PEOF) {
					USTPUTC('\\', out);
					pungetc();
				} else if (c == '\n') {
					if (doprompt)
						setprompt(2);
					else
						setprompt(0);
				} else {
					if (dblquote && c != '\\' && c != '`' && c != '$'
							 && (c != '"' || eofmark != NULL))
						USTPUTC('\\', out);
					if (SQSYNTAX[c] == CCTL)
						USTPUTC(CTLESC, out);
					else if (eofmark == NULL)
						USTPUTC(CTLQUOTEMARK, out);
					USTPUTC(c, out);
					quotef++;
				}
				break;
			case CSQUOTE:
				if (eofmark == NULL)
					USTPUTC(CTLQUOTEMARK, out);
				syntax = SQSYNTAX;
				break;
			case CDQUOTE:
				if (eofmark == NULL)
					USTPUTC(CTLQUOTEMARK, out);
				syntax = DQSYNTAX;
				dblquote = 1;
				break;
			case CENDQUOTE:
				if (eofmark != NULL && arinest == 0 &&
				    varnest == 0) {
					USTPUTC(c, out);
				} else {
					if (arinest) {
						syntax = ARISYNTAX;
						dblquote = 0;
					} else if (eofmark == NULL &&
						   dqvarnest == 0) {
						syntax = BASESYNTAX;
						dblquote = 0;
					}
					quotef++;
				}
				break;
			case CVAR:	/* '$' */
				PARSESUB();		/* parse substitution */
				break;
			case CENDVAR:	/* '}' */
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
#ifdef ASH_MATH_SUPPORT
			case CLP:	/* '(' in arithmetic */
				parenlevel++;
				USTPUTC(c, out);
				break;
			case CRP:	/* ')' in arithmetic */
				if (parenlevel > 0) {
					USTPUTC(c, out);
					--parenlevel;
				} else {
					if (pgetc() == ')') {
						if (--arinest == 0) {
							USTPUTC(CTLENDARI, out);
							syntax = prevsyntax;
							if (syntax == DQSYNTAX)
								dblquote = 1;
							else
								dblquote = 0;
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
			case CBQUOTE:	/* '`' */
				PARSEBACKQOLD();
				break;
			case CEOF:
				goto endword;		/* exit outer loop */
			case CIGN:
				break;
			default:
				if (varnest == 0)
					goto endword;	/* exit outer loop */
				if (c != PEOA) {
					USTPUTC(c, out);
				}
			}
			c = pgetc_macro();
		}
	}
endword:
	if (syntax == ARISYNTAX)
		synerror("Missing '))'");
	if (syntax != BASESYNTAX && ! parsebackquote && eofmark == NULL)
		synerror("Unterminated quoted string");
	if (varnest != 0) {
		startlinno = plinno;
		synerror("Missing '}'");
	}
	USTPUTC('\0', out);
	len = out - stackblock();
	out = stackblock();
	if (eofmark == NULL) {
		if ((c == '>' || c == '<')
		 && quotef == 0
		 && len <= 2
		 && (*out == '\0' || is_digit(*out))) {
			PARSEREDIR();
			return lasttoken = TREDIR;
		} else {
			pungetc();
		}
	}
	quoteflag = quotef;
	backquotelist = bqlist;
	grabstackblock(len);
	wordtext = out;
	return lasttoken = TWORD;
/* end of readtoken routine */



/*
 * Check to see whether we are at the end of the here document.  When this
 * is called, c is set to the first character of the next input line.  If
 * we are at the end of the here document, this routine sets the c to PEOF.
 */

checkend: {
	if (eofmark) {
		if (c == PEOA) {
			c = pgetc2();
		}
		if (striptabs) {
			while (c == '\t') {
				c = pgetc2();
			}
		}
		if (c == *eofmark) {
			if (pfgets(line, sizeof line) != NULL) {
				char *p, *q;

				p = line;
				for (q = eofmark + 1 ; *q && *p == *q ; p++, q++);
				if (*p == '\n' && *q == '\0') {
					c = PEOF;
					plinno++;
					needprompt = doprompt;
				} else {
					pushstring(line, strlen(line), NULL);
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

	np = (union node *)stalloc(sizeof (struct nfile));
	if (c == '>') {
		np->nfile.fd = 1;
		c = pgetc();
		if (c == '>')
			np->type = NAPPEND;
		else if (c == '&')
			np->type = NTOFD;
		else if (c == '|')
			np->type = NTOOV;
		else {
			np->type = NTO;
			pungetc();
		}
	} else {	/* c == '<' */
		np->nfile.fd = 0;
		switch (c = pgetc()) {
		case '<':
			if (sizeof (struct nfile) != sizeof (struct nhere)) {
				np = (union node *)stalloc(sizeof (struct nhere));
				np->nfile.fd = 0;
			}
			np->type = NHERE;
			heredoc = (struct heredoc *)stalloc(sizeof (struct heredoc));
			heredoc->here = np;
			if ((c = pgetc()) == '-') {
				heredoc->striptabs = 1;
			} else {
				heredoc->striptabs = 0;
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
		np->nfile.fd = digit_val(fd);
	redirnode = np;
	goto parseredir_return;
}


/*
 * Parse a substitution.  At this point, we have read the dollar sign
 * and nothing else.
 */

parsesub: {
	int subtype;
	int typeloc;
	int flags;
	char *p;
	static const char types[] = "}-+?=";

	c = pgetc();
	if (
		c <= PEOA  ||
		(c != '(' && c != '{' && !is_name(c) && !is_special(c))
	) {
		USTPUTC('$', out);
		pungetc();
	} else if (c == '(') {	/* $(command) or $((arith)) */
		if (pgetc() == '(') {
			PARSEARITH();
		} else {
			pungetc();
			PARSEBACKQNEW();
		}
	} else {
		USTPUTC(CTLVAR, out);
		typeloc = out - stackblock();
		USTPUTC(VSNORMAL, out);
		subtype = VSNORMAL;
		if (c == '{') {
			c = pgetc();
			if (c == '#') {
				if ((c = pgetc()) == '}')
					c = '#';
				else
					subtype = VSLENGTH;
			}
			else
				subtype = 0;
		}
		if (c > PEOA && is_name(c)) {
			do {
				STPUTC(c, out);
				c = pgetc();
			} while (c > PEOA && is_in_name(c));
		} else if (is_digit(c)) {
			do {
				USTPUTC(c, out);
				c = pgetc();
			} while (is_digit(c));
		}
		else if (is_special(c)) {
			USTPUTC(c, out);
			c = pgetc();
		}
		else
badsub:			synerror("Bad substitution");

		STPUTC('=', out);
		flags = 0;
		if (subtype == 0) {
			switch (c) {
			case ':':
				flags = VSNUL;
				c = pgetc();
				/*FALLTHROUGH*/
			default:
				p = strchr(types, c);
				if (p == NULL)
					goto badsub;
				subtype = p - types + VSNORMAL;
				break;
			case '%':
			case '#':
				{
					int cc = c;
					subtype = c == '#' ? VSTRIMLEFT :
							     VSTRIMRIGHT;
					c = pgetc();
					if (c == cc)
						subtype++;
					else
						pungetc();
					break;
				}
			}
		} else {
			pungetc();
		}
		if (dblquote || arinest)
			flags |= VSQUOTE;
		*(stackblock() + typeloc) = subtype | flags;
		if (subtype != VSNORMAL) {
			varnest++;
			if (dblquote) {
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
	int savepbq;
	union node *n;
	char *volatile str;
	struct jmploc jmploc;
	struct jmploc *volatile savehandler;
	int savelen;
	int saveprompt;
#ifdef __GNUC__
	(void) &saveprompt;
#endif

	savepbq = parsebackquote;
	if (setjmp(jmploc.loc)) {
		if (str)
			ckfree(str);
		parsebackquote = 0;
		handler = savehandler;
		longjmp(handler->loc, 1);
	}
	INTOFF;
	str = NULL;
	savelen = out - stackblock();
	if (savelen > 0) {
		str = ckmalloc(savelen);
		memcpy(str, stackblock(), savelen);
	}
	savehandler = handler;
	handler = &jmploc;
	INTON;
        if (oldstyle) {
                /* We must read until the closing backquote, giving special
                   treatment to some slashes, and then push the string and
                   reread it as input, interpreting it normally.  */
                char *pout;
                int pc;
                int psavelen;
                char *pstr;


                STARTSTACKSTR(pout);
		for (;;) {
			if (needprompt) {
				setprompt(2);
				needprompt = 0;
			}
			switch (pc = pgetc()) {
			case '`':
				goto done;

			case '\\':
                                if ((pc = pgetc()) == '\n') {
					plinno++;
					if (doprompt)
						setprompt(2);
					else
						setprompt(0);
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
				if (pc > PEOA) {
					break;
				}
				/* fall through */

			case PEOF:
			case PEOA:
			        startlinno = plinno;
				synerror("EOF in backquote substitution");

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
                psavelen = pout - stackblock();
                if (psavelen > 0) {
			pstr = grabstackstr(pout);
			setinputstring(pstr);
                }
        }
	nlpp = &bqlist;
	while (*nlpp)
		nlpp = &(*nlpp)->next;
	*nlpp = (struct nodelist *)stalloc(sizeof (struct nodelist));
	(*nlpp)->next = NULL;
	parsebackquote = oldstyle;

	if (oldstyle) {
		saveprompt = doprompt;
		doprompt = 0;
	}

	n = list(0);

	if (oldstyle)
		doprompt = saveprompt;
	else {
		if (readtoken() != TRP)
			synexpect(TRP);
	}

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
		INTOFF;
		ckfree(str);
		str = NULL;
		INTON;
	}
	parsebackquote = savepbq;
	handler = savehandler;
	if (arinest || dblquote)
		USTPUTC(CTLBACKQ | CTLQUOTE, out);
	else
		USTPUTC(CTLBACKQ, out);
	if (oldstyle)
		goto parsebackq_oldreturn;
	else
		goto parsebackq_newreturn;
}

/*
 * Parse an arithmetic expansion (indicate start of one and set state)
 */
parsearith: {

	if (++arinest == 1) {
		prevsyntax = syntax;
		syntax = ARISYNTAX;
		USTPUTC(CTLARI, out);
		if (dblquote)
			USTPUTC('"',out);
		else
			USTPUTC(' ',out);
	} else {
		/*
		 * we collapse embedded arithmetic expansion to
		 * parenthesis, which should be equivalent
		 */
		USTPUTC('(', out);
	}
	goto parsearith_return;
}

} /* end of readtoken */



#ifdef mkinit
INCLUDE "parser.h"
RESET {
	tokpushback = 0;
	checkkwd = 0;
	checkalias = 0;
}
#endif

/*
 * Returns true if the text contains nothing to expand (no dollar signs
 * or backquotes).
 */

static int
noexpand(text)
	char *text;
	{
	char *p;
	char c;

	p = text;
	while ((c = *p++) != '\0') {
		if (c == CTLQUOTEMARK)
			continue;
		if (c == CTLESC)
			p++;
		else if (BASESYNTAX[(int)c] == CCTL)
			return 0;
	}
	return 1;
}


/*
 * Return true if the argument is a legal variable name (a letter or
 * underscore followed by zero or more letters, underscores, and digits).
 */

static int
goodname(char *name)
	{
	char *p;

	p = name;
	if (! is_name(*p))
		return 0;
	while (*++p) {
		if (! is_in_name(*p))
			return 0;
	}
	return 1;
}


/*
 * Called when an unexpected token is read during the parse.  The argument
 * is the token that is expected, or -1 if more than one type of token can
 * occur at this point.
 */

static void
synexpect(token)
	int token;
{
	char msg[64];

	if (token >= 0) {
		fmtstr(msg, 64, "%s unexpected (expecting %s)",
			tokname[lasttoken], tokname[token]);
	} else {
		fmtstr(msg, 64, "%s unexpected", tokname[lasttoken]);
	}
	synerror(msg);
	/* NOTREACHED */
}


static void
synerror(msg)
	const char *msg;
	{
	if (commandname)
		outfmt(&errout, "%s: %d: ", commandname, startlinno);
	outfmt(&errout, "Syntax error: %s\n", msg);
	error((char *)NULL);
	/* NOTREACHED */
}

static void
setprompt(int which)
{
    whichprompt = which;
    putprompt(getprompt(NULL));
}

/*
 * called by editline -- any expansions to the prompt
 *    should be added here.
 */
static const char *
getprompt(void *unused)
	{
	switch (whichprompt) {
	case 0:
		return "";
	case 1:
		return ps1val();
	case 2:
		return ps2val();
	default:
		return "<internal prompt error>";
	}
}

static int
isassignment(const char *word) {
	if (!is_name(*word)) {
		return 0;
	}
	do {
		word++;
	} while (is_in_name(*word));
	return *word == '=';
}

static const char *const *
findkwd(const char *s) {
	return findstring(
		s, parsekwd, sizeof(parsekwd) / sizeof(const char *)
	);
}
/*	$NetBSD: redir.c,v 1.22 2000/05/22 10:18:47 elric Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

/*
 * Code for dealing with input/output redirection.
 */

#define EMPTY -2		/* marks an unused slot in redirtab */
#ifndef PIPE_BUF
# define PIPESIZE 4096		/* amount of buffering in a pipe */
#else
# define PIPESIZE PIPE_BUF
#endif


struct redirtab *redirlist;

/*
 * We keep track of whether or not fd0 has been redirected.  This is for
 * background commands, where we want to redirect fd0 to /dev/null only
 * if it hasn't already been redirected.
*/
static int fd0_redirected = 0;

/*
 * We also keep track of where fileno2 goes.
 */
static int fileno2 = 2;

static int openredirect __P((union node *));
static void dupredirect __P((union node *, int, char[10 ]));
static int openhere __P((union node *));
static int noclobberopen __P((const char *));


/*
 * Process a list of redirection commands.  If the REDIR_PUSH flag is set,
 * old file descriptors are stashed away so that the redirection can be
 * undone by calling popredir.  If the REDIR_BACKQ flag is set, then the
 * standard output, and the standard error if it becomes a duplicate of
 * stdout, is saved in memory.
 */

static void
redirect(redir, flags)
	union node *redir;
	int flags;
	{
	union node *n;
	struct redirtab *sv = NULL;
	int i;
	int fd;
	int newfd;
	int try;
	char memory[10];	/* file descriptors to write to memory */

	for (i = 10 ; --i >= 0 ; )
		memory[i] = 0;
	memory[1] = flags & REDIR_BACKQ;
	if (flags & REDIR_PUSH) {
		sv = ckmalloc(sizeof (struct redirtab));
		for (i = 0 ; i < 10 ; i++)
			sv->renamed[i] = EMPTY;
		sv->next = redirlist;
		redirlist = sv;
	}
	for (n = redir ; n ; n = n->nfile.next) {
		fd = n->nfile.fd;
		try = 0;
		if ((n->nfile.type == NTOFD || n->nfile.type == NFROMFD) &&
		    n->ndup.dupfd == fd)
			continue; /* redirect from/to same file descriptor */

		INTOFF;
		newfd = openredirect(n);
		if (((flags & REDIR_PUSH) && sv->renamed[fd] == EMPTY) ||
		    (fd == fileno2)) {
			if (newfd == fd) {
				try++;
			} else if ((i = fcntl(fd, F_DUPFD, 10)) == -1) {
				switch (errno) {
				case EBADF:
					if (!try) {
						dupredirect(n, newfd, memory);
						try++;
						break;
					}
					/* FALLTHROUGH*/
				default:
					if (newfd >= 0) {
						close(newfd);
					}
					INTON;
					error("%d: %s", fd, strerror(errno));
					/* NOTREACHED */
				}
			}
			if (!try) {
				close(fd);
				if (flags & REDIR_PUSH) {
					sv->renamed[fd] = i;
				}
				if (fd == fileno2) {
					fileno2 = i;
				}
			}
		} else if (fd != newfd) {
			close(fd);
		}
                if (fd == 0)
                        fd0_redirected++;
		if (!try)
			dupredirect(n, newfd, memory);
		INTON;
	}
	if (memory[1])
		out1 = &memout;
	if (memory[2])
		out2 = &memout;
}


static int
openredirect(redir)
	union node *redir;
	{
	char *fname;
	int f;

	switch (redir->nfile.type) {
	case NFROM:
		fname = redir->nfile.expfname;
		if ((f = open(fname, O_RDONLY)) < 0)
			goto eopen;
		break;
	case NFROMTO:
		fname = redir->nfile.expfname;
		if ((f = open(fname, O_RDWR|O_CREAT|O_TRUNC, 0666)) < 0)
			goto ecreate;
		break;
	case NTO:
		/* Take care of noclobber mode. */
		if (Cflag) {
			fname = redir->nfile.expfname;
			if ((f = noclobberopen(fname)) < 0)
				goto ecreate;
			break;
		}
	case NTOOV:
		fname = redir->nfile.expfname;
#ifdef O_CREAT
		if ((f = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0)
			goto ecreate;
#else
		if ((f = creat(fname, 0666)) < 0)
			goto ecreate;
#endif
		break;
	case NAPPEND:
		fname = redir->nfile.expfname;
#ifdef O_APPEND
		if ((f = open(fname, O_WRONLY|O_CREAT|O_APPEND, 0666)) < 0)
			goto ecreate;
#else
		if ((f = open(fname, O_WRONLY)) < 0
		 && (f = creat(fname, 0666)) < 0)
			goto ecreate;
		lseek(f, (off_t)0, 2);
#endif
		break;
	default:
#ifdef DEBUG
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
	error("cannot create %s: %s", fname, errmsg(errno, E_CREAT));
eopen:
	error("cannot open %s: %s", fname, errmsg(errno, E_OPEN));
}


static void
dupredirect(redir, f, memory)
	union node *redir;
	int f;
	char memory[10];
	{
	int fd = redir->nfile.fd;

	memory[fd] = 0;
	if (redir->nfile.type == NTOFD || redir->nfile.type == NFROMFD) {
		if (redir->ndup.dupfd >= 0) {	/* if not ">&-" */
			if (memory[redir->ndup.dupfd])
				memory[fd] = 1;
			else
				dup_as_newfd(redir->ndup.dupfd, fd);
		}
		return;
	}

	if (f != fd) {
		dup_as_newfd(f, fd);
		close(f);
	}
	return;
}


/*
 * Handle here documents.  Normally we fork off a process to write the
 * data to a pipe.  If the document is short, we can stuff the data in
 * the pipe without forking.
 */

static int
openhere(redir)
	union node *redir;
	{
	int pip[2];
	int len = 0;

	if (pipe(pip) < 0)
		error("Pipe call failed");
	if (redir->type == NHERE) {
		len = strlen(redir->nhere.doc->narg.text);
		if (len <= PIPESIZE) {
			xwrite(pip[1], redir->nhere.doc->narg.text, len);
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
			xwrite(pip[1], redir->nhere.doc->narg.text, len);
		else
			expandhere(redir->nhere.doc, pip[1]);
		_exit(0);
	}
out:
	close(pip[1]);
	return pip[0];
}



/*
 * Undo the effects of the last redirection.
 */

static void
popredir() {
	struct redirtab *rp = redirlist;
	int i;

	INTOFF;
	for (i = 0 ; i < 10 ; i++) {
		if (rp->renamed[i] != EMPTY) {
                        if (i == 0)
                                fd0_redirected--;
			close(i);
			if (rp->renamed[i] >= 0) {
				dup_as_newfd(rp->renamed[i], i);
				close(rp->renamed[i]);
			}
			if (rp->renamed[i] == fileno2) {
				fileno2 = i;
			}
		}
	}
	redirlist = rp->next;
	ckfree(rp);
	INTON;
}

/*
 * Undo all redirections.  Called on error or interrupt.
 */

#ifdef mkinit

INCLUDE "redir.h"

RESET {
	while (redirlist)
		popredir();
}

SHELLPROC {
	clearredir();
}

#endif

/* Return true if fd 0 has already been redirected at least once.  */
static int
fd0_redirected_p () {
        return fd0_redirected != 0;
}

/*
 * Discard all saved file descriptors.
 */

static void
clearredir() {
	struct redirtab *rp;
	int i;

	for (rp = redirlist ; rp ; rp = rp->next) {
		for (i = 0 ; i < 10 ; i++) {
			if (rp->renamed[i] >= 0) {
				close(rp->renamed[i]);
				if (rp->renamed[i] == fileno2) {
					fileno2 = -1;
				}
			}
			rp->renamed[i] = EMPTY;
		}
	}
	if (fileno2 != 2 && fileno2 >= 0) {
		close(fileno2);
		fileno2 = -1;
	}
}



/*
 * Copy a file descriptor to be >= to.  Returns -1
 * if the source file descriptor is closed, EMPTY if there are no unused
 * file descriptors left.
 */

static int
dup_as_newfd(from, to)
	int from;
	int to;
{
	int newfd;

	newfd = fcntl(from, F_DUPFD, to);
	if (newfd < 0) {
		if (errno == EMFILE)
			return EMPTY;
		else
			error("%d: %s", from, strerror(errno));
	}
	return newfd;
}

/*
 * Open a file in noclobber mode.
 * The code was copied from bash.
 */
static int
noclobberopen(fname)
	const char *fname;
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
	 if (fstat(fd, &finfo2) == 0 && !S_ISREG(finfo2.st_mode) &&
	     finfo.st_dev == finfo2.st_dev && finfo.st_ino == finfo2.st_ino)
	 	return fd;

	/* The file has been replaced.  badness. */
	close(fd);
	errno = EEXIST;
	return -1;
}
/*	$NetBSD: setmode.c,v 1.28 2000/01/25 15:43:43 enami Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Dave Borman at Cray Research, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#ifdef __weak_alias
__weak_alias(getmode,_getmode)
__weak_alias(setmode,_setmode)
#endif

#ifdef __GLIBC__
#define S_ISTXT __S_ISVTX
#endif

#define	SET_LEN	6		/* initial # of bitcmd struct to malloc */
#define	SET_LEN_INCR 4		/* # of bitcmd structs to add as needed */

typedef struct bitcmd {
	char	cmd;
	char	cmd2;
	mode_t	bits;
} BITCMD;

#define	CMD2_CLR	0x01
#define	CMD2_SET	0x02
#define	CMD2_GBITS	0x04
#define	CMD2_OBITS	0x08
#define	CMD2_UBITS	0x10

static BITCMD	*addcmd __P((BITCMD *, int, int, int, u_int));
static void	 compress_mode __P((BITCMD *));
#ifdef SETMODE_DEBUG
static void	 dumpmode __P((BITCMD *));
#endif

/*
 * Given the old mode and an array of bitcmd structures, apply the operations
 * described in the bitcmd structures to the old mode, and return the new mode.
 * Note that there is no '=' command; a strict assignment is just a '-' (clear
 * bits) followed by a '+' (set bits).
 */
mode_t
getmode(bbox, omode)
	const void *bbox;
	mode_t omode;
{
	const BITCMD *set;
	mode_t clrval, newmode, value;

	_DIAGASSERT(bbox != NULL);

	set = (const BITCMD *)bbox;
	newmode = omode;
	for (value = 0;; set++)
		switch(set->cmd) {
		/*
		 * When copying the user, group or other bits around, we "know"
		 * where the bits are in the mode so that we can do shifts to
		 * copy them around.  If we don't use shifts, it gets real
		 * grundgy with lots of single bit checks and bit sets.
		 */
		case 'u':
			value = (newmode & S_IRWXU) >> 6;
			goto common;

		case 'g':
			value = (newmode & S_IRWXG) >> 3;
			goto common;

		case 'o':
			value = newmode & S_IRWXO;
common:			if (set->cmd2 & CMD2_CLR) {
				clrval =
				    (set->cmd2 & CMD2_SET) ?  S_IRWXO : value;
				if (set->cmd2 & CMD2_UBITS)
					newmode &= ~((clrval<<6) & set->bits);
				if (set->cmd2 & CMD2_GBITS)
					newmode &= ~((clrval<<3) & set->bits);
				if (set->cmd2 & CMD2_OBITS)
					newmode &= ~(clrval & set->bits);
			}
			if (set->cmd2 & CMD2_SET) {
				if (set->cmd2 & CMD2_UBITS)
					newmode |= (value<<6) & set->bits;
				if (set->cmd2 & CMD2_GBITS)
					newmode |= (value<<3) & set->bits;
				if (set->cmd2 & CMD2_OBITS)
					newmode |= value & set->bits;
			}
			break;

		case '+':
			newmode |= set->bits;
			break;

		case '-':
			newmode &= ~set->bits;
			break;

		case 'X':
			if (omode & (S_IFDIR|S_IXUSR|S_IXGRP|S_IXOTH))
				newmode |= set->bits;
			break;

		case '\0':
		default:
#ifdef SETMODE_DEBUG
			(void)printf("getmode:%04o -> %04o\n", omode, newmode);
#endif
			return (newmode);
		}
}

#define	ADDCMD(a, b, c, d) do {						\
	if (set >= endset) {						\
		BITCMD *newset;						\
		setlen += SET_LEN_INCR;					\
		newset = realloc(saveset, sizeof(BITCMD) * setlen);	\
		if (newset == NULL) {					\
			free(saveset);					\
			return (NULL);					\
		}							\
		set = newset + (set - saveset);				\
		saveset = newset;					\
		endset = newset + (setlen - 2);				\
	}								\
	set = addcmd(set, (a), (b), (c), (d));				\
} while (/*CONSTCOND*/0)

#define	STANDARD_BITS	(S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO)

static void *
setmode(p)
	const char *p;
{
	int perm, who;
	char op, *ep;
	BITCMD *set, *saveset, *endset;
	sigset_t mysigset, sigoset;
	mode_t mask;
	int equalopdone = 0;	/* pacify gcc */
	int permXbits, setlen;

	if (!*p)
		return (NULL);

	/*
	 * Get a copy of the mask for the permissions that are mask relative.
	 * Flip the bits, we want what's not set.  Since it's possible that
	 * the caller is opening files inside a signal handler, protect them
	 * as best we can.
	 */
	sigfillset(&mysigset);
	(void)sigprocmask(SIG_BLOCK, &mysigset, &sigoset);
	(void)umask(mask = umask(0));
	mask = ~mask;
	(void)sigprocmask(SIG_SETMASK, &sigoset, NULL);

	setlen = SET_LEN + 2;
	
	if ((set = malloc((u_int)(sizeof(BITCMD) * setlen))) == NULL)
		return (NULL);
	saveset = set;
	endset = set + (setlen - 2);

	/*
	 * If an absolute number, get it and return; disallow non-octal digits
	 * or illegal bits.
	 */
	if (isdigit((unsigned char)*p)) {
		perm = (mode_t)strtol(p, &ep, 8);
		if (*ep || perm & ~(STANDARD_BITS|S_ISTXT)) {
			free(saveset);
			return (NULL);
		}
		ADDCMD('=', (STANDARD_BITS|S_ISTXT), perm, mask);
		set->cmd = 0;
		return (saveset);
	}

	/*
	 * Build list of structures to set/clear/copy bits as described by
	 * each clause of the symbolic mode.
	 */
	for (;;) {
		/* First, find out which bits might be modified. */
		for (who = 0;; ++p) {
			switch (*p) {
			case 'a':
				who |= STANDARD_BITS;
				break;
			case 'u':
				who |= S_ISUID|S_IRWXU;
				break;
			case 'g':
				who |= S_ISGID|S_IRWXG;
				break;
			case 'o':
				who |= S_IRWXO;
				break;
			default:
				goto getop;
			}
		}

getop:		if ((op = *p++) != '+' && op != '-' && op != '=') {
			free(saveset);
			return (NULL);
		}
		if (op == '=')
			equalopdone = 0;

		who &= ~S_ISTXT;
		for (perm = 0, permXbits = 0;; ++p) {
			switch (*p) {
			case 'r':
				perm |= S_IRUSR|S_IRGRP|S_IROTH;
				break;
			case 's':
				/*
				 * If specific bits where requested and 
				 * only "other" bits ignore set-id. 
				 */
				if (who == 0 || (who & ~S_IRWXO))
					perm |= S_ISUID|S_ISGID;
				break;
			case 't':
				/*
				 * If specific bits where requested and 
				 * only "other" bits ignore set-id. 
				 */
				if (who == 0 || (who & ~S_IRWXO)) {
					who |= S_ISTXT;
					perm |= S_ISTXT;
				}
				break;
			case 'w':
				perm |= S_IWUSR|S_IWGRP|S_IWOTH;
				break;
			case 'X':
				permXbits = S_IXUSR|S_IXGRP|S_IXOTH;
				break;
			case 'x':
				perm |= S_IXUSR|S_IXGRP|S_IXOTH;
				break;
			case 'u':
			case 'g':
			case 'o':
				/*
				 * When ever we hit 'u', 'g', or 'o', we have
				 * to flush out any partial mode that we have,
				 * and then do the copying of the mode bits.
				 */
				if (perm) {
					ADDCMD(op, who, perm, mask);
					perm = 0;
				}
				if (op == '=')
					equalopdone = 1;
				if (op == '+' && permXbits) {
					ADDCMD('X', who, permXbits, mask);
					permXbits = 0;
				}
				ADDCMD(*p, who, op, mask);
				break;

			default:
				/*
				 * Add any permissions that we haven't already
				 * done.
				 */
				if (perm || (op == '=' && !equalopdone)) {
					if (op == '=')
						equalopdone = 1;
					ADDCMD(op, who, perm, mask);
					perm = 0;
				}
				if (permXbits) {
					ADDCMD('X', who, permXbits, mask);
					permXbits = 0;
				}
				goto apply;
			}
		}

apply:		if (!*p)
			break;
		if (*p != ',')
			goto getop;
		++p;
	}
	set->cmd = 0;
#ifdef SETMODE_DEBUG
	(void)printf("Before compress_mode()\n");
	dumpmode(saveset);
#endif
	compress_mode(saveset);
#ifdef SETMODE_DEBUG
	(void)printf("After compress_mode()\n");
	dumpmode(saveset);
#endif
	return (saveset);
}

static BITCMD *
addcmd(set, op, who, oparg, mask)
	BITCMD *set;
	int oparg, who;
	int op;
	u_int mask;
{

	_DIAGASSERT(set != NULL);

	switch (op) {
	case '=':
		set->cmd = '-';
		set->bits = who ? who : STANDARD_BITS;
		set++;

		op = '+';
		/* FALLTHROUGH */
	case '+':
	case '-':
	case 'X':
		set->cmd = op;
		set->bits = (who ? who : mask) & oparg;
		break;

	case 'u':
	case 'g':
	case 'o':
		set->cmd = op;
		if (who) {
			set->cmd2 = ((who & S_IRUSR) ? CMD2_UBITS : 0) |
				    ((who & S_IRGRP) ? CMD2_GBITS : 0) |
				    ((who & S_IROTH) ? CMD2_OBITS : 0);
			set->bits = (mode_t)~0;
		} else {
			set->cmd2 = CMD2_UBITS | CMD2_GBITS | CMD2_OBITS;
			set->bits = mask;
		}
	
		if (oparg == '+')
			set->cmd2 |= CMD2_SET;
		else if (oparg == '-')
			set->cmd2 |= CMD2_CLR;
		else if (oparg == '=')
			set->cmd2 |= CMD2_SET|CMD2_CLR;
		break;
	}
	return (set + 1);
}

#ifdef SETMODE_DEBUG
static void
dumpmode(set)
	BITCMD *set;
{

	_DIAGASSERT(set != NULL);

	for (; set->cmd; ++set)
		(void)printf("cmd: '%c' bits %04o%s%s%s%s%s%s\n",
		    set->cmd, set->bits, set->cmd2 ? " cmd2:" : "",
		    set->cmd2 & CMD2_CLR ? " CLR" : "",
		    set->cmd2 & CMD2_SET ? " SET" : "",
		    set->cmd2 & CMD2_UBITS ? " UBITS" : "",
		    set->cmd2 & CMD2_GBITS ? " GBITS" : "",
		    set->cmd2 & CMD2_OBITS ? " OBITS" : "");
}
#endif

/*
 * Given an array of bitcmd structures, compress by compacting consecutive
 * '+', '-' and 'X' commands into at most 3 commands, one of each.  The 'u',
 * 'g' and 'o' commands continue to be separate.  They could probably be 
 * compacted, but it's not worth the effort.
 */
static void
compress_mode(set)
	BITCMD *set;
{
	BITCMD *nset;
	int setbits, clrbits, Xbits, op;

	_DIAGASSERT(set != NULL);

	for (nset = set;;) {
		/* Copy over any 'u', 'g' and 'o' commands. */
		while ((op = nset->cmd) != '+' && op != '-' && op != 'X') {
			*set++ = *nset++;
			if (!op)
				return;
		}

		for (setbits = clrbits = Xbits = 0;; nset++) {
			if ((op = nset->cmd) == '-') {
				clrbits |= nset->bits;
				setbits &= ~nset->bits;
				Xbits &= ~nset->bits;
			} else if (op == '+') {
				setbits |= nset->bits;
				clrbits &= ~nset->bits;
				Xbits &= ~nset->bits;
			} else if (op == 'X')
				Xbits |= nset->bits & ~setbits;
			else
				break;
		}
		if (clrbits) {
			set->cmd = '-';
			set->cmd2 = 0;
			set->bits = clrbits;
			set++;
		}
		if (setbits) {
			set->cmd = '+';
			set->cmd2 = 0;
			set->bits = setbits;
			set++;
		}
		if (Xbits) {
			set->cmd = 'X';
			set->cmd2 = 0;
			set->bits = Xbits;
			set++;
		}
	}
}
/*	$NetBSD: show.c,v 1.18 1999/10/08 21:10:44 pk Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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


#ifdef DEBUG
static void shtree __P((union node *, int, char *, FILE*));
static void shcmd __P((union node *, FILE *));
static void sharg __P((union node *, FILE *));
static void indent __P((int, char *, FILE *));
static void trstring __P((char *));


static void
showtree(n)
	union node *n;
{
	trputs("showtree called\n");
	shtree(n, 1, NULL, stdout);
}


static void
shtree(n, ind, pfx, fp)
	union node *n;
	int ind;
	char *pfx;
	FILE *fp;
{
	struct nodelist *lp;
	const char *s;

	if (n == NULL)
		return;

	indent(ind, pfx, fp);
	switch(n->type) {
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
	   /*    if (ind < 0) */
			fputs(s, fp);
		shtree(n->nbinary.ch2, ind, NULL, fp);
		break;
	case NCMD:
		shcmd(n, fp);
		if (ind >= 0)
			putc('\n', fp);
		break;
	case NPIPE:
		for (lp = n->npipe.cmdlist ; lp ; lp = lp->next) {
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
shcmd(cmd, fp)
	union node *cmd;
	FILE *fp;
{
	union node *np;
	int first;
	const char *s;
	int dftfd;

	first = 1;
	for (np = cmd->ncmd.args ; np ; np = np->narg.next) {
		if (! first)
			putchar(' ');
		sharg(np, fp);
		first = 0;
	}
	for (np = cmd->ncmd.redirect ; np ; np = np->nfile.next) {
		if (! first)
			putchar(' ');
		switch (np->nfile.type) {
			case NTO:	s = ">";  dftfd = 1; break;
			case NAPPEND:	s = ">>"; dftfd = 1; break;
			case NTOFD:	s = ">&"; dftfd = 1; break;
			case NTOOV:	s = ">|"; dftfd = 1; break;
			case NFROM:	s = "<";  dftfd = 0; break;
			case NFROMFD:	s = "<&"; dftfd = 0; break;
			case NFROMTO:	s = "<>"; dftfd = 0; break;
			default:  	s = "*error*"; dftfd = 0; break;
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
sharg(arg, fp)
	union node *arg;
	FILE *fp;
	{
	char *p;
	struct nodelist *bqlist;
	int subtype;

	if (arg->type != NARG) {
		printf("<node type %d>\n", arg->type);
		fflush(stdout);
		abort();
	}
	bqlist = arg->narg.backquote;
	for (p = arg->narg.text ; *p ; p++) {
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
				printf("<subtype %d>", subtype);
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
indent(amount, pfx, fp)
	int amount;
	char *pfx;
	FILE *fp;
{
	int i;

	for (i = 0 ; i < amount ; i++) {
		if (pfx && i == amount - 1)
			fputs(pfx, fp);
		putc('\t', fp);
	}
}
#endif



/*
 * Debugging stuff.
 */


#ifdef DEBUG
FILE *tracefile;

#if DEBUG == 2
static int debug = 1;
#else
static int debug = 0;
#endif


static void
trputc(c)
	int c;
{
	if (tracefile == NULL)
		return;
	putc(c, tracefile);
	if (c == '\n')
		fflush(tracefile);
}

static void
trace(const char *fmt, ...)
{
	va_list va;
#ifdef __STDC__
	va_start(va, fmt);
#else
	char *fmt;
	va_start(va);
	fmt = va_arg(va, char *);
#endif
	if (tracefile != NULL) {
		(void) vfprintf(tracefile, fmt, va);
		if (strchr(fmt, '\n'))
			(void) fflush(tracefile);
	}
	va_end(va);
}


static void
trputs(s)
	const char *s;
{
	if (tracefile == NULL)
		return;
	fputs(s, tracefile);
	if (strchr(s, '\n'))
		fflush(tracefile);
}


static void
trstring(s)
	char *s;
{
	char *p;
	char c;

	if (tracefile == NULL)
		return;
	putc('"', tracefile);
	for (p = s ; *p ; p++) {
		switch (*p) {
		case '\n':  c = 'n';  goto backslash;
		case '\t':  c = 't';  goto backslash;
		case '\r':  c = 'r';  goto backslash;
		case '"':  c = '"';  goto backslash;
		case '\\':  c = '\\';  goto backslash;
		case CTLESC:  c = 'e';  goto backslash;
		case CTLVAR:  c = 'v';  goto backslash;
		case CTLVAR+CTLQUOTE:  c = 'V';  goto backslash;
		case CTLBACKQ:  c = 'q';  goto backslash;
		case CTLBACKQ+CTLQUOTE:  c = 'Q';  goto backslash;
backslash:	  putc('\\', tracefile);
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
trargs(ap)
	char **ap;
{
	if (tracefile == NULL)
		return;
	while (*ap) {
		trstring(*ap++);
		if (*ap)
			putc(' ', tracefile);
		else
			putc('\n', tracefile);
	}
	fflush(tracefile);
}


static void
opentrace() {
	char s[100];
#ifdef O_APPEND
	int flags;
#endif

	if (!debug)
		return;
#ifdef not_this_way
	{
		char *p;
		if ((p = getenv("HOME")) == NULL) {
			if (geteuid() == 0)
				p = "/";
			else
				p = "/tmp";
		}
		scopy(p, s);
		strcat(s, "/trace");
	}
#else
	scopy("./trace", s);
#endif /* not_this_way */
	if ((tracefile = fopen(s, "a")) == NULL) {
		fprintf(stderr, "Can't open %s\n", s);
		return;
	}
#ifdef O_APPEND
	if ((flags = fcntl(fileno(tracefile), F_GETFL, 0)) >= 0)
		fcntl(fileno(tracefile), F_SETFL, flags | O_APPEND);
#endif
	fputs("\nTracing started.\n", tracefile);
	fflush(tracefile);
}
#endif /* DEBUG */


/*
 * This file was generated by the mksyntax program.
 */

/* syntax table used when not in quotes */
static const char basesyntax[257] = {
      CEOF,    CSPCL,   CWORD,   CCTL,
      CCTL,    CCTL,    CCTL,    CCTL,
      CCTL,    CCTL,    CCTL,    CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CSPCL,
      CNL,     CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CSPCL,   CWORD,
      CDQUOTE, CWORD,   CVAR,    CWORD,
      CSPCL,   CSQUOTE, CSPCL,   CSPCL,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CSPCL,   CSPCL,   CWORD,
      CSPCL,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CBACK,   CWORD,
      CWORD,   CWORD,   CBQUOTE, CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CSPCL,   CENDVAR,
      CWORD
};

/* syntax table used when in double quotes */
static const char dqsyntax[257] = {
      CEOF,    CIGN,    CWORD,   CCTL,
      CCTL,    CCTL,    CCTL,    CCTL,
      CCTL,    CCTL,    CCTL,    CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CNL,     CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CCTL,
      CENDQUOTE,CWORD,  CVAR,    CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CCTL,    CWORD,   CWORD,   CCTL,
      CWORD,   CCTL,    CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CCTL,    CWORD,   CWORD,   CCTL,
      CWORD,   CCTL,    CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CCTL,    CBACK,   CCTL,
      CWORD,   CWORD,   CBQUOTE, CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CENDVAR,
      CCTL
};

/* syntax table used when in single quotes */
static const char sqsyntax[257] = {
      CEOF,    CIGN,    CWORD,   CCTL,
      CCTL,    CCTL,    CCTL,    CCTL,
      CCTL,    CCTL,    CCTL,    CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CNL,     CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CCTL,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CENDQUOTE,CWORD,  CWORD,
      CCTL,    CWORD,   CWORD,   CCTL,
      CWORD,   CCTL,    CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CCTL,    CWORD,   CWORD,   CCTL,
      CWORD,   CCTL,    CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CCTL,    CCTL,    CCTL,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CCTL
};

/* syntax table used when in arithmetic */
static const char arisyntax[257] = {
      CEOF,    CIGN,    CWORD,   CCTL,
      CCTL,    CCTL,    CCTL,    CCTL,
      CCTL,    CCTL,    CCTL,    CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CNL,     CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CDQUOTE, CWORD,   CVAR,    CWORD,
      CWORD,   CSQUOTE, CLP,     CRP,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CBACK,   CWORD,
      CWORD,   CWORD,   CBQUOTE, CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CENDVAR,
      CWORD
};

/* character classification table */
static const char is_type[257] = {
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       ISSPECL,
      0,       ISSPECL, ISSPECL, 0,
      0,       0,       0,       0,
      ISSPECL, 0,       0,       ISSPECL,
      0,       0,       ISDIGIT, ISDIGIT,
      ISDIGIT, ISDIGIT, ISDIGIT, ISDIGIT,
      ISDIGIT, ISDIGIT, ISDIGIT, ISDIGIT,
      0,       0,       0,       0,
      0,       ISSPECL, ISSPECL, ISUPPER,
      ISUPPER, ISUPPER, ISUPPER, ISUPPER,
      ISUPPER, ISUPPER, ISUPPER, ISUPPER,
      ISUPPER, ISUPPER, ISUPPER, ISUPPER,
      ISUPPER, ISUPPER, ISUPPER, ISUPPER,
      ISUPPER, ISUPPER, ISUPPER, ISUPPER,
      ISUPPER, ISUPPER, ISUPPER, ISUPPER,
      ISUPPER, 0,       0,       0,
      0,       ISUNDER, 0,       ISLOWER,
      ISLOWER, ISLOWER, ISLOWER, ISLOWER,
      ISLOWER, ISLOWER, ISLOWER, ISLOWER,
      ISLOWER, ISLOWER, ISLOWER, ISLOWER,
      ISLOWER, ISLOWER, ISLOWER, ISLOWER,
      ISLOWER, ISLOWER, ISLOWER, ISLOWER,
      ISLOWER, ISLOWER, ISLOWER, ISLOWER,
      ISLOWER, 0,       0,       0,
      0
};
/*	$NetBSD: trap.c,v 1.25 2001/02/04 19:52:07 christos Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

/*
 * Sigmode records the current value of the signal handlers for the various
 * modes.  A value of zero means that the current handler is not known.
 * S_HARD_IGN indicates that the signal was ignored on entry to the shell,
 */

/*
 * The trap builtin.
 */

static int
trapcmd(argc, argv)
	int argc;
	char **argv;
{
	char *action;
	char **ap;
	int signo;

	if (argc <= 1) {
		for (signo = 0 ; signo < NSIG ; signo++) {
			if (trap[signo] != NULL) {
				char *p;

				p = single_quote(trap[signo]);
				out1fmt("trap -- %s %s\n", p,
					signal_names[signo] + (signo ? 3 : 0)
				);
				stunalloc(p);
			}
		}
		return 0;
	}
	ap = argv + 1;
	if (argc == 2)
		action = NULL;
	else
		action = *ap++;
	while (*ap) {
		if ((signo = decode_signal(*ap, 0)) < 0)
			error("%s: bad trap", *ap);
		INTOFF;
		if (action) {
			if (action[0] == '-' && action[1] == '\0')
				action = NULL;
			else
				action = savestr(action);
		}
		if (trap[signo])
			ckfree(trap[signo]);
		trap[signo] = action;
		if (signo != 0)
			setsignal(signo);
		INTON;
		ap++;
	}
	return 0;
}



/*
 * Clear traps on a fork.
 */

static void
clear_traps() {
	char **tp;

	for (tp = trap ; tp < &trap[NSIG] ; tp++) {
		if (*tp && **tp) {	/* trap not NULL or SIG_IGN */
			INTOFF;
			ckfree(*tp);
			*tp = NULL;
			if (tp != &trap[0])
				setsignal(tp - trap);
			INTON;
		}
	}
}



/*
 * Set the signal handler for the specified signal.  The routine figures
 * out what it should be set to.
 */

static void
setsignal(signo)
	int signo;
{
	int action;
	char *t;
	struct sigaction act;

	if ((t = trap[signo]) == NULL)
		action = S_DFL;
	else if (*t != '\0')
		action = S_CATCH;
	else
		action = S_IGN;
	if (rootshell && action == S_DFL) {
		switch (signo) {
		case SIGINT:
			if (iflag || minusc || sflag == 0)
				action = S_CATCH;
			break;
		case SIGQUIT:
#ifdef DEBUG
			{
			extern int debug;

			if (debug)
				break;
			}
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
	if (*t == 0) {
		/*
		 * current setting unknown
		 */
		if (sigaction(signo, 0, &act) == -1) {
			/*
			 * Pretend it worked; maybe we should give a warning
			 * here, but other shells don't. We don't alter
			 * sigmode, so that we retry every time.
			 */
			return;
		}
		if (act.sa_handler == SIG_IGN) {
			if (mflag && (signo == SIGTSTP ||
			     signo == SIGTTIN || signo == SIGTTOU)) {
				*t = S_IGN;	/* don't hard ignore these */
			} else
				*t = S_HARD_IGN;
		} else {
			*t = S_RESET;	/* force to be set */
		}
	}
	if (*t == S_HARD_IGN || *t == action)
		return;
	switch (action) {
	case S_CATCH:
		act.sa_handler = onsig;
		break;
	case S_IGN:
		act.sa_handler = SIG_IGN;
		break;
	default:
		act.sa_handler = SIG_DFL;
	}
	*t = action;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaction(signo, &act, 0);
}

/*
 * Ignore a signal.
 */

static void
ignoresig(signo)
	int signo;
{
	if (sigmode[signo - 1] != S_IGN && sigmode[signo - 1] != S_HARD_IGN) {
		signal(signo, SIG_IGN);
	}
	sigmode[signo - 1] = S_HARD_IGN;
}


#ifdef mkinit
INCLUDE <signal.h>
INCLUDE "trap.h"

SHELLPROC {
	char *sm;

	clear_traps();
	for (sm = sigmode ; sm < sigmode + NSIG - 1; sm++) {
		if (*sm == S_IGN)
			*sm = S_HARD_IGN;
	}
}
#endif



/*
 * Signal handler.
 */

static void
onsig(signo)
	int signo;
{
	if (signo == SIGINT && trap[SIGINT] == NULL) {
		onint();
		return;
	}
	gotsig[signo - 1] = 1;
	pendingsigs++;
}



/*
 * Called to execute a trap.  Perhaps we should avoid entering new trap
 * handlers while we are executing a trap handler.
 */

static void
dotrap() {
	int i;
	int savestatus;

	for (;;) {
		for (i = 1 ; ; i++) {
			if (gotsig[i - 1])
				break;
			if (i >= NSIG - 1)
				goto done;
		}
		gotsig[i - 1] = 0;
		savestatus=exitstatus;
		evalstring(trap[i], 0);
		exitstatus=savestatus;
	}
done:
	pendingsigs = 0;
}



/*
 * Controls whether the shell is interactive or not.
 */


static void
setinteractive(on)
	int on;
{
	static int is_interactive;

	if (on == is_interactive)
		return;
	setsignal(SIGINT);
	setsignal(SIGQUIT);
	setsignal(SIGTERM);
	chkmail(1);
	is_interactive = on;
}



/*
 * Called to exit the shell.
 */

static void
exitshell(status)
	int status;
{
	struct jmploc loc1, loc2;
	char *p;

	TRACE(("exitshell(%d) pid=%d\n", status, getpid()));
	if (setjmp(loc1.loc)) {
		goto l1;
	}
	if (setjmp(loc2.loc)) {
		goto l2;
	}
	handler = &loc1;
	if ((p = trap[0]) != NULL && *p != '\0') {
		trap[0] = NULL;
		evalstring(p, 0);
	}
l1:   handler = &loc2;			/* probably unnecessary */
	flushall();
#if JOBS
	setjobctl(0);
#endif
l2:   _exit(status);
	/* NOTREACHED */
}

static int decode_signal(const char *string, int minsig)
{
	int signo;

	if (is_number(string)) {
		signo = atoi(string);
		if (signo >= NSIG) {
			return -1;
		}
		return signo;
	}

	signo = minsig;
	if (!signo) {
		goto zero;
	}
	for (; signo < NSIG; signo++) {
		if (!strcasecmp(string, &(signal_names[signo])[3])) {
			return signo;
		}
zero:
		if (!strcasecmp(string, signal_names[signo])) {
			return signo;
		}
	}

	return -1;
}
/*	$NetBSD: var.c,v 1.27 2001/02/04 19:52:07 christos Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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


#define VTABSIZE 39


struct varinit {
	struct var *var;
	int flags;
	const char *text;
	void (*func) __P((const char *));
};

struct localvar *localvars;

#if ATTY
struct var vatty;
#endif
struct var vifs;
struct var vmail;
struct var vmpath;
struct var vpath;
struct var vps1;
struct var vps2;
struct var vvers;
struct var voptind;

static const char defpathvar[] =
	"PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
#ifdef IFS_BROKEN
static const char defifsvar[] = "IFS= \t\n";
#else
static const char defifs[] = " \t\n";
#endif

static const struct varinit varinit[] = {
#if ATTY
	{ &vatty,	VSTRFIXED|VTEXTFIXED|VUNSET,	"ATTY=",
	  NULL },
#endif
#ifdef IFS_BROKEN
	{ &vifs,	VSTRFIXED|VTEXTFIXED,		defifsvar,
#else
	{ &vifs,	VSTRFIXED|VTEXTFIXED|VUNSET,	"IFS=",
#endif
	  NULL },
	{ &vmail,	VSTRFIXED|VTEXTFIXED|VUNSET,	"MAIL=",
	  NULL },
	{ &vmpath,	VSTRFIXED|VTEXTFIXED|VUNSET,	"MAILPATH=",
	  NULL },
	{ &vpath,	VSTRFIXED|VTEXTFIXED,		defpathvar,
	  changepath },
	/*
	 * vps1 depends on uid
	 */
	{ &vps2,	VSTRFIXED|VTEXTFIXED,		"PS2=> ",
	  NULL },
	{ &voptind,	VSTRFIXED|VTEXTFIXED,		"OPTIND=1",
	  getoptsreset },
	{ NULL,	0,				NULL,
	  NULL }
};

struct var *vartab[VTABSIZE];

static struct var **hashvar __P((const char *));
static void showvars __P((const char *, int, int));
static struct var **findvar __P((struct var **, const char *));

/*
 * Initialize the varable symbol tables and import the environment
 */

#ifdef mkinit
INCLUDE <unistd.h>
INCLUDE "output.h"
INCLUDE "var.h"
static char **environ;
INIT {
	char **envp;
	char ppid[32];

	initvar();
	for (envp = environ ; *envp ; envp++) {
		if (strchr(*envp, '=')) {
			setvareq(*envp, VEXPORT|VTEXTFIXED);
		}
	}

	fmtstr(ppid, sizeof(ppid), "%d", (int) getppid());
	setvar("PPID", ppid, 0);
}
#endif


/*
 * This routine initializes the builtin variables.  It is called when the
 * shell is initialized and again when a shell procedure is spawned.
 */

static void
initvar() {
	const struct varinit *ip;
	struct var *vp;
	struct var **vpp;

	for (ip = varinit ; (vp = ip->var) != NULL ; ip++) {
		if ((vp->flags & VEXPORT) == 0) {
			vpp = hashvar(ip->text);
			vp->next = *vpp;
			*vpp = vp;
			vp->text = strdup(ip->text);
			vp->flags = ip->flags;
			vp->func = ip->func;
		}
	}
	/*
	 * PS1 depends on uid
	 */
	if ((vps1.flags & VEXPORT) == 0) {
		vpp = hashvar("PS1=");
		vps1.next = *vpp;
		*vpp = &vps1;
		vps1.text = strdup(geteuid() ? "PS1=$ " : "PS1=# ");
		vps1.flags = VSTRFIXED|VTEXTFIXED;
	}
}

/*
 * Set the value of a variable.  The flags argument is ored with the
 * flags of the variable.  If val is NULL, the variable is unset.
 */

static void
setvar(name, val, flags)
	const char *name, *val;
	int flags;
{
	const char *p;
	int len;
	int namelen;
	char *nameeq;
	int isbad;
	int vallen = 0;

	isbad = 0;
	p = name;
	if (! is_name(*p))
		isbad = 1;
	p++;
	for (;;) {
		if (! is_in_name(*p)) {
			if (*p == '\0' || *p == '=')
				break;
			isbad = 1;
		}
		p++;
	}
	namelen = p - name;
	if (isbad)
		error("%.*s: bad variable name", namelen, name);
	len = namelen + 2;		/* 2 is space for '=' and '\0' */
	if (val == NULL) {
		flags |= VUNSET;
	} else {
		len += vallen = strlen(val);
	}
	INTOFF;
	nameeq = ckmalloc(len);
	memcpy(nameeq, name, namelen);
	nameeq[namelen] = '=';
	if (val) {
		memcpy(nameeq + namelen + 1, val, vallen + 1);
	} else {
		nameeq[namelen + 1] = '\0';
	}
	setvareq(nameeq, flags);
	INTON;
}



/*
 * Same as setvar except that the variable and value are passed in
 * the first argument as name=value.  Since the first argument will
 * be actually stored in the table, it should not be a string that
 * will go away.
 */

static void
setvareq(s, flags)
	char *s;
	int flags;
{
	struct var *vp, **vpp;

	vpp = hashvar(s);
	flags |= (VEXPORT & (((unsigned) (1 - aflag)) - 1));
	if ((vp = *findvar(vpp, s))) {
		if (vp->flags & VREADONLY) {
			size_t len = strchr(s, '=') - s;
			error("%.*s: is read only", len, s);
		}
		INTOFF;

		if (vp->func && (flags & VNOFUNC) == 0)
			(*vp->func)(strchr(s, '=') + 1);

		if ((vp->flags & (VTEXTFIXED|VSTACK)) == 0)
			ckfree(vp->text);

		vp->flags &= ~(VTEXTFIXED|VSTACK|VUNSET);
		vp->flags |= flags;
		vp->text = s;

		/*
		 * We could roll this to a function, to handle it as
		 * a regular variable function callback, but why bother?
		 */
		if (iflag && (vp == &vmpath || (vp == &vmail && !mpathset())))
			chkmail(1);
		INTON;
		return;
	}
	/* not found */
	vp = ckmalloc(sizeof (*vp));
	vp->flags = flags;
	vp->text = s;
	vp->next = *vpp;
	vp->func = NULL;
	*vpp = vp;
}



/*
 * Process a linked list of variable assignments.
 */

static void
listsetvar(mylist)
	struct strlist *mylist;
	{
	struct strlist *lp;

	INTOFF;
	for (lp = mylist ; lp ; lp = lp->next) {
		setvareq(savestr(lp->text), 0);
	}
	INTON;
}



/*
 * Find the value of a variable.  Returns NULL if not set.
 */

static char *
lookupvar(name)
	const char *name;
	{
	struct var *v;

	if ((v = *findvar(hashvar(name), name)) && !(v->flags & VUNSET)) {
		return strchr(v->text, '=') + 1;
	}
	return NULL;
}



/*
 * Search the environment of a builtin command.
 */

static char *
bltinlookup(name)
	const char *name;
{
	struct strlist *sp;

	for (sp = cmdenviron ; sp ; sp = sp->next) {
		if (varequal(sp->text, name))
			return strchr(sp->text, '=') + 1;
	}
	return lookupvar(name);
}



/*
 * Generate a list of exported variables.  This routine is used to construct
 * the third argument to execve when executing a program.
 */

static char **
environment() {
	int nenv;
	struct var **vpp;
	struct var *vp;
	char **env;
	char **ep;

	nenv = 0;
	for (vpp = vartab ; vpp < vartab + VTABSIZE ; vpp++) {
		for (vp = *vpp ; vp ; vp = vp->next)
			if (vp->flags & VEXPORT)
				nenv++;
	}
	ep = env = stalloc((nenv + 1) * sizeof *env);
	for (vpp = vartab ; vpp < vartab + VTABSIZE ; vpp++) {
		for (vp = *vpp ; vp ; vp = vp->next)
			if (vp->flags & VEXPORT)
				*ep++ = vp->text;
	}
	*ep = NULL;
	return env;
}


/*
 * Called when a shell procedure is invoked to clear out nonexported
 * variables.  It is also necessary to reallocate variables of with
 * VSTACK set since these are currently allocated on the stack.
 */

#ifdef mkinit
static void shprocvar __P((void));

SHELLPROC {
	shprocvar();
}
#endif

static void
shprocvar() {
	struct var **vpp;
	struct var *vp, **prev;

	for (vpp = vartab ; vpp < vartab + VTABSIZE ; vpp++) {
		for (prev = vpp ; (vp = *prev) != NULL ; ) {
			if ((vp->flags & VEXPORT) == 0) {
				*prev = vp->next;
				if ((vp->flags & VTEXTFIXED) == 0)
					ckfree(vp->text);
				if ((vp->flags & VSTRFIXED) == 0)
					ckfree(vp);
			} else {
				if (vp->flags & VSTACK) {
					vp->text = savestr(vp->text);
					vp->flags &=~ VSTACK;
				}
				prev = &vp->next;
			}
		}
	}
	initvar();
}



/*
 * Command to list all variables which are set.  Currently this command
 * is invoked from the set command when the set command is called without
 * any variables.
 */

static int
showvarscmd(argc, argv)
	int argc;
	char **argv;
{
	showvars(nullstr, VUNSET, VUNSET);
	return 0;
}



/*
 * The export and readonly commands.
 */

static int
exportcmd(argc, argv)
	int argc;
	char **argv;
{
	struct var *vp;
	char *name;
	const char *p;
	int flag = argv[0][0] == 'r'? VREADONLY : VEXPORT;
	int pflag;

	listsetvar(cmdenviron);
	pflag = (nextopt("p") == 'p');
	if (argc > 1 && !pflag) {
		while ((name = *argptr++) != NULL) {
			if ((p = strchr(name, '=')) != NULL) {
				p++;
			} else {
				if ((vp = *findvar(hashvar(name), name))) {
					vp->flags |= flag;
					goto found;
				}
			}
			setvar(name, p, flag);
found:;
		}
	} else {
		showvars(argv[0], flag, 0);
	}
	return 0;
}


/*
 * The "local" command.
 */

static int
localcmd(argc, argv)
	int argc;
	char **argv;
{
	char *name;

	if (! in_function())
		error("Not in a function");
	while ((name = *argptr++) != NULL) {
		mklocal(name);
	}
	return 0;
}


/*
 * Make a variable a local variable.  When a variable is made local, it's
 * value and flags are saved in a localvar structure.  The saved values
 * will be restored when the shell function returns.  We handle the name
 * "-" as a special case.
 */

static void
mklocal(name)
	char *name;
	{
	struct localvar *lvp;
	struct var **vpp;
	struct var *vp;

	INTOFF;
	lvp = ckmalloc(sizeof (struct localvar));
	if (name[0] == '-' && name[1] == '\0') {
		char *p;
		p = ckmalloc(sizeof optlist);
		lvp->text = memcpy(p, optlist, sizeof optlist);
		vp = NULL;
	} else {
		vpp = hashvar(name);
		vp = *findvar(vpp, name);
		if (vp == NULL) {
			if (strchr(name, '='))
				setvareq(savestr(name), VSTRFIXED);
			else
				setvar(name, NULL, VSTRFIXED);
			vp = *vpp;	/* the new variable */
			lvp->text = NULL;
			lvp->flags = VUNSET;
		} else {
			lvp->text = vp->text;
			lvp->flags = vp->flags;
			vp->flags |= VSTRFIXED|VTEXTFIXED;
			if (strchr(name, '='))
				setvareq(savestr(name), 0);
		}
	}
	lvp->vp = vp;
	lvp->next = localvars;
	localvars = lvp;
	INTON;
}


/*
 * Called after a function returns.
 */

static void
poplocalvars() {
	struct localvar *lvp;
	struct var *vp;

	while ((lvp = localvars) != NULL) {
		localvars = lvp->next;
		vp = lvp->vp;
		if (vp == NULL) {	/* $- saved */
			memcpy(optlist, lvp->text, sizeof optlist);
			ckfree(lvp->text);
		} else if ((lvp->flags & (VUNSET|VSTRFIXED)) == VUNSET) {
			(void)unsetvar(vp->text);
		} else {
			if ((vp->flags & VTEXTFIXED) == 0)
				ckfree(vp->text);
			vp->flags = lvp->flags;
			vp->text = lvp->text;
		}
		ckfree(lvp);
	}
}


static int
setvarcmd(argc, argv)
	int argc;
	char **argv;
{
	if (argc <= 2)
		return unsetcmd(argc, argv);
	else if (argc == 3)
		setvar(argv[1], argv[2], 0);
	else
		error("List assignment not implemented");
	return 0;
}


/*
 * The unset builtin command.  We unset the function before we unset the
 * variable to allow a function to be unset when there is a readonly variable
 * with the same name.
 */

static int
unsetcmd(argc, argv)
	int argc;
	char **argv;
{
	char **ap;
	int i;
	int flg_func = 0;
	int flg_var = 0;
	int ret = 0;

	while ((i = nextopt("vf")) != '\0') {
		if (i == 'f')
			flg_func = 1;
		else
			flg_var = 1;
	}
	if (flg_func == 0 && flg_var == 0)
		flg_var = 1;

	for (ap = argptr; *ap ; ap++) {
		if (flg_func)
			unsetfunc(*ap);
		if (flg_var)
			ret |= unsetvar(*ap);
	}
	return ret;
}


/*
 * Unset the specified variable.
 */

static int
unsetvar(s)
	const char *s;
	{
	struct var **vpp;
	struct var *vp;

	vpp = findvar(hashvar(s), s);
	vp = *vpp;
	if (vp) {
		if (vp->flags & VREADONLY)
			return (1);
		INTOFF;
		if (*(strchr(vp->text, '=') + 1) != '\0')
			setvar(s, nullstr, 0);
		vp->flags &= ~VEXPORT;
		vp->flags |= VUNSET;
		if ((vp->flags & VSTRFIXED) == 0) {
			if ((vp->flags & VTEXTFIXED) == 0)
				ckfree(vp->text);
			*vpp = vp->next;
			ckfree(vp);
		}
		INTON;
		return (0);
	}

	return (0);
}



/*
 * Find the appropriate entry in the hash table from the name.
 */

static struct var **
hashvar(p)
	const char *p;
	{
	unsigned int hashval;

	hashval = ((unsigned char) *p) << 4;
	while (*p && *p != '=')
		hashval += (unsigned char) *p++;
	return &vartab[hashval % VTABSIZE];
}



/*
 * Returns true if the two strings specify the same varable.  The first
 * variable name is terminated by '='; the second may be terminated by
 * either '=' or '\0'.
 */

static int
varequal(p, q)
	const char *p, *q;
	{
	while (*p == *q++) {
		if (*p++ == '=')
			return 1;
	}
	if (*p == '=' && *(q - 1) == '\0')
		return 1;
	return 0;
}

static void
showvars(const char *myprefix, int mask, int xor)
{
	struct var **vpp;
	struct var *vp;
	const char *sep = myprefix == nullstr ? myprefix : spcstr;

	for (vpp = vartab ; vpp < vartab + VTABSIZE ; vpp++) {
		for (vp = *vpp ; vp ; vp = vp->next) {
			if ((vp->flags & mask) ^ xor) {
				char *p;
				int len;

				p = strchr(vp->text, '=') + 1;
				len = p - vp->text;
				p = single_quote(p);

				out1fmt(
					"%s%s%.*s%s\n", myprefix, sep, len,
					vp->text, p
				);
				stunalloc(p);
			}
		}
	}
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
 * Copyright (c) 1999 Herbert Xu <herbert@debian.org>
 * This file contains code for the times builtin.
 * $Id: ash.c,v 1.1 2001/06/28 07:25:15 andersen Exp $
 */
static int timescmd (int argc, char **argv)
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

