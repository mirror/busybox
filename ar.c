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
 * Last modified 9 September 2000
 */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <utime.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <malloc.h>
#include "internal.h"

#define BLOCK_SIZE 60
#define PRESERVE_DATE 1	/* preserve original dates */
#define VERBOSE       2	/* be verbose */
#define DISPLAY       4	/* display contents */
#define EXT_TO_FILE   8	/* extract contents of archive */
#define EXT_TO_STDOUT 16	/* extract to stdout */

#define MAX_NAME_LENGTH 100

//#define BB_DECLARE_EXTERN
//#define bb_need_io_error
//#include "messages.c"

typedef struct rawArHeader {    /* Byte Offset */
        char name[16];          /*  0-15 */
        char date[12];          /* 16-27 */
        char uid[6], gid[6];    /* 28-39 */
        char mode[8];           /* 40-47 */
        char size[10];          /* 48-57 */
        char fmag[2];           /* 58-59 */
} rawArHeader_t;

typedef struct headerL {
	char name[MAX_NAME_LENGTH];
        size_t size;
        uid_t uid;
        gid_t gid;
        mode_t mode;
        time_t mtime;
        off_t offset;
	struct headerL *next;
} headerL_t;

/*
 * identify Ar header (magic) and set srcFd to first header entry 
 */
static int checkArMagic(int srcFd)
{
        char arMagic[8];
        if (fullRead(srcFd, arMagic, 8) != 8)
                return (FALSE);
        
	if (strncmp(arMagic,"!<arch>",7) != 0)
                return(FALSE);
	return(TRUE);
}

/*
 * read, convert and check the raw ar header
 * srcFd should be pointing to the start of header prior to entry
 * srcFd will be pointing at the start of data after successful exit
 * if returns FALSE srcFd is reset to initial position
 */
static int readRawArHeader(int srcFd, headerL_t *header)
{
	rawArHeader_t rawArHeader;
	off_t	initialOffset;
	size_t nameLength;
	
	initialOffset = lseek(srcFd, 0, SEEK_CUR);
	if (fullRead(srcFd, (char *) &rawArHeader, 60) != 60) {
		lseek(srcFd, initialOffset, SEEK_SET);
		return(FALSE);
	}
	if ((rawArHeader.fmag[0]!='`') || (rawArHeader.fmag[1]!='\n')) {
		lseek(srcFd, initialOffset, SEEK_SET);
		return(FALSE);
	}

	strncpy(header->name, rawArHeader.name, 16);
	nameLength=strcspn(header->name, " \\");
	header->name[nameLength]='\0';
	parse_mode(rawArHeader.mode, &header->mode);
        header->mtime = atoi(rawArHeader.date);
        header->uid = atoi(rawArHeader.uid);
        header->gid = atoi(rawArHeader.gid);
        header->size = (size_t) atoi(rawArHeader.size);
        header->offset = initialOffset + (off_t) 60;
 	return(TRUE); 
}

/*
 * get, check and correct the converted header
 */ 
static int readArEntry(int srcFd, headerL_t *newEntry)
{
	size_t nameLength;

	if(readRawArHeader(srcFd, newEntry)==FALSE)
		return(FALSE);
	
	nameLength = strcspn(newEntry->name, "/");
	
	/* handle GNU style short filenames, strip trailing '/' */
	if (nameLength > 0)
		newEntry->name[nameLength]='\0';
	
	/* handle GNU style long filenames */ 
	if (nameLength == 0) {
		/* escape from recursive call */
		if (newEntry->name[1]=='0') 
			return(TRUE);

		/* the data section contains the real filename */
		if (newEntry->name[1]=='/') {
			char tempName[MAX_NAME_LENGTH];

			if (newEntry->size > MAX_NAME_LENGTH)
				newEntry->size = MAX_NAME_LENGTH;
			fullRead(srcFd, tempName, newEntry->size);
			tempName[newEntry->size-3]='\0';
			
			/* read the second header for this entry */
			/* be carefull, this is recursive */
			if (readArEntry(srcFd, newEntry)==FALSE)
				return(FALSE);
		
			if ((newEntry->name[0]='/') && (newEntry->name[1]='0'))
				strcpy(newEntry->name, tempName);
			else {
				errorMsg("Invalid long filename\n");
				return(FALSE);
			}
		}
	}
	return(TRUE);	
}

