#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <ctype.h>
#include <setjmp.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stddef.h>
#include <assert.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <malloc.h>
#include <termios.h>
#include <mntent.h>
#include <dirent.h>
#include "ext2fs/kernel-list.h"
#include <sys/types.h>
#include <linux/types.h>


/*
 * Now pull in the real linux/jfs.h definitions.
 */
#include "ext2fs/kernel-jbd.h"



#include "fsck.h"

#include "ext2fs/ext2_fs.h"
#include "blkid/blkid.h"
#include "ext2fs/ext2_ext_attr.h"
#include "uuid/uuid.h"
#include "busybox.h"

#ifdef HAVE_CONIO_H
#undef HAVE_TERMIOS_H
#include <conio.h>
#define read_a_char()   getch()
#else
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#endif


/*
 * The last ext2fs revision level that this version of e2fsck is able to
 * support
 */
#define E2FSCK_CURRENT_REV      1

/* Used by the region allocation code */
typedef __u32 region_addr_t;
typedef struct region_struct *region_t;

struct dx_dirblock_info {
	int             type;
	blk_t           phys;
	int             flags;
	blk_t           parent;
	ext2_dirhash_t  min_hash;
	ext2_dirhash_t  max_hash;
	ext2_dirhash_t  node_min_hash;
	ext2_dirhash_t  node_max_hash;
};

/*
These defines are used in the type field of dx_dirblock_info
*/

#define DX_DIRBLOCK_ROOT        1
#define DX_DIRBLOCK_LEAF        2
#define DX_DIRBLOCK_NODE        3
#define DX_DIRBLOCK_CORRUPT     4
#define DX_DIRBLOCK_CLEARED     8


/*
The following defines are used in the 'flags' field of a dx_dirblock_info
*/
#define DX_FLAG_REFERENCED      1
#define DX_FLAG_DUP_REF         2
#define DX_FLAG_FIRST           4
#define DX_FLAG_LAST            8

/*
 * E2fsck options
 */
#define E2F_OPT_READONLY        0x0001
#define E2F_OPT_PREEN           0x0002
#define E2F_OPT_YES             0x0004
#define E2F_OPT_NO              0x0008
#define E2F_OPT_TIME            0x0010
#define E2F_OPT_TIME2           0x0020
#define E2F_OPT_CHECKBLOCKS     0x0040
#define E2F_OPT_DEBUG           0x0080
#define E2F_OPT_FORCE           0x0100
#define E2F_OPT_WRITECHECK      0x0200
#define E2F_OPT_COMPRESS_DIRS   0x0400

/*
 * E2fsck flags
 */
#define E2F_FLAG_ABORT          0x0001 /* Abort signaled */
#define E2F_FLAG_CANCEL         0x0002 /* Cancel signaled */
#define E2F_FLAG_SIGNAL_MASK    0x0003
#define E2F_FLAG_RESTART        0x0004 /* Restart signaled */

#define E2F_FLAG_SETJMP_OK      0x0010 /* Setjmp valid for abort */

#define E2F_FLAG_PROG_BAR       0x0020 /* Progress bar on screen */
#define E2F_FLAG_PROG_SUPPRESS  0x0040 /* Progress suspended */
#define E2F_FLAG_JOURNAL_INODE  0x0080 /* Create a new ext3 journal inode */
#define E2F_FLAG_SB_SPECIFIED   0x0100 /* The superblock was explicitly
					* specified by the user */
#define E2F_FLAG_RESTARTED      0x0200 /* E2fsck has been restarted */
#define E2F_FLAG_RESIZE_INODE   0x0400 /* Request to recreate resize inode */

/*
 * Defines for indicating the e2fsck pass number
 */
#define E2F_PASS_1      1
#define E2F_PASS_2      2
#define E2F_PASS_3      3
#define E2F_PASS_4      4
#define E2F_PASS_5      5
#define E2F_PASS_1B     6

/*Don't know where these come from*/
#define READ 0
#define WRITE 1
#define KERN_ERR ""
#define KERN_DEBUG ""
#define cpu_to_be32(n) htonl(n)
#define be32_to_cpu(n) ntohl(n)


/*
 * The directory information structure; stores directory information
 * collected in earlier passes, to avoid disk i/o in fetching the
 * directory information.
 */
struct dir_info {
	ext2_ino_t              ino;    /* Inode number */
	ext2_ino_t              dotdot; /* Parent according to '..' */
	ext2_ino_t              parent; /* Parent according to treewalk */
};



/*
 * The indexed directory information structure; stores information for
 * directories which contain a hash tree index.
 */
