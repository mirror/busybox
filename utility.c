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

#include "internal.h"
#if defined (BB_CHMOD_CHOWN_CHGRP) \
 || defined (BB_CP_MV)		   \
 || defined (BB_FIND)		   \
 || defined (BB_INSMOD)		   \
 || defined (BB_LS)		   \
 || defined (BB_RM)		   \
 || defined (BB_TAR)
/* same conditions as recursiveAction */
#define bb_need_name_too_long
#endif
#define bb_need_memory_exhausted
#define bb_need_full_version
#define BB_DECLARE_EXTERN
#include "messages.c"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <utime.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>		/* for uname(2) */

#if defined BB_FEATURE_MOUNT_LOOP
#include <fcntl.h>
#include <linux/loop.h> /* Pull in loop device support */
#endif

/* Busybox mount uses either /proc/filesystems or /dev/mtab to get the 
 * list of available filesystems used for the -t auto option */ 
#if defined BB_FEATURE_USE_PROCFS && defined BB_FEATURE_USE_DEVPS_PATCH
//#error Sorry, but busybox can't use both /proc and /dev/ps at the same time -- Pick one and try again.
#error "Sorry, but busybox can't use both /proc and /dev/ps at the same time -- Pick one and try again."
#endif


#if defined BB_MOUNT || defined BB_UMOUNT || defined BB_DF
#  if defined BB_MTAB
const char mtab_file[] = "/etc/mtab";
#  else
#    if defined BB_FEATURE_USE_PROCFS
const char mtab_file[] = "/proc/mounts";
#    else
#      if defined BB_FEATURE_USE_DEVPS_PATCH
const char mtab_file[] = "/dev/mtab";
#    else
#        error With (BB_MOUNT||BB_UMOUNT||BB_DF) defined, you must define either BB_MTAB or ( BB_FEATURE_USE_PROCFS | BB_FEATURE_USE_DEVPS_PATCH)
#    endif
#  endif
#  endif
#endif

extern void usage(const char *usage)
{
	fprintf(stderr, "%s\n\n", full_version);
	fprintf(stderr, "Usage: %s\n", usage);
	exit FALSE;
}

extern void errorMsg(const char *s, ...)
{
	va_list p;

	va_start(p, s);
	fflush(stdout);
	fprintf(stderr, "%s: ", applet_name);
	vfprintf(stderr, s, p);
	va_end(p);
	fflush(stderr);
}

extern void fatalError(const char *s, ...)
{
	va_list p;

	va_start(p, s);
	fflush(stdout);
	fprintf(stderr, "%s: ", applet_name);
	vfprintf(stderr, s, p);
	va_end(p);
	fflush(stderr);
	exit( FALSE);
}

#if defined BB_INIT
/* Returns kernel version encoded as major*65536 + minor*256 + patch,
 * so, for example,  to check if the kernel is greater than 2.2.11:
 *     if (get_kernel_revision() <= 2*65536+2*256+11) { <stuff> }
 */
extern int get_kernel_revision(void)
{
	struct utsname name;
	int major = 0, minor = 0, patch = 0;

	if (uname(&name) == -1) {
		perror("cannot get system information");
		return (0);
	}
	sscanf(name.version, "%d.%d.%d", &major, &minor, &patch);
	return major * 65536 + minor * 256 + patch;
}
#endif                                                 /* BB_INIT */



#if defined BB_FREE || defined BB_INIT || defined BB_UNAME || defined BB_UPTIME
_syscall1(int, sysinfo, struct sysinfo *, info);
#endif                                                 /* BB_INIT */

#if defined BB_MOUNT || defined BB_UMOUNT

#ifndef __NR_umount2
#define __NR_umount2           52
#endif

/* Include our own version of <sys/mount.h>, since libc5 doesn't
 * know about umount2 */
extern _syscall1(int, umount, const char *, special_file);
extern _syscall2(int, umount2, const char *, special_file, int, flags);
extern _syscall5(int, mount, const char *, special_file, const char *, dir,
		const char *, fstype, unsigned long int, rwflag, const void *, data);
#endif



#if defined (BB_CP_MV) || defined (BB_DU)

#define HASH_SIZE	311		/* Should be prime */
#define hash_inode(i)	((i) % HASH_SIZE)

static ino_dev_hashtable_bucket_t *ino_dev_hashtable[HASH_SIZE];

/*
 * Return 1 if statbuf->st_ino && statbuf->st_dev are recorded in
 * `ino_dev_hashtable', else return 0
 *
 * If NAME is a non-NULL pointer to a character pointer, and there is
 * a match, then set *NAME to the value of the name slot in that
 * bucket.
 */
int is_in_ino_dev_hashtable(const struct stat *statbuf, char **name)
{
	ino_dev_hashtable_bucket_t *bucket;

	bucket = ino_dev_hashtable[hash_inode(statbuf->st_ino)];
	while (bucket != NULL) {
	  if ((bucket->ino == statbuf->st_ino) &&
		  (bucket->dev == statbuf->st_dev))
	  {
		if (name) *name = bucket->name;
		return 1;
	  }
	  bucket = bucket->next;
	}
	return 0;
}

/* Add statbuf to statbuf hash table */
void add_to_ino_dev_hashtable(const struct stat *statbuf, const char *name)
{
	int i;
	size_t s;
	ino_dev_hashtable_bucket_t *bucket;
    
	i = hash_inode(statbuf->st_ino);
	s = name ? strlen(name) : 0;
	bucket = xmalloc(sizeof(ino_dev_hashtable_bucket_t) + s);
	bucket->ino = statbuf->st_ino;
	bucket->dev = statbuf->st_dev;
	if (name)
		strcpy(bucket->name, name);
	else
		bucket->name[0] = '\0';
	bucket->next = ino_dev_hashtable[i];
	ino_dev_hashtable[i] = bucket;
}

