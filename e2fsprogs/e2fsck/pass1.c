/*
 * pass1.c -- pass #1 of e2fsck: sequential scan of the inode table
 * 
 * Copyright (C) 1993, 1994, 1995, 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 * 
 * Pass 1 of e2fsck iterates over all the inodes in the filesystems,
 * and applies the following tests to each inode:
 *
 * 	- The mode field of the inode must be legal.
 * 	- The size and block count fields of the inode are correct.
 * 	- A data block must not be used by another inode
 *
 * Pass 1 also gathers the collects the following information:
 *
 * 	- A bitmap of which inodes are in use.		(inode_used_map)
 * 	- A bitmap of which inodes are directories.	(inode_dir_map)
 * 	- A bitmap of which inodes are regular files.	(inode_reg_map)
 * 	- A bitmap of which inodes have bad fields.	(inode_bad_map)
 * 	- A bitmap of which inodes are in bad blocks.	(inode_bb_map)
 * 	- A bitmap of which inodes are imagic inodes.	(inode_imagic_map)
 * 	- A bitmap of which blocks are in use.		(block_found_map)
 * 	- A bitmap of which blocks are in use by two inodes	(block_dup_map)
 * 	- The data blocks of the directory inodes.	(dir_map)
 *
 * Pass 1 is designed to stash away enough information so that the
 * other passes should not need to read in the inode information
 * during the normal course of a filesystem check.  (Althogh if an
 * inconsistency is detected, other passes may need to read in an
 * inode to fix it.)
 *
 * Note that pass 1B will be invoked if there are any duplicate blocks
 * found.
 */

#define _GNU_SOURCE 1 /* get strnlen() */
#include <string.h>
#include <time.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "e2fsck.h"
#include <ext2fs/ext2_ext_attr.h>

#include "problem.h"

#ifdef NO_INLINE_FUNCS
#define _INLINE_
#else
#define _INLINE_ inline
#endif

static int process_block(ext2_filsys fs, blk_t	*blocknr,
			 e2_blkcnt_t blockcnt, blk_t ref_blk, 
			 int ref_offset, void *priv_data);
static int process_bad_block(ext2_filsys fs, blk_t *block_nr,
			     e2_blkcnt_t blockcnt, blk_t ref_blk,
			     int ref_offset, void *priv_data);
static void check_blocks(e2fsck_t ctx, struct problem_context *pctx,
			 char *block_buf);
static void mark_table_blocks(e2fsck_t ctx);
static void alloc_bb_map(e2fsck_t ctx);
static void alloc_imagic_map(e2fsck_t ctx);
static void mark_inode_bad(e2fsck_t ctx, ino_t ino);
static void handle_fs_bad_blocks(e2fsck_t ctx);
static void process_inodes(e2fsck_t ctx, char *block_buf);
static EXT2_QSORT_TYPE process_inode_cmp(const void *a, const void *b);
static errcode_t scan_callback(ext2_filsys fs, ext2_inode_scan scan,
				  dgrp_t group, void * priv_data);
static void adjust_extattr_refcount(e2fsck_t ctx, ext2_refcount_t refcount, 
				    char *block_buf, int adjust_sign);
/* static char *describe_illegal_block(ext2_filsys fs, blk_t block); */

extern void e2fsck_write_inode_full(e2fsck_t ctx, unsigned long ino,
			       struct ext2_inode * inode, int bufsize,
			       const char *proc);

struct process_block_struct {
	ext2_ino_t	ino;
	unsigned	is_dir:1, is_reg:1, clear:1, suppress:1,
				fragmented:1, compressed:1, bbcheck:1;
	blk_t		num_blocks;
	blk_t		max_blocks;
	e2_blkcnt_t	last_block;
	int		num_illegal_blocks;
	blk_t		previous_block;
	struct ext2_inode *inode;
	struct problem_context *pctx;
	ext2fs_block_bitmap fs_meta_blocks;
	e2fsck_t	ctx;
};

struct process_inode_block {
	ext2_ino_t ino;
	struct ext2_inode inode;
};

struct scan_callback_struct {
	e2fsck_t	ctx;
	char		*block_buf;
};

/*
 * For the inodes to process list.
 */
static struct process_inode_block *inodes_to_process;
static int process_inode_count;

static __u64 ext2_max_sizes[EXT2_MAX_BLOCK_LOG_SIZE -
			    EXT2_MIN_BLOCK_LOG_SIZE + 1];

/*
 * Free all memory allocated by pass1 in preparation for restarting
 * things.
 */
static void unwind_pass1(ext2_filsys fs EXT2FS_ATTR((unused)))
{
	ext2fs_free_mem(&inodes_to_process);
	inodes_to_process = 0;
}

/*
 * Check to make sure a device inode is real.  Returns 1 if the device
 * checks out, 0 if not.
 *
 * Note: this routine is now also used to check FIFO's and Sockets,
 * since they have the same requirement; the i_block fields should be
 * zero. 
 */
int e2fsck_pass1_check_device_inode(ext2_filsys fs, struct ext2_inode *inode)
{
	int	i;

	/*
	 * If i_blocks is non-zero, or the index flag is set, then
	 * this is a bogus device/fifo/socket
	 */
	if ((ext2fs_inode_data_blocks(fs, inode) != 0) ||
	    (inode->i_flags & EXT2_INDEX_FL))
		return 0;

	/*
	 * We should be able to do the test below all the time, but
	 * because the kernel doesn't forcibly clear the device
	 * inode's additional i_block fields, there are some rare
	 * occasions when a legitimate device inode will have non-zero
	 * additional i_block fields.  So for now, we only complain
	 * when the immutable flag is set, which should never happen
	 * for devices.  (And that's when the problem is caused, since
	 * you can't set or clear immutable flags for devices.)  Once
	 * the kernel has been fixed we can change this...
	 */
	if (inode->i_flags & (EXT2_IMMUTABLE_FL | EXT2_APPEND_FL)) {
		for (i=4; i < EXT2_N_BLOCKS; i++) 
			if (inode->i_block[i])
				return 0;
	}
	return 1;
}

/*
 * Check to make sure a symlink inode is real.  Returns 1 if the symlink
 * checks out, 0 if not.
 */
int e2fsck_pass1_check_symlink(ext2_filsys fs, struct ext2_inode *inode,
			       char *buf)
{
	unsigned int len;
	int i;
	blk_t	blocks;

	if ((inode->i_size_high || inode->i_size == 0) ||
	    (inode->i_flags & EXT2_INDEX_FL))
		return 0;

	blocks = ext2fs_inode_data_blocks(fs, inode);
	if (blocks) {
		if ((inode->i_size >= fs->blocksize) ||
		    (blocks != fs->blocksize >> 9) ||
		    (inode->i_block[0] < fs->super->s_first_data_block) ||
		    (inode->i_block[0] >= fs->super->s_blocks_count))
			return 0;

		for (i = 1; i < EXT2_N_BLOCKS; i++)
			if (inode->i_block[i])
				return 0;

		if (io_channel_read_blk(fs->io, inode->i_block[0], 1, buf))
			return 0;

		len = strnlen(buf, fs->blocksize);
		if (len == fs->blocksize)
			return 0;
	} else {
		if (inode->i_size >= sizeof(inode->i_block))
			return 0;

		len = strnlen((char *)inode->i_block, sizeof(inode->i_block));
		if (len == sizeof(inode->i_block))
			return 0;
	}
	if (len != inode->i_size)
		return 0;
	return 1;
}

/*
 * If the immutable (or append-only) flag is set on the inode, offer
 * to clear it.
 */
#define BAD_SPECIAL_FLAGS (EXT2_IMMUTABLE_FL | EXT2_APPEND_FL)
static void check_immutable(e2fsck_t ctx, struct problem_context *pctx)
{
	if (!(pctx->inode->i_flags & BAD_SPECIAL_FLAGS))
		return;

	if (!fix_problem(ctx, PR_1_SET_IMMUTABLE, pctx))
		return;

	pctx->inode->i_flags &= ~BAD_SPECIAL_FLAGS;
	e2fsck_write_inode(ctx, pctx->ino, pctx->inode, "pass1");
}

/*
 * If device, fifo or socket, check size is zero -- if not offer to
 * clear it
 */
static void check_size(e2fsck_t ctx, struct problem_context *pctx)
{
	struct ext2_inode *inode = pctx->inode;
	
	if ((inode->i_size == 0) && (inode->i_size_high == 0))
		return;
	
	if (!fix_problem(ctx, PR_1_SET_NONZSIZE, pctx))
		return;
	
	inode->i_size = 0;
	inode->i_size_high = 0;
	e2fsck_write_inode(ctx, pctx->ino, pctx->inode, "pass1");
}
	
