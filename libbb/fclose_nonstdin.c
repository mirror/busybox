/* vi: set sw=4 ts=4: */
/*
 * fclose_nonstdin implementation for busybox
 *
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

/* A number of standard utilites can accept multiple command line args
 * of '-' for stdin, according to SUSv3.  So we encapsulate the check
 * here to save a little space.
 */

#include <stdio.h>
#include <libbb.h>

int bb_fclose_nonstdin(FILE *f)
{
	if (f != stdin) {
		return fclose(f);
	}
	return 0;
}