/*
 * return the headerL_t struct for the specified filename
 */
static headerL_t *getHeaders(int srcFd, headerL_t *head)
{
	int arEntry=FALSE;
	headerL_t *list;
        list = (headerL_t *) malloc(sizeof(headerL_t));

        if (checkArMagic(srcFd)==TRUE)
		arEntry=TRUE;
	else
		errorMsg("isnt an ar archive\n");

	if (arEntry==TRUE) { 
        	while(readArEntry(srcFd, list) == TRUE) {
			list->next = (headerL_t *) malloc(sizeof(headerL_t));
        		*list->next = *head;
			*head = *list;

			/* recursive check for sub-archives */
			lseek(srcFd, list->size, SEEK_CUR);
/*			printf("checking for sub headers\n");
		        if ((subList = getHeaders(srcFd, list->next)) != NULL) {
				printf("found a sub archive !\n");
			}
			else	
				printf("didnt find a sub header\n"); */
		}
	}

        return(head);
}

/*
 * find an entry in the linked list matching the filename
 */
static headerL_t *findEntry(headerL_t *head, const char *filename)
{
	while(head->next != NULL) {
		if (strcmp(filename, head->name)==0) 
			return(head);
		head=head->next;
	}
	return(NULL);
}

/*
 * populate linked list with all ar file entries and offset 
 */
static int displayEntry(headerL_t *head, int funct)
{
	if ((funct & VERBOSE) == VERBOSE) {
		printf("%s %d/%d %8d %s ", modeString(head->mode), head->uid, head->gid, head->size, timeString(head->mtime));
	}
	printf("%s\n", head->name);
	head = head->next;
	return(TRUE);
}

static int extractAr(int srcFd, int dstFd, headerL_t *file)
{
	lseek(srcFd, file->offset, SEEK_SET);
	if (copySubFile(srcFd, dstFd, (size_t) file->size) == TRUE)
		return(TRUE);	
	return(FALSE);
}

extern int ar_main(int argc, char **argv)
{
        int funct = 0, opt=0;
	int srcFd=0, dstFd=0;
 	headerL_t *header, *entry, *extractList;

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
	
	entry = (headerL_t *) malloc(sizeof(headerL_t));
	header = (headerL_t *) malloc(sizeof(headerL_t));
	extractList = (headerL_t *) malloc(sizeof(headerL_t));	

	header = getHeaders(srcFd, header);
	
	/* find files to extract or display */
	if (optind<argc) {
		/* only handle specified files */
		while(optind < argc) { 
			if ( (entry = findEntry(header, argv[optind])) != NULL) {
	                        entry->next = (headerL_t *) malloc(sizeof(headerL_t));
                        	*entry->next = *extractList;
                        	*extractList = *entry;
			}
			optind++;
		}	
	}
	else 
		/* extract everything */
		extractList = header;
	
       while(extractList->next != NULL) {	
		if ( (funct & EXT_TO_FILE) == EXT_TO_FILE) {
			dstFd = open(extractList->name, O_WRONLY | O_CREAT);
			extractAr(srcFd, dstFd, extractList);
		}
		if ( (funct & EXT_TO_STDOUT) == EXT_TO_STDOUT)	
			extractAr(srcFd, fileno(stdout), extractList);	
		if ( (funct & DISPLAY) == DISPLAY)
			displayEntry(extractList, funct);
		extractList=extractList->next;
	}
	return (TRUE);
}
