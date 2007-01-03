/* vi: set sw=4 ts=4: */
/*
 * mkfs.c - make a linux (minix) file-system.
 *
 * (C) 1991 Linus Torvalds. This file may be redistributed as per
 * the Linux copyright.
 */

/*
 * DD.MM.YY
 *
 * 24.11.91  -	Time began. Used the fsck sources to get started.
 *
 * 25.11.91  -	Corrected some bugs. Added support for ".badblocks"
 *		The algorithm for ".badblocks" is a bit weird, but
 *		it should work. Oh, well.
 *
 * 25.01.92  -	Added the -l option for getting the list of bad blocks
 *		out of a named file. (Dave Rivers, rivers@ponds.uucp)
 *
 * 28.02.92  -	Added %-information when using -c.
 *
 * 28.02.93  -	Added support for other namelengths than the original
 *		14 characters so that I can test the new kernel routines..
 *
 * 09.10.93  -	Make exit status conform to that required by fsutil
 *		(Rik Faith, faith@cs.unc.edu)
 *
 * 31.10.93  -	Added inode request feature, for backup floppies: use
 *		32 inodes, for a news partition use more.
 *		(Scott Heavner, sdh@po.cwru.edu)
 *
 * 03.01.94  -	Added support for file system valid flag.
 *		(Dr. Wettstein, greg%wind.uucp@plains.nodak.edu)
 *
 * 30.10.94  -  added support for v2 filesystem
 *	        (Andreas Schwab, schwab@issan.informatik.uni-dortmund.de)
 *
 * 09.11.94  -	Added test to prevent overwrite of mounted fs adapted
 *		from Theodore Ts'o's (tytso@athena.mit.edu) mke2fs
 *		program.  (Daniel Quinlan, quinlan@yggdrasil.com)
 *
 * 03.20.95  -	Clear first 512 bytes of filesystem to make certain that
 *		the filesystem is not misidentified as a MS-DOS FAT filesystem.
 *		(Daniel Quinlan, quinlan@yggdrasil.com)
 *
 * 02.07.96  -  Added small patch from Russell King to make the program a
 *		good deal more portable (janl@math.uio.no)
 *
 * Usage:  mkfs [-c | -l filename ] [-v] [-nXX] [-iXX] device [size-in-blocks]
 *
 *	-c for readability checking (SLOW!)
 *      -l for getting a list of bad blocks from a file.
 *	-n for namelength (currently the kernel only uses 14 or 30)
 *	-i for number of inodes
 *	-v for v2 filesystem
 *
 * The device may be a block device or a image of one, but this isn't
 * enforced (but it's not much fun on a character device :-).
 *
 * Modified for BusyBox by Erik Andersen <andersen@debian.org> --
 *	removed getopt based parser and added a hand rolled one.
 */

#include "busybox.h"
#include <mntent.h>

#include "minix.h"

#define DEBUG 0

/* If debugging, store the very same times/uids/gids for image consistency */
#if DEBUG
# define CUR_TIME 0
# define GETUID 0
# define GETGID 0
#else
# define CUR_TIME time(NULL)
# define GETUID getuid()
# define GETGID getgid()
#endif

enum {
	MAX_GOOD_BLOCKS         = 512,
	TEST_BUFFER_BLOCKS      = 16,
};

#if ENABLE_FEATURE_MINIX2
static int version2;
#else
enum { version2 = 0 };
#endif

static char *device_name;
static int dev_fd = -1;
static uint32_t total_blocks;
static int badblocks;
/* default (changed to 30, per Linus's suggestion, Sun Nov 21 08:05:07 1993) */
static int namelen = 30;
static int dirsize = 32;
static int magic = MINIX1_SUPER_MAGIC2;

static char root_block[BLOCK_SIZE];
static char super_block_buffer[BLOCK_SIZE];
static char boot_block_buffer[512];
static char *inode_buffer;

static char *inode_map;
static char *zone_map;

static int used_good_blocks;
static unsigned short good_blocks_table[MAX_GOOD_BLOCKS];
static unsigned long req_nr_inodes;

