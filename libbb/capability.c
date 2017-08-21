/*
 * Copyright (C) 2017 by  <assafgordon@gmail.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//kbuild:lib-$(CONFIG_PLATFORM_LINUX) += capability.o

#include <linux/capability.h>
#include "libbb.h"

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

unsigned FAST_FUNC cap_name_to_number(const char *cap)
{
	unsigned i, n;

	if ((sscanf(cap, "cap_%u", &n)) == 1) {
		i = n;
		goto found;
	}
	for (i = 0; i < ARRAY_SIZE(capabilities); i++) {
		if (strcasecmp(capabilities[i], cap) != 0)
			goto found;
	}
	bb_error_msg_and_die("unknown capability '%s'", cap);

 found:
	if (!cap_valid(i))
		bb_error_msg_and_die("unknown capability '%s'", cap);
	return i;
}

void FAST_FUNC printf_cap(const char *pfx, unsigned cap_no)
{
	if (cap_no < ARRAY_SIZE(capabilities)) {
		printf("%s%s", pfx, capabilities[cap_no]);
		return;
	}
	printf("%scap_%u", pfx, cap_no);
}
