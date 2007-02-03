/* vi: set sw=4 ts=4: */
/*
 * Mini cpio implementation for busybox
 *
 * Copyright (C) 2001 by Glenn McGrath
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * Limitations:
 *		Doesn't check CRC's
 *		Only supports new ASCII and CRC formats
 *
 */
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "unarchive.h"
#include "busybox.h"

#define CPIO_OPT_EXTRACT			0x01
#define CPIO_OPT_TEST				0x02
#define CPIO_OPT_UNCONDITIONAL		0x04
#define CPIO_OPT_VERBOSE			0x08
#define CPIO_OPT_FILE				0x10
#define CPIO_OPT_CREATE_LEADING_DIR	0x20
#define CPIO_OPT_PRESERVE_MTIME		0x40

int cpio_main(int argc, char **argv);
int cpio_main(int argc, char **argv)
{
	archive_handle_t *archive_handle;
	char *cpio_filename = NULL;
	unsigned opt;

	/* Initialise */
	archive_handle = init_handle();
	archive_handle->src_fd = STDIN_FILENO;
	archive_handle->seek = seek_by_read;
	archive_handle->flags = ARCHIVE_EXTRACT_NEWER | ARCHIVE_PRESERVE_DATE;

	opt = getopt32(argc, argv, "ituvF:dm", &cpio_filename);

	/* One of either extract or test options must be given */
	if ((opt & (CPIO_OPT_TEST | CPIO_OPT_EXTRACT)) == 0) {
		bb_show_usage();
	}

	if (opt & CPIO_OPT_TEST) {
		/* if both extract and test options are given, ignore extract option */
		if (opt & CPIO_OPT_EXTRACT) {
			opt &= ~CPIO_OPT_EXTRACT;
		}
		archive_handle->action_header = header_list;
	}
	if (opt & CPIO_OPT_EXTRACT) {
		archive_handle->action_data = data_extract_all;
	}
	if (opt & CPIO_OPT_UNCONDITIONAL) {
		archive_handle->flags |= ARCHIVE_EXTRACT_UNCONDITIONAL;
		archive_handle->flags &= ~ARCHIVE_EXTRACT_NEWER;
	}
	if (opt & CPIO_OPT_VERBOSE) {
		if (archive_handle->action_header == header_list) {
			archive_handle->action_header = header_verbose_list;
		} else {
			archive_handle->action_header = header_list;
		}
	}
	if (cpio_filename) { /* CPIO_OPT_FILE */
		archive_handle->src_fd = xopen(cpio_filename, O_RDONLY);
		archive_handle->seek = seek_by_jump;
	}
	if (opt & CPIO_OPT_CREATE_LEADING_DIR) {
		archive_handle->flags |= ARCHIVE_CREATE_LEADING_DIRS;
	}

	while (optind < argc) {
		archive_handle->filter = filter_accept_list;
		llist_add_to(&(archive_handle->accept), argv[optind]);
		optind++;
	}

	while (get_header_cpio(archive_handle) == EXIT_SUCCESS);

	return EXIT_SUCCESS;
}
