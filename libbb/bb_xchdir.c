/* vi: set sw=4 ts=4: */
/*
 * bb_xchdir.c - a chdir() which dies on failure with error message
 *
 * Copyright (C) 2006 Denis Vlasenko
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
#include <unistd.h>
#include "libbb.h"

void bb_xchdir(const char *path)
{
	if (chdir(path))
		bb_perror_msg_and_die("chdir(%s)", path);
}

