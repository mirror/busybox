/* vi: set sw=4 ts=4: */
/*
 * Mini ar implementation for busybox 
 *
 * Copyright (C) 2000 by Glenn McGrath
 * Written by Glenn McGrath <bug1@netconnect.com.au> 1 June 2000
 * 		
 * Modified 8 August 2000 by Glenn McGrath 
 *  - now uses getopt
 *  - moved copySubFile function to utilities.c
 *  - creates linked list of all headers 
 *  - easily accessable to other busybox functions  
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
 * Last modified 25 August 2000
 */
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <utime.h>
#include <sys/types.h>
#include "internal.h"

#define BLOCK_SIZE 60
#define PRESERVE_DATE 1	/* preserve original dates */
#define VERBOSE       2	/* be verbose */
#define DISPLAY       4	/* display contents */
#define EXT_TO_FILE   8	/* extract contents of archive */
#define EXT_TO_STDOUT 16	/* extract to stdout */

#define BB_DECLARE_EXTERN
#define bb_need_io_error
#include "messages.c"

typedef struct rawArHeader {            /* Byte Offset */
        char name[16];          /*  0-15 */
        char date[12];          /* 16-27 */
        char uid[6], gid[6];    /* 28-39 */
        char mode[8];           /* 40-47 */
        char size[10];          /* 48-57 */
        char fmag[2];           /* 58-59 */
} rawArHeader_t;

typedef struct headerL {
	char name[16];
        size_t size;
        uid_t uid;
        gid_t gid;
        mode_t mode;
        time_t mtime;
        off_t offset;
	struct headerL *next;
} headerL_t;

/*
 * populate linked list with all ar file entries and offset 
 */
static int parseArArchive(int srcFd, headerL_t *current)
{
        off_t lastOffset=0, thisOffset=0;
        char arVersion[8];
        rawArHeader_t rawArHeader;
      	
	lseek(srcFd, 0, SEEK_SET);
        if (fullRead(srcFd, arVersion, 8) <= 0) {
                errorMsg("Invalid header magic\n");
                return (FALSE);
        }
        if (strncmp(arVersion,"!<arch>",7) != 0) {
                errorMsg("This doesnt appear to be an ar archive\n");
                return(FALSE);
        }
        while (fullRead(srcFd, (char *) &rawArHeader, 60) == 60) {
              if ( (rawArHeader.fmag[0] == '`') &&
                        (rawArHeader.fmag[1] == '\n')) {
                        sscanf(rawArHeader.name, "%s", current->name);
                        parse_mode(rawArHeader.mode, &current->mode);
                        current->mtime = atoi(rawArHeader.date);
                        current->uid = atoi(rawArHeader.uid);
                        current->gid = atoi(rawArHeader.gid);
                        current->size = (size_t) atoi(rawArHeader.size);
                        current->offset = lseek(srcFd, 0 , SEEK_CUR);
			current->next = (headerL_t *) xmalloc(sizeof(headerL_t));
			lastOffset = lseek(srcFd, (off_t) current->size, SEEK_CUR);
			current = current->next;
                }
                else {  /* GNU ar has an extra char after data */
			thisOffset=lseek(srcFd, 0, SEEK_CUR);
                        if ( (thisOffset - lastOffset) > ((off_t) 61) ) 
                                return(FALSE);
			lseek(srcFd, thisOffset - 59, SEEK_SET);
                }
        }
        return(FALSE);
}

/*
 * return the headerL_t struct for the specified filename
 */
static headerL_t *getSubFileHeader(int srcFd, const char filename[16])
{
        headerL_t *list;
	list = xmalloc(sizeof(headerL_t)); 

        parseArArchive(srcFd, list);
        while (list->next != NULL) {
                if (strncmp(list->name, filename, strlen(filename))==0)
                        return(list);
                list=list->next;
        }
        return(NULL);
}

/*
 * populate linked list with all ar file entries and offset 
 */
static int displayEntry(int srcFd, const char filename[16], int funct)
{
	headerL_t *file;

	if ((file = getSubFileHeader(srcFd, filename)) == NULL)
		return(FALSE);
	if ((funct & VERBOSE) == VERBOSE) {
		printf("%s %d/%d %8d %s ", modeString(file->mode), file->uid, file->gid, file->size, timeString(file->mtime));
	}
	printf("%s\n", file->name);
	return(TRUE);
}

static int extractAr(int srcFd, int dstFd, const char filename[16])
{
        headerL_t *file;
 
        if ( (file = getSubFileHeader(srcFd, filename)) == NULL)
		return(FALSE);
	lseek(srcFd, file->offset, SEEK_SET);
	if (copySubFile(srcFd, dstFd, (size_t) file->size) == TRUE)
		return(TRUE);	
	return(FALSE);
}

extern int ar_main(int argc, char **argv)
{
        int funct = 0, opt=0;
	int srcFd=0, dstFd=0;
 
	while ((opt = getopt(argc, argv, "ovt:p:x:")) != -1) {
		switch (opt) {
		case 'o':
			funct = funct | PRESERVE_DATE;
			break;
		case 'v':
			funct = funct | VERBOSE;
			break;
		case 't':
			funct = funct | DISPLAY;
		case 'x':
			if (opt=='x') {
				funct = funct | EXT_TO_FILE;
                        }
		case 'p':
			if (opt=='p') {
				funct = funct | EXT_TO_STDOUT;
			}
			/* following is common to 't','x' and 'p' */
                        if (optarg == NULL) {
                                printf("expected a filename\n");
                                return(FALSE);
                        }
			if ( (srcFd = open(optarg, O_RDONLY)) < 0) {
				errorMsg("Cannot read %s\n", optarg);
				return (FALSE);
			}
			break;
		default:
			usage(ar_usage);
		}
	}
 
        /* check options not just preserve_dates and/or verbose */  
	if (funct < 4) {
                usage(ar_usage);
                return(FALSE);
        }
	
	/* find files to extract */
	if (optind<argc)
        	for(; optind < argc; optind++) {
			if ( (funct & EXT_TO_FILE) == EXT_TO_FILE) {
				dstFd = open(argv[optind], O_WRONLY | O_CREAT);
				extractAr(srcFd, dstFd, argv[optind]);
			}
			if ( (funct & EXT_TO_STDOUT) == EXT_TO_STDOUT)	
				extractAr(srcFd, fileno(stdout), argv[optind]);	
			if ( (funct & DISPLAY) == DISPLAY)
				displayEntry(srcFd, argv[optind], funct);
		}
	return (TRUE);
}
