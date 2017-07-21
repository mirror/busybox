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
//config:	bool "setpriv (3.4 kb)"
//config:	default y
//config:	select PLATFORM_LINUX
//config:	select LONG_OPTS
//config:	help
//config:	Run a program with different Linux privilege settings.
//config:	Requires kernel >= 3.5
//config:
//config:config FEATURE_SETPRIV_DUMP
//config:	bool "Support dumping current privilege state"
//config:	default y
//config:	depends on SETPRIV
//config:	help
//config:	Enables the "--dump" switch to print out the current privilege
//config:	state. This is helpful for diagnosing problems.
//config:
//config:config FEATURE_SETPRIV_CAPABILITIES
//config:	bool "Support capabilities"
//config:	default y
//config:	depends on SETPRIV
//config:	help
//config:	Capabilities can be used to grant processes additional rights
//config:	without the necessity to always execute as the root user.
//config:	Enabling this option enables "--dump" to show information on
//config:	capabilities.
//config:
//config:config FEATURE_SETPRIV_CAPABILITY_NAMES
//config:	bool "Support capability names"
//config:	default y
//config:	depends on SETPRIV && FEATURE_SETPRIV_CAPABILITIES
//config:	help
//config:	Capabilities can be either referenced via a human-readble name,
//config:	e.g. "net_admin", or using their index, e.g. "cap_12". Enabling
//config:	this option allows using the human-readable names in addition to
//config:	the index-based names.

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
//usage:	IF_FEATURE_SETPRIV_CAPABILITIES(
//usage:     "\n--inh-caps CAP,CAP	Set inheritable capabilities"
//usage:     "\n--ambient-caps CAP,CAP	Set ambient capabilities"
//usage:	)

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

#if ENABLE_FEATURE_SETPRIV_CAPABILITIES
#include <linux/capability.h>
// #include <sys/capability.h>
// This header is in libcap, but the functions are in libc.
// Comment in the header says this above capset/capget:
/* system calls - look to libc for function to system call mapping */
extern int capset(cap_user_header_t header, cap_user_data_t data);
extern int capget(cap_user_header_t header, const cap_user_data_t data);
// so for bbox, let's just repeat the declarations.
// This way, libcap needs not be installed in build environment.
#endif
#include <sys/prctl.h>
#include "libbb.h"

#ifndef PR_CAPBSET_READ
#define PR_CAPBSET_READ 23
#endif

#ifndef PR_SET_NO_NEW_PRIVS
#define PR_SET_NO_NEW_PRIVS 38
#endif

#ifndef PR_GET_NO_NEW_PRIVS
#define PR_GET_NO_NEW_PRIVS 39
#endif

#ifndef PR_CAP_AMBIENT
#define PR_CAP_AMBIENT 47
#define PR_CAP_AMBIENT_IS_SET 1
#define PR_CAP_AMBIENT_RAISE 2
#define PR_CAP_AMBIENT_LOWER 3
#endif

enum {
	IF_FEATURE_SETPRIV_DUMP(OPTBIT_DUMP,)
	IF_FEATURE_SETPRIV_CAPABILITIES(OPTBIT_INH,)
	IF_FEATURE_SETPRIV_CAPABILITIES(OPTBIT_AMB,)
	OPTBIT_NNP,

	IF_FEATURE_SETPRIV_DUMP(OPT_DUMP = (1 << OPTBIT_DUMP),)
	IF_FEATURE_SETPRIV_CAPABILITIES(OPT_INH  = (1 << OPTBIT_INH),)
	IF_FEATURE_SETPRIV_CAPABILITIES(OPT_AMB  = (1 << OPTBIT_AMB),)
	OPT_NNP  = (1 << OPTBIT_NNP),
};

#if ENABLE_FEATURE_SETPRIV_CAPABILITIES
struct caps {
	struct __user_cap_header_struct header;
	cap_user_data_t data;
	int u32s;
};

# if ENABLE_FEATURE_SETPRIV_CAPABILITY_NAMES
static const char *const capabilities[] = {
	"chown",
	"dac_override",
	"dac_read_search",
	"fowner",
	"fsetid",
	"kill",
	"setgid",
	"setuid",
	"setpcap",
	"linux_immutable",
	"net_bind_service",
	"net_broadcast",
	"net_admin",
	"net_raw",
	"ipc_lock",
	"ipc_owner",
	"sys_module",
	"sys_rawio",
	"sys_chroot",
	"sys_ptrace",
	"sys_pacct",
	"sys_admin",
	"sys_boot",
	"sys_nice",
	"sys_resource",
	"sys_time",
	"sys_tty_config",
	"mknod",
	"lease",
	"audit_write",
	"audit_control",
	"setfcap",
	"mac_override",
	"mac_admin",
	"syslog",
	"wake_alarm",
	"block_suspend",
	"audit_read",
};
# endif /* FEATURE_SETPRIV_CAPABILITY_NAMES */

