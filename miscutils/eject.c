/*
 * eject implementation for busybox
 *
 * Copyright (C) 2004  Peter Willis <psyphreak@phreaker.net>
 * Copyright (C) 2005  Tito Ragusa <farmatito@tiscali.it>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 *
 */

/*
 * This is a simple hack of eject based on something Erik posted in #uclibc.
 * Most of the dirty work blatantly ripped off from cat.c =)
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mount.h>
#include <mntent.h>
#include "busybox.h"

/* various defines swiped from linux/cdrom.h */
#define CDROMCLOSETRAY            0x5319  /* pendant of CDROMEJECT  */
#define CDROMEJECT                0x5309  /* Ejects the cdrom media */
#define DEFAULT_CDROM             "/dev/cdrom"

int eject_main(int argc, char **argv)
{
	unsigned long flags;
	char *device;
	struct mntent *m;

	flags = bb_getopt_ulflags(argc, argv, "t");
	device = argv[optind] ? : DEFAULT_CDROM;

	if ((m = find_mount_point(device, bb_path_mtab_file))) {
		if (umount(m->mnt_dir)) {
			bb_error_msg_and_die("Can't umount");
		} else if (ENABLE_FEATURE_MTAB_SUPPORT) {
			erase_mtab(m->mnt_fsname);
		}
	}
	if (ioctl(bb_xopen(device, (O_RDONLY | O_NONBLOCK)),
				(flags ? CDROMCLOSETRAY : CDROMEJECT))) {
		bb_perror_msg_and_die("%s", device);
	}
	return (EXIT_SUCCESS);
}
