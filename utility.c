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

#include "utility.h"

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

extern void
name_and_error(const char * name)
{
	fprintf(stderr, "%s: %s\n", name, strerror(errno));
}



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
 * Get the time string to be used for a file.
 * This is down to the minute for new files, but only the date for old files.
 * The string is returned from a static buffer, and so is overwritten for
 * each call.
 */
const char *
timeString(time_t timeVal)
{
	time_t		now;
	char *		str;
	static	char	buf[26];

	time(&now);

	str = ctime(&timeVal);

	strcpy(buf, &str[4]);
	buf[12] = '\0';

	if ((timeVal > now) || (timeVal < now - 365*24*60*60L))
	{
		strcpy(&buf[7], &str[20]);
		buf[11] = '\0';
	}

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
 * Expand the wildcards in a fileName wildcard pattern, if any.
 * Returns an argument list with matching fileNames in sorted order.
 * The expanded names are stored in memory chunks which can later all
 * be freed at once.  The returned list is only valid until the next
 * call or until the next command.  Returns zero if the name is not a
 * wildcard, or returns the count of matched files if the name is a
 * wildcard and there was at least one match, or returns -1 if either
 * no fileNames matched or there was an allocation error.
 */
int
expandWildCards(const char * fileNamePattern, const char *** retFileTable)
{
	const char *	last;
	const char *	cp1;
	const char *	cp2;
	const char *	cp3;
	char *		str;
	DIR *		dirp;
	struct dirent *	dp;
	int		dirLen;
	int		newFileTableSize;
	char **		newFileTable;
	char		dirName[PATH_LEN];

	static int	fileCount;
	static int	fileTableSize;
	static char **	fileTable;

	/*
	 * Clear the return values until we know their final values.
	 */
	fileCount = 0;
	*retFileTable = NULL;

	/*
	 * Scan the file name pattern for any wildcard characters.
	 */
	cp1 = strchr(fileNamePattern, '*');
	cp2 = strchr(fileNamePattern, '?');
	cp3 = strchr(fileNamePattern, '[');

	/*
	 * If there are no wildcard characters then return zero to
	 * indicate that there was actually no wildcard pattern.
	 */
	if ((cp1 == NULL) && (cp2 == NULL) && (cp3 == NULL))
		return 0;

	/*
	 * There are wildcards in the specified filename.
	 * Get the last component of the file name.
	 */
	last = strrchr(fileNamePattern, '/');

	if (last)
		last++;
	else
		last = fileNamePattern;

	/*
	 * If any wildcards were found before the last filename component
	 * then return an error.
	 */
	if ((cp1 && (cp1 < last)) || (cp2 && (cp2 < last)) ||
		(cp3 && (cp3 < last)))
	{
		fprintf(stderr,
		"Wildcards only implemented for last file name component\n");

		return -1;
	}

	/*
	 * Assume at first that we are scanning the current directory.
	 */
	dirName[0] = '.';
	dirName[1] = '\0';

	/*
	 * If there was a directory given as part of the file name then
	 * copy it and null terminate it.
	 */
	if (last != fileNamePattern)
	{
		memcpy(dirName, fileNamePattern, last - fileNamePattern);
		dirName[last - fileNamePattern - 1] = '\0';

		if (dirName[0] == '\0')
		{
			dirName[0] = '/';
			dirName[1] = '\0';
		}
	}

	/*
	 * Open the directory containing the files to be checked.
	 */
	dirp = opendir(dirName);

	if (dirp == NULL)
	{
		perror(dirName);

		return -1;
	}

	/*
	 * Prepare the directory name for use in making full path names.
	 */
	dirLen = strlen(dirName);

	if (last == fileNamePattern)
	{
		dirLen = 0;
		dirName[0] = '\0';
	}
	else if (dirName[dirLen - 1] != '/')
	{
		dirName[dirLen++] = '/';
		dirName[dirLen] = '\0';
	}

	/*
	 * Find all of the files in the directory and check them against
	 * the wildcard pattern.
	 */
	while ((dp = readdir(dirp)) != NULL)
	{
		/*
		 * Skip the current and parent directories.
		 */
		if ((strcmp(dp->d_name, ".") == 0) ||
			(strcmp(dp->d_name, "..") == 0))
		{
			continue;
		}

		/*
		 * If the file name doesn't match the pattern then skip it.
		 */
		if (!match(dp->d_name, last))
			continue;

		/*
		 * This file name is selected.
		 * See if we need to reallocate the file name table.
		 */
		if (fileCount >= fileTableSize)
		{
			/*
			 * Increment the file table size and reallocate it.
			 */
			newFileTableSize = fileTableSize + EXPAND_ALLOC;

			newFileTable = (char **) realloc((char *) fileTable,
				(newFileTableSize * sizeof(char *)));

			if (newFileTable == NULL)
			{
				fprintf(stderr, "Cannot allocate file list\n");
				closedir(dirp);

				return -1;
			}

			fileTable = newFileTable;
			fileTableSize = newFileTableSize;
		}

		/*
		 * Allocate space for storing the file name in a chunk.
		 */
		str = getChunk(dirLen + strlen(dp->d_name) + 1);

		if (str == NULL)
		{
			fprintf(stderr, "No memory for file name\n");
			closedir(dirp);

			return -1;
		}

		/*
		 * Save the file name in the chunk.
		 */
		if (dirLen)
			memcpy(str, dirName, dirLen);

		strcpy(str + dirLen, dp->d_name);

		/*
		 * Save the allocated file name into the file table.
		 */
		fileTable[fileCount++] = str;
	}

	/*
	 * Close the directory and check for any matches.
	 */
	closedir(dirp);

	if (fileCount == 0)
	{
		fprintf(stderr, "No matches\n");

		return -1;
	}

	/*
	 * Sort the list of file names.
	 */
	qsort((void *) fileTable, fileCount, sizeof(char *), nameSort);

	/*
	 * Return the file list and count.
	 */
	*retFileTable = (const char **) fileTable;

	return fileCount;
}


/*
 * Sort routine for list of fileNames.
 */
int
nameSort(const void * p1, const void * p2)
{
	const char **	s1;
	const char **	s2;

	s1 = (const char **) p1;
	s2 = (const char **) p2;

	return strcmp(*s1, *s2);
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
BOOL
match(const char * text, const char * pattern)
{
	const char *	retryPat;
	const char *	retryText;
	int		ch;
	BOOL		found;

	retryPat = NULL;
	retryText = NULL;

	while (*text || *pattern)
	{
		ch = *pattern++;

		switch (ch)
		{
			case '*':  
				retryPat = pattern;
				retryText = text;
				break;

			case '[':  
				found = FALSE;

				while ((ch = *pattern++) != ']')
				{
					if (ch == '\\')
						ch = *pattern++;

					if (ch == '\0')
						return FALSE;

					if (*text == ch)
						found = TRUE;
				}

				if (!found)
				{
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
				if (*text == ch)
				{
					if (*text)
						text++;
					break;
				}

				if (*text)
				{
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
 * Take a command string and break it up into an argc, argv list while
 * handling quoting and wildcards.  The returned argument list and
 * strings are in static memory, and so are overwritten on each call.
 * The argument list is ended with a NULL pointer for convenience.
 * Returns TRUE if successful, or FALSE on an error with a message
 * already output.
 */
BOOL
makeArgs(const char * cmd, int * retArgc, const char *** retArgv)
{
	const char *		argument;
	char *			cp;
	char *			cpOut;
	char *			newStrings;
	const char **		fileTable;
	const char **		newArgTable;
	int			newArgTableSize;
	int			fileCount;
	int			len;
	int			ch;
	int			quote;
	BOOL			quotedWildCards;
	BOOL			unquotedWildCards;

	static int		stringsLength;
	static char *		strings;
	static int		argCount;
	static int		argTableSize;
	static const char **	argTable;

	/*
	 * Clear the returned values until we know them.
	 */
	argCount = 0;
	*retArgc = 0;
	*retArgv = NULL;

	/*
	 * Copy the command string into a buffer that we can modify,
	 * reallocating it if necessary.
	 */
	len = strlen(cmd) + 1;

	if (len > stringsLength)
	{
		newStrings = realloc(strings, len);

		if (newStrings == NULL)
		{
			fprintf(stderr, "Cannot allocate string\n");

			return FALSE;
		}

		strings = newStrings;
		stringsLength = len;
	}

	memcpy(strings, cmd, len);
	cp = strings;

	/*
	 * Keep parsing the command string as long as there are any
	 * arguments left.
	 */
	while (*cp)
	{
		/*
		 * Save the beginning of this argument.
		 */
		argument = cp;
		cpOut = cp;

		/*
		 * Reset quoting and wildcarding for this argument.
		 */
		quote = '\0';
		quotedWildCards = FALSE;
		unquotedWildCards = FALSE;

		/*
		 * Loop over the string collecting the next argument while
		 * looking for quoted strings or quoted characters, and
		 * remembering whether there are any wildcard characters
		 * in the argument.
		 */
		while (*cp)
		{
			ch = *cp++;

			/*
			 * If we are not in a quote and we see a blank then
			 * this argument is done.
			 */
			if (isBlank(ch) && (quote == '\0'))
				break;

			/*
			 * If we see a backslash then accept the next
			 * character no matter what it is.
			 */
			if (ch == '\\')
			{
				ch = *cp++;

				/*
				 * Make sure there is a next character.
				 */
				if (ch == '\0')
				{
					fprintf(stderr,
						"Bad quoted character\n");

					return FALSE;
				}

				/*
				 * Remember whether the quoted character
				 * is a wildcard.
				 */
				if (isWildCard(ch))
					quotedWildCards = TRUE;

				*cpOut++ = ch;

				continue;
			}

			/*
			 * If we see one of the wildcard characters then
			 * remember whether it was seen inside or outside
			 * of quotes.
			 */
			if (isWildCard(ch))
			{
				if (quote)
					quotedWildCards = TRUE;
				else
					unquotedWildCards = TRUE;
			}

			/*
			 * If we were in a quote and we saw the same quote
			 * character again then the quote is done.
			 */
			if (ch == quote)
			{
				quote = '\0';

				continue;
			}

			/*
			 * If we weren't in a quote and we see either type
			 * of quote character, then remember that we are
			 * now inside of a quote.
			 */
			if ((quote == '\0') && ((ch == '\'') || (ch == '"')))
			{
				quote = ch;

				continue;
			}

			/*
			 * Store the character.
			 */
			*cpOut++ = ch;
		}

		/*
		 * Make sure that quoting is terminated properly.
		 */
		if (quote)
		{
			fprintf(stderr, "Unmatched quote character\n");

			return FALSE;
		}

		/*
		 * Null terminate the argument if it had shrunk, and then
		 * skip over all blanks to the next argument, nulling them
		 * out too.
		 */
		if (cp != cpOut)
			*cpOut = '\0';

		while (isBlank(*cp))
 			*cp++ = '\0';

		/*
		 * If both quoted and unquoted wildcards were used then
		 * complain since we don't handle them properly.
		 */
		if (quotedWildCards && unquotedWildCards)
		{
			fprintf(stderr,
				"Cannot use quoted and unquoted wildcards\n");

			return FALSE;
		}

		/*
		 * Expand the argument into the matching filenames or accept
		 * it as is depending on whether there were any unquoted
		 * wildcard characters in it.
		 */
		if (unquotedWildCards)
		{
			/*
			 * Expand the argument into the matching filenames.
			 */
			fileCount = expandWildCards(argument, &fileTable);

			/*
			 * Return an error if the wildcards failed to match.
			 */
			if (fileCount < 0)
				return FALSE;

			if (fileCount == 0)
			{
				fprintf(stderr, "Wildcard expansion error\n");

				return FALSE;
			}
		}
		else
		{
			/*
			 * Set up to only store the argument itself.
			 */
			fileTable = &argument;
			fileCount = 1;
		}

		/*
		 * Now reallocate the argument table to hold the file name.
		 */
		if (argCount + fileCount >= argTableSize)
		{
			newArgTableSize = argCount + fileCount + 1;

			newArgTable = (const char **) realloc(argTable,
				(sizeof(const char *) * newArgTableSize));

			if (newArgTable == NULL)
			{
				fprintf(stderr, "No memory for arg list\n");

				return FALSE;
			}

			argTable = newArgTable;
			argTableSize = newArgTableSize;
		}

		/*
		 * Copy the new arguments to the end of the old ones.
		 */
		memcpy((void *) &argTable[argCount], (const void *) fileTable,
			(sizeof(const char **) * fileCount));

		/*
		 * Add to the argument count.
		 */
		argCount += fileCount;
	}

	/*
	 * Null terminate the argument list and return it.
	 */
	argTable[argCount] = NULL;

	*retArgc = argCount;
	*retArgv = argTable;

 	return TRUE;
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
 * Read all of the supplied buffer from a file.
 * This does multiple reads as necessary.
 * Returns the amount read, or -1 on an error.
 * A short read is returned on an end of file.
 */
int
recursive( const char *fileName, BOOL followLinks, const char* pattern, 
	int (*fileAction)(const char* fileName, const struct stat* statbuf), 
	int (*dirAction)(const char* fileName, const struct stat* statbuf))
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
	return( -1);
    }

    if (S_ISREG(statbuf.st_mode)) {
	if (match(fileName, pattern)) {
	    if (fileAction==NULL)
		fprintf( stdout, "%s\n", fileName);
	    else
		return(fileAction(fileName, &statbuf));
	}
    }
    else if (S_ISDIR(statbuf.st_mode)) {
	if (dirAction==NULL) {
	    DIR *dir;
	    if (! match(fileName, pattern))
		return 1;
	    dir = opendir(fileName);
	    if (!dir) {
		perror(fileName);
		return( -1);
	    }
	    while ((next = readdir (dir)) != NULL) {
		    status = recursive(fileName, followLinks, pattern, fileAction, dirAction);
		    if (status < 0) {
			closedir(dir);
			return(status);
		    }
	    }
	    status = closedir (dir);
	    if (status < 0) {
		perror(fileName);
		return( -1);
	    }
	}
	else
	    return(dirAction(fileName, &statbuf));
    }
    return( 1);

}



/* END CODE */
