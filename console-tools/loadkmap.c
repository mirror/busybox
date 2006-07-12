/* vi: set sw=4 ts=4: */
/*
 * Mini loadkmap implementation for busybox
 *
 * Copyright (C) 1998 Enrique Zanardi <ezanardi@ull.es>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "busybox.h"

#define BINARY_KEYMAP_MAGIC "bkeymap"

/* From <linux/kd.h> */
struct kbentry {
	unsigned char kb_table;
	unsigned char kb_index;
	unsigned short kb_value;
};
/* sets one entry in translation table */
#define KDSKBENT        0x4B47

/* From <linux/keyboard.h> */
#define NR_KEYS         128
#define MAX_NR_KEYMAPS  256

int loadkmap_main(int argc, char **argv)
{
	struct kbentry ke;
	int i, j, fd;
	u_short ibuff[NR_KEYS];
	char flags[MAX_NR_KEYMAPS];
	char buff[7];

	if (argc != 1)
		bb_show_usage();

	fd = bb_xopen(CURRENT_VC, O_RDWR);

	if ((bb_full_read(0, buff, 7) != 7) || (strncmp(buff, BINARY_KEYMAP_MAGIC, 7) != 0))
		bb_error_msg_and_die("This is not a valid binary keymap.");

	if (bb_full_read(0, flags, MAX_NR_KEYMAPS) != MAX_NR_KEYMAPS)
		bb_perror_msg_and_die("Error reading keymap flags");

	for (i = 0; i < MAX_NR_KEYMAPS; i++) {
		if (flags[i] == 1) {
			bb_full_read(0, ibuff, NR_KEYS * sizeof(u_short));
			for (j = 0; j < NR_KEYS; j++) {
				ke.kb_index = j;
				ke.kb_table = i;
				ke.kb_value = ibuff[j];
				ioctl(fd, KDSKBENT, &ke);
			}
		}
	}

	/* Don't bother to close files.  Exit does that
	 * automagically, so we can save a few bytes */
	/* close(fd); */
	return EXIT_SUCCESS;
}
