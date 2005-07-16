/* vi: set sw=4 ts=4: */
/*
 * Mini df implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
 * based on original code by (I think) Bruce Perens <bruce@pixar.com>.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mntent.h>
#include <sys/vfs.h>
#include <getopt.h>
#include "busybox.h"

extern const char mtab_file[];	/* Defined in utility.c */
#ifdef BB_FEATURE_HUMAN_READABLE
static unsigned long df_disp_hr = KILOBYTE; 
#endif

static int do_df(char *device, const char *mount_point)
{
	struct statfs s;
	long blocks_used;
	long blocks_percent_used;

	if (statfs(mount_point, &s) != 0) {
		perror_msg("%s", mount_point);
		return FALSE;
	}

	if (s.f_blocks > 0) {
		blocks_used = s.f_blocks - s.f_bfree;
		if(blocks_used == 0)
			blocks_percent_used = 0;
		else {
			blocks_percent_used = (long)
			  (blocks_used * 100.0 / (blocks_used + s.f_bavail) + 0.5);
		}
		if (strcmp(device, "/dev/root") == 0) {
			/* Adjusts device to be the real root device,
			 * or leaves device alone if it can't find it */
			device = find_real_root_device_name(device);
			if(device==NULL)
				return FALSE;
		}
#ifdef BB_FEATURE_HUMAN_READABLE
		printf("%-20s %9s ", device,
				make_human_readable_str(s.f_blocks, s.f_bsize, df_disp_hr));

		printf("%9s ",
				make_human_readable_str( (s.f_blocks - s.f_bfree), s.f_bsize, df_disp_hr));

		printf("%9s %3ld%% %s\n",
				make_human_readable_str(s.f_bavail, s.f_bsize, df_disp_hr),
				blocks_percent_used, mount_point);
#else
		printf("%-20s %9ld %9ld %9ld %3ld%% %s\n",
				device,
				(long) (s.f_blocks * (s.f_bsize / (double)KILOBYTE)),
				(long) ((s.f_blocks - s.f_bfree)*(s.f_bsize/(double)KILOBYTE)),
				(long) (s.f_bavail * (s.f_bsize / (double)KILOBYTE)),
				blocks_percent_used, mount_point);
#endif
	}

	return TRUE;
}

extern int df_main(int argc, char **argv)
{
	int status = EXIT_SUCCESS;
	int opt = 0;
	int i = 0;
	char disp_units_hdr[80] = "1k-blocks"; /* default display is kilobytes */

	while ((opt = getopt(argc, argv, "k"
#ifdef BB_FEATURE_HUMAN_READABLE
	"hm"
#endif
)) > 0)
	{
		switch (opt) {
#ifdef BB_FEATURE_HUMAN_READABLE
			case 'h':
				df_disp_hr = 0;
				strcpy(disp_units_hdr, "     Size");
				break;
			case 'm':
				df_disp_hr = MEGABYTE;
				strcpy(disp_units_hdr, "1M-blocks");
				break;
#endif
			case 'k':
				/* default display is kilobytes */
				break;
			default:
					  show_usage();
		}
	}

	printf("%-20s %-14s %s %s %s %s\n", "Filesystem", disp_units_hdr,
	       "Used", "Available", "Use%", "Mounted on");

	if(optind < argc) {
		struct mntent *mount_entry;
		for(i = optind; i < argc; i++)
		{
			if ((mount_entry = find_mount_point(argv[i], mtab_file)) == 0) {
				error_msg("%s: can't find mount point.", argv[i]);
				status = EXIT_FAILURE;
			} else if (!do_df(mount_entry->mnt_fsname, mount_entry->mnt_dir))
				status = EXIT_FAILURE;
		}
	} else {
		FILE *mount_table;
		struct mntent *mount_entry;

		mount_table = setmntent(mtab_file, "r");
		if (mount_table == 0) {
			perror_msg("%s", mtab_file);
			return EXIT_FAILURE;
		}

		while ((mount_entry = getmntent(mount_table))) {
			if (!do_df(mount_entry->mnt_fsname, mount_entry->mnt_dir))
				status = EXIT_FAILURE;
		}
		endmntent(mount_table);
	}

	return status;
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
