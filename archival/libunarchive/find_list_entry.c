/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 2002 by Glenn McGrath
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <fnmatch.h>
#include <stdlib.h>
#include "unarchive.h"

/* Find a string in a list */
const llist_t *find_list_entry(const llist_t *list, const char *filename)
{
	while (list) {
		if (fnmatch(list->data, filename, FNM_LEADING_DIR) == 0) {
			return (list);
		}
		list = list->link;
	}
	return(NULL);
}
