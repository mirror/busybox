/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <unistd.h>

#include "libbb.h"

/* transformer(), more than meets the eye */
extern int open_transformer(int src_fd, int (*transformer)(int src_fd, int dst_fd))
{
	int fd_pipe[2];
	int pid;

	if (pipe(fd_pipe) != 0) {
		bb_perror_msg_and_die("Can't create pipe");
	}

	pid = fork();
	if (pid == -1) {
		bb_perror_msg_and_die("Fork failed");
	}

	if (pid == 0) {
		/* child process */
	    close(fd_pipe[0]); /* We don't wan't to read from the parent */
	    transformer(src_fd, fd_pipe[1]);
	    close(fd_pipe[1]); /* Send EOF */
		close(src_fd);
	    exit(0);
	    /* notreached */
	}

	/* parent process */
	close(fd_pipe[1]); /* Don't want to write to the child */

	return(fd_pipe[0]);
}
