/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "unarchive.h"

void data_extract_all(archive_handle_t *archive_handle)
{
	file_header_t *file_header = archive_handle->file_header;
	int dst_fd;
	int res;

	if (archive_handle->flags & ARCHIVE_CREATE_LEADING_DIRS) {
		char *name = xstrdup(file_header->name);
		bb_make_directory(dirname(name), -1, FILEUTILS_RECUR);
		free(name);
	}

	/* Check if the file already exists */
	if (archive_handle->flags & ARCHIVE_EXTRACT_UNCONDITIONAL) {
		/* Remove the existing entry if it exists */
		if (((file_header->mode & S_IFMT) != S_IFDIR)
		 && (unlink(file_header->name) == -1)
		 && (errno != ENOENT)
		) {
			bb_perror_msg_and_die("cannot remove old file");
		}
	}
	else if (archive_handle->flags & ARCHIVE_EXTRACT_NEWER) {
		/* Remove the existing entry if its older than the extracted entry */
		struct stat statbuf;
		if (lstat(file_header->name, &statbuf) == -1) {
			if (errno != ENOENT) {
				bb_perror_msg_and_die("cannot stat old file");
			}
		}
		else if (statbuf.st_mtime <= file_header->mtime) {
			if (!(archive_handle->flags & ARCHIVE_EXTRACT_QUIET)) {
				bb_error_msg("%s not created: newer or "
					"same age file exists", file_header->name);
			}
			data_skip(archive_handle);
			return;
		}
		else if ((unlink(file_header->name) == -1) && (errno != EISDIR)) {
			bb_perror_msg_and_die("cannot remove old file %s",
					file_header->name);
		}
	}

	/* Handle hard links separately
	 * We identified hard links as regular files of size 0 with a symlink */
	if (S_ISREG(file_header->mode) && (file_header->link_name)
	 && (file_header->size == 0)
	) {
		/* hard link */
		res = link(file_header->link_name, file_header->name);
		if ((res == -1) && !(archive_handle->flags & ARCHIVE_EXTRACT_QUIET)) {
			bb_perror_msg("cannot create hard link");
		}
	} else {
		/* Create the filesystem entry */
		switch (file_header->mode & S_IFMT) {
		case S_IFREG: {
			/* Regular file */
			dst_fd = xopen3(file_header->name, O_WRONLY | O_CREAT | O_EXCL,
							file_header->mode);
			bb_copyfd_exact_size(archive_handle->src_fd, dst_fd, file_header->size);
			close(dst_fd);
			break;
		}
		case S_IFDIR:
			res = mkdir(file_header->name, file_header->mode);
			if ((errno != EISDIR) && (res == -1)
			 && !(archive_handle->flags & ARCHIVE_EXTRACT_QUIET)
			) {
				bb_perror_msg("extract_archive: %s", file_header->name);
			}
			break;
		case S_IFLNK:
			/* Symlink */
			res = symlink(file_header->link_name, file_header->name);
			if ((res == -1)
			 && !(archive_handle->flags & ARCHIVE_EXTRACT_QUIET)
			) {
				bb_perror_msg("cannot create symlink "
					"from %s to '%s'",
					file_header->name,
					file_header->link_name);
			}
			break;
		case S_IFSOCK:
		case S_IFBLK:
		case S_IFCHR:
		case S_IFIFO:
			res = mknod(file_header->name, file_header->mode, file_header->device);
			if ((res == -1)
			 && !(archive_handle->flags & ARCHIVE_EXTRACT_QUIET)
			) {
				bb_perror_msg("cannot create node %s", file_header->name);
			}
			break;
		default:
			bb_error_msg_and_die("unrecognized file type");
		}
	}

	if (!(archive_handle->flags & ARCHIVE_NOPRESERVE_OWN)) {
		lchown(file_header->name, file_header->uid, file_header->gid);
	}
	/* uclibc has no lchmod, glibc is even stranger -
	 * it has lchmod which seems to do nothing!
	 * so we use chmod... */
	if (!(archive_handle->flags & ARCHIVE_NOPRESERVE_PERM)
	 && (file_header->mode & S_IFMT) != S_IFLNK
	) {
		chmod(file_header->name, file_header->mode);
	}

	if (archive_handle->flags & ARCHIVE_PRESERVE_DATE) {
		struct utimbuf t;
		t.actime = t.modtime = file_header->mtime;
		utime(file_header->name, &t);
	}
}
