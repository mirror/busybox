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

#include "busybox.h"
#include <netinet/in.h> /* For ntohl & htonl function */
#include <string.h>

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

void skip_header(FILE *rpmfile)
{
	struct rpm_header header;

	fread(&header, sizeof(struct rpm_header), 1, rpmfile);
	if (strncmp((char *) &header.magic, RPM_HEADER_MAGIC, 3) != 0) error_msg_and_die("Invalid RPM header magic"); /* Invalid magic */
	if (header.version != 1) error_msg_and_die("Unsupported RPM header version"); /* This program only supports v1 headers */
	header.entries = ntohl(header.entries);
	header.size = ntohl(header.size);
	fseek (rpmfile, 16 * header.entries, SEEK_CUR); /* Seek past index entries */
	fseek (rpmfile, header.size, SEEK_CUR); /* Seek past store */
}

/* No getopt required */
extern int rpm2cpio_main(int argc, char **argv)
{
	struct rpm_lead lead;
	int gunzip_pid;
	FILE *rpmfile, *cpiofile;

	if (argc == 1) {
		rpmfile = stdin;
	} else {
		rpmfile = fopen(argv[1], "r");
	 	if (!rpmfile) perror_msg_and_die("Can't open rpm file");
		/* set the buffer size */
		setvbuf(rpmfile, NULL, _IOFBF, 0x8000);
	}

	fread (&lead, sizeof(struct rpm_lead), 1, rpmfile);
	if (strncmp((char *) &lead.magic, RPM_MAGIC, 4) != 0) error_msg_and_die("Invalid RPM magic"); /* Just check the magic, the rest is irrelevant */
	/* Skip the signature header */
	skip_header(rpmfile);
	fseek(rpmfile, (8 - (ftell(rpmfile) % 8)) % 8, SEEK_CUR); /* Pad to 8 byte boundary */
	/* Skip the main header */
	skip_header(rpmfile);

	cpiofile = gz_open(rpmfile, &gunzip_pid);

	copyfd(fileno(cpiofile), fileno(stdout));
	gz_close(gunzip_pid);
	fclose(rpmfile);
	return 0;
}
