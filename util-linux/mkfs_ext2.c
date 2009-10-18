/* vi: set sw=4 ts=4: */
/*
 * mkfs_ext2: utility to create EXT2 filesystem
 * inspired by genext2fs
 *
 * Busybox'ed (2009) by Vladimir Dronnikov <dronnikov@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"
#include <linux/fs.h>
#include <linux/ext2_fs.h>
#include "volume_id/volume_id_internal.h"

#define	ENABLE_FEATURE_MKFS_EXT2_RESERVED_GDT 0
#define	ENABLE_FEATURE_MKFS_EXT2_DIR_INDEX 1

// from e2fsprogs
#define s_reserved_gdt_blocks s_padding1
#define s_mkfs_time           s_reserved[0]
#define s_flags               s_reserved[22]
#define EXT2_HASH_HALF_MD4     1
#define EXT2_FLAGS_SIGNED_HASH 0x0001

// whiteout: for writable overlays
//#define LINUX_S_IFWHT                  0160000
//#define EXT2_FEATURE_INCOMPAT_WHITEOUT 0x0020

// storage helper
void BUG_unsupported_field_size(void);
#define STORE_LE(field, value) \
do { \
	if (sizeof(field) == 4) \
		field = cpu_to_le32(value); \
	else if (sizeof(field) == 2) \
		field = cpu_to_le16(value); \
	else if (sizeof(field) == 1) \
		field = (value); \
	else \
		BUG_unsupported_field_size(); \
} while (0)

// All fields are little-endian
struct ext2_dir {
	uint32_t inode1;
	uint16_t rec_len1;
	uint8_t  name_len1;
	uint8_t  file_type1;
	char     name1[4];
	uint32_t inode2;
	uint16_t rec_len2;
	uint8_t  name_len2;
	uint8_t  file_type2;
	char     name2[4];
	uint32_t inode3;
	uint16_t rec_len3;
	uint8_t  name_len3;
	uint8_t  file_type3;
	char     name3[12];
};

static unsigned int_log2(unsigned arg)
{
	unsigned r = 0;
	while ((arg >>= 1) != 0)
		r++;
	return r;
}

// taken from mkfs_minix.c. libbb candidate?
static unsigned div_roundup(uint32_t size, uint32_t n)
{
	return (size + n-1) / n;
}

static void allocate(uint8_t *bitmap, uint32_t blocksize, uint32_t start, uint32_t end)
{
	uint32_t i;
	memset(bitmap, 0, blocksize);
	i = start / 8;
	memset(bitmap, 0xFF, i);
	bitmap[i] = 0xFF >> (8-(start&7));
//bb_info_msg("ALLOC: [%u][%u][%u]: [%u]:=[%x]", blocksize, start, end, blocksize - end/8 - 1, (uint8_t)(0xFF << (8-(end&7))));
	i = end / 8;
	bitmap[blocksize - i - 1] = 0xFF << (8 - (end & 7));
	memset(bitmap + blocksize - i, 0xFF, i); // N.B. no overflow here!
}

static uint32_t has_super(uint32_t x)
{
	// 0, 1 and powers of 3, 5, 7 up to 2^32 limit
	static const uint32_t supers[] = {
		0, 1, 3, 5, 7, 9, 25, 27, 49, 81, 125, 243, 343, 625, 729,
		2187, 2401, 3125, 6561, 15625, 16807, 19683, 59049, 78125,
		117649, 177147, 390625, 531441, 823543, 1594323, 1953125,
		4782969, 5764801, 9765625, 14348907, 40353607, 43046721,
		48828125, 129140163, 244140625, 282475249, 387420489,
		1162261467, 1220703125, 1977326743, 3486784401/* >2^31 */,
	};
	const uint32_t *sp = supers + ARRAY_SIZE(supers);
	while (1) {
		sp--;
		if (x == *sp)
			return 1;
		if (x > *sp)
			return 0;
	}
}

