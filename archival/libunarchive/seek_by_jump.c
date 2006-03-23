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

#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include "libbb.h"
#include "unarchive.h"

extern void seek_by_jump(const archive_handle_t *archive_handle, const unsigned int amount)
{
	if (lseek(archive_handle->src_fd, (off_t) amount, SEEK_CUR) == (off_t) -1) {
#ifdef CONFIG_FEATURE_UNARCHIVE_TAPE
		if (errno == ESPIPE) {
			seek_by_char(archive_handle, amount);
		} else
#endif
			bb_perror_msg_and_die("Seek failure");
	}
}
