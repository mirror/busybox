/*
 * Mini loadkmap implementation for busybox
 *
 * Copyright (C) 1998 Enrique Zanardi <ezanardi@ull.es>
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

#include "internal.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <linux/kd.h>
#include <linux/keyboard.h>
#include <sys/ioctl.h>


static const char loadkmap_usage[] = "loadkmap\n"
"\n"
"\tLoad a binary keyboard translation table from standard input.\n"
"\n";


int
loadkmap_main(int argc, char * * argv)
{ 
    struct kbentry ke;
    u_short *ibuff;
    int i,j,fd,readsz,pos,ibuffsz=NR_KEYS * sizeof(u_short);
    char flags[MAX_NR_KEYMAPS],magic[]="bkeymap",buff[7];

    fd = open("/dev/tty0", O_RDWR);
    if (fd < 0) {
	fprintf(stderr, "Error opening /dev/tty0: %s\n", strerror(errno));
	return 1;
    }

    read(0,buff,7);
    if (0 != strncmp(buff,magic,7)) {
	fprintf(stderr, "This is not a valid binary keymap.\n");
	return 1;
    }
   
    if ( MAX_NR_KEYMAPS != read(0,flags,MAX_NR_KEYMAPS) ) {
	fprintf(stderr, "Error reading keymap flags: %s\n", strerror(errno));
	return 1;
    }

    ibuff=(u_short *) malloc(ibuffsz);
    if (!ibuff) {
	fprintf(stderr, "Out of memory.\n");
	return 1;
    }

    for(i=0; i<MAX_NR_KEYMAPS; i++) {
	if (flags[i]==1){
	    pos=0;
	    while (pos < ibuffsz) {
		if ( (readsz = read(0,(char *)ibuff+pos,ibuffsz-pos)) < 0 ) {
		    fprintf(stderr, "Error reading keymap: %s\n", 
			strerror(errno));
		    return 1;
	        }
		pos += readsz;
	    }
	    for(j=0; j<NR_KEYS; j++) {
		ke.kb_index = j;
		ke.kb_table = i;
		ke.kb_value = ibuff[j];
		ioctl(fd, KDSKBENT, &ke);
	    }
	}
    }
    close (fd);
    return 0;
}
