/* vi: set sw=4 ts=4: */
/*
 * Mini unzip implementation for busybox
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
 *
 */

/* For reference see
 * http://www.pkware.com/products/enterprise/white_papers/appnote.txt
 * http://www.info-zip.org/pub/infozip/doc/appnote-iz-latest.zip
 */

/* TODO Endian issues, exclude, should we accept input from stdin ? */

#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "unarchive.h"
#include "busybox.h"

#define ZIP_FILEHEADER_MAGIC	0x04034b50
#define ZIP_CDS_MAGIC			0x02014b50
#define ZIP_CDS_END_MAGIC		0x06054b50
#define ZIP_DD_MAGIC			0x08074b50

extern unsigned int gunzip_crc;
extern unsigned int gunzip_bytes_out;

static void header_list_unzip(const file_header_t *file_header)
{
	printf("  inflating: %s\n", file_header->name);
}

static void header_verbose_list_unzip(const file_header_t *file_header)
{
	unsigned int dostime = (unsigned int) file_header->mtime;

	/* can printf arguments cut of the decade component ? */
	unsigned short year = 1980 + ((dostime & 0xfe000000) >> 25);
	while (year >= 100) {
		year -= 100;
	}

	printf("%9u  %02u-%02u-%02u %02u:%02u   %s\n",
		(unsigned int) file_header->size,
		(dostime & 0x01e00000) >> 21,
		(dostime & 0x001f0000) >> 16,
		year,
		(dostime & 0x0000f800) >> 11,
		(dostime & 0x000007e0) >> 5,
		file_header->name);
}

