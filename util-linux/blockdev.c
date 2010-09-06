/*
 * blockdev implementation for busybox
 *
 * Copyright (C) 2010 Sergey Naumov <sknaumov@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

//applet:IF_BLOCKDEV(APPLET(blockdev, _BB_DIR_SBIN, _BB_SUID_DROP))

//kbuild:lib-$(CONFIG_BLOCKDEV) += blockdev.o

//config:config BLOCKDEV
//config:	bool "blockdev"
//config:	default y
//config:	help
//config:	  Performs some ioctls with block devices.

//usage:#define blockdev_trivial_usage
//usage:	"OPTION BLOCKDEV"
//usage:#define blockdev_full_usage "\n\n"
//usage:       "Options:"
//usage:     "\n	--setro		Set ro"
//usage:     "\n	--setrw		Set rw"
//usage:     "\n	--getro		Get ro"
//usage:     "\n	--getss		Get sector size"
//usage:     "\n	--getbsz	Get block size"
//usage:     "\n	--setbsz BYTES	Set block size"
//usage:     "\n	--getsize	Get device size in 512-byte sectors"
//usage:     "\n	--getsize64	Get device size in bytes"
//usage:     "\n	--flushbufs	Flush buffers"
//usage:     "\n	--rereadpt	Reread partition table"


#include "libbb.h"
#include <linux/fs.h>

enum {
	ARG_NONE   = 0,
	ARG_INT    = 1,
	ARG_ULONG  = 2,
	ARG_ULLONG = 3,
	ARG_MASK   = 3,

	FL_USRARG   = 4, /* argument is provided by user */
	FL_NORESULT = 8,
};

struct bdc {
	uint32_t   ioc;                       /* ioctl code */
	const char name[sizeof("flushbufs")]; /* "--setfoo" wothout "--" */
	uint8_t    flags;
	int8_t     argval;                    /* default argument value */
};

static const struct bdc bdcommands[] = {
	{
		.ioc = BLKROSET,
		.name = "setro",
		.flags = ARG_INT + FL_NORESULT,
		.argval = 1,
	},{
		.ioc = BLKROSET,
		.name = "setrw",
		.flags = ARG_INT + FL_NORESULT,
		.argval = 0,
	},{
		.ioc = BLKROGET,
		.name = "getro",
		.flags = ARG_INT,
		.argval = -1,
	},{
		.ioc = BLKSSZGET,
		.name = "getss",
		.flags = ARG_INT,
		.argval = -1,
	},{
		.ioc = BLKBSZGET,
		.name = "getbsz",
		.flags = ARG_INT,
		.argval = -1,
	},{
		.ioc = BLKBSZSET,
		.name = "setbsz",
		.flags = ARG_INT + FL_NORESULT + FL_USRARG,
		.argval = 0,
	},{
		.ioc = BLKGETSIZE,
		.name = "getsize",
		.flags = ARG_ULONG,
		.argval = -1,
	},{
		.ioc = BLKGETSIZE64,
		.name = "getsize64",
		.flags = ARG_ULLONG,
		.argval = -1,
	},{
		.ioc = BLKFLSBUF,
		.name = "flushbufs",
		.flags = ARG_NONE + FL_NORESULT,
		.argval = 0,
	},{
		.ioc = BLKRRPART,
		.name = "rereadpt",
		.flags = ARG_NONE + FL_NORESULT,
		.argval = 0,
	}
};

static const struct bdc *find_cmd(const char *s)
{
	const struct bdc *bdcmd = bdcommands;
	if (s[0] == '-' && s[1] == '-') {
		s += 2;
		do {
			if (strcmp(s, bdcmd->name) == 0)
				return bdcmd;
			bdcmd++;
		} while (bdcmd != bdcommands + ARRAY_SIZE(bdcommands));
	}
	bb_show_usage();
}

int blockdev_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int blockdev_main(int argc, char **argv)
{
	const struct bdc *bdcmd;
	void *ioctl_ptr;
	int fd;
	int iarg;
	unsigned long lu;
	unsigned long long llu;

	if ((unsigned)(argc - 3) > 1) /* must have 2 or 3 args */
		bb_show_usage();

	bdcmd = find_cmd(*++argv);

	llu = (int)bdcmd->argval;
	if (bdcmd->flags & FL_USRARG)
		llu = xatoi_positive(*++argv);
	lu = llu;
	iarg = llu;

	if (!*++argv || argv[1])
		bb_show_usage();
	fd = xopen(*argv, O_RDONLY);

	ioctl_ptr = NULL;
	switch (bdcmd->flags & ARG_MASK) {
	case ARG_INT:
		ioctl_ptr =  &iarg;
		break;
	case ARG_ULONG:
		ioctl_ptr = &lu;
		break;
	case ARG_ULLONG:
		ioctl_ptr = &llu;
		break;
	}

	if (ioctl(fd, bdcmd->ioc, ioctl_ptr) == -1)
		bb_simple_perror_msg_and_die(*argv);

	switch (bdcmd->flags & (ARG_MASK+FL_NORESULT)) {
	case ARG_INT:
		/* Smaller code when we use long long
		 * (gcc tail-merges printf call)
		 */
		printf("%lld\n", (long long)iarg);
		break;
	case ARG_ULONG:
		llu = lu;
		/* FALLTHROUGH */
	case ARG_ULLONG:
		printf("%llu\n", llu);
		break;
	}

	if (ENABLE_FEATURE_CLEAN_UP)
		close(fd);
	return EXIT_SUCCESS;
}
