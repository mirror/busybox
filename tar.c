/* vi: set sw=4 ts=4: */
/*
 * Mini tar implementation for busybox
 *
 * Copyright (C) 1999 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
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
 */


#include "internal.h"
#define BB_DECLARE_EXTERN
#define bb_need_io_error
#include "messages.c"
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/param.h>			/* for PATH_MAX */


#ifdef BB_FEATURE_TAR_CREATE

static const char tar_usage[] =
	"tar -[cxtvOf] [tarFileName] [FILE] ...\n\n"
	"Create, extract, or list files from a tar file.\n\n"
	"Options:\n"

	"\tc=create, x=extract, t=list contents, v=verbose,\n"
	"\tO=extract to stdout, f=tarfile or \"-\" for stdin\n";

#else

static const char tar_usage[] =
	"tar -[xtvOf] [tarFileName] [FILE] ...\n\n"
	"Extract, or list files stored in a tar file.  This\n"
	"version of tar does not support creation of tar files.\n\n"
	"Options:\n"

	"\tx=extract, t=list contents, v=verbose,\n"
	"\tO=extract to stdout, f=tarfile or \"-\" for stdin\n";

#endif


/* Tar file constants  */
#ifndef MAJOR
#define MAJOR(dev) (((dev)>>8)&0xff)
#define MINOR(dev) ((dev)&0xff)
#endif


/* POSIX tar Header Block, from POSIX 1003.1-1990  */
struct TarHeader
{
                                /* byte offset */
	char name[100];               /*   0 */
	char mode[8];                 /* 100 */
	char uid[8];                  /* 108 */
	char gid[8];                  /* 116 */
	char size[12];                /* 124 */
	char mtime[12];               /* 136 */
	char chksum[8];               /* 148 */
	char typeflag;                /* 156 */
	char linkname[100];           /* 157 */
	char magic[6];                /* 257 */
	char version[2];              /* 263 */
	char uname[32];               /* 265 */
	char gname[32];               /* 297 */
	char devmajor[8];             /* 329 */
	char devminor[8];             /* 337 */
	char prefix[155];             /* 345 */
	/* padding                       500 */
};
typedef struct TarHeader TarHeader;


/* A few useful constants */
#define TAR_MAGIC          "ustar"        /* ustar and a null */
#define TAR_VERSION        "00"           /* 00 and no null */
#define TAR_MAGIC_LEN       6
#define TAR_VERSION_LEN     2
#define TAR_NAME_LEN        100
#define TAR_BLOCK_SIZE      512

/* A nice enum with all the possible tar file content types */
enum TarFileType 
{
	REGTYPE  = '0',            /* regular file */
	REGTYPE0 = '\0',           /* regular file (ancient bug compat)*/
	LNKTYPE  = '1',            /* hard link */
	SYMTYPE  = '2',            /* symbolic link */
	CHRTYPE  = '3',            /* character special */
	BLKTYPE  = '4',            /* block special */
	DIRTYPE  = '5',            /* directory */
	FIFOTYPE = '6',            /* FIFO special */
	CONTTYPE = '7',            /* reserved */
};
typedef enum TarFileType TarFileType;

/* This struct ignores magic, non-numeric user name, 
 * non-numeric group name, and the checksum, since
 * these are all ignored by BusyBox tar. */ 
struct TarInfo
{
	int              tarFd;          /* An open file descriptor for reading from the tarball */
	char *           name;           /* File name */
	mode_t           mode;           /* Unix mode, including device bits. */
	uid_t            uid;            /* Numeric UID */
	gid_t            gid;            /* Numeric GID */
	size_t           size;           /* Size of file */
	time_t           mtime;          /* Last-modified time */
	enum TarFileType type;           /* Regular, directory, link, etc */
	char *           linkname;       /* Name for symbolic and hard links */
	long             devmajor;       /* Major number for special device */
	long             devminor;       /* Minor number for special device */
};
typedef struct TarInfo TarInfo;

/* Static data  */
static const unsigned long TarChecksumOffset = (const unsigned long)&(((TarHeader *)0)->chksum);


/* Local procedures to restore files from a tar file.  */
static int readTarFile(const char* tarName, int extractFlag, int listFlag, 
		int tostdoutFlag, int verboseFlag);



#ifdef BB_FEATURE_TAR_CREATE
/*
 * Local procedures to save files into a tar file.
 */
static void saveFile(const char *fileName, int seeLinks);
static void saveRegularFile(const char *fileName,
							const struct stat *statbuf);
static void saveDirectory(const char *fileName,
						  const struct stat *statbuf);
static void writeHeader(const char *fileName, const struct stat *statbuf);
static void writeTarFile(int argc, char **argv);
static void writeTarBlock(const char *buf, int len);
static int putOctal(char *cp, int len, long value);

#endif


