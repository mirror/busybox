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

int spawn_and_wait(char **argv)
{
	int rc;

#if ENABLE_FEATURE_EXEC_PREFER_APPLETS
	{
		const struct bb_applet *a = find_applet_by_name(argv[0]);
		if (a && (a->nofork
#ifndef BB_NOMMU
			 || a->noexec /* NOEXEC cannot be used on NOMMU */
#endif
		)) {
			int argc = 1;
			char **pp = argv;
			while (*++pp)
				argc++;
#ifndef BB_NOMMU
			if (a->nofork)
#endif
			{
				int old_sleep = die_sleep;
				int old_x = xfunc_error_retval;
				die_sleep = -1; /* special flag */
				/* xfunc_die() checks for it */

				rc = setjmp(die_jmp);
				if (!rc) {
					const struct bb_applet *old_a = current_applet;
					current_applet = a;
					applet_name = a->name;
// what else should we save/restore?
					rc = a->main(argc, argv);
					current_applet = old_a;
					applet_name = old_a->name;					
				} else {
					/* xfunc died in NOFORK applet */
					if (rc == -111)
						rc = 0;
				}

				die_sleep = old_sleep;
				xfunc_error_retval = old_x;
				return rc;
			}
#ifndef BB_NOMMU	/* MMU only */
			/* a->noexec is true */
			rc = fork();
			if (rc)
				goto w;
			/* child */
			current_applet = a;
			run_current_applet_and_exit(argc, argv);
#endif
		}

	}
	rc = spawn(argv);
 w:
#else /* !FEATURE_EXEC_PREFER_APPLETS */
	rc = spawn(argv);
#endif /* FEATURE_EXEC_PREFER_APPLETS */
	return wait4pid(rc);
}


#if 0 //ndef BB_NOMMU
// Die with an error message if we can't daemonize.
void xdaemon(int nochdir, int noclose)
{
	if (daemon(nochdir, noclose))
		bb_perror_msg_and_die("daemon");
}
#endif

#if 0 // def BB_NOMMU
void vfork_daemon_rexec(int nochdir, int noclose, char **argv)
{
	int fd;

	/* Maybe we are already re-execed and come here again? */
	if (re_execed)
		return;

	setsid();

	if (!nochdir)
		xchdir("/");

	if (!noclose) {
		/* if "/dev/null" doesn't exist, bail out! */
		fd = xopen(bb_dev_null, O_RDWR);
		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		while (fd > 2)
			close(fd--);
	}

	switch (vfork()) {
	case 0: /* child */
		/* Make certain we are not a session leader, or else we
		 * might reacquire a controlling terminal */
		if (vfork())
			_exit(0);
		/* High-order bit of first char in argv[0] is a hidden
		 * "we have (alrealy) re-execed, don't do it again" flag */
		argv[0][0] |= 0x80;
		execv(CONFIG_BUSYBOX_EXEC_PATH, argv);
		bb_perror_msg_and_die("exec %s", CONFIG_BUSYBOX_EXEC_PATH);
	case -1: /* error */
		bb_perror_msg_and_die("vfork");
	default: /* parent */
		exit(0);
	}
}
#endif /* BB_NOMMU */

#ifdef BB_NOMMU
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
	/* high-order bit of first char in argv[0] is a hidden
	 * "we have (alrealy) re-execed, don't do it again" flag */
	argv[0][0] |= 0x80;
	execv(CONFIG_BUSYBOX_EXEC_PATH, argv);
	bb_perror_msg_and_die("exec %s", CONFIG_BUSYBOX_EXEC_PATH);
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

	fd = xopen(bb_dev_null, O_RDWR);

	if (flags & DAEMON_CHDIR_ROOT)
		xchdir("/");

	if (flags & DAEMON_DEVNULL_STDIO) {
		close(0);
		close(1);
		close(2);
	}

	while ((unsigned)fd < 2)
		fd = dup(fd); /* have 0,1,2 open at least to /dev/null */

	if (!(flags & DAEMON_ONLY_SANITIZE)) {
		forkexit_or_rexec(argv);
		/* if daemonizing, make sure we detach from stdio */
		setsid();
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
	}
	if (fd > 2)
		close(fd--);
	if (flags & DAEMON_CLOSE_EXTRA_FDS)
		while (fd > 2)
			close(fd--); /* close everything after fd#2 */
}

void bb_sanitize_stdio(void)
{
	bb_daemonize_or_rexec(DAEMON_ONLY_SANITIZE, NULL);
}
