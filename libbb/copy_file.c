/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
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

#include <stdio.h>
#include <errno.h>
#include <utime.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "libbb.h"


/*
 * Copy one file to another, while possibly preserving its modes, times, and
 * modes.  Returns TRUE if successful, or FALSE on a failure with an error
 * message output.  (Failure is not indicated if attributes cannot be set.)
 * -Erik Andersen
 */
int
copy_file(const char *src_name, const char *dst_name,
		 int set_modes, int follow_links, int force_flag, int quiet_flag)
{
	FILE *src_file = NULL;
	FILE *dst_file = NULL;
	struct stat srcStatBuf;
	struct stat dstStatBuf;
	struct utimbuf times;
	int src_status;
	int dst_status;

	if (follow_links == TRUE) {
		src_status = stat(src_name, &srcStatBuf);
		dst_status = stat(dst_name, &dstStatBuf);
	} else {
		src_status = lstat(src_name, &srcStatBuf);
		dst_status = lstat(dst_name, &dstStatBuf);
	}

	if (src_status < 0) {
		if (!quiet_flag) {
			perror_msg("%s", src_name);
		}
		return FALSE;
	}

	if ((dst_status < 0) || force_flag) {
		unlink(dst_name);
		dstStatBuf.st_ino = -1;
		dstStatBuf.st_dev = -1;
	}

	if ((srcStatBuf.st_dev == dstStatBuf.st_dev) &&
		(srcStatBuf.st_ino == dstStatBuf.st_ino)) {
		if (!quiet_flag) {
			error_msg("Copying file \"%s\" to itself", src_name);
		}
		return FALSE;
	}

	if (S_ISDIR(srcStatBuf.st_mode)) {
		//fprintf(stderr, "copying directory %s to %s\n", srcName, destName);
		/* Make sure the directory is writable */
		dst_status = create_path(dst_name, 0777777 ^ umask(0));
		if ((dst_status < 0) && (errno != EEXIST)) {
			if (!quiet_flag) {
				perror_msg("%s", dst_name);
			}
			return FALSE;
		}
	} else if (S_ISLNK(srcStatBuf.st_mode)) {
		char link_val[BUFSIZ + 1];
		int link_size;

		//fprintf(stderr, "copying link %s to %s\n", srcName, destName);
		/* Warning: This could possibly truncate silently, to BUFSIZ chars */
		link_size = readlink(src_name, &link_val[0], BUFSIZ);
		if (link_size < 0) {
			if (quiet_flag) {
				perror_msg("%s", src_name);
			}
			return FALSE;
		}
		link_val[link_size] = '\0';
		src_status = symlink(link_val, dst_name);
		if (src_status < 0) {
			if (!quiet_flag) {
				perror_msg("%s", dst_name);
			}
			return FALSE;
		}
#if (__GLIBC__ >= 2) && (__GLIBC_MINOR__ >= 1)
		if (set_modes == TRUE) {
			/* Try to set owner, but fail silently like GNU cp */
			lchown(dst_name, srcStatBuf.st_uid, srcStatBuf.st_gid);
		}
#endif
		return TRUE;
	} else if (S_ISFIFO(srcStatBuf.st_mode)) {
		//fprintf(stderr, "copying fifo %s to %s\n", srcName, destName);
		if (mkfifo(dst_name, 0644) < 0) {
			if (!quiet_flag) {
				perror_msg("%s", dst_name);
			}
			return FALSE;
		}
	} else if (S_ISBLK(srcStatBuf.st_mode) || S_ISCHR(srcStatBuf.st_mode)
			   || S_ISSOCK(srcStatBuf.st_mode)) {
		//fprintf(stderr, "copying soc, blk, or chr %s to %s\n", srcName, destName);
		if (mknod(dst_name, srcStatBuf.st_mode, srcStatBuf.st_rdev) < 0) {
			if (!quiet_flag) {
				perror_msg("%s", dst_name);
			}
			return FALSE;
		}
	} else if (S_ISREG(srcStatBuf.st_mode)) {
		//fprintf(stderr, "copying regular file %s to %s\n", srcName, destName);
		src_file = fopen(src_name, "r");
		if (src_file == NULL) {
			if (!quiet_flag) {
				perror_msg("%s", src_name);
			}
			return FALSE;
		}

	 	dst_file = fopen(dst_name, "w");
		if (dst_file == NULL) {
			if (!quiet_flag) {
				perror_msg("%s", dst_name);
			}
			fclose(src_file);
			return FALSE;
		}

		if (copy_file_chunk(src_file, dst_file, srcStatBuf.st_size)==FALSE) {
			goto error_exit;
		}

		fclose(src_file);
		if (fclose(dst_file) < 0) {
			return FALSE;
		}
	}

	if (set_modes == TRUE) {
		/* This is fine, since symlinks never get here */
		if (chown(dst_name, srcStatBuf.st_uid, srcStatBuf.st_gid) < 0)
			perror_msg("%s", dst_name);
		if (chmod(dst_name, srcStatBuf.st_mode) < 0)
			perror_msg("%s", dst_name);
		times.actime = srcStatBuf.st_atime;
		times.modtime = srcStatBuf.st_mtime;
		if (utime(dst_name, &times) < 0)
			perror_msg("%s", dst_name);
	}

	return TRUE;

error_exit:
	perror_msg("%s", dst_name);
	fclose(src_file);
	fclose(dst_file);

	return FALSE;
}

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
