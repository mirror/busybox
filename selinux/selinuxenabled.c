/*
 * selinuxenabled
 *
 * Based on libselinux 1.33.1
 * Port to BusyBox  Hiroshi Shinji <shiroshi@my.email.ne.jp>
 *
 */
#include "busybox.h"

int selinuxenabled_main(int argc, char **argv);
int selinuxenabled_main(int argc, char **argv)
{
	return !is_selinux_enabled();
}