static ATTRIBUTE_ALWAYS_INLINE unsigned div_roundup(unsigned size, unsigned n)
{
	return (size + n-1) / n;
}

#define INODE_BUF1              (((struct minix1_inode*)inode_buffer) - 1)
#define INODE_BUF2              (((struct minix2_inode*)inode_buffer) - 1)

#define SB                      (*(struct minix_super_block*)super_block_buffer)

#define SB_INODES               (SB.s_ninodes)
#define SB_IMAPS                (SB.s_imap_blocks)
#define SB_ZMAPS                (SB.s_zmap_blocks)
#define SB_FIRSTZONE            (SB.s_firstdatazone)
#define SB_ZONE_SIZE            (SB.s_log_zone_size)
#define SB_MAXSIZE              (SB.s_max_size)
#define SB_MAGIC                (SB.s_magic)

#if !ENABLE_FEATURE_MINIX2
# define SB_ZONES               (SB.s_nzones)
# define INODE_BLOCKS           div_roundup(SB_INODES, MINIX1_INODES_PER_BLOCK)
#else
# define SB_ZONES               (version2 ? SB.s_zones : SB.s_nzones)
# define INODE_BLOCKS           div_roundup(SB_INODES, \
                                version2 ? MINIX2_INODES_PER_BLOCK : MINIX1_INODES_PER_BLOCK)
#endif

#define INODE_BUFFER_SIZE       (INODE_BLOCKS * BLOCK_SIZE)
#define NORM_FIRSTZONE          (2 + SB_IMAPS + SB_ZMAPS + INODE_BLOCKS)

static int bit(const char* a, unsigned i)
{
	  return a[i >> 3] & (1<<(i & 7));
}

/* Note: do not assume 0/1, it is 0/nonzero */
#define inode_in_use(x) bit(inode_map,(x))
#define zone_in_use(x)  bit(zone_map,(x)-SB_FIRSTZONE+1)

#define mark_inode(x)   setbit(inode_map,(x))
#define unmark_inode(x) clrbit(inode_map,(x))
#define mark_zone(x)    setbit(zone_map,(x)-SB_FIRSTZONE+1)
#define unmark_zone(x)  clrbit(zone_map,(x)-SB_FIRSTZONE+1)

#ifndef BLKGETSIZE
# define BLKGETSIZE     _IO(0x12,96)    /* return device size */
#endif


static long valid_offset(int fd, int offset)
{
	char ch;

	if (lseek(fd, offset, SEEK_SET) < 0)
		return 0;
	if (read(fd, &ch, 1) < 1)
		return 0;
	return 1;
}

static int count_blocks(int fd)
{
	int high, low;

	low = 0;
	for (high = 1; valid_offset(fd, high); high *= 2)
		low = high;

	while (low < high - 1) {
		const int mid = (low + high) / 2;

		if (valid_offset(fd, mid))
			low = mid;
		else
			high = mid;
	}
	valid_offset(fd, 0);
	return (low + 1);
}

static int get_size(const char *file)
{
	int fd;
	long size;

	fd = xopen(file, O_RDWR);
	if (ioctl(fd, BLKGETSIZE, &size) >= 0) {
		close(fd);
		return (size * 512);
	}

	size = count_blocks(fd);
	close(fd);
	return size;
}

static void write_tables(void)
{
	/* Mark the super block valid. */
	SB.s_state |= MINIX_VALID_FS;
	SB.s_state &= ~MINIX_ERROR_FS;

	msg_eol = "seek to 0 failed";
	xlseek(dev_fd, 0, SEEK_SET);

	msg_eol = "cannot clear boot sector";
	xwrite(dev_fd, boot_block_buffer, 512);

	msg_eol = "seek to BLOCK_SIZE failed";
	xlseek(dev_fd, BLOCK_SIZE, SEEK_SET);

	msg_eol = "cannot write superblock";
	xwrite(dev_fd, super_block_buffer, BLOCK_SIZE);

	msg_eol = "cannot write inode map";
	xwrite(dev_fd, inode_map, SB_IMAPS * BLOCK_SIZE);

	msg_eol = "cannot write zone map";
	xwrite(dev_fd, zone_map, SB_ZMAPS * BLOCK_SIZE);

	msg_eol = "cannot write inodes";
	xwrite(dev_fd, inode_buffer, INODE_BUFFER_SIZE);

	msg_eol = "\n";
}