/* Clear statbuf hash table */
void reset_ino_dev_hashtable(void)
{
	int i;
	ino_dev_hashtable_bucket_t *bucket;

	for (i = 0; i < HASH_SIZE; i++) {
		while (ino_dev_hashtable[i] != NULL) {
			bucket = ino_dev_hashtable[i]->next;
			free(ino_dev_hashtable[i]);
			ino_dev_hashtable[i] = bucket;
		}
	}
}

#endif /* BB_CP_MV || BB_DU */

#if defined (BB_CP_MV) || defined (BB_DU) || defined (BB_LN) || defined (BB_AR)
/*
 * Return TRUE if a fileName is a directory.
 * Nonexistant files return FALSE.
 */
int isDirectory(const char *fileName, const int followLinks, struct stat *statBuf)
{
	int status;
	int didMalloc = 0;

	if (statBuf == NULL) {
	    statBuf = (struct stat *)xmalloc(sizeof(struct stat));
	    ++didMalloc;
	}

	if (followLinks == TRUE)
		status = stat(fileName, statBuf);
	else
		status = lstat(fileName, statBuf);

	if (status < 0 || !(S_ISDIR(statBuf->st_mode))) {
	    status = FALSE;
	}
	else status = TRUE;

	if (didMalloc) {
	    free(statBuf);
	    statBuf = NULL;
	}
	return status;
}
#endif

#if defined (BB_CP_MV)
/*
 * Copy one file to another, while possibly preserving its modes, times, and
 * modes.  Returns TRUE if successful, or FALSE on a failure with an error
 * message output.  (Failure is not indicated if attributes cannot be set.)
 * -Erik Andersen
 */
int
copyFile(const char *srcName, const char *destName,
		 int setModes, int followLinks, int forceFlag)
{
	int rfd;
	int wfd;
	int rcc;
	int status;
	char buf[BUF_SIZE];
	struct stat srcStatBuf;
	struct stat dstStatBuf;
	struct utimbuf times;

	if (followLinks == TRUE)
		status = stat(srcName, &srcStatBuf);
	else
		status = lstat(srcName, &srcStatBuf);

	if (status < 0) {
		perror(srcName);
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
		errorMsg("Copying file \"%s\" to itself\n", srcName);
		return FALSE;
	}

	if (S_ISDIR(srcStatBuf.st_mode)) {
		//fprintf(stderr, "copying directory %s to %s\n", srcName, destName);
		/* Make sure the directory is writable */
		status = mkdir(destName, 0777777 ^ umask(0));
		if (status < 0 && errno != EEXIST) {
			perror(destName);
			return FALSE;
		}
	} else if (S_ISLNK(srcStatBuf.st_mode)) {
		char link_val[BUFSIZ + 1];
		int link_size;

		//fprintf(stderr, "copying link %s to %s\n", srcName, destName);
		/* Warning: This could possibly truncate silently, to BUFSIZ chars */
		link_size = readlink(srcName, &link_val[0], BUFSIZ);
		if (link_size < 0) {
			perror(srcName);
			return FALSE;
		}
		link_val[link_size] = '\0';
		status = symlink(link_val, destName);
		if (status < 0) {
			perror(destName);
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
			perror(destName);
			return FALSE;
		}
	} else if (S_ISBLK(srcStatBuf.st_mode) || S_ISCHR(srcStatBuf.st_mode)
			   || S_ISSOCK(srcStatBuf.st_mode)) {
		//fprintf(stderr, "copying soc, blk, or chr %s to %s\n", srcName, destName);
		if (mknod(destName, srcStatBuf.st_mode, srcStatBuf.st_rdev) < 0) {
			perror(destName);
			return FALSE;
		}
	} else if (S_ISREG(srcStatBuf.st_mode)) {
		//fprintf(stderr, "copying regular file %s to %s\n", srcName, destName);
		rfd = open(srcName, O_RDONLY);
		if (rfd < 0) {
			perror(srcName);
			return FALSE;
		}

		wfd =
			open(destName, O_WRONLY | O_CREAT | O_TRUNC,
				 srcStatBuf.st_mode);
		if (wfd < 0) {
			perror(destName);
			close(rfd);
			return FALSE;
		}

		while ((rcc = read(rfd, buf, sizeof(buf))) > 0) {
			if (fullWrite(wfd, buf, rcc) < 0)
				goto error_exit;
		}
		if (rcc < 0) {
			goto error_exit;
		}

		close(rfd);
		if (close(wfd) < 0) {
			return FALSE;
		}
	}

	if (setModes == TRUE) {
		/* This is fine, since symlinks never get here */
		if (chown(destName, srcStatBuf.st_uid, srcStatBuf.st_gid) < 0) {
			perror(destName);
			exit FALSE;
		}
		if (chmod(destName, srcStatBuf.st_mode) < 0) {
			perror(destName);
			exit FALSE;
		}
		times.actime = srcStatBuf.st_atime;
		times.modtime = srcStatBuf.st_mtime;
		if (utime(destName, &times) < 0) {
			perror(destName);
			exit FALSE;
		}
	}

	return TRUE;

  error_exit:
	perror(destName);
	close(rfd);
	close(wfd);

	return FALSE;
}
#endif							/* BB_CP_MV */



#if defined BB_TAR || defined BB_LS

#define TYPEINDEX(mode) (((mode) >> 12) & 0x0f)
#define TYPECHAR(mode)  ("0pcCd?bB-?l?s???" [TYPEINDEX(mode)])

/* The special bits. If set, display SMODE0/1 instead of MODE0/1 */
static const mode_t SBIT[] = {
	0, 0, S_ISUID,
	0, 0, S_ISGID,
	0, 0, S_ISVTX
};

/* The 9 mode bits to test */
static const mode_t MBIT[] = {
	S_IRUSR, S_IWUSR, S_IXUSR,
	S_IRGRP, S_IWGRP, S_IXGRP,
	S_IROTH, S_IWOTH, S_IXOTH
};