extern int tar_main(int argc, char **argv)
{
	const char *tarName=NULL;
	const char *options;
	int listFlag     = FALSE;
	int extractFlag  = FALSE;
	int createFlag   = FALSE;
	int verboseFlag  = FALSE;
	int tostdoutFlag = FALSE;

	argc--;
	argv++;

	if (argc < 1)
		usage(tar_usage);

	/* Parse options  */
	if (**argv == '-')
		options = (*argv++) + 1;
	else
		options = (*argv++);
	argc--;

	for (; *options; options++) {
		switch (*options) {
		case 'f':
			if (tarName != NULL)
				fatalError( "Only one 'f' option allowed\n");

			tarName = *argv++;
			if (tarName == NULL)
				fatalError( "Option requires an argument: No file specified\n");
			argc--;

			break;

		case 't':
			if (extractFlag == TRUE || createFlag == TRUE)
				goto flagError;
			listFlag = TRUE;
			break;

		case 'x':
			if (listFlag == TRUE || createFlag == TRUE)
				goto flagError;
			extractFlag = TRUE;
			break;
		case 'c':
			if (extractFlag == TRUE || listFlag == TRUE)
				goto flagError;
			createFlag = TRUE;
			break;

		case 'v':
			verboseFlag = TRUE;
			break;

		case 'O':
			tostdoutFlag = TRUE;
			break;

		case '-':
			usage(tar_usage);
			break;

		default:
			fatalError( "Unknown tar flag '%c'\n" 
					"Try `tar --help' for more information\n", *options);
		}
	}

	/* 
	 * Do the correct type of action supplying the rest of the
	 * command line arguments as the list of files to process.
	 */
	if (createFlag == TRUE) {
#ifndef BB_FEATURE_TAR_CREATE
		fatalError( "This version of tar was not compiled with tar creation support.\n");
#else
		exit(writeTarFile(argc, argv));
#endif
	} else {
		exit(readTarFile(tarName, extractFlag, listFlag, tostdoutFlag, verboseFlag));
	}

  flagError:
	fatalError( "Exactly one of 'c', 'x' or 't' must be specified\n");
}
					
static void
tarExtractRegularFile(TarInfo *header, int extractFlag, int tostdoutFlag)
{
	size_t  writeSize;
	size_t  readSize;
	size_t  actualWriteSz;
	char    buffer[BUFSIZ];
	size_t  size = header->size;
	int outFd=fileno(stdout);

	/* Open the file to be written, if a file is supposed to be written */
	if (extractFlag==TRUE && tostdoutFlag==FALSE) {
		if ((outFd=open(header->name, O_CREAT|O_TRUNC|O_WRONLY, header->mode & ~S_IFMT)) < 0)
			errorMsg(io_error, header->name, strerror(errno)); 
		/* Create the path to the file, just in case it isn't there...
		 * This should not screw up path permissions or anything. */
		createPath(header->name, 0777);
	}

	/* Write out the file, if we are supposed to be doing that */
	while ( size > 0 ) {
		actualWriteSz=0;
		if ( size > sizeof(buffer) )
			writeSize = readSize = sizeof(buffer);
		else {
			int mod = size % 512;
			if ( mod != 0 )
				readSize = size + (512 - mod);
			else
				readSize = size;
			writeSize = size;
		}
		if ( (readSize = fullRead(header->tarFd, buffer, readSize)) <= 0 ) {
			/* Tarball seems to have a problem */
			errorMsg("Error reading tarfile: %s", strerror(errno)); 
			return;
		}
		if ( readSize < writeSize )
			writeSize = readSize;

		/* Write out the file, if we are supposed to be doing that */
		if (extractFlag==TRUE) {

			if ((actualWriteSz=fullWrite(outFd, buffer, writeSize)) != writeSize ) {
				/* Output file seems to have a problem */
				errorMsg(io_error, header->name, strerror(errno)); 
				return;
			}
		}

		size -= actualWriteSz;
	}

	/* Now we are done writing the file out, so try 
	 * and fix up the permissions and whatnot */
	if (extractFlag==TRUE && tostdoutFlag==FALSE) {
		struct utimbuf t;
		/* Now set permissions etc for the new file */
		fchown(outFd, header->uid, header->gid);
		fchmod(outFd, header->mode & ~S_IFMT);
		close(outFd);
		/* File must be closed before trying to change the date */
		t.actime = time(0);
		t.modtime = header->mtime;
		utime(header->name, &t);
	}
}

static void
fixUpPermissions(TarInfo *header)
{
	struct utimbuf t;
	/* Now set permissions etc for the new file */
	chown(header->name, header->uid, header->gid);
	chmod(header->name, header->mode);
	/* Reset the time */
	t.actime = time(0);
	t.modtime = header->mtime;
	utime(header->name, &t);
}
				
