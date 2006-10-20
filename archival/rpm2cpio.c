/* vi: set sw=4 ts=4: */
/*
 * Mini rpm2cpio implementation for busybox
 *
 * Copyright (C) 2001 by Laurence Anderson
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
#include "busybox.h"
#include "unarchive.h"

#define RPM_MAGIC "\355\253\356\333"
#define RPM_HEADER_MAGIC "\216\255\350"

struct rpm_lead {
    unsigned char magic[4];
    uint8_t major, minor;
    uint16_t type;
    uint16_t archnum;
    char name[66];
    uint16_t osnum;
    uint16_t signature_type;
    char reserved[16];
};

struct rpm_header {
	char magic[3]; /* 3 byte magic: 0x8e 0xad 0xe8 */
	uint8_t version; /* 1 byte version number */
	uint32_t reserved; /* 4 bytes reserved */
	uint32_t entries; /* Number of entries in header (4 bytes) */
	uint32_t size; /* Size of store (4 bytes) */
};

static void skip_header(int rpm_fd)
{
	struct rpm_header header;

	xread(rpm_fd, &header, sizeof(struct rpm_header));
	if (strncmp((char *) &header.magic, RPM_HEADER_MAGIC, 3) != 0) {
		bb_error_msg_and_die("invalid RPM header magic"); /* Invalid magic */
	}
	if (header.version != 1) {
		bb_error_msg_and_die("unsupported RPM header version"); /* This program only supports v1 headers */
	}
	header.entries = ntohl(header.entries);
	header.size = ntohl(header.size);
	lseek (rpm_fd, 16 * header.entries, SEEK_CUR); /* Seek past index entries */
	lseek (rpm_fd, header.size, SEEK_CUR); /* Seek past store */
}

/* No getopt required */
int rpm2cpio_main(int argc, char **argv)
{
	struct rpm_lead lead;
	int rpm_fd;
	unsigned char magic[2];

	if (argc == 1) {
		rpm_fd = STDIN_FILENO;
	} else {
		rpm_fd = xopen(argv[1], O_RDONLY);
	}

	xread(rpm_fd, &lead, sizeof(struct rpm_lead));
	if (strncmp((char *) &lead.magic, RPM_MAGIC, 4) != 0) {
		bb_error_msg_and_die("invalid RPM magic"); /* Just check the magic, the rest is irrelevant */
	}

	/* Skip the signature header */
	skip_header(rpm_fd);
	lseek(rpm_fd, (8 - (lseek(rpm_fd, 0, SEEK_CUR) % 8)) % 8, SEEK_CUR);

	/* Skip the main header */
	skip_header(rpm_fd);

	xread(rpm_fd, &magic, 2);
	if ((magic[0] != 0x1f) || (magic[1] != 0x8b)) {
		bb_error_msg_and_die("invalid gzip magic");
	}

	check_header_gzip(rpm_fd);
	if (inflate_gunzip(rpm_fd, STDOUT_FILENO) < 0) {
		bb_error_msg("error inflating");
	}

	close(rpm_fd);

	return 0;
}
