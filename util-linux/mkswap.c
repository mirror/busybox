/* vi: set sw=4 ts=4: */
/* mkswap.c - format swap device (Linux v1 only)
 *
 * Copyright 2006 Rob Landley <rob@landley.net>
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */

#include "busybox.h"

int mkswap_main(int argc, char *argv[]);
int mkswap_main(int argc, char *argv[])
{
	int fd, pagesize;
	off_t len;
	unsigned int hdr[129];

	// No options supported.

	if (argc != 2) bb_show_usage();

	// Figure out how big the device is and announce our intentions.

	fd = xopen(argv[1], O_RDWR);
	len = fdlength(fd);
	pagesize = getpagesize();
	printf("Setting up swapspace version 1, size = %"OFF_FMT"d bytes\n",
			len - pagesize);

	// Make a header.

	memset(hdr, 0, sizeof(hdr));
	hdr[0] = 1;
	hdr[1] = (len / pagesize) - 1;

	// Write the header.  Sync to disk because some kernel versions check
	// signature on disk (not in cache) during swapon.

	xlseek(fd, 1024, SEEK_SET);
	xwrite(fd, hdr, sizeof(hdr));
	xlseek(fd, pagesize-10, SEEK_SET);
	xwrite(fd, "SWAPSPACE2", 10);
	fsync(fd);

	if (ENABLE_FEATURE_CLEAN_UP) close(fd);

	return 0;
}
