/*
 * Utility routines.
 *
 * Copyright (C) 1998 by Erik Andersen <andersee@debian.org>
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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <utime.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

/* volatile so gcc knows this is the enod of the line */
volatile void usage(const char *usage)
{
    fprintf(stderr, "Usage: %s\n", usage);
    exit(FALSE);
}



#if defined (BB_CP) || defined (BB_MV)
/*
 * Return TRUE if a fileName is a directory.
 * Nonexistant files return FALSE.
 */
int isDirectory(const char *name)
{
    struct stat statBuf;

    if (stat(name, &statBuf) < 0)
	return FALSE;
    if (S_ISDIR(statBuf.st_mode))
	return TRUE;
    return(FALSE);
}


/*
 * Copy one file to another, while possibly preserving its modes, times,
 * and modes.  Returns TRUE if successful, or FALSE on a failure with an
 * error message output.  (Failure is not indicted if the attributes cannot
 * be set.)
 */
int
copyFile( const char *srcName, const char *destName, 
	 int setModes, int followLinks)
{
    int rfd;
    int wfd;
    int rcc;
    int result;
    char buf[BUF_SIZE];
    struct stat srcStatBuf;
    struct stat dstStatBuf;
    struct utimbuf times;

    if (followLinks == FALSE)
	result = stat(srcName, &srcStatBuf);
    else 
	result = lstat(srcName, &srcStatBuf);
    if (result < 0) {
	perror(srcName);
	return FALSE;
    }

    if (followLinks == FALSE)
	result = stat(destName, &dstStatBuf);
    else 
	result = lstat(destName, &dstStatBuf);
    if (result < 0) {
	dstStatBuf.st_ino = -1;
	dstStatBuf.st_dev = -1;
    }

    if ((srcStatBuf.st_dev == dstStatBuf.st_dev) &&
	(srcStatBuf.st_ino == dstStatBuf.st_ino)) {
	fprintf(stderr, "Copying file \"%s\" to itself\n", srcName);
	return FALSE;
    }

    if (S_ISDIR(srcStatBuf.st_mode)) {
	//fprintf(stderr, "copying directory %s to %s\n", srcName, destName);
	/* Make sure the directory is writable */
	if (mkdir(destName, 0777777 ^ umask(0))) {
	    perror(destName);
	    return (FALSE);
	}
    } else if (S_ISLNK(srcStatBuf.st_mode)) {
	char *link_val;
	int link_size;

	//fprintf(stderr, "copying link %s to %s\n", srcName, destName);
	link_val = (char *) alloca(PATH_MAX + 2);
	link_size = readlink(srcName, link_val, PATH_MAX + 1);
	if (link_size < 0) {
	    perror(srcName);
	    return (FALSE);
	}
	link_val[link_size] = '\0';
	link_size = symlink(link_val, destName);
	if (link_size != 0) {
	    perror(destName);
	    return (FALSE);
	}
    } else if (S_ISFIFO(srcStatBuf.st_mode)) {
	//fprintf(stderr, "copying fifo %s to %s\n", srcName, destName);
	if (mkfifo(destName, 644)) {
	    perror(destName);
	    return (FALSE);
	}
    } else if (S_ISBLK(srcStatBuf.st_mode) || S_ISCHR(srcStatBuf.st_mode) 
	    || S_ISSOCK (srcStatBuf.st_mode)) {
	//fprintf(stderr, "copying soc, blk, or chr %s to %s\n", srcName, destName);
	if (mknod(destName, srcStatBuf.st_mode, srcStatBuf.st_rdev)) {
	    perror(destName);
	    return (FALSE);
	}
    } else if (S_ISREG(srcStatBuf.st_mode)) {
	//fprintf(stderr, "copying regular file %s to %s\n", srcName, destName);
	rfd = open(srcName, O_RDONLY);
	if (rfd < 0) {
	    perror(srcName);
	    return FALSE;
	}

	wfd = creat(destName, srcStatBuf.st_mode);
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
	//fprintf(stderr, "Setting permissions for %s\n", destName);
	chmod(destName, srcStatBuf.st_mode);
	if (followLinks == TRUE)
	    chown(destName, srcStatBuf.st_uid, srcStatBuf.st_gid);
	else
	    lchown(destName, srcStatBuf.st_uid, srcStatBuf.st_gid);

	times.actime = srcStatBuf.st_atime;
	times.modtime = srcStatBuf.st_mtime;

	utime(destName, &times);
    }

    return TRUE;


  error_exit:
    perror(destName);
    close(rfd);
    close(wfd);

    return FALSE;
}
#endif



