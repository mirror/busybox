/* vi: set sw=4 ts=4: */
/*
 *  busybox gunzip trailing header handler
 *  Copyright Glenn McGrath 2002
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "config.h"
#include "busybox.h"
#include "unarchive.h"

extern unsigned int gunzip_crc;
extern unsigned int gunzip_bytes_out;
extern unsigned char *gunzip_in_buffer;
extern unsigned char gunzip_in_buffer_count;

extern void check_trailer_gzip(int src_fd)
{

	unsigned int stored_crc = 0;
	unsigned char count;

	/* top up the input buffer with the rest of the trailer */
	xread_all(src_fd, &gunzip_in_buffer[gunzip_in_buffer_count], 8 - gunzip_in_buffer_count);
	for (count = 0; count != 4; count++) {
		stored_crc |= (gunzip_in_buffer[count] << (count * 8));
	}

	/* Validate decompression - crc */
	if (stored_crc != (gunzip_crc ^ 0xffffffffL)) {
		error_msg_and_die("invalid compressed data--crc error");
	}

	/* Validate decompression - size */
	if (gunzip_bytes_out != 
		(gunzip_in_buffer[4] | (gunzip_in_buffer[5] << 8) |
		(gunzip_in_buffer[6] << 16) | (gunzip_in_buffer[7] << 24))) {
		error_msg_and_die("invalid compressed data--length error");
	}

}