static void
tarExtractDirectory(TarInfo *header, int extractFlag, int tostdoutFlag)
{

	if (extractFlag==FALSE || tostdoutFlag==TRUE)
		return;

	if (createPath(header->name, header->mode) != TRUE) {
		errorMsg("Error creating directory '%s': %s", header->name, strerror(errno)); 
		return;  
	}
	/* make the final component, just in case it was
	 * omitted by createPath() (which will skip the
	 * directory if it doesn't have a terminating '/') */
	mkdir(header->name, header->mode);

	/* Now set permissions etc for the new directory */
	fixUpPermissions(header);
}

static void
tarExtractHardLink(TarInfo *header, int extractFlag, int tostdoutFlag)
{
	if (extractFlag==FALSE || tostdoutFlag==TRUE)
		return;

	if (link(header->linkname, header->name) < 0) {
		errorMsg("Error creating hard link '%s': %s", header->linkname, strerror(errno)); 
		return;
	}

	/* Now set permissions etc for the new directory */
	fixUpPermissions(header);
}

static void
tarExtractSymLink(TarInfo *header, int extractFlag, int tostdoutFlag)
{
	if (extractFlag==FALSE || tostdoutFlag==TRUE)
		return;

#ifdef	S_ISLNK
	if (symlink(header->linkname, header->name) < 0) {
		errorMsg("Error creating symlink '%s': %s", header->linkname, strerror(errno)); 
		return;
	}
	/* Try to change ownership of the symlink.
	 * If libs doesn't support that, don't bother.
	 * Changing the pointed-to-file is the Wrong Thing(tm).
	 */
#if (__GLIBC__ >= 2) && (__GLIBC_MINOR__ >= 1)
	lchown(header->name, header->uid, header->gid);
#endif

	/* Do not change permissions or date on symlink,
	 * since it changes the pointed to file instead.  duh. */
#else
	fprintf(stderr, "Cannot create symbolic links\n");
#endif
}

static void
tarExtractSpecial(TarInfo *header, int extractFlag, int tostdoutFlag)
{
	if (extractFlag==FALSE || tostdoutFlag==TRUE)
		return;

	if (S_ISCHR(header->mode) || S_ISBLK(header->mode) || S_ISSOCK(header->mode)) {
		mknod(header->name, header->mode, makedev(header->devmajor, header->devminor));
	} else if (S_ISFIFO(header->mode)) {
		mkfifo(header->name, header->mode);
	} else {
		open(header->name, O_WRONLY | O_CREAT | O_TRUNC, header->mode);
	}

	/* Now set permissions etc for the new directory */
	fixUpPermissions(header);
}

/* Read an octal value in a field of the specified width, with optional
 * spaces on both sides of the number and with an optional null character
 * at the end.  Returns -1 on an illegal format.  */
static long getOctal(const char *cp, int size)
{
	long val = 0;

	for(;(size > 0) && (*cp == ' '); cp++, size--);
	if ((size == 0) || !isOctal(*cp))
		return -1;
	for(; (size > 0) && isOctal(*cp); size--) {
		val = val * 8 + *cp++ - '0';
	}
	for (;(size > 0) && (*cp == ' '); cp++, size--);
	if ((size > 0) && *cp)
		return -1;
	return val;
}


/* Parse the tar header and fill in the nice struct with the details */
static int
parseTarHeader(struct TarHeader *rawHeader, struct TarInfo *header)
{
	int i;
	long chksum, sum;
	unsigned char *s = (unsigned char *)rawHeader;

	header->name  = rawHeader->name;
	header->mode  = getOctal(rawHeader->mode, sizeof(rawHeader->mode));
	header->uid   =  getOctal(rawHeader->uid, sizeof(rawHeader->uid));
	header->gid   =  getOctal(rawHeader->gid, sizeof(rawHeader->gid));
	header->size  = getOctal(rawHeader->size, sizeof(rawHeader->size));
	header->mtime = getOctal(rawHeader->mtime, sizeof(rawHeader->mtime));
	chksum = getOctal(rawHeader->chksum, sizeof(rawHeader->chksum));
	header->type  = rawHeader->typeflag;
	header->linkname  = rawHeader->linkname;
	header->devmajor  = getOctal(rawHeader->devmajor, sizeof(rawHeader->devmajor));
	header->devminor  = getOctal(rawHeader->devminor, sizeof(rawHeader->devminor));

	/* Check the checksum */
	sum = ' ' * sizeof(rawHeader->chksum);
	for ( i = TarChecksumOffset; i > 0; i-- )
		sum += *s++;
	s += sizeof(rawHeader->chksum);       
	for ( i = (512 - TarChecksumOffset - sizeof(rawHeader->chksum)); i > 0; i-- )
		sum += *s++;
	if (sum == chksum )
		return ( TRUE);
	return( FALSE);
}


