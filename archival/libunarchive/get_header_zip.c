/* vi: set sw=4 ts=4: */
/*
 * get_header_zip for busybox
 *
 * Copyright (C) 2001 by Laurence Anderson
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unarchive.h"
#include "libbb.h"

#define ZIP_FILEHEADER_MAGIC 0x04034b50
#define ZIP_CDS_MAGIC 0x02014b50
#define ZIP_CDS_END_MAGIC 0x06054b50
#define ZIP_DD_MAGIC 0x8074b50

file_header_t *get_header_zip(FILE *zip_stream)
{
	struct {
		short version __attribute__ ((packed));
		short flags __attribute__ ((packed));
		short method __attribute__ ((packed));
		short modtime __attribute__ ((packed));
		short moddate __attribute__ ((packed));
		int crc32 __attribute__ ((packed));
		int cmpsize __attribute__ ((packed));
		int ucmpsize __attribute__ ((packed));
		short filename_len __attribute__ ((packed));
		short extra_len __attribute__ ((packed));
	} zip_header;
	file_header_t *zip_entry = NULL;
	int magic;
	static int dd_ahead = 0; // If this is true, the we didn't know how long the last extraced file was

	fread (&magic, 4, 1, zip_stream);
	archive_offset += 4;

	if (feof(zip_stream)) return(NULL);
checkmagic:
	switch (magic) {
		case ZIP_FILEHEADER_MAGIC:
			zip_entry = xcalloc(1, sizeof(file_header_t));
			fread (&zip_header, sizeof(zip_header), 1, zip_stream);
			archive_offset += sizeof(zip_header);
			if (!(zip_header.method == 8 || zip_header.method == 0)) { printf("Unsupported compression method %d\n", zip_header.method); return(NULL); }
			zip_entry->name = calloc(zip_header.filename_len + 1, sizeof(char));
			fread (zip_entry->name, sizeof(char), zip_header.filename_len, zip_stream);
			archive_offset += zip_header.filename_len;
			seek_sub_file(zip_stream, zip_header.extra_len);
			zip_entry->size = zip_header.cmpsize;
			if (zip_header.method == 8) zip_entry->extract_func = &inflate;
			zip_entry->mode = S_IFREG | 0777;
			// Time/Date?
			if (*(zip_entry->name + strlen(zip_entry->name) - 1) == '/') { // Files that end in a / are directories
				zip_entry->mode ^= S_IFREG;
				zip_entry->mode |= S_IFDIR;
				*(zip_entry->name + strlen(zip_entry->name) - 1) = '\0'; // Remove trailing / so unarchive doesn't get confused
			}
			//printf("cmpsize: %d, ucmpsize: %d, method: %d\n", zip_header.cmpsize, zip_header.ucmpsize, zip_header.method);
			if (zip_header.flags & 0x8) { // crc32, and sizes are in the data description _after_ the file
				if (zip_header.cmpsize == 0) dd_ahead = 1; // As we don't know how long this file it is difficult to skip! but it is compressed, so normally its ok
				if (zip_header.ucmpsize != 0) dd_ahead = 2; // Humm... we would otherwise skip this twice - not good!
			}
			break;
		case ZIP_CDS_MAGIC: /* FALLTHRU */
		case ZIP_CDS_END_MAGIC:
			return(NULL);
			break;
		case ZIP_DD_MAGIC: {
			int cmpsize;
			seek_sub_file(zip_stream, 4); // Skip crc32
			fread(&cmpsize, 4, 1, zip_stream);
			archive_offset += 4;
			if (dd_ahead == 1) archive_offset += cmpsize;
			seek_sub_file(zip_stream, 4); // Skip uncompressed size
			dd_ahead = 0;
			return (get_header_zip(zip_stream));
			break; }
		default:
			if (!dd_ahead) error_msg("Invalid magic (%#x): Trying to skip junk", magic);
			dd_ahead = 0;
			while (!feof(zip_stream)) {
				int tmpmagic;
				tmpmagic = fgetc(zip_stream);
				archive_offset++;
				magic = ((magic >> 8) & 0x00ffffff) | ((tmpmagic << 24) & 0xff000000);
				if (magic == ZIP_FILEHEADER_MAGIC || magic == ZIP_CDS_MAGIC || magic == ZIP_CDS_END_MAGIC) goto checkmagic;
			}
			error_msg("End of archive reached: Bad archive");
			return(NULL);
	}
	//if (archive_offset != ftell(zip_stream)) printf("Archive offset out of sync (%d,%d)\n", (int) archive_offset, (int) ftell(zip_stream));
	return(zip_entry);
}
