/* vi: set sw=4 ts=4: */
/*
 * Mini ar implementation for busybox 
 *
 * Copyright (C) 2000 by Glenn McGrath
 * Written by Glenn McGrath <bug1@netconnect.com.au> 1 June 2000
 *
 * Based in part on BusyBox tar, Debian dpkg-deb and GNU ar.
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
 * Last modified 10 June 2000
 */


#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <utime.h>
#include <sys/types.h>
#include "internal.h"

#define AR_BLOCK_SIZE 60
#define AR_PRESERVE_DATE 1	/* preserve original dates */
#define AR_VERBOSE       2	/* be verbose */
#define AR_DISPLAY       4	/* display contents */
#define AR_EXT_TO_FILE   8	/* extract contents of archive */
#define AR_EXT_TO_STDOUT 16	/* extract to stdout */

#define BB_DECLARE_EXTERN
#define bb_need_io_error
#include "messages.c"

struct ArHeader {				/* Byte Offset */
	char ar_name[16];			/*  0-15 */
	char ar_date[12];			/* 16-27 */
	char ar_uid[6], ar_gid[6];	/* 28-39 */
	char ar_mode[8];			/* 40-47 */
	char ar_size[10];			/* 48-57 */
	char ar_fmag[2];			/* 58-59 */
};
typedef struct ArHeader ArHeader;

struct ArInfo {
	char name[17];				/* File name */
	time_t date;				/* long int, No of seconds since epoch */
	uid_t uid;					/* unsigned int, Numeric UID */
	gid_t gid;					/* unsigned int, Numeric GID */
	mode_t mode;				/* unsigned int, Unix mode */
	size_t size;				/* int, Size of the file */
};
typedef struct ArInfo ArInfo;

/*
 * Display details of a file, verbosly if funct=2   
 */
static void displayEntry(struct ArInfo *entry, int funct)
{
	/* TODO convert mode to string */
	if ((funct & AR_VERBOSE) == AR_VERBOSE)
		printf("%i %i/%i %8i %s ", entry->mode, entry->uid, entry->gid,
			   entry->size, timeString(entry->date));
	printf("%s\n", entry->name);
}

/* this is from tar.c remove later*/
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

/*
 * Converts from the char based struct to a new struct with stricter types
 */
static int processArHeader(struct ArHeader *rawHeader, struct ArInfo *header)
{
	int count2;
	int count;
	
	/* check end of header marker is valid */
	if ((rawHeader->ar_fmag[0]!='`') || (rawHeader->ar_fmag[1]!='\n')) 
		return(FALSE); 

	/* convert filename */ 
	for (count = 0; count < 16; count++) {
		/* allow spaces in filename except at the end */
		if (rawHeader->ar_name[count] == ' ') {
			for (count2 = count; count2 < 16; count2++)
				if (!isspace(rawHeader->ar_name[count2]))
					break;
			if (count2 >= 16)
				break;
		}
		/* GNU ar uses '/' as an end of filename marker */
		if (rawHeader->ar_name[count] == '/')
			break;
		header->name[count] = rawHeader->ar_name[count];
	}
	header->name[count] = '\0';
	header->date = atoi(rawHeader->ar_date);
	header->uid = atoi(rawHeader->ar_uid);
	header->gid = atoi(rawHeader->ar_gid);
	header->mode = getOctal(rawHeader->ar_mode, sizeof(rawHeader->ar_mode));
	header->size = atoi(rawHeader->ar_size);
	return (TRUE);
}

/*
 * Copy size bytes from current position if srcFd to current position in dstFd
 * taken from tarExtractRegularFile in tar.c, remove later
 */
static int copySubFile(int srcFd, int dstFd, int copySize)
{
	int readSize, writeSize, doneSize;
	char buffer[BUFSIZ];

	while (copySize > 0) {
		if (copySize > BUFSIZ)
			readSize = BUFSIZ;
		else
			readSize = copySize;
		writeSize = fullRead(srcFd, buffer, readSize);
		if (writeSize <= 0) {
			errorMsg(io_error, "copySubFile", strerror(errno));
			return (FALSE);
		}
		doneSize = fullWrite(dstFd, buffer, writeSize);
		if (doneSize <= 0) {
			errorMsg(io_error, "copySubFile", strerror(errno));
			return (FALSE);
		}
		copySize -= doneSize;
	}
	return (TRUE);
}

/*
 * Extract the file described in ArInfo to the specified path 
 * set the new files uid, gid and mode 
 */
