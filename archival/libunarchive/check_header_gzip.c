#include <stdlib.h>
#include <unistd.h>
#include "libbb.h"

extern void check_header_gzip(int src_fd)
{
	union {
		unsigned char raw[8];
		struct {
			unsigned char method;
			unsigned char flags;
			unsigned int mtime;
			unsigned char xtra_flags;
			unsigned char os_flags;
		} formated;
	} header;

	xread_all(src_fd, header.raw, 8);

   /* Check the compression method */
	if (header.formated.method != 8) {
		error_msg_and_die("Unknown compression method %d",
						  header.formated.method);
	}

	if (header.formated.flags & 0x04) {
		/* bit 2 set: extra field present */
		unsigned char extra_short;

		extra_short = xread_char(src_fd);
		extra_short += xread_char(src_fd) << 8;
		while (extra_short > 0) {
		   /* Ignore extra field */
			xread_char(src_fd);
			extra_short--;
		}
	}

   /* Discard original name if any */
	if (header.formated.flags & 0x08) {
	   /* bit 3 set: original file name present */
		char tmp;

		do {
			read(src_fd, &tmp, 1);
		} while (tmp != 0);
	}

   /* Discard file comment if any */
	if (header.formated.flags & 0x10) {
	   /* bit 4 set: file comment present */
		char tmp;

		do {
			read(src_fd, &tmp, 1);
		} while (tmp != 0);
	}

   /* Read the header checksum */
	if (header.formated.flags & 0x02) {
		char tmp;

		read(src_fd, &tmp, 1);
		read(src_fd, &tmp, 1);
	}

	return;
}
