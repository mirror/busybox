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
extern unsigned char *bytebuffer;
extern unsigned short bytebuffer_offset;
extern unsigned short bytebuffer_size;
//extern unsigned char *gunzip_in_buffer;
//extern unsigned char gunzip_in_buffer_count;

extern void check_trailer_gzip(int src_fd)
{
	unsigned int stored_crc = 0;
	unsigned char count;

	/* top up the input buffer with the rest of the trailer */
	count = bytebuffer_size - bytebuffer_offset;
	if (count < 8) {
		xread_all(src_fd, &bytebuffer[bytebuffer_size], 8 - count);
		bytebuffer_size += 8 - count;
	}
	for (count = 0; count != 4; count++) {
		stored_crc |= (bytebuffer[bytebuffer_offset] << (count * 8));
		bytebuffer_offset++;
	}

	/* Validate decompression - crc */
	if (stored_crc != (gunzip_crc ^ 0xffffffffL)) {
		error_msg_and_die("crc error");
	}

	/* Validate decompression - size */
	if (gunzip_bytes_out !=
		(bytebuffer[bytebuffer_offset] | (bytebuffer[bytebuffer_offset+1] << 8) |
		(bytebuffer[bytebuffer_offset+2] << 16) | (bytebuffer[bytebuffer_offset+3] << 24))) {
		error_msg_and_die("Incorrect length, but crc is correct");
	}

}
