/* libbb_udhcp.h - busybox compatability wrapper */

#ifndef _LIBBB_UDHCP_H
#define _LIBBB_UDHCP_H

#ifdef IN_BUSYBOX
#include "busybox.h"

#ifdef CONFIG_FEATURE_UDHCP_SYSLOG
#define SYSLOG
#endif

#ifdef CONFIG_FEATURE_UDHCP_DEBUG
#define DEBUG
#endif

#define COMBINED_BINARY
#include "version.h"

#ifdef CONFIG_INSTALL_NO_USR
#define DEFAULT_SCRIPT  "/share/udhcpc/default.script"
#else
#define DEFAULT_SCRIPT  "/usr/share/udhcpc/default.script"
#endif

#define xfopen bb_xfopen

#else /* ! BB_VER */

#include <stdlib.h>
#include <stdio.h>

#define TRUE			1
#define FALSE			0

#define xmalloc malloc
#define xcalloc calloc

#define DEFAULT_SCRIPT  "/usr/share/udhcpc/default.script"

static inline FILE *xfopen(const char *file, const char *mode)
{
	FILE *fp;
	if (!(fp = fopen(file, mode))) {
		perror("could not open input file");
		exit(0);
	}
	return fp;
}

#endif /* BB_VER */

#endif /* _LIBBB_UDHCP_H */
