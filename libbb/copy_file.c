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
copy_file(const char *srcName, const char *destName,
		 int setModes, int followLinks, int forceFlag)
{
	int rfd;
	int wfd;
	int status;
	struct stat srcStatBuf;
	struct stat dstStatBuf;
	struct utimbuf times;

	if (followLinks == TRUE)
		status = stat(srcName, &srcStatBuf);
	else
		status = lstat(srcName, &srcStatBuf);

	if (status < 0) {
		perror_msg("%s", srcName);
		return FALSE;
	}

	if (followLinks == TRUE)
		status = stat(destName, &dstStatBuf);
	else
		status = lstat(destName, &dstStatBuf);

	if (status < 0 || forceFlag==TRUE) {
		unlink(destName);
		dstStatBuf.st_ino = -1;
		dstStatBuf.st_dev = -1;
	}

	if ((srcStatBuf.st_dev == dstStatBuf.st_dev) &&
		(srcStatBuf.st_ino == dstStatBuf.st_ino)) {
		error_msg("Copying file \"%s\" to itself", srcName);
		return FALSE;
	}

	if (S_ISDIR(srcStatBuf.st_mode)) {
		//fprintf(stderr, "copying directory %s to %s\n", srcName, destName);
		/* Make sure the directory is writable */
		status = mkdir(destName, 0777777 ^ umask(0));
		if (status < 0 && errno != EEXIST) {
			perror_msg("%s", destName);
			return FALSE;
		}
	} else if (S_ISLNK(srcStatBuf.st_mode)) {
		char link_val[BUFSIZ + 1];
		int link_size;

		//fprintf(stderr, "copying link %s to %s\n", srcName, destName);
		/* Warning: This could possibly truncate silently, to BUFSIZ chars */
		link_size = readlink(srcName, &link_val[0], BUFSIZ);
		if (link_size < 0) {
			perror_msg("%s", srcName);
			return FALSE;
		}
		link_val[link_size] = '\0';
		status = symlink(link_val, destName);
		if (status < 0) {
			perror_msg("%s", destName);
			return FALSE;
		}
#if (__GLIBC__ >= 2) && (__GLIBC_MINOR__ >= 1)
		if (setModes == TRUE) {
			/* Try to set owner, but fail silently like GNU cp */
			lchown(destName, srcStatBuf.st_uid, srcStatBuf.st_gid);
		}
#endif
		return TRUE;
	} else if (S_ISFIFO(srcStatBuf.st_mode)) {
		//fprintf(stderr, "copying fifo %s to %s\n", srcName, destName);
		if (mkfifo(destName, 0644) < 0) {
			perror_msg("%s", destName);
			return FALSE;
		}
	} else if (S_ISBLK(srcStatBuf.st_mode) || S_ISCHR(srcStatBuf.st_mode)
			   || S_ISSOCK(srcStatBuf.st_mode)) {
		//fprintf(stderr, "copying soc, blk, or chr %s to %s\n", srcName, destName);
		if (mknod(destName, srcStatBuf.st_mode, srcStatBuf.st_rdev) < 0) {
			perror_msg("%s", destName);
			return FALSE;
		}
	} else if (S_ISREG(srcStatBuf.st_mode)) {
		//fprintf(stderr, "copying regular file %s to %s\n", srcName, destName);
		rfd = open(srcName, O_RDONLY);
		if (rfd < 0) {
			perror_msg("%s", srcName);
			return FALSE;
		}

	 	wfd = open(destName, O_WRONLY | O_CREAT | O_TRUNC,
				 srcStatBuf.st_mode);
		if (wfd < 0) {
			perror_msg("%s", destName);
			close(rfd);
			return FALSE;
		}

		if (copy_file_chunk(rfd, wfd, srcStatBuf.st_size)==FALSE)
			goto error_exit;	
		
		close(rfd);
		if (close(wfd) < 0) {
			return FALSE;
		}
	}

	if (setModes == TRUE) {
		/* This is fine, since symlinks never get here */
		if (chown(destName, srcStatBuf.st_uid, srcStatBuf.st_gid) < 0)
			perror_msg_and_die("%s", destName);
		if (chmod(destName, srcStatBuf.st_mode) < 0)
			perror_msg_and_die("%s", destName);
		times.actime = srcStatBuf.st_atime;
		times.modtime = srcStatBuf.st_mtime;
		if (utime(destName, &times) < 0)
			perror_msg_and_die("%s", destName);
	}

	return TRUE;

  error_exit:
	perror_msg("%s", destName);
	close(rfd);
	close(wfd);

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
