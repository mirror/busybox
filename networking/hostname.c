/* vi: set sw=4 ts=4: */
/*
 * Mini hostname implementation for busybox
 *
 * Copyright (C) 1999 by Randolph Chung <tausq@debian.org>
 *
 * adjusted by Erik Andersen <andersen@codepoet.org> to remove
 * use of long options and GNU getopt.  Improved the usage info.
 *
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

static void do_sethostname(char *s, int isfile)
{
	if (!s)
		return;
	if (isfile) {
		parser_t *parser = config_open2(s, xfopen_for_read);
		while (config_read(parser, &s, 1, 1, "# \t", PARSE_NORMAL & ~PARSE_GREEDY)) {
			do_sethostname(s, 0);
		}
		if (ENABLE_FEATURE_CLEAN_UP)
			config_close(parser);
	} else if (sethostname(s, strlen(s)) < 0) {
		if (errno == EPERM)
			bb_error_msg_and_die(bb_msg_perm_denied_are_you_root);
		bb_perror_msg_and_die("sethostname");
	}
}

int hostname_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int hostname_main(int argc, char **argv)
{
	enum {
		OPT_d = 0x1,
		OPT_f = 0x2,
		OPT_i = 0x4,
		OPT_s = 0x8,
		OPT_F = 0x10,
		OPT_dfis = 0xf,
	};

	char *buf;
	char *hostname_str;

	if (argc < 1)
		bb_show_usage();

	getopt32(argv, "dfisF:", &hostname_str);
	argv += optind;
	buf = safe_gethostname();

	/* Output in desired format */
	if (option_mask32 & OPT_dfis) {
		struct hostent *hp;
		char *p;
		hp = xgethostbyname(buf);
		p = strchr(hp->h_name, '.');
		if (option_mask32 & OPT_f) {
			puts(hp->h_name);
		} else if (option_mask32 & OPT_s) {
			if (p)
				*p = '\0';
			puts(hp->h_name);
		} else if (option_mask32 & OPT_d) {
			if (p)
				puts(p + 1);
		} else if (option_mask32 & OPT_i) {
			while (hp->h_addr_list[0]) {
				printf("%s ", inet_ntoa(*(struct in_addr *) (*hp->h_addr_list++)));
			}
			bb_putchar('\n');
		}
	}
	/* Set the hostname */
	else if (option_mask32 & OPT_F) {
		do_sethostname(hostname_str, 1);
	} else if (argv[0]) {
		do_sethostname(argv[0], 0);
	}
	/* Or if all else fails,
	 * just print the current hostname */
	else {
		puts(buf);
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		free(buf);
	return EXIT_SUCCESS;
}
