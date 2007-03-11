/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <sys/types.h>

#include "libbb.h"
#include "unarchive.h"

void data_align(archive_handle_t *archive_handle, const unsigned short boundary)
{
	const unsigned short skip_amount = (boundary - (archive_handle->offset % boundary)) % boundary;

	archive_handle->seek(archive_handle, skip_amount);
	archive_handle->offset += skip_amount;
}
