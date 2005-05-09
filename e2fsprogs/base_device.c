/*
 * base_device.c
 *
 * Return the "base device" given a particular device; this is used to
 * assure that we only fsck one partition on a particular drive at any
 * one time.  Otherwise, the disk heads will be seeking all over the
 * place.  If the base device can not be determined, return NULL.
 * 
 * The base_device() function returns an allocated string which must
 * be freed.
 * 
 * Written by Theodore Ts'o, <tytso@mit.edu>
 * 
 * Copyright (C) 2000 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "busybox.h"

#ifdef CONFIG_FEATURE_DEVFS
/*
 * Required for the uber-silly devfs /dev/ide/host1/bus2/target3/lun3
 * pathames.
 */
static const char *devfs_hier[] = {
	"host", "bus", "target", "lun", 0
};
#endif

char *base_device(const char *device)
{
	char *str, *cp;
#ifdef CONFIG_FEATURE_DEVFS
	const char **hier, *disk;
	int len;
#endif

	cp = str = bb_xstrdup(device);

	/* Skip over /dev/; if it's not present, give up. */
	if (strncmp(cp, "/dev/", 5) != 0)
		goto errout;
	cp += 5;

#if 0	/* this is for old stuff no one uses anymore ? */
	/* Skip over /dev/dsk/... */
	if (strncmp(cp, "dsk/", 4) == 0)
		cp += 4;
#endif

	/*
	 * For md devices, we treat them all as if they were all
	 * on one disk, since we don't know how to parallelize them.
	 */
	if (cp[0] == 'm' && cp[1] == 'd') {
		*(cp+2) = 0;
		return str;
	}

	/* Handle DAC 960 devices */
	if (strncmp(cp, "rd/", 3) == 0) {
		cp += 3;
		if (cp[0] != 'c' || cp[2] != 'd' ||
		    !isdigit(cp[1]) || !isdigit(cp[3]))
			goto errout;
		*(cp+4) = 0;
		return str;
	}

	/* Now let's handle /dev/hd* and /dev/sd* devices.... */
	if ((cp[0] == 'h' || cp[0] == 's') && (cp[1] == 'd')) {
		cp += 2;
		/* If there's a single number after /dev/hd, skip it */
		if (isdigit(*cp))
			cp++;
		/* What follows must be an alpha char, or give up */
		if (!isalpha(*cp))
			goto errout;
		*(cp + 1) = 0;
		return str;
	}

#ifdef CONFIG_FEATURE_DEVFS
	/* Now let's handle devfs (ugh) names */
	len = 0;
	if (strncmp(cp, "ide/", 4) == 0)
		len = 4;
	if (strncmp(cp, "scsi/", 5) == 0)
		len = 5;
	if (len) {
		cp += len;
		/*
		 * Now we proceed down the expected devfs hierarchy.
		 * i.e., .../host1/bus2/target3/lun4/...
		 * If we don't find the expected token, followed by
		 * some number of digits at each level, abort.
		 */
		for (hier = devfs_hier; *hier; hier++) {
			len = strlen(*hier);
			if (strncmp(cp, *hier, len) != 0)
				goto errout;
			cp += len;
			while (*cp != '/' && *cp != 0) {
				if (!isdigit(*cp))
					goto errout;
				cp++;
			}
			cp++;
		}
		*(cp - 1) = 0;
		return str;
	}

	/* Now handle devfs /dev/disc or /dev/disk names */
	disk = 0;
	if (strncmp(cp, "discs/", 6) == 0)
		disk = "disc";
	else if (strncmp(cp, "disks/", 6) == 0)
		disk = "disk";
	if (disk) {
		cp += 6;
		if (strncmp(cp, disk, 4) != 0)
			goto errout;
		cp += 4;
		while (*cp != '/' && *cp != 0) {
			if (!isdigit(*cp))
				goto errout;
			cp++;
		}
		*cp = 0;
		return str;
	}
#endif

errout:
	free(str);
	return NULL;
}
