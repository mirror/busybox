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

#include "libbb.h"

/* various defines swiped from linux/cdrom.h */
#define CDROMCLOSETRAY            0x5319  /* pendant of CDROMEJECT  */
#define CDROMEJECT                0x5309  /* Ejects the cdrom media */
#define CDROM_DRIVE_STATUS        0x5326  /* Get tray position, etc. */
/* drive status possibilities returned by CDROM_DRIVE_STATUS ioctl */
#define CDS_TRAY_OPEN        2

#define FLAG_CLOSE  1
#define FLAG_SMART  2


/* Code taken from the original eject (http://eject.sourceforge.net/),
 * refactored it a bit for busybox (ne-bb@nicoerfurth.de) */
#define FLAG_SCSI   4

#include <scsi/sg.h>
#include <scsi/scsi.h>
static void eject_scsi(const int fd, const char * const dev)
{
  int i;
  unsigned char sense_buffer[32];
  unsigned char inqBuff[2];
  sg_io_hdr_t io_hdr;
  char sg_commands[3][6] = {
    {ALLOW_MEDIUM_REMOVAL, 0, 0, 0, 0, 0},
    {START_STOP, 0, 0, 0, 1, 0},
    {START_STOP, 0, 0, 0, 2, 0}
  };

  if ((ioctl(fd, SG_GET_VERSION_NUM, &i) < 0) || (i < 30000))
    bb_error_msg_and_die("not an sg device or old sg driver");

  memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
  io_hdr.interface_id = 'S';
  io_hdr.cmd_len = 6;
  io_hdr.mx_sb_len = sizeof(sense_buffer);
  io_hdr.dxfer_direction = SG_DXFER_NONE;
  /* io_hdr.dxfer_len = 0; */
  io_hdr.dxferp = inqBuff;
  io_hdr.sbp = sense_buffer;
  io_hdr.timeout = 2000;

  for (i=0; i < 3; i++) {
    io_hdr.cmdp = sg_commands[i];
    ioctl_or_perror_and_die(fd, SG_IO, (void *)&io_hdr, "%s", dev);
  }

  /* force kernel to reread partition table when new disc inserted */
  ioctl(fd, BLKRRPART);
}

static void eject_cdrom(const int fd, const unsigned long flags,
						const char * const dev)
{
	int cmd = CDROMEJECT;

	if (flags & FLAG_CLOSE
	 || (flags & FLAG_SMART && ioctl(fd, CDROM_DRIVE_STATUS) == CDS_TRAY_OPEN))
		cmd = CDROMCLOSETRAY;

	return ioctl_or_perror_and_die(fd, cmd, NULL, "%s", dev);
}

int eject_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int eject_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	unsigned long flags;
	const char *device;
	int dev;

	opt_complementary = "?1:t--T:T--t";
	flags = getopt32(argv, "tT" USE_FEATURE_EJECT_SCSI("s"));
	device = argv[optind] ? argv[optind] : "/dev/cdrom";

	/* We used to do "umount <device>" here, but it was buggy
	   if something was mounted OVER cdrom and
	   if cdrom is mounted many times.

	   This works equally well (or better):
	   #!/bin/sh
	   umount /dev/cdrom
	   eject
	*/

	dev = xopen(device, O_RDONLY|O_NONBLOCK);

	if (ENABLE_FEATURE_EJECT_SCSI && (flags & FLAG_SCSI))
		eject_scsi(dev, device);
	else
		eject_cdrom(dev, flags, device);

	if (ENABLE_FEATURE_CLEAN_UP)
		close(dev);

	return EXIT_SUCCESS;
}
