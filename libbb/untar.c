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
 *
 * NOTE: This is used by deb_extract to avoid calling the whole tar applet.
 * Its functionality should be merged with tar to avoid duplication
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libbb.h"

extern int untar(FILE *src_tar_file, int untar_function, char *base_path)
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

//	const int dpkg_deb_contents = 1;
//	const int dpkg_deb_control = 2;
//	const int dpkg_deb_info = 4;
	const int untar_extract = 8;
//	const int dpkg_deb_verbose_extract = 16;
//	const int dpkg_deb_list = 32;

	raw_tar_header_t raw_tar_header;
	unsigned char *temp = (unsigned char *) &raw_tar_header;
	long i;
	long next_header_offset = 0;
	long uncompressed_count = 0;
	size_t size;
	mode_t mode;

//	signal(SIGCHLD, child_died);

	while (fread((char *) &raw_tar_header, 1, 512, src_tar_file)==512) {
		long sum = 0;
		char *dir;

		uncompressed_count += 512;

		/* Check header has valid magic */
		if (strncmp(raw_tar_header.magic, "ustar", 5) != 0) {
			/* Put this pack after TODO (check child still alive) is done */
//			error_msg("Invalid tar magic");
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
        size = strtol(raw_tar_header.size, NULL, 8);
		parse_mode(raw_tar_header.mode, &mode);

		/* start of next header is at */
		next_header_offset = uncompressed_count + size;
		if (size % 512 != 0) {
			next_header_offset += (512 - size % 512);
		}

		/*
		 * seek to start of control file, return length
		 *
		if (dpkg_untar_function & dpkg_untar_seek_control) {
			if ((raw_tar_header.typeflag == '0') || (raw_tar_header.typeflag == '\0')) {
				char *tar_filename;

				tar_filename = strrchr(raw_tar_header.name, '/');
				if (tar_filename == NULL) {
					tar_filename = strdup(raw_tar_header.name);
				} else {
					tar_filename++;
				}

				if (strcmp(tar_filename, "control") == 0) {
					return(size);
				}
			}

		}
*/
		/* extract files */
		dir = xmalloc(strlen(raw_tar_header.name) + strlen(base_path) + 2);

		sprintf(dir, "%s/%s", base_path, raw_tar_header.name);
		create_path(dir, 0777);
		switch (raw_tar_header.typeflag ) {
			case '0':
			case '\0':
				/* If the name ends in a '/' then assume it is
				 * supposed to be a directory, and fall through
				 */
				if (raw_tar_header.name[strlen(raw_tar_header.name)-1] != '/') {
					if (untar_function & untar_extract) {
						int out_count=0;
						FILE *dst_file;
	
						dst_file = wfopen(dir, "w");
						if (copy_file_chunk(src_tar_file, dst_file, size) == FALSE) {
							error_msg_and_die("Couldnt extract file");
						}
						uncompressed_count += out_count;
						fclose(dst_file);
						break;
					}
				}
			case '5':
				if (untar_function & untar_extract) {
					if (create_path(dir, mode) != TRUE) {
						free(dir);
						perror_msg("%s: Cannot mkdir", raw_tar_header.name); 
						return(EXIT_FAILURE);
					}
				}
				break;
			case '1':
				if (untar_function & untar_extract) {
					if (link(raw_tar_header.linkname, raw_tar_header.name) < 0) {
						free(dir);
						perror_msg("%s: Cannot create hard link to '%s'", raw_tar_header.name, raw_tar_header.linkname); 
						return(EXIT_FAILURE);
					}
				}
				break;
			case '2':
				if (untar_function & untar_extract) {
					if (symlink(raw_tar_header.linkname, raw_tar_header.name) < 0) {
						free(dir);
						perror_msg("%s: Cannot create symlink to '%s'", raw_tar_header.name, raw_tar_header.linkname); 
						return(EXIT_FAILURE);
					}
				}
				break;
			case '3':
			case '4':
			case '6':
//				if (tarExtractSpecial( &header, extractFlag, tostdoutFlag)==FALSE)
//					errorFlag=TRUE;
//				break;
			default:
				error_msg("Unknown file type '%c' in tar file", raw_tar_header.typeflag);
				free(dir);
				return(EXIT_FAILURE);
		}
		free(dir);
	}
	/* skip to start of next header */
	while(uncompressed_count < next_header_offset) {
		char ch[2];
		if (fread(ch, 1, 1, src_tar_file) != 1) {
			error_msg("read error\n");
			getchar();
		} else {
			uncompressed_count++;
		}
	}
		/* 
		 * TODO: Check that gunzip child hasnt finished
		 */
	return(EXIT_SUCCESS);
}
