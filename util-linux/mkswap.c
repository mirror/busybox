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
# define mkswap_selinux_setcontext(fd, path) ((void)0)
#endif

/* from Linux 2.6.23 */
/*
 * Magic header for a swap area. ... Note that the first
 * kilobyte is reserved for boot loader or disk label stuff.
 */
struct swap_header_v1 {
/*	char     bootbits[1024];    Space for disklabel etc. */
	uint32_t version;        /* second kbyte, word 0 */
	uint32_t last_page;      /* 1 */
	uint32_t nr_badpages;    /* 2 */
	char     sws_uuid[16];   /* 3,4,5,6 */
	char     sws_volume[16]; /* 7,8,9,10 */
	uint32_t padding[117];   /* 11..127 */
	uint32_t badpages[1];    /* 128 */
	/* total 129 32-bit words in 2nd kilobyte */
};

#define NWORDS 129
#define hdr ((struct swap_header_v1*)bb_common_bufsiz1)

struct BUG_sizes {
	char swap_header_v1_wrong[sizeof(*hdr)  != (NWORDS * 4) ? -1 : 1];
	char bufsiz1_is_too_small[COMMON_BUFSIZE < (NWORDS * 4) ? -1 : 1];
};

/* Stored without terminating NUL */
static const char SWAPSPACE2[sizeof("SWAPSPACE2")-1] ALIGN1 = "SWAPSPACE2";

int mkswap_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int mkswap_main(int argc UNUSED_PARAM, char **argv)
{
	int fd;
	unsigned pagesize;
	off_t len;
	const char *label = "";

	opt_complementary = "=1";
	/* TODO: -p PAGESZ, -U UUID,
	 * optional SIZE_IN_KB 2nd param
	 */
	getopt32(argv, "L:", &label);
	argv += optind;

	fd = xopen(argv[0], O_WRONLY);

	/* Figure out how big the device is and announce our intentions */
	/* fdlength was reported to be unreliable - use seek */
	len = xlseek(fd, 0, SEEK_END);
	if (ENABLE_SELINUX)
		xlseek(fd, 0, SEEK_SET);

	pagesize = getpagesize();
	len -= pagesize;
	printf("Setting up swapspace version 1, size = %"OFF_FMT"u bytes\n", len);
	mkswap_selinux_setcontext(fd, argv[0]);

	/* Make a header. hdr is zero-filled so far... */
	hdr->version = 1;
	hdr->last_page = (uoff_t)len / pagesize;

	if (ENABLE_FEATURE_MKSWAP_UUID) {
		char uuid_string[32];
		generate_uuid((void*)hdr->sws_uuid);
		bin2hex(uuid_string, hdr->sws_uuid, 16);
		/* f.e. UUID=dfd9c173-be52-4d27-99a5-c34c6c2ff55f */
		printf("UUID=%.8s"  "-%.4s-%.4s-%.4s-%.12s\n",
			uuid_string,
			uuid_string+8,
			uuid_string+8+4,
			uuid_string+8+4+4,
			uuid_string+8+4+4+4
		);
	}
	safe_strncpy(hdr->sws_volume, label, 16);

	/* Write the header.  Sync to disk because some kernel versions check
	 * signature on disk (not in cache) during swapon. */
	xlseek(fd, 1024, SEEK_SET);
	xwrite(fd, hdr, NWORDS * 4);
	xlseek(fd, pagesize - 10, SEEK_SET);
	xwrite(fd, SWAPSPACE2, 10);
	fsync(fd);

	if (ENABLE_FEATURE_CLEAN_UP)
		close(fd);

	return 0;
}
