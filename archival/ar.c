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
#define RECURSIVE     32  	 

#define MAX_NAME_LENGTH 100

//#define BB_DECLARE_EXTERN
//#define bb_need_io_error
//#include "messages.c"

//#define BB_AR_EXPERIMENTAL_UNTAR

#if defined BB_AR_EXPERIMENTAL_UNTAR
typedef struct rawTarHeader {
        char name[100];               /*   0-99 */
        char mode[8];                 /* 100-107 */
        char uid[8];                  /* 108-115 */
        char gid[8];                  /* 116-123 */
        char size[12];                /* 124-135 */
        char mtime[12];               /* 136-147 */
        char chksum[8];               /* 148-155 */
        char typeflag;                /* 156-156 */
        char linkname[100];           /* 157-256 */
        char magic[6];                /* 257-262 */
        char version[2];              /* 263-264 */
        char uname[32];               /* 265-296 */
        char gname[32];               /* 297-328 */
        char devmajor[8];             /* 329-336 */
        char devminor[8];             /* 337-344 */
        char prefix[155];             /* 345-499 */
        char padding[12];             /* 500-512 */
} rawTarHeader_t;
#endif

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

#if defined BB_AR_EXPERIMENTAL_UNTAR
/*
 * identify Tar header (magic field) and reset srcFd to entry position
 */
static int checkTarMagic(int srcFd)
{
        off_t headerStart;
        char magic[6];

        headerStart = lseek(srcFd, 0, SEEK_CUR);
        lseek(srcFd, (off_t) 257, SEEK_CUR);
        fullRead(srcFd, magic, 6);
        lseek(srcFd, headerStart, SEEK_SET);
        if (strncmp(magic, "ustar", 5)!=0)
                return(FALSE);
        return(TRUE);
}


static int readTarHeader(int srcFd, headerL_t *current)
{
     	rawTarHeader_t rawTarHeader;
        unsigned char *temp = (unsigned char *) &rawTarHeader;
        long sum = 0;
        int i;
	off_t initialOffset;

        initialOffset = lseek(srcFd, 0, SEEK_CUR);
        if (fullRead(srcFd, (char *) &rawTarHeader, 512) != 512) {
                lseek(srcFd, initialOffset, SEEK_SET);
                return(FALSE);
        }
        for (i =  0; i < 148 ; i++)
        sum += temp[i];
        sum += ' ' * 8;
        for (i =  156; i < 512 ; i++)
                sum += temp[i];
        if (sum!= strtol(rawTarHeader.chksum, NULL, 8))
		return(FALSE);
  	sscanf(rawTarHeader.name, "%s", current->name);
        current->size = strtol(rawTarHeader.size, NULL, 8);
        current->uid = strtol(rawTarHeader.uid, NULL, 8);
        current->gid = strtol(rawTarHeader.gid, NULL, 8);
        current->mode = strtol(rawTarHeader.mode, NULL, 8);
        current->mtime = strtol(rawTarHeader.mtime, NULL, 8);
        current->offset = lseek(srcFd, 0 , SEEK_CUR);

        current->next = (headerL_t *) xmalloc(sizeof(headerL_t));
        current = current->next;
     	return(TRUE);
}
#endif

/*
 * identify Ar header (magic) and reset srcFd to entry position
 */
static int checkArMagic(int srcFd)
{
        off_t headerStart;
        char arMagic[8];

        headerStart = lseek(srcFd, 0, SEEK_CUR);
        if (fullRead(srcFd, arMagic, 8) != 8) {
                printf("fatal error/n");
                return (FALSE);
        }
        lseek(srcFd, headerStart, SEEK_SET);

        if (strncmp(arMagic,"!<arch>",7) != 0)
                return(FALSE);
        return(TRUE);
}

/*
 * get, check and correct the converted header
 */ 
