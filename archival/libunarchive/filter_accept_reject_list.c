#include <fnmatch.h>
#include <stdlib.h>
#include "unarchive.h"

/*
 * Accept names that are in the accept list
 */
extern char filter_accept_reject_list(const llist_t *accept_list, const llist_t *reject_list, const char *key)
{
	const llist_t *accept_entry = find_list_entry(accept_list, key);
	const llist_t *reject_entry = find_list_entry(reject_list, key);

	/* Fail if an accept list was specified and the key wasnt in there */
	if (accept_list && (accept_entry == NULL)) {
		return(EXIT_FAILURE);
	}

	/* If the key is in a reject list fail */
	if (reject_entry) {
		return(EXIT_FAILURE);
	}

	/* Accepted */
	return(EXIT_SUCCESS);
}