static void write_block(int blk, char *buffer)
{
	xlseek(dev_fd, blk * BLOCK_SIZE, SEEK_SET);
	xwrite(dev_fd, buffer, BLOCK_SIZE);
}

static int get_free_block(void)
{
	int blk;

	if (used_good_blocks + 1 >= MAX_GOOD_BLOCKS)
		bb_error_msg_and_die("too many bad blocks");
	if (used_good_blocks)
		blk = good_blocks_table[used_good_blocks - 1] + 1;
	else
		blk = SB_FIRSTZONE;
	while (blk < SB_ZONES && zone_in_use(blk))
		blk++;
	if (blk >= SB_ZONES)
		bb_error_msg_and_die("not enough good blocks");
	good_blocks_table[used_good_blocks] = blk;
	used_good_blocks++;
	return blk;
}

static void mark_good_blocks(void)
{
	int blk;

	for (blk = 0; blk < used_good_blocks; blk++)
		mark_zone(good_blocks_table[blk]);
}

static int next(int zone)
{
	if (!zone)
		zone = SB_FIRSTZONE - 1;
	while (++zone < SB_ZONES)
		if (zone_in_use(zone))
			return zone;
	return 0;
}

static void make_bad_inode(void)
{
	struct minix1_inode *inode = &INODE_BUF1[MINIX_BAD_INO];
	int i, j, zone;
	int ind = 0, dind = 0;
	unsigned short ind_block[BLOCK_SIZE >> 1];
	unsigned short dind_block[BLOCK_SIZE >> 1];

#define NEXT_BAD (zone = next(zone))

	if (!badblocks)
		return;
	mark_inode(MINIX_BAD_INO);
	inode->i_nlinks = 1;
	/* BTW, setting this makes all images different */
	/* it's harder to check for bugs then - diff isn't helpful :(... */
	inode->i_time = CUR_TIME;
	inode->i_mode = S_IFREG + 0000;
	inode->i_size = badblocks * BLOCK_SIZE;
	zone = next(0);
	for (i = 0; i < 7; i++) {
		inode->i_zone[i] = zone;
		if (!NEXT_BAD)
			goto end_bad;
	}
	inode->i_zone[7] = ind = get_free_block();
	memset(ind_block, 0, BLOCK_SIZE);
	for (i = 0; i < 512; i++) {
		ind_block[i] = zone;
		if (!NEXT_BAD)
			goto end_bad;
	}
	inode->i_zone[8] = dind = get_free_block();
	memset(dind_block, 0, BLOCK_SIZE);
	for (i = 0; i < 512; i++) {
		write_block(ind, (char *) ind_block);
		dind_block[i] = ind = get_free_block();
		memset(ind_block, 0, BLOCK_SIZE);
		for (j = 0; j < 512; j++) {
			ind_block[j] = zone;
			if (!NEXT_BAD)
				goto end_bad;
		}
	}
	bb_error_msg_and_die("too many bad blocks");
 end_bad:
	if (ind)
		write_block(ind, (char *) ind_block);
	if (dind)
		write_block(dind, (char *) dind_block);
}

