/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unarchive.h"
#include "libbb.h"

file_header_t *get_header_ar(FILE *src_stream)
{
	file_header_t *typed;
	union {
		char raw[60];
	 	struct {
 			char name[16];
 			char date[12];
 			char uid[6];
 			char gid[6];
 			char mode[8];
 			char size[10];
 			char magic[2];
 		} formated;
	} ar;
	static char *ar_long_names;

	if (fread(ar.raw, 1, 60, src_stream) != 60) {
		return(NULL);
	}
	archive_offset += 60;
	/* align the headers based on the header magic */
	if ((ar.formated.magic[0] != '`') || (ar.formated.magic[1] != '\n')) {
		/* some version of ar, have an extra '\n' after each data entry,
		 * this puts the next header out by 1 */
		if (ar.formated.magic[1] != '`') {
			error_msg("Invalid magic");
			return(NULL);
		}
		/* read the next char out of what would be the data section,
		 * if its a '\n' then it is a valid header offset by 1*/
		archive_offset++;
		if (fgetc(src_stream) != '\n') {
			error_msg("Invalid magic");
			return(NULL);
		}
		/* fix up the header, we started reading 1 byte too early */
		/* raw_header[60] wont be '\n' as it should, but it doesnt matter */
		memmove(ar.raw, &ar.raw[1], 59);
	}
		
	typed = (file_header_t *) xcalloc(1, sizeof(file_header_t));

	typed->size = (size_t) atoi(ar.formated.size);
	/* long filenames have '/' as the first character */
	if (ar.formated.name[0] == '/') {
		if (ar.formated.name[1] == '/') {
			/* If the second char is a '/' then this entries data section
			 * stores long filename for multiple entries, they are stored
			 * in static variable long_names for use in future entries */
			ar_long_names = (char *) xrealloc(ar_long_names, typed->size);
			fread(ar_long_names, 1, typed->size, src_stream);
			archive_offset += typed->size;
			/* This ar entries data section only contained filenames for other records
			 * they are stored in the static ar_long_names for future reference */
			return (get_header_ar(src_stream)); /* Return next header */
		} else if (ar.formated.name[1] == ' ') {
			/* This is the index of symbols in the file for compilers */
			seek_sub_file(src_stream, typed->size);
			return (get_header_ar(src_stream)); /* Return next header */
		} else {
			/* The number after the '/' indicates the offset in the ar data section
			(saved in variable long_name) that conatains the real filename */
			if (!ar_long_names) {
				error_msg("Cannot resolve long file name");
				return (NULL);
			}
			typed->name = xstrdup(ar_long_names + atoi(&ar.formated.name[1]));
		}
	} else {
		/* short filenames */
		typed->name = xcalloc(1, 16);
		strncpy(typed->name, ar.formated.name, 16);
	}
	typed->name[strcspn(typed->name, " /")]='\0';

	/* convert the rest of the now valid char header to its typed struct */	
	parse_mode(ar.formated.mode, &typed->mode);
	typed->mtime = atoi(ar.formated.date);
	typed->uid = atoi(ar.formated.uid);
	typed->gid = atoi(ar.formated.gid);

	return(typed);
}