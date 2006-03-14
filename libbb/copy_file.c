/* vi: set sw=4 ts=4: */
/*
 * Mini copy_file implementation for busybox
 *
 * Copyright (C) 2001 by Matt Kraai <kraai@alumni.carnegiemellon.edu>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <utime.h>
#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

#include "busybox.h"

/* Compiler version-specific crap that should be in a header file somewhere. */

#if !((__GLIBC__ >= 2) && (__GLIBC_MINOR__ >= 1))
#define lchown chown
#endif


int copy_file(const char *source, const char *dest, int flags)
{
	struct stat source_stat;
	struct stat dest_stat;
	int dest_exists = 0;
	int status = 0;

	if ((!(flags & FILEUTILS_DEREFERENCE) &&
			lstat(source, &source_stat) < 0) ||
			((flags & FILEUTILS_DEREFERENCE) &&
			 stat(source, &source_stat) < 0)) {
		bb_perror_msg("%s", source);
		return -1;
	}

	if (lstat(dest, &dest_stat) < 0) {
		if (errno != ENOENT) {
			bb_perror_msg("unable to stat `%s'", dest);
			return -1;
		}
	} else {
		if (source_stat.st_dev == dest_stat.st_dev &&
			source_stat.st_ino == dest_stat.st_ino)
		{
			bb_error_msg("`%s' and `%s' are the same file", source, dest);
			return -1;
		}
		dest_exists = 1;
	}

	if (S_ISDIR(source_stat.st_mode)) {
		DIR *dp;
		struct dirent *d;
		mode_t saved_umask = 0;

		if (!(flags & FILEUTILS_RECUR)) {
			bb_error_msg("%s: omitting directory", source);
			return -1;
		}

		/* Create DEST.  */
		if (dest_exists) {
			if (!S_ISDIR(dest_stat.st_mode)) {
				bb_error_msg("`%s' is not a directory", dest);
				return -1;
			}
		} else {
			mode_t mode;
			saved_umask = umask(0);

			mode = source_stat.st_mode;
			if (!(flags & FILEUTILS_PRESERVE_STATUS))
				mode = source_stat.st_mode & ~saved_umask;
			mode |= S_IRWXU;

			if (mkdir(dest, mode) < 0) {
				umask(saved_umask);
				bb_perror_msg("cannot create directory `%s'", dest);
				return -1;
			}

			umask(saved_umask);
		}

		/* Recursively copy files in SOURCE.  */
		if ((dp = opendir(source)) == NULL) {
			bb_perror_msg("unable to open directory `%s'", source);
			status = -1;
			goto preserve_status;
		}

		while ((d = readdir(dp)) != NULL) {
			char *new_source, *new_dest;

			new_source = concat_subpath_file(source, d->d_name);
			if(new_source == NULL)
				continue;
			new_dest = concat_path_file(dest, d->d_name);
			if (copy_file(new_source, new_dest, flags) < 0)
				status = -1;
			free(new_source);
			free(new_dest);
		}
		/* closedir have only EBADF error, but "dp" not changes */
		closedir(dp);

		if (!dest_exists &&
				chmod(dest, source_stat.st_mode & ~saved_umask) < 0) {
			bb_perror_msg("unable to change permissions of `%s'", dest);
			status = -1;
		}
	} else if (S_ISREG(source_stat.st_mode) || (flags & FILEUTILS_DEREFERENCE))
	{
		int src_fd;
		int dst_fd;
#ifdef CONFIG_FEATURE_PRESERVE_HARDLINKS
		char *link_name;

		if (!(flags & FILEUTILS_DEREFERENCE) &&
				is_in_ino_dev_hashtable(&source_stat, &link_name)) {
			if (link(link_name, dest) < 0) {
				bb_perror_msg("unable to link `%s'", dest);
				return -1;
			}

			return 0;
		}
		add_to_ino_dev_hashtable(&source_stat, dest);
#endif
		src_fd = open(source, O_RDONLY);
		if (src_fd == -1) {
			bb_perror_msg("unable to open `%s'", source);
			return(-1);
		}

		if (dest_exists) {
			if (flags & FILEUTILS_INTERACTIVE) {
				fprintf(stderr, "%s: overwrite `%s'? ", bb_applet_name, dest);
				if (!bb_ask_confirmation()) {
					close (src_fd);
					return 0;
				}
			}

			dst_fd = open(dest, O_WRONLY|O_TRUNC);
			if (dst_fd == -1) {
				if (!(flags & FILEUTILS_FORCE)) {
					bb_perror_msg("unable to open `%s'", dest);
					close(src_fd);
					return -1;
				}

				if (unlink(dest) < 0) {
					bb_perror_msg("unable to remove `%s'", dest);
					close(src_fd);
					return -1;
				}

				goto dest_removed;
			}
		} else {
dest_removed:
			dst_fd = open(dest, O_WRONLY|O_CREAT, source_stat.st_mode);
			if (dst_fd == -1) {
				bb_perror_msg("unable to open `%s'", dest);
				close(src_fd);
				return(-1);
			}
		}

		if (bb_copyfd_eof(src_fd, dst_fd) == -1)
			status = -1;

		if (close(dst_fd) < 0) {
			bb_perror_msg("unable to close `%s'", dest);
			status = -1;
		}

		if (close(src_fd) < 0) {
			bb_perror_msg("unable to close `%s'", source);
			status = -1;
		}
	} else if (S_ISBLK(source_stat.st_mode) || S_ISCHR(source_stat.st_mode) ||
	    S_ISSOCK(source_stat.st_mode) || S_ISFIFO(source_stat.st_mode) ||
	    S_ISLNK(source_stat.st_mode)) {

		if (dest_exists) {
			if((flags & FILEUTILS_FORCE) == 0) {
				fprintf(stderr, "`%s' exists\n", dest);
				return -1;
			}
			if(unlink(dest) < 0) {
				bb_perror_msg("unable to remove `%s'", dest);
				return -1;
			}
		}
		if (S_ISFIFO(source_stat.st_mode)) {
			if (mkfifo(dest, source_stat.st_mode) < 0) {
				bb_perror_msg("cannot create fifo `%s'", dest);
				return -1;
			}
		} else if (S_ISLNK(source_stat.st_mode)) {
			char *lpath;

			lpath = xreadlink(source);
			if (symlink(lpath, dest) < 0) {
				bb_perror_msg("cannot create symlink `%s'", dest);
				return -1;
			}
			free(lpath);

			if (flags & FILEUTILS_PRESERVE_STATUS)
				if (lchown(dest, source_stat.st_uid, source_stat.st_gid) < 0)
					bb_perror_msg("unable to preserve ownership of `%s'", dest);

			return 0;

		} else {
			if (mknod(dest, source_stat.st_mode, source_stat.st_rdev) < 0) {
				bb_perror_msg("unable to create `%s'", dest);
				return -1;
			}
		}
	} else {
		bb_error_msg("internal error: unrecognized file type");
		return -1;
	}

preserve_status:

	if (flags & FILEUTILS_PRESERVE_STATUS) {
		struct utimbuf times;
		char *msg="unable to preserve %s of `%s'";

		times.actime = source_stat.st_atime;
		times.modtime = source_stat.st_mtime;
		if (utime(dest, &times) < 0)
			bb_perror_msg(msg, "times", dest);
		if (chown(dest, source_stat.st_uid, source_stat.st_gid) < 0) {
			source_stat.st_mode &= ~(S_ISUID | S_ISGID);
			bb_perror_msg(msg, "ownership", dest);
		}
		if (chmod(dest, source_stat.st_mode) < 0)
			bb_perror_msg(msg, "permissions", dest);
	}

	return status;
}
