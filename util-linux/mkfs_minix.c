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
 * 30.10.94 - added support for v2 filesystem
 *	      (Andreas Schwab, schwab@issan.informatik.uni-dortmund.de)
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
 *	-c for readablility checking (SLOW!)
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

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <mntent.h>
#include "busybox.h"

#define MINIX_ROOT_INO 1
#define MINIX_LINK_MAX	250
#define MINIX2_LINK_MAX	65530

#define MINIX_I_MAP_SLOTS	8
#define MINIX_Z_MAP_SLOTS	64
#define MINIX_SUPER_MAGIC	0x137F		/* original minix fs */
#define MINIX_SUPER_MAGIC2	0x138F		/* minix fs, 30 char names */
#define MINIX2_SUPER_MAGIC	0x2468		/* minix V2 fs */
#define MINIX2_SUPER_MAGIC2	0x2478		/* minix V2 fs, 30 char names */
#define MINIX_VALID_FS		0x0001		/* Clean fs. */
#define MINIX_ERROR_FS		0x0002		/* fs has errors. */

#define MINIX_INODES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct minix_inode)))
#define MINIX2_INODES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct minix2_inode)))

#define MINIX_V1		0x0001		/* original minix fs */
#define MINIX_V2		0x0002		/* minix V2 fs */

#define INODE_VERSION(inode)	inode->i_sb->u.minix_sb.s_version

/*
 * This is the original minix inode layout on disk.
 * Note the 8-bit gid and atime and ctime.
 */
struct minix_inode {
	u_int16_t i_mode;
	u_int16_t i_uid;
	u_int32_t i_size;
	u_int32_t i_time;
	u_int8_t  i_gid;
	u_int8_t  i_nlinks;
	u_int16_t i_zone[9];
};

/*
 * The new minix inode has all the time entries, as well as
 * long block numbers and a third indirect block (7+1+1+1
 * instead of 7+1+1). Also, some previously 8-bit values are
 * now 16-bit. The inode is now 64 bytes instead of 32.
 */
struct minix2_inode {
	u_int16_t i_mode;
	u_int16_t i_nlinks;
	u_int16_t i_uid;
	u_int16_t i_gid;
	u_int32_t i_size;
	u_int32_t i_atime;
	u_int32_t i_mtime;
	u_int32_t i_ctime;
	u_int32_t i_zone[10];
};

/*
 * minix super-block data on disk
 */
struct minix_super_block {
	u_int16_t s_ninodes;
	u_int16_t s_nzones;
	u_int16_t s_imap_blocks;
	u_int16_t s_zmap_blocks;
	u_int16_t s_firstdatazone;
	u_int16_t s_log_zone_size;
	u_int32_t s_max_size;
	u_int16_t s_magic;
	u_int16_t s_state;
	u_int32_t s_zones;
};

struct minix_dir_entry {
	u_int16_t inode;
	char name[0];
};

#define BLOCK_SIZE_BITS 10
#define BLOCK_SIZE (1<<BLOCK_SIZE_BITS)

#define NAME_MAX         255   /* # chars in a file name */

#define MINIX_INODES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct minix_inode)))

#define MINIX_VALID_FS               0x0001          /* Clean fs. */
#define MINIX_ERROR_FS               0x0002          /* fs has errors. */

#define MINIX_SUPER_MAGIC    0x137F          /* original minix fs */
#define MINIX_SUPER_MAGIC2   0x138F          /* minix fs, 30 char names */

#ifndef BLKGETSIZE
#define BLKGETSIZE _IO(0x12,96)    /* return device size */
#endif


#ifndef __linux__
#define volatile
#endif

#define MINIX_ROOT_INO 1
#define MINIX_BAD_INO 2

#define TEST_BUFFER_BLOCKS 16
#define MAX_GOOD_BLOCKS 512