static int extractToFile(struct ArInfo *file, int funct, int srcFd, const char *path)
{
	int dstFd, temp;
	struct stat tmpStat;
	char *pathname = NULL;
	struct utimbuf newtime;
	
	if ((temp = isDirectory(path, TRUE, &tmpStat)) != TRUE) {
		if (!createPath(path, 0777)) {
			fatalError("Cannot extract to specified path");
			return (FALSE);
		}
	}
	temp = (strlen(path) + 16);
	pathname = (char *) xmalloc(temp);
	pathname = strcpy(pathname, path);
	pathname = strcat(pathname, file->name);
	dstFd = device_open(pathname, O_WRONLY | O_CREAT);
	temp = copySubFile(srcFd, dstFd, file->size);
	fchown(dstFd, file->uid, file->gid);
	fchmod(dstFd, file->mode);
	close(dstFd);
	if ((funct&AR_PRESERVE_DATE)==AR_PRESERVE_DATE) 
		newtime.modtime=file->date;
	else
		newtime.modtime=time(0);
	newtime.actime=time(0);
	temp = utime(pathname, &newtime);
	return (TRUE);
}

/*
 * Return a file descriptor for the specified file and do error checks
 */
static int getArFd(char *filename)
{
        int arFd;
        char arVersion[8];

        arFd = open(filename, O_RDONLY);
        if (arFd < 0) { 
                errorMsg("Error opening '%s': %s\n", filename, strerror(errno));
		return (FALSE);
	}
        if (fullRead(arFd, arVersion, 8) <= 0) {
                errorMsg( "Unexpected EOF in archive\n");
                return (FALSE);
        }
        if (strncmp(arVersion,"!<arch>",7) != 0) {
                errorMsg("ar header fails check ");
                return(FALSE);
        }
        return arFd;
}

/*
 * Step through the ar file and process it one entry at a time
 * fileList[0] is the name of the ar archive
 * fileList[1] and up are filenames to extract from the archive
 * funct contains flags to specify the actions to be performed 
 */
static int readArFile(char *fileList[16], int fileListSize, int funct)
{
	int arFd, status, extFileFlag, i, lastOffset=0;
	ArHeader rawArHeader;
	ArInfo arEntry;

	/* open the ar archive */
	arFd=getArFd(fileList[0]);

	/* read the first header, then loop until ono more headers */ 
	while ((status = fullRead(arFd, (char *) &rawArHeader, AR_BLOCK_SIZE))
		   == AR_BLOCK_SIZE) {

		/* check the header is valid, if not try reading the header
		   agian with an offset of 1, needed as some ar archive end
                   with a '\n' which isnt counted in specified file size */
		if ((status=processArHeader(&rawArHeader, &arEntry))==FALSE ) {
		  	if ((i=lseek(arFd, 0, SEEK_CUR))==(lastOffset+60)) 
				lseek(arFd, lastOffset+1, SEEK_SET);
			else 
				return(FALSE);
       		}
		else {  
			extFileFlag=0;
			
			if ((funct&AR_DISPLAY) || (funct&AR_VERBOSE))
				displayEntry(&arEntry, funct);

			/* check file was specified to be extracted only if 
			   some file were specified */
 	   		if ((funct&AR_EXT_TO_FILE) || (funct&AR_EXT_TO_STDOUT)){
        	                if (fileListSize==1)
                	                extFileFlag=1;
                        	else {
      					for( i=1; i<=fileListSize; i++)
                                        	if ((status=(strcmp(fileList[i],arEntry.name)))==0)
                                                	extFileFlag=1;
                        	}
			}
			if (extFileFlag==1) { 
		       		if (funct&AR_EXT_TO_FILE)
                               		extractToFile(&arEntry, funct, arFd, "./");
		       		else	
                               		copySubFile(arFd,fileno(stdout),arEntry.size);
                	}
			else
                        	lseek(arFd, arEntry.size, SEEK_CUR);
			lastOffset=lseek(arFd, 0, SEEK_CUR);
		} /* if processArHeader */
	}  /* while */
	return (TRUE);
}

extern int ar_main(int argc, char **argv)
{
        int funct = 0, ret=0, i=0;
        char *fileList[16], c, *opt_ptr;

	if (argc < 2)
		usage(ar_usage);

	opt_ptr = argv[1];
	if (*opt_ptr == '-')
		++opt_ptr;
	while ((c = *opt_ptr++) != '\0') {
		switch (c) {
		case 'o':
			funct = funct | AR_PRESERVE_DATE;
			break;
		case 'v':
			funct = funct | AR_VERBOSE;
			break;
		case 't':
			funct = funct | AR_DISPLAY;
			break;
		case 'x':
			funct = funct | AR_EXT_TO_FILE;
			break;
		case 'p':
			funct = funct | AR_EXT_TO_STDOUT;
			break;
		default:
			usage(ar_usage);
		}
	}

        for(i=0; i<(argc-2); i++) 
                fileList[i]=argv[i+2];
	
	if (funct > 3)
		ret = readArFile(fileList, (argc-2), funct);
	
	return (ret);
}
