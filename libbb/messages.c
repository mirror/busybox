/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 */

#include "libbb.h"

#ifdef L_full_version
#ifndef BB_EXTRA_VERSION
#define LIBBB_BANNER "BusyBox's library v" BB_VER " (" BB_BT ")"
#else
#define LIBBB_BANNER "BusyBox's library v" BB_VER " (" BB_EXTRA_VERSION ")"
#endif
	const char * const libbb_msg_full_version = LIBBB_BANNER;
#endif
#ifdef L_memory_exhausted
	const char * const bb_msg_memory_exhausted = "memory exhausted";
#endif
#ifdef L_invalid_date
	const char * const bb_msg_invalid_date = "invalid date `%s'";
#endif
#ifdef L_io_error
	const char * const bb_msg_io_error = "%s: input/output error -- %m";
#endif
#ifdef L_write_error
	const char * const bb_msg_write_error = "Write Error";
#endif
#ifdef L_read_error
	const char * const bb_msg_read_error = "Read Error";
#endif
#ifdef L_name_longer_than_foo
	const char * const bb_msg_name_longer_than_foo = "Names longer than %d chars not supported.";
#endif
#ifdef L_unknown
	const char * const bb_msg_unknown = "(unknown)";
#endif
#ifdef L_can_not_create_raw_socket
	const char * const bb_msg_can_not_create_raw_socket = "can't create raw socket";
#endif
#ifdef L_perm_denied_are_you_root
	const char * const bb_msg_perm_denied_are_you_root = "permission denied. (are you root?)";
#endif
#ifdef L_msg_requires_arg
	const char * const bb_msg_requires_arg = "%s requires an argument";
#endif
#ifdef L_msg_invalid_arg
	const char * const bb_msg_invalid_arg = "invalid argument `%s' to `%s'";
#endif
#ifdef L_msg_standard_input
	const char * const bb_msg_standard_input = "standard input";
#endif
#ifdef L_msg_standard_output
	const char * const bb_msg_standard_output = "standard output";
#endif

#ifdef L_passwd_file
#define PASSWD_FILE        "/etc/passwd"
const char * const bb_path_passwd_file = PASSWD_FILE;
#endif

#ifdef L_shadow_file
#define SHADOW_FILE        "/etc/shadow"
const char * const bb_path_shadow_file = SHADOW_FILE;
#endif

#ifdef L_group_file
#define GROUP_FILE         "/etc/group"
const char * const bb_path_group_file = GROUP_FILE;
#endif

#ifdef L_gshadow_file
#define GSHADOW_FILE       "/etc/gshadow"
const char * const bb_path_gshadow_file = GSHADOW_FILE;
#endif

#ifdef L_nologin_file
#define NOLOGIN_FILE       "/etc/nologin"
const char * const bb_path_nologin_file = NOLOGIN_FILE;
#endif

#ifdef L_securetty_file
#define SECURETTY_FILE     "/etc/securetty"
const char * const bb_path_securetty_file = SECURETTY_FILE;
#endif

#ifdef L_motd_file
#define MOTD_FILE          "/etc/motd"
const char * const bb_path_motd_file = MOTD_FILE;
#endif

#ifdef L_shell_file
const char * const bb_default_login_shell = LIBBB_DEFAULT_LOGIN_SHELL;
#endif

#ifdef L_bb_dev_null
const char * const bb_dev_null = "/dev/null";
#endif

#ifdef L_bb_path_wtmp_file
#include <utmp.h>
/* This is usually something like "/var/adm/wtmp" or "/var/log/wtmp" */
const char * const bb_path_wtmp_file =
#if defined _PATH_WTMP
_PATH_WTMP;
#elif defined WTMP_FILE
WTMP_FILE;
#else
# error unknown path to wtmp file
#endif
#endif


#ifdef L_bb_common_bufsiz1
char bb_common_bufsiz1[BUFSIZ+1];
#endif
