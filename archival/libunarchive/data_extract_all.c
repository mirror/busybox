/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "unarchive.h"

void FAST_FUNC data_extract_all(archive_handle_t *archive_handle)
{
	file_header_t *file_header = archive_handle->file_header;
	int dst_fd;
	int res;

	if (archive_handle->ah_flags & ARCHIVE_CREATE_LEADING_DIRS) {
		char *slash = strrchr(file_header->name, '/');
		if (slash) {
			*slash = '\0';
			bb_make_directory(file_header->name, -1, FILEUTILS_RECUR);
			*slash = '/';
		}
	}

	if (archive_handle->ah_flags & ARCHIVE_UNLINK_OLD) {
		/* Remove the entry if it exists */
		if ((!S_ISDIR(file_header->mode))
		 && (unlink(file_header->name) == -1)
		 && (errno != ENOENT)
		) {
			bb_perror_msg_and_die("can't remove old file %s",
					file_header->name);
		}
	}
	else if (archive_handle->ah_flags & ARCHIVE_EXTRACT_NEWER) {
		/* Remove the existing entry if its older than the extracted entry */
		struct stat existing_sb;
		if (lstat(file_header->name, &existing_sb) == -1) {
			if (errno != ENOENT) {
				bb_perror_msg_and_die("can't stat old file");
			}
		}
		else if (existing_sb.st_mtime >= file_header->mtime) {
			if (!(archive_handle->ah_flags & ARCHIVE_EXTRACT_QUIET)) {
				bb_error_msg("%s not created: newer or "
					"same age file exists", file_header->name);
			}
			data_skip(archive_handle);
			return;
		}
		else if ((unlink(file_header->name) == -1) && (errno != EISDIR)) {
			bb_perror_msg_and_die("can't remove old file %s",
					file_header->name);
		}
	}

	/* Handle hard links separately
	 * We identified hard links as regular files of size 0 with a symlink */
	if (S_ISREG(file_header->mode)
	 && file_header->link_target
	 && file_header->size == 0
	) {
		/* hard link */
		res = link(file_header->link_target, file_header->name);
		if ((res == -1) && !(archive_handle->ah_flags & ARCHIVE_EXTRACT_QUIET)) {
			bb_perror_msg("can't create %slink "
					"from %s to %s", "hard",
					file_header->name,
					file_header->link_target);
		}
	} else {
		/* Create the filesystem entry */
		switch (file_header->mode & S_IFMT) {
		case S_IFREG: {
			/* Regular file */
			int flags = O_WRONLY | O_CREAT | O_EXCL;
			if (archive_handle->ah_flags & ARCHIVE_O_TRUNC)
				flags = O_WRONLY | O_CREAT | O_TRUNC;
			dst_fd = xopen3(file_header->name,
				flags,
				file_header->mode
				);
			bb_copyfd_exact_size(archive_handle->src_fd, dst_fd, file_header->size);
			close(dst_fd);
			break;
		}
		case S_IFDIR:
			res = mkdir(file_header->name, file_header->mode);
			if ((res == -1)
			 && (errno != EISDIR) /* btw, Linux doesn't return this */
			 && (errno != EEXIST)
			 && !(archive_handle->ah_flags & ARCHIVE_EXTRACT_QUIET)
			) {
				bb_perror_msg("can't make dir %s", file_header->name);
			}
			break;
		case S_IFLNK:
			/* Symlink */
			res = symlink(file_header->link_target, file_header->name);
			if ((res == -1)
			 && !(archive_handle->ah_flags & ARCHIVE_EXTRACT_QUIET)
			) {
				bb_perror_msg("can't create %slink "
					"from %s to %s", "sym",
					file_header->name,
					file_header->link_target);
			}
			break;
		case S_IFSOCK:
		case S_IFBLK:
		case S_IFCHR:
		case S_IFIFO:
			res = mknod(file_header->name, file_header->mode, file_header->device);
			if ((res == -1)
			 && !(archive_handle->ah_flags & ARCHIVE_EXTRACT_QUIET)
			) {
				bb_perror_msg("can't create node %s", file_header->name);
			}
			break;
		default:
			bb_error_msg_and_die("unrecognized file type");
		}
	}

	if (!(archive_handle->ah_flags & ARCHIVE_DONT_RESTORE_OWNER)) {
#if ENABLE_FEATURE_TAR_UNAME_GNAME
		if (!(archive_handle->ah_flags & ARCHIVE_NUMERIC_OWNER)) {
			uid_t uid = file_header->uid;
			gid_t gid = file_header->gid;

			if (file_header->tar__uname) {
//TODO: cache last name/id pair?
				struct passwd *pwd = getpwnam(file_header->tar__uname);
				if (pwd) uid = pwd->pw_uid;
			}
			if (file_header->tar__gname) {
				struct group *grp = getgrnam(file_header->tar__gname);
				if (grp) gid = grp->gr_gid;
			}
			/* GNU tar 1.15.1 uses chown, not lchown */
			chown(file_header->name, uid, gid);
		} else
#endif
			chown(file_header->name, file_header->uid, file_header->gid);
	}
	if (!S_ISLNK(file_header->mode)) {
		/* uclibc has no lchmod, glibc is even stranger -
		 * it has lchmod which seems to do nothing!
		 * so we use chmod... */
		if (!(archive_handle->ah_flags & ARCHIVE_DONT_RESTORE_PERM)) {
			chmod(file_header->name, file_header->mode);
		}
		/* same for utime */
		if (archive_handle->ah_flags & ARCHIVE_RESTORE_DATE) {
			struct timeval t[2];

			t[1].tv_sec = t[0].tv_sec = file_header->mtime;
			t[1].tv_usec = t[0].tv_usec = 0;
			utimes(file_header->name, t);
		}
	}
}
