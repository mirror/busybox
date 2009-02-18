/* eraseall.c -- erase the whole of a MTD device

   Ported to busybox from mtd-utils.

   Copyright (C) 2000 Arcom Control System Ltd

   Renamed to flash_eraseall.c

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include "libbb.h"

#include <mtd/mtd-user.h>
#include <mtd/jffs2-user.h>

#define OPTION_J	(1 << 0)
#define OPTION_Q	(1 << 1)

int target_endian = __BYTE_ORDER;

static uint32_t crc32(uint32_t val, const void *ss, int len,
		uint32_t *crc32_table)
{
	const unsigned char *s = ss;
	while (--len >= 0)
		val = crc32_table[(val ^ *s++) & 0xff] ^ (val >> 8);
	return val;
}

static void show_progress(mtd_info_t *meminfo, erase_info_t *erase)
{
	printf("\rErasing %d Kibyte @ %x -- %2llu %% complete.",
		meminfo->erasesize / 1024, erase->start,
		(unsigned long long) erase->start * 100 / meminfo->size);
	fflush(stdout);
}

int flash_eraseall_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int flash_eraseall_main(int argc, char **argv)
{
	struct jffs2_unknown_node cleanmarker;
	mtd_info_t meminfo;
	int fd, clmpos = 0, clmlen = 8;
	erase_info_t erase;
	struct stat st;
	int isNAND, bbtest = 1;
	unsigned int flags;
	char *mtd_name;

	opt_complementary = "=1";
	flags = getopt32(argv, "jq");

	mtd_name = *(argv + optind);
	xstat(mtd_name, &st);
	if (!S_ISCHR(st.st_mode))
		bb_error_msg_and_die("%s: not a char device", mtd_name);

	fd = xopen(mtd_name, O_RDWR);

	xioctl(fd, MEMGETINFO, &meminfo);

	erase.length = meminfo.erasesize;
	isNAND = meminfo.type == MTD_NANDFLASH ? 1 : 0;

	if (flags & OPTION_J) {
		uint32_t *crc32_table;

		crc32_table = crc32_filltable(NULL, 0);

		cleanmarker.magic = cpu_to_je16 (JFFS2_MAGIC_BITMASK);
		cleanmarker.nodetype = cpu_to_je16 (JFFS2_NODETYPE_CLEANMARKER);
		if (!isNAND)
			cleanmarker.totlen = cpu_to_je32 (sizeof (struct jffs2_unknown_node));
		else {
			struct nand_oobinfo oobinfo;

			xioctl(fd, MEMGETOOBSEL, &oobinfo);

			/* Check for autoplacement */
			if (oobinfo.useecc == MTD_NANDECC_AUTOPLACE) {
				/* Get the position of the free bytes */
				if (!oobinfo.oobfree[0][1])
					bb_error_msg_and_die("Autoplacement selected and no empty space in oob");

				clmpos = oobinfo.oobfree[0][0];
				clmlen = oobinfo.oobfree[0][1];
				if (clmlen > 8)
					clmlen = 8;
			} else {
				/* Legacy mode */
				switch (meminfo.oobsize) {
				case 8:
					clmpos = 6;
					clmlen = 2;
					break;
				case 16:
					clmpos = 8;
					clmlen = 8;
					break;
				case 64:
					clmpos = 16;
					clmlen = 8;
					break;
				}
			}
			cleanmarker.totlen = cpu_to_je32(8);
		}

		cleanmarker.hdr_crc = cpu_to_je32(crc32(0, &cleanmarker,  sizeof(struct jffs2_unknown_node) - 4,
					crc32_table));
	}

	for (erase.start = 0; erase.start < meminfo.size;
	     erase.start += meminfo.erasesize) {
		if (bbtest) {
			int ret;

			loff_t offset = erase.start;
			ret = ioctl(fd, MEMGETBADBLOCK, &offset);
			if (ret > 0) {
				if (!(flags & OPTION_Q))
					bb_info_msg("\nSkipping bad block at 0x%08x", erase.start);
				continue;
			} else if (ret < 0) {
				/* Black block table is not available on certain flash
				 * types e.g. NOR
				 */
				if (errno == EOPNOTSUPP) {
					bbtest = 0;
					if (isNAND)
						bb_error_msg_and_die("%s: Bad block check not available",
								mtd_name);
				} else {
					bb_error_msg_and_die("\n%s: MTD get bad block failed: %s",
							mtd_name, strerror(errno));
				}
			}
		}

		if (!(flags & OPTION_Q))
			show_progress(&meminfo, &erase);

		xioctl(fd, MEMERASE, &erase);

		/* format for JFFS2 ? */
		if (!(flags & OPTION_J))
			continue;

		/* write cleanmarker */
		if (isNAND) {
			struct mtd_oob_buf oob;
			oob.ptr = (unsigned char *) &cleanmarker;
			oob.start = erase.start + clmpos;
			oob.length = clmlen;
			xioctl (fd, MEMWRITEOOB, &oob);
		} else {
			if (lseek (fd, erase.start, SEEK_SET) < 0) {
				bb_error_msg("\n%s: MTD lseek failure: %s", mtd_name, strerror(errno));
				continue;
			}
			if (write (fd , &cleanmarker, sizeof (cleanmarker)) != sizeof (cleanmarker)) {
				bb_error_msg("\n%s: MTD write failure: %s", mtd_name, strerror(errno));
				continue;
			}
		}
		if (!(flags & OPTION_Q))
			printf(" Cleanmarker written at %x.", erase.start);
	}
	if (!(flags & OPTION_Q)) {
		show_progress(&meminfo, &erase);
		printf("\n");
	}

	return EXIT_SUCCESS;
}
