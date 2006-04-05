/* vi:set ts=4:*/
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "unarchive.h"
#include "libbb.h"

void unpack_ar_archive(archive_handle_t *ar_archive)
{
	char magic[7];

	archive_xread_all(ar_archive, magic, 7);
	if (strncmp(magic, "!<arch>", 7) != 0) {
		bb_error_msg_and_die("Invalid ar magic");
	}
	ar_archive->offset += 7;

	while (get_header_ar(ar_archive) == EXIT_SUCCESS);
}
