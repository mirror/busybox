/* vi: set sw=4 ts=4: */
/*
 * Rexec program for system have fork() as vfork() with foreground option
 *
 * Copyright (C) Vladimir N. Oleynik <dzo@simtreas.ru>
 * Copyright (C) 2003 Russ Dill <Russ.Dill@asu.edu>
 *
 * daemon() portion taken from uClibc:
 *
 * Copyright (c) 1991, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Modified for uClibc by Erik Andersen <andersee@debian.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <paths.h>
#include "busybox.h" /* uses applet tables */

/* This does a fork/exec in one call, using vfork().  Returns PID of new child,
 * -1 for failure.  Runs argv[0], searching path if that has no / in it. */
pid_t spawn(char **argv)
{
	/* Compiler should not optimize stores here */
	volatile int failed;
	pid_t pid;

// Ain't it a good place to fflush(NULL)?

	/* Be nice to nommu machines. */
	failed = 0;
	pid = vfork();
	if (pid < 0) /* error */
		return pid;
	if (!pid) { /* child */
		/* This macro is ok - it doesn't do NOEXEC/NOFORK tricks */
		BB_EXECVP(argv[0], argv);

		/* We are (maybe) sharing a stack with blocked parent,
		 * let parent know we failed and then exit to unblock parent
		 * (but don't run atexit() stuff, which would screw up parent.)
		 */
		failed = errno;
		_exit(111);
	}
	/* parent */
	/* Unfortunately, this is not reliable: according to standards
	 * vfork() can be equivalent to fork() and we won't see value
	 * of 'failed'.
	 * Interested party can wait on pid and learn exit code.
	 * If 111 - then it (most probably) failed to exec */
	if (failed) {
		errno = failed;
		return -1;
	}
	return pid;
}

/* Die with an error message if we can't spawn a child process. */
pid_t xspawn(char **argv)
{
	pid_t pid = spawn(argv);
	if (pid < 0)
		bb_simple_perror_msg_and_die(*argv);
	return pid;
}

int safe_waitpid(int pid, int *wstat, int options)
{
	int r;

	do
		r = waitpid(pid, wstat, options);
	while ((r == -1) && (errno == EINTR));
	return r;
}

int wait_any_nohang(int *wstat)
{
	return safe_waitpid(-1, wstat, WNOHANG);
}

// Wait for the specified child PID to exit, returning child's error return.
int wait4pid(int pid)
{
	int status;

	if (pid <= 0) {
		/*errno = ECHILD; -- wrong. */
		/* we expect errno to be already set from failed [v]fork/exec */
		return -1;
	}
	if (safe_waitpid(pid, &status, 0) == -1)
		return -1;
	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	if (WIFSIGNALED(status))
		return WTERMSIG(status) + 1000;
	return 0;
}

#if ENABLE_FEATURE_PREFER_APPLETS
void save_nofork_data(struct nofork_save_area *save)
{
	memcpy(&save->die_jmp, &die_jmp, sizeof(die_jmp));
	save->applet_name = applet_name;
	save->xfunc_error_retval = xfunc_error_retval;
	save->option_mask32 = option_mask32;
	save->die_sleep = die_sleep;
	save->saved = 1;
}

void restore_nofork_data(struct nofork_save_area *save)
{
	memcpy(&die_jmp, &save->die_jmp, sizeof(die_jmp));
	applet_name = save->applet_name;
	xfunc_error_retval = save->xfunc_error_retval;
	option_mask32 = save->option_mask32;
	die_sleep = save->die_sleep;
}

int run_nofork_applet_prime(struct nofork_save_area *old, int applet_no, char **argv)
{
	int rc, argc;

	applet_name = APPLET_NAME(applet_no);
	xfunc_error_retval = EXIT_FAILURE;

	/* Special flag for xfunc_die(). If xfunc will "die"
	 * in NOFORK applet, xfunc_die() sees negative
	 * die_sleep and longjmp here instead. */
	die_sleep = -1;

	/* option_mask32 = 0; - not needed */

	argc = 1;
	while (argv[argc])
		argc++;

	rc = setjmp(die_jmp);
	if (!rc) {
		/* Some callers (xargs)
		 * need argv untouched because they free argv[i]! */
		char *tmp_argv[argc+1];
		memcpy(tmp_argv, argv, (argc+1) * sizeof(tmp_argv[0]));
		/* Finally we can call NOFORK applet's main() */
		rc = applet_main[applet_no](argc, tmp_argv);

	/* The whole reason behind nofork_save_area is that <applet>_main
	 * may exit non-locally! For example, in hush Ctrl-Z tries
	 * (modulo bugs) to dynamically create a child (backgrounded task)
	 * if it detects that Ctrl-Z was pressed when a NOFORK was running.
	 * Testcase: interactive "rm -i".
	 * Don't fool yourself into thinking "and <applet>_main() returns
	 * quickly here" and removing "useless" nofork_save_area code. */

	} else { /* xfunc died in NOFORK applet */
		/* in case they meant to return 0... */
		if (rc == -2222)
			rc = 0;
	}

	/* Restoring globals */
	restore_nofork_data(old);
	return rc & 0xff; /* don't confuse people with "exitcodes" >255 */
}

