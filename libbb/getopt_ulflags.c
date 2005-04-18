/* vi: set sw=4 ts=4: */
/*
 * universal getopt_ulflags implementation for busybox
 *
 * Copyright (C) 2003  Vladimir Oleynik  <dzo@simtreas.ru>
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
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "libbb.h"

/*                  Documentation !

bb_getopt_ulflags (int argc, char **argv, const char *applet_opts, ...)

          The command line options must be declared in const char
          *applet_opts as a string of chars, for example:

          flags = bb_getopt_ulflags(argc, argv, "rnug");

          If one of the given options is found a flag value is added to
          the unsigned long returned by bb_getopt_ulflags.

          The value of this flag is given by the position of the char in
          const char *applet_opts so for example in this case:

          flags = bb_getopt_ulflags(argc, argv, "rnug");

          "r" will add 1
          "n" will add 2
          "u  will add 4
          "g" will add 8

           and so on.

           If an argument is required by one of the options add a ":"
           after the char in const char *applet_opts and provide a pointer
           where the arg could be stored if it is found, for example:

           char *pointer_to_arg_for_a;
           char *pointer_to_arg_for_b;
           char *pointer_to_arg_for_c;
           char *pointer_to_arg_for_d;

           flags = bb_getopt_ulflags(argc, argv, "a:b:c:d:",
                            &pointer_to_arg_for_a, &pointer_to_arg_for_b,
                            &pointer_to_arg_for_c, &pointer_to_arg_for_d);

           The type of the pointer (char* or llist_t *) can be influenced
           by the "*" special character that can be set in const char
           *bb_opt_complementaly (see below).

const char *bb_opt_complementaly

   ":"     The colon (":") is used in bb_opt_complementaly as separator
           between groups of two or more chars and/or groups of chars and
           special characters (stating some conditions to be checked).

   "abc"   If groups of two or more chars are specified the first char
           is the main option and the other chars are secondary options
           whose flags will be turned on if the main option is found even
           if they are not specifed on the command line, for example:

           bb_opt_complementaly = "abc";

           flags = bb_getopt_ulflags(argc, argv, "abcd")

           If getopt() finds "-a" on the command line, then
           bb_getopt_ulflags's return value will be as if "-a -b -c" were
           found.

Special characters:

   "-"     A dash between two options causes the second of the two
           to be unset (and ignored) if it is given on the command line.

           For example:
           The du applet can have the options "-s" and "-d depth", if
           bb_getopt_ulflags finds -s then -d is unset or if it finds -d
           then -s is unset.  (Note:  busybox implements the GNU
           "--max-depth" option as "-d".)  In this case bb_getopt_ulflags's
           return value has no error flag set (0x80000000UL).  To achieve
           this result you must set bb_opt_complementaly = "s-d:d-s".
           Only one flag value is added to bb_getopt_ulflags's return
           value depending on the position of the options on the command
           line.  If one of the two options requires an argument pointer
           (":" in const char *applet_opts as in "d:") optarg is set
           accordingly.

           char *smax_print_depth;

           bb_opt_complementaly = "s-d:d-s";
           opt = bb_getopt_ulflags(argc, argv, "sd:" , &smax_print_depth);

           if (opt & 2) {
                    max_print_depth = bb_xgetularg10_bnd(smax_print_depth,
                                0, INT_MAX);
           }

   "~"     A tilde between two options or between an option and a group
           of options means that they are mutually exclusive.  Unlike
           the "-" case above, an error will be forced if the options
           are used together.

           For example:
           The cut applet must have only one type of list specified, so
           -b, -c and -f are mutally exclusive and should raise an error
           if specified together.  In this case you must set
           bb_opt_complementaly = "b~cf:c~bf:f~bc".  If two of the
           mutually exclusive options are found, bb_getopt_ulflags's
           return value will have the error flag set (0x80000000UL) so
           that we can check for it:

           if ((flags & 0x80000000UL)
                   bb_show_usage();

   "*"     A star after a char in bb_opt_complementaly means that the
           option can occur multiple times:

           For example:
           The grep applet can have one or more "-e pattern" arguments.
           In this case you should use bb_getopt_ulflags() as follows:

           llist_t *patterns=NULL;

           (this pointer must be initializated to NULL if the list is empty
           as required by *llist_add_to(llist_t *old_head, char *new_item).)

           bb_opt_complementaly = "e*";

           bb_getopt_ulflags (argc, argv, "e:", &patterns);
           grep -e user -e root /etc/passwd
           root:x:0:0:root:/root:/bin/bash
           user:x:500:500::/home/user:/bin/bash

*/

