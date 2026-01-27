/* vi: set sw=4 ts=4: */
/*
 * PiPlayInit — minimal init for PiPlayOS
 *
 * Copyright (C)
 * PiPlay Project contributors
 *
 * PiPlayInit is the authoritative early userspace init for PiPlayOS.
 * It replaces legacy BusyBox init behavior and exists solely to:
 *
 *   • establish early userspace
 *   • mount essential virtual filesystems
 *   • optionally present a splash
 *   • hand off control to the next init stage
 *
 * PiPlayInit does NOT:
 *   • manage runlevels
 *   • supervise long-running services
 *   • own system lifecycle beyond early boot
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

#define DEBUG_INIT 0

#include "libbb.h"
#include <syslog.h>
#include <sys/mount.h>
#include <unistd.h>
#include <signal.h>
#include <sys/reboot.h>

static void sleep_forever(void) NORETURN;
static void sleep_forever(void)
{
	for (;;)
		sleep(60);
}

int init_main(int argc UNUSED_PARAM, char **argv)
{
	/* PiPlayInit must be PID 1 */
	if (getpid() != 1) {
		bb_simple_error_msg_and_die("PiPlayInit must run as PID 1");
	}

	/* Basic signal safety */
	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

	/* Console setup */
	bb_sanitize_stdio();
	putenv((char *)"PATH=/sbin:/bin:/usr/sbin:/usr/bin");
	putenv((char *)"USER=root");
	putenv((char *)"HOME=/");

	/* Mount essential virtual filesystems */
	mount("proc", "/proc", "proc", 0, NULL);
	mount("sysfs", "/sys", "sysfs", 0, NULL);
	mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);

	/* Optional: early splash could be started here */
	/* (left intentionally minimal) */

	bb_info_msg("PiPlayInit: early userspace ready");

	/*
	 * Canonical handoff:
	 * PiPlayInit relinquishes all authority here.
	 * systemd becomes PID 1 controller of the system.
	 */
	execv("/sbin/init", (char *const[]){ "init", NULL });

	/* If exec fails, do not exit PID 1 */
	bb_perror_msg("PiPlayInit: failed to exec /sbin/init");
	sleep_forever();
}