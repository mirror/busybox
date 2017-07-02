/* vi: set sw=4 ts=4: */
/*
 * setpriv implementation for busybox based on linux-utils-ng 2.29
 *
 * Copyright (C) 2017 by  <assafgordon@gmail.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 *
 */
//config:config SETPRIV
//config:	bool "setpriv"
//config:	default y
//config:	select PLATFORM_LINUX
//config:	select LONG_OPTS
//config:	help
//config:	  Run a program with different Linux privilege settings.
//config:	  Requires kernel >= 3.5

//applet:IF_SETPRIV(APPLET(setpriv, BB_DIR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_SETPRIV) += setpriv.o

//usage:#define setpriv_trivial_usage
//usage:	"[OPTIONS] PROG [ARGS]"
//usage:#define setpriv_full_usage "\n\n"
//usage:       "Run PROG with different privilege settings\n"
//usage:     "\n--nnp,--no-new-privs	Ignore setuid/setgid bits and file capabilities"

//setpriv from util-linux 2.28:
// -d, --dump               show current state (and do not exec anything)
// --nnp, --no-new-privs    disallow granting new privileges
// --inh-caps <caps,...>    set inheritable capabilities
// --bounding-set <caps>    set capability bounding set
// --ruid <uid>             set real uid
// --euid <uid>             set effective uid
// --rgid <gid>             set real gid
// --egid <gid>             set effective gid
// --reuid <uid>            set real and effective uid
// --regid <gid>            set real and effective gid
// --clear-groups           clear supplementary groups
// --keep-groups            keep supplementary groups
// --groups <group,...>     set supplementary groups
// --securebits <bits>      set securebits
// --selinux-label <label>  set SELinux label
// --apparmor-profile <pr>  set AppArmor profile

#include <sys/prctl.h>
#include "libbb.h"

#ifndef PR_SET_NO_NEW_PRIVS
#define PR_SET_NO_NEW_PRIVS 38
#endif

int setpriv_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int setpriv_main(int argc UNUSED_PARAM, char **argv)
{
	static const char setpriv_longopts[] ALIGN1 =
		"nnp\0"          No_argument	"\xff"
		"no-new-privs\0" No_argument	"\xff"
		;
	int opts;

	opt_complementary = "-1";
	applet_long_options = setpriv_longopts;
	opts = getopt32(argv, "+");

	if (opts) {
		if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0))
			bb_simple_perror_msg_and_die("prctl: NO_NEW_PRIVS");
	}

	argv += optind;
	BB_EXECVP_or_die(argv);
}
