/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "unarchive.h"

/* transformer(), more than meets the eye */
/*
 * On MMU machine, the transform_prog is removed by macro magic
 * in include/unarchive.h. On NOMMU, transformer is removed.
 */
int open_transformer(int src_fd,
	USE_DESKTOP(long long) int (*transformer)(int src_fd, int dst_fd),
	const char *transform_prog)
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
		{
			char *argv[4];
			xmove_fd(src_fd, 0);
			xmove_fd(fd_pipe[1], 1);
			argv[0] = (char*)transform_prog;
			argv[1] = (char*)"-cf";
			argv[2] = (char*)"-";
			argv[3] = NULL;
			BB_EXECVP(transform_prog, argv);
			bb_perror_msg_and_die("exec failed");
		}
#endif
		/* notreached */
	}

	/* parent process */
	close(fd_pipe[1]); /* Don't want to write to the child */

	return fd_pipe[0];
}
