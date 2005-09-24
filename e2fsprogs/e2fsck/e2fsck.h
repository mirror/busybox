/*
 * e2fsck.h
 * 
 * Copyright (C) 1993, 1994 Theodore Ts'o.  This file may be
 * redistributed under the terms of the GNU Public License.
 * 
 */

#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <time.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SETJMP_H
#include <setjmp.h>
#endif

#if EXT2_FLAT_INCLUDES
#include "ext2_fs.h"
#include "ext2fs.h"
#include "blkid.h"
#else
#include "ext2fs/ext2_fs.h"
#include "ext2fs/ext2fs.h"
#include "blkid/blkid.h"
#endif

/*
 * Exit codes used by fsck-type programs
 */
#define FSCK_OK          0	/* No errors */
#define FSCK_NONDESTRUCT 1	/* File system errors corrected */
#define FSCK_REBOOT      2	/* System should be rebooted */
#define FSCK_UNCORRECTED 4	/* File system errors left uncorrected */
#define FSCK_ERROR       8	/* Operational error */
#define FSCK_USAGE       16	/* Usage or syntax error */
#define FSCK_CANCELED	 32	/* Aborted with a signal or ^C */
#define FSCK_LIBRARY     128	/* Shared library error */

/*
 * The last ext2fs revision level that this version of e2fsck is able to
 * support
 */
#define E2FSCK_CURRENT_REV	1

/*
 * The directory information structure; stores directory information
 * collected in earlier passes, to avoid disk i/o in fetching the
 * directory information.
 */
struct dir_info {
	ext2_ino_t		ino;	/* Inode number */
	ext2_ino_t		dotdot;	/* Parent according to '..' */
	ext2_ino_t		parent; /* Parent according to treewalk */
};


/*
 * The indexed directory information structure; stores information for
 * directories which contain a hash tree index.
 */
struct dx_dir_info {
	ext2_ino_t		ino; 		/* Inode number */
	int			numblocks;	/* number of blocks */
	int			hashversion;
	short			depth;		/* depth of tree */
	struct dx_dirblock_info	*dx_block; 	/* Array of size numblocks */
};

#define DX_DIRBLOCK_ROOT	1
#define DX_DIRBLOCK_LEAF	2
#define DX_DIRBLOCK_NODE	3
#define DX_DIRBLOCK_CORRUPT	4
#define DX_DIRBLOCK_CLEARED	8

struct dx_dirblock_info {
	int		type;
	blk_t		phys;
	int		flags;
	blk_t		parent;
	ext2_dirhash_t	min_hash; 
	ext2_dirhash_t	max_hash;
	ext2_dirhash_t	node_min_hash; 
	ext2_dirhash_t	node_max_hash;
};

#define DX_FLAG_REFERENCED	1
#define DX_FLAG_DUP_REF		2
#define DX_FLAG_FIRST		4
#define DX_FLAG_LAST		8

#ifdef RESOURCE_TRACK
/*
 * This structure is used for keeping track of how much resources have
 * been used for a particular pass of e2fsck.
 */
struct resource_track {
	struct timeval time_start;
	struct timeval user_start;
	struct timeval system_start;
	void	*brk_start;
};
#endif

/*
 * E2fsck options
 */
#define E2F_OPT_READONLY	0x0001
#define E2F_OPT_PREEN		0x0002
#define E2F_OPT_YES		0x0004
#define E2F_OPT_NO		0x0008
#define E2F_OPT_TIME		0x0010
#define E2F_OPT_TIME2		0x0020
#define E2F_OPT_CHECKBLOCKS	0x0040
#define E2F_OPT_DEBUG		0x0080
#define E2F_OPT_FORCE		0x0100
#define E2F_OPT_WRITECHECK	0x0200
#define E2F_OPT_COMPRESS_DIRS	0x0400

/*
 * E2fsck flags
 */
#define E2F_FLAG_ABORT		0x0001 /* Abort signaled */
#define E2F_FLAG_CANCEL		0x0002 /* Cancel signaled */
#define E2F_FLAG_SIGNAL_MASK	0x0003
#define E2F_FLAG_RESTART	0x0004 /* Restart signaled */

#define E2F_FLAG_SETJMP_OK	0x0010 /* Setjmp valid for abort */

#define E2F_FLAG_PROG_BAR	0x0020 /* Progress bar on screen */
#define E2F_FLAG_PROG_SUPPRESS	0x0040 /* Progress suspended */
#define E2F_FLAG_JOURNAL_INODE	0x0080 /* Create a new ext3 journal inode */
#define E2F_FLAG_SB_SPECIFIED	0x0100 /* The superblock was explicitly 
					* specified by the user */