#define MODE1  "rwxrwxrwx"
#define MODE0  "---------"
#define SMODE1 "..s..s..t"
#define SMODE0 "..S..S..T"

/*
 * Return the standard ls-like mode string from a file mode.
 * This is static and so is overwritten on each call.
 */
const char *modeString(int mode)
{
	static char buf[12];

	int i;

	buf[0] = TYPECHAR(mode);
	for (i = 0; i < 9; i++) {
		if (mode & SBIT[i])
			buf[i + 1] = (mode & MBIT[i]) ? SMODE1[i] : SMODE0[i];
		else
			buf[i + 1] = (mode & MBIT[i]) ? MODE1[i] : MODE0[i];
	}
	return buf;
}
#endif							/* BB_TAR || BB_LS */


#if defined BB_TAR || defined BB_AR
/*
 * Return the standard ls-like time string from a time_t
 * This is static and so is overwritten on each call.
 */
const char *timeString(time_t timeVal)
{
	time_t now;
	char *str;
	static char buf[26];

	time(&now);

	str = ctime(&timeVal);

	strcpy(buf, &str[4]);
	buf[12] = '\0';

	if ((timeVal > now) || (timeVal < now - 365 * 24 * 60 * 60L)) {
		strcpy(&buf[7], &str[20]);
		buf[11] = '\0';
	}

	return buf;
}
#endif /* BB_TAR || BB_AR */

#if defined BB_TAR || defined BB_CP_MV || defined BB_AR
/*
 * Write all of the supplied buffer out to a file.
 * This does multiple writes as necessary.
 * Returns the amount written, or -1 on an error.
 */
int fullWrite(int fd, const char *buf, int len)
{
	int cc;
	int total;

	total = 0;

	while (len > 0) {
		cc = write(fd, buf, len);

		if (cc < 0)
			return -1;

		buf += cc;
		total += cc;
		len -= cc;
	}

	return total;
}
#endif /* BB_TAR || BB_CP_MV || BB_AR */


#if defined BB_TAR || defined BB_TAIL || defined BB_AR || defined BB_SH
/*
 * Read all of the supplied buffer from a file.
 * This does multiple reads as necessary.
 * Returns the amount read, or -1 on an error.
 * A short read is returned on an end of file.
 */
int fullRead(int fd, char *buf, int len)
{
	int cc;
	int total;

	total = 0;

	while (len > 0) {
		cc = read(fd, buf, len);

		if (cc < 0)
			return -1;

		if (cc == 0)
			break;

		buf += cc;
		total += cc;
		len -= cc;
	}

	return total;
}
#endif /* BB_TAR || BB_TAIL || BB_AR || BB_SH */


#if defined (BB_CHMOD_CHOWN_CHGRP) \
 || defined (BB_CP_MV)			\
 || defined (BB_FIND)			\
 || defined (BB_INSMOD)			\
 || defined (BB_LS)				\
 || defined (BB_RM)				\
 || defined (BB_TAR)

/*
 * Walk down all the directories under the specified 
 * location, and do something (something specified
 * by the fileAction and dirAction function pointers).
 *
 * Unfortunatly, while nftw(3) could replace this and reduce 
 * code size a bit, nftw() wasn't supported before GNU libc 2.1, 
 * and so isn't sufficiently portable to take over since glibc2.1
 * is so stinking huge.
 */
int recursiveAction(const char *fileName,
					int recurse, int followLinks, int depthFirst,
					int (*fileAction) (const char *fileName,
									   struct stat * statbuf,
									   void* userData),
					int (*dirAction) (const char *fileName,
									  struct stat * statbuf,
									  void* userData),
					void* userData)
{
	int status;
	struct stat statbuf;
	struct dirent *next;

	if (followLinks == TRUE)
		status = stat(fileName, &statbuf);
	else
		status = lstat(fileName, &statbuf);

	if (status < 0) {
#ifdef BB_DEBUG_PRINT_SCAFFOLD
		fprintf(stderr,
				"status=%d followLinks=%d TRUE=%d\n",
				status, followLinks, TRUE);
#endif
		perror(fileName);
		return FALSE;
	}

	if ((followLinks == FALSE) && (S_ISLNK(statbuf.st_mode))) {
		if (fileAction == NULL)
			return TRUE;
		else
			return fileAction(fileName, &statbuf, userData);
	}

	if (recurse == FALSE) {
		if (S_ISDIR(statbuf.st_mode)) {
			if (dirAction != NULL)
				return (dirAction(fileName, &statbuf, userData));
			else
				return TRUE;
		}
	}

	if (S_ISDIR(statbuf.st_mode)) {
		DIR *dir;

		dir = opendir(fileName);
		if (!dir) {
			perror(fileName);
			return FALSE;
		}
		if (dirAction != NULL && depthFirst == FALSE) {
			status = dirAction(fileName, &statbuf, userData);
			if (status == FALSE) {
				perror(fileName);
				return FALSE;
			}
		}
		while ((next = readdir(dir)) != NULL) {
			char nextFile[BUFSIZ + 1];

			if ((strcmp(next->d_name, "..") == 0)
				|| (strcmp(next->d_name, ".") == 0)) {
				continue;
			}
			if (strlen(fileName) + strlen(next->d_name) + 1 > BUFSIZ) {
				errorMsg(name_too_long);
				return FALSE;
			}
			memset(nextFile, 0, sizeof(nextFile));
			sprintf(nextFile, "%s/%s", fileName, next->d_name);
			status =
				recursiveAction(nextFile, TRUE, followLinks, depthFirst,
								fileAction, dirAction, userData);
			if (status == FALSE) {
				closedir(dir);
				return FALSE;
			}
		}
		status = closedir(dir);
		if (status < 0) {
			perror(fileName);
			return FALSE;
		}
		if (dirAction != NULL && depthFirst == TRUE) {
			status = dirAction(fileName, &statbuf, userData);
			if (status == FALSE) {
				perror(fileName);
				return FALSE;
			}
		}
	} else {
		if (fileAction == NULL)
			return TRUE;
		else
			return fileAction(fileName, &statbuf, userData);
	}
	return TRUE;
}