#if ENABLE_FEATURE_MINIX2
static void make_bad_inode2(void)
{
	struct minix2_inode *inode = &INODE_BUF2[MINIX_BAD_INO];
	int i, j, zone;
	int ind = 0, dind = 0;
	unsigned long ind_block[BLOCK_SIZE >> 2];
	unsigned long dind_block[BLOCK_SIZE >> 2];

	if (!badblocks)
		return;
	mark_inode(MINIX_BAD_INO);
	inode->i_nlinks = 1;
	inode->i_atime = inode->i_mtime = inode->i_ctime = CUR_TIME;
	inode->i_mode = S_IFREG + 0000;
	inode->i_size = badblocks * BLOCK_SIZE;
	zone = next(0);
	for (i = 0; i < 7; i++) {
		inode->i_zone[i] = zone;
		if (!NEXT_BAD)
			goto end_bad;
	}
	inode->i_zone[7] = ind = get_free_block();
	memset(ind_block, 0, BLOCK_SIZE);
	for (i = 0; i < 256; i++) {
		ind_block[i] = zone;
		if (!NEXT_BAD)
			goto end_bad;
	}
	inode->i_zone[8] = dind = get_free_block();
	memset(dind_block, 0, BLOCK_SIZE);
	for (i = 0; i < 256; i++) {
		write_block(ind, (char *) ind_block);
		dind_block[i] = ind = get_free_block();
		memset(ind_block, 0, BLOCK_SIZE);
		for (j = 0; j < 256; j++) {
			ind_block[j] = zone;
			if (!NEXT_BAD)
				goto end_bad;
		}
	}
	/* Could make triple indirect block here */
	bb_error_msg_and_die("too many bad blocks");
 end_bad:
	if (ind)
		write_block(ind, (char *) ind_block);
	if (dind)
		write_block(dind, (char *) dind_block);
}
#else
void make_bad_inode2(void);
#endif

static void make_root_inode(void)
{
	struct minix1_inode *inode = &INODE_BUF1[MINIX_ROOT_INO];

	mark_inode(MINIX_ROOT_INO);
	inode->i_zone[0] = get_free_block();
	inode->i_nlinks = 2;
	inode->i_time = CUR_TIME;
	if (badblocks)
		inode->i_size = 3 * dirsize;
	else {
		root_block[2 * dirsize] = '\0';
		root_block[2 * dirsize + 1] = '\0';
		inode->i_size = 2 * dirsize;
	}
	inode->i_mode = S_IFDIR + 0755;
	inode->i_uid = GETUID;
	if (inode->i_uid)
		inode->i_gid = GETGID;
	write_block(inode->i_zone[0], root_block);
}

#if ENABLE_FEATURE_MINIX2
static void make_root_inode2(void)
{
	struct minix2_inode *inode = &INODE_BUF2[MINIX_ROOT_INO];

	mark_inode(MINIX_ROOT_INO);
	inode->i_zone[0] = get_free_block();
	inode->i_nlinks = 2;
	inode->i_atime = inode->i_mtime = inode->i_ctime = CUR_TIME;
	if (badblocks)
		inode->i_size = 3 * dirsize;
	else {
		root_block[2 * dirsize] = '\0';
		root_block[2 * dirsize + 1] = '\0';
		inode->i_size = 2 * dirsize;
	}
	inode->i_mode = S_IFDIR + 0755;
	inode->i_uid = GETUID;
	if (inode->i_uid)
		inode->i_gid = GETGID;
	write_block(inode->i_zone[0], root_block);
}
#else
void make_root_inode2(void);
#endif

