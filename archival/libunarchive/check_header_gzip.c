/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "unarchive.h" /* for external decl of check_header_gzip_or_die */

void check_header_gzip_or_die(int src_fd)
{
	union {
		unsigned char raw[8];
		struct {
			unsigned char method;
			unsigned char flags;
			unsigned int mtime;
			unsigned char xtra_flags;
			unsigned char os_flags;
		} formatted;
	} header;

	xread(src_fd, header.raw, 8);

	/* Check the compression method */
	if (header.formatted.method != 8) {
		bb_error_msg_and_die("unknown compression method %d",
						  header.formatted.method);
	}

	if (header.formatted.flags & 0x04) {
		/* bit 2 set: extra field present */
		unsigned extra_short;

		extra_short = xread_char(src_fd) + (xread_char(src_fd) << 8);
		while (extra_short > 0) {
			/* Ignore extra field */
			xread_char(src_fd);
			extra_short--;
		}
	}

	/* Discard original name if any */
	if (header.formatted.flags & 0x08) {
		/* bit 3 set: original file name present */
		while (xread_char(src_fd) != 0);
	}

	/* Discard file comment if any */
	if (header.formatted.flags & 0x10) {
		/* bit 4 set: file comment present */
		while (xread_char(src_fd) != 0);
	}

	/* Read the header checksum */
	if (header.formatted.flags & 0x02) {
		xread_char(src_fd);
		xread_char(src_fd);
	}
}