#endif							/* BB_CHMOD_CHOWN_CHGRP || BB_CP_MV || BB_FIND || BB_LS || BB_INSMOD */



#if defined (BB_TAR) || defined (BB_MKDIR) || defined (BB_AR)
/*
 * Attempt to create the directories along the specified path, except for
 * the final component.  The mode is given for the final directory only,
 * while all previous ones get default protections.  Errors are not reported
 * here, as failures to restore files can be reported later.
 */
extern int createPath(const char *name, int mode)
{
	char *cp;
	char *cpOld;
	char buf[BUFSIZ + 1];
	int retVal = 0;

	strcpy(buf, name);
	for (cp = buf; *cp == '/'; cp++);
	cp = strchr(cp, '/');
	while (cp) {
		cpOld = cp;
		cp = strchr(cp + 1, '/');
		*cpOld = '\0';
		retVal = mkdir(buf, cp ? 0777 : mode);
		if (retVal != 0 && errno != EEXIST) {
			perror(buf);
			return FALSE;
		}
		*cpOld = '/';
	}
	return TRUE;
}
#endif							/* BB_TAR || BB_MKDIR */



#if defined (BB_CHMOD_CHOWN_CHGRP) || defined (BB_MKDIR) \
 || defined (BB_MKFIFO) || defined (BB_MKNOD)
/* [ugoa]{+|-|=}[rwxst] */



extern int parse_mode(const char *s, mode_t * theMode)
{
	mode_t andMode =

		S_ISVTX | S_ISUID | S_ISGID | S_IRWXU | S_IRWXG | S_IRWXO;
	mode_t orMode = 0;
	mode_t mode = 0;
	mode_t groups = 0;
	char type;
	char c;

	do {
		for (;;) {
			switch (c = *s++) {
			case '\0':
				return -1;
			case 'u':
				groups |= S_ISUID | S_IRWXU;
				continue;
			case 'g':
				groups |= S_ISGID | S_IRWXG;
				continue;
			case 'o':
				groups |= S_IRWXO;
				continue;
			case 'a':
				groups |= S_ISUID | S_ISGID | S_IRWXU | S_IRWXG | S_IRWXO;
				continue;
			case '+':
			case '=':
			case '-':
				type = c;
				if (groups == 0)	/* The default is "all" */
					groups |=
						S_ISUID | S_ISGID | S_IRWXU | S_IRWXG | S_IRWXO;
				break;
			default:
				if (isdigit(c) && c >= '0' && c <= '7' &&
					mode == 0 && groups == 0) {
					*theMode = strtol(--s, NULL, 8);
					return (TRUE);
				} else
					return (FALSE);
			}
			break;
		}

		while ((c = *s++) != '\0') {
			switch (c) {
			case ',':
				break;
			case 'r':
				mode |= S_IRUSR | S_IRGRP | S_IROTH;
				continue;
			case 'w':
				mode |= S_IWUSR | S_IWGRP | S_IWOTH;
				continue;
			case 'x':
				mode |= S_IXUSR | S_IXGRP | S_IXOTH;
				continue;
			case 's':
				mode |= S_IXGRP | S_ISUID | S_ISGID;
				continue;
			case 't':
				mode |= 0;
				continue;
			default:
				*theMode &= andMode;
				*theMode |= orMode;
				return (TRUE);
			}
			break;
		}
		switch (type) {
		case '=':
			andMode &= ~(groups);
			/* fall through */
		case '+':
			orMode |= mode & groups;
			break;
		case '-':
			andMode &= ~(mode & groups);
			orMode &= andMode;
			break;
		}
	} while (c == ',');
	*theMode &= andMode;
	*theMode |= orMode;
	return (TRUE);
}


#endif
/* BB_CHMOD_CHOWN_CHGRP || BB_MKDIR || BB_MKFIFO || BB_MKNOD */





#if defined BB_CHMOD_CHOWN_CHGRP || defined BB_PS || defined BB_LS \
 || defined BB_TAR || defined BB_ID || defined BB_LOGGER \
 || defined BB_LOGNAME || defined BB_WHOAMI

/* This parses entries in /etc/passwd and /etc/group.  This is desirable
 * for BusyBox, since we want to avoid using the glibc NSS stuff, which
 * increases target size and is often not needed or wanted for embedded
 * systems.
 *
 * /etc/passwd entries look like this: 
 *		root:x:0:0:root:/root:/bin/bash
 * and /etc/group entries look like this: 
 *		root:x:0:
 *
 * This uses buf as storage to hold things.
 * 
 */
unsigned long my_getid(const char *filename, char *name, long id, long *gid)
{
	FILE *file;
	char *rname, *start, *end, buf[128];
	long rid;
	long rgid = 0;

	file = fopen(filename, "r");
	if (file == NULL) {
		/* Do not complain.  It is ok for /etc/passwd and
		 * friends to be missing... */
		return (-1);
	}

	while (fgets(buf, 128, file) != NULL) {
		if (buf[0] == '#')
			continue;

		/* username/group name */
		start = buf;
		end = strchr(start, ':');
		if (end == NULL)
			continue;
		*end = '\0';
		rname = start;

		/* password */
		start = end + 1;
		end = strchr(start, ':');
		if (end == NULL)
			continue;

		/* uid in passwd, gid in group */
		start = end + 1;
		rid = (unsigned long) strtol(start, &end, 10);
		if (end == start)
			continue;

		/* gid in passwd */
		start = end + 1;
		rgid = (unsigned long) strtol(start, &end, 10);
		
		if (name) {
			if (0 == strcmp(rname, name)) {
			    if (gid) *gid = rgid;
				fclose(file);
				return (rid);
			}
		}
		if (id != -1 && id == rid) {
			strncpy(name, rname, 8);
			if (gid) *gid = rgid;
			fclose(file);
			return (TRUE);
		}
	}
	fclose(file);
	return (-1);
}