static void setup_tables(void)
{
	unsigned long inodes;
	unsigned norm_firstzone;
	unsigned sb_zmaps;
	unsigned i;

	memset(super_block_buffer, 0, BLOCK_SIZE);
	memset(boot_block_buffer, 0, 512);
	SB_MAGIC = magic;
	SB_ZONE_SIZE = 0;
	SB_MAXSIZE = version2 ? 0x7fffffff : (7 + 512 + 512 * 512) * 1024;
	if (version2)
		SB.s_zones = total_blocks;
	else
		SB.s_nzones = total_blocks;

	/* some magic nrs: 1 inode / 3 blocks */
	if (req_nr_inodes == 0)
		inodes = total_blocks / 3;
	else
		inodes = req_nr_inodes;
	/* Round up inode count to fill block size */
	if (version2)
		inodes = (inodes + MINIX2_INODES_PER_BLOCK - 1) &
		                 ~(MINIX2_INODES_PER_BLOCK - 1);
	else
		inodes = (inodes + MINIX1_INODES_PER_BLOCK - 1) &
		                 ~(MINIX1_INODES_PER_BLOCK - 1);
	if (inodes > 65535)
		inodes = 65535;
	SB_INODES = inodes;
	SB_IMAPS = div_roundup(SB_INODES + 1, BITS_PER_BLOCK);

	/* Real bad hack but overwise mkfs.minix can be thrown
	 * in infinite loop...
	 * try:
	 * dd if=/dev/zero of=test.fs count=10 bs=1024
	 * mkfs.minix -i 200 test.fs
	 */
	/* This code is not insane: NORM_FIRSTZONE is not a constant,
	 * it is calculated from SB_INODES, SB_IMAPS and SB_ZMAPS */
	i = 999;
	SB_ZMAPS = 0;
	do {
		norm_firstzone = NORM_FIRSTZONE;
		sb_zmaps = div_roundup(total_blocks - norm_firstzone + 1, BITS_PER_BLOCK);
		if (SB_ZMAPS == sb_zmaps) goto got_it;
		SB_ZMAPS = sb_zmaps;
		/* new SB_ZMAPS, need to recalc NORM_FIRSTZONE */
	} while (--i);
	bb_error_msg_and_die("incompatible size/inode count, try different -i N");
 got_it:

	SB_FIRSTZONE = norm_firstzone;
	inode_map = xmalloc(SB_IMAPS * BLOCK_SIZE);
	zone_map = xmalloc(SB_ZMAPS * BLOCK_SIZE);
	memset(inode_map, 0xff, SB_IMAPS * BLOCK_SIZE);
	memset(zone_map, 0xff, SB_ZMAPS * BLOCK_SIZE);
	for (i = SB_FIRSTZONE; i < SB_ZONES; i++)
		unmark_zone(i);
	for (i = MINIX_ROOT_INO; i <= SB_INODES; i++)
		unmark_inode(i);
	inode_buffer = xzalloc(INODE_BUFFER_SIZE);
	printf("%ld inodes\n", (long)SB_INODES);
	printf("%ld blocks\n", (long)SB_ZONES);
	printf("Firstdatazone=%ld (%ld)\n", (long)SB_FIRSTZONE, (long)norm_firstzone);
	printf("Zonesize=%d\n", BLOCK_SIZE << SB_ZONE_SIZE);
	printf("Maxsize=%ld\n", (long)SB_MAXSIZE);
}

/*
 * Perform a test of a block; return the number of
 * blocks readable/writable.
 */
static long do_check(char *buffer, int try, unsigned current_block)
{
	long got;

	/* Seek to the correct loc. */
	msg_eol = "seek failed during testing of blocks";
	xlseek(dev_fd, current_block * BLOCK_SIZE, SEEK_SET);
	msg_eol = "\n";

	/* Try the read */
	got = read(dev_fd, buffer, try * BLOCK_SIZE);
	if (got < 0)
		got = 0;
	if (got & (BLOCK_SIZE - 1)) {
		printf("Weird values in do_check: probably bugs\n");
	}
	got /= BLOCK_SIZE;
	return got;
}

static unsigned currently_testing;

static void alarm_intr(int alnum)
{
	if (currently_testing >= SB_ZONES)
		return;
	signal(SIGALRM, alarm_intr);
	alarm(5);
	if (!currently_testing)
		return;
	printf("%d ...", currently_testing);
	fflush(stdout);
}

static void check_blocks(void)
{
	int try, got;
	/* buffer[] was the biggest static in entire bbox */
	char *buffer = xmalloc(BLOCK_SIZE * TEST_BUFFER_BLOCKS);

	currently_testing = 0;
	signal(SIGALRM, alarm_intr);
	alarm(5);
	while (currently_testing < SB_ZONES) {
		msg_eol = "seek failed in check_blocks";
		xlseek(dev_fd, currently_testing * BLOCK_SIZE, SEEK_SET);
		msg_eol = "\n";
		try = TEST_BUFFER_BLOCKS;
		if (currently_testing + try > SB_ZONES)
			try = SB_ZONES - currently_testing;
		got = do_check(buffer, try, currently_testing);
		currently_testing += got;
		if (got == try)
			continue;
		if (currently_testing < SB_FIRSTZONE)
			bb_error_msg_and_die("bad blocks before data-area: cannot make fs");
		mark_zone(currently_testing);
		badblocks++;
		currently_testing++;
	}
	free(buffer);
	printf("%d bad block(s)\n", badblocks);
}

