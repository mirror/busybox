/*
 * getenforce
 *
 * Based on libselinux 1.33.1
 * Port to BusyBox  Hiroshi Shinji <shiroshi@my.email.ne.jp>
 *
 */

#include "busybox.h"

int getenforce_main(int argc, char **argv);
int getenforce_main(int argc, char **argv)
{
	int rc;

	rc = is_selinux_enabled();
	if (rc < 0)
		bb_error_msg_and_die("is_selinux_enabled() failed");

	if (rc == 1) {
		rc = security_getenforce();
		if (rc < 0)
			bb_error_msg_and_die("getenforce() failed");

		if (rc)
			puts("Enforcing");
		else
			puts("Permissive");
	} else {
		puts("Disabled");
	}

	return 0;
}