/* returns a uid given a username */
long my_getpwnam(char *name)
{
	return my_getid("/etc/passwd", name, -1, NULL);
}

/* returns a gid given a group name */
long my_getgrnam(char *name)
{
	return my_getid("/etc/group", name, -1, NULL);
}

/* gets a username given a uid */
void my_getpwuid(char *name, long uid)
{
	my_getid("/etc/passwd", name, uid, NULL);
}

/* gets a groupname given a gid */
void my_getgrgid(char *group, long gid)
{
	my_getid("/etc/group", group, gid, NULL);
}

/* gets a gid given a user name */
long my_getpwnamegid(char *name)
{
	long gid;
	my_getid("/etc/passwd", name, -1, &gid);
	return gid;
}

#endif
 /* BB_CHMOD_CHOWN_CHGRP || BB_PS || BB_LS || BB_TAR \
 || BB_ID || BB_LOGGER || BB_LOGNAME || BB_WHOAMI */


#if (defined BB_CHVT) || (defined BB_DEALLOCVT) || (defined BB_SETKEYCODES)

/* From <linux/kd.h> */ 
#define KDGKBTYPE       0x4B33  /* get keyboard type */
#define         KB_84           0x01
#define         KB_101          0x02    /* this is what we always answer */

int is_a_console(int fd)
{
	char arg;

	arg = 0;
	return (ioctl(fd, KDGKBTYPE, &arg) == 0
			&& ((arg == KB_101) || (arg == KB_84)));
}

static int open_a_console(char *fnam)
{
	int fd;

	/* try read-only */
	fd = open(fnam, O_RDWR);

	/* if failed, try read-only */
	if (fd < 0 && errno == EACCES)
		fd = open(fnam, O_RDONLY);

	/* if failed, try write-only */
	if (fd < 0 && errno == EACCES)
		fd = open(fnam, O_WRONLY);

	/* if failed, fail */
	if (fd < 0)
		return -1;

	/* if not a console, fail */
	if (!is_a_console(fd)) {
		close(fd);
		return -1;
	}

	/* success */
	return fd;
}

/*
 * Get an fd for use with kbd/console ioctls.
 * We try several things because opening /dev/console will fail
 * if someone else used X (which does a chown on /dev/console).
 *
 * if tty_name is non-NULL, try this one instead.
 */

int get_console_fd(char *tty_name)
{
	int fd;

	if (tty_name) {
		if (-1 == (fd = open_a_console(tty_name)))
			return -1;
		else
			return fd;
	}

	fd = open_a_console("/dev/tty");
	if (fd >= 0)
		return fd;

	fd = open_a_console("/dev/tty0");
	if (fd >= 0)
		return fd;

	fd = open_a_console("/dev/console");
	if (fd >= 0)
		return fd;

	for (fd = 0; fd < 3; fd++)
		if (is_a_console(fd))
			return fd;

	errorMsg("Couldnt get a file descriptor referring to the console\n");
	return -1;					/* total failure */
}


#endif							/* BB_CHVT || BB_DEALLOCVT || BB_SETKEYCODES */


#if !defined BB_REGEXP && (defined BB_GREP || defined BB_SED)

/* Do a case insensitive strstr() */
char *stristr(char *haystack, const char *needle)
{
	int len = strlen(needle);

	while (*haystack) {
		if (!strncasecmp(haystack, needle, len))
			break;
		haystack++;
	}

	if (!(*haystack))
		haystack = NULL;

	return haystack;
}

/* This tries to find a needle in a haystack, but does so by
 * only trying to match literal strings (look 'ma, no regexps!)
 * This is short, sweet, and carries _very_ little baggage,
 * unlike its beefier cousin in regexp.c
 *  -Erik Andersen
 */
extern int find_match(char *haystack, char *needle, int ignoreCase)
{

	if (ignoreCase == FALSE)
		haystack = strstr(haystack, needle);
	else
		haystack = stristr(haystack, needle);
	if (haystack == NULL)
		return FALSE;
	return TRUE;
}


/* This performs substitutions after a string match has been found.  */
extern int replace_match(char *haystack, char *needle, char *newNeedle,
						 int ignoreCase)
{
	int foundOne = 0;
	char *where, *slider, *slider1, *oldhayStack;

	if (ignoreCase == FALSE)
		where = strstr(haystack, needle);
	else
		where = stristr(haystack, needle);

	if (strcmp(needle, newNeedle) == 0)
		return FALSE;

	oldhayStack = (char *) xmalloc((unsigned) (strlen(haystack)));
	while (where != NULL) {
		foundOne++;
		strcpy(oldhayStack, haystack);
		for (slider = haystack, slider1 = oldhayStack; slider != where;
			 slider++, slider1++);
		*slider = 0;
		haystack = strcat(haystack, newNeedle);
		slider1 += strlen(needle);
		haystack = strcat(haystack, slider1);
		where = strstr(slider, needle);
	}
	free(oldhayStack);

	if (foundOne > 0)
		return TRUE;
	else
		return FALSE;
}

#endif							/* ! BB_REGEXP && (BB_GREP || BB_SED) */


#if defined BB_FIND || defined BB_INSMOD
/*
 * Routine to see if a text string is matched by a wildcard pattern.
 * Returns TRUE if the text is matched, or FALSE if it is not matched
 * or if the pattern is invalid.
 *  *		matches zero or more characters
 *  ?		matches a single character
 *  [abc]	matches 'a', 'b' or 'c'
 *  \c		quotes character c
 * Adapted from code written by Ingo Wilken, and
 * then taken from sash, Copyright (c) 1999 by David I. Bell
 * Permission is granted to use, distribute, or modify this source,
 * provided that this copyright notice remains intact.
 * Permission to distribute this code under the GPL has been granted.
 */
