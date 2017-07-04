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
//config:
//config:config FEATURE_SETPRIV_DUMP
//config:	bool "Support dumping current privilege state"
//config:	default y
//config:	depends on SETPRIV
//config:	help
//config:	  Enables the "--dump" switch to print out the current privilege
//config:	  state. This is helpful for diagnosing problems.

//applet:IF_SETPRIV(APPLET(setpriv, BB_DIR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_SETPRIV) += setpriv.o

//usage:#define setpriv_trivial_usage
//usage:	"[OPTIONS] PROG [ARGS]"
//usage:#define setpriv_full_usage "\n\n"
//usage:       "Run PROG with different privilege settings\n"
//usage:	IF_FEATURE_SETPRIV_DUMP(
//usage:     "\n-d,--dump		Show current capabilities"
//usage:	)
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

enum {
	IF_FEATURE_SETPRIV_DUMP(OPTBIT_DUMP,)
	OPTBIT_NNP,

	IF_FEATURE_SETPRIV_DUMP(OPT_DUMP = (1 << OPTBIT_DUMP),)
	OPT_NNP  = (1 << OPTBIT_NNP),
};

#if ENABLE_FEATURE_SETPRIV_DUMP
static int dump(void)
{
	uid_t ruid, euid, suid;
	gid_t rgid, egid, sgid;
	gid_t *gids;
	int ngids;

	getresuid(&ruid, &euid, &suid); /* never fails in Linux */
	getresgid(&rgid, &egid, &sgid); /* never fails in Linux */
	ngids = 0;
	gids = bb_getgroups(&ngids, NULL); /* never fails in Linux */

	printf("uid: %u\n", (unsigned)ruid);
	printf("euid: %u\n", (unsigned)euid);
	printf("gid: %u\n", (unsigned)rgid);
	printf("egid: %u\n", (unsigned)egid);

	printf("Supplementary groups: ");
	if (ngids == 0) {
		printf("[none]");
	} else {
		const char *fmt = ",%u" + 1;
		int i;
		for (i = 0; i < ngids; i++) {
			printf(fmt, (unsigned)gids[i]);
			fmt = ",%u";
		}
	}
	bb_putchar('\n');

	if (ENABLE_FEATURE_CLEAN_UP)
		free(gids);
	return EXIT_SUCCESS;
}
#endif /* FEATURE_SETPRIV_DUMP */

int setpriv_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int setpriv_main(int argc UNUSED_PARAM, char **argv)
{
	static const char setpriv_longopts[] ALIGN1 =
		IF_FEATURE_SETPRIV_DUMP(
		"dump\0"         No_argument	"d"
		)
		"nnp\0"          No_argument	"\xff"
		"no-new-privs\0" No_argument	"\xff"
		;
	int opts;

	applet_long_options = setpriv_longopts;
	opts = getopt32(argv, "+"IF_FEATURE_SETPRIV_DUMP("d"));
	argv += optind;

#if ENABLE_FEATURE_SETPRIV_DUMP
	if (opts & OPT_DUMP) {
		if (argv[0] || (opts - OPT_DUMP) != 0)
			bb_show_usage();
		return dump();
	}
#endif
	if (opts & OPT_NNP) {
		if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0))
			bb_simple_perror_msg_and_die("prctl: NO_NEW_PRIVS");
	}

	if (!argv[0])
		bb_show_usage();
	BB_EXECVP_or_die(argv);
}
