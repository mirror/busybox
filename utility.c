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

#if defined BB_MOUNT || defined BB_UMOUNT || defined BB_DF
#  if defined BB_FEATURE_USE_PROCFS
const char mtab_file[] = "/proc/mounts";
#  else
#    if defined BB_MTAB
const char mtab_file[] = "/etc/mtab";
#    else
#      error With (BB_MOUNT||BB_UMOUNT||BB_DF) defined, you must define either BB_MTAB or BB_FEATURE_USE_PROCFS
#    endif
#  endif
#endif


extern void usage(const char *usage)
{
    fprintf(stderr, "BusyBox v%s (%s) multi-call binary -- GPL2\n\n", BB_VER, BB_BT);
    fprintf(stderr, "Usage: %s\n", usage);
    exit(FALSE);
}


#if defined (BB_INIT) || defined (BB_PS)

#if ! defined BB_FEATURE_USE_PROCFS
#error Sorry, I depend on the /proc filesystem right now.
#endif
/* Returns kernel version encoded as major*65536 + minor*256 + patch,
 * so, for example,  to check if the kernel is greater than 2.2.11:
 *	if (get_kernel_revision() <= 2*65536+2*256+11) { <stuff> }
 */
int
get_kernel_revision()
{
  FILE *file;
  int major=0, minor=0, patch=0;
  char* filename="/proc/sys/kernel/osrelease";

  file = fopen(filename,"r");
  if (file == NULL) {
    /* bummer, /proc must not be mounted... */
    return( 0);
  }
  fscanf(file,"%d.%d.%d",&major,&minor,&patch);
  fclose(file);
  return major*65536 + minor*256 + patch;
}

#endif



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
 *  -Erik Andersen
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
	result = mkdir(destName, 0777777 ^ umask(0));
	if (result < 0 && errno != EEXIST) {
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
#if (__GLIBC__ >= 2) && (__GLIBC_MINOR__ >= 1)
	if (followLinks == FALSE)
	    lchown(destName, srcStatBuf.st_uid, srcStatBuf.st_gid);
	else
#endif
	    chown(destName, srcStatBuf.st_uid, srcStatBuf.st_gid);

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
    for (i=0; i<9; i++) {
	if (mode & SBIT[i])
	    buf[i+1] = (mode & MBIT[i])? 
		SMODE1[i] : SMODE0[i];
	else
	    buf[i+1] = (mode & MBIT[i])? 
		MODE1[i] : MODE0[i];
    }
    return buf;
}
#endif


#if defined BB_TAR
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
#endif

#if defined BB_TAR || defined BB_CP || defined BB_MV
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
#endif


#if defined BB_TAR || defined BB_TAIL
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


