/* vi: set sw=4 ts=4: */
/*
 * deluser (remove lusers from the system ;) for TinyLogin
 *
 * Copyright (C) 1999 by Lineo, inc. and John Beppu
 * Copyright (C) 1999,2000,2001 by John Beppu <beppu@codepoet.org>
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

#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "busybox.h"


#if ! defined CONFIG_DELUSER
#include "delline.c"
#else
extern int del_line_matching(const char *login, const char *filename);
#endif

int delgroup_main(int argc, char **argv)
{
	/* int successful; */
	int failure;

	if (argc != 2) {
		bb_show_usage();
	} else {

		failure = del_line_matching(argv[1], bb_path_group_file);
#ifdef CONFIG_FEATURE_SHADOWPASSWDS
		if (access(bb_path_gshadow_file, W_OK) == 0) {
			/* EDR the |= works if the error is not 0, so he had it wrong */
			failure |= del_line_matching(argv[1], bb_path_gshadow_file);
		}
#endif
		if (failure) {
			bb_error_msg_and_die("%s: Group could not be removed\n", argv[1]);
		}

	}
	return (EXIT_SUCCESS);
}

/* $Id: delgroup.c,v 1.2 2003/07/14 21:50:51 andersen Exp $ */
