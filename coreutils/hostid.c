/* vi: set sw=4 ts=4: */
/*
 * Mini hostid implementation for busybox
 *
 * Copyright (C) 2000  Edward Betts <edward@debian.org>.
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

#warning This applet has moved to netkit-tiny.  After BusyBox 0.49, this
#warning applet will be removed from BusyBox.  All maintenance efforts
#warning should be done in the netkit-tiny source tree.

#include "busybox.h"
#include <stdio.h>

extern int hostid_main(int argc, char **argv)
{
	printf("%lx\n", gethostid());
	return EXIT_SUCCESS;
}
