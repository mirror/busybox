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

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

off_t archive_offset;

void seek_sub_file(FILE *src_stream, const int count)
{
	/* Try to fseek as faster */
	archive_offset += count;
	if (fseek(src_stream, count, SEEK_CUR) != 0 && errno == ESPIPE) {
		int i;
		for (i = 0; i < count; i++) {
			fgetc(src_stream);
		}
	}
	return;
}