/* vi: set sw=4 ts=4: */
/*
 * linked list helper functions.
 *
 * Copyright (C) 2003 Glenn McGrath
 * Copyright (C) 2005 Vladimir Oleynik
 * Copyright (C) 2005 Bernhard Fischer
 * Copyright (C) 2006 Rob Landley <rob@landley.net>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include <stdlib.h>
#include "libbb.h"

/* Add data to the start of the linked list.  */
void llist_add_to(llist_t **old_head, void *data)
{
	llist_t *new_head = xmalloc(sizeof(llist_t));
	new_head->data = data;
	new_head->link = *old_head;
	*old_head = new_head;
}

/* Add data to the end of the linked list.  */
void llist_add_to_end(llist_t **list_head, void *data)
{
	llist_t *new_item = xmalloc(sizeof(llist_t));
	new_item->data = data;
	new_item->link = NULL;

	if (!*list_head) *list_head = new_item;
	else {
		llist_t *tail = *list_head;
		while (tail->link) tail = tail->link;
		tail->link = new_item;
	}
}

/* Remove first element from the list and return it */
void *llist_pop(llist_t **head)
{
	void *data;

	if(!*head) data = *head;
	else {
		void *next = (*head)->link;
		data = (*head)->data;
		free(*head);
		*head = next;
	}

	return data;
}

/* Recursively free all elements in the linked list.  If freeit != NULL
 * call it on each datum in the list */
void llist_free(llist_t *elm, void (*freeit)(void *data))
{
	while (elm) {
		void *data = llist_pop(&elm);
		if (freeit) freeit(data);
	}
}

/* Reverse list order. Useful since getopt32 saves option params
 * in reverse order */
llist_t* rev_llist(llist_t *list)
{
	llist_t *new = NULL;
	while (list) {
		llist_t *next = list->link;
		list->link = new;
		new = list;
		list = next;
	}
	return new;
}
