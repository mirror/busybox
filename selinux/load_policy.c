/*
 * load_policy
 * This implementation is based on old load_policy to be small.
 * Author: Yuichi Nakamura <ynakam@hitachisoft.jp>
 */
#include "libbb.h"

int load_policy_main(int argc, char **argv);
int load_policy_main(int argc, char **argv)
{
	int fd;
	struct stat st;
	void *data;
	if (argc != 2) {
		bb_show_usage();
	}

	fd = xopen(argv[1], O_RDONLY);
	if (fstat(fd, &st) < 0) {
		bb_perror_msg_and_die("can't fstat");
	}
	data = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		bb_perror_msg_and_die("can't mmap");
	}
	if (security_load_policy(data, st.st_size) < 0) {
		bb_perror_msg_and_die("can't load policy");
	}

	return 0;
}
