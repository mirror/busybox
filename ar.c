/* vi: set sw=4 ts=4: */
/*
 * Mini ar implementation for busybox 
 *
 * Copyright (C) 2000 by Glenn McGrath
 * Written by Glenn McGrath <bug1@optushome.com.au> 1 June 2000
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
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include "busybox.h"

typedef struct ar_headers_s {
	char *name;
	size_t size;
	uid_t uid;
	gid_t gid;
	mode_t mode;
	time_t mtime;
	off_t offset;
	struct ar_headers_s *next;
} ar_headers_t;

/*
 * return the headerL_t struct for the filename descriptor
 */
extern ar_headers_t get_ar_headers(int srcFd)
{
	typedef struct raw_ar_header_s {	/* Byte Offset */
		char name[16];	/*  0-15 */
		char date[12];	/* 16-27 */
		char uid[6];	
		char gid[6];	/* 28-39 */
		char mode[8];	/* 40-47 */
		char size[10];	/* 48-57 */
		char fmag[2];	/* 58-59 */
	} raw_ar_header_t;

	raw_ar_header_t raw_ar_header;

	ar_headers_t *head, *entry;
	char ar_magic[8];
	char *long_name=NULL;
	
	head = (ar_headers_t *) xmalloc(sizeof(ar_headers_t));
	entry = (ar_headers_t *) xmalloc(sizeof(ar_headers_t));
	
	/* check ar magic */
	if (full_read(srcFd, ar_magic, 8) != 8) {
		error_msg_and_die("cannot read magic");
	}

	if (strncmp(ar_magic,"!<arch>",7) != 0) {
		error_msg_and_die("invalid magic");
	}

	while (full_read(srcFd, (char *) &raw_ar_header, 60)==60) {
		/* check the end of header markers are valid */
		if ((raw_ar_header.fmag[0]!='`') || (raw_ar_header.fmag[1]!='\n')) {
			char newline;
			if (raw_ar_header.fmag[1]!='`') {
				break;
			}
			/* some version of ar, have an extra '\n' after each entry */
			read(srcFd, &newline, 1);
			if (newline!='\n') {
				break;
			}
			/* fix up the header, we started reading 1 byte too early due to a '\n' */
			memmove((char *) &raw_ar_header, (char *)&raw_ar_header+1, 59);
			/* dont worry about adding the last '\n', we dont need it now */
		}
		
		entry->size = (size_t) atoi(raw_ar_header.size);
		/* long filenames have '/' as the first character */
		if (raw_ar_header.name[0] == '/') {
			if (raw_ar_header.name[1] == '/') {
				/* multiple long filenames are stored as data in one entry */
				long_name = (char *) xrealloc(long_name, entry->size);
				full_read(srcFd, long_name, entry->size);
				continue;
			}
			else {
				/* The number after the '/' indicates the offset in the ar data section
					(saved in variable long_name) that conatains the real filename */
				const int long_name_offset = (int) atoi((char *) &raw_ar_header.name[1]);
				entry->name = xmalloc(strlen(&long_name[long_name_offset]));
				strcpy(entry->name, &long_name[long_name_offset]);
			}
		}
		else {
			/* short filenames */
			entry->name = xmalloc(16);
			strncpy(entry->name, raw_ar_header.name, 16);
		}
		entry->name[strcspn(entry->name, " /")]='\0';

		/* convert the rest of the now valid char header to its typed struct */	
		parse_mode(raw_ar_header.mode, &entry->mode);
		entry->mtime = atoi(raw_ar_header.date);
		entry->uid = atoi(raw_ar_header.uid);
		entry->gid = atoi(raw_ar_header.gid);
		entry->offset = lseek(srcFd, 0, SEEK_CUR);

		/* add this entries header to our combined list */
		entry->next = (ar_headers_t *) xmalloc(sizeof(ar_headers_t));
		*entry->next = *head;
		*head = *entry;
		lseek(srcFd, (off_t) entry->size, SEEK_CUR);
	}
	return(*head);
}

extern int ar_main(int argc, char **argv)
{
	const int preserve_date = 1;	/* preserve original dates */
	const int verbose = 2;		/* be verbose */
	const int display = 4;		/* display contents */
	const int extract_to_file = 8;	/* extract contents of archive */
	const int extract_to_stdout = 16;	/* extract to stdout */

	int funct = 0, opt=0;
	int srcFd=0, dstFd=0;

	ar_headers_t head, *extract_list=NULL;

	extract_list = (ar_headers_t *) xmalloc(sizeof(ar_headers_t));

	while ((opt = getopt(argc, argv, "ovtpx")) != -1) {
		switch (opt) {
		case 'o':
			funct |= preserve_date;
			break;
		case 'v':
			funct |= verbose;
			break;
		case 't':
			funct |= display;
			break;
		case 'p':
			funct |= extract_to_stdout;
			break;
		case 'x':
			funct |= extract_to_file;
			break;
		default:
			usage(ar_usage);
		}
	}
 
	/* check the src filename was specified */
	if (optind == argc)
		usage(ar_usage);
	
	if ( (srcFd = open(argv[optind], O_RDONLY)) < 0)
		error_msg_and_die("Cannot read %s", argv[optind]);

	optind++;	
	head = get_ar_headers(srcFd);

	/* find files to extract or display */
	/* search through argv and build extract list */
	for (;optind<argc; optind++) {
		ar_headers_t *ar_entry;
		ar_entry = (ar_headers_t *) xmalloc(sizeof(ar_headers_t));
		ar_entry = &head;
		while (ar_entry->next != NULL) {
			if (strcmp(argv[optind], ar_entry->name) == 0) {
				ar_headers_t *tmp;
				tmp = (ar_headers_t *) xmalloc(sizeof(ar_headers_t));
				*tmp = *extract_list;
				*extract_list = *ar_entry;
				extract_list->next = tmp;
				break;					
			}
			ar_entry=ar_entry->next;
		}
	}

	/* if individual files not found extract all files */	
	if (extract_list->next==NULL) {
		extract_list = &head;
	}
	
	/* find files to extract or display */	
	while (extract_list->next != NULL) {
		if (funct & extract_to_file) {
			dstFd = open(extract_list->name, O_WRONLY | O_CREAT, extract_list->mode);				
		}
		else if (funct & extract_to_stdout) {
			dstFd = fileno(stdout);
		}
		if ((funct & extract_to_file) || (funct & extract_to_stdout)) {
			lseek(srcFd, extract_list->offset, SEEK_SET);
			copy_file_chunk(srcFd, dstFd, (size_t) extract_list->size);			
		}
		if (funct & verbose) {
			printf("%s %d/%d %8d %s ", mode_string(extract_list->mode), 
				extract_list->uid, extract_list->gid,
				extract_list->size, time_string(extract_list->mtime));
		}
		if ((funct & display) || (funct & verbose)){
			printf("%s\n", extract_list->name);
		}
		extract_list=extract_list->next;
	}
	return EXIT_SUCCESS;
}
