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
 */
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "unarchive.h"
#include "busybox.h"

extern void unpack_ar_archive(archive_handle_t *ar_archive)
{
	char magic[7];

	archive_xread_all(ar_archive, magic, 7);
	if (strncmp(magic, "!<arch>", 7) != 0) {
		bb_error_msg_and_die("Invalid ar magic");
	}
	ar_archive->offset += 7;

	while (get_header_ar(ar_archive) == EXIT_SUCCESS);
}
