/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 2009 Denys Vlasenko.
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* Just #include "autoconf.h" doesn't work for builds in separate
 * object directory */
#include "autoconf.h"

#define SKIP_applet_main
#define ALIGN1 /* nothing, just to placate applet_tables.h */
#define ALIGN2 /* nothing, just to placate applet_tables.h */
#include "applet_tables.h"

/* Since we can't use platform.h, have to do this again by hand: */
#if ENABLE_NOMMU
# define BB_MMU 0
# define USE_FOR_NOMMU(...) __VA_ARGS__
# define USE_FOR_MMU(...)
#else
# define BB_MMU 1
# define USE_FOR_NOMMU(...)
# define USE_FOR_MMU(...) __VA_ARGS__
#endif

static const char usage_messages[] = ""
#define MAKE_USAGE
#include "usage.h"
#include "applets.h"
;

int main(void)
{
	const char *names;
	const char *usage;
	int col, len2;

	col = 0;
	names = applet_names;
	while (*names) {
		len2 = strlen(names) + 2;
		if (col >= 76 - len2) {
			printf(",\n");
			col = 0;
		}
		if (col == 0) {
			col = 6;
			printf("\t");
		} else {
			printf(", ");
		}
		printf(names);
		col += len2;
		names += len2 - 1;
	}
	printf("\n\n");

	printf("=head1 COMMAND DESCRIPTIONS\n\n");
	printf("=over 4\n\n");

	names = applet_names;
	usage = usage_messages;
	while (*names) {
		if (*names >= 'a' && *names <= 'z'
		 && *usage != NOUSAGE_STR[0]
		) {
			printf("=item B<%s>\n\n", names);
			if (*usage)
				printf("%s %s\n\n", names, usage);
			else
				printf("%s\n\n", names);
		}
		names += strlen(names) + 1;
		usage += strlen(usage) + 1;
	}
	return 0;
}

/* TODO: we used to make options bold with B<> and output an example too:

=item B<cat>

cat [B<-u>] [FILE]...

Concatenate FILE(s) and print them to stdout

Options:
        -u      Use unbuffered i/o (ignored)

Example:
        $ cat /proc/uptime
        110716.72 17.67

*/
