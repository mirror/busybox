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
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include "busybox.h"

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
			show_usage();
		}
	}
 
	/* check the src filename was specified */
	if (optind == argc)
		show_usage();
	
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
			copy_file_chunk(srcFd, dstFd, (off_t) extract_list->size);			
		}
		if (funct & verbose) {
			printf("%s %d/%d %8d %s ", mode_string(extract_list->mode), 
				extract_list->uid, extract_list->gid,
				(int) extract_list->size, time_string(extract_list->mtime));
		}
		if ((funct & display) || (funct & verbose)){
			printf("%s\n", extract_list->name);
		}
		extract_list=extract_list->next;
	}
	return EXIT_SUCCESS;
}
