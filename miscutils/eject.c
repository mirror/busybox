/*
 * eject implementation for busybox
 *
 * Copyright (C) 2004  Peter Willis <psyphreak@phreaker.net>
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

/*
 * This is a simple hack of eject based on something Erik posted in #uclibc.
 * Most of the dirty work blatantly ripped off from cat.c =)
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>
#include "busybox.h"

/* various defines swiped from linux/cdrom.h */
#define CDROMCLOSETRAY            0x5319  /* pendant of CDROMEJECT  */
#define CDROMEJECT                0x5309  /* Ejects the cdrom media */
#define DEFAULT_CDROM             "/dev/cdrom"

#ifdef CONFIG_FEATURE_MTAB_SUPPORT
#define MTAB  CONFIG_FEATURE_MTAB_FILENAME
#else
#define MTAB  "/proc/mounts"
#endif

extern int eject_main(int argc, char **argv)
{
	unsigned long flags;
	char * command; 
	char *device=argv[optind] ? : DEFAULT_CDROM;
	
	flags = bb_getopt_ulflags(argc, argv, "t");
	bb_xasprintf(&command, "umount '%s'", device);
	
	/* validate input before calling system */
	if(find_mount_point(device, MTAB))
		system(command);
	
	if (ioctl(bb_xopen( device, 
	                   (O_RDONLY | O_NONBLOCK)), 
	          ( flags ? CDROMCLOSETRAY : CDROMEJECT)))
	{
		bb_perror_msg_and_die(device);
	}
#ifdef CONFIG_FEATURE_CLEAN_UP 
	free(command);
#endif
	return(EXIT_SUCCESS);
}