extern int check_wildcard_match(const char *text, const char *pattern)
{
	const char *retryPat;
	const char *retryText;
	int ch;
	int found;
	int len;

	retryPat = NULL;
	retryText = NULL;

	while (*text || *pattern) {
		ch = *pattern++;

		switch (ch) {
		case '*':
			retryPat = pattern;
			retryText = text;
			break;

		case '[':
			found = FALSE;

			while ((ch = *pattern++) != ']') {
				if (ch == '\\')
					ch = *pattern++;

				if (ch == '\0')
					return FALSE;

				if (*text == ch)
					found = TRUE;
			}
			len=strlen(text);
			if (found == FALSE && len!=0) {
				return FALSE;
			}
			if (found == TRUE) {
				if (strlen(pattern)==0 && len==1) {
					return TRUE;
				}
				if (len!=0) {
					text++;
					continue;
				}
			}

			/* fall into next case */

		case '?':
			if (*text++ == '\0')
				return FALSE;

			break;

		case '\\':
			ch = *pattern++;

			if (ch == '\0')
				return FALSE;

			/* fall into next case */

		default:
			if (*text == ch) {
				if (*text)
					text++;
				break;
			}

			if (*text) {
				pattern = retryPat;
				text = ++retryText;
				break;
			}

			return FALSE;
		}

		if (pattern == NULL)
			return FALSE;
	}

	return TRUE;
}
#endif                            /* BB_FIND || BB_INSMOD */




#if defined BB_DF || defined BB_MTAB
/*
 * Given a block device, find the mount table entry if that block device
 * is mounted.
 *
 * Given any other file (or directory), find the mount table entry for its
 * filesystem.
 */
extern struct mntent *findMountPoint(const char *name, const char *table)
{
	struct stat s;
	dev_t mountDevice;
	FILE *mountTable;
	struct mntent *mountEntry;

	if (stat(name, &s) != 0)
		return 0;

	if ((s.st_mode & S_IFMT) == S_IFBLK)
		mountDevice = s.st_rdev;
	else
		mountDevice = s.st_dev;


	if ((mountTable = setmntent(table, "r")) == 0)
		return 0;

	while ((mountEntry = getmntent(mountTable)) != 0) {
		if (strcmp(name, mountEntry->mnt_dir) == 0
			|| strcmp(name, mountEntry->mnt_fsname) == 0)	/* String match. */
			break;
		if (stat(mountEntry->mnt_fsname, &s) == 0 && s.st_rdev == mountDevice)	/* Match the device. */
			break;
		if (stat(mountEntry->mnt_dir, &s) == 0 && s.st_dev == mountDevice)	/* Match the directory's mount point. */
			break;
	}
	endmntent(mountTable);
	return mountEntry;
}
#endif							/* BB_DF || BB_MTAB */



#if defined BB_DD || defined BB_TAIL
/*
 * Read a number with a possible multiplier.
 * Returns -1 if the number format is illegal.
 */
extern long getNum(const char *cp)
{
	long value;

	if (!isDecimal(*cp))
		return -1;

	value = 0;

	while (isDecimal(*cp))
		value = value * 10 + *cp++ - '0';

	switch (*cp++) {
	case 'M':
	case 'm':					/* `tail' uses it traditionally */
		value *= 1048576;
		break;

	case 'k':
		value *= 1024;
		break;

	case 'b':
		value *= 512;
		break;

	case 'w':
		value *= 2;
		break;

	case '\0':
		return value;

	default:
		return -1;
	}

	if (*cp)
		return -1;

	return value;
}
#endif							/* BB_DD || BB_TAIL */


#if defined BB_INIT || defined BB_SYSLOGD || defined BB_AR
/* try to open up the specified device */
extern int device_open(char *device, int mode)
{
	int m, f, fd = -1;

	m = mode | O_NONBLOCK;

	/* Retry up to 5 times */
	for (f = 0; f < 5; f++)
		if ((fd = open(device, m, 0600)) >= 0)
			break;
	if (fd < 0)
		return fd;
	/* Reset original flags. */
	if (m != mode)
		fcntl(fd, F_SETFL, mode);
	return fd;
}
#endif							/* BB_INIT BB_SYSLOGD */


#if defined BB_KILLALL || ( defined BB_FEATURE_LINUXRC && ( defined BB_HALT || defined BB_REBOOT || defined BB_POWEROFF ))
#ifdef BB_FEATURE_USE_DEVPS_PATCH
#include <linux/devps.h> /* For Erik's nifty devps device driver */
#endif

#if defined BB_FEATURE_USE_DEVPS_PATCH
/* findPidByName()
 *  
 *  This finds the pid of the specified process,
 *  by using the /dev/ps device driver.
 *
 *  Returns a list of all matching PIDs
 */
