#include <fnmatch.h>
#include <stdlib.h>
#include "unarchive.h"
/*
 * Accept names that are in the accept list
 */
extern char filter_accept_all(const llist_t *accept_list, const llist_t *reject_list, const char *key)
{
	return(EXIT_SUCCESS);
}
