/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 1999-2003 by Erik Andersen <andersen@codepoet.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "busybox.h"
#include "libbb.h"

#ifdef L_full_version
	const char * const bb_msg_full_version = BB_BANNER " multi-call binary";
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
#ifdef L_name_longer_than_foo
	const char * const bb_msg_name_longer_than_foo = "Names longer than %d chars not supported.";
#endif
#ifdef L_unknown
	const char * const bb_msg_unknown = "(unknown)";
#endif
#ifdef L_can_not_create_raw_socket
	const char * const bb_msg_can_not_create_raw_socket = "can`t create raw socket";
#endif
#ifdef L_perm_denied_are_you_root
	const char * const bb_msg_perm_denied_are_you_root = "permission denied. (are you root?)";
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

