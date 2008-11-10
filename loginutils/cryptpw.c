/* vi: set sw=4 ts=4: */
/*
 * cryptpw.c
 *
 * Cooked from passwd.c by Thomas Lundquist <thomasez@zelow.no>
 */

#include "libbb.h"

#define TESTING 0

/*
set TESTING to 1 and pipe some file through this script
if you played with bbox's crypt implementation.

while read line; do
	n=`./busybox cryptpw -a des -- "$line"`
	o=`./busybox_org cryptpw -a des -- "$line"`
	test "$n" != "$o" && {
		echo n="$n"
		echo o="$o"
		exit
	}
	n=`./busybox cryptpw -- "$line"`
	o=`./busybox_org cryptpw -- "$line"`
	test "$n" != "$o" && {
		echo n="$n"
		echo o="$o"
		exit
	}
done
 */

int cryptpw_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int cryptpw_main(int argc UNUSED_PARAM, char **argv)
{
	char salt[sizeof("$N$") + 16];
	char *opt_a;
	int opts;

	opts = getopt32(argv, "a:", &opt_a);

	if (opts && opt_a[0] == 'd') {
		crypt_make_salt(salt, 2/2, 0);     /* des */
#if TESTING
		strcpy(salt, "a.");
#endif
	} else {
		salt[0] = '$';
		salt[1] = '1';
		salt[2] = '$';
#if !ENABLE_USE_BB_CRYPT || ENABLE_USE_BB_CRYPT_SHA
		if (opts && opt_a[0] == 's') {
			salt[1] = '5' + (strcmp(opt_a, "sha512") == 0);
			crypt_make_salt(salt + 3, 16/2, 0); /* sha */
#if TESTING
			strcpy(salt, "$6$em7yVj./Mv5n1V5X");
#endif
		} else
#endif
		{
			crypt_make_salt(salt + 3, 8/2, 0); /* md5 */
#if TESTING
			strcpy(salt + 3, "ajg./bcf");
#endif
		}
	}

	puts(pw_encrypt(argv[optind] ? argv[optind] : xmalloc_fgetline(stdin), salt, 1));

	return 0;
}
