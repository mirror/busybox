/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "unarchive.h"

void unpack_ar_archive(archive_handle_t *ar_archive)
{
	char magic[7];

	xread(ar_archive->src_fd, magic, 7);
	if (strncmp(magic, "!<arch>", 7) != 0) {
		bb_error_msg_and_die("invalid ar magic");
	}
	ar_archive->offset += 7;

	while (get_header_ar(ar_archive) == EXIT_SUCCESS);
}
