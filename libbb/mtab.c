/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <mntent.h>
#include "libbb.h"

#define MTAB_MAX_ENTRIES 40

#ifdef CONFIG_FEATURE_MTAB_SUPPORT
void erase_mtab(const char *name)
{
	struct mntent entries[MTAB_MAX_ENTRIES];
	int count = 0;
	FILE *mountTable = setmntent(bb_path_mtab_file, "r");
	struct mntent *m;

	/* Check if reading the mtab file failed */
	if (mountTable == 0
			/* Bummer.  fall back on trying the /proc filesystem */
			&& (mountTable = setmntent("/proc/mounts", "r")) == 0) {
		bb_perror_msg(bb_path_mtab_file);
		return;
	}

	while (((m = getmntent(mountTable)) != 0) && (count < MTAB_MAX_ENTRIES))
	{
		entries[count].mnt_fsname = strdup(m->mnt_fsname);
		entries[count].mnt_dir = strdup(m->mnt_dir);
		entries[count].mnt_type = strdup(m->mnt_type);
		entries[count].mnt_opts = strdup(m->mnt_opts);
		entries[count].mnt_freq = m->mnt_freq;
		entries[count].mnt_passno = m->mnt_passno;
		count++;
	}
	endmntent(mountTable);
	if ((mountTable = setmntent(bb_path_mtab_file, "w"))) {
		int i;

		for (i = 0; i < count; i++) {
			int result = (strcmp(entries[i].mnt_fsname, name) == 0
						  || strcmp(entries[i].mnt_dir, name) == 0);

			if (result)
				continue;
			else
				addmntent(mountTable, &entries[i]);
		}
		endmntent(mountTable);
	} else if (errno != EROFS)
		bb_perror_msg(bb_path_mtab_file);
}
#endif