/* Standard mke2fs 1.41.9:
 * Usage: mke2fs [-c|-l filename] [-b block-size] [-f fragment-size]
 * 	[-i bytes-per-inode] [-I inode-size] [-J journal-options]
 * 	[-G meta group size] [-N number-of-inodes]
 * 	[-m reserved-blocks-percentage] [-o creator-os]
 * 	[-g blocks-per-group] [-L volume-label] [-M last-mounted-directory]
 * 	[-O feature[,...]] [-r fs-revision] [-E extended-option[,...]]
 * 	[-T fs-type] [-U UUID] [-jnqvFSV] device [blocks-count]
*/
// N.B. not commented below options are taken and silently ignored
enum {
	OPT_c = 1 << 0,
	OPT_l = 1 << 1,
	OPT_b = 1 << 2,		// block size, in bytes
	OPT_f = 1 << 3,
	OPT_i = 1 << 4,		// bytes per inode
	OPT_I = 1 << 5,
	OPT_J = 1 << 6,
	OPT_G = 1 << 7,
	OPT_N = 1 << 8,
	OPT_m = 1 << 9,		// percentage of blocks reserved for superuser
	OPT_o = 1 << 10,
	OPT_g = 1 << 11,
	OPT_L = 1 << 12,	// label
	OPT_M = 1 << 13,
	OPT_O = 1 << 14,
	OPT_r = 1 << 15,
	OPT_E = 1 << 16,
	OPT_T = 1 << 17,
	OPT_U = 1 << 18,
	OPT_j = 1 << 19,
	OPT_n = 1 << 20,	// dry run: do not write anything
	OPT_q = 1 << 21,
	OPT_v = 1 << 22,
	OPT_F = 1 << 23,
	OPT_S = 1 << 24,
	//OPT_V = 1 << 25,	// -V version. bbox applets don't support that
};

#define fd 3	/* predefined output descriptor */

static void PUT(uint64_t off, void *buf, uint32_t size)
{
//	bb_info_msg("PUT[%llu]:[%u]", off, size);
	xlseek(fd, off, SEEK_SET);
	xwrite(fd, buf, size);
}

