/* vi: set sw=4 ts=4: */
/*
 * Mini cpio implementation for busybox
 *
 * Copyright (C) 2001 by Glenn McGrath 
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
 * Limitations:
 *		Doesn't check CRC's
 *		Only supports new ASCII and CRC formats
 *
 */
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "unarchive.h"
#include "busybox.h"

extern int cpio_main(int argc, char **argv)
{
	archive_handle_t *archive_handle;
	int opt;

	/* Initialise */
	archive_handle = init_handle();
	archive_handle->src_fd = fileno(stdin);
	archive_handle->seek = seek_by_char;
	archive_handle->flags = ARCHIVE_EXTRACT_NEWER | ARCHIVE_PRESERVE_DATE;
	
	while ((opt = getopt(argc, argv, "idmuvtF:")) != -1) {
		switch (opt) {
		case 'i': /* extract */
			archive_handle->action_data = data_extract_all;
			break;
		case 'd': /* create _leading_ directories */
			archive_handle->flags |= ARCHIVE_CREATE_LEADING_DIRS;
			break;
#if 0
		case 'm': /* preserve modification time */
			archive_handle->flags |= ARCHIVE_PRESERVE_DATE;
			break;
#endif
		case 'v': /* verbosly list files */
			if (archive_handle->action_header == header_list) {
				archive_handle->action_header = header_verbose_list;
			} else {
				archive_handle->action_header = header_list;
			}
			break;
		case 'u': /* unconditional */
			archive_handle->flags |= ARCHIVE_EXTRACT_UNCONDITIONAL;
			archive_handle->flags &= ~ARCHIVE_EXTRACT_NEWER;
			break;
		case 't': /* list files */
			if (archive_handle->action_header == header_list) {
				archive_handle->action_header = header_verbose_list;
			} else {
				archive_handle->action_header = header_list;
			}
			break;
		case 'F':
			archive_handle->src_fd = bb_xopen(optarg, O_RDONLY);
			archive_handle->seek = seek_by_jump;
			break;
		default:
			bb_show_usage();
		}
	}

	while (optind < argc) {
		archive_handle->filter = filter_accept_list;
		archive_handle->accept = llist_add_to(archive_handle->accept, argv[optind]);
		optind++;
	}

	while (get_header_cpio(archive_handle) == EXIT_SUCCESS);

	return(EXIT_SUCCESS);
}
