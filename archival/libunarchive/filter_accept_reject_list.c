#include <fnmatch.h>
#include <stdlib.h>
#include "unarchive.h"

static char check_list(const llist_t *list, const char *filename)
{
	if (list) {
		while (list) {
			if (fnmatch(list->data, filename, 0) == 0) {
				return(EXIT_SUCCESS);
			}
			list = list->link;
		}
	}
	return(EXIT_FAILURE);
}

/*
 * Accept names that are in the accept list
 */
extern char filter_accept_reject_list(const llist_t *accept_list, const llist_t *reject_list, const char *key)
{
	/* Fail if an accept list was specified and the key wasnt in there */
	if ((accept_list) && (check_list(accept_list, key) == EXIT_FAILURE)) {
		return(EXIT_FAILURE);
	}

	/* If the key is in a reject list fail */
	if (check_list(reject_list, key) == EXIT_FAILURE) {
		return(EXIT_FAILURE);
	}

	return(EXIT_SUCCESS);
}