#ifdef BB_TAR
/*
 * Return the standard ls-like mode string from a file mode.
 * This is static and so is overwritten on each call.
 */
const char *modeString(int mode)
{
    static char buf[12];

    strcpy(buf, "----------");

    /*
     * Fill in the file type.
     */
    if (S_ISDIR(mode))
	buf[0] = 'd';
    if (S_ISCHR(mode))
	buf[0] = 'c';
    if (S_ISBLK(mode))
	buf[0] = 'b';
    if (S_ISFIFO(mode))
	buf[0] = 'p';
    if (S_ISLNK(mode))
	buf[0] = 'l';
    if (S_ISSOCK(mode))
	buf[0] = 's';
    /*
     * Now fill in the normal file permissions.
     */
    if (mode & S_IRUSR)
	buf[1] = 'r';
    if (mode & S_IWUSR)
	buf[2] = 'w';
    if (mode & S_IXUSR)
	buf[3] = 'x';
    if (mode & S_IRGRP)
	buf[4] = 'r';
    if (mode & S_IWGRP)
	buf[5] = 'w';
    if (mode & S_IXGRP)
	buf[6] = 'x';
    if (mode & S_IROTH)
	buf[7] = 'r';
    if (mode & S_IWOTH)
	buf[8] = 'w';
    if (mode & S_IXOTH)
	buf[9] = 'x';

    /*
     * Finally fill in magic stuff like suid and sticky text.
     */
    if (mode & S_ISUID)
	buf[3] = ((mode & S_IXUSR) ? 's' : 'S');
    if (mode & S_ISGID)
	buf[6] = ((mode & S_IXGRP) ? 's' : 'S');
    if (mode & S_ISVTX)
	buf[9] = ((mode & S_IXOTH) ? 't' : 'T');

    return buf;
}


/*
 * Get the time string to be used for a file.
 * This is down to the minute for new files, but only the date for old files.
 * The string is returned from a static buffer, and so is overwritten for
 * each call.
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


/*
 * Routine to see if a text string is matched by a wildcard pattern.
 * Returns TRUE if the text is matched, or FALSE if it is not matched
 * or if the pattern is invalid.
 *  *		matches zero or more characters
 *  ?		matches a single character
 *  [abc]	matches 'a', 'b' or 'c'
 *  \c		quotes character c
 *  Adapted from code written by Ingo Wilken.
 */
