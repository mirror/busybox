/* vi: set sw=4 ts=4: */
/*
 * Mini rpm2cpio implementation for busybox
 *
 * Copyright (C) 2001 by Laurence Anderson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include <sys/types.h>
#include <netinet/in.h> /* For ntohl & htonl function */
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "busybox.h"
#include "unarchive.h"

#define RPM_MAGIC "\355\253\356\333"
#define RPM_HEADER_MAGIC "\216\255\350"

struct rpm_lead {
    unsigned char magic[4];
    u_int8_t major, minor;
    u_int16_t type;
    u_int16_t archnum;
    char name[66];
    u_int16_t osnum;
    u_int16_t signature_type;
    char reserved[16];
};

struct rpm_header {
	char magic[3]; /* 3 byte magic: 0x8e 0xad 0xe8 */
	u_int8_t version; /* 1 byte version number */
	u_int32_t reserved; /* 4 bytes reserved */
	u_int32_t entries; /* Number of entries in header (4 bytes) */
	u_int32_t size; /* Size of store (4 bytes) */
};

void skip_header(int rpm_fd)
{
	struct rpm_header header;

	bb_xread_all(rpm_fd, &header, sizeof(struct rpm_header));
	if (strncmp((char *) &header.magic, RPM_HEADER_MAGIC, 3) != 0) {
		bb_error_msg_and_die("Invalid RPM header magic"); /* Invalid magic */
	}
	if (header.version != 1) {
		bb_error_msg_and_die("Unsupported RPM header version"); /* This program only supports v1 headers */
	}
	header.entries = ntohl(header.entries);
	header.size = ntohl(header.size);
	lseek (rpm_fd, 16 * header.entries, SEEK_CUR); /* Seek past index entries */
	lseek (rpm_fd, header.size, SEEK_CUR); /* Seek past store */
}

/* No getopt required */
extern int rpm2cpio_main(int argc, char **argv)
{
	struct rpm_lead lead;
	int rpm_fd;
	unsigned char magic[2];

	if (argc == 1) {
		rpm_fd = fileno(stdin);
	} else {
		rpm_fd = bb_xopen(argv[1], O_RDONLY);
	}

	bb_xread_all(rpm_fd, &lead, sizeof(struct rpm_lead));
	if (strncmp((char *) &lead.magic, RPM_MAGIC, 4) != 0) {
		bb_error_msg_and_die("Invalid RPM magic"); /* Just check the magic, the rest is irrelevant */
	}

	/* Skip the signature header */
	skip_header(rpm_fd);
	lseek(rpm_fd, (8 - (lseek(rpm_fd, 0, SEEK_CUR) % 8)) % 8, SEEK_CUR);

	/* Skip the main header */
	skip_header(rpm_fd);
	
	bb_xread_all(rpm_fd, &magic, 2);
	if ((magic[0] != 0x1f) || (magic[1] != 0x8b)) {
		bb_error_msg_and_die("Invalid gzip magic");
	}

	check_header_gzip(rpm_fd);
	if (inflate_gunzip(rpm_fd, fileno(stdout)) != 0) {
		bb_error_msg("Error inflating");
	}

	close(rpm_fd);

	return 0;
}
