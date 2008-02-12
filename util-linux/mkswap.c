/* vi: set sw=4 ts=4: */
/* mkswap.c - format swap device (Linux v1 only)
 *
 * Copyright 2006 Rob Landley <rob@landley.net>
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

#if ENABLE_SELINUX
static void mkswap_selinux_setcontext(int fd, const char *path)
{
	struct stat stbuf;

	if (!is_selinux_enabled())
		return;

	if (fstat(fd, &stbuf) < 0)
		bb_perror_msg_and_die("fstat failed");
	if (S_ISREG(stbuf.st_mode)) {
		security_context_t newcon;
		security_context_t oldcon = NULL;
		context_t context;

		if (fgetfilecon_raw(fd, &oldcon) < 0) {
			if (errno != ENODATA)
				goto error;
			if (matchpathcon(path, stbuf.st_mode, &oldcon) < 0)
				goto error;
		}
		context = context_new(oldcon);
		if (!context || context_type_set(context, "swapfile_t"))
			goto error;
		newcon = context_str(context);
		if (!newcon)
			goto error;
		if (strcmp(oldcon, newcon) != 0 && fsetfilecon_raw(fd, newcon) < 0)
			goto error;
		if (ENABLE_FEATURE_CLEAN_UP) {
			context_free(context);
			freecon(oldcon);
		}
	}
	return;
 error:
	bb_perror_msg_and_die("SELinux relabeling failed");
}
#else
#define mkswap_selinux_setcontext(fd, path) ((void)0)
#endif

int mkswap_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int mkswap_main(int argc, char **argv)
{
	int fd, pagesize;
	off_t len;
	unsigned int hdr[129];

	// No options supported.

	if (argc != 2) bb_show_usage();

	// Figure out how big the device is and announce our intentions.

	fd = xopen(argv[1], O_RDWR);
	len = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	pagesize = getpagesize();
	printf("Setting up swapspace version 1, size = %"OFF_FMT"u bytes\n",
			len - pagesize);
	mkswap_selinux_setcontext(fd, argv[1]);

	// Make a header.

	memset(hdr, 0, sizeof(hdr));
	hdr[0] = 1;
	hdr[1] = (len / pagesize) - 1;

	// Write the header.  Sync to disk because some kernel versions check
	// signature on disk (not in cache) during swapon.

	xlseek(fd, 1024, SEEK_SET);
	xwrite(fd, hdr, sizeof(hdr));
	xlseek(fd, pagesize - 10, SEEK_SET);
	xwrite(fd, "SWAPSPACE2", 10);
	fsync(fd);

	if (ENABLE_FEATURE_CLEAN_UP) close(fd);

	return 0;
}