static void check_ea_in_inode(e2fsck_t ctx, struct problem_context *pctx)
{
	struct ext2_super_block *sb = ctx->fs->super;
	struct ext2_inode_large *inode;
	struct ext2_ext_attr_entry *entry;
	char *start, *end;
	int storage_size, remain, offs;
	int problem = 0;

	inode = (struct ext2_inode_large *) pctx->inode;
	storage_size = EXT2_INODE_SIZE(ctx->fs->super) - EXT2_GOOD_OLD_INODE_SIZE -
		inode->i_extra_isize;
	start = ((char *) inode) + EXT2_GOOD_OLD_INODE_SIZE +
		inode->i_extra_isize + sizeof(__u32);
	end = (char *) inode + EXT2_INODE_SIZE(ctx->fs->super);
	entry = (struct ext2_ext_attr_entry *) start;

	/* scan all entry's headers first */

	/* take finish entry 0UL into account */
	remain = storage_size - sizeof(__u32); 
	offs = end - start;

	while (!EXT2_EXT_IS_LAST_ENTRY(entry)) {

		/* header eats this space */
		remain -= sizeof(struct ext2_ext_attr_entry);
		
		/* is attribute name valid? */
		if (EXT2_EXT_ATTR_SIZE(entry->e_name_len) > remain) {
			pctx->num = entry->e_name_len;
			problem = PR_1_ATTR_NAME_LEN;
			goto fix;
		}

		/* attribute len eats this space */
		remain -= EXT2_EXT_ATTR_SIZE(entry->e_name_len);

		/* check value size */
		if (entry->e_value_size == 0 || entry->e_value_size > remain) {
			pctx->num = entry->e_value_size;
			problem = PR_1_ATTR_VALUE_SIZE;
			goto fix;
		}

		/* check value placement */
		if (entry->e_value_offs + 
		    EXT2_XATTR_SIZE(entry->e_value_size) != offs) {
			printf("(entry->e_value_offs + entry->e_value_size: %d, offs: %d)\n", entry->e_value_offs + entry->e_value_size, offs);
			pctx->num = entry->e_value_offs;
			problem = PR_1_ATTR_VALUE_OFFSET;
			goto fix;
		}
	
		/* e_value_block must be 0 in inode's ea */
		if (entry->e_value_block != 0) {
			pctx->num = entry->e_value_block;
			problem = PR_1_ATTR_VALUE_BLOCK;
			goto fix;
		}
		
		/* e_hash must be 0 in inode's ea */
		if (entry->e_hash != 0) {
			pctx->num = entry->e_hash;
			problem = PR_1_ATTR_HASH;
			goto fix;
		}

		remain -= entry->e_value_size;
		offs -= EXT2_XATTR_SIZE(entry->e_value_size);

		entry = EXT2_EXT_ATTR_NEXT(entry);
	}
fix:
	/*
	 * it seems like a corruption. it's very unlikely we could repair
	 * EA(s) in automatic fashion -bzzz
	 */
#if 0
	problem = PR_1_ATTR_HASH;
#endif
	if (problem == 0 || !fix_problem(ctx, problem, pctx))
		return;

	/* simple remove all possible EA(s) */
	*((__u32 *)start) = 0UL;
	e2fsck_write_inode_full(ctx, pctx->ino, (struct ext2_inode *)inode,
				EXT2_INODE_SIZE(sb), "pass1");
}

static void check_inode_extra_space(e2fsck_t ctx, struct problem_context *pctx)
{
	struct ext2_super_block *sb = ctx->fs->super;
	struct ext2_inode_large *inode;
	__u32 *eamagic;
	int min, max;

	inode = (struct ext2_inode_large *) pctx->inode;
	if (EXT2_INODE_SIZE(sb) == EXT2_GOOD_OLD_INODE_SIZE) {
		/* this isn't large inode. so, nothing to check */
		return;
	}

#if 0
	printf("inode #%u, i_extra_size %d\n", pctx->ino,
			inode->i_extra_isize);
#endif	
	/* i_extra_isize must cover i_extra_isize + i_pad1 at least */
	min = sizeof(inode->i_extra_isize) + sizeof(inode->i_pad1);
	max = EXT2_INODE_SIZE(sb) - EXT2_GOOD_OLD_INODE_SIZE;
	/* 
	 * For now we will allow i_extra_isize to be 0, but really
	 * implementations should never allow i_extra_isize to be 0
	 */
	if (inode->i_extra_isize &&
	    (inode->i_extra_isize < min || inode->i_extra_isize > max)) {
		if (!fix_problem(ctx, PR_1_EXTRA_ISIZE, pctx))
			return;
		inode->i_extra_isize = min;
		e2fsck_write_inode_full(ctx, pctx->ino, pctx->inode,
					EXT2_INODE_SIZE(sb), "pass1");
		return;
	}

	eamagic = (__u32 *) (((char *) inode) + EXT2_GOOD_OLD_INODE_SIZE +
			inode->i_extra_isize);
	if (*eamagic == EXT2_EXT_ATTR_MAGIC) {
		/* it seems inode has an extended attribute(s) in body */
		check_ea_in_inode(ctx, pctx);
	}
}

