/* vi: set sw=4 ts=4: */
/*
 * linked list helper functions.
 *
 * Copyright (C) 2003 Glenn McGrath
 * Copyright (C) 2005 Vladimir Oleynik
 * Copyright (C) 2005 Bernhard Fischer
 *
 * Licensed under the GPL v2, see the file LICENSE in this tarball.
 */
#include <stdlib.h>
#include "libbb.h"

#ifdef L_llist_add_to
/* Add data to the start of the linked list.  */
llist_t *llist_add_to(llist_t *old_head, char *new_item)
{
	llist_t *new_head;

	new_head = xmalloc(sizeof(llist_t));
	new_head->data = new_item;
	new_head->link = old_head;

	return (new_head);
}
#endif

#ifdef L_llist_add_to_end
/* Add data to the end of the linked list.  */
llist_t *llist_add_to_end(llist_t *list_head, char *data)
{
	llist_t *new_item;

	new_item = xmalloc(sizeof(llist_t));
	new_item->data = data;
	new_item->link = NULL;

	if (list_head == NULL) {
		list_head = new_item;
	} else {
		llist_t *tail = list_head;
		while (tail->link)
			tail = tail->link;
		tail->link = new_item;
	}
	return list_head;
}
#endif

#ifdef L_llist_free_one
/* Free the current list element and advance to the next entry in the list.
 * Returns a pointer to the next element.  */
llist_t *llist_free_one(llist_t *elm)
{
	llist_t *next = elm ? elm->link : NULL;
	free(elm);
	return next;
}
#endif

#ifdef L_llist_free
/* Recursively free all elements in the linked list.  */
void llist_free(llist_t *elm)
{
	while ((elm = llist_free_one(elm)));
}
#endif
