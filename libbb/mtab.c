/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
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
 */

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <mntent.h>
#include "libbb.h"

extern const char mtab_file[];	/* Defined in utility.c */
static const int MS_RDONLY = 1;	/* Mount read-only.  */

void erase_mtab(const char *name)
{
	struct mntent entries[20];
	int count = 0;
	FILE *mountTable = setmntent(mtab_file, "r");
	struct mntent *m;

	/* Check if reading the mtab file failed */
	if (mountTable == 0
			/* Bummer.  fall back on trying the /proc filesystem */
			&& (mountTable = setmntent("/proc/mounts", "r")) == 0) {
		perror_msg("%s", mtab_file);
		return;
	}

	while ((m = getmntent(mountTable)) != 0) {
		entries[count].mnt_fsname = strdup(m->mnt_fsname);
		entries[count].mnt_dir = strdup(m->mnt_dir);
		entries[count].mnt_type = strdup(m->mnt_type);
		entries[count].mnt_opts = strdup(m->mnt_opts);
		entries[count].mnt_freq = m->mnt_freq;
		entries[count].mnt_passno = m->mnt_passno;
		count++;
	}
	endmntent(mountTable);
	if ((mountTable = setmntent(mtab_file, "w"))) {
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
		perror_msg("%s", mtab_file);
}

void write_mtab(char *blockDevice, char *directory,
				char *filesystemType, long flags, char *string_flags)
{
	FILE *mountTable = setmntent(mtab_file, "a+");
	struct mntent m;

	if (mountTable == 0) {
		perror_msg("%s", mtab_file);
		return;
	}
	if (mountTable) {
		int length = strlen(directory);

		if (length > 1 && directory[length - 1] == '/')
			directory[length - 1] = '\0';

		if (filesystemType == 0) {
			struct mntent *p = find_mount_point(blockDevice, "/proc/mounts");

			if (p && p->mnt_type)
				filesystemType = p->mnt_type;
		}
		m.mnt_fsname = blockDevice;
		m.mnt_dir = directory;
		m.mnt_type = filesystemType ? filesystemType : "default";

		if (*string_flags) {
			m.mnt_opts = string_flags;
		} else {
			if ((flags | MS_RDONLY) == flags)
				m.mnt_opts = "ro";
			else
				m.mnt_opts = "rw";
		}

		m.mnt_freq = 0;
		m.mnt_passno = 0;
		addmntent(mountTable, &m);
		endmntent(mountTable);
	}
}
