/* vi: set sw=4 ts=4: */
/*
 * universal getopt_ulflags implementation for busybox
 *
 * Copyright (C) 2003-2005  Vladimir Oleynik  <dzo@simtreas.ru>
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

unsigned long
bb_getopt_ulflags (int argc, char **argv, const char *applet_opts, ...)

	The command line options must be declared in const char
	*applet_opts as a string of chars, for example:

	flags = bb_getopt_ulflags(argc, argv, "rnug");

	If one of the given options is found, a flag value is added to
	the return value (an unsigned long).

	The flag value is determined by the position of the char in
	applet_opts string.  For example, in the above case:

	flags = bb_getopt_ulflags(argc, argv, "rnug");

	"r" will add 1    (bit 1 : 0x01)
	"n" will add 2    (bit 2 : 0x02)
	"u  will add 4    (bit 3 : 0x03)
	"g" will add 8    (bit 4 : 0x04)

	 and so on.  You can also look at the return value as a bit
	 field and each option sets one of bits.

 ":"    If one of the options requires an argument, then add a ":"
	after the char in applet_opts and provide a pointer to store
	the argument.  For example:

	char *pointer_to_arg_for_a;
	char *pointer_to_arg_for_b;
	char *pointer_to_arg_for_c;
	char *pointer_to_arg_for_d;

	flags = bb_getopt_ulflags(argc, argv, "a:b:c:d:",
			 &pointer_to_arg_for_a, &pointer_to_arg_for_b,
			 &pointer_to_arg_for_c, &pointer_to_arg_for_d);

	The type of the pointer (char* or llist_t *) may be controlled
	by the "::" special separator that is set in the external string
	bb_opt_complementally (see below for more info).

 "+"    If the first character in the applet_opts string is a plus,
	then option processing will stop as soon as a non-option is
	encountered in the argv array.  Useful for applets like env
	which should not process arguments to subprograms:
	env -i ls -d /
	Here we want env to process just the '-i', not the '-d'.

static const struct option bb_default_long_options[]

	This struct allows you to define long options.  The syntax for
	declaring the array is just like that of getopt's longopts.
	(see getopt(3))

	static const struct option applet_long_options[] = {
		{ "verbose", 0, 0, v },
		{ 0, 0, 0, 0 }
	};
	bb_applet_long_options = applet_long_options;

	The last argument (val) can undefined from applet_opts.
	If you use this, then:
	- return bit have next position after short options
	- if has_arg is not "no_argument", use ptr for arg also
	- bb_opt_complementally have effects for this too

	Note: a good applet will make long options configurable via the
	config process and not a required feature.  The current standard
	is to name the config option CONFIG_FEATURE_<applet>_LONG_OPTIONS.

const char *bb_opt_complementally
	this should be bb_opt_complementary, but we'll just keep it as
	bb_opt_complementally due to the Russian origins

 ":"    The colon (":") is used to separate groups of two or more chars
	and/or groups of chars and special characters (stating some
	conditions to be checked).

 "abc"  If groups of two or more chars are specified, the first char
	is the main option and the other chars are secondary options.
	Their flags will be turned on if the main option is found even
	if they are not specifed on the command line.  For example:

	bb_opt_complementally = "abc";

	flags = bb_getopt_ulflags(argc, argv, "abcd")

	If getopt() finds "-a" on the command line, then
	bb_getopt_ulflags's return value will be as if "-a -b -c" were
	found.

 "ww"   Adjacent double options have a counter associated which indicates
	the number of occurances of the option.
	For example the ps applet needs:
	if w is given once, GNU ps sets the width to 132,
	if w is given more than once, it is "unlimited"

	int w_counter = 0;
	bb_opt_complementally = "ww";
	bb_getopt_ulflags(argc, argv, "w", &w_counter);

	if(w_counter)
		width = (w_counter == 1) ? 132 : INT_MAX;
	else
		get_terminal_width(...&width...);

	w_counter is a pointer to an integer. It has to be passed to
	bb_getopt_ulflags() after all other option argument sinks.
	For example: accept multiple -v to indicate the level of verbosity
	and for each -b optarg, add optarg to my_b. Finally, if b is given,
	turn off c and vice versa:

	llist_t *my_b = NULL;
	int verbose_level = 0;
	bb_opt_complementally = "vv:b::b-c:c-b";
	f = bb_getopt_ulflags(argc, argv, "vb:c", &my_b, &verbose_level);
	if((f & 2))     // -c after -b unset this -b flag
	  while (my_b) { dosomething_with(my_b->data) ; my_b = my_b->link; }
	if(my_b)        // but llist stored always if -b found
		free_llist(my_b);
	if (verbose_level) bb_printf("verbose level is %d\n", verbose_level);

Special characters:

 "-"    A dash between two options causes the second of the two
	to be unset (and ignored or triggered) if it is given on
	the command line.

	For example:
	The du applet has the options "-s" and "-d depth".  If
	bb_getopt_ulflags finds -s, then -d is unset or if it finds -d
	then -s is unset.  (Note:  busybox implements the GNU
	"--max-depth" option as "-d".)  To obtain this behavior, you
	set bb_opt_complementally = "s-d:d-s".  Only one flag value is
	added to bb_getopt_ulflags's return value depending on the
	position of the options on the command line.  If one of the
	two options requires an argument pointer (":" in applet_opts
	as in "d:") optarg is set accordingly.

	char *smax_print_depth;

	bb_opt_complementally = "s-d:d-s:x-x";
	opt = bb_getopt_ulflags(argc, argv, "sd:x", &smax_print_depth);

	if (opt & 2) {
		 max_print_depth = atoi(smax_print_depth);
	}
	if(opt & 4)
		printf("Detected odd -x usaging\n");

 "-"    A dash as the first char in a bb_opt_complementally group means to
	convert the arguments as option. Next char for this case can't set
	[0-9], recomended use ':' or end of line. For example:

	bb_opt_complementally = "-:w-x:x-w";
	bb_getopt_ulflags(argc, argv, "wx");

	Allows any arguments to be given without a dash (./program w x)
	as well as with a dash (./program -x). Why unset -w see above.

 "-N"   A dash as the first char in a bb_opt_complementally group with
	number 0-9 as one char is means check minimal arguments required.

 "V-"   A option with dash before colon or end line indicate: call
	bb_show_usage if this option give, for example verbose
	usage option.

 "--"   A double dash between two options, or between an option and a group
	of options, means that they are mutually exclusive.  Unlike
	the "-" case above, an error will be forced if the options
	are used together.

	For example:
	The cut applet must have only one type of list specified, so
	-b, -c and -f are mutally exclusive and should raise an error
	if specified together.  In this case you must set
	bb_opt_complementally = "b--cf:c--bf:f--bc".  If two of the
	mutually exclusive options are found, bb_getopt_ulflags's
	return value will have the error flag set (BB_GETOPT_ERROR) so
	that we can check for it:

	if (flags & BB_GETOPT_ERROR)
		bb_show_usage();

 "?"    A "ask" as the first char in a bb_opt_complementally group give:
	if previous point set BB_GETOPT_ERROR, don't return and
	call previous example internally. Next char for this case can't
	set to [0-9], recomended use ':' or end of line.

 "?N"   A "ask" as the first char in a bb_opt_complementally group with
	number 0-9 as one char is means check maximal arguments possible.

 "::"   A double colon after a char in bb_opt_complementally means that the
	option can occur multiple times:

	For example:
	The grep applet can have one or more "-e pattern" arguments.
	In this case you should use bb_getopt_ulflags() as follows:

	llist_t *patterns = NULL;

	(this pointer must be initializated to NULL if the list is empty
	as required by *llist_add_to(llist_t *old_head, char *new_item).)

	bb_opt_complementally = "e::";

	bb_getopt_ulflags(argc, argv, "e:", &patterns);
	$ grep -e user -e root /etc/passwd
	root:x:0:0:root:/root:/bin/bash
	user:x:500:500::/home/user:/bin/bash

 "--"   A double dash at the beginning of bb_opt_complementally means the
	argv[1] string should always be treated as options, even if it isn't
	prefixed with a "-".  This is to support the special syntax in applets
	such as "ar" and "tar":
	tar xvf foo.tar

 "?"    An "ask" between main and group options causes the second of the two
	to be depending required as or if first is given on the command line.
	For example from "id" applet:

	// Don't allow -n -r -rn -ug -rug -nug -rnug
	bb_opt_complementally = "r?ug:n?ug:?u--g:g--u";
	flags = bb_getopt_ulflags(argc, argv, "rnug");

	This example allowed only:
	$ id; id -u; id -g; id -ru; id -nu; id -rg; id -ng; id -rnu; id -rng

 "X"    A one options in bb_opt_complementally group means
	requires this option always with "or" logic if more one specified,
	checked after switch off from complementally logic.
	For example from "start-stop-daemon" applet:

	// Don't allow -KS -SK, but -S or -K required
	bb_opt_complementally = "K:S:?K--S:S--K";
	flags = bb_getopt_ulflags(argc, argv, "KS...);


 "x--x" give error if double or more used -x option

 Don't forget ':' store. For example "?322-22-23X-x-a" interpretet as
 "?3:22:-2:2-2:2-3Xa:2--x": max args is 3, count -2 usaged, min args is 2,
 -2 option triggered, unset -3 and -X and -a if -2 any usaged, give error if
 after -2 the -x option usaged.

*/

