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
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include "libbb.h"

/* Extract files in the linear linked list 'extract_headers' to either
 * filesystem, stdout or buffer depending on the value of 'function' which is
 * described in extract_files.h 
 *
 * prefix doesnt have to be just a directory, it may prefix the filename as well.
 *
 * e.g. '/var/lib/dpkg/info/dpkg.' will extract all files to the base bath 
 * '/var/lib/dpkg/info/' and all files/dirs created in that dir will have 
 * 'dpkg.' as their prefix
 *
 * For this reason if prefix does point to a dir then it must end with a
 * trailing '/' or else the last dir will be assumed to be the file prefix 
 */

char *extract_archive(FILE *src_stream, FILE *out_stream, file_headers_t *extract_headers, int function, const char *prefix)
{
	FILE *dst_stream = NULL;
	char *full_name = NULL;
	char *buffer = NULL;
	size_t uncompressed_count = 0;
	struct utimbuf t;

	/* find files to extract or display */	
	while (extract_headers != NULL) {
		/* prefix doesnt have to be a proper path it may prepend 
		 * the filename as well */
		if (prefix != NULL) {
			/* strip leading '/' in filename to extract as prefix may not be dir */
			char *tmp_str = strchr(extract_headers->name, '/');
			if (tmp_str == NULL) {
				tmp_str = extract_headers->name;
			} else {
				tmp_str++;
			}
			/* Cant use concat_path_file here as prefix might not be a directory */
			full_name = xmalloc(strlen(prefix) + strlen(tmp_str) + 1);
			strcpy(full_name, prefix);
			strcat(full_name, tmp_str);
		} else {
			full_name = extract_headers->name;
		}

		/* Seek to start of file, have to use fgetc as src_stream may be a pipe
		 * you cant [lf]seek on pipes */
		while (uncompressed_count < extract_headers->offset) {
			fgetc(src_stream);
			uncompressed_count++;
		}

		if (S_ISREG(extract_headers->mode)) {
			if (function & extract_to_stdout) {
				copy_file_chunk(src_stream, out_stream, extract_headers->size);			
				uncompressed_count += extract_headers->size;
			}
			else if (function & extract_all_to_fs) {
				dst_stream = wfopen(full_name, "w");
				copy_file_chunk(src_stream, dst_stream, extract_headers->size);			
				uncompressed_count += extract_headers->size;
				fclose(dst_stream);
			}
			else if (function & extract_one_to_buffer) {
				buffer = (char *) xmalloc(extract_headers->size + 1);
				fread(buffer, 1, extract_headers->size, src_stream);
				break;
			}
		}
#if defined BB_DPKG | defined BB_DPKG_DEB 
		else if (S_ISDIR(extract_headers->mode)) {
			if (function & extract_all_to_fs) {
				if (create_path(full_name, extract_headers->mode) == FALSE) {
					perror_msg("Cannot mkdir %s", extract_headers->name); 
					return NULL;
				}
			}
		}
		else if (S_ISLNK(extract_headers->mode)) {
			if (function & extract_all_to_fs) {
				if (symlink(extract_headers->link_name, full_name) < 0) {
					perror_msg("Cannot create hard link from %s to '%s'", extract_headers->name, extract_headers->link_name); 
					return NULL;
				}
			}
		}
#endif
		if (function & extract_verbose_list) {
			fprintf(out_stream, "%s %d/%d %8d %s ", mode_string(extract_headers->mode), 
				extract_headers->uid, extract_headers->gid,
				(int) extract_headers->size, time_string(extract_headers->mtime));
		}

		if ((function & extract_list) || (function & extract_verbose_list)){
			/* fputs doesnt add a trailing \n, so use fprintf */
			fprintf(out_stream, "%s\n", extract_headers->name);
		}

		if (function & extract_preserve_date) {
			t.actime = extract_headers->mtime;
			t.modtime = extract_headers->mtime;
			utime(full_name, &t);
		}
		chmod(full_name, extract_headers->mode);
		free(full_name);

		extract_headers = extract_headers->next;
	}

	return(buffer);
}