int match(const char *text, const char *pattern)
{
    const char *retryPat;
    const char *retryText;
    int ch;
    int found;

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

	    if (!found) {
		pattern = retryPat;
		text = ++retryText;
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
#endif


#if defined (BB_CHOWN) || defined (BB_CP) || defined (BB_FIND) || defined (BB_LS)
/*
 * Walk down all the directories under the specified 
 * location, and do something (something specified
 * by the fileAction and dirAction function pointers).
 *
 * TODO: check if ftw(3) can replace this to reduce code size...
 */
int
recursiveAction(const char *fileName, int recurse, int followLinks, int delayDirAction,
		int (*fileAction) (const char *fileName, struct stat* statbuf),
		int (*dirAction) (const char *fileName, struct stat* statbuf))
{
    int status;
    struct stat statbuf;
    struct dirent *next;

    if (followLinks == FALSE)
	status = stat(fileName, &statbuf);
    else
	status = lstat(fileName, &statbuf);

    if (status < 0) {
	perror(fileName);
	return (FALSE);
    }

    if (recurse == FALSE) {
	if (S_ISDIR(statbuf.st_mode)) {
	    if (dirAction != NULL)
		return (dirAction(fileName, &statbuf));
	    else
		return (TRUE);
	} 
    }

    if (S_ISDIR(statbuf.st_mode)) {
	DIR *dir;
	dir = opendir(fileName);
	if (!dir) {
	    perror(fileName);
	    return (FALSE);
	}
	if (dirAction != NULL && delayDirAction == FALSE) {
	    status = dirAction(fileName, &statbuf);
	    if (status == FALSE) {
		perror(fileName);
		return (FALSE);
	    }
	}
	while ((next = readdir(dir)) != NULL) {
	    char nextFile[NAME_MAX];
	    if ((strcmp(next->d_name, "..") == 0)
		|| (strcmp(next->d_name, ".") == 0)) {
		continue;
	    }
	    sprintf(nextFile, "%s/%s", fileName, next->d_name);
	    status =
		recursiveAction(nextFile, TRUE, followLinks, delayDirAction, 
			fileAction, dirAction);
	    if (status < 0) {
		closedir(dir);
		return (FALSE);
	    }
	}
	status = closedir(dir);
	if (status < 0) {
	    perror(fileName);
	    return (FALSE);
	}
	if (dirAction != NULL && delayDirAction == TRUE) {
	    status = dirAction(fileName, &statbuf);
	    if (status == FALSE) {
		perror(fileName);
		return (FALSE);
	    }
	}
    } else {
	if (fileAction == NULL)
	    return (TRUE);
	else
	    return (fileAction(fileName, &statbuf));
    }
    return (TRUE);
}

#endif



#if defined (BB_TAR) || defined (BB_MKDIR)
/*
 * Attempt to create the directories along the specified path, except for
 * the final component.  The mode is given for the final directory only,
 * while all previous ones get default protections.  Errors are not reported
 * here, as failures to restore files can be reported later.
 */
extern void createPath (const char *name, int mode)
{
    char *cp;
    char *cpOld;
    char buf[NAME_MAX];

    strcpy (buf, name);

    cp = strchr (buf, '/');

    while (cp) {
	cpOld = cp;
	cp = strchr (cp + 1, '/');

	*cpOld = '\0';

	if (mkdir (buf, cp ? 0777 : mode) == 0)
	    printf ("Directory \"%s\" created\n", buf);

	*cpOld = '/';
    }
}
#endif



#if defined (BB_CHMOD_CHOWN_CHGRP) || defined (BB_MKDIR)
/* [ugoa]{+|-|=}[rwxst] */



extern int 
parse_mode( const char* s, mode_t* theMode)
{
	mode_t andMode = S_ISVTX|S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO;
	mode_t orMode = 0;
	mode_t	mode = 0;
	mode_t	groups = 0;
	char	type;
	char	c;

	do {
		for ( ; ; ) {
			switch ( c = *s++ ) {
			case '\0':
				return -1;
			case 'u':
				groups |= S_ISUID|S_IRWXU;
				continue;
			case 'g':
				groups |= S_ISGID|S_IRWXG;
				continue;
			case 'o':
				groups |= S_IRWXO;
				continue;
			case 'a':
				groups |= S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO;
				continue;
			case '+':
			case '=':
			case '-':
				type = c;
				if ( groups == 0 ) /* The default is "all" */
					groups |= S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO;
				break;
			default:
				if ( isdigit(c) && c >= '0' && c <= '7' && mode == 0 && groups == 0 ) {
					andMode = 0;
					orMode = strtol(--s, NULL, 8);
					*theMode &= andMode;
					*theMode |= orMode;
					return (TRUE);
				}
				else
					return (FALSE);
			}
			break;
		}

		while ( (c = *s++) != '\0' ) {
			switch ( c ) {
			case ',':
				break;
			case 'r':
				mode |= S_IRUSR|S_IRGRP|S_IROTH;
				continue;
			case 'w':
				mode |= S_IWUSR|S_IWGRP|S_IWOTH;
				continue;
			case 'x':
				mode |= S_IXUSR|S_IXGRP|S_IXOTH;
				continue;
			case 's':
				mode |= S_IXGRP|S_ISUID|S_ISGID;
				continue;
			case 't':
				mode |= 0;
				continue;
			default:
				*theMode &= andMode;
				*theMode |= orMode;
				return( TRUE);
			}
			break;
		}
		switch ( type ) {
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
	} while ( c == ',' );
	*theMode &= andMode;
	*theMode |= orMode;
	return (TRUE);
}


#endif



/* END CODE */
