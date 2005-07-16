/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) many different people.  If you wrote this, please
 * acknowledge your work.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <stdio.h>
#include <sys/stat.h>
#include "libbb.h"

/* Copy CHUNKSIZE bytes (or until EOF if CHUNKSIZE equals -1) from SRC_FILE
 * to DST_FILE.  */
extern int copy_file_chunk(FILE *src_file, FILE *dst_file, unsigned long long chunksize)
{
	size_t nread, nwritten, size;
	char buffer[BUFSIZ];

	while (chunksize != 0) {
		if (chunksize > BUFSIZ)
			size = BUFSIZ;
		else
			size = chunksize;

		nread = fread (buffer, 1, size, src_file);

		if (nread != size && ferror (src_file)) {
			perror_msg ("read");
			return -1;
		} else if (nread == 0) {
			if (chunksize != -1) {
				error_msg ("Unable to read all data");
				return -1;
			}

			return 0;
		}

		nwritten = fwrite (buffer, 1, nread, dst_file);

		if (nwritten != nread) {
			if (ferror (dst_file))
				perror_msg ("write");
			else
				error_msg ("Unable to write all data");
			return -1;
		}

		if (chunksize != -1)
			chunksize -= nwritten;
	}

	return 0;
}
