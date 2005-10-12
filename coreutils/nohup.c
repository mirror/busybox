/* vi: set sw=4 ts=4: */
/* nohup -- run a command immune to hangups, with output to a non-tty
   Copyright (C) 2003, 2004, 2005 Free Software Foundation, Inc.

   Licensed under the GPL v2, see the file LICENSE in this tarball.

 */

/* Written by Jim Meyering  */
/* initial busybox port by Bernhard Fischer */

#include <stdio_ext.h> /* __fpending */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

#include "busybox.h"
#define EXIT_CANNOT_INVOKE (126)
#define NOHUP_FAILURE (127)
#define EXIT_ENOENT NOHUP_FAILURE



#if defined F_GETFD && defined F_SETFD
static inline int set_cloexec_flag (int desc)
{
	int flags = fcntl (desc, F_GETFD, 0);
	if (0 <= flags) {
		if (flags == (flags |= FD_CLOEXEC) ||
			fcntl (desc, F_SETFD, flags) != -1) {
			return 0;
		}
	}
	return -1;
}
#else
#define set_cloexec_flag(desc) (0)
#endif

static int fd_reopen (int desired_fd, char const *file, int flags)
{
	int fd;

	close (desired_fd);
	fd = open (file, flags | O_WRONLY, S_IRUSR | S_IWUSR);
	if (fd == desired_fd || fd < 0)
		return fd;
	else {
		int fd2 = fcntl (fd, F_DUPFD, desired_fd);
		int saved_errno = errno;
		close (fd);
		errno = saved_errno;
		return fd2;
	}
}


/* Close standard output, exiting with status 'exit_failure' on failure.
   If a program writes *anything* to stdout, that program should close
   stdout and make sure that it succeeds before exiting.  Otherwise,
   suppose that you go to the extreme of checking the return status
   of every function that does an explicit write to stdout.  The last
   printf can succeed in writing to the internal stream buffer, and yet
   the fclose(stdout) could still fail (due e.g., to a disk full error)
   when it tries to write out that buffered data.  Thus, you would be
   left with an incomplete output file and the offending program would
   exit successfully.  Even calling fflush is not always sufficient,
   since some file systems (NFS and CODA) buffer written/flushed data
   until an actual close call.

   Besides, it's wasteful to check the return value from every call
   that writes to stdout -- just let the internal stream state record
   the failure.  That's what the ferror test is checking below.

   It's important to detect such failures and exit nonzero because many
   tools (most notably `make' and other build-management systems) depend
   on being able to detect failure in other tools via their exit status.  */

static void close_stdout (void)
{
	int prev_fail = ferror (stdout);
	int none_pending = (0 == __fpending (stdout));
	int fclose_fail = fclose (stdout);

	if (prev_fail || fclose_fail) {
		/* If ferror returned zero, no data remains to be flushed, and we'd
		otherwise fail with EBADF due to a failed fclose, then assume that
		it's ok to ignore the fclose failure.  That can happen when a
		program like cp is invoked like this `cp a b >&-' (i.e., with
		stdout closed) and doesn't generate any output (hence no previous
		error and nothing to be flushed).  */
		if ((fclose_fail ? errno : 0) == EBADF && !prev_fail && none_pending)
			return;

		bb_perror_msg_and_die(bb_msg_write_error);
	}
}


int nohup_main (int argc, char **argv)
{
	int saved_stderr_fd;

	if (argc < 2)
		bb_show_usage();

	bb_default_error_retval = NOHUP_FAILURE;

	atexit (close_stdout);

	/* If standard input is a tty, replace it with /dev/null.
	 Note that it is deliberately opened for *writing*,
	 to ensure any read evokes an error.  */
	if (isatty (STDIN_FILENO))
		fd_reopen (STDIN_FILENO, bb_dev_null, 0);

	/* If standard output is a tty, redirect it (appending) to a file.
	 First try nohup.out, then $HOME/nohup.out.  */
	if (isatty (STDOUT_FILENO)) {
		char *in_home = NULL;
		char const *file = "nohup.out";
		int fd = fd_reopen (STDOUT_FILENO, file, O_CREAT | O_APPEND);

		if (fd < 0) {
			if ((in_home = getenv ("HOME")) != NULL) {
				in_home = concat_path_file(in_home, file);
				fd = fd_reopen (STDOUT_FILENO, in_home, O_CREAT | O_APPEND);
			}
			if (fd < 0) {
				bb_perror_msg("failed to open '%s'", file);
				if (in_home)
					bb_perror_msg("failed to open '%s'",in_home);
				return (NOHUP_FAILURE);
			}
			file = in_home;
		}

		umask (~(S_IRUSR | S_IWUSR));
		bb_error_msg("appending output to '%s'", file);
		if (ENABLE_FEATURE_CLEAN_UP)
			free (in_home);
	}

	/* If standard error is a tty, redirect it to stdout.  */
	if (isatty (STDERR_FILENO)) {
	/* Save a copy of stderr before redirecting, so we can use the original
	 if execve fails.  It's no big deal if this dup fails.  It might
	 not change anything, and at worst, it'll lead to suppression of
	 the post-failed-execve diagnostic.  */
		saved_stderr_fd = dup (STDERR_FILENO);

		if (0 <= saved_stderr_fd && set_cloexec_flag (saved_stderr_fd) == -1)
			bb_perror_msg_and_die("failed to set the copy"
					"of stderr to close on exec");

		if (dup2 (STDOUT_FILENO, STDERR_FILENO) < 0) {
			if (errno != EBADF)
				bb_perror_msg_and_die("failed to redirect standard error");
			close (STDERR_FILENO);
		}
	} else
		saved_stderr_fd = STDERR_FILENO;

	signal (SIGHUP, SIG_IGN);

	{
	char **cmd = argv + 1;

	execvp (*cmd, cmd);

	/* The execve failed.  Output a diagnostic to stderr only if:
	   - stderr was initially redirected to a non-tty, or
	   - stderr was initially directed to a tty, and we
	   can dup2 it to point back to that same tty.
	   In other words, output the diagnostic if possible, but only if
	   it will go to the original stderr.  */
	if (dup2 (saved_stderr_fd, STDERR_FILENO) == STDERR_FILENO)
		bb_perror_msg("cannot run command '%s'",*cmd);

	return (errno == ENOENT ? EXIT_ENOENT : EXIT_CANNOT_INVOKE);
	}
}
