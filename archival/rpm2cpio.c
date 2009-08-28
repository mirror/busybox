/* vi: set sw=4 ts=4: */
/*
 * Mini rpm2cpio implementation for busybox
 *
 * Copyright (C) 2001 by Laurence Anderson
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
#include "libbb.h"
#include "unarchive.h"

#define RPM_MAGIC            0xedabeedb
#define RPM_MAGIC_STR        "\355\253\356\333"

struct rpm_lead {
	uint32_t magic;
	uint8_t major, minor;
	uint16_t type;
	uint16_t archnum;
	char name[66];
	uint16_t osnum;
	uint16_t signature_type;
	char reserved[16];
};

#define RPM_HEADER_VERnMAGIC 0x8eade801
#define RPM_HEADER_MAGIC_STR "\216\255\350"

struct rpm_header {
	uint32_t magic_and_ver; /* 3 byte magic: 0x8e 0xad 0xe8; 1 byte version */
	uint32_t reserved; /* 4 bytes reserved */
	uint32_t entries; /* Number of entries in header (4 bytes) */
	uint32_t size; /* Size of store (4 bytes) */
};

static off_t skip_header(int rpm_fd)
{
	struct rpm_header header;

	xread(rpm_fd, &header, sizeof(header));
//	if (strncmp((char *) &header.magic, RPM_HEADER_MAGIC_STR, 3) != 0) {
//		bb_error_msg_and_die("invalid RPM header magic");
//	}
//	if (header.version != 1) {
//		bb_error_msg_and_die("unsupported RPM header version");
//	}
	if (header.magic_and_ver != htonl(RPM_HEADER_VERnMAGIC)) {
		bb_error_msg_and_die("invalid RPM header magic or unsupported version");
		// ": %x != %x", header.magic_and_ver, htonl(RPM_HEADER_VERnMAGIC));
	}
	/* Seek past index entries */
	lseek(rpm_fd, 16 * ntohl(header.entries), SEEK_CUR);
	/* Seek past store */
	return lseek(rpm_fd, ntohl(header.size), SEEK_CUR);
}

/* No getopt required */
int rpm2cpio_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int rpm2cpio_main(int argc UNUSED_PARAM, char **argv)
{
	struct rpm_lead lead;
	int rpm_fd;
	off_t pos;
	unsigned char magic[2];
	IF_DESKTOP(long long) int FAST_FUNC (*unpack)(int src_fd, int dst_fd);

	if (!argv[1]) {
		rpm_fd = STDIN_FILENO;
	} else {
		rpm_fd = xopen(argv[1], O_RDONLY);
	}
	xread(rpm_fd, &lead, sizeof(lead));

	/* Just check the magic, the rest is irrelevant */
	if (lead.magic != htonl(RPM_MAGIC)) {
		bb_error_msg_and_die("invalid RPM magic");
	}

	/* Skip the signature header, align to 8 bytes */
	pos = skip_header(rpm_fd);
	lseek(rpm_fd, (8 - (unsigned)pos) & 7, SEEK_CUR);

	/* Skip the main header */
	skip_header(rpm_fd);

	xread(rpm_fd, &magic, 2);
	unpack = unpack_gz_stream;
	if (magic[0] != 0x1f || magic[1] != 0x8b) {
		if (!ENABLE_FEATURE_SEAMLESS_BZ2
		 || magic[0] != 'B' || magic[1] != 'Z'
		) {
			bb_error_msg_and_die("invalid gzip"
					IF_FEATURE_SEAMLESS_BZ2("/bzip2")
					" magic");
		}
		unpack = unpack_bz2_stream;
	}

	if (unpack(rpm_fd, STDOUT_FILENO) < 0) {
		bb_error_msg_and_die("error unpacking");
	}

	if (ENABLE_FEATURE_CLEAN_UP) {
		close(rpm_fd);
	}

	return 0;
}
