/* vi: set sw=4 ts=4: */
/*
 * Storing and retrieving data for static leases
 *
 * Wade Berrier <wberrier@myrealbox.com> September 2004
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#include "common.h"
#include "dhcpd.h"

/* Check to see if an IP is reserved as a static IP */
int FAST_FUNC is_nip_reserved(struct static_lease *st_lease, uint32_t nip)
{
	while (st_lease) {
		if (st_lease->nip == nip)
			return 1;
		st_lease = st_lease->next;
	}

	return 0;
}
