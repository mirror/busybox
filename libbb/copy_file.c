/* vi: set sw=4 ts=4: */
/*
 * Mini copy_file implementation for busybox
 *
 * Copyright (C) 2001 by Matt Kraai <kraai@alumni.carnegiemellon.edu>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 */

#include "libbb.h"

static int retry_overwrite(const char *dest, int flags)
{
	if (!(flags & (FILEUTILS_FORCE|FILEUTILS_INTERACTIVE))) {
		fprintf(stderr, "'%s' exists\n", dest);
		return -1;
	}
	if (flags & FILEUTILS_INTERACTIVE) {
		fprintf(stderr, "%s: overwrite '%s'? ", applet_name, dest);
		if (!bb_ask_confirmation())
			return 0; // not allowed to overwrite
	}
	if (unlink(dest) < 0) {
		bb_perror_msg("cannot remove '%s'", dest);
		return -1; // error
	}
	return 1; // ok (to try again)
}

int copy_file(const char *source, const char *dest, int flags)
{
	struct stat source_stat;
	struct stat dest_stat;
	int status = 0;
	signed char dest_exists = 0;
	signed char ovr;

#define FLAGS_DEREF (flags & FILEUTILS_DEREFERENCE)

	if ((FLAGS_DEREF ? stat : lstat)(source, &source_stat) < 0) {
		// This may be a dangling symlink.
		// Making [sym]links to dangling symlinks works, so...
		if (flags & (FILEUTILS_MAKE_SOFTLINK|FILEUTILS_MAKE_HARDLINK))
			goto make_links;
		bb_perror_msg("cannot stat '%s'", source);
		return -1;
	}

	if (lstat(dest, &dest_stat) < 0) {
		if (errno != ENOENT) {
			bb_perror_msg("cannot stat '%s'", dest);
			return -1;
		}
	} else {
		if (source_stat.st_dev == dest_stat.st_dev
		 && source_stat.st_ino == dest_stat.st_ino
		) {
			bb_error_msg("'%s' and '%s' are the same file", source, dest);
			return -1;
		}
		dest_exists = 1;
	}

	if (S_ISDIR(source_stat.st_mode)) {
		DIR *dp;
		struct dirent *d;
		mode_t saved_umask = 0;

		if (!(flags & FILEUTILS_RECUR)) {
			bb_error_msg("omitting directory '%s'", source);
			return -1;
		}

		/* Create DEST.  */
		if (dest_exists) {
			if (!S_ISDIR(dest_stat.st_mode)) {
				bb_error_msg("target '%s' is not a directory", dest);
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
				bb_perror_msg("cannot create directory '%s'", dest);
				return -1;
			}

			umask(saved_umask);
		}

		/* Recursively copy files in SOURCE.  */
		dp = opendir(source);
		if (dp == NULL) {
			status = -1;
			goto preserve_status;
		}

		while ((d = readdir(dp)) != NULL) {
			char *new_source, *new_dest;

			new_source = concat_subpath_file(source, d->d_name);
			if (new_source == NULL)
				continue;
			new_dest = concat_path_file(dest, d->d_name);
			if (copy_file(new_source, new_dest, flags) < 0)
				status = -1;
			free(new_source);
			free(new_dest);
		}
		closedir(dp);

		if (!dest_exists
		 && chmod(dest, source_stat.st_mode & ~saved_umask) < 0
		) {
			bb_perror_msg("cannot change permissions of '%s'", dest);
			status = -1;
		}

	} else if (flags & (FILEUTILS_MAKE_SOFTLINK|FILEUTILS_MAKE_HARDLINK)) {
		int (*lf)(const char *oldpath, const char *newpath);
 make_links:
		// Hmm... maybe
		// if (DEREF && MAKE_SOFTLINK) source = realpath(source) ?
		// (but realpath returns NULL on dangling symlinks...)
		lf = (flags & FILEUTILS_MAKE_SOFTLINK) ? symlink : link;
		if (lf(source, dest) < 0) {
			ovr = retry_overwrite(dest, flags);
			if (ovr <= 0)
				return ovr;
			if (lf(source, dest) < 0) {
				bb_perror_msg("cannot create link '%s'", dest);
				return -1;
			}
		}
		return 0;

	} else if (S_ISREG(source_stat.st_mode)
	// Huh? DEREF uses stat, which never returns links IIRC...
	 || (FLAGS_DEREF && S_ISLNK(source_stat.st_mode))
	) {
		int src_fd;
		int dst_fd;
		if (ENABLE_FEATURE_PRESERVE_HARDLINKS) {
			char *link_name;

			if (!FLAGS_DEREF
			 && is_in_ino_dev_hashtable(&source_stat, &link_name)
			) {
				if (link(link_name, dest) < 0) {
					ovr = retry_overwrite(dest, flags);
					if (ovr <= 0)
						return ovr;
					if (link(link_name, dest) < 0) {
						bb_perror_msg("cannot create link '%s'", dest);
						return -1;
					}
				}
				return 0;
			}
			// TODO: probably is_in_.. and add_to_...
			// can be combined: find_or_add_...
			add_to_ino_dev_hashtable(&source_stat, dest);
		}

		src_fd = open(source, O_RDONLY);
		if (src_fd == -1) {
			bb_perror_msg("cannot open '%s'", source);
			return -1;
		}

		// POSIX: if exists and -i, ask (w/o -i assume yes).
		// Then open w/o EXCL.
		// If open still fails and -f, try unlink, then try open again.
		// Result: a mess:
		// If dest is a softlink, we overwrite softlink's destination!
		// (or fail, if it points to dir/nonexistent location/etc).
		// This is strange, but POSIX-correct.
		// coreutils cp has --remove-destination to override this...
		dst_fd = open(dest, (flags & FILEUTILS_INTERACTIVE)
				? O_WRONLY|O_CREAT|O_TRUNC|O_EXCL
				: O_WRONLY|O_CREAT|O_TRUNC, source_stat.st_mode);
		if (dst_fd == -1) {
			// We would not do POSIX insanity. -i asks,
			// then _unlinks_ the offender. Presto.
			// Or else we will end up having 3 open()s!
			ovr = retry_overwrite(dest, flags);
			if (ovr <= 0) {
				close(src_fd);
				return ovr;
			}
			dst_fd = open(dest, O_WRONLY|O_CREAT|O_TRUNC, source_stat.st_mode);
			if (dst_fd == -1) {
				bb_perror_msg("cannot open '%s'", dest);
				close(src_fd);
				return -1;
			}
		}

		if (bb_copyfd_eof(src_fd, dst_fd) == -1)
			status = -1;
		if (close(dst_fd) < 0) {
			bb_perror_msg("cannot close '%s'", dest);
			status = -1;
		}
		if (close(src_fd) < 0) {
			bb_perror_msg("cannot close '%s'", source);
			status = -1;
		}

	} else if (S_ISBLK(source_stat.st_mode) || S_ISCHR(source_stat.st_mode)
	 || S_ISSOCK(source_stat.st_mode) || S_ISFIFO(source_stat.st_mode)
	 || S_ISLNK(source_stat.st_mode)
	) {
		// We are lazy here, a bit lax with races...
		if (dest_exists) {
			ovr = retry_overwrite(dest, flags);
			if (ovr <= 0)
				return ovr;
		}
		if (S_ISFIFO(source_stat.st_mode)) {
			if (mkfifo(dest, source_stat.st_mode) < 0) {
				bb_perror_msg("cannot create fifo '%s'", dest);
				return -1;
			}
		} else if (S_ISLNK(source_stat.st_mode)) {
			char *lpath;

			lpath = xreadlink(source);
			if (symlink(lpath, dest) < 0) {
				bb_perror_msg("cannot create symlink '%s'", dest);
				free(lpath);
				return -1;
			}
			free(lpath);

			if (flags & FILEUTILS_PRESERVE_STATUS)
				if (lchown(dest, source_stat.st_uid, source_stat.st_gid) < 0)
					bb_perror_msg("cannot preserve %s of '%s'", "ownership", dest);

			return 0;

		} else {
			if (mknod(dest, source_stat.st_mode, source_stat.st_rdev) < 0) {
				bb_perror_msg("cannot create '%s'", dest);
				return -1;
			}
		}
	} else {
		bb_error_msg("internal error: unrecognized file type");
		return -1;
	}

 preserve_status:

	if (flags & FILEUTILS_PRESERVE_STATUS
	/* Cannot happen: */
	/* && !(flags & (FILEUTILS_MAKE_SOFTLINK|FILEUTILS_MAKE_HARDLINK)) */
	) {
		struct utimbuf times;

		times.actime = source_stat.st_atime;
		times.modtime = source_stat.st_mtime;
		if (utime(dest, &times) < 0)
			bb_perror_msg("cannot preserve %s of '%s'", "times", dest);
		if (chown(dest, source_stat.st_uid, source_stat.st_gid) < 0) {
			source_stat.st_mode &= ~(S_ISUID | S_ISGID);
			bb_perror_msg("cannot preserve %s of '%s'", "ownership", dest);
		}
		if (chmod(dest, source_stat.st_mode) < 0)
			bb_perror_msg("cannot preserve %s of '%s'", "permissions", dest);
	}

	return status;
}