struct dx_dir_info {
	ext2_ino_t              ino;            /* Inode number */
	int                     numblocks;      /* number of blocks */
	int                     hashversion;
	short                   depth;          /* depth of tree */
	struct dx_dirblock_info *dx_block;      /* Array of size numblocks */
};

/*
 * Define the extended attribute refcount structure
 */
typedef struct ea_refcount *ext2_refcount_t;

struct e2fsck_struct {
	ext2_filsys fs;
	const char *program_name;
	char *filesystem_name;
	char *device_name;
	char *io_options;
	int     flags;          /* E2fsck internal flags */
	int     options;
	blk_t   use_superblock; /* sb requested by user */
	blk_t   superblock;     /* sb used to open fs */
	int     blocksize;      /* blocksize */
	blk_t   num_blocks;     /* Total number of blocks */
	int     mount_flags;
	blkid_cache blkid;      /* blkid cache */

	jmp_buf abort_loc;

	unsigned long abort_code;

	int (*progress)(e2fsck_t ctx, int pass, unsigned long cur,
			unsigned long max);

	ext2fs_inode_bitmap inode_used_map; /* Inodes which are in use */
	ext2fs_inode_bitmap inode_bad_map; /* Inodes which are bad somehow */
	ext2fs_inode_bitmap inode_dir_map; /* Inodes which are directories */
	ext2fs_inode_bitmap inode_bb_map; /* Inodes which are in bad blocks */
	ext2fs_inode_bitmap inode_imagic_map; /* AFS inodes */
	ext2fs_inode_bitmap inode_reg_map; /* Inodes which are regular files*/

	ext2fs_block_bitmap block_found_map; /* Blocks which are in use */
	ext2fs_block_bitmap block_dup_map; /* Blks referenced more than once */
	ext2fs_block_bitmap block_ea_map; /* Blocks which are used by EA's */

	/*
	 * Inode count arrays
	 */
	ext2_icount_t   inode_count;
	ext2_icount_t inode_link_info;

	ext2_refcount_t refcount;
	ext2_refcount_t refcount_extra;

	/*
	 * Array of flags indicating whether an inode bitmap, block
	 * bitmap, or inode table is invalid
	 */
	int *invalid_inode_bitmap_flag;
	int *invalid_block_bitmap_flag;
	int *invalid_inode_table_flag;
	int invalid_bitmaps;    /* There are invalid bitmaps/itable */

	/*
	 * Block buffer
	 */
	char *block_buf;

	/*
	 * For pass1_check_directory and pass1_get_blocks
	 */
	ext2_ino_t stashed_ino;
	struct ext2_inode *stashed_inode;

	/*
	 * Location of the lost and found directory
	 */
	ext2_ino_t lost_and_found;
	int bad_lost_and_found;

	/*
	 * Directory information
	 */
	int             dir_info_count;
	int             dir_info_size;
	struct dir_info *dir_info;

	/*
	 * Indexed directory information
	 */
	int             dx_dir_info_count;
	int             dx_dir_info_size;
	struct dx_dir_info *dx_dir_info;

	/*
	 * Directories to hash
	 */
	ext2_u32_list   dirs_to_hash;

	/*
	 * Tuning parameters
	 */
	int process_inode_size;
	int inode_buffer_blocks;

	/*
	 * ext3 journal support
	 */
	io_channel      journal_io;
	char    *journal_name;

	/*
	 * How we display the progress update (for unix)
	 */
	int progress_fd;
	int progress_pos;
	int progress_last_percent;
	unsigned int progress_last_time;
	int interactive;        /* Are we connected directly to a tty? */
	char start_meta[2], stop_meta[2];

	/* File counts */
	int fs_directory_count;
	int fs_regular_count;
	int fs_blockdev_count;
	int fs_chardev_count;
	int fs_links_count;
	int fs_symlinks_count;
	int fs_fast_symlinks_count;
	int fs_fifo_count;
	int fs_total_count;
	int fs_badblocks_count;
	int fs_sockets_count;
	int fs_ind_count;
	int fs_dind_count;
	int fs_tind_count;
	int fs_fragmented;
	int large_files;
	int fs_ext_attr_inodes;
	int fs_ext_attr_blocks;

	int ext_attr_ver;

	/*
	 * For the use of callers of the e2fsck functions; not used by
	 * e2fsck functions themselves.
	 */
	void *priv_data;
};



static inline int tid_gt(tid_t x, tid_t y)
{
	int difference = (x - y);
	return (difference > 0);
}

static inline int tid_geq(tid_t x, tid_t y)
{
	int difference = (x - y);
	return (difference >= 0);
}