static void get_list_blocks(char *filename)
{
	FILE *listfile;
	unsigned long blockno;

	listfile = xfopen(filename, "r");
	while (!feof(listfile)) {
		fscanf(listfile, "%ld\n", &blockno);
		mark_zone(blockno);
		badblocks++;
	}
	printf("%d bad block(s)\n", badblocks);
}

int mkfs_minix_main(int argc, char **argv)
{
	struct mntent *mp;
	unsigned opt;
	char *tmp;
	struct stat statbuf;
	char *str_i, *str_n;
	char *listfile = NULL;

	if (INODE_SIZE1 * MINIX1_INODES_PER_BLOCK != BLOCK_SIZE)
		bb_error_msg_and_die("bad inode size");
#if ENABLE_FEATURE_MINIX2
	if (INODE_SIZE2 * MINIX2_INODES_PER_BLOCK != BLOCK_SIZE)
		bb_error_msg_and_die("bad inode size");
#endif

	opt = getopt32(argc, argv, "ci:l:n:v", &str_i, &listfile, &str_n);
	argv += optind;
	//if (opt & 1) -c
	if (opt & 2) req_nr_inodes = xatoul(str_i); // -i
	//if (opt & 4) -l
	if (opt & 8) { // -n
		namelen = xatoi_u(str_n);
		if (namelen == 14) magic = MINIX1_SUPER_MAGIC;
		else if (namelen == 30) magic = MINIX1_SUPER_MAGIC2;
		else bb_show_usage();
		dirsize = namelen + 2;
	}
	if (opt & 0x10) { // -v
#if ENABLE_FEATURE_MINIX2
		version2 = 1;
#else
		bb_error_msg_and_die("%s: not compiled with minix v2 support",
			device_name);
#endif
	}

	device_name = *argv++;
	if (!device_name)
		bb_show_usage();
	if (*argv)
		total_blocks = xatou32(*argv);
	else
		total_blocks = get_size(device_name) / 1024;

	if (total_blocks < 10)
		bb_error_msg_and_die("must have at least 10 blocks");

	if (version2) {
		magic = MINIX2_SUPER_MAGIC2;
		if (namelen == 14)
			magic = MINIX2_SUPER_MAGIC;
	} else if (total_blocks > 65535)
		total_blocks = 65535;

	/* Check if it is mounted */
	mp = find_mount_point(device_name, NULL);
	if (mp && strcmp(device_name, mp->mnt_fsname) == 0)
		bb_error_msg_and_die("%s is mounted on %s; "
				"refusing to make a filesystem",
				device_name, mp->mnt_dir);

	dev_fd = xopen(device_name, O_RDWR);
	if (fstat(dev_fd, &statbuf) < 0)
		bb_error_msg_and_die("cannot stat %s", device_name);
	if (!S_ISBLK(statbuf.st_mode))
		opt &= ~1; // clear -c (check)

/* I don't know why someone has special code to prevent mkfs.minix
 * on IDE devices. Why IDE but not SCSI, etc?... */
#if 0
	else if (statbuf.st_rdev == 0x0300 || statbuf.st_rdev == 0x0340)
		/* what is this? */
		bb_error_msg_and_die("will not try "
			"to make filesystem on '%s'", device_name);
#endif

	tmp = root_block;
	*(short *) tmp = 1;
	strcpy(tmp + 2, ".");
	tmp += dirsize;
	*(short *) tmp = 1;
	strcpy(tmp + 2, "..");
	tmp += dirsize;
	*(short *) tmp = 2;
	strcpy(tmp + 2, ".badblocks");

	setup_tables();

	if (opt & 1) // -c ?
		check_blocks();
	else if (listfile)
		get_list_blocks(listfile);

	if (version2) {
		make_root_inode2();
		make_bad_inode2();
	} else {
		make_root_inode();
		make_bad_inode();
	}

	mark_good_blocks();
	write_tables();
	return 0;
}