#if defined (BB_CHMOD_CHOWN_CHGRP) || defined (BB_CP) || defined (BB_FIND) || defined (BB_LS) || defined (BB_INSMOD)
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
int
recursiveAction(const char *fileName, int recurse, int followLinks, int depthFirst,
		int (*fileAction) (const char *fileName, struct stat* statbuf),
		int (*dirAction) (const char *fileName, struct stat* statbuf))
{
    int status;
    struct stat statbuf, statbuf1;
    struct dirent *next;

    if (followLinks == TRUE)
	status = stat(fileName, &statbuf);
    else
	status = lstat(fileName, &statbuf);

    status = lstat(fileName, &statbuf);
    if (status < 0) {
	perror(fileName);
	return (FALSE);
    }

    if ( (followLinks == FALSE) && (S_ISLNK(statbuf.st_mode)) ) {
	if (fileAction == NULL)
	    return (TRUE);
	else
	    return (fileAction(fileName, &statbuf));
    }

    if (recurse == FALSE) {
	if (S_ISDIR(statbuf.st_mode)) {
	    if (dirAction != NULL)
		return (dirAction(fileName, &statbuf));
	    else
		return (TRUE);
	} 
    }
    
    status = lstat(fileName, &statbuf1);
    if (status < 0) {
	perror(fileName);
	return (FALSE);
    }

    if (S_ISDIR(statbuf.st_mode) && S_ISDIR(statbuf1.st_mode)) {
	DIR *dir;
	dir = opendir(fileName);
	if (!dir) {
	    perror(fileName);
	    return (FALSE);
	}
	if (dirAction != NULL && depthFirst == FALSE) {
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
		recursiveAction(nextFile, TRUE, followLinks, depthFirst, 
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
	if (dirAction != NULL && depthFirst == TRUE) {
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
extern int createPath (const char *name, int mode)
{
    char *cp;
    char *cpOld;
    char buf[NAME_MAX];
    int retVal=0;

    strcpy( buf, name);
    cp = strchr (buf, '/');
    while (cp) {
	cpOld = cp;
	cp = strchr (cp + 1, '/');
	*cpOld = '\0';
	retVal = mkdir (buf, cp ? 0777 : mode);
	*cpOld = '/';
    }
    /* Return the result from the final directory, as that
     * is the one that counts */
    if( retVal!=0) {
	if ( errno!=EEXIST) {
	    return( FALSE);
	}
    }
    return( TRUE);
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
				if ( isdigit(c) && c >= '0' && c <= '7' && 
						mode == 0 && groups == 0 ) {
					*theMode = strtol(--s, NULL, 8);
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







#if defined (BB_CHMOD_CHOWN_CHGRP) || defined (BB_PS)

/* Use this to avoid needing the glibc NSS stuff 
 * This uses storage buf to hold things.
 * */
uid_t 
my_getid(const char *filename, char *name, uid_t id) 
{
	FILE *file;
	char *rname, *start, *end, buf[128];
	uid_t rid;

	file=fopen(filename,"r");
	if (file == NULL) {
	    perror(filename);
	    return (-1);
	}

	while (fgets (buf, 128, file) != NULL) {
		if (buf[0] == '#')
			continue;

		start = buf;
		end = strchr (start, ':');
		if (end == NULL)
			continue;
		*end = '\0';
		rname = start;

		start = end + 1;
		end = strchr (start, ':');
		if (end == NULL)
			continue;

		start = end + 1;
		rid = (uid_t) strtol (start, &end, 10);
		if (end == start)
			continue;

		if (name) {
		    if (0 == strcmp(rname, name)) {
			fclose( file);
			return( rid);
		    }
                }
		if ( id != -1 && id == rid ) {
		    strncpy(name, rname, 8);
		    fclose( file);
		    return( TRUE);
		}
	}
	fclose(file);
	return (-1);
}

uid_t 
my_getpwnam(char *name) 
{
    return my_getid("/etc/passwd", name, -1);
}

gid_t 
my_getgrnam(char *name) 
{
    return my_getid("/etc/group", name, -1);
}

void
my_getpwuid(char* name, uid_t uid) 
{
    my_getid("/etc/passwd", name, uid);
}

void
my_getgrgid(char* group, gid_t gid) 
{
    my_getid("/etc/group", group, gid);
}


#endif




#if (defined BB_CHVT) || (defined BB_DEALLOCVT)


#include <linux/kd.h>
#include <sys/ioctl.h>

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
  if (! is_a_console(fd))
    {
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

int get_console_fd(char* tty_name) 
{
  int fd;

  if (tty_name)
    {
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
  
  fprintf(stderr,
	  "Couldnt get a file descriptor referring to the console\n");
  return -1;		/* total failure */
}


#endif


#if !defined BB_REGEXP && (defined BB_GREP || defined BB_SED)  

/* Do a case insensitive strstr() */
char* stristr(char *haystack, const char *needle)
{
    int len = strlen( needle );
    while( *haystack ) {
	if( !strncasecmp( haystack, needle, len ) )
	    break;
	haystack++;
    }

    if( !(*haystack) )
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
	haystack = strstr (haystack, needle);
    else
	haystack = stristr (haystack, needle);
    if (haystack == NULL)
	return FALSE;
    return TRUE;
}


/* This performs substitutions after a string match has been found.  */
extern int replace_match(char *haystack, char *needle, char *newNeedle, int ignoreCase)
{
    int foundOne=0;
    char *where, *slider, *slider1, *oldhayStack;

    if (ignoreCase == FALSE)
	where = strstr (haystack, needle);
    else
	where = stristr (haystack, needle);

    if (strcmp(needle, newNeedle)==0)
	return FALSE;

    oldhayStack = (char*)malloc((unsigned)(strlen(haystack)));
    while(where!=NULL) {
	foundOne++;
	strcpy(oldhayStack, haystack);
#if 0
	if ( strlen(newNeedle) > strlen(needle)) {
	    haystack = (char *)realloc(haystack, (unsigned)(strlen(haystack) - 
		strlen(needle) + strlen(newNeedle)));
	}
#endif
	for(slider=haystack,slider1=oldhayStack;slider!=where;slider++,slider1++);
	*slider=0;
	haystack=strcat(haystack, newNeedle);
	slider1+=strlen(needle);
	haystack = strcat(haystack, slider1);
	where = strstr (slider, needle);
    }
    free( oldhayStack);

    if (foundOne > 0)
	return TRUE;
    else
	return FALSE;
}


#endif


#if defined BB_FIND
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
extern int
check_wildcard_match(const char* text, const char* pattern)
{
    const char*	retryPat;
    const char*	retryText;
    int		ch;
    int		found;

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

		//if (!found)
		if (found==TRUE)
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
#endif




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
#endif



#if defined BB_DD || defined BB_TAIL
/*
 * Read a number with a possible multiplier.
 * Returns -1 if the number format is illegal.
 */
extern long getNum (const char *cp)
{
    long value;

    if (!isDecimal (*cp))
	return -1;

    value = 0;

    while (isDecimal (*cp))
	value = value * 10 + *cp++ - '0';

    switch (*cp++) {
    case 'm':
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
#endif


#if defined BB_INIT || defined BB_HALT || defined BB_REBOOT 

#if ! defined BB_FEATURE_USE_PROCFS
#error Sorry, I depend on the /proc filesystem right now.
#endif
/* findInitPid()
 *  
 *  This finds the pid of init (which is not always 1).
 *  Currently, it's implemented by rummaging through the proc filesystem.
 *
 *  [return]
 *  0	    failure
 *  pid	    when init's pid is found.
 */
extern pid_t
findInitPid()
{
    pid_t   init_pid;
    char    filename[256];
    char    buffer[256];

    /* no need to opendir ;) */
    for (init_pid = 1; init_pid < 65536; init_pid++) {
	FILE	*status;

	sprintf(filename, "/proc/%d/status", init_pid);
	status = fopen(filename, "r");
	if (!status) { continue; }
	fgets(buffer, 256, status);
	fclose(status);

	if ( (strstr(buffer, "init\n") != NULL )) {
	    return init_pid;
	}
    }
    return 0;
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
#endif

/* END CODE */
