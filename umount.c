/*
 * Mini umount implementation for busybox
 *
 *
 * Copyright (C) 1999 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
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
#include <stdio.h>
#include <sys/mount.h>
#include <mntent.h>
#include <fstab.h>
#include <errno.h>

static const char umount_usage[] = 
"Usage: umount filesystem\n"
"   or: umount directory\n"
"   or: umount -a"
"to unmount all mounted file systems.\n";

static int
umount_all()
{
	int status;
	struct mntent *m;
        FILE *mountTable;

        if ((mountTable = setmntent ("/proc/mounts", "r"))) {
            while ((m = getmntent (mountTable)) != 0) {
                char *blockDevice = m->mnt_fsname;
                if (strcmp (blockDevice, "/dev/root") == 0)
                    blockDevice = (getfsfile ("/"))->fs_spec;
		status=umount (m->mnt_dir);
		if (status!=0) {
		    /* Don't bother retrying the umount on busy devices */
		    if (errno==EBUSY) {
			perror(m->mnt_dir); 
			continue;
		    }
		    printf ("Trying to umount %s failed: %s\n", 
			    m->mnt_dir, strerror(errno)); 
		    printf ("Instead trying to umount %s\n", blockDevice); 
		    status=umount (blockDevice);
		    if (status!=0) {
			printf ("Couldn't umount %s on %s (type %s): %s\n", 
				blockDevice, m->mnt_dir, m->mnt_type, strerror(errno));
		    }
		}
            }
            endmntent (mountTable);
        }
        return( TRUE);
}

extern int
umount_main(int argc, char * * argv)
{

    if (argc < 2) {
	usage( umount_usage); 
    }
    argc--;
    argv++;

    /* Parse any options */
    while (**argv == '-') {
	while (*++(*argv)) switch (**argv) {
	    case 'a':
		exit ( umount_all() );
		break;
	    default:
		usage( umount_usage);
	}
    }
    if ( umount(*argv) == 0 )
	     exit (TRUE);
    else {
	perror("umount");
	exit( FALSE);
    }
}