static void getcaps(struct caps *caps)
{
	static const uint8_t versions[] = {
		_LINUX_CAPABILITY_U32S_3, /* = 2 (fits into byte) */
		_LINUX_CAPABILITY_U32S_2, /* = 2 */
		_LINUX_CAPABILITY_U32S_1, /* = 1 */
	};
	int i;

	caps->header.pid = 0;
	for (i = 0; i < ARRAY_SIZE(versions); i++) {
		caps->header.version = versions[i];
		if (capget(&caps->header, NULL) == 0)
			goto got_it;
	}
	bb_simple_perror_msg_and_die("capget");
 got_it:

	switch (caps->header.version) {
		case _LINUX_CAPABILITY_VERSION_1:
			caps->u32s = _LINUX_CAPABILITY_U32S_1;
			break;
		case _LINUX_CAPABILITY_VERSION_2:
			caps->u32s = _LINUX_CAPABILITY_U32S_2;
			break;
		case _LINUX_CAPABILITY_VERSION_3:
			caps->u32s = _LINUX_CAPABILITY_U32S_3;
			break;
		default:
			bb_error_msg_and_die("unsupported capability version");
	}

	caps->data = xmalloc(sizeof(caps->data[0]) * caps->u32s);
	if (capget(&caps->header, caps->data) < 0)
		bb_simple_perror_msg_and_die("capget");
}

static void parse_cap(unsigned long *index, const char *cap)
{
	unsigned long i;

	switch (cap[0]) {
	case '-':
		break;
	case '+':
		break;
	default:
		bb_error_msg_and_die("invalid capability '%s'", cap);
		break;
	}

	cap++;
	if ((sscanf(cap, "cap_%lu", &i)) == 1) {
		if (!cap_valid(i))
			bb_error_msg_and_die("unsupported capability '%s'", cap);
		*index = i;
		return;
	}

# if ENABLE_FEATURE_SETPRIV_CAPABILITY_NAMES
	for (i = 0; i < ARRAY_SIZE(capabilities); i++) {
		if (strcmp(capabilities[i], cap) != 0)
			continue;

		if (!cap_valid(i))
			bb_error_msg_and_die("unsupported capability '%s'", cap);
		*index = i;
		return;
	}
# endif

	bb_error_msg_and_die("unknown capability '%s'", cap);
}

static void set_inh_caps(char *capstring)
{
	struct caps caps;

	getcaps(&caps);

	capstring = strtok(capstring, ",");
	while (capstring) {
		unsigned long cap;

		parse_cap(&cap, capstring);
		if (CAP_TO_INDEX(cap) >= caps.u32s)
			bb_error_msg_and_die("invalid capability cap");

		if (capstring[0] == '+')
			caps.data[CAP_TO_INDEX(cap)].inheritable |= CAP_TO_MASK(cap);
		else
			caps.data[CAP_TO_INDEX(cap)].inheritable &= ~CAP_TO_MASK(cap);
		capstring = strtok(NULL, ",");
	}

	if ((capset(&caps.header, caps.data)) < 0)
		bb_perror_msg_and_die("capset");

	if (ENABLE_FEATURE_CLEAN_UP)
		free(caps.data);
}

static void set_ambient_caps(char *string)
{
	char *cap;

	cap = strtok(string, ",");
	while (cap) {
		unsigned long index;

		parse_cap(&index, cap);
		if (cap[0] == '+') {
			if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, index, 0, 0) < 0)
				bb_perror_msg("cap_ambient_raise");
		} else {
			if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_LOWER, index, 0, 0) < 0)
				bb_perror_msg("cap_ambient_lower");
		}
		cap = strtok(NULL, ",");
	}
}
#endif /* FEATURE_SETPRIV_CAPABILITIES */

#if ENABLE_FEATURE_SETPRIV_DUMP
# if ENABLE_FEATURE_SETPRIV_CAPABILITY_NAMES
static void printf_cap(const char *pfx, unsigned cap_no)
{
	if (cap_no < ARRAY_SIZE(capabilities)) {
		printf("%s%s", pfx, capabilities[cap_no]);
		return;
	}
	printf("%scap_%u", pfx, cap_no);
}
# else
#  define printf_cap(pfx, cap_no) printf("%scap_%u", (pfx), (cap_no))
# endif

