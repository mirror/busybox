/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
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
 *
 */

/* Seems silly to copyright a global variable.  ;-)  Oh well.
 *
 * At least one applet (cmp) returns a value different from the typical
 * EXIT_FAILURE values (1) when an error occurs.  So, make it configurable
 * by the applet.  I suppose we could use a wrapper function to set it, but
 * that too seems silly.
 */

#include <stdlib.h>
#include "libbb.h"

int bb_default_error_retval = EXIT_FAILURE;
