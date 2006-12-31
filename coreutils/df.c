/* vi: set sw=4 ts=4: */
/*
 * Mini df implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * based on original code by (I think) Bruce Perens <bruce@pixar.com>.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 _NOT_ compliant -- options -P and -t missing.  Also blocksize. */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/df.html */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Size reduction.  Removed floating point dependency.  Added error checking
 * on output.  Output stats on 0-sized filesystems if specifically listed on
 * the command line.  Properly round *-blocks, Used, and Available quantities.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mntent.h>
#include <sys/vfs.h>
#include "busybox.h"

#ifndef CONFIG_FEATURE_HUMAN_READABLE
static long kscale(long b, long bs)
{
	return ( b * (long long) bs + 1024/2 ) / 1024;
}
#endif

int df_main(int argc, char **argv)
{
	long blocks_used;
	long blocks_percent_used;
#ifdef CONFIG_FEATURE_HUMAN_READABLE
	unsigned long df_disp_hr = 1024;
#endif
	int status = EXIT_SUCCESS;
	unsigned opt;
	FILE *mount_table;
	struct mntent *mount_entry;
	struct statfs s;
	static const char hdr_1k[] = "1k-blocks"; /* default display is kilobytes */
	const char *disp_units_hdr = hdr_1k;

#ifdef CONFIG_FEATURE_HUMAN_READABLE
	opt_complementary = "h-km:k-hm:m-hk";
	opt = getopt32(argc, argv, "hmk");
	if (opt & 1) {
		df_disp_hr = 0;
		disp_units_hdr = "     Size";
	}
	if (opt & 2) {
		df_disp_hr = 1024*1024;
		disp_units_hdr = "1M-blocks";
	}
#else
	opt = getopt32(argc, argv, "k");
#endif

	printf("Filesystem%11s%-15sUsed Available Use%% Mounted on\n",
			  "", disp_units_hdr);

	mount_table = NULL;
	argv += optind;
	if (optind >= argc) {
		mount_table = setmntent(bb_path_mtab_file, "r");
		if (!mount_table) {
			bb_perror_msg_and_die(bb_path_mtab_file);
		}
	}

	do {
		const char *device;
		const char *mount_point;

		if (mount_table) {
			mount_entry = getmntent(mount_table);
			if (!mount_entry) {
				endmntent(mount_table);
				break;
			}
		} else {
			mount_point = *argv++;
			if (!mount_point) {
				break;
			}
			mount_entry = find_mount_point(mount_point, bb_path_mtab_file);
			if (!mount_entry) {
				bb_error_msg("%s: can't find mount point", mount_point);
 SET_ERROR:
				status = EXIT_FAILURE;
				continue;
			}
		}

		device = mount_entry->mnt_fsname;
		mount_point = mount_entry->mnt_dir;

		if (statfs(mount_point, &s) != 0) {
			bb_perror_msg("%s", mount_point);
			goto SET_ERROR;
		}

		if ((s.f_blocks > 0) || !mount_table){
			blocks_used = s.f_blocks - s.f_bfree;
			blocks_percent_used = 0;
			if (blocks_used + s.f_bavail) {
				blocks_percent_used = (((long long) blocks_used) * 100
						+ (blocks_used + s.f_bavail)/2
						) / (blocks_used + s.f_bavail);
			}

			if (strcmp(device, "rootfs") == 0) {
				continue;
			} else if (strcmp(device, "/dev/root") == 0) {
				/* Adjusts device to be the real root device,
				* or leaves device alone if it can't find it */
				device = find_block_device("/");
				if (!device) {
					goto SET_ERROR;
				}
			}

#ifdef CONFIG_FEATURE_HUMAN_READABLE
			printf("%-20s %9s ", device,
				make_human_readable_str(s.f_blocks, s.f_bsize, df_disp_hr));

			printf("%9s ",
				make_human_readable_str( (s.f_blocks - s.f_bfree),
						s.f_bsize, df_disp_hr));

			printf("%9s %3ld%% %s\n",
					  make_human_readable_str(s.f_bavail, s.f_bsize, df_disp_hr),
					  blocks_percent_used, mount_point);
#else
			printf("%-20s %9ld %9ld %9ld %3ld%% %s\n",
					  device,
					  kscale(s.f_blocks, s.f_bsize),
					  kscale(s.f_blocks-s.f_bfree, s.f_bsize),
					  kscale(s.f_bavail, s.f_bsize),
					  blocks_percent_used, mount_point);
#endif
		}

	} while (1);

	fflush_stdout_and_exit(status);
}
