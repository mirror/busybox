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
 * There is no single standard to adhere to so ar may not portable
 * between different systems
 * http://www.unix-systems.org/single_unix_specification_v2/xcu/ar.html
 */

#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <utime.h>
#include <unistd.h>

#include "unarchive.h"
#include "busybox.h"

static void header_verbose_list_ar(const file_header_t *file_header)
{
	const char *mode = bb_mode_string(file_header->mode);
	char *mtime;

	mtime = ctime(&file_header->mtime);
	mtime[16] = ' ';
	memmove(&mtime[17], &mtime[20], 4);
	mtime[21] = '\0';
	printf("%s %d/%d%7d %s %s\n", &mode[1], file_header->uid, file_header->gid, (int) file_header->size, &mtime[4], file_header->name);
}

#define AR_CTX_PRINT	1
#define AR_CTX_LIST		2
#define AR_CTX_EXTRACT	4
#define AR_OPT_PRESERVE_DATE	8
#define AR_OPT_VERBOSE			16

extern int ar_main(int argc, char **argv)
{
	archive_handle_t *archive_handle;
	unsigned long opt;
	char magic[8];

	archive_handle = init_handle();

	bb_opt_complementaly = "p~tx:t~px:x~pt";
	opt = bb_getopt_ulflags(argc, argv, "ptxov");

	if ((opt & 0x80000000UL) || (optind == argc)) {
		bb_show_usage();
	}

	if (opt & AR_CTX_PRINT) {
		archive_handle->action_data = data_extract_to_stdout;
	}
	if (opt & AR_CTX_LIST) {
		archive_handle->action_header = header_list;
	}
	if (opt & AR_CTX_EXTRACT) {
		archive_handle->action_data = data_extract_all;
	}
	if (opt & AR_OPT_PRESERVE_DATE) {
		archive_handle->flags |= ARCHIVE_PRESERVE_DATE;
	}
	if (opt & AR_OPT_VERBOSE) {
		archive_handle->action_header = header_verbose_list_ar;
	}

	archive_handle->src_fd = bb_xopen(argv[optind++], O_RDONLY);

	while (optind < argc) {
		archive_handle->filter = filter_accept_list;
		archive_handle->accept = llist_add_to(archive_handle->accept, argv[optind++]);
	}

	archive_xread_all(archive_handle, magic, 7);
	if (strncmp(magic, "!<arch>", 7) != 0) {
		bb_error_msg_and_die("Invalid ar magic");
	}
	archive_handle->offset += 7;

	while (get_header_ar(archive_handle) == EXIT_SUCCESS);

	return EXIT_SUCCESS;
}
