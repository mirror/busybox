/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include "busybox.h"

extern int seq_main(int argc, char **argv)
{
	double last;
	double first = 1;
	double increment = 1;
	double i;

	if (argc == 4) {
		first = atof(argv[1]);
		increment = atof(argv[2]);
	}
	else if (argc == 3) {
		first = atof(argv[1]);
	}
	else if (argc != 2) {
		bb_show_usage();
	}
	last = atof(argv[argc - 1]);

	for (i = first; ((first <= last) ? (i <= last): (i >= last));i += increment) {
		printf("%g\n", i);
	}

	return(EXIT_SUCCESS);
}
