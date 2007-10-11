/* vi: set sw=4 ts=4: */
/*
 * cryptpw.c
 *
 * Cooked from passwd.c by Thomas Lundquist <thomasez@zelow.no>
 */

#include "libbb.h"

int cryptpw_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int cryptpw_main(int argc, char **argv)
{
	char salt[sizeof("$N$XXXXXXXX")];

	if (!getopt32(argv, "a:", NULL) || argv[optind - 1][0] != 'd') {
		strcpy(salt, "$1$");
		/* Too ugly, and needs even more magic to handle endianness: */
		//((uint32_t*)&salt)[0] = '$' + '1'*0x100 + '$'*0x10000;
		/* Hope one day gcc will do it itself (inlining strcpy) */
		crypt_make_salt(salt + 3, 4, 0); /* md5 */
	} else {
		crypt_make_salt(salt, 1, 0);     /* des */
	}

	puts(pw_encrypt(argv[optind] ? argv[optind] : xmalloc_getline(stdin), salt));

	return 0;
}
