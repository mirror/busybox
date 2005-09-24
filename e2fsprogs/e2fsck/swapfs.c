/*
 * swapfs.c --- byte-swap an ext2 filesystem
 *
 * Copyright 1996, 1997 by Theodore Ts'o
 * 
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 * 
 */

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <et/com_err.h>
#include "e2fsck.h"

#ifdef ENABLE_SWAPFS

struct swap_block_struct {
	ext2_ino_t	ino;
	int		isdir;
	errcode_t	errcode;
	char		*dir_buf;
	struct ext2_inode *inode;
};

/*
 * This is a helper function for block_iterate.  We mark all of the
 * indirect and direct blocks as changed, so that block_iterate will
 * write them out.
 */
static int swap_block(ext2_filsys fs, blk_t *block_nr, int blockcnt,
		      void *priv_data)
{
	errcode_t	retval;
	
	struct swap_block_struct *sb = (struct swap_block_struct *) priv_data;

	if (sb->isdir && (blockcnt >= 0) && *block_nr) {
		retval = ext2fs_read_dir_block(fs, *block_nr, sb->dir_buf);
		if (retval) {
			sb->errcode = retval;
			return BLOCK_ABORT;
		}
		retval = ext2fs_write_dir_block(fs, *block_nr, sb->dir_buf);
		if (retval) {
			sb->errcode = retval;
			return BLOCK_ABORT;
		}
	}
	if (blockcnt >= 0) {
		if (blockcnt < EXT2_NDIR_BLOCKS)
			return 0;
		return BLOCK_CHANGED;
	}
	if (blockcnt == BLOCK_COUNT_IND) {
		if (*block_nr == sb->inode->i_block[EXT2_IND_BLOCK])
			return 0;
		return BLOCK_CHANGED;
	}
	if (blockcnt == BLOCK_COUNT_DIND) {
		if (*block_nr == sb->inode->i_block[EXT2_DIND_BLOCK])
			return 0;
		return BLOCK_CHANGED;
	}
	if (blockcnt == BLOCK_COUNT_TIND) {
		if (*block_nr == sb->inode->i_block[EXT2_TIND_BLOCK])
			return 0;
		return BLOCK_CHANGED;
	}
	return BLOCK_CHANGED;
}

/*
 * This function is responsible for byte-swapping all of the indirect,
 * block pointers.  It is also responsible for byte-swapping directories.
 */
static void swap_inode_blocks(e2fsck_t ctx, ext2_ino_t ino, char *block_buf,
			      struct ext2_inode *inode)
{
	errcode_t			retval;
	struct swap_block_struct	sb;

	sb.ino = ino;
	sb.inode = inode;
	sb.dir_buf = block_buf + ctx->fs->blocksize*3;
	sb.errcode = 0;
	sb.isdir = 0;
	if (LINUX_S_ISDIR(inode->i_mode))
		sb.isdir = 1;

