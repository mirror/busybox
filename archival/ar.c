/* vi: set sw=4 ts=4: */
/*
 * Mini ar implementation for busybox
 *
 * Copyright (C) 2000 by Glenn McGrath
 *
 * Based in part on BusyBox tar, Debian dpkg-deb and GNU ar.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * There is no single standard to adhere to so ar may not portable
 * between different systems
 * http://www.unix-systems.org/single_unix_specification_v2/xcu/ar.html
 */

#include "libbb.h"
#include "unarchive.h"

static void FAST_FUNC header_verbose_list_ar(const file_header_t *file_header)
{
	const char *mode = bb_mode_string(file_header->mode);
	char *mtime;

	mtime = ctime(&file_header->mtime);
	mtime[16] = ' ';
	memmove(&mtime[17], &mtime[20], 4);
	mtime[21] = '\0';
	printf("%s %d/%d%7d %s %s\n", &mode[1], file_header->uid, file_header->gid,
			(int) file_header->size, &mtime[4], file_header->name);
}

#define AR_CTX_PRINT		0x01
#define AR_CTX_LIST		0x02
#define AR_CTX_EXTRACT		0x04
#define AR_OPT_PRESERVE_DATE	0x08
#define AR_OPT_VERBOSE		0x10
#define AR_OPT_CREATE		0x20
#define AR_OPT_INSERT		0x40

int ar_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int ar_main(int argc UNUSED_PARAM, char **argv)
{
	static const char msg_unsupported_err[] ALIGN1 =
		"archive %s is not supported";

	archive_handle_t *archive_handle;
	unsigned opt;

	archive_handle = init_handle();

	/* Prepend '-' to the first argument if required */
	opt_complementary = "--:p:t:x:-1:p--tx:t--px:x--pt";
	opt = getopt32(argv, "ptxovcr");
	argv += optind;

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
		archive_handle->ah_flags |= ARCHIVE_RESTORE_DATE;
	}
	if (opt & AR_OPT_VERBOSE) {
		archive_handle->action_header = header_verbose_list_ar;
	}
	if (opt & AR_OPT_CREATE) {
		bb_error_msg_and_die(msg_unsupported_err, "creation");
	}
	if (opt & AR_OPT_INSERT) {
		bb_error_msg_and_die(msg_unsupported_err, "insertion");
	}

	archive_handle->src_fd = xopen(*argv++, O_RDONLY);

	while (*argv) {
		archive_handle->filter = filter_accept_list;
		llist_add_to(&archive_handle->accept, *argv++);
	}

	unpack_ar_archive(archive_handle);

	return EXIT_SUCCESS;
}