static int readArEntry(int srcFd, headerL_t *entry)
{
	size_t nameLength;
        rawArHeader_t rawArHeader;
        off_t   initialOffset;

        initialOffset = lseek(srcFd, 0, SEEK_CUR);
        if (fullRead(srcFd, (char *) &rawArHeader, 60) != 60) {
                lseek(srcFd, initialOffset, SEEK_SET);
                return(FALSE);
        }
        if ((rawArHeader.fmag[0]!='`') || (rawArHeader.fmag[1]!='\n')) {
                lseek(srcFd, initialOffset, SEEK_SET);
                return(FALSE);
        }

        strncpy(entry->name, rawArHeader.name, 16);
        nameLength=strcspn(entry->name, " \\");
        entry->name[nameLength]='\0';
        parse_mode(rawArHeader.mode, &entry->mode);
        entry->mtime = atoi(rawArHeader.date);
        entry->uid = atoi(rawArHeader.uid);
        entry->gid = atoi(rawArHeader.gid);
        entry->size = (size_t) atoi(rawArHeader.size);
        entry->offset = initialOffset + (off_t) 60;

	nameLength = strcspn(entry->name, "/");
	
	/* handle GNU style short filenames, strip trailing '/' */
	if (nameLength > 0)
		entry->name[nameLength]='\0';
	
	/* handle GNU style long filenames */ 
	if (nameLength == 0) {
		/* escape from recursive call */
		if (entry->name[1]=='0') 
			return(TRUE);

		/* the data section contains the real filename */
		if (entry->name[1]=='/') {
			char tempName[MAX_NAME_LENGTH];

			if (entry->size > MAX_NAME_LENGTH)
				entry->size = MAX_NAME_LENGTH;
			fullRead(srcFd, tempName, entry->size);
			tempName[entry->size-3]='\0';
			
			/* read the second header for this entry */
			/* be carefull, this is recursive */
			if (readArEntry(srcFd, entry)==FALSE)
				return(FALSE);
		
			if ((entry->name[0]='/') && (entry->name[1]='0'))
				strcpy(entry->name, tempName);
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
static headerL_t *getHeaders(int srcFd, headerL_t *head, int funct)
{
#if defined BB_AR_EXPERIMENTAL_UNTAR
        int tar=FALSE;
#endif
	int ar=FALSE;
	headerL_t *list;
	off_t initialOffset;

        list = (headerL_t *) malloc(sizeof(headerL_t));
	initialOffset=lseek(srcFd, 0, SEEK_CUR);
	if (checkArMagic(srcFd)==TRUE) 
		ar=TRUE;

#if defined BB_AR_EXPERIMENTAL_UNTAR
	if (checkTarMagic(srcFd)==TRUE)
		tar=TRUE;

        if (tar==TRUE) {
                while(readTarHeader(srcFd, list)==TRUE) {
			off_t tarOffset;
                        list->next = (headerL_t *) malloc(sizeof(headerL_t));
                        *list->next = *head;
                        *head = *list;

                        /* recursive check for sub-archives */
                        if ((funct & RECURSIVE) == RECURSIVE)
                                head = getHeaders(srcFd, head, funct);
                        tarOffset = (off_t) head->size/512;
                        if ( head->size % 512 > 0)
                                tarOffset++;
                        tarOffset=tarOffset*512;
                        lseek(srcFd, head->offset + tarOffset, SEEK_SET);
                }
        }
#endif

        if (ar==TRUE) {
		lseek(srcFd, 8, SEEK_CUR); 
        	while(1) {
			if (readArEntry(srcFd, list) == FALSE) {
				lseek(srcFd, ++initialOffset, SEEK_CUR); 
				if (readArEntry(srcFd, list) == FALSE)
					return(head);
			}
			list->next = (headerL_t *) malloc(sizeof(headerL_t));
        		*list->next = *head;
			*head = *list;
			/* recursive check for sub-archives */
			if (funct & RECURSIVE)  
		        	head = getHeaders(srcFd, head, funct);
			lseek(srcFd, head->offset + head->size, SEEK_SET);
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

extern int ar_main(int argc, char **argv)
{
        int funct = 0, opt=0;
	int srcFd=0, dstFd=0;
 	headerL_t *header, *entry, *extractList;

	while ((opt = getopt(argc, argv, "ovtpxR")) != -1) {
		switch (opt) {
		case 'o':
			funct |= PRESERVE_DATE;
			break;
		case 'v':
			funct |= VERBOSE;
			break;
		case 't':
			funct |= DISPLAY;
			break;
		case 'x':
			funct |= EXT_TO_FILE;
			break;
		case 'p':
			funct |= EXT_TO_STDOUT;
			break;
		case 'R':
			funct |= RECURSIVE;
			break;
		default:
			usage(ar_usage);
		}
	}
 
        /* check the src filename was specified */
	if (optind == argc) {
                usage(ar_usage);
                return(FALSE);
        }
	
        if ( (srcFd = open(argv[optind], O_RDONLY)) < 0) {
          	errorMsg("Cannot read %s\n", optarg);
                return (FALSE);
        }
 	optind++;	
	entry = (headerL_t *) malloc(sizeof(headerL_t));
	header = (headerL_t *) malloc(sizeof(headerL_t));
	extractList = (headerL_t *) malloc(sizeof(headerL_t));	

	header = getHeaders(srcFd, header, funct);
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
		extractList = header;
	
        while(extractList->next != NULL) {	
		if (funct & EXT_TO_FILE) {
 			if (isDirectory(extractList->name, TRUE, NULL)==FALSE)
				createPath(extractList->name, 0666);
			dstFd = open(extractList->name, O_WRONLY | O_CREAT, extractList->mode);
			lseek(srcFd, extractList->offset, SEEK_SET);
        		copySubFile(srcFd, dstFd, (size_t) extractList->size);
		}
		if (funct & EXT_TO_STDOUT) {	
                   	lseek(srcFd, extractList->offset, SEEK_SET);
                        copySubFile(srcFd, fileno(stdout), (size_t) extractList->size);
		}
		if ( (funct & DISPLAY) || (funct & VERBOSE)) {
			if (funct & VERBOSE)
				printf("%s %d/%d %8d %s ", modeString(extractList->mode), 
					extractList->uid, extractList->gid,
					extractList->size, timeString(extractList->mtime));
		        printf("%s\n", extractList->name);
		}
		extractList=extractList->next;
	}
	return (TRUE);
}
