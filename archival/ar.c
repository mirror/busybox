/* vi: set sw=4 ts=4: */
/*
 * Mini ar implementation for busybox 
 *
 * Copyright (C) 2000 by Glenn McGrath
 * Written by Glenn McGrath <bug1@optushome.com.au> 1 June 2000
 * 		
 * Based in part on BusyBox tar, Debian dpkg-deb and GNU ar.
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
 * There is no signle standard to adhere to so ar may not portable
 * between different systems
 * http://www.unix-systems.org/single_unix_specification_v2/xcu/ar.html
 */
#include <sys/types.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <utime.h>
#include <unistd.h>
#include <getopt.h>
#include "unarchive.h"
#include "busybox.h"

static void header_verbose_list_ar(const file_header_t *file_header)
{
	const char *mode = mode_string(file_header->mode);
	char *mtime;

	mtime = ctime(&file_header->mtime);
	mtime[16] = ' ';
	memmove(&mtime[17], &mtime[20], 4);
	mtime[21] = '\0';
	printf("%s %d/%d%7d %s %s\n", &mode[1], file_header->uid, file_header->gid, (int) file_header->size, &mtime[4], file_header->name);
}

#if !defined CONFIG_TAR && !defined CONFIG_DPKG_DEB && !defined CONFIG_CPIO
/* This is simpler than data_extract_all */
static void data_extract_regular_file(archive_handle_t *archive_handle)
{
	file_header_t *file_header;
	int dst_fd;

	file_header = archive_handle->file_header;
	dst_fd = xopen(file_header->name, O_WRONLY | O_CREAT);
	archive_copy_file(archive_handle, dst_fd);	
	close(dst_fd);

	chmod(file_header->name, file_header->mode);
	chown(file_header->name, file_header->uid, file_header->gid);

	if (archive_handle->flags & ARCHIVE_PRESERVE_DATE) {
		struct utimbuf t;
		t.actime = t.modtime = file_header->mtime;
		utime(file_header->name, &t);
	}

	return;
}
#endif

extern int ar_main(int argc, char **argv)
{
	archive_handle_t *archive_handle;
	int opt;

#if !defined CONFIG_DPKG_DEB && !defined CONFIG_DPKG
	char magic[8];
#endif
	archive_handle = init_handle();

	while ((opt = getopt(argc, argv, "covtpxX")) != -1) {
		switch (opt) {
		case 'p':	/* print */
			archive_handle->action_data = data_extract_to_stdout;
			break;
		case 't':	/* list contents */
			archive_handle->action_header = header_list;
			break;
		case 'X':
			archive_handle->action_header = header_verbose_list_ar;
		case 'x':	/* extract */
#if defined CONFIG_TAR || defined CONFIG_DPKG_DEB || defined CONFIG_CPIO
			archive_handle->action_data = data_extract_all;
#else
			archive_handle->action_data = data_extract_regular_file;
#endif
			break;
		/* Modifiers */
		case 'o':	/* preserve original dates */
			archive_handle->flags |= ARCHIVE_PRESERVE_DATE;
			break;
		case 'v':	/* verbose */
			archive_handle->action_header = header_verbose_list_ar;
			break;
		default:
			show_usage();
		}
	}
 
	/* check the src filename was specified */
	if (optind == argc) {
		show_usage();
	}

	archive_handle->src_fd = xopen(argv[optind++], O_RDONLY);

	/* TODO: This is the same as in tar, seperate function ? */
	while (optind < argc) {
		archive_handle->filter = filter_accept_list;
		archive_handle->accept = add_to_list(archive_handle->accept, argv[optind]);
		optind++;
	}

#if defined CONFIG_DPKG_DEB || defined CONFIG_DPKG
	unpack_ar_archive(archive_handle);
#else
	archive_xread_all(archive_handle, magic, 7);
	if (strncmp(magic, "!<arch>", 7) != 0) {
		error_msg_and_die("Invalid ar magic");
	}
	archive_handle->offset += 7;

	while (get_header_ar(archive_handle) == EXIT_SUCCESS);
#endif

	return EXIT_SUCCESS;
}
