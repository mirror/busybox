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
//#include <sys/types.h>
//#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <utime.h>

#if 0

extern char *
join_paths(char * buffer, const char * a, const char * b)
{
	int	length = 0;

	if ( a && *a ) {
		length = strlen(a);
		memcpy(buffer, a, length);
	}
	if ( b && *b ) {
		if ( length > 0 && buffer[length - 1] != '/' )
			buffer[length++] = '/';
		if ( *b == '/' )
			b++;
		strcpy(&buffer[length], b);
	}
	return buffer;
}

#endif






static	CHUNK *	chunkList;


/*
 * Return the standard ls-like mode string from a file mode.
 * This is static and so is overwritten on each call.
 */
const char *
modeString(int mode)
{
	static	char	buf[12];

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
#ifdef	S_ISLNK
	if (S_ISLNK(mode))
		buf[0] = 'l';
#endif
#ifdef	S_ISSOCK
	if (S_ISSOCK(mode))
		buf[0] = 's';
#endif

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
 * Return TRUE if a fileName is a directory.
 * Nonexistant files return FALSE.
 */
BOOL
isDirectory(const char * name)
{
	struct	stat	statBuf;

	if (stat(name, &statBuf) < 0)
		return FALSE;

	return S_ISDIR(statBuf.st_mode);
}


/*
 * Return TRUE if a filename is a block or character device.
 * Nonexistant files return FALSE.
 */
BOOL
isDevice(const char * name)
{
	struct	stat	statBuf;

	if (stat(name, &statBuf) < 0)
		return FALSE;

	return S_ISBLK(statBuf.st_mode) || S_ISCHR(statBuf.st_mode);
}


/*
 * Copy one file to another, while possibly preserving its modes, times,
 * and modes.  Returns TRUE if successful, or FALSE on a failure with an
 * error message output.  (Failure is not indicted if the attributes cannot
 * be set.)
 */
BOOL
copyFile(
	const char *	srcName,
	const char *	destName,
	BOOL		setModes
)
{
	int		rfd;
	int		wfd;
	int		rcc;
	char		buf[BUF_SIZE];
	struct	stat	statBuf1;
	struct	stat	statBuf2;
	struct	utimbuf	times;
	
	if (stat(srcName, &statBuf1) < 0)
	{
		perror(srcName);

		return FALSE;
	}

	if (stat(destName, &statBuf2) < 0)
	{
		statBuf2.st_ino = -1;
		statBuf2.st_dev = -1;
	}

	if ((statBuf1.st_dev == statBuf2.st_dev) &&
		(statBuf1.st_ino == statBuf2.st_ino))
	{
		fprintf(stderr, "Copying file \"%s\" to itself\n", srcName);

		return FALSE;
	}

	rfd = open(srcName, O_RDONLY);

	if (rfd < 0)
	{
		perror(srcName);

		return FALSE;
	}

	wfd = creat(destName, statBuf1.st_mode);

	if (wfd < 0)
	{
		perror(destName);
		close(rfd);

		return FALSE;
	}

	while ((rcc = read(rfd, buf, sizeof(buf))) > 0)
	{
		if (fullWrite(wfd, buf, rcc) < 0)
			goto error_exit;
	}

	if (rcc < 0)
	{
		perror(srcName);
		goto error_exit;
	}

	(void) close(rfd);

	if (close(wfd) < 0)
	{
		perror(destName);

		return FALSE;
	}

	if (setModes)
	{
		(void) chmod(destName, statBuf1.st_mode);

		(void) chown(destName, statBuf1.st_uid, statBuf1.st_gid);

		times.actime = statBuf1.st_atime;
		times.modtime = statBuf1.st_mtime;

		(void) utime(destName, &times);
	}

	return TRUE;


error_exit:
	close(rfd);
	close(wfd);

	return FALSE;
}


/*
 * Build a path name from the specified directory name and file name.
 * If the directory name is NULL, then the original fileName is returned.
 * The built path is in a static area, and is overwritten for each call.
 */
const char *
buildName(const char * dirName, const char * fileName)
{
	const char *	cp;
	static	char	buf[PATH_LEN];

	if ((dirName == NULL) || (*dirName == '\0'))
		return fileName;

	cp = strrchr(fileName, '/');

	if (cp)
		fileName = cp + 1;

	strcpy(buf, dirName);
	strcat(buf, "/");
	strcat(buf, fileName);

	return buf;
}



/*
 * Make a NULL-terminated string out of an argc, argv pair.
 * Returns TRUE if successful, or FALSE if the string is too long,
 * with an error message given.  This does not handle spaces within
 * arguments correctly.
 */
BOOL
makeString(
	int		argc,
	const char **	argv,
	char *		buf,
	int		bufLen
)
{
	int	len;

	while (argc-- > 0)
	{
		len = strlen(*argv);

		if (len >= bufLen)
		{
			fprintf(stderr, "Argument string too long\n");

			return FALSE;
		}

		strcpy(buf, *argv++);

		buf += len;
		bufLen -= len;

		if (argc)
			*buf++ = ' ';

		bufLen--; 
	}

	*buf = '\0';

	return TRUE;
}


/*
 * Allocate a chunk of memory (like malloc).
 * The difference, though, is that the memory allocated is put on a
 * list of chunks which can be freed all at one time.  You CAN NOT free
 * an individual chunk.
 */
char *
getChunk(int size)
{
	CHUNK *	chunk;

	if (size < CHUNK_INIT_SIZE)
		size = CHUNK_INIT_SIZE;

	chunk = (CHUNK *) malloc(size + sizeof(CHUNK) - CHUNK_INIT_SIZE);

	if (chunk == NULL)
		return NULL;

	chunk->next = chunkList;
	chunkList = chunk;

	return chunk->data;
}


/*
 * Duplicate a string value using the chunk allocator.
 * The returned string cannot be individually freed, but can only be freed
 * with other strings when freeChunks is called.  Returns NULL on failure.
 */
char *
chunkstrdup(const char * str)
{
	int	len;
	char *	newStr;

	len = strlen(str) + 1;
	newStr = getChunk(len);

	if (newStr)
		memcpy(newStr, str, len);

	return newStr;
}


/*
 * Free all chunks of memory that had been allocated since the last
 * call to this routine.
 */
void
freeChunks(void)
{
	CHUNK *	chunk;

	while (chunkList)
	{
		chunk = chunkList;
		chunkList = chunk->next;
		free((char *) chunk);
	}
}


/*
 * Write all of the supplied buffer out to a file.
 * This does multiple writes as necessary.
 * Returns the amount written, or -1 on an error.
 */
int
fullWrite(int fd, const char * buf, int len)
{
	int	cc;
	int	total;

	total = 0;

	while (len > 0)
	{
		cc = write(fd, buf, len);

		if (cc < 0)
			return -1;

		buf += cc;
		total+= cc;
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
int
fullRead(int fd, char * buf, int len)
{
	int	cc;
	int	total;

	total = 0;

	while (len > 0)
	{
		cc = read(fd, buf, len);

		if (cc < 0)
			return -1;

		if (cc == 0)
			break;

		buf += cc;
		total+= cc;
		len -= cc;
	}

	return total;
}


/*
 * Walk down all the directories under the specified 
 * location, and do something (something specified
 * by the fileAction and dirAction function pointers).
 */
int
recursiveAction( const char *fileName, BOOL followLinks,
	int (*fileAction)(const char* fileName), 
	int (*dirAction)(const char* fileName))
{
    int             status;
    struct stat     statbuf;
    struct dirent*  next;

    if (followLinks)
	status = stat(fileName, &statbuf);
    else
	status = lstat(fileName, &statbuf);

    if (status < 0) {
	perror(fileName);
	return( FALSE);
    }

    if (S_ISDIR(statbuf.st_mode)) {
	DIR *dir;
	dir = opendir(fileName);
	if (!dir) {
	    perror(fileName);
	    return(FALSE);
	}
	while ((next = readdir (dir)) != NULL) {
		char nextFile[NAME_MAX];
		if ( (strcmp(next->d_name, "..") == 0) || (strcmp(next->d_name, ".") == 0) ) {
		    continue;
		}
		sprintf(nextFile, "%s/%s", fileName, next->d_name);
		status = recursiveAction(nextFile, followLinks, fileAction, dirAction);
		if (status < 0) {
		    closedir(dir);
		    return(FALSE);
		}
	}
	status = closedir (dir);
	if (status < 0) {
	    perror(fileName);
	    return( FALSE);
	}
	if (dirAction==NULL)
	    return(TRUE);
	else
	    return(dirAction(fileName));
    }
    else {
	if (fileAction==NULL)
	    return(TRUE);
	else
	    return(fileAction(fileName));
    }
}



/* END CODE */
