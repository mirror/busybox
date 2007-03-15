/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

#ifndef BB_EXTRA_VERSION
#define BANNER "BusyBox v" BB_VER " (" BB_BT ")"
#else
#define BANNER "BusyBox v" BB_VER " (" BB_EXTRA_VERSION ")"
#endif
const char BB_BANNER[] = BANNER;
const char bb_msg_full_version[] = BANNER " multi-call binary";

const char bb_msg_memory_exhausted[] = "memory exhausted";
const char bb_msg_invalid_date[] = "invalid date '%s'";
const char bb_msg_write_error[] = "write error";
const char bb_msg_read_error[] = "read error";
const char bb_msg_unknown[] = "(unknown)";
const char bb_msg_can_not_create_raw_socket[] = "can't create raw socket";
const char bb_msg_perm_denied_are_you_root[] = "permission denied. (are you root?)";
const char bb_msg_requires_arg[] = "%s requires an argument";
const char bb_msg_invalid_arg[] = "invalid argument '%s' to '%s'";
const char bb_msg_standard_input[] = "standard input";
const char bb_msg_standard_output[] = "standard output";

const char bb_str_default[] = "default";
const char bb_hexdigits_upcase[] = "0123456789ABCDEF";

const char bb_path_passwd_file[] = "/etc/passwd";
const char bb_path_shadow_file[] = "/etc/shadow";
const char bb_path_group_file[] = "/etc/group";
const char bb_path_gshadow_file[] = "/etc/gshadow";
const char bb_path_nologin_file[] = "/etc/nologin";
const char bb_path_securetty_file[] = "/etc/securetty";
const char bb_path_motd_file[] = "/etc/motd";
const char bb_default_login_shell[] = LIBBB_DEFAULT_LOGIN_SHELL;
const char bb_dev_null[] = "/dev/null";

const int const_int_0;
const int const_int_1 = 1;

#include <utmp.h>
/* This is usually something like "/var/adm/wtmp" or "/var/log/wtmp" */
const char bb_path_wtmp_file[] =
#if defined _PATH_WTMP
_PATH_WTMP;
#elif defined WTMP_FILE
WTMP_FILE;
#else
# error unknown path to wtmp file
#endif

char bb_common_bufsiz1[BUFSIZ+1];

struct globals;
/* Make it reside in R/W memory: */
struct globals *const ptr_to_globals __attribute__ ((section (".data")));
