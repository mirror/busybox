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

file_header_t *get_header_tar(FILE * tar_stream)
{
	union {
		unsigned char raw[512];
		struct {
			char name[100];	/*   0-99 */
			char mode[8];	/* 100-107 */
			char uid[8];	/* 108-115 */
			char gid[8];	/* 116-123 */
			char size[12];	/* 124-135 */
			char mtime[12];	/* 136-147 */
			char chksum[8];	/* 148-155 */
			char typeflag;	/* 156-156 */
			char linkname[100];	/* 157-256 */
			char magic[6];	/* 257-262 */
			char version[2];	/* 263-264 */
			char uname[32];	/* 265-296 */
			char gname[32];	/* 297-328 */
			char devmajor[8];	/* 329-336 */
			char devminor[8];	/* 337-344 */
			char prefix[155];	/* 345-499 */
			char padding[12];	/* 500-512 */
		} formated;
	} tar;
	file_header_t *tar_entry = NULL;
	long sum = 0;
	long i;

	if (archive_offset % 512 != 0) {
		seek_sub_file(tar_stream, 512 - (archive_offset % 512));
	}

	if (fread(tar.raw, 1, 512, tar_stream) != 512) {
		/* Unfortunatly its common for tar files to have all sorts of
		 * trailing garbage, fail silently */
		return (NULL);
	}
	archive_offset += 512;

	/* Check header has valid magic, "ustar" is for the proper tar
	 * 0's are for the old tar format
	 */
	if (strncmp(tar.formated.magic, "ustar", 5) != 0) {
#ifdef CONFIG_FEATURE_TAR_OLD_FORMAT
		if (strncmp(tar.formated.magic, "\0\0\0\0\0", 5) != 0)
#endif
			return (NULL);
	}

	/* If there is no filename its an empty header, skip it */
	if (tar.formated.name[0] == 0) {
		return (NULL);
	}

	/* Do checksum on headers */
	for (i = 0; i < 148; i++) {
		sum += tar.raw[i];
	}
	sum += ' ' * 8;
	for (i = 156; i < 512; i++) {
		sum += tar.raw[i];
	}
	if (sum != strtol(tar.formated.chksum, NULL, 8)) {
		error_msg("Invalid tar header checksum");
		return (NULL);
	}

	/* convert to type'ed variables */
	tar_entry = xcalloc(1, sizeof(file_header_t));
	if (tar.formated.prefix[0] == 0) {
		tar_entry->name = xstrdup(tar.formated.name);
	} else {
		tar_entry->name =
			concat_path_file(tar.formated.prefix, tar.formated.name);
	}

	tar_entry->mode = strtol(tar.formated.mode, NULL, 8);
#ifdef CONFIG_FEATURE_TAR_OLD_FORMAT
	switch (tar.formated.typeflag) {
	case 0:
		tar_entry->mode |= S_IFREG;
		break;
	case 1:
		error_msg("internal hard link not handled\n");
		break;
	case 2:
		tar_entry->mode |= S_IFLNK;
		break;
	case 3:
		tar_entry->mode |= S_IFCHR;
		break;
	case 4:
		tar_entry->mode |= S_IFBLK;
		break;
	case 5:
		tar_entry->mode |= S_IFDIR;
		break;
	case 6:
		tar_entry->mode |= S_IFIFO;
		break;
	}
#endif
	tar_entry->uid = strtol(tar.formated.uid, NULL, 8);
	tar_entry->gid = strtol(tar.formated.gid, NULL, 8);
	tar_entry->size = strtol(tar.formated.size, NULL, 8);
	tar_entry->mtime = strtol(tar.formated.mtime, NULL, 8);
	tar_entry->link_name =
		strlen(tar.formated.linkname) ? xstrdup(tar.formated.linkname) : NULL;
	tar_entry->device =
		(strtol(tar.formated.devmajor, NULL, 8) << 8) +
		strtol(tar.formated.devminor, NULL, 8);

	return (tar_entry);
}