extern pid_t* findPidByName( char* pidName)
{
	int fd, i, j;
	char device[] = "/dev/ps";
	pid_t num_pids;
	pid_t* pid_array = NULL;
	pid_t* pidList=NULL;

	/* open device */ 
	fd = open(device, O_RDONLY);
	if (fd < 0)
		fatalError( "open failed for `%s': %s\n", device, strerror (errno));

	/* Find out how many processes there are */
	if (ioctl (fd, DEVPS_GET_NUM_PIDS, &num_pids)<0) 
		fatalError( "\nDEVPS_GET_PID_LIST: %s\n", strerror (errno));
	
	/* Allocate some memory -- grab a few extras just in case 
	 * some new processes start up while we wait. The kernel will
	 * just ignore any extras if we give it too many, and will trunc.
	 * the list if we give it too few.  */
	pid_array = (pid_t*) calloc( num_pids+10, sizeof(pid_t));
	pid_array[0] = num_pids+10;

	/* Now grab the pid list */
	if (ioctl (fd, DEVPS_GET_PID_LIST, pid_array)<0) 
		fatalError( "\nDEVPS_GET_PID_LIST: %s\n", strerror (errno));

	/* Now search for a match */
	for (i=1, j=0; i<pid_array[0] ; i++) {
		char* p;
		struct pid_info info;

	    info.pid = pid_array[i];
	    if (ioctl (fd, DEVPS_GET_PID_INFO, &info)<0)
			fatalError( "\nDEVPS_GET_PID_INFO: %s\n", strerror (errno));

		/* Make sure we only match on the process name */
		p=info.command_line+1;
		while ((*p != 0) && !isspace(*(p)) && (*(p-1) != '\\')) { 
			(p)++;
		}
		if (isspace(*(p)))
				*p='\0';

		if ((strstr(info.command_line, pidName) != NULL)
				&& (strlen(pidName) == strlen(info.command_line))) {
			pidList=realloc( pidList, sizeof(pid_t) * (j+2));
			if (pidList==NULL)
				fatalError(memory_exhausted);
			pidList[j++]=info.pid;
		}
	}
	if (pidList)
		pidList[j]=0;

	/* Free memory */
	free( pid_array);

	/* close device */
	if (close (fd) != 0) 
		fatalError( "close failed for `%s': %s\n",device, strerror (errno));

	return pidList;
}
#else		/* BB_FEATURE_USE_DEVPS_PATCH */
#if ! defined BB_FEATURE_USE_PROCFS
#error Sorry, I depend on the /proc filesystem right now.
#endif

/* findPidByName()
 *  
 *  This finds the pid of the specified process.
 *  Currently, it's implemented by rummaging through 
 *  the proc filesystem.
 *
 *  Returns a list of all matching PIDs
 */
extern pid_t* findPidByName( char* pidName)
{
	DIR *dir;
	struct dirent *next;
	pid_t* pidList=NULL;
	int i=0;

	dir = opendir("/proc");
	if (!dir)
		fatalError( "Cannot open /proc: %s\n", strerror (errno));
	
	while ((next = readdir(dir)) != NULL) {
		FILE *status;
		char filename[256];
		char buffer[256];
		char* p;

		/* If it isn't a number, we don't want it */
		if (!isdigit(*next->d_name))
			continue;

		/* Now open the status file */
		sprintf(filename, "/proc/%s/status", next->d_name);
		status = fopen(filename, "r");
		if (!status) {
			continue;
		}
		fgets(buffer, 256, status);
		fclose(status);

		/* Make sure we only match on the process name */
		p=buffer+5; /* Skip the name */
		while ((p)++) {
			if (*p==0 || *p=='\n') {
				*p='\0';
				break;
			}
		}
		p=buffer+6; /* Skip the "Name:\t" */

		if ((strstr(p, pidName) != NULL)
				&& (strlen(pidName) == strlen(p))) {
			pidList=realloc( pidList, sizeof(pid_t) * (i+2));
			if (pidList==NULL)
				fatalError(memory_exhausted);
			pidList[i++]=strtol(next->d_name, NULL, 0);
		}
	}
	if (pidList)
		pidList[i]=0;
	return pidList;
}
#endif							/* BB_FEATURE_USE_DEVPS_PATCH */
#endif							/* BB_KILLALL || ( BB_FEATURE_LINUXRC && ( BB_HALT || BB_REBOOT || BB_POWEROFF )) */

#ifndef DMALLOC
/* this should really be farmed out to libbusybox.a */
extern void *xmalloc(size_t size)
{
	void *ptr = malloc(size);

	if (!ptr)
		fatalError(memory_exhausted);
	return ptr;
}

extern void *xrealloc(void *old, size_t size)
{
	void *ptr = realloc(old, size);
	if (!ptr)
		fatalError(memory_exhausted);
	return ptr;
}

extern void *xcalloc(size_t nmemb, size_t size)
{
	void *ptr = calloc(nmemb, size);
	if (!ptr)
		fatalError(memory_exhausted);
	return ptr;
}
#endif

#if defined BB_FEATURE_NFSMOUNT
# ifndef DMALLOC
extern char * xstrdup (const char *s) {
	char *t;

	if (s == NULL)
		return NULL;

	t = strdup (s);

	if (t == NULL)
		fatalError(memory_exhausted);

	return t;
}
# endif

extern char * xstrndup (const char *s, int n) {
	char *t;

	if (s == NULL)
		fatalError("xstrndup bug");

	t = xmalloc(n+1);
	strncpy(t,s,n);
	t[n] = 0;

	return t;
}
#endif


#if (__GLIBC__ < 2) && (defined BB_SYSLOGD || defined BB_INIT)
extern int vdprintf(int d, const char *format, va_list ap)
{
	char buf[BUF_SIZE];
	int len;

	len = vsprintf(buf, format, ap);
	return write(d, buf, len);
}
#endif							/* BB_SYSLOGD */

#if defined BB_FEATURE_MOUNT_LOOP
extern int del_loop(const char *device)
{
	int fd;

	if ((fd = open(device, O_RDONLY)) < 0) {
		perror(device);
		return (FALSE);
	}
	if (ioctl(fd, LOOP_CLR_FD, 0) < 0) {
		perror("ioctl: LOOP_CLR_FD");
		return (FALSE);
	}
	close(fd);
	return (TRUE);
}

