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
"Usage: umount [flags] filesystem|directory\n\n"
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

#if ! defined BB_MTAB
#define do_umount( blockDevice, useMtab) umount( blockDevice)
#else
static int 
do_umount(const char* name, int useMtab)
{
    int status = umount(name);

    if ( status == 0 ) {
	if ( useMtab==TRUE )
	    erase_mtab(name);
	return 0;
    }
    else 
	return( status);
}
#endif

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
    int i=0;
    char **foo=argv;
    while(*foo) {
	fprintf(stderr, "argv[%d]='%s'\n", i++, *foo);
	foo++;
    }

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
	exit( FALSE);
    }
}