	retval = ext2fs_block_iterate(ctx->fs, ino, 0, block_buf,
				      swap_block, &sb);
	if (retval) {
		com_err("swap_inode_blocks", retval,
			_("while calling ext2fs_block_iterate"));
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
	if (sb.errcode) {
		com_err("swap_inode_blocks", sb.errcode,
			_("while calling iterator function"));
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
}

static void swap_inodes(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	dgrp_t			group;
	unsigned int		i;
	ext2_ino_t		ino = 1;
	char 			*buf, *block_buf;
	errcode_t		retval;
	struct ext2_inode *	inode;

	e2fsck_use_inode_shortcuts(ctx, 1);
	
	retval = ext2fs_get_mem(fs->blocksize * fs->inode_blocks_per_group,
				&buf);
	if (retval) {
		com_err("swap_inodes", retval,
			_("while allocating inode buffer"));
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
	block_buf = (char *) e2fsck_allocate_memory(ctx, fs->blocksize * 4,
						    "block interate buffer");
	for (group = 0; group < fs->group_desc_count; group++) {
		retval = io_channel_read_blk(fs->io,
		      fs->group_desc[group].bg_inode_table,
		      fs->inode_blocks_per_group, buf);
		if (retval) {
			com_err("swap_inodes", retval,
				_("while reading inode table (group %d)"),
				group);
			ctx->flags |= E2F_FLAG_ABORT;
			return;
		}
		inode = (struct ext2_inode *) buf;
		for (i=0; i < fs->super->s_inodes_per_group;
		     i++, ino++, inode++) {
			ctx->stashed_ino = ino;
			ctx->stashed_inode = inode;
			
			if (fs->flags & EXT2_FLAG_SWAP_BYTES_READ)
				ext2fs_swap_inode(fs, inode, inode, 0);
			
			/*
			 * Skip deleted files.
			 */
			if (inode->i_links_count == 0)
				continue;
			
			if (LINUX_S_ISDIR(inode->i_mode) ||
			    ((inode->i_block[EXT2_IND_BLOCK] ||
			      inode->i_block[EXT2_DIND_BLOCK] ||
			      inode->i_block[EXT2_TIND_BLOCK]) &&
			     ext2fs_inode_has_valid_blocks(inode)))
				swap_inode_blocks(ctx, ino, block_buf, inode);

			if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
				return;
			
			if (fs->flags & EXT2_FLAG_SWAP_BYTES_WRITE)
				ext2fs_swap_inode(fs, inode, inode, 1);
		}
		retval = io_channel_write_blk(fs->io,
		      fs->group_desc[group].bg_inode_table,
		      fs->inode_blocks_per_group, buf);
		if (retval) {
			com_err("swap_inodes", retval,
				_("while writing inode table (group %d)"),
				group);
			ctx->flags |= E2F_FLAG_ABORT;
			return;
		}
	}
	ext2fs_free_mem(&buf);
	ext2fs_free_mem(&block_buf);
	e2fsck_use_inode_shortcuts(ctx, 0);
	ext2fs_flush_icache(fs);
}

#if defined(__powerpc__) && defined(EXT2FS_ENABLE_SWAPFS)
/*
 * On the PowerPC, the big-endian variant of the ext2 filesystem
 * has its bitmaps stored as 32-bit words with bit 0 as the LSB
 * of each word.  Thus a bitmap with only bit 0 set would be, as
 * a string of bytes, 00 00 00 01 00 ...
 * To cope with this, we byte-reverse each word of a bitmap if
 * we have a big-endian filesystem, that is, if we are *not*
 * byte-swapping other word-sized numbers.
 */
#define EXT2_BIG_ENDIAN_BITMAPS
#endif

#ifdef EXT2_BIG_ENDIAN_BITMAPS
static void ext2fs_swap_bitmap(ext2fs_generic_bitmap bmap)
{
	__u32 *p = (__u32 *) bmap->bitmap;
	int n, nbytes = (bmap->end - bmap->start + 7) / 8;
		
	for (n = nbytes / sizeof(__u32); n > 0; --n, ++p)
		*p = ext2fs_swab32(*p);
}
#endif


void swap_filesys(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
#ifdef RESOURCE_TRACK
	struct resource_track	rtrack;

	init_resource_track(&rtrack);
#endif

	if (!(ctx->options & E2F_OPT_PREEN))
		printf(_("Pass 0: Doing byte-swap of filesystem\n"));
	
#ifdef MTRACE
	mtrace_print("Byte swap");
#endif

	if (fs->super->s_mnt_count) {
		fprintf(stderr, _("%s: the filesystem must be freshly "
			"checked using fsck\n"
			"and not mounted before trying to "
			"byte-swap it.\n"), ctx->device_name);
		ctx->flags |= E2F_FLAG_ABORT;
		return;
	}
	if (fs->flags & EXT2_FLAG_SWAP_BYTES) {
		fs->flags &= ~(EXT2_FLAG_SWAP_BYTES|
			       EXT2_FLAG_SWAP_BYTES_WRITE);
		fs->flags |= EXT2_FLAG_SWAP_BYTES_READ;
	} else {
		fs->flags &= ~EXT2_FLAG_SWAP_BYTES_READ;
		fs->flags |= EXT2_FLAG_SWAP_BYTES_WRITE;
	}
	swap_inodes(ctx);
	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		return;
	if (fs->flags & EXT2_FLAG_SWAP_BYTES_WRITE)
		fs->flags |= EXT2_FLAG_SWAP_BYTES;
	fs->flags &= ~(EXT2_FLAG_SWAP_BYTES_READ|
		       EXT2_FLAG_SWAP_BYTES_WRITE);

#ifdef EXT2_BIG_ENDIAN_BITMAPS
	e2fsck_read_bitmaps(ctx);
	ext2fs_swap_bitmap(fs->inode_map);
	ext2fs_swap_bitmap(fs->block_map);
	fs->flags |= EXT2_FLAG_BB_DIRTY | EXT2_FLAG_IB_DIRTY;
#endif
	fs->flags &= ~EXT2_FLAG_MASTER_SB_ONLY;
	ext2fs_flush(fs);
	fs->flags |= EXT2_FLAG_MASTER_SB_ONLY;
	
#ifdef RESOURCE_TRACK
	if (ctx->options & E2F_OPT_TIME2)
		print_resource_track(_("Byte swap"), &rtrack);
#endif
}

#endif