extern int set_loop(const char *device, const char *file, int offset,
					int *loopro)
{
	struct loop_info loopinfo;
	int fd, ffd, mode;

	mode = *loopro ? O_RDONLY : O_RDWR;
	if ((ffd = open(file, mode)) < 0 && !*loopro
		&& (errno != EROFS || (ffd = open(file, mode = O_RDONLY)) < 0)) {
		perror(file);
		return 1;
	}
	if ((fd = open(device, mode)) < 0) {
		close(ffd);
		perror(device);
		return 1;
	}
	*loopro = (mode == O_RDONLY);

	memset(&loopinfo, 0, sizeof(loopinfo));
	strncpy(loopinfo.lo_name, file, LO_NAME_SIZE);
	loopinfo.lo_name[LO_NAME_SIZE - 1] = 0;

	loopinfo.lo_offset = offset;

	loopinfo.lo_encrypt_key_size = 0;
	if (ioctl(fd, LOOP_SET_FD, ffd) < 0) {
		perror("ioctl: LOOP_SET_FD");
		close(fd);
		close(ffd);
		return 1;
	}
	if (ioctl(fd, LOOP_SET_STATUS, &loopinfo) < 0) {
		(void) ioctl(fd, LOOP_CLR_FD, 0);
		perror("ioctl: LOOP_SET_STATUS");
		close(fd);
		close(ffd);
		return 1;
	}
	close(fd);
	close(ffd);
	return 0;
}

extern char *find_unused_loop_device(void)
{
	char dev[20];
	int i, fd;
	struct stat statbuf;
	struct loop_info loopinfo;

	for (i = 0; i <= 7; i++) {
		sprintf(dev, "/dev/loop%d", i);
		if (stat(dev, &statbuf) == 0 && S_ISBLK(statbuf.st_mode)) {
			if ((fd = open(dev, O_RDONLY)) >= 0) {
				if (ioctl(fd, LOOP_GET_STATUS, &loopinfo) == -1) {
					if (errno == ENXIO) {	/* probably free */
						close(fd);
						return strdup(dev);
					}
				}
				close(fd);
			}
		}
	}
	return NULL;
}
#endif							/* BB_FEATURE_MOUNT_LOOP */

#if defined BB_MOUNT || defined BB_DF || ( defined BB_UMOUNT && ! defined BB_MTAB)
extern int find_real_root_device_name(char* name)
{
	DIR *dir;
	struct dirent *entry;
	struct stat statBuf, rootStat;
	char fileName[BUFSIZ];

	if (stat("/", &rootStat) != 0) {
		errorMsg("could not stat '/'\n");
		return( FALSE);
	}

	dir = opendir("/dev");
	if (!dir) {
		errorMsg("could not open '/dev'\n");
		return( FALSE);
	}

	while((entry = readdir(dir)) != NULL) {

		/* Must skip ".." since that is "/", and so we 
		 * would get a false positive on ".."  */
		if (strcmp(entry->d_name, "..") == 0)
			continue;

		snprintf( fileName, strlen(name)+1, "/dev/%s", entry->d_name);

		if (stat(fileName, &statBuf) != 0)
			continue;
		/* Some char devices have the same dev_t as block
		 * devices, so make sure this is a block device */
		if (! S_ISBLK(statBuf.st_mode))
			continue;
		if (statBuf.st_rdev == rootStat.st_rdev) {
			strcpy(name, fileName); 
			return ( TRUE);
		}
	}

	return( FALSE);
}
#endif


/* get_line_from_file() - This function reads an entire line from a text file
 * up to a newline. It returns a malloc'ed char * which must be stored and
 * free'ed  by the caller. */
extern char *get_line_from_file(FILE *file)
{
	static const int GROWBY = 80; /* how large we will grow strings by */

	int ch;
	int idx = 0;
	char *linebuf = NULL;
	int linebufsz = 0;

	while (1) {
		ch = fgetc(file);
		if (ch == EOF)
			break;
		/* grow the line buffer as necessary */
		if (idx > linebufsz-2)
			linebuf = realloc(linebuf, linebufsz += GROWBY);
		linebuf[idx++] = (char)ch;
		if ((char)ch == '\n')
			break;
	}

	if (idx == 0)
		return NULL;

	linebuf[idx] = 0;
	return linebuf;
}

#if defined BB_CAT || defined BB_LSMOD
extern void print_file(FILE *file)
{
	int c;

	while ((c = getc(file)) != EOF)
		putc(c, stdout);
	fclose(file);
	fflush(stdout);
}

extern int print_file_by_name(char *filename)
{
	FILE *file;
	file = fopen(filename, "r");
	if (file == NULL) {
		return FALSE;
	}
	print_file(file);
	return TRUE;
}
#endif /* BB_CAT || BB_LSMOD */

#if defined BB_ECHO || defined BB_TR
char process_escape_sequence(char **ptr)
{
	char c;

	switch (c = *(*ptr)++) {
	case 'a':
		c = '\a';
		break;
	case 'b':
		c = '\b';
		break;
	case 'f':
		c = '\f';
		break;
	case 'n':
		c = '\n';
		break;
	case 't':
		c = '\t';
		break;
	case 'v':
		c = '\v';
		break;
	case '\\':
		c = '\\';
		break;
	case '0': case '1': case '2': case '3':
	case '4': case '5': case '6': case '7':
		c -= '0';
		if ('0' <= **ptr && **ptr <= '7') {
			c = c * 8 + (*(*ptr)++ - '0');
			if ('0' <= **ptr && **ptr <= '7')
				c = c * 8 + (*(*ptr)++ - '0');
		}
		break;
	default:
		(*ptr)--;
		c = '\\';
		break;
	}
	return c;
}
#endif

#if defined BB_BASENAME || defined BB_LN
char *get_last_path_component(char *path)
{
	char *s=path+strlen(path)-1;

	/* strip trailing slashes */
	while (s && *s == '/') {
		*s-- = '\0';
	}

	/* find last component */
	s = strrchr(path, '/');
	if (s==NULL) return path;
	else return s+1;
}
#endif

#if defined BB_GREP || defined BB_SED
void xregcomp(regex_t *preg, const char *regex, int cflags)
{
	int ret;
	if ((ret = regcomp(preg, regex, cflags)) != 0) {
		int errmsgsz = regerror(ret, preg, NULL, 0);
		char *errmsg = xmalloc(errmsgsz);
		regerror(ret, preg, errmsg, errmsgsz);
		fatalError("bb_regcomp: %s\n", errmsg);
	}
}
#endif

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
