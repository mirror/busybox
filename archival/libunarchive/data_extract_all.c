/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <utime.h>
#include <unistd.h>
#include <stdlib.h>

#include "libbb.h"
#include "unarchive.h"

extern void data_extract_all(archive_handle_t *archive_handle)
{
	file_header_t *file_header = archive_handle->file_header;
	int dst_fd;
	int res;

	if (archive_handle->flags & ARCHIVE_CREATE_LEADING_DIRS) {
		char *name = bb_xstrdup(file_header->name);
		bb_make_directory (dirname(name), 0777, FILEUTILS_RECUR);
		free(name);
	}                  

	/* Create the filesystem entry */
	switch(file_header->mode & S_IFMT) {
		case S_IFREG: {
#ifdef CONFIG_CPIO
			if (file_header->link_name && file_header->size == 0) {
				/* hard link */
				res = link(file_header->link_name, file_header->name);
				if ((res == -1) && !(archive_handle->flags & ARCHIVE_EXTRACT_QUIET)) {
					bb_perror_msg("Couldnt create hard link");
				}
			} else
#endif
			{
				/* Regular file */
				dst_fd = bb_xopen(file_header->name, O_WRONLY | O_CREAT);
				archive_copy_file(archive_handle, dst_fd);
				close(dst_fd);
			}
			break;
		}
		case S_IFDIR:
			unlink(file_header->name);
			res = mkdir(file_header->name, file_header->mode);
			if ((res == -1) && !(archive_handle->flags & ARCHIVE_EXTRACT_QUIET)) {
				bb_perror_msg("extract_archive: %s", file_header->name);
			}
			break;
		case S_IFLNK:
			/* Symlink */
			unlink(file_header->name);
			res = symlink(file_header->link_name, file_header->name);
			if ((res == -1) && !(archive_handle->flags & ARCHIVE_EXTRACT_QUIET)) {
				bb_perror_msg("Cannot create symlink from %s to '%s'", file_header->name, file_header->link_name);
			}
			break;
		case S_IFSOCK:
		case S_IFBLK:
		case S_IFCHR:
		case S_IFIFO:
			unlink(file_header->name);
			res = mknod(file_header->name, file_header->mode, file_header->device);
			if ((res == -1) && !(archive_handle->flags & ARCHIVE_EXTRACT_QUIET)) {
				bb_perror_msg("Cannot create node %s", file_header->name);
			}
			break;
		default:
			bb_error_msg_and_die("Unrecognised file type");
	}

	chmod(file_header->name, file_header->mode);
	chown(file_header->name, file_header->uid, file_header->gid);

	if (archive_handle->flags & ARCHIVE_PRESERVE_DATE) {
		struct utimbuf t;
		t.actime = t.modtime = file_header->mtime;
		utime(file_header->name, &t);
	}
}
