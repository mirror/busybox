/*
 * selinuxenabled
 *
 * Based on libselinux 1.33.1
 * Port to BusyBox  Hiroshi Shinji <shiroshi@my.email.ne.jp>
 *
 */
#include "libbb.h"

int selinuxenabled_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int selinuxenabled_main(int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED)
{
	return !is_selinux_enabled();
}