void e2fsck_pass1(e2fsck_t ctx)
{
	int	i;
	__u64	max_sizes;
	ext2_filsys fs = ctx->fs;
	ext2_ino_t	ino;
	struct ext2_inode *inode;
	ext2_inode_scan	scan;
	char		*block_buf;
#ifdef RESOURCE_TRACK
	struct resource_track	rtrack;
#endif
	unsigned char	frag, fsize;
	struct		problem_context pctx;
	struct		scan_callback_struct scan_struct;
	struct ext2_super_block *sb = ctx->fs->super;
	int		imagic_fs;
	int		busted_fs_time = 0;
	int		inode_size;
	
#ifdef RESOURCE_TRACK
	init_resource_track(&rtrack);
#endif
	clear_problem_context(&pctx);

	if (!(ctx->options & E2F_OPT_PREEN))
		fix_problem(ctx, PR_1_PASS_HEADER, &pctx);

	if ((fs->super->s_feature_compat & EXT2_FEATURE_COMPAT_DIR_INDEX) &&
	    !(ctx->options & E2F_OPT_NO)) {
		if (ext2fs_u32_list_create(&ctx->dirs_to_hash, 50))
			ctx->dirs_to_hash = 0;
	}

#ifdef MTRACE
	mtrace_print("Pass 1");
#endif

#define EXT2_BPP(bits) (1ULL << ((bits) - 2))

	for (i = EXT2_MIN_BLOCK_LOG_SIZE; i <= EXT2_MAX_BLOCK_LOG_SIZE; i++) {
		max_sizes = EXT2_NDIR_BLOCKS + EXT2_BPP(i);
		max_sizes = max_sizes + EXT2_BPP(i) * EXT2_BPP(i);
		max_sizes = max_sizes + EXT2_BPP(i) * EXT2_BPP(i) * EXT2_BPP(i);
		max_sizes = (max_sizes * (1UL << i)) - 1;
		ext2_max_sizes[i - EXT2_MIN_BLOCK_LOG_SIZE] = max_sizes;
	}
#undef EXT2_BPP

	imagic_fs = (sb->s_feature_compat & EXT2_FEATURE_COMPAT_IMAGIC_INODES);

	/*
	 * Allocate bitmaps structures
	 */
	pctx.errcode = ext2fs_allocate_inode_bitmap(fs, _("in-use inode map"),
					      &ctx->inode_used_map);
	if (pctx.errcode) {
		pctx.num = 1;
		fix_problem(ctx, PR_1_ALLOCATE_IBITMAP_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
	pctx.errcode = ext2fs_allocate_inode_bitmap(fs,
				_("directory inode map"), &ctx->inode_dir_map);
	if (pctx.errcode) {
		pctx.num = 2;
		fix_problem(ctx, PR_1_ALLOCATE_IBITMAP_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
	pctx.errcode = ext2fs_allocate_inode_bitmap(fs,
			_("regular file inode map"), &ctx->inode_reg_map);
	if (pctx.errcode) {
		pctx.num = 6;
		fix_problem(ctx, PR_1_ALLOCATE_IBITMAP_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
	pctx.errcode = ext2fs_allocate_block_bitmap(fs, _("in-use block map"),
					      &ctx->block_found_map);
	if (pctx.errcode) {
		pctx.num = 1;
		fix_problem(ctx, PR_1_ALLOCATE_BBITMAP_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
	pctx.errcode = ext2fs_create_icount2(fs, 0, 0, 0,
					     &ctx->inode_link_info);
	if (pctx.errcode) {
		fix_problem(ctx, PR_1_ALLOCATE_ICOUNT, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
	inode_size = EXT2_INODE_SIZE(fs->super);
	inode = (struct ext2_inode *)
		e2fsck_allocate_memory(ctx, inode_size, "scratch inode");

	inodes_to_process = (struct process_inode_block *)
		e2fsck_allocate_memory(ctx,
				       (ctx->process_inode_size *
					sizeof(struct process_inode_block)),
				       "array of inodes to process");
	process_inode_count = 0;

	pctx.errcode = ext2fs_init_dblist(fs, 0);
	if (pctx.errcode) {
		fix_problem(ctx, PR_1_ALLOCATE_DBCOUNT, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}

	/*
	 * If the last orphan field is set, clear it, since the pass1
	 * processing will automatically find and clear the orphans.
	 * In the future, we may want to try using the last_orphan
	 * linked list ourselves, but for now, we clear it so that the
	 * ext3 mount code won't get confused.
	 */
	if (!(ctx->options & E2F_OPT_READONLY)) {
		if (fs->super->s_last_orphan) {
			fs->super->s_last_orphan = 0;
			ext2fs_mark_super_dirty(fs);
		}
	}

	mark_table_blocks(ctx);
	block_buf = (char *) e2fsck_allocate_memory(ctx, fs->blocksize * 3,
						    "block interate buffer");
	e2fsck_use_inode_shortcuts(ctx, 1);
	ehandler_operation(_("doing inode scan"));
	pctx.errcode = ext2fs_open_inode_scan(fs, ctx->inode_buffer_blocks, 
					      &scan);
	if (pctx.errcode) {
		fix_problem(ctx, PR_1_ISCAN_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
	ext2fs_inode_scan_flags(scan, EXT2_SF_SKIP_MISSING_ITABLE, 0);
	ctx->stashed_inode = inode;
	scan_struct.ctx = ctx;
	scan_struct.block_buf = block_buf;
	ext2fs_set_inode_callback(scan, scan_callback, &scan_struct);
	if (ctx->progress)
		if ((ctx->progress)(ctx, 1, 0, ctx->fs->group_desc_count))
			return;
	if (fs->super->s_wtime < fs->super->s_inodes_count)
		busted_fs_time = 1;

	while (1) {
		pctx.errcode = ext2fs_get_next_inode_full(scan, &ino, 
							  inode, inode_size);
		if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
			return;
		if (pctx.errcode == EXT2_ET_BAD_BLOCK_IN_INODE_TABLE) {
			if (!ctx->inode_bb_map)
				alloc_bb_map(ctx);
			ext2fs_mark_inode_bitmap(ctx->inode_bb_map, ino);
			ext2fs_mark_inode_bitmap(ctx->inode_used_map, ino);
			continue;
		}
		if (pctx.errcode) {
			fix_problem(ctx, PR_1_ISCAN_ERROR, &pctx);
			ctx->flags |= E2F_FLAG_ABORT;
			return;
		}
		if (!ino)
			break;
		pctx.ino = ino;
		pctx.inode = inode;
		ctx->stashed_ino = ino;
		if (inode->i_links_count) {
			pctx.errcode = ext2fs_icount_store(ctx->inode_link_info, 
					   ino, inode->i_links_count);
			if (pctx.errcode) {
				pctx.num = inode->i_links_count;
				fix_problem(ctx, PR_1_ICOUNT_STORE, &pctx);
				ctx->flags |= E2F_FLAG_ABORT;
				return;
			}
		}
		if (ino == EXT2_BAD_INO) {
			struct process_block_struct pb;
			
			pctx.errcode = ext2fs_copy_bitmap(ctx->block_found_map,
							  &pb.fs_meta_blocks);
			if (pctx.errcode) {
				pctx.num = 4;
				fix_problem(ctx, PR_1_ALLOCATE_BBITMAP_ERROR, &pctx);
				ctx->flags |= E2F_FLAG_ABORT;
				return;
			}
			pb.ino = EXT2_BAD_INO;
			pb.num_blocks = pb.last_block = 0;
			pb.num_illegal_blocks = 0;
			pb.suppress = 0; pb.clear = 0; pb.is_dir = 0;
			pb.is_reg = 0; pb.fragmented = 0; pb.bbcheck = 0;
			pb.inode = inode;
			pb.pctx = &pctx;
			pb.ctx = ctx;
			pctx.errcode = ext2fs_block_iterate2(fs, ino, 0, 
				     block_buf, process_bad_block, &pb);
			ext2fs_free_block_bitmap(pb.fs_meta_blocks);
			if (pctx.errcode) {
				fix_problem(ctx, PR_1_BLOCK_ITERATE, &pctx);
				ctx->flags |= E2F_FLAG_ABORT;
				return;
			}
			if (pb.bbcheck)
				if (!fix_problem(ctx, PR_1_BBINODE_BAD_METABLOCK_PROMPT, &pctx)) {
				ctx->flags |= E2F_FLAG_ABORT;
				return;
			}
			ext2fs_mark_inode_bitmap(ctx->inode_used_map, ino);
			clear_problem_context(&pctx);
			continue;
		} else if (ino == EXT2_ROOT_INO) {
			/*
			 * Make sure the root inode is a directory; if
			 * not, offer to clear it.  It will be
			 * regnerated in pass #3.
			 */
			if (!LINUX_S_ISDIR(inode->i_mode)) {
				if (fix_problem(ctx, PR_1_ROOT_NO_DIR, &pctx)) {
					inode->i_dtime = time(0);
					inode->i_links_count = 0;
					ext2fs_icount_store(ctx->inode_link_info,
							    ino, 0);
					e2fsck_write_inode(ctx, ino, inode,
							   "pass1");
				}

			}
			/*
			 * If dtime is set, offer to clear it.  mke2fs
			 * version 0.2b created filesystems with the
			 * dtime field set for the root and lost+found
			 * directories.  We won't worry about
			 * /lost+found, since that can be regenerated
			 * easily.  But we will fix the root directory
			 * as a special case.
			 */
			if (inode->i_dtime && inode->i_links_count) {
				if (fix_problem(ctx, PR_1_ROOT_DTIME, &pctx)) {
					inode->i_dtime = 0;
					e2fsck_write_inode(ctx, ino, inode,
							   "pass1");
				}
			}
		} else if (ino == EXT2_JOURNAL_INO) {
			ext2fs_mark_inode_bitmap(ctx->inode_used_map, ino);
			if (fs->super->s_journal_inum == EXT2_JOURNAL_INO) {
				if (!LINUX_S_ISREG(inode->i_mode) &&
				    fix_problem(ctx, PR_1_JOURNAL_BAD_MODE,
						&pctx)) {
					inode->i_mode = LINUX_S_IFREG;
					e2fsck_write_inode(ctx, ino, inode,
							   "pass1");
				}
				check_blocks(ctx, &pctx, block_buf);
				continue;
			}
			if ((inode->i_links_count || inode->i_blocks ||
			     inode->i_blocks || inode->i_block[0]) &&
			    fix_problem(ctx, PR_1_JOURNAL_INODE_NOT_CLEAR, 
					&pctx)) {
				memset(inode, 0, inode_size);
				ext2fs_icount_store(ctx->inode_link_info,
						    ino, 0);
				e2fsck_write_inode_full(ctx, ino, inode, 
							inode_size, "pass1");
			}
		} else if (ino < EXT2_FIRST_INODE(fs->super)) {
			int	problem = 0;
			
			ext2fs_mark_inode_bitmap(ctx->inode_used_map, ino);
			if (ino == EXT2_BOOT_LOADER_INO) {
				if (LINUX_S_ISDIR(inode->i_mode))
					problem = PR_1_RESERVED_BAD_MODE;
			} else if (ino == EXT2_RESIZE_INO) {
				if (inode->i_mode &&
				    !LINUX_S_ISREG(inode->i_mode))
					problem = PR_1_RESERVED_BAD_MODE;
			} else {
				if (inode->i_mode != 0)
					problem = PR_1_RESERVED_BAD_MODE;
			}
			if (problem) {
				if (fix_problem(ctx, problem, &pctx)) {
					inode->i_mode = 0;
					e2fsck_write_inode(ctx, ino, inode,
							   "pass1");
				}
			}
			check_blocks(ctx, &pctx, block_buf);
			continue;
		}
		/*
		 * Check for inodes who might have been part of the
		 * orphaned list linked list.  They should have gotten
		 * dealt with by now, unless the list had somehow been
		 * corrupted.
		 * 
		 * FIXME: In the future, inodes which are still in use
		 * (and which are therefore) pending truncation should
		 * be handled specially.  Right now we just clear the
		 * dtime field, and the normal e2fsck handling of
		 * inodes where i_size and the inode blocks are
		 * inconsistent is to fix i_size, instead of releasing
		 * the extra blocks.  This won't catch the inodes that
		 * was at the end of the orphan list, but it's better
		 * than nothing.  The right answer is that there
		 * shouldn't be any bugs in the orphan list handling.  :-)
		 */
		if (inode->i_dtime && !busted_fs_time &&
		    inode->i_dtime < ctx->fs->super->s_inodes_count) {
			if (fix_problem(ctx, PR_1_LOW_DTIME, &pctx)) {
				inode->i_dtime = inode->i_links_count ?
					0 : time(0);
				e2fsck_write_inode(ctx, ino, inode,
						   "pass1");
			}
		}
		
		/*
		 * This code assumes that deleted inodes have
		 * i_links_count set to 0.  
		 */
		if (!inode->i_links_count) {
			if (!inode->i_dtime && inode->i_mode) {
				if (fix_problem(ctx,
					    PR_1_ZERO_DTIME, &pctx)) {
					inode->i_dtime = time(0);
					e2fsck_write_inode(ctx, ino, inode,
							   "pass1");
				}
			}
			continue;
		}
		/*
		 * n.b.  0.3c ext2fs code didn't clear i_links_count for
		 * deleted files.  Oops.
		 *
		 * Since all new ext2 implementations get this right,
		 * we now assume that the case of non-zero
		 * i_links_count and non-zero dtime means that we
		 * should keep the file, not delete it.
		 * 
		 */
		if (inode->i_dtime) {
			if (fix_problem(ctx, PR_1_SET_DTIME, &pctx)) {
				inode->i_dtime = 0;
				e2fsck_write_inode(ctx, ino, inode, "pass1");
			}
		}
		
		ext2fs_mark_inode_bitmap(ctx->inode_used_map, ino);
		switch (fs->super->s_creator_os) {
		    case EXT2_OS_LINUX:
			frag = inode->osd2.linux2.l_i_frag;
			fsize = inode->osd2.linux2.l_i_fsize;
			break;
		    case EXT2_OS_HURD:
			frag = inode->osd2.hurd2.h_i_frag;
			fsize = inode->osd2.hurd2.h_i_fsize;
			break;
		    case EXT2_OS_MASIX:
			frag = inode->osd2.masix2.m_i_frag;
			fsize = inode->osd2.masix2.m_i_fsize;
			break;
		    default:
			frag = fsize = 0;
		}
		
		if (inode->i_faddr || frag || fsize ||
		    (LINUX_S_ISDIR(inode->i_mode) && inode->i_dir_acl))
			mark_inode_bad(ctx, ino);
		if (inode->i_flags & EXT2_IMAGIC_FL) {
			if (imagic_fs) {
				if (!ctx->inode_imagic_map)
					alloc_imagic_map(ctx);
				ext2fs_mark_inode_bitmap(ctx->inode_imagic_map,
							 ino);
			} else {
				if (fix_problem(ctx, PR_1_SET_IMAGIC, &pctx)) {
					inode->i_flags &= ~EXT2_IMAGIC_FL;
					e2fsck_write_inode(ctx, ino,
							   inode, "pass1");
				}
			}
		}

		check_inode_extra_space(ctx, &pctx);

		if (LINUX_S_ISDIR(inode->i_mode)) {
			ext2fs_mark_inode_bitmap(ctx->inode_dir_map, ino);
			e2fsck_add_dir_info(ctx, ino, 0);
			ctx->fs_directory_count++;
		} else if (LINUX_S_ISREG (inode->i_mode)) {
			ext2fs_mark_inode_bitmap(ctx->inode_reg_map, ino);
			ctx->fs_regular_count++;
		} else if (LINUX_S_ISCHR (inode->i_mode) &&
			   e2fsck_pass1_check_device_inode(fs, inode)) {
			check_immutable(ctx, &pctx);
			check_size(ctx, &pctx);
			ctx->fs_chardev_count++;
		} else if (LINUX_S_ISBLK (inode->i_mode) &&
			   e2fsck_pass1_check_device_inode(fs, inode)) {
			check_immutable(ctx, &pctx);
			check_size(ctx, &pctx);
			ctx->fs_blockdev_count++;
		} else if (LINUX_S_ISLNK (inode->i_mode) &&
			   e2fsck_pass1_check_symlink(fs, inode, block_buf)) {
			check_immutable(ctx, &pctx);
			ctx->fs_symlinks_count++;
			if (ext2fs_inode_data_blocks(fs, inode) == 0) {
				ctx->fs_fast_symlinks_count++;
				check_blocks(ctx, &pctx, block_buf);
				continue;
			}
		}
		else if (LINUX_S_ISFIFO (inode->i_mode) &&
			 e2fsck_pass1_check_device_inode(fs, inode)) {
			check_immutable(ctx, &pctx);
			check_size(ctx, &pctx);
			ctx->fs_fifo_count++;
		} else if ((LINUX_S_ISSOCK (inode->i_mode)) &&
			   e2fsck_pass1_check_device_inode(fs, inode)) {
			check_immutable(ctx, &pctx);
			check_size(ctx, &pctx);
			ctx->fs_sockets_count++;
		} else
			mark_inode_bad(ctx, ino);
		if (inode->i_block[EXT2_IND_BLOCK])
			ctx->fs_ind_count++;
		if (inode->i_block[EXT2_DIND_BLOCK])
			ctx->fs_dind_count++;
		if (inode->i_block[EXT2_TIND_BLOCK])
			ctx->fs_tind_count++;
		if (inode->i_block[EXT2_IND_BLOCK] ||
		    inode->i_block[EXT2_DIND_BLOCK] ||
		    inode->i_block[EXT2_TIND_BLOCK] ||
		    inode->i_file_acl) {
			inodes_to_process[process_inode_count].ino = ino;
			inodes_to_process[process_inode_count].inode = *inode;
			process_inode_count++;
		} else
			check_blocks(ctx, &pctx, block_buf);

		if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
			return;

		if (process_inode_count >= ctx->process_inode_size) {
			process_inodes(ctx, block_buf);

			if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
				return;
		}
	}
	process_inodes(ctx, block_buf);
	ext2fs_close_inode_scan(scan);
	ehandler_operation(0);

	/*
	 * If any extended attribute blocks' reference counts need to
	 * be adjusted, either up (ctx->refcount_extra), or down
	 * (ctx->refcount), then fix them.
	 */
	if (ctx->refcount) {
		adjust_extattr_refcount(ctx, ctx->refcount, block_buf, -1);
		ea_refcount_free(ctx->refcount);
		ctx->refcount = 0;
	}
	if (ctx->refcount_extra) {
		adjust_extattr_refcount(ctx, ctx->refcount_extra,
					block_buf, +1);
		ea_refcount_free(ctx->refcount_extra);
		ctx->refcount_extra = 0;
	}
		
	if (ctx->invalid_bitmaps)
		handle_fs_bad_blocks(ctx);

	/* We don't need the block_ea_map any more */
	if (ctx->block_ea_map) {
		ext2fs_free_block_bitmap(ctx->block_ea_map);
		ctx->block_ea_map = 0;
	}

	if (ctx->flags & E2F_FLAG_RESIZE_INODE) {
		ext2fs_block_bitmap save_bmap;

		save_bmap = fs->block_map;
		fs->block_map = ctx->block_found_map;
		clear_problem_context(&pctx);
		pctx.errcode = ext2fs_create_resize_inode(fs);
		if (pctx.errcode) {
			fix_problem(ctx, PR_1_RESIZE_INODE_CREATE, &pctx);
			/* Should never get here */
			ctx->flags |= E2F_FLAG_ABORT;
			return;
		}
		fs->block_map = save_bmap;
		ctx->flags &= ~E2F_FLAG_RESIZE_INODE;
	}
		       
	if (ctx->flags & E2F_FLAG_RESTART) {
		/*
		 * Only the master copy of the superblock and block
		 * group descriptors are going to be written during a
		 * restart, so set the superblock to be used to be the
		 * master superblock.
		 */
		ctx->use_superblock = 0;
		unwind_pass1(fs);
		goto endit;
	}

	if (ctx->block_dup_map) {
		if (ctx->options & E2F_OPT_PREEN) {
			clear_problem_context(&pctx);
			fix_problem(ctx, PR_1_DUP_BLOCKS_PREENSTOP, &pctx);
		}
		e2fsck_pass1_dupblocks(ctx, block_buf);
	}
	ext2fs_free_mem(&inodes_to_process);
endit:
	e2fsck_use_inode_shortcuts(ctx, 0);
	
	ext2fs_free_mem(&block_buf);
	ext2fs_free_mem(&inode);

#ifdef RESOURCE_TRACK
	if (ctx->options & E2F_OPT_TIME2) {
		e2fsck_clear_progbar(ctx);
		print_resource_track(_("Pass 1"), &rtrack);
	}
#endif
}

/*
 * When the inode_scan routines call this callback at the end of the
 * glock group, call process_inodes.
 */
static errcode_t scan_callback(ext2_filsys fs, 
			       ext2_inode_scan scan EXT2FS_ATTR((unused)),
			       dgrp_t group, void * priv_data)
{
	struct scan_callback_struct *scan_struct;
	e2fsck_t ctx;

	scan_struct = (struct scan_callback_struct *) priv_data;
	ctx = scan_struct->ctx;
	
	process_inodes((e2fsck_t) fs->priv_data, scan_struct->block_buf);

	if (ctx->progress)
		if ((ctx->progress)(ctx, 1, group+1,
				    ctx->fs->group_desc_count))
			return EXT2_ET_CANCEL_REQUESTED;

	return 0;
}

/*
 * Process the inodes in the "inodes to process" list.
 */
static void process_inodes(e2fsck_t ctx, char *block_buf)
{
	int			i;
	struct ext2_inode	*old_stashed_inode;
	ext2_ino_t		old_stashed_ino;
	const char		*old_operation;
	char			buf[80];
	struct problem_context	pctx;
	
#if 0
	printf("begin process_inodes: ");
#endif
	if (process_inode_count == 0)
		return;
	old_operation = ehandler_operation(0);
	old_stashed_inode = ctx->stashed_inode;
	old_stashed_ino = ctx->stashed_ino;
	qsort(inodes_to_process, process_inode_count,
		      sizeof(struct process_inode_block), process_inode_cmp);
	clear_problem_context(&pctx);
	for (i=0; i < process_inode_count; i++) {
		pctx.inode = ctx->stashed_inode = &inodes_to_process[i].inode;
		pctx.ino = ctx->stashed_ino = inodes_to_process[i].ino;
		
#if 0
		printf("%u ", pctx.ino);
#endif
		sprintf(buf, _("reading indirect blocks of inode %u"),
			pctx.ino);
		ehandler_operation(buf);
		check_blocks(ctx, &pctx, block_buf);
		if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
			break;
	}
	ctx->stashed_inode = old_stashed_inode;
	ctx->stashed_ino = old_stashed_ino;
	process_inode_count = 0;
#if 0
	printf("end process inodes\n");
#endif
	ehandler_operation(old_operation);
}

static EXT2_QSORT_TYPE process_inode_cmp(const void *a, const void *b)
{
	const struct process_inode_block *ib_a =
		(const struct process_inode_block *) a;
	const struct process_inode_block *ib_b =
		(const struct process_inode_block *) b;
	int	ret;
	
	ret = (ib_a->inode.i_block[EXT2_IND_BLOCK] -
	       ib_b->inode.i_block[EXT2_IND_BLOCK]);
	if (ret == 0)
		ret = ib_a->inode.i_file_acl - ib_b->inode.i_file_acl;
	return ret;
}

/*
 * Mark an inode as being bad in some what
 */
static void mark_inode_bad(e2fsck_t ctx, ino_t ino)
{
	struct		problem_context pctx;

	if (!ctx->inode_bad_map) {
		clear_problem_context(&pctx);
	
		pctx.errcode = ext2fs_allocate_inode_bitmap(ctx->fs,
			    _("bad inode map"), &ctx->inode_bad_map);
		if (pctx.errcode) {
			pctx.num = 3;
			fix_problem(ctx, PR_1_ALLOCATE_IBITMAP_ERROR, &pctx);
			/* Should never get here */
			ctx->flags |= E2F_FLAG_ABORT;
			return;
		}
	}
	ext2fs_mark_inode_bitmap(ctx->inode_bad_map, ino);
}


/*
 * This procedure will allocate the inode "bb" (badblock) map table
 */
static void alloc_bb_map(e2fsck_t ctx)
{
	struct		problem_context pctx;
	
	clear_problem_context(&pctx);
	pctx.errcode = ext2fs_allocate_inode_bitmap(ctx->fs,
					      _("inode in bad block map"),
					      &ctx->inode_bb_map);
	if (pctx.errcode) {
		pctx.num = 4;
		fix_problem(ctx, PR_1_ALLOCATE_IBITMAP_ERROR, &pctx);
		/* Should never get here */
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
}

/*
 * This procedure will allocate the inode imagic table
 */
static void alloc_imagic_map(e2fsck_t ctx)
{
	struct		problem_context pctx;
	
	clear_problem_context(&pctx);
	pctx.errcode = ext2fs_allocate_inode_bitmap(ctx->fs,
					      _("imagic inode map"),
					      &ctx->inode_imagic_map);
	if (pctx.errcode) {
		pctx.num = 5;
		fix_problem(ctx, PR_1_ALLOCATE_IBITMAP_ERROR, &pctx);
		/* Should never get here */
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
}

/*
 * Marks a block as in use, setting the dup_map if it's been set
 * already.  Called by process_block and process_bad_block.
 *
 * WARNING: Assumes checks have already been done to make sure block
 * is valid.  This is true in both process_block and process_bad_block.
 */
static _INLINE_ void mark_block_used(e2fsck_t ctx, blk_t block)
{
	struct		problem_context pctx;
	
	clear_problem_context(&pctx);
	
	if (ext2fs_fast_test_block_bitmap(ctx->block_found_map, block)) {
		if (!ctx->block_dup_map) {
			pctx.errcode = ext2fs_allocate_block_bitmap(ctx->fs,
			      _("multiply claimed block map"),
			      &ctx->block_dup_map);
			if (pctx.errcode) {
				pctx.num = 3;
				fix_problem(ctx, PR_1_ALLOCATE_BBITMAP_ERROR, 
					    &pctx);
				/* Should never get here */
				ctx->flags |= E2F_FLAG_ABORT;
				return;
			}
		}
		ext2fs_fast_mark_block_bitmap(ctx->block_dup_map, block);
	} else {
		ext2fs_fast_mark_block_bitmap(ctx->block_found_map, block);
	}
}

/*
 * Adjust the extended attribute block's reference counts at the end
 * of pass 1, either by subtracting out references for EA blocks that
 * are still referenced in ctx->refcount, or by adding references for
 * EA blocks that had extra references as accounted for in
 * ctx->refcount_extra.
 */
static void adjust_extattr_refcount(e2fsck_t ctx, ext2_refcount_t refcount, 
				    char *block_buf, int adjust_sign)
{
	struct ext2_ext_attr_header 	*header;
	struct problem_context		pctx;
	ext2_filsys			fs = ctx->fs;
	blk_t				blk;
	__u32				should_be;
	int				count;

	clear_problem_context(&pctx);
	
	ea_refcount_intr_begin(refcount);
	while (1) {
		if ((blk = ea_refcount_intr_next(refcount, &count)) == 0)
			break;
		pctx.blk = blk;
		pctx.errcode = ext2fs_read_ext_attr(fs, blk, block_buf);
		if (pctx.errcode) {
			fix_problem(ctx, PR_1_EXTATTR_READ_ABORT, &pctx);
			return;
		}
		header = (struct ext2_ext_attr_header *) block_buf;
		pctx.blkcount = header->h_refcount;
		should_be = header->h_refcount + adjust_sign * count;
		pctx.num = should_be;
		if (fix_problem(ctx, PR_1_EXTATTR_REFCOUNT, &pctx)) {
			header->h_refcount = should_be;
			pctx.errcode = ext2fs_write_ext_attr(fs, blk,
							     block_buf);
			if (pctx.errcode) {
				fix_problem(ctx, PR_1_EXTATTR_WRITE, &pctx);
				continue;
			}
		}
	}
}

/*
 * Handle processing the extended attribute blocks
 */
static int check_ext_attr(e2fsck_t ctx, struct problem_context *pctx,
			   char *block_buf)
{
	ext2_filsys fs = ctx->fs;
	ext2_ino_t	ino = pctx->ino;
	struct ext2_inode *inode = pctx->inode;
	blk_t		blk;
	char *		end;
	struct ext2_ext_attr_header *header;
	struct ext2_ext_attr_entry *entry;
	int		count;
	region_t	region;
	
	blk = inode->i_file_acl;
	if (blk == 0)
		return 0;

	/*
	 * If the Extended attribute flag isn't set, then a non-zero
	 * file acl means that the inode is corrupted.
	 *
	 * Or if the extended attribute block is an invalid block,
	 * then the inode is also corrupted.
	 */
	if (!(fs->super->s_feature_compat & EXT2_FEATURE_COMPAT_EXT_ATTR) ||
	    (blk < fs->super->s_first_data_block) ||
	    (blk >= fs->super->s_blocks_count)) {
		mark_inode_bad(ctx, ino);
		return 0;
	}

	/* If ea bitmap hasn't been allocated, create it */
	if (!ctx->block_ea_map) {
		pctx->errcode = ext2fs_allocate_block_bitmap(fs,
						      _("ext attr block map"),
						      &ctx->block_ea_map);
		if (pctx->errcode) {
			pctx->num = 2;
			fix_problem(ctx, PR_1_ALLOCATE_BBITMAP_ERROR, pctx);
			ctx->flags |= E2F_FLAG_ABORT;
			return 0;
		}
	}

	/* Create the EA refcount structure if necessary */
	if (!ctx->refcount) {
		pctx->errcode = ea_refcount_create(0, &ctx->refcount);
		if (pctx->errcode) {
			pctx->num = 1;
			fix_problem(ctx, PR_1_ALLOCATE_REFCOUNT, pctx);
			ctx->flags |= E2F_FLAG_ABORT;
			return 0;
		}
	}

#if 0
	/* Debugging text */
	printf("Inode %u has EA block %u\n", ino, blk);
#endif

	/* Have we seen this EA block before? */
	if (ext2fs_fast_test_block_bitmap(ctx->block_ea_map, blk)) {
		if (ea_refcount_decrement(ctx->refcount, blk, 0) == 0)
			return 1;
		/* Ooops, this EA was referenced more than it stated */
		if (!ctx->refcount_extra) {
			pctx->errcode = ea_refcount_create(0,
					   &ctx->refcount_extra);
			if (pctx->errcode) {
				pctx->num = 2;
				fix_problem(ctx, PR_1_ALLOCATE_REFCOUNT, pctx);
				ctx->flags |= E2F_FLAG_ABORT;
				return 0;
			}
		}
		ea_refcount_increment(ctx->refcount_extra, blk, 0);
		return 1;
	}
	
	/*
	 * OK, we haven't seen this EA block yet.  So we need to
	 * validate it
	 */
	pctx->blk = blk;
	pctx->errcode = ext2fs_read_ext_attr(fs, blk, block_buf);
	if (pctx->errcode && fix_problem(ctx, PR_1_READ_EA_BLOCK, pctx))
		goto clear_extattr;
	header = (struct ext2_ext_attr_header *) block_buf;
	pctx->blk = inode->i_file_acl;
	if (((ctx->ext_attr_ver == 1) &&
	     (header->h_magic != EXT2_EXT_ATTR_MAGIC_v1)) ||
	    ((ctx->ext_attr_ver == 2) &&
	     (header->h_magic != EXT2_EXT_ATTR_MAGIC))) {
		if (fix_problem(ctx, PR_1_BAD_EA_BLOCK, pctx))
			goto clear_extattr;
	}

	if (header->h_blocks != 1) {
		if (fix_problem(ctx, PR_1_EA_MULTI_BLOCK, pctx))
			goto clear_extattr;
	}

	region = region_create(0, fs->blocksize);
	if (!region) {
		fix_problem(ctx, PR_1_EA_ALLOC_REGION, pctx);
		ctx->flags |= E2F_FLAG_ABORT;
		return 0;
	}
	if (region_allocate(region, 0, sizeof(struct ext2_ext_attr_header))) {
		if (fix_problem(ctx, PR_1_EA_ALLOC_COLLISION, pctx))
			goto clear_extattr;
	}
	
	entry = (struct ext2_ext_attr_entry *)(header+1);
	end = block_buf + fs->blocksize;
	while ((char *)entry < end && *(__u32 *)entry) {
		if (region_allocate(region, (char *)entry - (char *)header,
			           EXT2_EXT_ATTR_LEN(entry->e_name_len))) {
			if (fix_problem(ctx, PR_1_EA_ALLOC_COLLISION, pctx))
				goto clear_extattr;
		}
		if ((ctx->ext_attr_ver == 1 &&
		     (entry->e_name_len == 0 || entry->e_name_index != 0)) ||
		    (ctx->ext_attr_ver == 2 &&
		     entry->e_name_index == 0)) {
			if (fix_problem(ctx, PR_1_EA_BAD_NAME, pctx))
				goto clear_extattr;
		}
		if (entry->e_value_block != 0) {
			if (fix_problem(ctx, PR_1_EA_BAD_VALUE, pctx))
				goto clear_extattr;
		}
		if (entry->e_value_size &&
		    region_allocate(region, entry->e_value_offs,
				    EXT2_EXT_ATTR_SIZE(entry->e_value_size))) {
			if (fix_problem(ctx, PR_1_EA_ALLOC_COLLISION, pctx))
				goto clear_extattr;
		}
		entry = EXT2_EXT_ATTR_NEXT(entry);
	}
	if (region_allocate(region, (char *)entry - (char *)header, 4)) {
		if (fix_problem(ctx, PR_1_EA_ALLOC_COLLISION, pctx))
			goto clear_extattr;
	}
	region_free(region);

	count = header->h_refcount - 1;
	if (count)
		ea_refcount_store(ctx->refcount, blk, count);
	mark_block_used(ctx, blk);
	ext2fs_fast_mark_block_bitmap(ctx->block_ea_map, blk);
	
	return 1;

clear_extattr:
	inode->i_file_acl = 0;
	e2fsck_write_inode(ctx, ino, inode, "check_ext_attr");
	return 0;
}

/* Returns 1 if bad htree, 0 if OK */
static int handle_htree(e2fsck_t ctx, struct problem_context *pctx,
			ext2_ino_t ino EXT2FS_ATTR((unused)),
			struct ext2_inode *inode,
			char *block_buf)
{
	struct ext2_dx_root_info	*root;
	ext2_filsys			fs = ctx->fs;
	errcode_t			retval;
	blk_t				blk;

	if ((!LINUX_S_ISDIR(inode->i_mode) &&
	     fix_problem(ctx, PR_1_HTREE_NODIR, pctx)) ||
	    (!(fs->super->s_feature_compat & EXT2_FEATURE_COMPAT_DIR_INDEX) &&
	     fix_problem(ctx, PR_1_HTREE_SET, pctx)))
		return 1;

	blk = inode->i_block[0];
	if (((blk == 0) ||
	     (blk < fs->super->s_first_data_block) ||
	     (blk >= fs->super->s_blocks_count)) &&
	    fix_problem(ctx, PR_1_HTREE_BADROOT, pctx))
		return 1;

	retval = io_channel_read_blk(fs->io, blk, 1, block_buf);
	if (retval && fix_problem(ctx, PR_1_HTREE_BADROOT, pctx))
		return 1;
	
	/* XXX should check that beginning matches a directory */
	root = (struct ext2_dx_root_info *) (block_buf + 24);

	if ((root->reserved_zero || root->info_length < 8) &&
	    fix_problem(ctx, PR_1_HTREE_BADROOT, pctx))
		return 1;

	pctx->num = root->hash_version;
	if ((root->hash_version != EXT2_HASH_LEGACY) &&
	    (root->hash_version != EXT2_HASH_HALF_MD4) &&
	    (root->hash_version != EXT2_HASH_TEA) &&
	    fix_problem(ctx, PR_1_HTREE_HASHV, pctx))
		return 1;
		
	if ((root->unused_flags & EXT2_HASH_FLAG_INCOMPAT) &&
	    fix_problem(ctx, PR_1_HTREE_INCOMPAT, pctx))
		return 1;

	pctx->num = root->indirect_levels;
	if ((root->indirect_levels > 1) &&
	    fix_problem(ctx, PR_1_HTREE_DEPTH, pctx))
		return 1;
	
	return 0;
}

/*
 * This subroutine is called on each inode to account for all of the
 * blocks used by that inode.
 */
static void check_blocks(e2fsck_t ctx, struct problem_context *pctx,
			 char *block_buf)
{
	ext2_filsys fs = ctx->fs;
	struct process_block_struct pb;
	ext2_ino_t	ino = pctx->ino;
	struct ext2_inode *inode = pctx->inode;
	int		bad_size = 0;
	int		dirty_inode = 0;
	__u64		size;
	
	pb.ino = ino;
	pb.num_blocks = 0;
	pb.last_block = -1;
	pb.num_illegal_blocks = 0;
	pb.suppress = 0; pb.clear = 0;
	pb.fragmented = 0;
	pb.compressed = 0;
	pb.previous_block = 0;
	pb.is_dir = LINUX_S_ISDIR(inode->i_mode);
	pb.is_reg = LINUX_S_ISREG(inode->i_mode);
	pb.max_blocks = 1 << (31 - fs->super->s_log_block_size);
	pb.inode = inode;
	pb.pctx = pctx;
	pb.ctx = ctx;
	pctx->ino = ino;
	pctx->errcode = 0;

	if (inode->i_flags & EXT2_COMPRBLK_FL) {
		if (fs->super->s_feature_incompat &
		    EXT2_FEATURE_INCOMPAT_COMPRESSION)
			pb.compressed = 1;
		else {
			if (fix_problem(ctx, PR_1_COMPR_SET, pctx)) {
				inode->i_flags &= ~EXT2_COMPRBLK_FL;
				dirty_inode++;
			}
		}
	}

	if (inode->i_file_acl && check_ext_attr(ctx, pctx, block_buf))
		pb.num_blocks++;

	if (ext2fs_inode_has_valid_blocks(inode))
		pctx->errcode = ext2fs_block_iterate2(fs, ino,
				       pb.is_dir ? BLOCK_FLAG_HOLE : 0,
				       block_buf, process_block, &pb);
	end_problem_latch(ctx, PR_LATCH_BLOCK);
	end_problem_latch(ctx, PR_LATCH_TOOBIG);
	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		goto out;
	if (pctx->errcode)
		fix_problem(ctx, PR_1_BLOCK_ITERATE, pctx);

	if (pb.fragmented && pb.num_blocks < fs->super->s_blocks_per_group)
		ctx->fs_fragmented++;

	if (pb.clear) {
		inode->i_links_count = 0;
		ext2fs_icount_store(ctx->inode_link_info, ino, 0);
		inode->i_dtime = time(0);
		dirty_inode++;
		ext2fs_unmark_inode_bitmap(ctx->inode_dir_map, ino);
		ext2fs_unmark_inode_bitmap(ctx->inode_reg_map, ino);
		ext2fs_unmark_inode_bitmap(ctx->inode_used_map, ino);
		/*
		 * The inode was probably partially accounted for
		 * before processing was aborted, so we need to
		 * restart the pass 1 scan.
		 */
		ctx->flags |= E2F_FLAG_RESTART;
		goto out;
	}
	
	if (inode->i_flags & EXT2_INDEX_FL) {
		if (handle_htree(ctx, pctx, ino, inode, block_buf)) {
			inode->i_flags &= ~EXT2_INDEX_FL;
			dirty_inode++;
		} else {
#ifdef ENABLE_HTREE
			e2fsck_add_dx_dir(ctx, ino, pb.last_block+1);
#endif
		}
	}
	if (ctx->dirs_to_hash && pb.is_dir &&
	    !(inode->i_flags & EXT2_INDEX_FL) &&
	    ((inode->i_size / fs->blocksize) >= 3))
		ext2fs_u32_list_add(ctx->dirs_to_hash, ino);
		
	if (!pb.num_blocks && pb.is_dir) {
		if (fix_problem(ctx, PR_1_ZERO_LENGTH_DIR, pctx)) {
			inode->i_links_count = 0;
			ext2fs_icount_store(ctx->inode_link_info, ino, 0);
			inode->i_dtime = time(0);
			dirty_inode++;
			ext2fs_unmark_inode_bitmap(ctx->inode_dir_map, ino);
			ext2fs_unmark_inode_bitmap(ctx->inode_reg_map, ino);
			ext2fs_unmark_inode_bitmap(ctx->inode_used_map, ino);
			ctx->fs_directory_count--;
			goto out;
		}
	}

	pb.num_blocks *= (fs->blocksize / 512);
#if 0
	printf("inode %u, i_size = %lu, last_block = %lld, i_blocks=%lu, num_blocks = %lu\n",
	       ino, inode->i_size, pb.last_block, inode->i_blocks,
	       pb.num_blocks);
#endif
	if (pb.is_dir) {
		int nblock = inode->i_size >> EXT2_BLOCK_SIZE_BITS(fs->super);
		if (nblock > (pb.last_block + 1))
			bad_size = 1;
		else if (nblock < (pb.last_block + 1)) {
			if (((pb.last_block + 1) - nblock) >
			    fs->super->s_prealloc_dir_blocks)
				bad_size = 2;
		}
	} else {
		size = EXT2_I_SIZE(inode);
		if ((pb.last_block >= 0) &&
		    (size < (__u64) pb.last_block * fs->blocksize))
			bad_size = 3;
		else if (size > ext2_max_sizes[fs->super->s_log_block_size])
			bad_size = 4;
	}
	/* i_size for symlinks is checked elsewhere */
	if (bad_size && !LINUX_S_ISLNK(inode->i_mode)) {
		pctx->num = (pb.last_block+1) * fs->blocksize;
		if (fix_problem(ctx, PR_1_BAD_I_SIZE, pctx)) {
			inode->i_size = pctx->num;
			if (!LINUX_S_ISDIR(inode->i_mode))
				inode->i_size_high = pctx->num >> 32;
			dirty_inode++;
		}
		pctx->num = 0;
	}
	if (LINUX_S_ISREG(inode->i_mode) &&
	    (inode->i_size_high || inode->i_size & 0x80000000UL))
		ctx->large_files++;
	if (pb.num_blocks != inode->i_blocks) {
		pctx->num = pb.num_blocks;
		if (fix_problem(ctx, PR_1_BAD_I_BLOCKS, pctx)) {
			inode->i_blocks = pb.num_blocks;
			dirty_inode++;
		}
		pctx->num = 0;
	}
out:
	if (dirty_inode)
		e2fsck_write_inode(ctx, ino, inode, "check_blocks");
}

#if 0
/*
 * Helper function called by process block when an illegal block is
 * found.  It returns a description about why the block is illegal
 */
static char *describe_illegal_block(ext2_filsys fs, blk_t block)
{
	blk_t	super;
	int	i;
	static char	problem[80];

	super = fs->super->s_first_data_block;
	strcpy(problem, "PROGRAMMING ERROR: Unknown reason for illegal block");
	if (block < super) {
		sprintf(problem, "< FIRSTBLOCK (%u)", super);
		return(problem);
	} else if (block >= fs->super->s_blocks_count) {
		sprintf(problem, "> BLOCKS (%u)", fs->super->s_blocks_count);
		return(problem);
	}
	for (i = 0; i < fs->group_desc_count; i++) {
		if (block == super) {
			sprintf(problem, "is the superblock in group %d", i);
			break;
		}
		if (block > super &&
		    block <= (super + fs->desc_blocks)) {
			sprintf(problem, "is in the group descriptors "
				"of group %d", i);
			break;
		}
		if (block == fs->group_desc[i].bg_block_bitmap) {
			sprintf(problem, "is the block bitmap of group %d", i);
			break;
		}
		if (block == fs->group_desc[i].bg_inode_bitmap) {
			sprintf(problem, "is the inode bitmap of group %d", i);
			break;
		}
		if (block >= fs->group_desc[i].bg_inode_table &&
		    (block < fs->group_desc[i].bg_inode_table
		     + fs->inode_blocks_per_group)) {
			sprintf(problem, "is in the inode table of group %d",
				i);
			break;
		}
		super += fs->super->s_blocks_per_group;
	}
	return(problem);
}
#endif

/*
 * This is a helper function for check_blocks().
 */
static int process_block(ext2_filsys fs,
		  blk_t	*block_nr,
		  e2_blkcnt_t blockcnt,
		  blk_t ref_block EXT2FS_ATTR((unused)),
		  int ref_offset EXT2FS_ATTR((unused)),
		  void *priv_data)
{
	struct process_block_struct *p;
	struct problem_context *pctx;
	blk_t	blk = *block_nr;
	int	ret_code = 0;
	int	problem = 0;
	e2fsck_t	ctx;

	p = (struct process_block_struct *) priv_data;
	pctx = p->pctx;
	ctx = p->ctx;

	if (p->compressed && (blk == EXT2FS_COMPRESSED_BLKADDR)) {
		/* todo: Check that the comprblk_fl is high, that the
		   blkaddr pattern looks right (all non-holes up to
		   first EXT2FS_COMPRESSED_BLKADDR, then all
		   EXT2FS_COMPRESSED_BLKADDR up to end of cluster),
		   that the feature_incompat bit is high, and that the
		   inode is a regular file.  If we're doing a "full
		   check" (a concept introduced to e2fsck by e2compr,
		   meaning that we look at data blocks as well as
		   metadata) then call some library routine that
		   checks the compressed data.  I'll have to think
		   about this, because one particularly important
		   problem to be able to fix is to recalculate the
		   cluster size if necessary.  I think that perhaps
		   we'd better do most/all e2compr-specific checks
		   separately, after the non-e2compr checks.  If not
		   doing a full check, it may be useful to test that
		   the personality is linux; e.g. if it isn't then
		   perhaps this really is just an illegal block. */
		return 0;
	}

	if (blk == 0) {
		if (p->is_dir == 0) {
			/*
			 * Should never happen, since only directories
			 * get called with BLOCK_FLAG_HOLE
			 */
#if DEBUG_E2FSCK
			printf("process_block() called with blk == 0, "
			       "blockcnt=%d, inode %lu???\n",
			       blockcnt, p->ino);
#endif
			return 0;
		}
		if (blockcnt < 0)
			return 0;
		if (blockcnt * fs->blocksize < p->inode->i_size) {
#if 0
			printf("Missing block (#%d) in directory inode %lu!\n",
			       blockcnt, p->ino);
#endif
			goto mark_dir;
		}
		return 0;
	}

#if 0
	printf("Process_block, inode %lu, block %u, #%d\n", p->ino, blk,
	       blockcnt);
#endif
	
	/*
	 * Simplistic fragmentation check.  We merely require that the
	 * file be contiguous.  (Which can never be true for really
	 * big files that are greater than a block group.)
	 */
	if (!HOLE_BLKADDR(p->previous_block)) {
		if (p->previous_block+1 != blk)
			p->fragmented = 1;
	}
	p->previous_block = blk;

	if (p->is_dir && blockcnt > (1 << (21 - fs->super->s_log_block_size)))
		problem = PR_1_TOOBIG_DIR;
	if (p->is_reg && p->num_blocks+1 >= p->max_blocks)
		problem = PR_1_TOOBIG_REG;
	if (!p->is_dir && !p->is_reg && blockcnt > 0)
		problem = PR_1_TOOBIG_SYMLINK;
	    
	if (blk < fs->super->s_first_data_block ||
	    blk >= fs->super->s_blocks_count)
		problem = PR_1_ILLEGAL_BLOCK_NUM;

	if (problem) {
		p->num_illegal_blocks++;
		if (!p->suppress && (p->num_illegal_blocks % 12) == 0) {
			if (fix_problem(ctx, PR_1_TOO_MANY_BAD_BLOCKS, pctx)) {
				p->clear = 1;
				return BLOCK_ABORT;
			}
			if (fix_problem(ctx, PR_1_SUPPRESS_MESSAGES, pctx)) {
				p->suppress = 1;
				set_latch_flags(PR_LATCH_BLOCK,
						PRL_SUPPRESS, 0);
			}
		}
		pctx->blk = blk;
		pctx->blkcount = blockcnt;
		if (fix_problem(ctx, problem, pctx)) {
			blk = *block_nr = 0;
			ret_code = BLOCK_CHANGED;
			goto mark_dir;
		} else
			return 0;
	}

	if (p->ino == EXT2_RESIZE_INO) {
		/* 
		 * The resize inode has already be sanity checked
		 * during pass #0 (the superblock checks).  All we
		 * have to do is mark the double indirect block as
		 * being in use; all of the other blocks are handled
		 * by mark_table_blocks()).
		 */
		if (blockcnt == BLOCK_COUNT_DIND)
			mark_block_used(ctx, blk);
	} else
		mark_block_used(ctx, blk);
	p->num_blocks++;
	if (blockcnt >= 0)
		p->last_block = blockcnt;
mark_dir:
	if (p->is_dir && (blockcnt >= 0)) {
		pctx->errcode = ext2fs_add_dir_block(fs->dblist, p->ino,
						    blk, blockcnt);
		if (pctx->errcode) {
			pctx->blk = blk;
			pctx->num = blockcnt;
			fix_problem(ctx, PR_1_ADD_DBLOCK, pctx);
			/* Should never get here */
			ctx->flags |= E2F_FLAG_ABORT;
			return BLOCK_ABORT;
		}
	}
	return ret_code;
}

static int process_bad_block(ext2_filsys fs,
		      blk_t *block_nr,
		      e2_blkcnt_t blockcnt,
		      blk_t ref_block EXT2FS_ATTR((unused)),
		      int ref_offset EXT2FS_ATTR((unused)),
		      void *priv_data)
{
	struct process_block_struct *p;
	blk_t		blk = *block_nr;
	blk_t		first_block;
	dgrp_t		i;
	struct problem_context *pctx;
	e2fsck_t	ctx;

	/*
	 * Note: This function processes blocks for the bad blocks
	 * inode, which is never compressed.  So we don't use HOLE_BLKADDR().
	 */

	if (!blk)
		return 0;
	
	p = (struct process_block_struct *) priv_data;
	ctx = p->ctx;
	pctx = p->pctx;
	
	pctx->ino = EXT2_BAD_INO;
	pctx->blk = blk;
	pctx->blkcount = blockcnt;

	if ((blk < fs->super->s_first_data_block) ||
	    (blk >= fs->super->s_blocks_count)) {
		if (fix_problem(ctx, PR_1_BB_ILLEGAL_BLOCK_NUM, pctx)) {
			*block_nr = 0;
			return BLOCK_CHANGED;
		} else
			return 0;
	}

	if (blockcnt < 0) {
		if (ext2fs_test_block_bitmap(p->fs_meta_blocks, blk)) {
			p->bbcheck = 1;
			if (fix_problem(ctx, PR_1_BB_FS_BLOCK, pctx)) {
				*block_nr = 0;
				return BLOCK_CHANGED;
			}
		} else if (ext2fs_test_block_bitmap(ctx->block_found_map, 
						    blk)) {
			p->bbcheck = 1;
			if (fix_problem(ctx, PR_1_BBINODE_BAD_METABLOCK, 
					pctx)) {
				*block_nr = 0;
				return BLOCK_CHANGED;
			}
			if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
				return BLOCK_ABORT;
		} else
			mark_block_used(ctx, blk);
		return 0;
	}
#if 0 
	printf ("DEBUG: Marking %u as bad.\n", blk);
#endif
	ctx->fs_badblocks_count++;
	/*
	 * If the block is not used, then mark it as used and return.
	 * If it is already marked as found, this must mean that
	 * there's an overlap between the filesystem table blocks
	 * (bitmaps and inode table) and the bad block list.
	 */
	if (!ext2fs_test_block_bitmap(ctx->block_found_map, blk)) {
		ext2fs_mark_block_bitmap(ctx->block_found_map, blk);
		return 0;
	}
	/*
	 * Try to find the where the filesystem block was used...
	 */
	first_block = fs->super->s_first_data_block;
	
	for (i = 0; i < fs->group_desc_count; i++ ) {
		pctx->group = i;
		pctx->blk = blk;
		if (!ext2fs_bg_has_super(fs, i))
			goto skip_super;
		if (blk == first_block) {
			if (i == 0) {
				if (fix_problem(ctx,
						PR_1_BAD_PRIMARY_SUPERBLOCK,
						pctx)) {
					*block_nr = 0;
					return BLOCK_CHANGED;
				}
				return 0;
			}
			fix_problem(ctx, PR_1_BAD_SUPERBLOCK, pctx);
			return 0;
		}
		if ((blk > first_block) &&
		    (blk <= first_block + fs->desc_blocks)) {
			if (i == 0) {
				pctx->blk = *block_nr;
				if (fix_problem(ctx,
			PR_1_BAD_PRIMARY_GROUP_DESCRIPTOR, pctx)) {
					*block_nr = 0;
					return BLOCK_CHANGED;
				}
				return 0;
			}
			fix_problem(ctx, PR_1_BAD_GROUP_DESCRIPTORS, pctx);
			return 0;
		}
	skip_super:
		if (blk == fs->group_desc[i].bg_block_bitmap) {
			if (fix_problem(ctx, PR_1_BB_BAD_BLOCK, pctx)) {
				ctx->invalid_block_bitmap_flag[i]++;
				ctx->invalid_bitmaps++;
			}
			return 0;
		}
		if (blk == fs->group_desc[i].bg_inode_bitmap) {
			if (fix_problem(ctx, PR_1_IB_BAD_BLOCK, pctx)) {
				ctx->invalid_inode_bitmap_flag[i]++;
				ctx->invalid_bitmaps++;
			}
			return 0;
		}
		if ((blk >= fs->group_desc[i].bg_inode_table) &&
		    (blk < (fs->group_desc[i].bg_inode_table +
			    fs->inode_blocks_per_group))) {
			/*
			 * If there are bad blocks in the inode table,
			 * the inode scan code will try to do
			 * something reasonable automatically.
			 */
			return 0;
		}
		first_block += fs->super->s_blocks_per_group;
	}
	/*
	 * If we've gotten to this point, then the only
	 * possibility is that the bad block inode meta data
	 * is using a bad block.
	 */
	if ((blk == p->inode->i_block[EXT2_IND_BLOCK]) ||
	    (blk == p->inode->i_block[EXT2_DIND_BLOCK]) ||
	    (blk == p->inode->i_block[EXT2_TIND_BLOCK])) {
		p->bbcheck = 1;
		if (fix_problem(ctx, PR_1_BBINODE_BAD_METABLOCK, pctx)) {
			*block_nr = 0;
			return BLOCK_CHANGED;
		}
		if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
			return BLOCK_ABORT;
		return 0;
	}

	pctx->group = -1;

	/* Warn user that the block wasn't claimed */
	fix_problem(ctx, PR_1_PROGERR_CLAIMED_BLOCK, pctx);

	return 0;
}

static void new_table_block(e2fsck_t ctx, blk_t first_block, int group, 
			    const char *name, int num, blk_t *new_block)
{
	ext2_filsys fs = ctx->fs;
	blk_t		old_block = *new_block;
	int		i;
	char		*buf;
	struct problem_context	pctx;

	clear_problem_context(&pctx);

	pctx.group = group;
	pctx.blk = old_block;
	pctx.str = name;

	pctx.errcode = ext2fs_get_free_blocks(fs, first_block,
			first_block + fs->super->s_blocks_per_group,
					num, ctx->block_found_map, new_block);
	if (pctx.errcode) {
		pctx.num = num;
		fix_problem(ctx, PR_1_RELOC_BLOCK_ALLOCATE, &pctx);
		ext2fs_unmark_valid(fs);
		return;
	}
	pctx.errcode = ext2fs_get_mem(fs->blocksize, &buf);
	if (pctx.errcode) {
		fix_problem(ctx, PR_1_RELOC_MEMORY_ALLOCATE, &pctx);
		ext2fs_unmark_valid(fs);
		return;
	}
	ext2fs_mark_super_dirty(fs);
	fs->flags &= ~EXT2_FLAG_MASTER_SB_ONLY;
	pctx.blk2 = *new_block;
	fix_problem(ctx, (old_block ? PR_1_RELOC_FROM_TO :
			  PR_1_RELOC_TO), &pctx);
	pctx.blk2 = 0;
	for (i = 0; i < num; i++) {
		pctx.blk = i;
		ext2fs_mark_block_bitmap(ctx->block_found_map, (*new_block)+i);
		if (old_block) {
			pctx.errcode = io_channel_read_blk(fs->io,
				   old_block + i, 1, buf);
			if (pctx.errcode)
				fix_problem(ctx, PR_1_RELOC_READ_ERR, &pctx);
		} else
			memset(buf, 0, fs->blocksize);

		pctx.blk = (*new_block) + i;
		pctx.errcode = io_channel_write_blk(fs->io, pctx.blk,
					      1, buf);
		if (pctx.errcode)
			fix_problem(ctx, PR_1_RELOC_WRITE_ERR, &pctx);
	}
	ext2fs_free_mem(&buf);
}

/*
 * This routine gets called at the end of pass 1 if bad blocks are
 * detected in the superblock, group descriptors, inode_bitmaps, or
 * block bitmaps.  At this point, all of the blocks have been mapped
 * out, so we can try to allocate new block(s) to replace the bad
 * blocks.
 */
static void handle_fs_bad_blocks(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	dgrp_t		i;
	int		first_block = fs->super->s_first_data_block;

	for (i = 0; i < fs->group_desc_count; i++) {
		if (ctx->invalid_block_bitmap_flag[i]) {
			new_table_block(ctx, first_block, i, _("block bitmap"),
					1, &fs->group_desc[i].bg_block_bitmap);
		}
		if (ctx->invalid_inode_bitmap_flag[i]) {
			new_table_block(ctx, first_block, i, _("inode bitmap"),
					1, &fs->group_desc[i].bg_inode_bitmap);
		}
		if (ctx->invalid_inode_table_flag[i]) {
			new_table_block(ctx, first_block, i, _("inode table"),
					fs->inode_blocks_per_group, 
					&fs->group_desc[i].bg_inode_table);
			ctx->flags |= E2F_FLAG_RESTART;
		}
		first_block += fs->super->s_blocks_per_group;
	}
	ctx->invalid_bitmaps = 0;
}

/*
 * This routine marks all blocks which are used by the superblock,
 * group descriptors, inode bitmaps, and block bitmaps.
 */
static void mark_table_blocks(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	blk_t	block, b;
	dgrp_t	i;
	int	j;
	struct problem_context pctx;
	
	clear_problem_context(&pctx);
	
	block = fs->super->s_first_data_block;
	for (i = 0; i < fs->group_desc_count; i++) {
		pctx.group = i;

		ext2fs_reserve_super_and_bgd(fs, i, ctx->block_found_map);

		/*
		 * Mark the blocks used for the inode table
		 */
		if (fs->group_desc[i].bg_inode_table) {
			for (j = 0, b = fs->group_desc[i].bg_inode_table;
			     j < fs->inode_blocks_per_group;
			     j++, b++) {
				if (ext2fs_test_block_bitmap(ctx->block_found_map,
							     b)) {
					pctx.blk = b;
					if (fix_problem(ctx,
						PR_1_ITABLE_CONFLICT, &pctx)) {
						ctx->invalid_inode_table_flag[i]++;
						ctx->invalid_bitmaps++;
					}
				} else {
				    ext2fs_mark_block_bitmap(ctx->block_found_map,
							     b);
			    	}
			}
		}
			    
		/*
		 * Mark block used for the block bitmap 
		 */
		if (fs->group_desc[i].bg_block_bitmap) {
			if (ext2fs_test_block_bitmap(ctx->block_found_map,
				     fs->group_desc[i].bg_block_bitmap)) {
				pctx.blk = fs->group_desc[i].bg_block_bitmap;
				if (fix_problem(ctx, PR_1_BB_CONFLICT, &pctx)) {
					ctx->invalid_block_bitmap_flag[i]++;
					ctx->invalid_bitmaps++;
				}
			} else {
			    ext2fs_mark_block_bitmap(ctx->block_found_map,
				     fs->group_desc[i].bg_block_bitmap);
		    }
			
		}
		/*
		 * Mark block used for the inode bitmap 
		 */
		if (fs->group_desc[i].bg_inode_bitmap) {
			if (ext2fs_test_block_bitmap(ctx->block_found_map,
				     fs->group_desc[i].bg_inode_bitmap)) {
				pctx.blk = fs->group_desc[i].bg_inode_bitmap;
				if (fix_problem(ctx, PR_1_IB_CONFLICT, &pctx)) {
					ctx->invalid_inode_bitmap_flag[i]++;
					ctx->invalid_bitmaps++;
				} 
			} else {
			    ext2fs_mark_block_bitmap(ctx->block_found_map,
				     fs->group_desc[i].bg_inode_bitmap);
			}
		}
		block += fs->super->s_blocks_per_group;
	}
}
	
/*
 * Thes subroutines short circuits ext2fs_get_blocks and
 * ext2fs_check_directory; we use them since we already have the inode
 * structure, so there's no point in letting the ext2fs library read
 * the inode again.
 */
static errcode_t pass1_get_blocks(ext2_filsys fs, ext2_ino_t ino,
				  blk_t *blocks)
{
	e2fsck_t ctx = (e2fsck_t) fs->priv_data;
	int	i;
	
	if ((ino != ctx->stashed_ino) || !ctx->stashed_inode)
		return EXT2_ET_CALLBACK_NOTHANDLED;

	for (i=0; i < EXT2_N_BLOCKS; i++)
		blocks[i] = ctx->stashed_inode->i_block[i];
	return 0;
}

static errcode_t pass1_read_inode(ext2_filsys fs, ext2_ino_t ino,
				  struct ext2_inode *inode)
{
	e2fsck_t ctx = (e2fsck_t) fs->priv_data;

	if ((ino != ctx->stashed_ino) || !ctx->stashed_inode)
		return EXT2_ET_CALLBACK_NOTHANDLED;
	*inode = *ctx->stashed_inode;
	return 0;
}

static errcode_t pass1_write_inode(ext2_filsys fs, ext2_ino_t ino,
			    struct ext2_inode *inode)
{
	e2fsck_t ctx = (e2fsck_t) fs->priv_data;

	if ((ino == ctx->stashed_ino) && ctx->stashed_inode)
		*ctx->stashed_inode = *inode;
	return EXT2_ET_CALLBACK_NOTHANDLED;
}

static errcode_t pass1_check_directory(ext2_filsys fs, ext2_ino_t ino)
{
	e2fsck_t ctx = (e2fsck_t) fs->priv_data;

	if ((ino != ctx->stashed_ino) || !ctx->stashed_inode)
		return EXT2_ET_CALLBACK_NOTHANDLED;

	if (!LINUX_S_ISDIR(ctx->stashed_inode->i_mode))
		return EXT2_ET_NO_DIRECTORY;
	return 0;
}

void e2fsck_use_inode_shortcuts(e2fsck_t ctx, int bool)
{
	ext2_filsys fs = ctx->fs;

	if (bool) {
		fs->get_blocks = pass1_get_blocks;
		fs->check_directory = pass1_check_directory;
		fs->read_inode = pass1_read_inode;
		fs->write_inode = pass1_write_inode;
		ctx->stashed_ino = 0;
	} else {
		fs->get_blocks = 0;
		fs->check_directory = 0;
		fs->read_inode = 0;
		fs->write_inode = 0;
	}
}
