/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <stdio.h>
#include "libbb.h"


/* Busybox mount uses either /proc/mounts or /dev/mtab to 
 * get the list of currently mounted filesystems */ 
#if defined BB_FEATURE_MTAB_SUPPORT
const char mtab_file[] = "/etc/mtab";
#else
#  if defined BB_FEATURE_USE_DEVPS_PATCH
      const char mtab_file[] = "/dev/mtab";
#  else
      const char mtab_file[] = "/proc/mounts";
#  endif
#endif


/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
