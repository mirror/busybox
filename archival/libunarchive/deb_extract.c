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
#include <stdlib.h>
#include <string.h>
#include "unarchive.h"
#include "libbb.h"

char *deb_extract(const char *package_filename, FILE *out_stream, 
	const int extract_function, const char *prefix, const char *filename)
{
	FILE *deb_stream;
	FILE *uncompressed_stream = NULL;
	file_header_t *ar_header = NULL;
	char **file_list = NULL;
	char *output_buffer = NULL;
	char *ared_file = NULL;
	char ar_magic[8];
	int gunzip_pid;

	if (filename != NULL) {
		file_list = xmalloc(sizeof(char *) * 2);
		file_list[0] = xstrdup(filename);
		file_list[1] = NULL;
	}
	
	if (extract_function & extract_control_tar_gz) {
		ared_file = xstrdup("control.tar.gz");
	}
	else if (extract_function & extract_data_tar_gz) {		
		ared_file = xstrdup("data.tar.gz");
	}

	/* open the debian package to be worked on */
	deb_stream = wfopen(package_filename, "r");
	if (deb_stream == NULL) {
		return(NULL);
	}
	/* set the buffer size */
	setvbuf(deb_stream, NULL, _IOFBF, 0x8000);

	/* check ar magic */
	fread(ar_magic, 1, 8, deb_stream);
	if (strncmp(ar_magic,"!<arch>",7) != 0) {
		error_msg_and_die("invalid magic");
	}
	archive_offset = 8;

	while ((ar_header = get_header_ar(deb_stream)) != NULL) {
		if (strcmp(ared_file, ar_header->name) == 0) {
			/* open a stream of decompressed data */
			uncompressed_stream = gz_open(deb_stream, &gunzip_pid);
			archive_offset = 0;
			output_buffer = unarchive(uncompressed_stream, out_stream, get_header_tar, extract_function, prefix, file_list, NULL);
		}
		seek_sub_file(deb_stream, ar_header->size);
		free(ar_header->name);
		free(ar_header);
	}
	gz_close(gunzip_pid);
	fclose(deb_stream);
	fclose(uncompressed_stream);
	free(ared_file);
	if (filename != NULL) {
		free(file_list[0]);
		free(file_list);
	}
	return(output_buffer);
}