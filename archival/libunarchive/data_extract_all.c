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
		bb_make_directory (dirname(name), -1, FILEUTILS_RECUR);
		free(name);
	}

	/* Check if the file already exists */
	if (archive_handle->flags & ARCHIVE_EXTRACT_UNCONDITIONAL) {
		/* Remove the existing entry if it exists */
		if (((file_header->mode & S_IFMT) != S_IFDIR) && (unlink(file_header->name) == -1) && (errno != ENOENT)) {
			bb_perror_msg_and_die("Couldnt remove old file");
		}
	}
	else if (archive_handle->flags & ARCHIVE_EXTRACT_NEWER) {
		/* Remove the existing entry if its older than the extracted entry */
		struct stat statbuf;
		if (lstat(file_header->name, &statbuf) == -1) {
			if (errno != ENOENT) {
				bb_perror_msg_and_die("Couldnt stat old file");
			}
		}
		else if (statbuf.st_mtime <= file_header->mtime) {
			if (!(archive_handle->flags & ARCHIVE_EXTRACT_QUIET)) {
				bb_error_msg("%s not created: newer or same age file exists", file_header->name);
			}
			data_skip(archive_handle);
			return;
		}
		else if ((unlink(file_header->name) == -1) && (errno != EISDIR)) {
			bb_perror_msg_and_die("Couldnt remove old file %s", file_header->name);
		}
	}

	/* Handle hard links separately
	 * We identified hard links as regular files of size 0 with a symlink */
	if (S_ISREG(file_header->mode) && (file_header->link_name) && (file_header->size == 0)) {
		/* hard link */
		res = link(file_header->link_name, file_header->name);
		if ((res == -1) && !(archive_handle->flags & ARCHIVE_EXTRACT_QUIET)) {
			bb_perror_msg("Couldnt create hard link");
		}
	} else {
		/* Create the filesystem entry */
		switch(file_header->mode & S_IFMT) {
			case S_IFREG: {
				/* Regular file */
				dst_fd = bb_xopen(file_header->name, O_WRONLY | O_CREAT | O_EXCL);
				bb_copyfd_size(archive_handle->src_fd, dst_fd, file_header->size);
				close(dst_fd);
				break;
				}
			case S_IFDIR:
				res = mkdir(file_header->name, file_header->mode);
				if ((errno != EISDIR) && (res == -1) && !(archive_handle->flags & ARCHIVE_EXTRACT_QUIET)) {
					bb_perror_msg("extract_archive: %s", file_header->name);
				}
				break;
			case S_IFLNK:
				/* Symlink */
				res = symlink(file_header->link_name, file_header->name);
				if ((res == -1) && !(archive_handle->flags & ARCHIVE_EXTRACT_QUIET)) {
					bb_perror_msg("Cannot create symlink from %s to '%s'", file_header->name, file_header->link_name);
				}
				break;
			case S_IFSOCK:
			case S_IFBLK:
			case S_IFCHR:
			case S_IFIFO:
				res = mknod(file_header->name, file_header->mode, file_header->device);
				if ((res == -1) && !(archive_handle->flags & ARCHIVE_EXTRACT_QUIET)) {
					bb_perror_msg("Cannot create node %s", file_header->name);
				}
				break;
			default:
				bb_error_msg_and_die("Unrecognised file type");
		}
	}

	lchown(file_header->name, file_header->uid, file_header->gid);
	if ((file_header->mode & S_IFMT) != S_IFLNK) {
		chmod(file_header->name, file_header->mode);
	}

	if (archive_handle->flags & ARCHIVE_PRESERVE_DATE) {
		struct utimbuf t;
		t.actime = t.modtime = file_header->mtime;
		utime(file_header->name, &t);
	}
}