#define UPPER(size,n) (((size)+((n)-1))/(n))
#define INODE_SIZE (sizeof(struct minix_inode))
#ifdef CONFIG_FEATURE_MINIX2
#define INODE_SIZE2 (sizeof(struct minix2_inode))
#define INODE_BLOCKS UPPER(INODES, (version2 ? MINIX2_INODES_PER_BLOCK \
				    : MINIX_INODES_PER_BLOCK))
#else
#define INODE_BLOCKS UPPER(INODES, (MINIX_INODES_PER_BLOCK))
#endif
#define INODE_BUFFER_SIZE (INODE_BLOCKS * BLOCK_SIZE)

#define BITS_PER_BLOCK (BLOCK_SIZE<<3)

static char *device_name = NULL;
static int DEV = -1;
static long BLOCKS = 0;
static int check = 0;
static int badblocks = 0;
static int namelen = 30;		/* default (changed to 30, per Linus's

								   suggestion, Sun Nov 21 08:05:07 1993) */
static int dirsize = 32;
static int magic = MINIX_SUPER_MAGIC2;
static int version2 = 0;

static char root_block[BLOCK_SIZE] = "\0";

static char *inode_buffer = NULL;

#define Inode (((struct minix_inode *) inode_buffer)-1)
#ifdef CONFIG_FEATURE_MINIX2
#define Inode2 (((struct minix2_inode *) inode_buffer)-1)
#endif
static char super_block_buffer[BLOCK_SIZE];
static char boot_block_buffer[512];

#define Super (*(struct minix_super_block *)super_block_buffer)
#define INODES ((unsigned long)Super.s_ninodes)
#ifdef CONFIG_FEATURE_MINIX2
#define ZONES ((unsigned long)(version2 ? Super.s_zones : Super.s_nzones))
#else
#define ZONES ((unsigned long)(Super.s_nzones))
#endif
#define IMAPS ((unsigned long)Super.s_imap_blocks)
#define ZMAPS ((unsigned long)Super.s_zmap_blocks)
#define FIRSTZONE ((unsigned long)Super.s_firstdatazone)
#define ZONESIZE ((unsigned long)Super.s_log_zone_size)
#define MAXSIZE ((unsigned long)Super.s_max_size)
#define MAGIC (Super.s_magic)
#define NORM_FIRSTZONE (2+IMAPS+ZMAPS+INODE_BLOCKS)

static char *inode_map;
static char *zone_map;

static unsigned short good_blocks_table[MAX_GOOD_BLOCKS];
static int used_good_blocks = 0;
static unsigned long req_nr_inodes = 0;

static inline int bit(char * a,unsigned int i)
{
	  return (a[i >> 3] & (1<<(i & 7))) != 0;
}
#define inode_in_use(x) (bit(inode_map,(x)))
#define zone_in_use(x) (bit(zone_map,(x)-FIRSTZONE+1))

#define mark_inode(x) (setbit(inode_map,(x)))
#define unmark_inode(x) (clrbit(inode_map,(x)))

#define mark_zone(x) (setbit(zone_map,(x)-FIRSTZONE+1))
#define unmark_zone(x) (clrbit(zone_map,(x)-FIRSTZONE+1))

/*
 * Check to make certain that our new filesystem won't be created on
 * an already mounted partition.  Code adapted from mke2fs, Copyright
 * (C) 1994 Theodore Ts'o.  Also licensed under GPL.
 */
static inline void check_mount(void)
{
	FILE *f;
	struct mntent *mnt;

	if ((f = setmntent(MOUNTED, "r")) == NULL)
		return;
	while ((mnt = getmntent(f)) != NULL)
		if (strcmp(device_name, mnt->mnt_fsname) == 0)
			break;
	endmntent(f);
	if (!mnt)
		return;

	bb_error_msg_and_die("%s is mounted; will not make a filesystem here!", device_name);
}

static long valid_offset(int fd, int offset)
{
	char ch;

	if (lseek(fd, offset, 0) < 0)
		return 0;
	if (read(fd, &ch, 1) < 1)
		return 0;
	return 1;
}