int mkfs_ext2_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int mkfs_ext2_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned i, pos, n;
	unsigned bs, blocksize, blocksize_log2;
	unsigned nreserved = 5;
	uint32_t nblocks;
	uint32_t ngroups;
	unsigned bytes_per_inode;
	uint32_t nblocks_per_group;
	uint32_t first_data_block;
	uint32_t ninodes;
	uint32_t ninodes_per_group;
	uint32_t gdtsz, rgdtsz, itsz;
	time_t timestamp;
	unsigned opts;
	const char *label;
	struct stat st;
	struct ext2_super_block *sb; // superblock
	struct ext2_group_desc *gd; // group descriptors
	struct ext2_inode *inode;
	struct ext2_dir *dir;
	uint8_t *buf;

	bs = EXT2_MIN_BLOCK_SIZE;
	opt_complementary = "-1:b+:m+:i+";
	opts = getopt32(argv, "cl:b:f:i:I:J:G:N:m:o:g:L:M:O:r:E:T:U:jnqvFS",
		NULL, &bs, NULL, &bytes_per_inode, NULL, NULL, NULL, NULL,
		&nreserved, NULL, NULL, &label, NULL, NULL, NULL, NULL, NULL, NULL);
	argv += optind; // argv[0] -- device

	// block size minimax, block size is a multiple of minimum
	blocksize = bs;
	if (blocksize < EXT2_MIN_BLOCK_SIZE
	 || blocksize > EXT2_MAX_BLOCK_SIZE
	 || (blocksize & (blocksize - 1)) // not power of 2
	) {
		bb_error_msg_and_die("-%c is bad", 'b');
	}

	// reserved blocks count
	if (nreserved > 50)
		bb_error_msg_and_die("-%c is bad", 'm');

	// check the device is a block device
	xstat(argv[0], &st);
	if (!S_ISBLK(st.st_mode) && !(opts & OPT_F))
		bb_error_msg_and_die("not a block device");

	// check if it is mounted
	// N.B. what if we format a file? find_mount_point will return false negative since
	// it is loop block device which mounted!
	if (find_mount_point(argv[0], 0))
		bb_error_msg_and_die("can't format mounted filesystem");

	// TODO: 5?/5 WE MUST NOT DEPEND ON WHETHER DEVICE IS /dev/zero 'ed OR NOT
	// TODO: 3/5 refuse if mounted
	// TODO: 4/5 compat options
	// TODO: 1/5 sanity checks
	// TODO: 0/5 more verbose error messages
	// TODO: 0/5 info printing
	// TODO: 2/5 bigendianness! Spot where it comes to play! sb->, gd->
	// TODO: 2/5 reserved GDT: how to mark but not allocate?
	// TODO: 3/5 dir_index?

	// fill the superblock
	sb = xzalloc(blocksize);
	sb->s_rev_level = 1; // revision 1 filesystem
	sb->s_magic = EXT2_SUPER_MAGIC;
	sb->s_inode_size = sizeof(*inode);
	sb->s_first_ino = EXT2_GOOD_OLD_FIRST_INO;
	blocksize_log2 = int_log2(blocksize);
	sb->s_log_block_size = sb->s_log_frag_size = blocksize_log2 - EXT2_MIN_BLOCK_LOG_SIZE;
	// first 1024 bytes of the device are for boot record. If block size is 1024 bytes, then
	// the first block available for data is 1, otherwise 0
	first_data_block = sb->s_first_data_block = (EXT2_MIN_BLOCK_SIZE == blocksize);
	// block and inode bitmaps occupy no more than one block, so maximum number of blocks is
	// number of bits in one block, i.e. 8*blocksize
	nblocks_per_group = sb->s_blocks_per_group = sb->s_frags_per_group = sb->s_inodes_per_group = 8 * blocksize;
	timestamp = time(NULL);
	sb->s_mkfs_time = sb->s_wtime = sb->s_lastcheck = timestamp;
	sb->s_state = 1;
	sb->s_creator_os = EXT2_OS_LINUX;
	sb->s_max_mnt_count = EXT2_DFL_MAX_MNT_COUNT;
	sb->s_checkinterval = 24*60*60 * 180; // 180 days
	sb->s_errors = EXT2_ERRORS_DEFAULT;
	sb->s_feature_compat = EXT2_FEATURE_COMPAT_SUPP
		| (EXT2_FEATURE_COMPAT_RESIZE_INO * ENABLE_FEATURE_MKFS_EXT2_RESERVED_GDT)
		| (EXT2_FEATURE_COMPAT_DIR_INDEX * ENABLE_FEATURE_MKFS_EXT2_DIR_INDEX)
		;
	// e2fsprogs-1.41.9 doesn't like EXT2_FEATURE_INCOMPAT_WHITEOUT
	sb->s_feature_incompat = EXT2_FEATURE_INCOMPAT_FILETYPE;// | EXT2_FEATURE_INCOMPAT_WHITEOUT;
	sb->s_feature_ro_compat = EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER;
	sb->s_flags = EXT2_FLAGS_SIGNED_HASH * ENABLE_FEATURE_MKFS_EXT2_DIR_INDEX;
	generate_uuid(sb->s_uuid);
	if (ENABLE_FEATURE_MKFS_EXT2_DIR_INDEX) {
		sb->s_def_hash_version = EXT2_HASH_HALF_MD4;
		generate_uuid((uint8_t *)sb->s_hash_seed);
	}
	/*
	 * From e2fsprogs: add "jitter" to the superblock's check interval so that we
	 * don't check all the filesystems at the same time.  We use a
	 * kludgy hack of using the UUID to derive a random jitter value.
	 */
	sb->s_max_mnt_count += sb->s_uuid[ARRAY_SIZE(sb->s_uuid)-1] % EXT2_DFL_MAX_MNT_COUNT;

	// open the device, get number of blocks
	xmove_fd(xopen3(argv[0], O_WRONLY | O_CREAT, 0666), fd);
	if (argv[1]) {
		nblocks = xatou(argv[1]);
	} else {
		nblocks = (uoff_t)xlseek(fd, 0, SEEK_END) >> blocksize_log2;
	}
	sb->s_blocks_count = nblocks;

	// nblocks is the total number of blocks in the filesystem
	if (nblocks < 8)
		bb_error_msg_and_die("nblocks");
	// reserve blocks for superuser
	sb->s_r_blocks_count = ((uint64_t) nblocks * nreserved) / 100;

	// N.B. a block group can have no more than nblocks_per_group blocks
	ngroups = div_roundup(nblocks - first_data_block, nblocks_per_group);
	if (0 == ngroups)
		bb_error_msg_and_die("ngroups");
	gdtsz = div_roundup(ngroups, EXT2_DESC_PER_BLOCK(sb));
	/*
	 * From e2fsprogs: Calculate the number of GDT blocks to reserve for online
	 * filesystem growth.
	 * The absolute maximum number of GDT blocks we can reserve is determined by
	 * the number of block pointers that can fit into a single block.
	 */
	/* We set it at 1024x the current filesystem size, or
	 * the upper block count limit (2^32), whichever is lower.
	 */
