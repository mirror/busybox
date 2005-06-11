/*
 * dev.c - allocation/initialization/free routines for dev
 *
 * Copyright (C) 2001 Andreas Dilger
 * Copyright (C) 2003 Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 * %End-Header%
 */

#include <stdlib.h>
#include <string.h>

#include "blkidP.h"

blkid_dev blkid_new_dev(void)
{
	blkid_dev dev;

	if (!(dev = (blkid_dev) calloc(1, sizeof(struct blkid_struct_dev))))
		return NULL;

	INIT_LIST_HEAD(&dev->bid_devs);
	INIT_LIST_HEAD(&dev->bid_tags);

	return dev;
}

void blkid_free_dev(blkid_dev dev)
{
	if (!dev)
		return;

	DBG(DEBUG_DEV,
	    printf("  freeing dev %s (%s)\n", dev->bid_name, dev->bid_type));
	DEB_DUMP_DEV(DEBUG_DEV, dev);

	list_del(&dev->bid_devs);
	while (!list_empty(&dev->bid_tags)) {
		blkid_tag tag = list_entry(dev->bid_tags.next,
					   struct blkid_struct_tag,
					   bit_tags);
		blkid_free_tag(tag);
	}
	if (dev->bid_name)
		free(dev->bid_name);
	free(dev);
}

/*
 * Given a blkid device, return its name
 */
extern const char *blkid_dev_devname(blkid_dev dev)
{
	return dev->bid_name;
}

/*
 * dev iteration routines for the public libblkid interface.
 *
 * These routines do not expose the list.h implementation, which are a
 * contamination of the namespace, and which force us to reveal far, far
 * too much of our internal implemenation.  I'm not convinced I want
 * to keep list.h in the long term, anyway.  It's fine for kernel
 * programming, but performance is not the #1 priority for this
 * library, and I really don't like the tradeoff of type-safety for
 * performance for this application.  [tytso:20030125.2007EST]
 */

/*
 * This series of functions iterate over all devices in a blkid cache
 */
#define DEV_ITERATE_MAGIC	0x01a5284c
	
struct blkid_struct_dev_iterate {
	int			magic;
	blkid_cache		cache;
	struct list_head	*p;
};

extern blkid_dev_iterate blkid_dev_iterate_begin(blkid_cache cache)
{
	blkid_dev_iterate	iter;

	iter = xmalloc(sizeof(struct blkid_struct_dev_iterate));
	iter->magic = DEV_ITERATE_MAGIC;
	iter->cache = cache;
	iter->p	= cache->bic_devs.next;
	return (iter);
}

/*
 * Return 0 on success, -1 on error
 */
extern int blkid_dev_next(blkid_dev_iterate iter,
			  blkid_dev *dev)
{
	*dev = 0;
	if (!iter || iter->magic != DEV_ITERATE_MAGIC ||
	    iter->p == &iter->cache->bic_devs)
		return -1;
	*dev = list_entry(iter->p, struct blkid_struct_dev, bid_devs);
	iter->p = iter->p->next;
	return 0;
}

extern void blkid_dev_iterate_end(blkid_dev_iterate iter)
{
	if (!iter || iter->magic != DEV_ITERATE_MAGIC)
		return;
	iter->magic = 0;
	free(iter);
}

