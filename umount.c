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

#if defined BB_FEATURE_MOUNT_LOOP
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/loop.h>

static int del_loop(const char *device);
#endif

static const char umount_usage[] = 
"umount [flags] filesystem|directory\n\n"
"Flags:\n"
"\t-a:\tUnmount all file systems"
#ifdef BB_MTAB
" in /etc/mtab\n\t-n:\tDon't erase /etc/mtab entries\n"
#else
"\n"
#endif
;


static int useMtab = TRUE;
static int umountAll = FALSE;
extern const char mtab_file[]; /* Defined in utility.c */

static int 
do_umount(const char* name, int useMtab)
{
    int status;

#if defined BB_FEATURE_MOUNT_LOOP
    /* check to see if this is a loop device */
    struct stat fst;
    char dev[20];
    const char *oldname = NULL;
    int i;
    
    if (stat(name, &fst)) {
	fprintf(stderr, "umount: %s: %s\n", name, strerror(errno));
	exit(1);
    }
    for (i = 0 ; i <= 7 ; i++) {
	struct stat lst;
	sprintf(dev, "/dev/loop%d", i);
	if (stat(dev, &lst))
	    continue;
	if (lst.st_dev == fst.st_dev) {
	    oldname = name;
	    name = dev;
	    break;
	}
    }
#endif

    status = umount(name);

#if defined BB_FEATURE_MOUNT_LOOP
    if (!strncmp("/dev/loop", name, 9)) { /* this was a loop device, delete it */
	del_loop(name);
	if (oldname != NULL)
	    name = oldname;
    }
#endif
#if defined BB_MTAB
    if ( status == 0 ) {
	if ( useMtab==TRUE )
	    erase_mtab(name);
	return 0;
    }
    else
#endif
	return(status);
}

static int
umount_all(int useMtab)
{
	int status;
	struct mntent *m;
        FILE *mountTable;

        if ((mountTable = setmntent (mtab_file, "r"))) {
            while ((m = getmntent (mountTable)) != 0) {
                char *blockDevice = m->mnt_fsname;
#if ! defined BB_MTAB
		if (strcmp (blockDevice, "/dev/root") == 0) {
		    struct fstab* fstabItem;
		    fstabItem = getfsfile ("/");
		    if (fstabItem != NULL) {
			blockDevice = fstabItem->fs_spec;
		    }
		}
#endif
		/* Don't umount /proc when doing umount -a */
                if (strcmp (blockDevice, "proc") == 0)
		    continue;

		status=do_umount (m->mnt_dir, useMtab);
		if (status!=0) {
		    /* Don't bother retrying the umount on busy devices */
		    if (errno==EBUSY) {
			perror(m->mnt_dir); 
			continue;
		    }
		    status=do_umount (blockDevice, useMtab);
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
umount_main(int argc, char** argv)
{
    if (argc < 2) {
	usage( umount_usage); 
    }

    /* Parse any options */
    while (--argc > 0 && **(++argv) == '-') {
	while (*++(*argv)) switch (**argv) {
	    case 'a':
		umountAll = TRUE;
		break;
#ifdef BB_MTAB
	    case 'n':
		useMtab = FALSE;
		break;
#endif
	    default:
		usage( umount_usage);
	}
    }


    if(umountAll==TRUE) {
	exit(umount_all(useMtab));
    }
    if ( do_umount(*argv,useMtab) == 0 )
	exit (TRUE);
    else {
	perror("umount");
	exit(FALSE);
    }
}

#if defined BB_FEATURE_MOUNT_LOOP
static int del_loop(const char *device)
{
	int fd;

	if ((fd = open(device, O_RDONLY)) < 0) {
		perror(device);
		exit(1);
	}
	if (ioctl(fd, LOOP_CLR_FD, 0) < 0) {
		perror("ioctl: LOOP_CLR_FD");
		exit(1);
	}
	close(fd);
	return(0);
}
#endif