#if ENABLE_FEATURE_MKFS_EXT2_RESERVED_GDT
	rgdtsz = 0xFFFFFFFF; // maximum block number
	if (nblocks < rgdtsz / 1024)
		rgdtsz = nblocks * 1024;
	rgdtsz = div_roundup(rgdtsz - first_data_block, nblocks_per_group);
	rgdtsz = div_roundup(rgdtsz, EXT2_DESC_PER_BLOCK(sb)) - gdtsz;
	if (rgdtsz > EXT2_ADDR_PER_BLOCK(sb))
		rgdtsz = EXT2_ADDR_PER_BLOCK(sb);
	sb->s_reserved_gdt_blocks = rgdtsz;
	//bb_info_msg("RSRVD[%u]", n);
#else
	rgdtsz = 0;
#endif

	// ninodes is the total number of inodes (files) in the file system
	if (!(opts & OPT_i)) {
		bytes_per_inode = 16384;
		if (nblocks < 512*1024)
			bytes_per_inode = 4096;
		if (nblocks < 3*1024)
			bytes_per_inode = 8192;
	}
	ninodes = nblocks / (bytes_per_inode >> blocksize_log2);
	if (ninodes < EXT2_GOOD_OLD_FIRST_INO+1)
		ninodes = EXT2_GOOD_OLD_FIRST_INO+1;
	ninodes_per_group = div_roundup(ninodes, ngroups);
	if (ninodes_per_group < 16)
		ninodes_per_group = 16; // minimum number because the first EXT2_GOOD_OLD_FIRST_INO-1 are reserved
	// N.B. a block group can have no more than 8*blocksize = sb->s_inodes_per_group inodes
	if (ninodes_per_group > sb->s_inodes_per_group)
		ninodes_per_group = sb->s_inodes_per_group;
	// adjust inodes per group so they completely fill the inode table blocks in the descriptor
	ninodes_per_group = (div_roundup(ninodes_per_group * EXT2_INODE_SIZE(sb), blocksize) << blocksize_log2) / EXT2_INODE_SIZE(sb);
	// make sure the number of inodes per group is a multiple of 8
	ninodes_per_group &= ~7;
	sb->s_inodes_per_group = ninodes_per_group;// = div_roundup(ninodes_per_group * sb->s_inode_size, blocksize);
	// total ninodes
	ninodes = sb->s_inodes_count = ninodes_per_group * ngroups;

	itsz = ninodes_per_group * sb->s_inode_size / blocksize;
	sb->s_free_inodes_count = sb->s_inodes_count - EXT2_GOOD_OLD_FIRST_INO;

	// write the label, if any
	if (opts & OPT_L)
		safe_strncpy((char *)sb->s_volume_name, label, sizeof(sb->s_volume_name));

