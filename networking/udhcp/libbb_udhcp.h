/* libbb_udhcp.h - busybox compatability wrapper */

#ifndef _LIBBB_UDHCP_H
#define _LIBBB_UDHCP_H

#ifdef IN_BUSYBOX
#include "libbb.h"

#ifdef CONFIG_FEATURE_UDHCP_SYSLOG
#define SYSLOG
#endif

#ifdef CONFIG_FEATURE_UDHCP_DEBUG
#define DEBUG
#endif

#define COMBINED_BINARY
#include "version.h"

#else /* ! BB_VER */

#define TRUE			1
#define FALSE			0

#define xmalloc malloc

#endif /* BB_VER */

#endif /* _LIBBB_UDHCP_H */
