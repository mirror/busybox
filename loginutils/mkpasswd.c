/* vi: set sw=4 ts=4 sts=4: */
/*
 * mkpasswd - Overfeatured front end to crypt(3)
 * Copyright (c) 2008 Bernhard Reutner-Fischer
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

int mkpasswd_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int mkpasswd_main(int argc UNUSED_PARAM, char **argv)
{
	char *chp = NULL, *method = NULL, *salt = NULL;
	char *encrypted;
	int fd = STDIN_FILENO;
	enum {
		OPT_P = (1 << 0),
		OPT_s = (1 << 1),
		OPT_m = (1 << 2),
		OPT_S = (1 << 3)
	};
	static const char methods[] ALIGN1 =
		/*"des\0"*/"md5\0""sha-256\0""sha-512\0";
	enum { TYPE_des, TYPE_md5, TYPE_sha256, TYPE_sha512 };
	unsigned algo = TYPE_des, algobits = 1;
#if ENABLE_GETOPT_LONG
	static const char mkpasswd_longopts[] ALIGN1 =
		"password-fd\0"	Required_argument "P"
		"stdin\0"		No_argument "s"
		"method\0"		Required_argument "m"
		"salt\0"		Required_argument "S"
	;
	applet_long_options = mkpasswd_longopts;
#endif
	opt_complementary = "?1"; /* at most one non-option argument */
	getopt32(argv, "P:sm:S:", &chp, &method, &salt);
	argv += optind;
	if (option_mask32 & OPT_P)
		fd = xatoi_u(chp);
	if (option_mask32 & OPT_m)
		algo = index_in_strings(methods, method) + 1;
	if (*argv) /* we have a cleartext passwd */
		chp = *argv;
	else
		chp = bb_ask(fd, 0, "Password: ");
	if (!salt)
		salt = xmalloc(128);

	if (algo) {
		char foo[2];
		foo[0] = foo[2] = '$';
		algobits = 4;
		/* MD5 == "$1$", SHA-256 == "$5$", SHA-512 == "$6$" */
		if (algo > 1) {
			algo += 3;
			algobits = 8;
		}
		foo[1] = '0' + (algo);
		strcpy(salt, foo);
	}
	/* The opt_complementary adds a bit of additional noise, which is good
	   but not strictly needed.  */
	crypt_make_salt(salt + ((!!algo) * 3), algobits, (int)&opt_complementary);
	encrypted = pw_encrypt(chp, salt, 1);
	puts(encrypted);
	if (ENABLE_FEATURE_CLEAN_UP) {
		free(encrypted);
	}
	return EXIT_SUCCESS;
}
