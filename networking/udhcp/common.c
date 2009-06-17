/* vi: set sw=4 ts=4: */
/* common.c
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
#include "common.h"

#if defined CONFIG_UDHCP_DEBUG && CONFIG_UDHCP_DEBUG >= 1
unsigned dhcp_verbose;
#endif

const uint8_t MAC_BCAST_ADDR[6] ALIGN2 = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};