static int dump(void)
{
	IF_FEATURE_SETPRIV_CAPABILITIES(struct caps caps;)
	const char *fmt;
	uid_t ruid, euid, suid;
	gid_t rgid, egid, sgid;
	gid_t *gids;
	int i, ngids, nnp;

	getresuid(&ruid, &euid, &suid); /* never fails in Linux */
	getresgid(&rgid, &egid, &sgid); /* never fails in Linux */
	ngids = 0;
	gids = bb_getgroups(&ngids, NULL); /* never fails in Linux */

	nnp = prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0);
	if (nnp < 0)
		bb_perror_msg_and_die("prctl: %s", "GET_NO_NEW_PRIVS");

	printf("uid: %u\n", (unsigned)ruid);
	printf("euid: %u\n", (unsigned)euid);
	printf("gid: %u\n", (unsigned)rgid);
	printf("egid: %u\n", (unsigned)egid);

	printf("Supplementary groups: ");
	if (ngids == 0) {
		printf("[none]");
	} else {
		fmt = ",%u" + 1;
		for (i = 0; i < ngids; i++) {
			printf(fmt, (unsigned)gids[i]);
			fmt = ",%u";
		}
	}
	printf("\nno_new_privs: %d\n", nnp);

# if ENABLE_FEATURE_SETPRIV_CAPABILITIES
	getcaps(&caps);
	printf("Inheritable capabilities: ");
	fmt = "";
	for (i = 0; cap_valid(i); i++) {
		unsigned idx = CAP_TO_INDEX(i);
		if (idx >= caps.u32s) {
			printf("\nindex: %u u32s: %u capability: %u\n", idx, caps.u32s, i);
			bb_error_msg_and_die("unsupported capability");
		}
		if (caps.data[idx].inheritable & CAP_TO_MASK(i)) {
			printf_cap(fmt, i);
			fmt = ",";
		}
	}
	if (!fmt[0])
		printf("[none]");

	printf("\nAmbient capabilities: ");
	fmt = "";
	for (i = 0; cap_valid(i); i++) {
		int ret = prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_IS_SET, (unsigned long) i, 0UL, 0UL);
		if (ret < 0)
			bb_perror_msg_and_die("prctl: %s", "CAP_AMBIENT_IS_SET");
		if (ret) {
			printf_cap(fmt, i);
			fmt = ",";
		}
	}
	if (i == 0)
		printf("[unsupported]");
	else if (!fmt[0])
		printf("[none]");

	printf("\nCapability bounding set: ");
	fmt = "";
	for (i = 0; cap_valid(i); i++) {
		int ret = prctl(PR_CAPBSET_READ, (unsigned long) i, 0UL, 0UL, 0UL);
		if (ret < 0)
			bb_perror_msg_and_die("prctl: %s", "CAPBSET_READ");
		if (ret) {
			printf_cap(fmt, i);
			fmt = ",";
		}
	}
	if (!fmt[0])
		printf("[none]");
	bb_putchar('\n');
# endif

	if (ENABLE_FEATURE_CLEAN_UP) {
		IF_FEATURE_SETPRIV_CAPABILITIES(free(caps.data);)
		free(gids);
	}
	return EXIT_SUCCESS;
}
#endif /* FEATURE_SETPRIV_DUMP */

int setpriv_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int setpriv_main(int argc UNUSED_PARAM, char **argv)
{
	static const char setpriv_longopts[] ALIGN1 =
		IF_FEATURE_SETPRIV_DUMP(
		"dump\0"         No_argument		"d"
		)
		"nnp\0"          No_argument		"\xff"
		"no-new-privs\0" No_argument		"\xff"
		IF_FEATURE_SETPRIV_CAPABILITIES(
		"inh-caps\0"     Required_argument	"\xfe"
		"ambient-caps\0" Required_argument	"\xfd"
		)
		;
	int opts;
	IF_FEATURE_SETPRIV_CAPABILITIES(char *inh_caps, *ambient_caps;)

	applet_long_options = setpriv_longopts;
	opts = getopt32(argv, "+"IF_FEATURE_SETPRIV_DUMP("d")
			IF_FEATURE_SETPRIV_CAPABILITIES("\xfe:\xfd:", &inh_caps, &ambient_caps));
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
			bb_perror_msg_and_die("prctl: %s", "SET_NO_NEW_PRIVS");
	}

#if ENABLE_FEATURE_SETPRIV_CAPABILITIES
	if (opts & OPT_INH)
		set_inh_caps(inh_caps);
	if (opts & OPT_AMB)
		set_ambient_caps(ambient_caps);
#endif

	if (!argv[0])
		bb_show_usage();
	BB_EXECVP_or_die(argv);
}
