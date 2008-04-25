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

		if (fgetfilecon(fd, &oldcon) < 0) {
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
		/* fsetfilecon_raw is hidden */
		if (strcmp(oldcon, newcon) != 0 && fsetfilecon(fd, newcon) < 0)
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

#if 0 /* from Linux 2.6.23 */
/*
 * Magic header for a swap area. The first part of the union is
 * what the swap magic looks like for the old (limited to 128MB)
 * swap area format, the second part of the union adds - in the
 * old reserved area - some extra information. Note that the first
 * kilobyte is reserved for boot loader or disk label stuff...
 */
union swap_header {
	struct {
		char reserved[PAGE_SIZE - 10];
		char magic[10];			/* SWAP-SPACE or SWAPSPACE2 */
	} magic;
	struct {
		char            bootbits[1024];	/* Space for disklabel etc. */
		__u32           version;        /* second kbyte, word 0 */
		__u32           last_page;      /* 1 */
		__u32           nr_badpages;    /* 2 */
		unsigned char   sws_uuid[16];   /* 3,4,5,6 */
		unsigned char   sws_volume[16]; /* 7,8,9,10  */
		__u32           padding[117];   /* 11..127 */
		__u32           badpages[1];    /* 128, total 129 32-bit words */
	} info;
};
#endif

#define NWORDS 129
#define hdr ((uint32_t*)(&bb_common_bufsiz1))

struct BUG_bufsiz1_is_too_small {
	char BUG_bufsiz1_is_too_small[COMMON_BUFSIZE < (NWORDS * 4) ? -1 : 1];
};

/* Stored without terminating NUL */
static const char SWAPSPACE2[sizeof("SWAPSPACE2")-1] ALIGN1 = "SWAPSPACE2";

int mkswap_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int mkswap_main(int argc, char **argv)
{
	int fd, pagesize;
	off_t len;

	// No options supported.

	if (argc != 2) bb_show_usage();

	// Figure out how big the device is and announce our intentions.

	fd = xopen(argv[1], O_RDWR);
	/* fdlength was reported to be unreliable - use seek */
	len = xlseek(fd, 0, SEEK_END);
#if ENABLE_SELINUX
	xlseek(fd, 0, SEEK_SET);
#endif
	pagesize = getpagesize();
	printf("Setting up swapspace version 1, size = %"OFF_FMT"u bytes\n",
			len - pagesize);
	mkswap_selinux_setcontext(fd, argv[1]);

	// Make a header. hdr is zero-filled so far...
	hdr[0] = 1;
	hdr[1] = (len / pagesize) - 1;

	// Write the header.  Sync to disk because some kernel versions check
	// signature on disk (not in cache) during swapon.

	xlseek(fd, 1024, SEEK_SET);
	xwrite(fd, hdr, NWORDS * 4);
	xlseek(fd, pagesize - 10, SEEK_SET);
	xwrite(fd, SWAPSPACE2, 10);
	fsync(fd);

	if (ENABLE_FEATURE_CLEAN_UP) close(fd);

	return 0;
}
