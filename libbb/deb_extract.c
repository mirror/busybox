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
#include <string.h>
#include "libbb.h"

int seek_sub_file(FILE *in_file, file_headers_t *headers, const char *tar_gz_file)
{
	/* find the headers for the specified .tar.gz file */
	while (headers != NULL) {
		if (strcmp(headers->name, tar_gz_file) == 0) {
			fseek(in_file, headers->offset, SEEK_SET);
			return(EXIT_SUCCESS);
		}
		headers = headers->next;
	}

	return(EXIT_FAILURE);
}

/*
 * The contents of argument depend on the value of function.
 * It is either a dir name or a control file or field name(see dpkg_deb.c)
 */
char *deb_extract(const char *package_filename, FILE *out_stream, const int extract_function,
	const char *prefix, const char *filename)
{
	FILE *deb_stream, *uncompressed_stream;
	file_headers_t *ar_headers = NULL;
	file_headers_t *tar_headers = NULL;
	file_headers_t *list_ptr;
	file_headers_t *deb_extract_list = (file_headers_t *) calloc(1, sizeof(file_headers_t));

	char *ared_file = NULL;
	char *output_buffer = NULL;
	int gunzip_pid;

	if (extract_function & extract_control_tar_gz) {
		ared_file = xstrdup("control.tar.gz");
	}
	else if (extract_function & extract_data_tar_gz) {		
		ared_file = xstrdup("data.tar.gz");
	}

	/* open the debian package to be worked on */
	deb_stream = wfopen(package_filename, "r");

	ar_headers = (file_headers_t *) xmalloc(sizeof(file_headers_t));	
	
	/* get a linked list of all ar entries */
	ar_headers = get_ar_headers(deb_stream);
	if (ar_headers == NULL) {
		error_msg("Couldnt get ar headers\n");
		return(NULL);
	}

	/* seek to the start of the .tar.gz file within the ar file*/
	fseek(deb_stream, 0, SEEK_SET);
	if (seek_sub_file(deb_stream, ar_headers, ared_file) == EXIT_FAILURE) {
		error_msg("Couldnt seek to file %s", ared_file);
	}

	/* get a linked list of all tar entries */
	tar_headers = get_tar_gz_headers(deb_stream);
	if (tar_headers == NULL) {
		error_msg("Couldnt get tar headers\n");
		return(NULL);
	}

	if (extract_function & extract_one_to_buffer) {
		list_ptr = tar_headers;
		while (list_ptr != NULL) {
			if (strcmp(filename, list_ptr->name) == 0) {
				deb_extract_list = append_archive_list(deb_extract_list, list_ptr);
				break;
			}
			list_ptr = list_ptr->next;
		}
	} else {
		deb_extract_list = tar_headers;
	}

	/* seek to the start of the .tar.gz file within the ar file*/
	if (seek_sub_file(deb_stream, ar_headers, ared_file) == EXIT_FAILURE) {
		error_msg("Couldnt seek to ar file");
	}

	/* open a stream of decompressed data */
	uncompressed_stream = fdopen(gz_open(deb_stream, &gunzip_pid), "r");

	output_buffer = extract_archive(uncompressed_stream, out_stream, deb_extract_list, extract_function, prefix);

	gz_close(gunzip_pid);
	fclose(deb_stream);
	fclose(uncompressed_stream);
	free(ared_file);

	return(output_buffer);
}