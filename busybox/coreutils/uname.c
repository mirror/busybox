/* vi: set sw=4 ts=4: */
/* uname -- print system information
   Copyright (C) 1989-1999 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Option		Example

   -s, --sysname	SunOS
   -n, --nodename	rocky8
   -r, --release	4.0
   -v, --version
   -m, --machine	sun
   -a, --all		SunOS rocky8 4.0  sun

   The default behavior is equivalent to `-s'.

   David MacKenzie <djm@gnu.ai.mit.edu> */

/* Busyboxed by Erik Andersen */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/utsname.h>

#if defined (HAVE_SYSINFO) && defined (HAVE_SYS_SYSTEMINFO_H)
# include <sys/systeminfo.h>
#endif
#include "busybox.h"

static void print_element(unsigned int mask, char *element);

/* Values that are bitwise or'd into `toprint'. */
/* Operating system name. */
static const int PRINT_SYSNAME = 1;

/* Node name on a communications network. */
static const int PRINT_NODENAME = 2;

/* Operating system release. */
static const int PRINT_RELEASE = 4;

/* Operating system version. */
static const int PRINT_VERSION = 8;

/* Machine hardware name. */
static const int PRINT_MACHINE = 16;

 /* Host processor type. */
static const int PRINT_PROCESSOR = 32;

/* Mask indicating which elements of the name to print. */
static unsigned char toprint;


int uname_main(int argc, char **argv)
{
	struct utsname name;
	char processor[256];

#if defined(__sparc__) && defined(__linux__)
	char *fake_sparc = getenv("FAKE_SPARC");
#endif

	toprint = 0;

	/* Parse any options */
	//fprintf(stderr, "argc=%d, argv=%s\n", argc, *argv);
	while (--argc > 0 && **(++argv) == '-') {
		while (*(++(*argv))) {
			switch (**argv) {
			case 's':
				toprint |= PRINT_SYSNAME;
				break;
			case 'n':
				toprint |= PRINT_NODENAME;
				break;
			case 'r':
				toprint |= PRINT_RELEASE;
				break;
			case 'v':
				toprint |= PRINT_VERSION;
				break;
			case 'm':
				toprint |= PRINT_MACHINE;
				break;
			case 'p':
				toprint |= PRINT_PROCESSOR;
				break;
			case 'a':
				toprint = (PRINT_SYSNAME | PRINT_NODENAME | PRINT_RELEASE |
						   PRINT_PROCESSOR | PRINT_VERSION |
						   PRINT_MACHINE);
				break;
			default:
				show_usage();
			}
		}
	}

	if (toprint == 0)
		toprint = PRINT_SYSNAME;

	if (uname(&name) == -1)
		perror_msg("cannot get system name");

#if defined (HAVE_SYSINFO) && defined (SI_ARCHITECTURE)
	if (sysinfo(SI_ARCHITECTURE, processor, sizeof(processor)) == -1)
		perror_msg("cannot get processor type");
}

#else
	strcpy(processor, "unknown");
#endif

#if defined(__sparc__) && defined(__linux__)
	if (fake_sparc != NULL
		&& (fake_sparc[0] == 'y'
			|| fake_sparc[0] == 'Y')) strcpy(name.machine, "sparc");
#endif

	print_element(PRINT_SYSNAME, name.sysname);
	print_element(PRINT_NODENAME, name.nodename);
	print_element(PRINT_RELEASE, name.release);
	print_element(PRINT_VERSION, name.version);
	print_element(PRINT_MACHINE, name.machine);
	print_element(PRINT_PROCESSOR, processor);

	return EXIT_SUCCESS;
}

/* If the name element set in MASK is selected for printing in `toprint',
   print ELEMENT; then print a space unless it is the last element to
   be printed, in which case print a newline. */

static void print_element(unsigned int mask, char *element)
{
	if (toprint & mask) {
		toprint &= ~mask;
		printf("%s%c", element, toprint ? ' ' : '\n');
	}
}
