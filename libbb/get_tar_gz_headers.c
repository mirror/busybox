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
#include <stdio.h>
#include <unistd.h>
#include "libbb.h"

file_headers_t *get_tar_gz_headers(FILE *compressed_stream)
{
	FILE *uncompressed_stream;
	file_headers_t *tar_headers_tree;
	int gunzip_pid;
	int gz_fd ;

	/* open a stream of decompressed data */
	gz_fd = gz_open(compressed_stream, &gunzip_pid);
	if (gz_fd == -1) {
		error_msg("Couldnt initialise gzip stream");
	}
	uncompressed_stream = fdopen(gz_fd, "r");
	tar_headers_tree = get_tar_headers(uncompressed_stream);
	fclose(uncompressed_stream);
	close(gz_fd);
	gz_close(gunzip_pid);

	return(tar_headers_tree);
}