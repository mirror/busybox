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

#ifdef CONFIG_FEATURE_TAR_GNU_EXTENSIONS
static char *longname = NULL;
static char *linkname = NULL;
#endif

extern char get_header_tar(archive_handle_t *archive_handle)
{
	file_header_t *file_header = archive_handle->file_header;
	union {
		/* ustar header, Posix 1003.1 */
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
	long sum = 0;
	long i;

	/* Align header */
	data_align(archive_handle, 512);

	if (bb_full_read(archive_handle->src_fd, tar.raw, 512) != 512) {
		/* Assume end of file */
		return(EXIT_FAILURE);
	}
	archive_handle->offset += 512;

	/* If there is no filename its an empty header */
	if (tar.formated.name[0] == 0) {
		return(EXIT_SUCCESS);
	}

	/* Check header has valid magic, "ustar" is for the proper tar
	 * 0's are for the old tar format
	 */
	if (strncmp(tar.formated.magic, "ustar", 5) != 0) {
#ifdef CONFIG_FEATURE_TAR_OLDGNU_COMPATABILITY
		if (strncmp(tar.formated.magic, "\0\0\0\0\0", 5) != 0)
#endif
			bb_error_msg_and_die("Invalid tar magic");
	}
	/* Do checksum on headers */
	for (i =  0; i < 148 ; i++) {
		sum += tar.raw[i];
	}
	sum += ' ' * 8;
	for (i =  156; i < 512 ; i++) {
		sum += tar.raw[i];
	}
	if (sum != strtol(tar.formated.chksum, NULL, 8)) {
		bb_error_msg("Invalid tar header checksum");
		return(EXIT_FAILURE);
	}

#ifdef CONFIG_FEATURE_TAR_GNU_EXTENSIONS
	if (longname) {
		file_header->name = longname;
		longname = NULL;
	}
	else if (linkname) {
		file_header->name = linkname;
		linkname = NULL;
	} else
#endif
	if (tar.formated.prefix[0] == 0) {
		file_header->name = strdup(tar.formated.name);
	} else {
		file_header->name = concat_path_file(tar.formated.prefix, tar.formated.name);
	}

	{	/* Strip trailing '/' in directories */
		char *tmp = last_char_is(file_header->name, '/');
		if (tmp) {
			*tmp = '\0';
		}
	}


	file_header->mode = strtol(tar.formated.mode, NULL, 8);
	file_header->uid = strtol(tar.formated.uid, NULL, 8);
	file_header->gid = strtol(tar.formated.gid, NULL, 8);
	file_header->size = strtol(tar.formated.size, NULL, 8);
	file_header->mtime = strtol(tar.formated.mtime, NULL, 8);
	file_header->link_name = (tar.formated.linkname[0] != '\0') ? 
	    bb_xstrdup(tar.formated.linkname) : NULL;
	file_header->device = (dev_t) ((strtol(tar.formated.devmajor, NULL, 8) << 8) +
				 strtol(tar.formated.devminor, NULL, 8));

	/* Fix mode, used by the old format */
	switch (tar.formated.typeflag) {
# ifdef CONFIG_FEATURE_TAR_OLDGNU_COMPATABILITY
	case 0:
	case '0':
		if (last_char_is(file_header->name, '/')) {
			file_header->mode |= S_IFDIR;
		} else {
			file_header->mode |= S_IFREG;
		}
		break;
	case '2':
		file_header->mode |= S_IFLNK;
		break;
	case '3':
		file_header->mode |= S_IFCHR;
		break;
	case '4':
		file_header->mode |= S_IFBLK;
		break;
	case '5':
		file_header->mode |= S_IFDIR;
		break;
	case '6':
		file_header->mode |= S_IFIFO;
		break;
# endif
	/* hard links are detected as entries with 0 size, a link name, 
	 * and not being a symlink, hence we have nothing to do here */
	case '1':
		file_header->mode |= ~S_IFLNK;
		break;
# ifdef CONFIG_FEATURE_TAR_GNU_EXTENSIONS
	case 'L': {
			longname = xmalloc(file_header->size + 1);
			archive_xread_all(archive_handle, longname, file_header->size);
			longname[file_header->size] = '\0';
			archive_handle->offset += file_header->size;

			return(get_header_tar(archive_handle));
		}
	case 'K': {
			linkname = xmalloc(file_header->size + 1);
			archive_xread_all(archive_handle, linkname, file_header->size);
			linkname[file_header->size] = '\0';
			archive_handle->offset += file_header->size;

			file_header->name = linkname;
			return(get_header_tar(archive_handle));
		}
	case 'D':
	case 'M':
	case 'N':
	case 'S':
	case 'V':
		bb_error_msg("Ignoring GNU extension type %c", tar.formated.typeflag);
# endif
	}

	if (archive_handle->filter(archive_handle) == EXIT_SUCCESS) {
		archive_handle->action_header(archive_handle->file_header);
		archive_handle->flags |= ARCHIVE_EXTRACT_QUIET;
		archive_handle->action_data(archive_handle);
		archive_handle->passed = llist_add_to(archive_handle->passed, file_header->name);
	} else {
		data_skip(archive_handle);			
	}
	archive_handle->offset += file_header->size;

	free(file_header->link_name);

	return(EXIT_SUCCESS);
}
