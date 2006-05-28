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


enum syslog_levels {
	LOG_EMERG = 0,
	LOG_ALERT,
	LOG_CRIT,
	LOG_WARNING,
	LOG_ERR,
	LOG_INFO,
	LOG_DEBUG
};
#include <syslog.h>

long uptime(void);

#define LOG(level, str, args...) udhcp_logging(level, str, ## args)

#if ENABLE_FEATURE_UDHCP_DEBUG
# define DEBUG(level, str, args...) LOG(level, str, ## args)
#else
# define DEBUG(level, str, args...) do {;} while(0)
#endif

#endif
