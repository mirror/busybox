#include <fnmatch.h>
#include <stdlib.h>
#include "unarchive.h"
/*
 * Accept names that are in the accept list
 */
extern char filter_accept_list(const llist_t *accept_list, const llist_t *reject_list, const char *key)
{
	llist_t *accept_old;

	while (accept_list) {
		if (fnmatch(accept_list->data, key, 0) == 0) {
			/* Remove entry from list */
			accept_old->link = accept_list->link;
			free(accept_list->data);
			free(accept_list);
			accept_list = accept_old;
			return(EXIT_SUCCESS);
		}
		accept_old = accept_list;
		accept_list = accept_list->link;
	}
	return(EXIT_FAILURE);
}
