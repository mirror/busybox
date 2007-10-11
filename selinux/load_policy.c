/*
 * load_policy
 * Author: Yuichi Nakamura <ynakam@hitachisoft.jp>
 */
#include "libbb.h"

int load_policy_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int load_policy_main(int argc, char **argv)
{
	int rc;

	if (argc != 1) {
		bb_show_usage();
	}

	rc = selinux_mkload_policy(1);
	if (rc < 0) {
		bb_perror_msg_and_die("can't load policy");
	}

	return 0;
}