/* this should be bb_opt_complementary, but we'll just keep it as
   bb_opt_complementally due to the Russian origins */
const char *bb_opt_complementally;

typedef struct {
	int opt;
	int list_flg;
	unsigned long switch_on;
	unsigned long switch_off;
	unsigned long incongruously;
	unsigned long requires;
	void **optarg;               /* char **optarg or llist_t **optarg */
	int *counter;
} t_complementally;

/* You can set bb_applet_long_options for parse called long options */

static const struct option bb_default_long_options[] = {
/*      { "help", 0, NULL, '?' }, */
	{ 0, 0, 0, 0 }
};

const struct option *bb_applet_long_options = bb_default_long_options;

unsigned long
bb_getopt_ulflags (int argc, char **argv, const char *applet_opts, ...)
{
	unsigned long flags = 0;
	unsigned long requires = 0;
	t_complementally complementally[sizeof(flags) * 8 + 1];
	int c;
	const unsigned char *s;
	t_complementally *on_off;
	va_list p;
	const struct option *l_o;
	unsigned long trigger;
#ifdef CONFIG_PS
	char **pargv = NULL;
#endif
	int min_arg = 0;
	int max_arg = -1;

#define SHOW_USAGE_IF_ERROR     1
#define ALL_ARGV_IS_OPTS        2
#define FIRST_ARGV_IS_OPT       4
#define FREE_FIRST_ARGV_IS_OPT  8
	int spec_flgs = 0;

	va_start (p, applet_opts);

	c = 0;
	on_off = complementally;
	memset(on_off, 0, sizeof(complementally));

	/* skip GNU extension */
	s = (const unsigned char *)applet_opts;
	if(*s == '+' || *s == '-')
		s++;
	for (; *s; s++) {
		if(c >= (int)(sizeof(flags)*8))
			break;
		on_off->opt = *s;
		on_off->switch_on = (1 << c);
		if (s[1] == ':') {
			on_off->optarg = va_arg (p, void **);
			do
				s++;
			while (s[1] == ':');
		}
		on_off++;
		c++;
	}

	for(l_o = bb_applet_long_options; l_o->name; l_o++) {
		if(l_o->flag)
			continue;
		for(on_off = complementally; on_off->opt != 0; on_off++)
			if(on_off->opt == l_o->val)
				break;
		if(on_off->opt == 0) {
			if(c >= (int)(sizeof(flags)*8))
				break;
			on_off->opt = l_o->val;
			on_off->switch_on = (1 << c);
			if(l_o->has_arg != no_argument)
				on_off->optarg = va_arg (p, void **);
			c++;
		}
	}
	for (s = (const unsigned char *)bb_opt_complementally; s && *s; s++) {
		t_complementally *pair;
		unsigned long *pair_switch;

		if (*s == ':')
			continue;
		c = s[1];
		if(*s == '?') {
			if(c < '0' || c > '9') {
				spec_flgs |= SHOW_USAGE_IF_ERROR;
			} else {
				max_arg = c - '0';
				s++;
			}
			continue;
		}
		if(*s == '-') {
			if(c < '0' || c > '9') {
				if(c == '-') {
					spec_flgs |= FIRST_ARGV_IS_OPT;
					s++;
				} else
					spec_flgs |= ALL_ARGV_IS_OPTS;
			} else {
				min_arg = c - '0';
				s++;
			}
			continue;
		}
		for (on_off = complementally; on_off->opt; on_off++)
			if (on_off->opt == *s)
				break;
		if(c == ':' && s[2] == ':') {
			on_off->list_flg++;
			continue;
		}
		if(c == ':' || c == '\0') {
			requires |= on_off->switch_on;
			continue;
		}
		if(c == '-' && (s[2] == ':' || s[2] == '\0')) {
			flags |= on_off->switch_on;
			on_off->incongruously |= on_off->switch_on;
			s++;
			continue;
		}
		if(c == *s) {
			on_off->counter = va_arg (p, int *);
			s++;
		}
		pair = on_off;
		pair_switch = &(pair->switch_on);
		for(s++; *s && *s != ':'; s++) {
			if(*s == '?') {
				pair_switch = &(pair->requires);
			} else if (*s == '-') {
				if(pair_switch == &(pair->switch_off))
					pair_switch = &(pair->incongruously);
				else
					pair_switch = &(pair->switch_off);
			} else {
			    for (on_off = complementally; on_off->opt; on_off++)
				if (on_off->opt == *s) {
				    *pair_switch |= on_off->switch_on;
				    break;
				}
			}
		}
		s--;
	}
	va_end (p);

#if defined(CONFIG_AR) || defined(CONFIG_TAR)
	if((spec_flgs & FIRST_ARGV_IS_OPT)) {
		if(argv[1] && argv[1][0] != '-' && argv[1][0] != '\0') {
			argv[1] = bb_xasprintf("-%s", argv[1]);
			if(ENABLE_FEATURE_CLEAN_UP)
				spec_flgs |= FREE_FIRST_ARGV_IS_OPT;
		}
	}
#endif
	while ((c = getopt_long (argc, argv, applet_opts,
				 bb_applet_long_options, NULL)) >= 0) {
#ifdef CONFIG_PS
loop_arg_is_opt:
#endif
		for (on_off = complementally; on_off->opt != c; on_off++) {
			/* c==0 if long opt have non NULL flag */
			if(on_off->opt == 0 && c != 0)
				bb_show_usage ();
		}
		if(flags & on_off->incongruously) {
			if((spec_flgs & SHOW_USAGE_IF_ERROR))
				bb_show_usage ();
			flags |= BB_GETOPT_ERROR;
		}
		trigger = on_off->switch_on & on_off->switch_off;
		flags &= ~(on_off->switch_off ^ trigger);
		flags |= on_off->switch_on ^ trigger;
		flags ^= trigger;
		if(on_off->counter)
			(*(on_off->counter))++;
		if(on_off->list_flg) {
			*(llist_t **)(on_off->optarg) =
			  llist_add_to(*(llist_t **)(on_off->optarg), optarg);
		} else if (on_off->optarg) {
			*(char **)(on_off->optarg) = optarg;
		}
#ifdef CONFIG_PS
		if(pargv != NULL)
			break;
#endif
	}

#ifdef CONFIG_PS
	if((spec_flgs & ALL_ARGV_IS_OPTS)) {
		/* process argv is option, for example "ps" applet */
		if(pargv == NULL)
			pargv = argv + optind;
		while(*pargv) {
			c = **pargv;
			if(c == '\0') {
				pargv++;
			} else {
				(*pargv)++;
				goto loop_arg_is_opt;
			}
		}
	}
#endif

#if (defined(CONFIG_AR) || defined(CONFIG_TAR)) && \
				defined(CONFIG_FEATURE_CLEAN_UP)
	if((spec_flgs & FREE_FIRST_ARGV_IS_OPT))
		free(argv[1]);
#endif
	/* check depending requires for given options */
	for (on_off = complementally; on_off->opt; on_off++) {
		if(on_off->requires && (flags & on_off->switch_on) &&
					(flags & on_off->requires) == 0)
			bb_show_usage ();
	}
	if(requires && (flags & requires) == 0)
		bb_show_usage ();
	argc -= optind;
	if(argc < min_arg || (max_arg >= 0 && argc > max_arg))
		bb_show_usage ();
	return flags;
}
