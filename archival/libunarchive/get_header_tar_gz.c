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

#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libbb.h"
#include "unarchive.h"

extern char get_header_tar_gz(archive_handle_t *archive_handle)
{
	int fd_pipe[2];
	int pid;
	unsigned char magic[2];
	
	xread_all(archive_handle->src_fd, &magic, 2);
	if ((magic[0] != 0x1f) || (magic[1] != 0x8b)) {
		error_msg_and_die("Invalid gzip magic");
	}

	check_header_gzip(archive_handle->src_fd);

	if (pipe(fd_pipe) != 0) {
		error_msg_and_die("Can't create pipe");
	}

	pid = fork();
	if (pid == -1) {
		error_msg_and_die("Fork failed\n");
	}

	if (pid == 0) {
		/* child process */
	    close(fd_pipe[0]); /* We don't wan't to read from the pipe */
	    inflate(archive_handle->src_fd, fd_pipe[1]);
		check_trailer_gzip(archive_handle->src_fd);
	    close(fd_pipe[1]); /* Send EOF */
	    exit(0);
	    /* notreached */
	}
	/* parent process */
	close(fd_pipe[1]); /* Don't want to write down the pipe */
	close(archive_handle->src_fd);

	archive_handle->src_fd = fd_pipe[0];

	archive_handle->offset = 0;
	while (get_header_tar(archive_handle) == EXIT_SUCCESS);

	if (kill(pid, SIGTERM) == -1) {
		error_msg_and_die("Couldnt kill gunzip process");
	}
	
	/* I dont think this is needed */
#if 0
	if (waitpid(pid, NULL, 0) == -1) {
		error_msg("Couldnt wait ?");
	}
#endif

	/* Can only do one file at a time */
	return(EXIT_FAILURE);
}

