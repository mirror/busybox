/* vi: set sw=4 ts=4: */
/*
 * Mini xgethostbyname2 implementation.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <netdb.h>
#include "libbb.h"


#ifdef CONFIG_FEATURE_IPV6
struct hostent *xgethostbyname2(const char *name, int af)
{
	struct hostent *retval;

	if ((retval = gethostbyname2(name, af)) == NULL)
		bb_herror_msg_and_die("%s", name);

	return retval;
}
#endif