#if 1
/*	if (fs_param.s_blocks_count != s->s_blocks_count)
		fprintf(stderr, _("warning: %u blocks unused.\n\n"),
		       fs_param.s_blocks_count - s->s_blocks_count);
*/

	printf(
		"Filesystem label=%s\n"
		"OS type: Linux\n"
		"Block size=%u (log=%u)\n"
		"Fragment size=%u (log=%u)\n"
		"%u inodes, %u blocks\n"
		"%u blocks (%u%%) reserved for the super user\n"
		"First data block=%u\n"
//		"Maximum filesystem blocks=%lu\n"
		"%u block groups\n"
		"%u blocks per group, %u fragments per group\n"
		"%u inodes per group"
		, (char *)sb->s_volume_name
		, blocksize, sb->s_log_block_size
		, blocksize, sb->s_log_block_size
		, sb->s_inodes_count, sb->s_blocks_count
		, sb->s_r_blocks_count, nreserved
		, first_data_block
//		, (rgdtsz + gdtsz) * EXT2_DESC_PER_BLOCK(sb) * nblocks_per_group
		, ngroups
		, nblocks_per_group, nblocks_per_group
		, ninodes_per_group
	);
	{
		const char *fmt = "\nSuperblock backups stored on blocks: %u";
		pos = first_data_block;
		for (i = 1; i < ngroups; i++) {
			pos += nblocks_per_group;
			if (has_super(i)) {
				printf(fmt, (unsigned)pos);
				fmt = ", %u";
			}
		}
	}
	bb_putchar('\n');
#endif

	if (opts & OPT_n)
		return EXIT_SUCCESS;

	// fill group descriptors
	gd = xzalloc((gdtsz + rgdtsz) * blocksize);
	sb->s_free_blocks_count = 0;
	for (i = 0, pos = first_data_block, n = nblocks;
		i < ngroups;
		i++, pos += nblocks_per_group, n -= nblocks_per_group
	) {
		uint32_t overhead = pos + (has_super(i) ? (1/*sb*/ + gdtsz + rgdtsz) : 0);
		gd[i].bg_block_bitmap = overhead + 0;
		gd[i].bg_inode_bitmap = overhead + 1;
		gd[i].bg_inode_table  = overhead + 2;
		overhead = overhead - pos + 1/*bbmp*/ + 1/*ibmp*/ + itsz;
		gd[i].bg_free_inodes_count = ninodes_per_group;
		//gd[i].bg_used_dirs_count = 0;
		// N.B. both root and lost+found dirs are within the first block group, thus +2
		if (0 == i) {
			overhead += 2;
			gd[i].bg_used_dirs_count = 2;
			gd[i].bg_free_inodes_count -= EXT2_GOOD_OLD_FIRST_INO;
		}
		// N.B. the following is pure heuristics!
		// Likely to cope with 1024-byte blocks, when first block is for boot sectors
		if (ngroups-1 == i) {
			overhead += first_data_block;
		}
		gd[i].bg_free_blocks_count = (n < nblocks_per_group ? n : nblocks_per_group) - overhead;
		sb->s_free_blocks_count += gd[i].bg_free_blocks_count;
	}
	STORE_LE(sb->s_free_blocks_count, sb->s_free_blocks_count);

	// dump filesystem skeleton structures
