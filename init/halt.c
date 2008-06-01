/* vi: set sw=4 ts=4: */
/*
 * Poweroff reboot and halt, oh my.
 *
 * Copyright 2006 by Rob Landley <rob@landley.net>
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include <sys/reboot.h>

#if ENABLE_FEATURE_WTMP
#include <sys/utsname.h>
#include <utmp.h>
#endif

int halt_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int halt_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	static const int magic[] = {
#ifdef RB_HALT_SYSTEM
RB_HALT_SYSTEM,
#elif defined RB_HALT
RB_HALT,
#endif
#ifdef RB_POWER_OFF
RB_POWER_OFF,
#elif defined RB_POWERDOWN
RB_POWERDOWN,
#endif
RB_AUTOBOOT
	};
	static const smallint signals[] = { SIGUSR1, SIGUSR2, SIGTERM };

	int delay = 0;
	int which, flags, rc = 1;
#if ENABLE_FEATURE_WTMP
	struct utmp utmp;
	struct utsname uts;
#endif

	/* Figure out which applet we're running */
	for (which = 0; "hpr"[which] != *applet_name; which++)
		continue;

	/* Parse and handle arguments */
	opt_complementary = "d+"; /* -d N */
	flags = getopt32(argv, "d:nfw", &delay);

	sleep(delay);

#if ENABLE_FEATURE_WTMP
	if (access(bb_path_wtmp_file, R_OK|W_OK) == -1) {
		close(creat(bb_path_wtmp_file, 0664));
	}
	memset(&utmp, 0, sizeof(utmp));
	utmp.ut_tv.tv_sec = time(NULL);
	safe_strncpy(utmp.ut_user, "shutdown", UT_NAMESIZE);
	utmp.ut_type = RUN_LVL;
	safe_strncpy(utmp.ut_id, "~~", sizeof(utmp.ut_id));
	safe_strncpy(utmp.ut_line, "~~", UT_LINESIZE);
	if (uname(&uts) == 0)
		safe_strncpy(utmp.ut_host, uts.release, sizeof(utmp.ut_host));
	updwtmp(bb_path_wtmp_file, &utmp);
#endif /* !ENABLE_FEATURE_WTMP */

	if (flags & 8) /* -w */
		return 0;
	if (!(flags & 2)) /* no -n */
		sync();

	/* Perform action. */
	if (ENABLE_INIT && !(flags & 4)) {
		if (ENABLE_FEATURE_INITRD) {
			pid_t *pidlist = find_pid_by_name("linuxrc");
			if (pidlist[0] > 0)
				rc = kill(pidlist[0], signals[which]);
			if (ENABLE_FEATURE_CLEAN_UP)
				free(pidlist);
		}
		if (rc)
			rc = kill(1, signals[which]);
	} else
		rc = reboot(magic[which]);

	if (rc)
		bb_error_msg("no");
	return rc;
}
