/* vi: set sw=4 ts=4: */
/* common.h
 *
 * Russ Dill <Russ.Dill@asu.edu> September 2001
 * Rewritten by Vladimir Oleynik <dzo@simtreas.ru> (C) 2003
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#ifndef _COMMON_H
#define _COMMON_H

#include "libbb_udhcp.h"

long uptime(void);

#if ENABLE_FEATURE_UDHCP_DEBUG
# define DEBUG(str, args...) bb_info_msg(str, ## args)
#else
# define DEBUG(str, args...) do {;} while(0)
#endif

#endif
