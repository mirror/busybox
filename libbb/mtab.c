/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <mntent.h>
#include "libbb.h"

#if ENABLE_FEATURE_MTAB_SUPPORT
void erase_mtab(const char *name)
{
	struct mntent *entries = NULL;
	int i, count = 0;
	FILE *mountTable;
	struct mntent *m;

	mountTable = setmntent(bb_path_mtab_file, "r");
	/* Bummer. Fall back on trying the /proc filesystem */
	if (!mountTable) mountTable = setmntent("/proc/mounts", "r");
	if (!mountTable) {
		bb_perror_msg(bb_path_mtab_file);
		return;
	}

	while ((m = getmntent(mountTable)) != 0) {
		i = count++;
		entries = xrealloc(entries, count * sizeof(entries[0]));
		entries[i].mnt_fsname = xstrdup(m->mnt_fsname);
		entries[i].mnt_dir = xstrdup(m->mnt_dir);
		entries[i].mnt_type = xstrdup(m->mnt_type);
		entries[i].mnt_opts = xstrdup(m->mnt_opts);
		entries[i].mnt_freq = m->mnt_freq;
		entries[i].mnt_passno = m->mnt_passno;
	}
	endmntent(mountTable);

	mountTable = setmntent(bb_path_mtab_file, "w");
	if (mountTable) {
		for (i = 0; i < count; i++) {
			if (strcmp(entries[i].mnt_fsname, name) != 0
			 && strcmp(entries[i].mnt_dir, name) != 0)
				addmntent(mountTable, &entries[i]);
		}
		endmntent(mountTable);
	} else if (errno != EROFS)
		bb_perror_msg(bb_path_mtab_file);
}
#endif
