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
 */


#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include "internal.h"

#define AR_BLOCK_SIZE 60
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

static const char ar_usage[] = "ar [optxvV] archive [filenames] \n"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\nExtract or list files from an ar archive.\n\n"
	"Options:\n"
	"\to\t\tpreserve original dates\n"
	"\tp\t\textract to stdout\n"
	"\tt\t\tlist\n"
	"\tx\t\textract\n"
	"\tv\t\tverbosely list files processed\n"
#endif
	;

static void displayContents(struct ArInfo *entry, int funct)
{
	/* TODO convert mode to string */
	if ((funct & 2) == 2)
		printf("%i %i/%i %6i %s ", entry->mode, entry->uid, entry->gid,
			   entry->size, timeString(entry->date));
	printf("%s\n", entry->name);
}

/* Converts to new typed struct */
static int readArHeader(struct ArHeader *rawHeader, struct ArInfo *header)
{
	int count2;
	int count;

	for (count = 0; count < 16; count++) {
		if (rawHeader->ar_name[count] == ' ') {
			for (count2 = count; count2 < 16; count2++)
				if (!isspace(rawHeader->ar_name[count2]))
					break;
			if (count2 >= 16)
				break;
		}
		if (rawHeader->ar_name[count] == '/')
			break;
		header->name[count] = rawHeader->ar_name[count];
	}
	header->name[count] = '\0';
	header->date = atoi(rawHeader->ar_date);
	header->uid = atoi(rawHeader->ar_uid);
	header->gid = atoi(rawHeader->ar_gid);
	header->mode = atoi(rawHeader->ar_mode);
	header->size = atoi(rawHeader->ar_size);
	return (TRUE);
}

/*
 * Copy size bytes from current position if srcFd to current position in dstFd
 * taken from tarExtractRegularFile in tar.c
 * could be used for ar, tar and copyFile .  
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
			errorMsg(io_error, "copySubFile :", strerror(errno));
			return (FALSE);
		}
		doneSize = fullWrite(dstFd, buffer, writeSize);
		if (doneSize <= 0) {
			errorMsg(io_error, "copySubFile :", strerror(errno));
			return (FALSE);
		}
		copySize -= doneSize;
	}
	return (TRUE);
}

/*
 * Need to add checks, stat newfile before creation,change mode from 0777
 * extract to current dir
 * dstStat.st_size copied from current position of file pointed to by srcFd
 */
static int extractToFile(int srcFd, const char *path, const char *name,
						 int size)
{
	int dstFd, temp;
	struct stat tmpStat;
	char *pathname = NULL;

	if ((temp = isDirectory(path, TRUE, &tmpStat)) != TRUE) {
		if (!createPath(path, 0777)) {
			fatalError("Cannot extract to specified path");
			return (FALSE);
		}
	}
	temp = (strlen(path) + 16);
	pathname = (char *) xmalloc(temp);
	pathname = strcpy(pathname, path);
	pathname = strcat(pathname, &name[0]);
	dstFd = device_open(pathname, O_WRONLY | O_CREAT);
	pathname = NULL;
	temp = copySubFile(srcFd, dstFd, size);
	close(dstFd);
	return (TRUE);
}

/* Step through the ar file entries  */
static int readArFile(char **fileNames, int argc, int funct)
{
	int arFd = 0;
	int pdates = 0;
	int verbose = 0;
	int display = 0;
	int extract = 0;
	int extToStdout = 0;
	int status = 0;
	int found = 0;
	ArHeader rawArHeader;
	ArInfo arEntry;
	char arVersion[8];
	char *arName;
	char *selName[argc - 2];
	int i;

	if ((funct & 1) == 1)
		pdates = 1;
	if ((funct & 2) == 2)
		verbose = 1;
	if ((funct & 4) == 4)
		display = 1;
	if ((funct & 16) == 16) {	/* extract to stdout */
		extract = 1;
		extToStdout = 1;
	}
	if ((funct & 8) == 8) {		/* extract to file */
		extract = 1;
	}
	arName = fileNames[2];
	for (i = 0; i < (argc - 3); i++)
		selName[i] = fileNames[i + 3];
	arFd = open(arName, O_RDONLY);
	if (arFd < 0) {
		errorMsg("Error opening '%s': %s\n", arName, strerror(errno));
		return (FALSE);
	}
	if (fullRead(arFd, arVersion, 8) <= 0) {
		errorMsg("ar: Unexpected EOF in archive\n");
		return (FALSE);
	}
	if (strncmp(arVersion, "!<arch>", 7) != 0) {
		errorMsg("ar header fails check ");
		return (FALSE);
	}
	while ((status = fullRead(arFd, (char *) &rawArHeader, AR_BLOCK_SIZE))
		   == AR_BLOCK_SIZE) {
		readArHeader(&rawArHeader, &arEntry);

		if (display == 1) {
			displayContents(&arEntry, funct);
		}
		if (argc == 3)
			found = 1;
		else {
			found = 0;
			for (i = 0; i < (argc - 3); i++) {
				if ((status = (strcmp(selName[i], arEntry.name))) == 0)
					found = 1;
			}
		}
		if ((extract == 1) && (found == 1)) {
			if (extToStdout == 1) {
				copySubFile(arFd, fileno(stdout), arEntry.size);
			} else {
				extractToFile(arFd, "./", arEntry.name, arEntry.size);
			}
		} else
			lseek(arFd, arEntry.size, SEEK_CUR);
	}
	return (0);
}

extern int ar_main(int argc, char **argv)
{
	int ret = 0;
	char *opt_ptr;
	char c;
	int funct = 0;

	if (argc < 2)
		usage(ar_usage);

	opt_ptr = argv[1];
	if (*opt_ptr == '-')
		++opt_ptr;
	while ((c = *opt_ptr++) != '\0') {
		switch (c) {
		case 'o':				/* preserver original dates */
			funct = funct | 1;
			break;
		case 'p':				/* extract to stdout */
			funct = funct | 16;
			break;
		case 't':				/* display contents */
			funct = funct | 4;
			break;
		case 'x':				/* extract contents of archive */
			funct = funct | 8;
			break;
		case 'v':				/* be verbose */
			funct = funct | 2;
			break;
		default:
			usage(ar_usage);
		}
	}
	if (funct > 3)
		ret = readArFile(argv, argc, funct);
	return (ret);
}