extern int unzip_main(int argc, char **argv)
{
	union {
		unsigned char raw[26];
		struct {
			unsigned short version;	/* 0-1 */
			unsigned short flags;	/* 2-3 */
			unsigned short method;	/* 4-5 */
			unsigned short modtime;	/* 6-7 */
			unsigned short moddate;	/* 8-9 */
			unsigned int crc32 __attribute__ ((packed));		/* 10-13 */
			unsigned int cmpsize __attribute__ ((packed));;	/* 14-17 */
			unsigned int ucmpsize __attribute__ ((packed));;	/* 18-21 */
			unsigned short filename_len;		/* 22-23 */
			unsigned short extra_len;		/* 24-25 */
		} formated __attribute__ ((packed));
	} zip_header;

	archive_handle_t *archive_handle;
	unsigned int total_size = 0;
	unsigned int total_entries = 0;
	char *base_dir = NULL;
	int opt = 0;

	/* Initialise */
	archive_handle = init_handle();
	archive_handle->action_data = NULL;
	archive_handle->action_header = header_list_unzip;

	while ((opt = getopt(argc, argv, "lnopqd:")) != -1) {
		switch (opt) {
			case 'l':	/* list */
				archive_handle->action_header = header_verbose_list_unzip;
				archive_handle->action_data = data_skip;
				break;
			case 'n':	/* never overwright existing files */
				break;
			case 'o':
				archive_handle->flags = ARCHIVE_EXTRACT_UNCONDITIONAL;
				break;
			case 'p':	/* extract files to stdout */
				archive_handle->action_data = data_extract_to_stdout;
				break;
			case 'q':	/* Extract files quietly */
				archive_handle->action_header = header_skip;
				break;
			case 'd':	/* Extract files to specified base directory*/
				base_dir = optarg;
				break;
#if 0
			case 'x':	/* Exclude the specified files */
				archive_handle->filter = filter_accept_reject_list;
				break;
#endif
			default:
				bb_show_usage();
		}
	}

	if (argc == optind) {
		bb_show_usage();
	}

	printf("Archive:  %s\n", argv[optind]);
	if (archive_handle->action_header == header_verbose_list_unzip) {
		printf("  Length     Date   Time    Name\n");
		printf(" --------    ----   ----    ----\n");
	}

	if (*argv[optind] == '-') {
		archive_handle->src_fd = STDIN_FILENO;
		archive_handle->seek = seek_by_char;
	} else {
		archive_handle->src_fd = bb_xopen(argv[optind++], O_RDONLY);
	}

	if ((base_dir) && (chdir(base_dir))) {
		bb_perror_msg_and_die("Couldnt chdir");
	}

	while (optind < argc) {
		archive_handle->filter = filter_accept_list;
		archive_handle->accept = llist_add_to(archive_handle->accept, argv[optind]);
		optind++;
	}

	while (1) {
		unsigned int magic;
		int dst_fd;

		/* TODO Endian issues */
		archive_xread_all(archive_handle, &magic, 4);
		archive_handle->offset += 4;

		if (magic == ZIP_CDS_MAGIC) {
			break;
		}
		else if (magic != ZIP_FILEHEADER_MAGIC) {
			bb_error_msg_and_die("Invlaide zip magic");
		}

		/* Read the file header */
		archive_xread_all(archive_handle, zip_header.raw, 26);
		archive_handle->offset += 26;
		archive_handle->file_header->mode = S_IFREG | 0777;

		if (zip_header.formated.method != 8) {
			bb_error_msg_and_die("Unsupported compression method %d\n", zip_header.formated.method);
		}

		/* Read filename */
		archive_handle->file_header->name = xmalloc(zip_header.formated.filename_len + 1);
		archive_xread_all(archive_handle, archive_handle->file_header->name, zip_header.formated.filename_len);
		archive_handle->offset += zip_header.formated.filename_len;
		archive_handle->file_header->name[zip_header.formated.filename_len] = '\0';

		/* Skip extra header bits */
		archive_handle->file_header->size = zip_header.formated.extra_len;
		data_skip(archive_handle);
		archive_handle->offset += zip_header.formated.extra_len;

		/* Handle directories */
		archive_handle->file_header->mode = S_IFREG | 0777;
		if (last_char_is(archive_handle->file_header->name, '/')) {
			archive_handle->file_header->mode ^= S_IFREG;
			archive_handle->file_header->mode |= S_IFDIR;
		}

		/* Data section */
		archive_handle->file_header->size = zip_header.formated.cmpsize;
		if (archive_handle->action_data) {
			archive_handle->action_data(archive_handle);
		} else {
			dst_fd = bb_xopen(archive_handle->file_header->name, O_WRONLY | O_CREAT);
			inflate_init(zip_header.formated.cmpsize);
			inflate_unzip(archive_handle->src_fd, dst_fd);
			close(dst_fd);
			chmod(archive_handle->file_header->name, archive_handle->file_header->mode);

			/* Validate decompression - crc */
			if (zip_header.formated.crc32 != (gunzip_crc ^ 0xffffffffL)) {
				bb_error_msg("Invalid compressed data--crc error");
			}

			/* Validate decompression - size */
			if (gunzip_bytes_out != zip_header.formated.ucmpsize) {
				bb_error_msg("Invalid compressed data--length error");
			}
		}

		/* local file descriptor section */
		archive_handle->offset += zip_header.formated.cmpsize;
		/* This ISNT unix time */
		archive_handle->file_header->mtime = zip_header.formated.modtime | (zip_header.formated.moddate << 16);
		archive_handle->file_header->size = zip_header.formated.ucmpsize;
		total_size += archive_handle->file_header->size;
		total_entries++;

		archive_handle->action_header(archive_handle->file_header);

		/* Data descriptor section */
		if (zip_header.formated.flags & 4) {
			/* skip over duplicate crc, compressed size and uncompressed size */
			unsigned char data_description[12];
			archive_xread_all(archive_handle, data_description, 12);
			archive_handle->offset += 12;
		}
	}
	/* Central directory section */

	if (archive_handle->action_header == header_verbose_list_unzip) {
		printf(" --------                   -------\n");
		printf("%9d                   %d files\n", total_size, total_entries);
	}

	return(EXIT_SUCCESS);
}
