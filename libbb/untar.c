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

/*
 * stdout can be redircted to FILE *output
 * If const char *file_prefix isnt NULL is prepended to all the extracted filenames.
 * The purpose of const char *argument depends on the value of const int untar_function
 */

extern char *untar(FILE *src_tar_file, FILE *output, const int untar_function,
	const char *argument, const char *file_prefix)
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
	long i;
	long next_header_offset = 0;
	long uncompressed_count = 0;
	size_t size;
	mode_t mode;

	while (fread((char *) &raw_tar_header, 1, 512, src_tar_file) == 512) {
		long sum = 0;
		
		if (ferror(src_tar_file) || feof(src_tar_file)) {
			perror_msg("untar: ");
			break;
		}

		uncompressed_count += 512;

		/* Check header has valid magic */
		if (strncmp(raw_tar_header.magic, "ustar", 5) != 0) {
/*
 * FIXME, Need HELP with this
 *
 * This has to fail silently or it incorrectly reports errors when called from
 * deb_extract.
 * The problem is deb_extract decompresses the .gz file in a child process and
 * untar reads from the child proccess. The child process finishes and exits,
 * but fread reads 0's from the src_tar_file even though the child
 * closed its handle.
 */
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

		/* If an exclude list is specified check current file against list */
#if 0
		if (*exclude_list != NULL) {
			i = 0;
			while (exclude_list[i] != 0) {
				if (strncmp(exclude_list[i], raw_tar_header.name, strlen(raw_tar_header.name)) == 0) {
					break;
				}
				i++;
			}
		if (exclude_list[i] != 0) {
				continue;
			}
		}
#endif
		if (untar_function & (extract_contents | extract_verbose_extract | extract_contents_to_file)) {
			fprintf(output, "%s\n", raw_tar_header.name);
		}

		switch (raw_tar_header.typeflag ) {
			case '0':
			case '\0': {
				/* If the name ends in a '/' then assume it is
				 * supposed to be a directory, and fall through
				 */
				int name_length = strlen(raw_tar_header.name);
				if (raw_tar_header.name[name_length - 1] != '/') {
					switch (untar_function) {
						case (extract_extract):
						case (extract_verbose_extract):
						case (extract_control): {
								FILE *dst_file = NULL;
								char *full_name;

								if (file_prefix != NULL) {
									full_name = xmalloc(strlen(argument) + strlen(file_prefix) + name_length + 3);
									sprintf(full_name, "%s/%s.%s", argument, file_prefix, strrchr(raw_tar_header.name, '/') + 1);
								} else {
									full_name = concat_path_file(argument, raw_tar_header.name);
								}
								dst_file = wfopen(full_name, "w");
								copy_file_chunk(src_tar_file, dst_file, (unsigned long long) size);
								uncompressed_count += size;
								fclose(dst_file);
								chmod(full_name, mode);
								free(full_name);
							}
							break;
						case (extract_info):
							if (strstr(raw_tar_header.name, argument) != NULL) {
								copy_file_chunk(src_tar_file, stdout, (unsigned long long) size);
								uncompressed_count += size;
							}
							break;
						case (extract_field):
							if (strstr(raw_tar_header.name, "./control") != NULL) {
								return(read_text_file_to_buffer(src_tar_file));
							}
							break;
					}
					break;
				}
			}
			case '5':
				if (untar_function & (extract_extract | extract_verbose_extract | extract_control)) {
					int ret;
					char *dir;
					dir = concat_path_file(argument, raw_tar_header.name);
					ret = create_path(dir, mode);
					free(dir);
					if (ret == FALSE) {
						perror_msg("%s: Cannot mkdir", raw_tar_header.name); 
						return NULL;
					}
					chmod(dir, mode);
				}
				break;
			case '1':
				if (untar_function & (extract_extract | extract_verbose_extract | extract_control)) {
					if (link(raw_tar_header.linkname, raw_tar_header.name) < 0) {
						perror_msg("%s: Cannot create hard link to '%s'", raw_tar_header.name, raw_tar_header.linkname); 
						return NULL;
					}
				}
				break;
			case '2':
				if (untar_function & (extract_extract | extract_verbose_extract | extract_control)) {
					if (symlink(raw_tar_header.linkname, raw_tar_header.name) < 0) {
						perror_msg("%s: Cannot create symlink to '%s'", raw_tar_header.name, raw_tar_header.linkname); 
						return NULL;
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
				return NULL;
		}
		/*
		 * Seek to start of next block, cant use fseek as unzip() does support it
		 */
		while (uncompressed_count < next_header_offset) {
			if (fgetc(src_tar_file) == EOF) {
				break;
			}
			uncompressed_count++;
		}
	}
	return NULL;
}
