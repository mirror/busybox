#include <stdlib.h>
#include <string.h>
#include "unarchive.h"
#include "libbb.h"

extern const llist_t *add_to_list(const llist_t *old_head, const char *new_item)
{
	llist_t *new_head;

	new_head = xmalloc(sizeof(llist_t));
	new_head->data = new_item;
	new_head->link = old_head;

	return(new_head);
}
