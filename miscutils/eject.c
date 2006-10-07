/* vi: set sw=4 ts=4: */
/*
 * eject implementation for busybox
 *
 * Copyright (C) 2004  Peter Willis <psyphreak@phreaker.net>
 * Copyright (C) 2005  Tito Ragusa <farmatito@tiscali.it>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

/*
 * This is a simple hack of eject based on something Erik posted in #uclibc.
 * Most of the dirty work blatantly ripped off from cat.c =)
 */

#include "busybox.h"

/* various defines swiped from linux/cdrom.h */
#define CDROMCLOSETRAY            0x5319  /* pendant of CDROMEJECT  */
#define CDROMEJECT                0x5309  /* Ejects the cdrom media */
#define CDROM_DRIVE_STATUS        0x5326  /* Get tray position, etc. */
/* drive status possibilities returned by CDROM_DRIVE_STATUS ioctl */
#define CDS_TRAY_OPEN        2

#define FLAG_CLOSE  1
#define FLAG_SMART  2

int eject_main(int argc, char **argv)
{
	unsigned long flags;
	char *device;
	int dev, cmd;

	opt_complementary = "?:?1:t--T:T--t";
	flags = getopt32(argc, argv, "tT");
	device = argv[optind] ? : "/dev/cdrom";

	// We used to do "umount <device>" here, but it was buggy
	// if something was mounted OVER cdrom and
	// if cdrom is mounted many times.
	//
	// This works equally well (or better):
	// #!/bin/sh
	// umount /dev/cdrom
	// eject

	dev = xopen(device, O_RDONLY|O_NONBLOCK);
	cmd = CDROMEJECT;
	if (flags & FLAG_CLOSE
	 || (flags & FLAG_SMART && ioctl(dev, CDROM_DRIVE_STATUS) == CDS_TRAY_OPEN))
		cmd = CDROMCLOSETRAY;
	if (ioctl(dev, cmd)) {
		bb_perror_msg_and_die("%s", device);
	}

	if (ENABLE_FEATURE_CLEAN_UP)
		close(dev);

	return EXIT_SUCCESS;
}