static inline int count_blocks(int fd)
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

static inline int get_size(const char *file)
{
	int fd;
	long size;

	if ((fd = open(file, O_RDWR)) < 0)
		bb_perror_msg_and_die("%s", file);
	if (ioctl(fd, BLKGETSIZE, &size) >= 0) {
		close(fd);
		return (size * 512);
	}

	size = count_blocks(fd);
	close(fd);
	return size;
}

static inline void write_tables(void)
{
	/* Mark the super block valid. */
	Super.s_state |= MINIX_VALID_FS;
	Super.s_state &= ~MINIX_ERROR_FS;

	if (lseek(DEV, 0, SEEK_SET))
		bb_error_msg_and_die("seek to boot block failed in write_tables");
	if (512 != write(DEV, boot_block_buffer, 512))
		bb_error_msg_and_die("unable to clear boot sector");
	if (BLOCK_SIZE != lseek(DEV, BLOCK_SIZE, SEEK_SET))
		bb_error_msg_and_die("seek failed in write_tables");
	if (BLOCK_SIZE != write(DEV, super_block_buffer, BLOCK_SIZE))
		bb_error_msg_and_die("unable to write super-block");
	if (IMAPS * BLOCK_SIZE != write(DEV, inode_map, IMAPS * BLOCK_SIZE))
		bb_error_msg_and_die("unable to write inode map");
	if (ZMAPS * BLOCK_SIZE != write(DEV, zone_map, ZMAPS * BLOCK_SIZE))
		bb_error_msg_and_die("unable to write zone map");
	if (INODE_BUFFER_SIZE != write(DEV, inode_buffer, INODE_BUFFER_SIZE))
		bb_error_msg_and_die("unable to write inodes");

}

static void write_block(int blk, char *buffer)
{
	if (blk * BLOCK_SIZE != lseek(DEV, blk * BLOCK_SIZE, SEEK_SET))
		bb_error_msg_and_die("seek failed in write_block");
	if (BLOCK_SIZE != write(DEV, buffer, BLOCK_SIZE))
		bb_error_msg_and_die("write failed in write_block");
}

static int get_free_block(void)
{
	int blk;

	if (used_good_blocks + 1 >= MAX_GOOD_BLOCKS)
		bb_error_msg_and_die("too many bad blocks");
	if (used_good_blocks)
		blk = good_blocks_table[used_good_blocks - 1] + 1;
	else
		blk = FIRSTZONE;
	while (blk < ZONES && zone_in_use(blk))
		blk++;
	if (blk >= ZONES)
		bb_error_msg_and_die("not enough good blocks");
	good_blocks_table[used_good_blocks] = blk;
	used_good_blocks++;
	return blk;
}

static inline void mark_good_blocks(void)
{
	int blk;

	for (blk = 0; blk < used_good_blocks; blk++)
		mark_zone(good_blocks_table[blk]);
}

static int next(int zone)
{
	if (!zone)
		zone = FIRSTZONE - 1;
	while (++zone < ZONES)
		if (zone_in_use(zone))
			return zone;
	return 0;
}

