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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libbb.h"

file_headers_t *get_tar_headers(FILE *tar_stream)
{
	typedef struct raw_tar_header {
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
	} raw_tar_header_t;

	raw_tar_header_t raw_tar_header;
	unsigned char *temp = (unsigned char *) &raw_tar_header;
	file_headers_t *tar_headers_list = (file_headers_t *) calloc(1, sizeof(file_headers_t));
	file_headers_t *tar_entry;
	long i;
	long next_header_offset = 0;
	long current_offset = 0;

	while (fread((char *) &raw_tar_header, 1, 512, tar_stream) == 512) {
		long sum = 0;
		current_offset += 512;

		/* Check header has valid magic, unfortunately some tar files
		 * have empty (0'ed) tar entries at the end, which will
		 * cause this to fail, so fail silently for now
		 */
		if (strncmp(raw_tar_header.magic, "ustar", 5) != 0) {
//			error_msg("invalid magic, [%s]\n", raw_tar_header.magic);
			break;
		}

		/* Do checksum on headers */
        for (i =  0; i < 148 ; i++) {
			sum += temp[i];
		}
        sum += ' ' * 8;
		for (i =  156; i < 512 ; i++) {
			sum += temp[i];
		}
        if (sum != strtol(raw_tar_header.chksum, NULL, 8)) {
			error_msg("Invalid tar header checksum");
			break;
		}

		/* convert to type'ed variables */
		tar_entry = xcalloc(1, sizeof(file_headers_t));
		tar_entry->name = xstrdup(raw_tar_header.name);

		tar_entry->offset = (off_t) current_offset;
		parse_mode(raw_tar_header.mode, &tar_entry->mode);
		tar_entry->uid   = strtol(raw_tar_header.uid, NULL, 8);
		tar_entry->gid   = strtol(raw_tar_header.gid, NULL, 8);
		tar_entry->size  = strtol(raw_tar_header.size, NULL, 8);
		tar_entry->mtime = strtol(raw_tar_header.mtime, NULL, 8);
		tar_entry->link_name  = xstrdup(raw_tar_header.linkname);
//		tar_entry->devmajor  = strtol(rawHeader->devmajor, NULL, 8);
//		tar_entry->devminor  = strtol(rawHeader->devminor, NULL, 8);

		tar_headers_list = append_archive_list(tar_headers_list, tar_entry);

		/* start of next header is at */
		next_header_offset = current_offset + tar_entry->size;

		if (tar_entry->size % 512 != 0) {
			next_header_offset += (512 - tar_entry->size % 512);
		}
		/*
		 * Seek to start of next block, cant use fseek as unzip() does support it
		 */
		while (current_offset < next_header_offset) {
			if (fgetc(tar_stream) == EOF) {
				break;
			}
			current_offset++;
		}
	}
	return(tar_headers_list);
}

