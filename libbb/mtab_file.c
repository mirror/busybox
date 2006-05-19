/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <stdio.h>
#include "libbb.h"


/* Busybox mount uses either /proc/mounts or /etc/mtab to
 * get the list of currently mounted filesystems */
#if defined(CONFIG_FEATURE_MTAB_SUPPORT)
const char bb_path_mtab_file[] = "/etc/mtab";
#else
const char bb_path_mtab_file[] = "/proc/mounts";
#endif