//	printf("Writing superblocks and filesystem accounting information: ");
	buf = xmalloc(blocksize);
	for (i = 0, pos = first_data_block; i < ngroups; i++, pos += nblocks_per_group) {
		uint32_t overhead = has_super(i) ? (1/*sb*/ + gdtsz + rgdtsz) : 0;
		uint32_t start;// = has_super(i) ? (1/*sb*/ + gdtsz + rgdtsz) : 0;
		uint32_t end;

		// dump superblock and group descriptors and their backups
		if (overhead) { // N.B. in fact, we want (has_super(i)) condition, but it is equal to (overhead != 0) and is cheaper
			// N.B. 1024 byte blocks are special
			PUT(((uint64_t)pos << blocksize_log2) + ((0 == i && 0 == first_data_block) ? 1024 : 0), sb, 1024);//blocksize);
			PUT(((uint64_t)pos << blocksize_log2) + blocksize, gd, (gdtsz + rgdtsz) * blocksize);
		}

		start = overhead + 1/*bbmp*/ + 1/*ibmp*/ + itsz;
		if (i == 0)
			start += 2; // for "/" and "/lost+found"
		end = nblocks_per_group - (start + gd[i].bg_free_blocks_count);

		// mark preallocated blocks as allocated
		allocate(buf, blocksize, start, end);
		// dump block bitmap
		PUT((uint64_t)(pos + overhead) * blocksize, buf, blocksize);

		// mark preallocated inodes as allocated
		allocate(buf, blocksize,
			ninodes_per_group - gd[i].bg_free_inodes_count,
			8 * blocksize - ninodes_per_group
		);
		// dump inode bitmap
		//PUT((uint64_t)(pos + overhead + 1) * blocksize, buf, blocksize);
		//but it's right after block bitmap, so we can just:
		xwrite(fd, buf, blocksize);
	}

	// zero boot sectors
	memset(buf, 0, blocksize);
	PUT(0, buf, 1024); // N.B. 1024 <= blocksize
	// zero inode tables
	for (i = 0; i < ngroups; ++i)
		for (n = 0; n < itsz; ++n)
			PUT((uint64_t)(gd[i].bg_inode_table + n) * blocksize, buf, blocksize);

	// prepare directory inode
	inode = (struct ext2_inode *)buf;
	STORE_LE(inode->i_mode, S_IFDIR | S_IRWXU | S_IRGRP | S_IROTH | S_IXGRP | S_IXOTH);
	STORE_LE(inode->i_mtime, timestamp);
	STORE_LE(inode->i_atime, timestamp);
	STORE_LE(inode->i_ctime, timestamp);
	STORE_LE(inode->i_size, blocksize);
	// N.B. inode->i_blocks stores the number of 512 byte data blocks. Why on Earth?!
	STORE_LE(inode->i_blocks, blocksize / 512);

	// dump root dir inode
	STORE_LE(inode->i_links_count, 3); // "/.", "/..", "/lost+found/.." point to this inode
	STORE_LE(inode->i_block[0], gd[0].bg_inode_table + itsz);
	PUT(((uint64_t)gd[0].bg_inode_table << blocksize_log2) + (EXT2_ROOT_INO-1) * sizeof(*inode), buf, sizeof(*inode));

	// dump lost+found dir inode
	STORE_LE(inode->i_links_count, 2); // both "/lost+found" and "/lost+found/." point to this inode
	STORE_LE(inode->i_block[0], inode->i_block[0]+1); // use next block //= gd[0].bg_inode_table + itsz + 1;
	PUT(((uint64_t)gd[0].bg_inode_table << blocksize_log2) + (EXT2_GOOD_OLD_FIRST_INO-1) * sizeof(*inode), buf, sizeof(*inode));

	// dump directories
	memset(buf, 0, blocksize);
	dir = (struct ext2_dir *)buf;

	// dump lost+found dir block
	STORE_LE(dir->inode1, EXT2_GOOD_OLD_FIRST_INO);
	STORE_LE(dir->rec_len1, 12);
	STORE_LE(dir->name_len1, 1);
	STORE_LE(dir->file_type1, EXT2_FT_DIR);
	dir->name1[0] = '.';
	STORE_LE(dir->inode2, EXT2_ROOT_INO);
	STORE_LE(dir->rec_len2, blocksize - 12);
	STORE_LE(dir->name_len2, 2);
	STORE_LE(dir->file_type2, EXT2_FT_DIR);
	dir->name2[0] = '.'; dir->name2[1] = '.';
	PUT((uint64_t)(gd[0].bg_inode_table + itsz + 1) * blocksize, buf, blocksize);

	// dump root dir block
	STORE_LE(dir->inode1, EXT2_ROOT_INO);
	STORE_LE(dir->rec_len2, 12);
	STORE_LE(dir->inode3, EXT2_GOOD_OLD_FIRST_INO);
	STORE_LE(dir->rec_len3, blocksize - 12 - 12);
	STORE_LE(dir->name_len3, 10);
	STORE_LE(dir->file_type3, EXT2_FT_DIR);
	strcpy(dir->name3, "lost+found");
	PUT((uint64_t)(gd[0].bg_inode_table + itsz + 0) * blocksize, buf, blocksize);

//	bb_info_msg("done\n"
//		"This filesystem will be automatically checked every %u mounts or\n"
//		"%u days, whichever comes first. Use tune2fs -c or -i to override.",
//		sb->s_max_mnt_count, sb->s_checkinterval / (3600 * 24)
//	);

	// cleanup
	if (ENABLE_FEATURE_CLEAN_UP) {
		free(buf);
		free(gd);
		free(sb);
	}

	xclose(fd);
	return EXIT_SUCCESS;
}