#define E2F_FLAG_RESTARTED	0x0200 /* E2fsck has been restarted */
#define E2F_FLAG_RESIZE_INODE	0x0400 /* Request to recreate resize inode */

/*
 * Defines for indicating the e2fsck pass number
 */
#define E2F_PASS_1	1
#define E2F_PASS_2	2
#define E2F_PASS_3	3
#define E2F_PASS_4	4
#define E2F_PASS_5	5
#define E2F_PASS_1B	6

/*
 * Define the extended attribute refcount structure
 */
typedef struct ea_refcount *ext2_refcount_t;

/*
 * This is the global e2fsck structure.
 */
typedef struct e2fsck_struct *e2fsck_t;

struct e2fsck_struct {
	ext2_filsys fs;
	const char *program_name;
	char *filesystem_name;
	char *device_name;
	char *io_options;
	int	flags;		/* E2fsck internal flags */
	int	options;
	blk_t	use_superblock;	/* sb requested by user */
	blk_t	superblock;	/* sb used to open fs */
	int	blocksize;	/* blocksize */
	blk_t	num_blocks;	/* Total number of blocks */
	int	mount_flags;
	blkid_cache blkid;	/* blkid cache */

#ifdef HAVE_SETJMP_H
	jmp_buf	abort_loc;
#endif
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
	ext2_icount_t	inode_count;
	ext2_icount_t inode_link_info;

	ext2_refcount_t	refcount;
	ext2_refcount_t refcount_extra;

	/*
	 * Array of flags indicating whether an inode bitmap, block
	 * bitmap, or inode table is invalid
	 */
	int *invalid_inode_bitmap_flag;
	int *invalid_block_bitmap_flag;
	int *invalid_inode_table_flag;
	int invalid_bitmaps;	/* There are invalid bitmaps/itable */

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
	int		dir_info_count;
	int		dir_info_size;
	struct dir_info	*dir_info;

	/*
	 * Indexed directory information
	 */
	int		dx_dir_info_count;
	int		dx_dir_info_size;
	struct dx_dir_info *dx_dir_info;

	/*
	 * Directories to hash
	 */
	ext2_u32_list	dirs_to_hash;

	/*
	 * Tuning parameters
	 */
	int process_inode_size;
	int inode_buffer_blocks;

	/*
	 * ext3 journal support
	 */
	io_channel	journal_io;
	char	*journal_name;

#ifdef RESOURCE_TRACK
	/*
	 * For timing purposes
	 */
	struct resource_track	global_rtrack;
#endif

