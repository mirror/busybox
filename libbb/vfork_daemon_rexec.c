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
#include "busybox.h" /* for struct bb_applet */

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
		bb_perror_msg_and_die("%s", *argv);
	return pid;
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
	if (waitpid(pid, &status, 0) == -1)
		return -1;
	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	if (WIFSIGNALED(status))
		return WTERMSIG(status) + 1000;
	return 0;
}

int wait_nohang(int *wstat)
{
	return waitpid(-1, wstat, WNOHANG);
}

int wait_pid(int *wstat, int pid)
{
	int r;

	do
		r = waitpid(pid, wstat, 0);
	while ((r == -1) && (errno == EINTR));
	return r;
}

#if ENABLE_FEATURE_PREFER_APPLETS
void save_nofork_data(struct nofork_save_area *save)
{
	memcpy(&save->die_jmp, &die_jmp, sizeof(die_jmp));
	save->current_applet = current_applet;
	save->xfunc_error_retval = xfunc_error_retval;
	save->option_mask32 = option_mask32;
	save->die_sleep = die_sleep;
	save->saved = 1;
}

void restore_nofork_data(struct nofork_save_area *save)
{
	memcpy(&die_jmp, &save->die_jmp, sizeof(die_jmp));
	current_applet = save->current_applet;
	xfunc_error_retval = save->xfunc_error_retval;
	option_mask32 = save->option_mask32;
	die_sleep = save->die_sleep;

	applet_name = current_applet->name;
}

int run_nofork_applet_prime(struct nofork_save_area *old, const struct bb_applet *a, char **argv)
{
	int rc, argc;

	current_applet = a;
	applet_name = a->name;
	xfunc_error_retval = EXIT_FAILURE;
	/*option_mask32 = 0; - not needed */
	/* special flag for xfunc_die(). If xfunc will "die"
	 * in NOFORK applet, xfunc_die() sees negative
	 * die_sleep and longjmp here instead. */
	die_sleep = -1;

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
		rc = a->main(argc, tmp_argv);
	} else { /* xfunc died in NOFORK applet */
		/* in case they meant to return 0... */
		if (rc == -2222)
			rc = 0;
	}

	/* Restoring globals */
	restore_nofork_data(old);
	return rc;
}

int run_nofork_applet(const struct bb_applet *a, char **argv)
{
	struct nofork_save_area old;

	/* Saving globals */
	save_nofork_data(&old);
	return run_nofork_applet_prime(&old, a, argv);
}
#endif /* FEATURE_PREFER_APPLETS */

int spawn_and_wait(char **argv)
{
	int rc;
#if ENABLE_FEATURE_PREFER_APPLETS
	const struct bb_applet *a = find_applet_by_name(argv[0]);

	if (a && (a->nofork
#if BB_MMU
		 || a->noexec /* NOEXEC trick needs fork() */
#endif
	)) {
#if BB_MMU
		if (a->nofork)
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
		current_applet = a;
		run_current_applet_and_exit(argv);
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
		exit(0);
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
		exit(0);
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
