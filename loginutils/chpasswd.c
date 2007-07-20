/* vi: set sw=4 ts=4: */
/*
 * chpasswd.c
 *
 * Written for SLIND (from passwd.c) by Alexander Shishkin <virtuoso@slind.org>
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

#if ENABLE_GETOPT_LONG
#include <getopt.h>

static const struct option chpasswd_opts[] = {
	{ "encrypted", no_argument, NULL, 'e' },
	{ "md5", no_argument, NULL, 'm' },
	{ NULL, 0, NULL, 0 }
};
#endif

#define OPT_ENC		1
#define OPT_MD5		2

int chpasswd_main(int argc, char **argv);
int chpasswd_main(int argc, char **argv)
{
	char *name, *pass;
	char salt[sizeof("$N$XXXXXXXX")];
	int opt, rc;
	int rnd = rnd; /* we *want* it to be non-initialized! */
	const char *pwfile = bb_path_passwd_file;

	if (getuid() != 0)
		bb_error_msg_and_die(bb_msg_perm_denied_are_you_root);

#if ENABLE_FEATURE_SHADOWPASSWDS
	if (access(bb_path_shadow_file, F_OK) == 0)
		pwfile = bb_path_shadow_file;
#endif

 	opt_complementary = "m--e";
	USE_GETOPT_LONG(applet_long_options = chpasswd_opts;)
	opt = getopt32(argc, argv, "em");

	while ((name = xmalloc_getline(stdin)) != NULL) {
		pass = strchr(name, ':');
		if (!pass)
			bb_error_msg_and_die("missing new password");
		*pass++ = '\0';

		//if (!getpwnam(name))
		//	bb_error_msg_and_die("unknown user %s", name);

		if (!(opt & OPT_ENC)) {
			rnd = crypt_make_salt(salt, 1, rnd);
			if (opt & OPT_MD5) {
				strcpy(salt, "$1$");
				rnd = crypt_make_salt(salt + 3, 4, rnd);
			}
			pass = pw_encrypt(pass, salt);
		}

		rc = update_passwd(pwfile, name, pass);
		/* LOGMODE_BOTH logs to syslog */
		logmode = LOGMODE_BOTH;
		if (rc < 0)
			bb_error_msg_and_die("an error occurred updating %s", pwfile);
		if (rc > 0)
			bb_info_msg("Password for '%s' changed", name);
		else
			bb_info_msg("User '%s' not found", name);
		logmode = LOGMODE_STDIO;
		free(name);
	}

	return 0;
}
