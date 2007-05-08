/* vi: set sw=4 ts=4: */
/*
 * cryptpw.c
 * 
 * Cooked from passwd.c by Thomas Lundquist <thomasez@zelow.no>
 * 
 */

#include "busybox.h"

int cryptpw_main(int argc, char **argv);
int cryptpw_main(int argc, char **argv)
{
	char *clear;
	char salt[sizeof("$N$XXXXXXXX")]; /* "$N$XXXXXXXX" or "XX" */
	const char *opt_a = "md5";

	getopt32(argc, argv, "a:", &opt_a);
	/* move past the commandline options */
	/*argc -= optind; - unused */
	argv += optind;

	crypt_make_salt(salt, 1); /* des */
	if (strcasecmp(opt_a, "md5") == 0) {
		strcpy(salt, "$1$");
		crypt_make_salt(salt + 3, 4);
	} else if (strcasecmp(opt_a, "des") != 0) {
		bb_show_usage();
	}

	clear = argv[0];
	if (!clear)
		clear = xmalloc_getline(stdin);

	puts(pw_encrypt(clear, salt));
	return 0;
}
