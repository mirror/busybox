/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
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

#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include "libbb.h"

#ifdef L_safe_strtoi
extern
int safe_strtoi(char *arg, int* value)
{
	int error;
	long lvalue = *value;
	error = safe_strtol(arg, &lvalue);
	*value = (int) lvalue;
	return error;
}
#endif

#ifdef L_safe_strtod
extern
int safe_strtod(char *arg, double* value)
{
	char *endptr;
	int errno_save = errno;

	assert(arg!=NULL);
	errno = 0;
	*value = strtod(arg, &endptr);
	if (errno != 0 || *endptr!='\0' || endptr==arg) {
		return 1;
	}
	errno = errno_save;
	return 0;
}
#endif

#ifdef L_safe_strtol
extern
int safe_strtol(char *arg, long* value)
{
	char *endptr;
	int errno_save = errno;

	assert(arg!=NULL);
	errno = 0;
	*value = strtol(arg, &endptr, 0);
	if (errno != 0 || *endptr!='\0' || endptr==arg) {
		return 1;
	}
	errno = errno_save;
	return 0;
}
#endif

#ifdef L_safe_strtoul
extern
int safe_strtoul(char *arg, unsigned long* value)
{
	char *endptr;
	int errno_save = errno;

	assert(arg!=NULL);
	errno = 0;
	*value = strtoul(arg, &endptr, 0);
	if (errno != 0 || *endptr!='\0' || endptr==arg) {
		return 1;
	}
	errno = errno_save;
	return 0;
}
#endif