static inline void make_bad_inode(void)
{
	struct minix_inode *inode = &Inode[MINIX_BAD_INO];
	int i, j, zone;
	int ind = 0, dind = 0;
	unsigned short ind_block[BLOCK_SIZE >> 1];
	unsigned short dind_block[BLOCK_SIZE >> 1];

#define NEXT_BAD (zone = next(zone))

	if (!badblocks)
		return;
	mark_inode(MINIX_BAD_INO);
	inode->i_nlinks = 1;
	inode->i_time = time(NULL);
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

#ifdef CONFIG_FEATURE_MINIX2
static inline void make_bad_inode2(void)
{
	struct minix2_inode *inode = &Inode2[MINIX_BAD_INO];
	int i, j, zone;
	int ind = 0, dind = 0;
	unsigned long ind_block[BLOCK_SIZE >> 2];
	unsigned long dind_block[BLOCK_SIZE >> 2];

	if (!badblocks)
		return;
	mark_inode(MINIX_BAD_INO);
	inode->i_nlinks = 1;
	inode->i_atime = inode->i_mtime = inode->i_ctime = time(NULL);
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
#endif

static inline void make_root_inode(void)
{
	struct minix_inode *inode = &Inode[MINIX_ROOT_INO];

	mark_inode(MINIX_ROOT_INO);
	inode->i_zone[0] = get_free_block();
	inode->i_nlinks = 2;
	inode->i_time = time(NULL);
	if (badblocks)
		inode->i_size = 3 * dirsize;
	else {
		root_block[2 * dirsize] = '\0';
		root_block[2 * dirsize + 1] = '\0';
		inode->i_size = 2 * dirsize;
	}
	inode->i_mode = S_IFDIR + 0755;
	inode->i_uid = getuid();
	if (inode->i_uid)
		inode->i_gid = getgid();
	write_block(inode->i_zone[0], root_block);
}

#ifdef CONFIG_FEATURE_MINIX2
static inline void make_root_inode2(void)
{
	struct minix2_inode *inode = &Inode2[MINIX_ROOT_INO];

	mark_inode(MINIX_ROOT_INO);
	inode->i_zone[0] = get_free_block();
	inode->i_nlinks = 2;
	inode->i_atime = inode->i_mtime = inode->i_ctime = time(NULL);
	if (badblocks)
		inode->i_size = 3 * dirsize;
	else {
		root_block[2 * dirsize] = '\0';
		root_block[2 * dirsize + 1] = '\0';
		inode->i_size = 2 * dirsize;
	}
	inode->i_mode = S_IFDIR + 0755;
	inode->i_uid = getuid();
	if (inode->i_uid)
		inode->i_gid = getgid();
	write_block(inode->i_zone[0], root_block);
}
#endif

static inline void setup_tables(void)
{
	int i;
	unsigned long inodes;

	memset(super_block_buffer, 0, BLOCK_SIZE);
	memset(boot_block_buffer, 0, 512);
	MAGIC = magic;
	ZONESIZE = 0;
	MAXSIZE = version2 ? 0x7fffffff : (7 + 512 + 512 * 512) * 1024;
	ZONES = BLOCKS;
/* some magic nrs: 1 inode / 3 blocks */
	if (req_nr_inodes == 0)
		inodes = BLOCKS / 3;
	else
		inodes = req_nr_inodes;
	/* Round up inode count to fill block size */
#ifdef CONFIG_FEATURE_MINIX2
	if (version2)
		inodes = ((inodes + MINIX2_INODES_PER_BLOCK - 1) &
				  ~(MINIX2_INODES_PER_BLOCK - 1));
	else
#endif
		inodes = ((inodes + MINIX_INODES_PER_BLOCK - 1) &
				  ~(MINIX_INODES_PER_BLOCK - 1));
	if (inodes > 65535)
		inodes = 65535;
	INODES = inodes;
	IMAPS = UPPER(INODES + 1, BITS_PER_BLOCK);
	ZMAPS = 0;
	i = 0;
	while (ZMAPS !=
		   UPPER(BLOCKS - (2 + IMAPS + ZMAPS + INODE_BLOCKS) + 1,
				 BITS_PER_BLOCK) && i < 1000) {
		ZMAPS =
			UPPER(BLOCKS - (2 + IMAPS + ZMAPS + INODE_BLOCKS) + 1,
				  BITS_PER_BLOCK);
		i++;
	}
	/* Real bad hack but overwise mkfs.minix can be thrown
	 * in infinite loop...
	 * try:
	 * dd if=/dev/zero of=test.fs count=10 bs=1024
	 * /sbin/mkfs.minix -i 200 test.fs
	 * */
	if (i >= 999) {
		bb_error_msg_and_die("unable to allocate buffers for maps");
	}
	FIRSTZONE = NORM_FIRSTZONE;
	inode_map = xmalloc(IMAPS * BLOCK_SIZE);
	zone_map = xmalloc(ZMAPS * BLOCK_SIZE);
	memset(inode_map, 0xff, IMAPS * BLOCK_SIZE);
	memset(zone_map, 0xff, ZMAPS * BLOCK_SIZE);
	for (i = FIRSTZONE; i < ZONES; i++)
		unmark_zone(i);
	for (i = MINIX_ROOT_INO; i <= INODES; i++)
		unmark_inode(i);
	inode_buffer = xmalloc(INODE_BUFFER_SIZE);
	memset(inode_buffer, 0, INODE_BUFFER_SIZE);
	printf("%ld inodes\n", INODES);
	printf("%ld blocks\n", ZONES);
	printf("Firstdatazone=%ld (%ld)\n", FIRSTZONE, NORM_FIRSTZONE);
	printf("Zonesize=%d\n", BLOCK_SIZE << ZONESIZE);
	printf("Maxsize=%ld\n\n", MAXSIZE);
}

/*
 * Perform a test of a block; return the number of
 * blocks readable/writeable.
 */
static inline long do_check(char *buffer, int try, unsigned int current_block)
{
	long got;

	/* Seek to the correct loc. */
	if (lseek(DEV, current_block * BLOCK_SIZE, SEEK_SET) !=
		current_block * BLOCK_SIZE) {
		bb_error_msg_and_die("seek failed during testing of blocks");
	}


	/* Try the read */
	got = read(DEV, buffer, try * BLOCK_SIZE);
	if (got < 0)
		got = 0;
	if (got & (BLOCK_SIZE - 1)) {
		printf("Weird values in do_check: probably bugs\n");
	}
	got /= BLOCK_SIZE;
	return got;
}

static unsigned int currently_testing = 0;

static void alarm_intr(int alnum)
{
	if (currently_testing >= ZONES)
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
	static char buffer[BLOCK_SIZE * TEST_BUFFER_BLOCKS];

	currently_testing = 0;
	signal(SIGALRM, alarm_intr);
	alarm(5);
	while (currently_testing < ZONES) {
		if (lseek(DEV, currently_testing * BLOCK_SIZE, SEEK_SET) !=
			currently_testing * BLOCK_SIZE)
			bb_error_msg_and_die("seek failed in check_blocks");
		try = TEST_BUFFER_BLOCKS;
		if (currently_testing + try > ZONES)
			try = ZONES - currently_testing;
		got = do_check(buffer, try, currently_testing);
		currently_testing += got;
		if (got == try)
			continue;
		if (currently_testing < FIRSTZONE)
			bb_error_msg_and_die("bad blocks before data-area: cannot make fs");
		mark_zone(currently_testing);
		badblocks++;
		currently_testing++;
	}
	if (badblocks > 1)
		printf("%d bad blocks\n", badblocks);
	else if (badblocks == 1)
		printf("one bad block\n");
}

static void get_list_blocks(char *filename)
{
	FILE *listfile;
	unsigned long blockno;

	listfile = bb_xfopen(filename, "r");
	while (!feof(listfile)) {
		fscanf(listfile, "%ld\n", &blockno);
		mark_zone(blockno);
		badblocks++;
	}
	if (badblocks > 1)
		printf("%d bad blocks\n", badblocks);
	else if (badblocks == 1)
		printf("one bad block\n");
}

extern int mkfs_minix_main(int argc, char **argv)
{
	int i=1;
	char *tmp;
	struct stat statbuf;
	char *listfile = NULL;
	int stopIt=FALSE;

	if (INODE_SIZE * MINIX_INODES_PER_BLOCK != BLOCK_SIZE)
		bb_error_msg_and_die("bad inode size");
#ifdef CONFIG_FEATURE_MINIX2
	if (INODE_SIZE2 * MINIX2_INODES_PER_BLOCK != BLOCK_SIZE)
		bb_error_msg_and_die("bad inode size");
#endif
	
	/* Parse options */
	argv++;
	while (--argc >= 0 && *argv && **argv) {
		if (**argv == '-') {
			stopIt=FALSE;
			while (i > 0 && *++(*argv) && stopIt==FALSE) {
				switch (**argv) {
					case 'c':
						check = 1;
						break;
					case 'i':
						{
							char *cp=NULL;
							if (*(*argv+1) != 0) {
								cp = ++(*argv);
							} else {
								if (--argc == 0) {
									goto goodbye;
								}
								cp = *(++argv);
							}
							req_nr_inodes = strtoul(cp, &tmp, 0);
							if (*tmp)
								bb_show_usage();
							stopIt=TRUE;
							break;
						}
					case 'l':
						if (--argc == 0) {
							goto goodbye;
						}
						listfile = *(++argv);
						break;
					case 'n':
						{
							char *cp=NULL;

							if (*(*argv+1) != 0) {
								cp = ++(*argv);
							} else {
								if (--argc == 0) {
									goto goodbye;
								}
								cp = *(++argv);
							}
							i = strtoul(cp, &tmp, 0);
							if (*tmp)
								bb_show_usage();
							if (i == 14)
								magic = MINIX_SUPER_MAGIC;
							else if (i == 30)
								magic = MINIX_SUPER_MAGIC2;
							else 
								bb_show_usage();
							namelen = i;
							dirsize = i + 2;
							stopIt=TRUE;
							break;
						}
					case 'v':
#ifdef CONFIG_FEATURE_MINIX2
						version2 = 1;
#else
						bb_error_msg("%s: not compiled with minix v2 support",
								device_name);
						exit(-1);
#endif
						break;
					case '-':
					case 'h':
					default:
goodbye:
						bb_show_usage();
				}
			}
		} else {
			if (device_name == NULL)
				device_name = *argv;
			else if (BLOCKS == 0)
				BLOCKS = strtol(*argv, &tmp, 0);
			else {
				goto goodbye;
			}
		}
		argv++;
	}

	if (device_name && !BLOCKS)
		BLOCKS = get_size(device_name) / 1024;
	if (!device_name || BLOCKS < 10) {
		bb_show_usage();
	}
#ifdef CONFIG_FEATURE_MINIX2
	if (version2) {
		if (namelen == 14)
			magic = MINIX2_SUPER_MAGIC;
		else
			magic = MINIX2_SUPER_MAGIC2;
	} else
#endif
	if (BLOCKS > 65535)
		BLOCKS = 65535;
	check_mount();				/* is it already mounted? */
	tmp = root_block;
	*(short *) tmp = 1;
	strcpy(tmp + 2, ".");
	tmp += dirsize;
	*(short *) tmp = 1;
	strcpy(tmp + 2, "..");
	tmp += dirsize;
	*(short *) tmp = 2;
	strcpy(tmp + 2, ".badblocks");
	DEV = open(device_name, O_RDWR);
	if (DEV < 0)
		bb_error_msg_and_die("unable to open %s", device_name);
	if (fstat(DEV, &statbuf) < 0)
		bb_error_msg_and_die("unable to stat %s", device_name);
	if (!S_ISBLK(statbuf.st_mode))
		check = 0;
	else if (statbuf.st_rdev == 0x0300 || statbuf.st_rdev == 0x0340)
		bb_error_msg_and_die("will not try to make filesystem on '%s'", device_name);
	setup_tables();
	if (check)
		check_blocks();
	else if (listfile)
		get_list_blocks(listfile);
#ifdef CONFIG_FEATURE_MINIX2
	if (version2) {
		make_root_inode2();
		make_bad_inode2();
	} else
#endif
	{
		make_root_inode();
		make_bad_inode();
	}
	mark_good_blocks();
	write_tables();
	return( 0);

}