	/*
	 * How we display the progress update (for unix)
	 */
	int progress_fd;
	int progress_pos;
	int progress_last_percent;
	unsigned int progress_last_time;
	int interactive;	/* Are we connected directly to a tty? */
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

/* Used by the region allocation code */
typedef __u32 region_addr_t;
typedef struct region_struct *region_t;

#ifndef HAVE_STRNLEN
#define strnlen(str, x) e2fsck_strnlen((str),(x))
extern int e2fsck_strnlen(const char * s, int count);
#endif

/*
 * Procedure declarations
 */

extern void e2fsck_pass1(e2fsck_t ctx);
extern void e2fsck_pass1_dupblocks(e2fsck_t ctx, char *block_buf);
extern void e2fsck_pass2(e2fsck_t ctx);
extern void e2fsck_pass3(e2fsck_t ctx);
extern void e2fsck_pass4(e2fsck_t ctx);
extern void e2fsck_pass5(e2fsck_t ctx);

/* e2fsck.c */
extern errcode_t e2fsck_allocate_context(e2fsck_t *ret);
extern errcode_t e2fsck_reset_context(e2fsck_t ctx);
extern void e2fsck_free_context(e2fsck_t ctx);
extern int e2fsck_run(e2fsck_t ctx);


/* badblock.c */
extern void read_bad_blocks_file(e2fsck_t ctx, const char *bad_blocks_file,
				 int replace_bad_blocks);

/* dirinfo.c */
extern void e2fsck_add_dir_info(e2fsck_t ctx, ext2_ino_t ino, ext2_ino_t parent);
extern struct dir_info *e2fsck_get_dir_info(e2fsck_t ctx, ext2_ino_t ino);
extern void e2fsck_free_dir_info(e2fsck_t ctx);
extern int e2fsck_get_num_dirinfo(e2fsck_t ctx);
extern struct dir_info *e2fsck_dir_info_iter(e2fsck_t ctx, int *control);

/* dx_dirinfo.c */
extern void e2fsck_add_dx_dir(e2fsck_t ctx, ext2_ino_t ino, int num_blocks);
extern struct dx_dir_info *e2fsck_get_dx_dir_info(e2fsck_t ctx, ext2_ino_t ino);
extern void e2fsck_free_dx_dir_info(e2fsck_t ctx);
extern int e2fsck_get_num_dx_dirinfo(e2fsck_t ctx);
extern struct dx_dir_info *e2fsck_dx_dir_info_iter(e2fsck_t ctx, int *control);

/* ea_refcount.c */
extern errcode_t ea_refcount_create(int size, ext2_refcount_t *ret);
extern void ea_refcount_free(ext2_refcount_t refcount);
extern errcode_t ea_refcount_fetch(ext2_refcount_t refcount, blk_t blk,
				   int *ret);
extern errcode_t ea_refcount_increment(ext2_refcount_t refcount,
				       blk_t blk, int *ret);
extern errcode_t ea_refcount_decrement(ext2_refcount_t refcount,
				       blk_t blk, int *ret);
extern errcode_t ea_refcount_store(ext2_refcount_t refcount,
				   blk_t blk, int count);
extern blk_t ext2fs_get_refcount_size(ext2_refcount_t refcount);
extern void ea_refcount_intr_begin(ext2_refcount_t refcount);
extern blk_t ea_refcount_intr_next(ext2_refcount_t refcount, int *ret);

/* ehandler.c */
extern const char *ehandler_operation(const char *op);
extern void ehandler_init(io_channel channel);

/* journal.c */
extern int e2fsck_check_ext3_journal(e2fsck_t ctx);
extern int e2fsck_run_ext3_journal(e2fsck_t ctx);
extern void e2fsck_move_ext3_journal(e2fsck_t ctx);

/* pass1.c */
extern void e2fsck_use_inode_shortcuts(e2fsck_t ctx, int bool);
extern int e2fsck_pass1_check_device_inode(ext2_filsys fs,
					   struct ext2_inode *inode);
extern int e2fsck_pass1_check_symlink(ext2_filsys fs,
				      struct ext2_inode *inode, char *buf);

/* pass2.c */
extern int e2fsck_process_bad_inode(e2fsck_t ctx, ext2_ino_t dir,
				    ext2_ino_t ino, char *buf);

/* pass3.c */
extern int e2fsck_reconnect_file(e2fsck_t ctx, ext2_ino_t inode);
extern errcode_t e2fsck_expand_directory(e2fsck_t ctx, ext2_ino_t dir,
					 int num, int gauranteed_size);
extern ext2_ino_t e2fsck_get_lost_and_found(e2fsck_t ctx, int fix);
extern errcode_t e2fsck_adjust_inode_count(e2fsck_t ctx, ext2_ino_t ino, 
					   int adj);


/* region.c */
extern region_t region_create(region_addr_t min, region_addr_t max);
extern void region_free(region_t region);
extern int region_allocate(region_t region, region_addr_t start, int n);

/* rehash.c */
errcode_t e2fsck_rehash_dir(e2fsck_t ctx, ext2_ino_t ino);
void e2fsck_rehash_directories(e2fsck_t ctx);

/* super.c */
void check_super_block(e2fsck_t ctx);
errcode_t e2fsck_get_device_size(e2fsck_t ctx);

/* swapfs.c */
void swap_filesys(e2fsck_t ctx);

/* util.c */
extern void *e2fsck_allocate_memory(e2fsck_t ctx, unsigned int size,
				    const char *description);
extern int ask(e2fsck_t ctx, const char * string, int def);
extern int ask_yn(const char * string, int def);
extern void e2fsck_read_bitmaps(e2fsck_t ctx);
extern void e2fsck_write_bitmaps(e2fsck_t ctx);
extern void preenhalt(e2fsck_t ctx);
extern char *string_copy(e2fsck_t ctx, const char *str, int len);
#ifdef RESOURCE_TRACK
extern void print_resource_track(const char *desc,
				 struct resource_track *track);
extern void init_resource_track(struct resource_track *track);
#endif
extern int inode_has_valid_blocks(struct ext2_inode *inode);
extern void e2fsck_read_inode(e2fsck_t ctx, unsigned long ino,
			      struct ext2_inode * inode, const char * proc);
extern void e2fsck_write_inode(e2fsck_t ctx, unsigned long ino,
			       struct ext2_inode * inode, const char * proc);
#ifdef MTRACE
extern void mtrace_print(char *mesg);
#endif
extern blk_t get_backup_sb(e2fsck_t ctx, ext2_filsys fs,
			   const char *name, io_manager manager);
extern int ext2_file_type(unsigned int mode);

/* unix.c */
extern void e2fsck_clear_progbar(e2fsck_t ctx);
extern int e2fsck_simple_progress(e2fsck_t ctx, const char *label,
				  float percent, unsigned int dpynum);
