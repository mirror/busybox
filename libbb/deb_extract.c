/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) tons of folks.  Tracking down who wrote what
 * isn't something I'm going to worry about...  If you wrote something
 * here, please feel free to acknowledge your work.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Based in part on code from sash, Copyright (c) 1999 by David I. Bell 
 * Permission has been granted to redistribute this code under the GPL.
 *
 */

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include "libbb.h"

extern int deb_extract(const char *package_filename, int function, char *target_dir)
{

	FILE *deb_file, *uncompressed_file;
	ar_headers_t *headers = NULL;
	char *ared_file = NULL;
	int gunzip_pid;

	switch (function) {
		case (extract_info):
		case (extract_control):
			ared_file = xstrdup("control.tar.gz");
			break;
		case (extract_contents):
		case (extract_extract):
		case (extract_verbose_extract):
		case (extract_list):
			ared_file = xstrdup("data.tar.gz");
			break;
		default:
			error_msg("Unknown extraction function");
	}

	/* open the debian package to be worked on */
	deb_file = wfopen(package_filename, "r");

	headers = (ar_headers_t *) xmalloc(sizeof(ar_headers_t));	
	
	/* get a linked list of all ar entries */
	if ((headers = get_ar_headers(deb_file)) == NULL) {
		error_msg("Couldnt get ar headers\n");
		return(EXIT_FAILURE);
	}

	/* seek to the start of the .tar.gz file within the ar file*/
	if (seek_ared_file(deb_file, headers, ared_file) == EXIT_FAILURE) {
		error_msg("Couldnt seek to ar file");
	}

	/* open a stream of decompressed data */
	uncompressed_file = fdopen(gz_open(deb_file, &gunzip_pid), "r");

	/* get a list of all tar headers inside the .gz file */
	untar(uncompressed_file, function, target_dir);

	/* we are deliberately terminating the child so we can safely ignore this */
	gz_close(gunzip_pid);

	fclose(deb_file);
	fclose(uncompressed_file);
	free(ared_file);

	return(EXIT_SUCCESS);
}