/* vi: set sw=4 ts=4: */
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libbb.h"

file_headers_t *get_ar_headers(FILE *in_file)
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

	file_headers_t *ar_list, *ar_entry;
	char ar_magic[8];
	char *long_name=NULL;
	
	/* check ar magic */
	if (fread(ar_magic, 1, 8, in_file) != 8) {
		error_msg("cannot read magic");
		return(NULL);
	}

	if (strncmp(ar_magic,"!<arch>",7) != 0) {
		error_msg("invalid magic");
		return(NULL);
	}
	
	ar_list = (file_headers_t *) xcalloc(1, sizeof(file_headers_t));

	while (fread((char *) &raw_ar_header, 1, 60, in_file) == 60) {
		ar_entry = (file_headers_t *) xcalloc(1, sizeof(file_headers_t));
		/* check the end of header markers are valid */
		if ((raw_ar_header.fmag[0] != '`') || (raw_ar_header.fmag[1] != '\n')) {
			char newline;
			if (raw_ar_header.fmag[1] != '`') {
				break;
			}
			/* some version of ar, have an extra '\n' after each entry */
			fread(&newline, 1, 1, in_file);
			if (newline!='\n') {
				break;
			}
			/* fix up the header, we started reading 1 byte too early due to a '\n' */
			memmove((char *) &raw_ar_header, (char *)&raw_ar_header+1, 59);
			/* dont worry about adding the last '\n', we dont need it now */
		}
		
		ar_entry->size = (size_t) atoi(raw_ar_header.size);
		/* long filenames have '/' as the first character */
		if (raw_ar_header.name[0] == '/') {
			if (raw_ar_header.name[1] == '/') {
				/* multiple long filenames are stored as data in one entry */
				long_name = (char *) xrealloc(long_name, ar_entry->size);
				fread(long_name, 1, ar_entry->size, in_file);
				continue;
			}
			else {
				/* The number after the '/' indicates the offset in the ar data section
					(saved in variable long_name) that conatains the real filename */
				const int long_name_offset = (int) atoi((char *) &raw_ar_header.name[1]);
				ar_entry->name = xmalloc(strlen(&long_name[long_name_offset]));
				strcpy(ar_entry->name, &long_name[long_name_offset]);
			}
		}
		else {
			/* short filenames */
			ar_entry->name = xmalloc(16);
			ar_entry->name = strncpy(ar_entry->name, raw_ar_header.name, 16);
		}
		ar_entry->name[strcspn(ar_entry->name, " /")]='\0';

		/* convert the rest of the now valid char header to its typed struct */	
		parse_mode(raw_ar_header.mode, &ar_entry->mode);
		ar_entry->mtime = atoi(raw_ar_header.date);
		ar_entry->uid = atoi(raw_ar_header.uid);
		ar_entry->gid = atoi(raw_ar_header.gid);
		ar_entry->offset = ftell(in_file);
		ar_entry->next = NULL;

		fseek(in_file, (off_t) ar_entry->size, SEEK_CUR);

		ar_list = append_archive_list(ar_list, ar_entry);
	}
	return(ar_list);
}
