/* vi: set sw=4 ts=4: */
/*
 * $Id: hostname.c,v 1.36 2003/07/14 21:21:01 andersen Exp $
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

#include "busybox.h"

static void do_sethostname(char *s, int isfile)
{
	FILE *f;

	if (!s)
		return;
	if (!isfile) {
		if (sethostname(s, strlen(s)) < 0) {
			if (errno == EPERM)
				bb_error_msg_and_die(bb_msg_perm_denied_are_you_root);
			else
				bb_perror_msg_and_die("sethostname");
		}
	} else {
		f = xfopen(s, "r");
		while (fgets(bb_common_bufsiz1, sizeof(bb_common_bufsiz1), f) != NULL) {
			if (bb_common_bufsiz1[0] == '#') {
				continue;
			}
			chomp(bb_common_bufsiz1);
			do_sethostname(bb_common_bufsiz1, 0);
		}
		if (ENABLE_FEATURE_CLEAN_UP)
			fclose(f);
	}
}

int hostname_main(int argc, char **argv)
{
	enum {
		OPT_d = 0x1,
		OPT_f = 0x2,
		OPT_i = 0x4,
		OPT_s = 0x8,
		OPT_dfis = 0xf,
	};

	char buf[256];
	char *hostname_str = NULL;

	if (argc < 1)
		bb_show_usage();

	getopt32(argc, argv, "dfisF:", &hostname_str);

	/* Output in desired format */
	if (option_mask32 & OPT_dfis) {
		struct hostent *hp;
		char *p;
		gethostname(buf, sizeof(buf));
		hp = xgethostbyname(buf);
		p = strchr(hp->h_name, '.');
		if (option_mask32 & OPT_f) {
			puts(hp->h_name);
		} else if (option_mask32 & OPT_s) {
			if (p != NULL) {
				*p = 0;
			}
			puts(hp->h_name);
		} else if (option_mask32 & OPT_d) {
			if (p)
				puts(p + 1);
		} else if (option_mask32 & OPT_i) {
			while (hp->h_addr_list[0]) {
				printf("%s ", inet_ntoa(*(struct in_addr *) (*hp->h_addr_list++)));
			}
			puts("");
		}
	}
	/* Set the hostname */
	else if (hostname_str != NULL) {
		do_sethostname(hostname_str, 1);
	} else if (optind < argc) {
		do_sethostname(argv[optind], 0);
	}
	/* Or if all else fails,
	 * just print the current hostname */
	else {
		gethostname(buf, sizeof(buf));
		puts(buf);
	}
	return 0;
}
