/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "unarchive.h"

/* transformer(), more than meets the eye */
/*
 * On MMU machine, the transform_prog and ... are stripped
 * by a macro in include/unarchive.h. On NOMMU, transformer is stripped.
 */
int open_transformer(int src_fd,
	USE_DESKTOP(long long) int (*transformer)(int src_fd, int dst_fd),
	const char *transform_prog, ...)
{
	int fd_pipe[2];
	int pid;

	xpipe(fd_pipe);

#if BB_MMU
	pid = fork();
#else
	pid = vfork();
#endif
	if (pid == -1)
		bb_perror_msg_and_die("fork failed");

	if (pid == 0) {
#if !BB_MMU
		va_list ap;
#endif
		/* child process */
		close(fd_pipe[0]); /* We don't wan't to read from the parent */
		// FIXME: error check?
#if BB_MMU
		transformer(src_fd, fd_pipe[1]);
		if (ENABLE_FEATURE_CLEAN_UP) {
			close(fd_pipe[1]); /* Send EOF */
			close(src_fd);
		}
		exit(0);
#else
		xmove_fd(src_fd, 0);
		xmove_fd(fd_pipe[1], 1);
		va_start(ap, transform_prog);
		BB_EXECVP(transform_prog, ap);
		bb_perror_and_die("exec failed");
#endif
		/* notreached */
	}

	/* parent process */
	close(fd_pipe[1]); /* Don't want to write to the child */

	return fd_pipe[0];
}
