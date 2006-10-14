/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <stdlib.h>
#include <unistd.h>

#include "libbb.h"

#include "unarchive.h"

/* transformer(), more than meets the eye */
int open_transformer(int src_fd,
	USE_DESKTOP(long long) int (*transformer)(int src_fd, int dst_fd))
{
	int fd_pipe[2];
	int pid;

	if (pipe(fd_pipe) != 0) {
		bb_perror_msg_and_die("can't create pipe");
	}

	pid = fork();
	if (pid == -1) {
		bb_perror_msg_and_die("fork failed");
	}

	if (pid == 0) {
		/* child process */
		close(fd_pipe[0]); /* We don't wan't to read from the parent */
		// FIXME: error check?
		transformer(src_fd, fd_pipe[1]);
		close(fd_pipe[1]); /* Send EOF */
		close(src_fd);
		exit(0);
		/* notreached */
	}

	/* parent process */
	close(fd_pipe[1]); /* Don't want to write to the child */

	return fd_pipe[0];
}