const char *bb_opt_complementaly;

typedef struct {
	unsigned char opt;
	char list_flg;
	unsigned long switch_on;
	unsigned long switch_off;
	unsigned long incongruously;
	void **optarg;               /* char **optarg or llist_t **optarg */
} t_complementaly;

/* You can set bb_applet_long_options for parse called long options */

static const struct option bb_default_long_options[] = {
/*	{ "help", 0, NULL, '?' }, */
	{ 0, 0, 0, 0 }
};

const struct option *bb_applet_long_options = bb_default_long_options;

unsigned long
bb_getopt_ulflags (int argc, char **argv, const char *applet_opts, ...)
{
	unsigned long flags = 0;
	t_complementaly complementaly[sizeof(flags) * 8 + 1];
	int c;
	const unsigned char *s;
	t_complementaly *on_off;
	va_list p;

	va_start (p, applet_opts);

	/* skip GNU extension */
	s = applet_opts;
	if(*s == '+' || *s == '-')
		s++;

	c = 0;
	on_off = complementaly;
	for (; *s; s++) {
		if(c >= (sizeof(flags)*8))
			break;
		on_off->opt = *s;
		on_off->switch_on = (1 << c);
		on_off->list_flg = 0;
		on_off->switch_off = 0;
		on_off->incongruously = 0;
		on_off->optarg = NULL;
		if (s[1] == ':') {
			on_off->optarg = va_arg (p, void **);
			do
				s++;
			while (s[1] == ':');
		}
		on_off++;
		c++;
	}
	on_off->opt = 0;
	c = 0;
	for (s = bb_opt_complementaly; s && *s; s++) {
		t_complementaly *pair;

		if (*s == ':') {
			c = 0;
			continue;
		}
		if (c)
			continue;
		for (on_off = complementaly; on_off->opt; on_off++)
			if (on_off->opt == *s)
				break;
		pair = on_off;
		for(s++; *s && *s != ':'; s++) {
			if (*s == '-' || *s == '~') {
				c = *s;
			} else if(*s == '*') {
				pair->list_flg++;
			} else {
				unsigned long *pair_switch = &(pair->switch_on);
				if(c)
					pair_switch = c == '-' ? &(pair->switch_off) : &(pair->incongruously);
				for (on_off = complementaly; on_off->opt; on_off++)
					if (on_off->opt == *s) {
						*pair_switch |= on_off->switch_on;
						break;
					}
			}
		}
		s--;
	}

	while ((c = getopt_long (argc, argv, applet_opts,
	                         bb_applet_long_options, NULL)) > 0) {
		for (on_off = complementaly; on_off->opt != c; on_off++) {
			if(!on_off->opt)
				bb_show_usage ();
		}
		if(flags & on_off->incongruously)
			flags |= 0x80000000UL;
		flags &= ~on_off->switch_off;
		flags |= on_off->switch_on;
		if(on_off->list_flg) {
			*(llist_t **)(on_off->optarg) =
				llist_add_to(*(llist_t **)(on_off->optarg), optarg);
		} else if (on_off->optarg) {
			*(char **)(on_off->optarg) = optarg;
		}
	}

	return flags;
}
