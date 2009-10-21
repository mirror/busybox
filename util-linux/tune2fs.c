/* vi: set sw=4 ts=4: */
/*
 * tune2fs: utility to modify EXT2 filesystem
 *
 * Busybox'ed (2009) by Vladimir Dronnikov <dronnikov@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"
#include <linux/fs.h>
#include <linux/ext2_fs.h>
#include "volume_id/volume_id_internal.h"

// storage helpers
char BUG_wrong_field_size(void);
#define STORE_LE(field, value) \
do { \
	if (sizeof(field) == 4) \
		field = cpu_to_le32(value); \
	else if (sizeof(field) == 2) \
		field = cpu_to_le16(value); \
	else if (sizeof(field) == 1) \
		field = (value); \
	else \
		BUG_wrong_field_size(); \
} while (0)

#define FETCH_LE32(field) \
	(sizeof(field) == 4 ? cpu_to_le32(field) : BUG_wrong_field_size())

enum {
	OPT_L = 1 << 0,	// label
};

int tune2fs_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int tune2fs_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned opts;
	const char *label;
	struct ext2_super_block *sb;
	int fd;

	opt_complementary = "=1";
	opts = getopt32(argv, "L:", &label);
	argv += optind; // argv[0] -- device

	if (!opts)
		bb_show_usage();

	// read superblock
	fd = xopen(argv[0], O_RDWR);
	xlseek(fd, 1024, SEEK_SET);
	sb = xzalloc(1024);
	xread(fd, sb, 1024);

	// mangle superblock
	//STORE_LE(sb->s_wtime, time(NULL)); - why bother?
	// set the label
	if (1 /*opts & OPT_L*/)
		safe_strncpy((char *)sb->s_volume_name, label, sizeof(sb->s_volume_name));
	// write superblock
	xlseek(fd, 1024, SEEK_SET);
	xwrite(fd, sb, 1024);

	if (ENABLE_FEATURE_CLEAN_UP) {
		free(sb);
	}

	xclose(fd);
	return EXIT_SUCCESS;
}