int run_nofork_applet(int applet_no, char **argv)
{
	struct nofork_save_area old;

	/* Saving globals */
	save_nofork_data(&old);
	return run_nofork_applet_prime(&old, applet_no, argv);
}
#endif /* FEATURE_PREFER_APPLETS */

int spawn_and_wait(char **argv)
{
	int rc;
#if ENABLE_FEATURE_PREFER_APPLETS
	int a = find_applet_by_name(argv[0]);

	if (a >= 0 && (APPLET_IS_NOFORK(a)
#if BB_MMU
			|| APPLET_IS_NOEXEC(a) /* NOEXEC trick needs fork() */
#endif
	)) {
#if BB_MMU
		if (APPLET_IS_NOFORK(a))
#endif
		{
			return run_nofork_applet(a, argv);
		}
#if BB_MMU
		/* MMU only */
		/* a->noexec is true */
		rc = fork();
		if (rc) /* parent or error */
			return wait4pid(rc);
		/* child */
		xfunc_error_retval = EXIT_FAILURE;
		run_applet_no_and_exit(a, argv);
#endif
	}
#endif /* FEATURE_PREFER_APPLETS */
	rc = spawn(argv);
	return wait4pid(rc);
}

#if !BB_MMU
void re_exec(char **argv)
{
	/* high-order bit of first char in argv[0] is a hidden
	 * "we have (already) re-execed, don't do it again" flag */
	argv[0][0] |= 0x80;
	execv(bb_busybox_exec_path, argv);
	bb_perror_msg_and_die("exec %s", bb_busybox_exec_path);
}

void forkexit_or_rexec(char **argv)
{
	pid_t pid;
	/* Maybe we are already re-execed and come here again? */
	if (re_execed)
		return;

	pid = vfork();
	if (pid < 0) /* wtf? */
		bb_perror_msg_and_die("vfork");
	if (pid) /* parent */
		exit(EXIT_SUCCESS);
	/* child - re-exec ourself */
	re_exec(argv);
}
#else
/* Dance around (void)...*/
#undef forkexit_or_rexec
void forkexit_or_rexec(void)
{
	pid_t pid;
	pid = fork();
	if (pid < 0) /* wtf? */
		bb_perror_msg_and_die("fork");
	if (pid) /* parent */
		exit(EXIT_SUCCESS);
	/* child */
}
#define forkexit_or_rexec(argv) forkexit_or_rexec()
#endif

/* Due to a #define in libbb.h on MMU systems we actually have 1 argument -
 * char **argv "vanishes" */
void bb_daemonize_or_rexec(int flags, char **argv)
{
	int fd;

	if (flags & DAEMON_CHDIR_ROOT)
		xchdir("/");

	if (flags & DAEMON_DEVNULL_STDIO) {
		close(0);
		close(1);
		close(2);
	}

	fd = xopen(bb_dev_null, O_RDWR);

	while ((unsigned)fd < 2)
		fd = dup(fd); /* have 0,1,2 open at least to /dev/null */

	if (!(flags & DAEMON_ONLY_SANITIZE)) {
		forkexit_or_rexec(argv);
		/* if daemonizing, make sure we detach from stdio & ctty */
		setsid();
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
	}
	while (fd > 2) {
		close(fd--);
		if (!(flags & DAEMON_CLOSE_EXTRA_FDS))
			return;
		/* else close everything after fd#2 */
	}
}

void bb_sanitize_stdio(void)
{
	bb_daemonize_or_rexec(DAEMON_ONLY_SANITIZE, NULL);
}
