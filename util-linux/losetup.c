/*
 * Mini losetup implementation for busybox
 *
 * Copyright (C) 2002  Matt Kraai.
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

#include <getopt.h>
#include <stdlib.h>

#include "busybox.h"

int
losetup_main (int argc, char **argv)
{
  int delete = 0;
  int offset = 0;
  int opt;

  while ((opt = getopt (argc, argv, "do:")) != -1)
    switch (opt)
      {
      case 'd':
	delete = 1;
	break;

      case 'o':
	offset = parse_number (optarg, NULL);
	break;

      default:
	show_usage ();
      }

  if ((delete && (offset || optind + 1 != argc))
      || (!delete && optind + 2 != argc))
    show_usage ();

  if (delete)
    return del_loop (argv[optind]) ? EXIT_SUCCESS : EXIT_FAILURE;
  else
    return set_loop (argv[optind], argv[optind + 1], offset, &opt)
      ? EXIT_FAILURE : EXIT_SUCCESS;
}
