#include <stdlib.h>
#include <string.h>
#include "unarchive.h"
#include "libbb.h"

#ifdef L_llist_add_to
extern llist_t *llist_add_to(llist_t *old_head, char *new_item)
{
	llist_t *new_head;

	new_head = xmalloc(sizeof(llist_t));
	new_head->data = new_item;
	new_head->link = old_head;

	return (new_head);
}
#endif

#ifdef L_llist_add_to_end
extern llist_t *llist_add_to_end(llist_t *list_head, char *data)
{
	llist_t *new_item, *tmp, *prev;

	new_item = xmalloc(sizeof(llist_t));
	new_item->data = data;
	new_item->link = NULL;

	prev = NULL;
	tmp = list_head;
	while (tmp) {
		prev = tmp;
		tmp = tmp->link;
	}
	if (prev) {
		prev->link = new_item;
	} else {
		list_head = new_item;
	}

	return (list_head);
}
#endif

