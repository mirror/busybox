/* vi: set sw=4 ts=4: */
/*
 * Mini df implementation for busybox
 *
 * Copyright (C) 1999,2000,2001 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
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

#include "busybox.h"
#include <stdio.h>
#include <stdlib.h>
#include <mntent.h>
#include <sys/vfs.h>
#include <getopt.h>

extern const char mtab_file[];	/* Defined in utility.c */
#ifdef BB_FEATURE_HUMAN_READABLE
unsigned long disp_hr = KILOBYTE; 
#endif

static int df(char *device, const char *mountPoint)
{
	struct statfs s;
	long blocks_used;
	long blocks_percent_used;

	if (statfs(mountPoint, &s) != 0) {
		perror_msg("%s", mountPoint);
		return FALSE;
	}

	if (s.f_blocks > 0) {
		blocks_used = s.f_blocks - s.f_bfree;
		if(0 == blocks_used)
			blocks_percent_used = 0;
		else
			blocks_percent_used = (long)
			  (blocks_used * 100.0 / (blocks_used + s.f_bavail) + 0.5);
		if (strcmp(device, "/dev/root") == 0) {
			/* Adjusts device to be the real root device,
			 * or leaves device alone if it can't find it */
			find_real_root_device_name( device);
		}
#ifdef BB_FEATURE_HUMAN_READABLE
		printf("%-20s %9s",
			   device,
			   format((s.f_blocks * s.f_bsize), disp_hr));
		printf(" %9s", format((s.f_blocks - s.f_bfree) * s.f_bsize, disp_hr));
		printf(" %9s %3ld%% %s\n",
			   format(s.f_bavail * s.f_bsize, disp_hr),
			   blocks_percent_used, mountPoint);
#else
		printf("%-20s %9ld %9ld %9ld %3ld%% %s\n",
			   device,
			   (long) (s.f_blocks * (s.f_bsize / 1024.0)),
			   (long) ((s.f_blocks - s.f_bfree) * (s.f_bsize / 1024.0)),
			   (long) (s.f_bavail * (s.f_bsize / 1024.0)),
			   blocks_percent_used, mountPoint);
#endif

	}

	return TRUE;
}

extern int df_main(int argc, char **argv)
{
	int status = EXIT_SUCCESS;
	int opt = 0;
	int i = 0;

	while ((opt = getopt(argc, argv, "?"
#ifdef BB_FEATURE_HUMAN_READABLE
	"hm"
#endif
	"k"
)) > 0)
	{
		switch (opt) {
#ifdef BB_FEATURE_HUMAN_READABLE
			case 'h': disp_hr = 0;         break;
			case 'm': disp_hr = MEGABYTE;  break;
			case 'k': disp_hr = KILOBYTE;  break;
#else
			case 'k': break;
#endif
			case '?': goto print_df_usage; break;
		}
	}

	printf("%-20s %-14s %s %s %s %s\n", "Filesystem",
#ifdef BB_FEATURE_HUMAN_READABLE
		   (KILOBYTE == disp_hr) ? "1k-blocks" : "     Size",
#else
		   "1k-blocks",
#endif
	       "Used", "Available", "Use%", "Mounted on");


	if(optind < argc) {
		struct mntent *mountEntry;
		for(i = optind; i < argc; i++)
		{
			if ((mountEntry = find_mount_point(argv[i], mtab_file)) == 0) {
				error_msg("%s: can't find mount point.", argv[i]);
				status = EXIT_FAILURE;
			} else if (!df(mountEntry->mnt_fsname, mountEntry->mnt_dir))
				status = EXIT_FAILURE;
		}
	} else {
		FILE *mountTable;
		struct mntent *mountEntry;

		mountTable = setmntent(mtab_file, "r");
		if (mountTable == 0) {
			perror_msg("%s", mtab_file);
			return EXIT_FAILURE;
		}

		while ((mountEntry = getmntent(mountTable))) {
			if (!df(mountEntry->mnt_fsname, mountEntry->mnt_dir))
				status = EXIT_FAILURE;
		}
		endmntent(mountTable);
	}

	return status;

print_df_usage:
    usage(df_usage);
    return(FALSE);
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