/*
 * Read a tar file and extract or list the specified files within it.
 * If the list is empty than all files are extracted or listed.
 */
static int readTarFile(const char* tarName, int extractFlag, int listFlag, 
		int tostdoutFlag, int verboseFlag)
{
	int status, tarFd=0;
	int errorFlag=FALSE;
	TarHeader rawHeader;
	TarInfo header;
	int alreadyWarned=FALSE;
	//int skipFileFlag=FALSE;

	/* Open the tar file for reading.  */
	if (!strcmp(tarName, "-"))
		tarFd = fileno(stdin);
	else
		tarFd = open(tarName, O_RDONLY);
	if (tarFd < 0) {
		errorMsg( "Error opening '%s': %s", tarName, strerror(errno));
		return ( FALSE);
	}

	/* Set the umask for this process so it doesn't 
	 * screw up permission setting for us later. */
	umask(0);

	/* Read the tar file, and iterate over it one file at a time */
	while ( (status = fullRead(tarFd, (char*)&rawHeader, TAR_BLOCK_SIZE)) == TAR_BLOCK_SIZE ) {

		/* First, try to read the header */
		if ( parseTarHeader(&rawHeader, &header) == FALSE ) {
			close( tarFd);
			if ( *(header.name) == '\0' ) {
				goto endgame;
			} else {
				errorFlag=TRUE;
				errorMsg("Bad tar header, skipping\n");
				continue;
			}
		}
		if ( *(header.name) == '\0' )
				goto endgame;

		/* Check for and relativify any absolute paths */
		if ( *(header.name) == '/' ) {

			while (*(header.name) == '/')
				++*(header.name);

			if (alreadyWarned == FALSE) {
				errorMsg("Absolute path detected, removing leading slashes\n");
				alreadyWarned = TRUE;
			}
		}

		/* Special treatment if the list (-t) flag is on */
		if (verboseFlag == TRUE && extractFlag == FALSE) {
			int len, len1;
			char buf[35];
			struct tm *tm = localtime (&(header.mtime));

			len=printf("%s %d/%-d ", modeString(header.mode), header.uid, header.gid);
			if (header.type==CHRTYPE || header.type==BLKTYPE) {
				len1=snprintf(buf, sizeof(buf), "%ld,%-ld ", 
						header.devmajor, header.devminor);
			} else {
				len1=snprintf(buf, sizeof(buf), "%d ", header.size);
			}
			/* Jump through some hoops to make the columns match up */
			for(;(len+len1)<31;len++)
				printf(" ");
			printf(buf);

			/* Use ISO 8610 time format */
			if (tm) { 
				printf ("%04d-%02d-%02d %02d:%02d:%02d ", 
						tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, 
						tm->tm_hour, tm->tm_min, tm->tm_sec);
			}
		}
		/* List contents if we are supposed to do that */
		if (verboseFlag == TRUE || listFlag == TRUE) {
			/* Now the normal listing */
			printf("%s", header.name);
			/* If this is a link, say so */
			if (header.type==LNKTYPE)
				printf(" link to %s", header.linkname);
			else if (header.type==SYMTYPE)
				printf(" -> %s", header.linkname);
			printf("\n");
		}

#if 0
		/* See if we want to restore this file or not */
		skipFileFlag=FALSE;
		if (wantFileName(outName) == FALSE) {
			skipFileFlag = TRUE;
		}
#endif

		/* If we got here, we can be certain we have a legitimate 
		 * header to work with.  So work with it.  */
		switch ( header.type ) {
			case REGTYPE:
			case REGTYPE0:
				/* If the name ends in a '/' then assume it is
				 * supposed to be a directory, and fall through */
				if (header.name[strlen(header.name)-1] != '/') {
					tarExtractRegularFile(&header, extractFlag, tostdoutFlag);
					break;
				}
			case DIRTYPE:
				tarExtractDirectory( &header, extractFlag, tostdoutFlag);
				break;
			case LNKTYPE:
				tarExtractHardLink( &header, extractFlag, tostdoutFlag);
				break;
			case SYMTYPE:
				tarExtractSymLink( &header, extractFlag, tostdoutFlag);
				break;
			case CHRTYPE:
			case BLKTYPE:
			case FIFOTYPE:
				tarExtractSpecial( &header, extractFlag, tostdoutFlag);
				break;
			default:
				close( tarFd);
				return( FALSE);
		}
	}
	close(tarFd);
	if (status > 0) {
		/* Bummer - we read a partial header */
		errorMsg( "Error reading '%s': %s", tarName, strerror(errno));
		return ( FALSE);
	}
	else 
		return( status);

	/* Stuff to do when we are done */
endgame:
	close( tarFd);
	if ( *(header.name) == '\0' ) {
		if (errorFlag==FALSE)
			return( TRUE);
	} 
	return( FALSE);
}

