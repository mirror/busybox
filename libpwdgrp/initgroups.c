/*
 * initgroups.c - This file is part of the libc-8086/grp package for ELKS,
 * Copyright (C) 1995, 1996 Nat Friedman <ndf@linux.mit.edu>.
 * 
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "busybox.h" 

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "grp_.h"

/*
 * Define GR_SCALE_DYNAMIC if you want grp to dynamically scale its read buffer
 * so that lines of any length can be used.  On very very small systems,
 * you may want to leave this undefined becasue it will make the grp functions
 * somewhat larger (because of the inclusion of malloc and the code necessary).
 * On larger systems, you will want to define this, because grp will _not_
 * deal with long lines gracefully (they will be skipped).
 */
#undef GR_SCALE_DYNAMIC

#ifndef GR_SCALE_DYNAMIC
/*
 * If scaling is not dynamic, the buffers will be statically allocated, and
 * maximums must be chosen.  GR_MAX_LINE_LEN is the maximum number of
 * characters per line in the group file.  GR_MAX_MEMBERS is the maximum
 * number of members of any given group.
 */
#define GR_MAX_LINE_LEN 128
/* GR_MAX_MEMBERS = (GR_MAX_LINE_LEN-(24+3+6))/9 */
#define GR_MAX_MEMBERS 11

#endif /* !GR_SCALE_DYNAMIC */


/*
 * Define GR_DYNAMIC_GROUP_LIST to make initgroups() dynamically allocate
 * space for it's GID array before calling setgroups().  This is probably
 * unnecessary scalage, so it's undefined by default.
 */
#undef GR_DYNAMIC_GROUP_LIST

#ifndef GR_DYNAMIC_GROUP_LIST
/*
 * GR_MAX_GROUPS is the size of the static array initgroups() uses for
 * its static GID array if GR_DYNAMIC_GROUP_LIST isn't defined.
 */
#define GR_MAX_GROUPS 64

#endif /* !GR_DYNAMIC_GROUP_LIST */

int initgroups(__const char *user, gid_t gid)
{
	register struct group *group;

#ifndef GR_DYNAMIC_GROUP_LIST
	gid_t group_list[GR_MAX_GROUPS];
#else
	gid_t *group_list = NULL;
#endif
	register char **tmp_mem;
	int num_groups;
	int grp_fd;


	if ((grp_fd = open(bb_path_group_file, O_RDONLY)) < 0)
		return -1;

	num_groups = 0;
#ifdef GR_DYNAMIC_GROUP_LIST
	group_list = (gid_t *) realloc(group_list, 1);
#endif
	group_list[num_groups] = gid;
#ifndef GR_DYNAMIC_GROUP_LIST
	while (num_groups < GR_MAX_GROUPS &&
		   (group = bb_getgrent(grp_fd)) != NULL)
#else
	while ((group = bb_getgrent(grp_fd)) != NULL)
#endif
	{
		if (group->gr_gid != gid)
		{
			tmp_mem = group->gr_mem;
			while (*tmp_mem != NULL) {
				if (!strcmp(*tmp_mem, user)) {
					num_groups++;
#ifdef GR_DYNAMIC_GROUP_LIST
					group_list = (gid_t *) realloc(group_list, num_groups *
						sizeof(gid_t *));
#endif
					group_list[num_groups-1] = group->gr_gid;
				}
				tmp_mem++;
			}
		}
	}
	close(grp_fd);
	return setgroups(num_groups, group_list);
}
