/* vi: set sw=4 ts=4: */
/*
 * Mini dumpkmap implementation for busybox
 *
 * Copyright (C) Arne Bernin <arne@matrix.loopback.org>
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
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "busybox.h"

/* From <linux/kd.h> */
struct kbentry {
	unsigned char kb_table;
	unsigned char kb_index;
	unsigned short kb_value;
};
static const int KDGKBENT = 0x4B46;  /* gets one entry in translation table */

/* From <linux/keyboard.h> */
static const int NR_KEYS = 128;
static const int MAX_NR_KEYMAPS = 256;

int dumpkmap_main(int argc, char **argv)
{
	struct kbentry ke;
	int i, j, fd;
	char flags[MAX_NR_KEYMAPS], magic[] = "bkeymap";

	if (argc>=2 && *argv[1]=='-') {
		bb_show_usage();
	}

	fd=bb_xopen(CURRENT_VC, O_RDWR);

	write(1, magic, 7);

	for (i=0; i < MAX_NR_KEYMAPS; i++) flags[i]=0;
	flags[0]=1; 
	flags[1]=1;
	flags[2]=1;
	flags[4]=1;
	flags[5]=1;
	flags[6]=1;
	flags[8]=1;
	flags[9]=1;
	flags[10]=1;
	flags[12]=1;
	
	/* dump flags */
	for (i=0; i < MAX_NR_KEYMAPS; i++) write(1,&flags[i],1); 

	for (i = 0; i < MAX_NR_KEYMAPS; i++) {
		if (flags[i] == 1) {
			for (j = 0; j < NR_KEYS; j++) {
				ke.kb_index = j;
				ke.kb_table = i;
				if (ioctl(fd, KDGKBENT, &ke) < 0) {
				
					bb_error_msg("ioctl returned: %m, %s, %s, %xqq", (char *)&ke.kb_index,(char *)&ke.kb_table,(int)&ke.kb_value);
					}
				else {
					write(1,(void*)&ke.kb_value,2);	
					}	
				
			}
		}
	}
	close(fd);
	return EXIT_SUCCESS;
}
