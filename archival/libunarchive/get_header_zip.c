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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unarchive.h"
#include "libbb.h"

#define ZIP_FILEHEADER_MAGIC	0x04034b50
#define ZIP_CDS_MAGIC			0x02014b50
#define ZIP_CDS_END_MAGIC		0x06054b50
#define ZIP_DD_MAGIC			0x08074b50


enum {
	zh_version,
	zh_flags,
	zh_method,
	zh_modtime,
	zh_moddate,
	zh_crc32,
	zh_cmpsize,
	zh_ucmpsize,
	zh_filename_len,
	zh_extra_len
};

static const char fail_msg[] = "Invalid file or read error";

static int le_read_int(FILE *fp, int n)
{
	int r = 0;
	int s = 1;
	int c;

	archive_offset += n;

	do {
		if ((c = fgetc(fp)) == EOF) {
			error_msg_and_die(fail_msg);
		}
		r += s * c;
		s <<= 8;
	} while (--n);

	return r;
}

static void read_header(FILE *fp, int *zh)
{
	static const char s[] = { 2, 2, 2, 2, 2, 4, 4, 4, 2, 2, 0 };
	int i = 0;

	do {
		zh[i] = le_read_int(fp, s[i]);
	} while (s[++i]);
}

file_header_t *get_header_zip(FILE *zip_stream)
{
	int zip_header[10];
	file_header_t *zip_entry = NULL;
	int magic, tmpmagic;
	/* If dd_ahead is true, we didn't know the last extracted file's length. */
	static int dd_ahead = 0;

	magic = le_read_int(zip_stream, 4);

 checkmagic:
	switch (magic) {
		case ZIP_FILEHEADER_MAGIC:
			zip_entry = xcalloc(1, sizeof(file_header_t));

			read_header(zip_stream, zip_header);

			if (zip_header[zh_method] == 8) {
				zip_entry->extract_func = &inflate;
			} else if (zip_header[zh_method] != 0) {
				error_msg_and_die("Unsupported compression method %d\n",
								  zip_header[zh_method]);
			}

			zip_entry->name = xmalloc(zip_header[zh_filename_len] + 1);
			if (!fread(zip_entry->name, zip_header[zh_filename_len], 1, zip_stream)) {
				error_msg_and_die(fail_msg);
			}
			archive_offset += zip_header[zh_filename_len];

			seek_sub_file(zip_stream, zip_header[zh_extra_len]);

			zip_entry->size = zip_header[zh_cmpsize];

			zip_entry->mode = S_IFREG | 0777;

			/* Time/Date? */

			zip_entry->name[zip_header[zh_filename_len]] = 0;
			if (*(zip_entry->name + zip_header[zh_filename_len] - 1) == '/') {
				/* Files that end in a / are directories */
				/* Remove trailing / so unarchive doesn't get confused */
				*(zip_entry->name + zip_header[zh_filename_len] - 1) = '\0';
				zip_entry->mode ^= S_IFREG;
				zip_entry->mode |= S_IFDIR;
			}

			if (zip_header[zh_flags] & 0x8) {
				/* crc32, and sizes are in the data description _after_ the file */
				if (zip_header[zh_cmpsize] == 0) {
					/* As we don't know how long this file it is difficult to skip!
					 * but it is compressed, so normally its ok */
					dd_ahead = 1;
				}
				if (zip_header[zh_ucmpsize] != 0) {
					/* Humm... we would otherwise skip this twice - not good! */
					dd_ahead = 2;
				}
			}
			break;

		case ZIP_CDS_MAGIC: /* FALLTHRU */
		case ZIP_CDS_END_MAGIC:
			return(NULL);
			break;

		case ZIP_DD_MAGIC: {
			int cmpsize;
			seek_sub_file(zip_stream, 4); // Skip crc32
			cmpsize = le_read_int(zip_stream, 4);
			if (dd_ahead == 1) archive_offset += cmpsize;
			seek_sub_file(zip_stream, 4); // Skip uncompressed size
			dd_ahead = 0;
			return (get_header_zip(zip_stream));
			break; }

		default:
			if (!dd_ahead) error_msg("Invalid magic (%#x): Trying to skip junk", magic);
			dd_ahead = 0;
			while ((tmpmagic = fgetc(zip_stream)) != EOF) {
				archive_offset++;
				magic = ((magic >> 8) & 0x00ffffff) | ((tmpmagic << 24) & 0xff000000);
				if (magic == ZIP_FILEHEADER_MAGIC
					|| magic == ZIP_CDS_MAGIC
					|| magic == ZIP_CDS_END_MAGIC) {
					goto checkmagic;
				}
			}
			error_msg_and_die(fail_msg);
	}

	return(zip_entry);
}
