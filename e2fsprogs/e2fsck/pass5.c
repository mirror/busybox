/*
 * pass5.c --- check block and inode bitmaps against on-disk bitmaps
 * 
 * Copyright (C) 1993, 1994, 1995, 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 * 
 */

#include "e2fsck.h"
#include "problem.h"

static void check_block_bitmaps(e2fsck_t ctx);
static void check_inode_bitmaps(e2fsck_t ctx);
static void check_inode_end(e2fsck_t ctx);
static void check_block_end(e2fsck_t ctx);

void e2fsck_pass5(e2fsck_t ctx)
{
#ifdef RESOURCE_TRACK
	struct resource_track	rtrack;
#endif
	struct problem_context	pctx;
	
#ifdef MTRACE
	mtrace_print("Pass 5");
#endif

#ifdef RESOURCE_TRACK
	init_resource_track(&rtrack);
#endif
	
	clear_problem_context(&pctx);

	if (!(ctx->options & E2F_OPT_PREEN))
		fix_problem(ctx, PR_5_PASS_HEADER, &pctx);

	if (ctx->progress)
		if ((ctx->progress)(ctx, 5, 0, ctx->fs->group_desc_count*2))
			return;

	e2fsck_read_bitmaps(ctx);

	check_block_bitmaps(ctx);
	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		return;
	check_inode_bitmaps(ctx);
	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		return;
	check_inode_end(ctx);
	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		return;
	check_block_end(ctx);
	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		return;

	ext2fs_free_inode_bitmap(ctx->inode_used_map);
	ctx->inode_used_map = 0;
	ext2fs_free_inode_bitmap(ctx->inode_dir_map);
	ctx->inode_dir_map = 0;
	ext2fs_free_block_bitmap(ctx->block_found_map);
	ctx->block_found_map = 0;

#ifdef RESOURCE_TRACK
	if (ctx->options & E2F_OPT_TIME2) {
		e2fsck_clear_progbar(ctx);
		print_resource_track(_("Pass 5"), &rtrack);
	}
#endif
}

#define NO_BLK ((blk_t) -1)

static void print_bitmap_problem(e2fsck_t ctx, int problem,
			    struct problem_context *pctx)
{
	switch (problem) {
	case PR_5_BLOCK_UNUSED:
		if (pctx->blk == pctx->blk2)
			pctx->blk2 = 0;
		else
			problem = PR_5_BLOCK_RANGE_UNUSED;
		break;
	case PR_5_BLOCK_USED:
		if (pctx->blk == pctx->blk2)
			pctx->blk2 = 0;
		else
			problem = PR_5_BLOCK_RANGE_USED;
		break;
	case PR_5_INODE_UNUSED:
		if (pctx->ino == pctx->ino2)
			pctx->ino2 = 0;
		else
			problem = PR_5_INODE_RANGE_UNUSED;
		break;
	case PR_5_INODE_USED:
		if (pctx->ino == pctx->ino2)
			pctx->ino2 = 0;
		else
			problem = PR_5_INODE_RANGE_USED;
		break;
	}
	fix_problem(ctx, problem, pctx);
	pctx->blk = pctx->blk2 = NO_BLK;
	pctx->ino = pctx->ino2 = 0;
}
	
static void check_block_bitmaps(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	blk_t	i;
	int	*free_array;
	int	group = 0;
	unsigned int	blocks = 0;
	unsigned int	free_blocks = 0;
	int	group_free = 0;
	int	actual, bitmap;
	struct problem_context	pctx;
	int	problem, save_problem, fixit, had_problem;
	errcode_t	retval;
	
	clear_problem_context(&pctx);
	free_array = (int *) e2fsck_allocate_memory(ctx,
	    fs->group_desc_count * sizeof(int), "free block count array");

	if ((fs->super->s_first_data_block <
	     ext2fs_get_block_bitmap_start(ctx->block_found_map)) ||
	    (fs->super->s_blocks_count-1 >
	     ext2fs_get_block_bitmap_end(ctx->block_found_map))) {
		pctx.num = 1;
		pctx.blk = fs->super->s_first_data_block;
		pctx.blk2 = fs->super->s_blocks_count -1;
		pctx.ino = ext2fs_get_block_bitmap_start(ctx->block_found_map);
		pctx.ino2 = ext2fs_get_block_bitmap_end(ctx->block_found_map);
		fix_problem(ctx, PR_5_BMAP_ENDPOINTS, &pctx);

		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		return;
	}
		       
	if ((fs->super->s_first_data_block <
	     ext2fs_get_block_bitmap_start(fs->block_map)) ||
	    (fs->super->s_blocks_count-1 >
	     ext2fs_get_block_bitmap_end(fs->block_map))) {
		pctx.num = 2;
		pctx.blk = fs->super->s_first_data_block;
		pctx.blk2 = fs->super->s_blocks_count -1;
		pctx.ino = ext2fs_get_block_bitmap_start(fs->block_map);
		pctx.ino2 = ext2fs_get_block_bitmap_end(fs->block_map);
		fix_problem(ctx, PR_5_BMAP_ENDPOINTS, &pctx);

		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		return;
	}
		       
redo_counts:
	had_problem = 0;
	save_problem = 0;
	pctx.blk = pctx.blk2 = NO_BLK;
	for (i = fs->super->s_first_data_block;
	     i < fs->super->s_blocks_count;
	     i++) {
		actual = ext2fs_fast_test_block_bitmap(ctx->block_found_map, i);
		bitmap = ext2fs_fast_test_block_bitmap(fs->block_map, i);
		
		if (actual == bitmap)
			goto do_counts;
		
		if (!actual && bitmap) {
			/*
			 * Block not used, but marked in use in the bitmap.
			 */
			problem = PR_5_BLOCK_UNUSED;
		} else {
			/*
			 * Block used, but not marked in use in the bitmap.
			 */
			problem = PR_5_BLOCK_USED;
		}
		if (pctx.blk == NO_BLK) {
			pctx.blk = pctx.blk2 = i;
			save_problem = problem;
		} else {
			if ((problem == save_problem) &&
			    (pctx.blk2 == i-1))
				pctx.blk2++;
			else {
				print_bitmap_problem(ctx, save_problem, &pctx);
				pctx.blk = pctx.blk2 = i;
				save_problem = problem;
			}
		}
		ctx->flags |= E2F_FLAG_PROG_SUPPRESS;
		had_problem++;
		
	do_counts:
		if (!bitmap) {
			group_free++;
			free_blocks++;
		}
		blocks ++;
		if ((blocks == fs->super->s_blocks_per_group) ||
		    (i == fs->super->s_blocks_count-1)) {
			free_array[group] = group_free;
			group ++;
			blocks = 0;
			group_free = 0;
			if (ctx->progress)
				if ((ctx->progress)(ctx, 5, group,
						    fs->group_desc_count*2))
					return;
		}
	}
	if (pctx.blk != NO_BLK)
		print_bitmap_problem(ctx, save_problem, &pctx);
	if (had_problem)
		fixit = end_problem_latch(ctx, PR_LATCH_BBITMAP);
	else
		fixit = -1;
	ctx->flags &= ~E2F_FLAG_PROG_SUPPRESS;
	
	if (fixit == 1) {
		ext2fs_free_block_bitmap(fs->block_map);
		retval = ext2fs_copy_bitmap(ctx->block_found_map,
						  &fs->block_map);
		if (retval) {
			clear_problem_context(&pctx);
			fix_problem(ctx, PR_5_COPY_BBITMAP_ERROR, &pctx);
			ctx->flags |= E2F_FLAG_ABORT;
			return;
		}
		ext2fs_set_bitmap_padding(fs->block_map);
		ext2fs_mark_bb_dirty(fs);
		
		/* Redo the counts */
		blocks = 0; free_blocks = 0; group_free = 0; group = 0;
		memset(free_array, 0, fs->group_desc_count * sizeof(int));
		goto redo_counts;
	} else if (fixit == 0)
		ext2fs_unmark_valid(fs);

	for (i = 0; i < fs->group_desc_count; i++) {
		if (free_array[i] != fs->group_desc[i].bg_free_blocks_count) {
			pctx.group = i;
			pctx.blk = fs->group_desc[i].bg_free_blocks_count;
			pctx.blk2 = free_array[i];

			if (fix_problem(ctx, PR_5_FREE_BLOCK_COUNT_GROUP,
					&pctx)) {
				fs->group_desc[i].bg_free_blocks_count =
					free_array[i];
				ext2fs_mark_super_dirty(fs);
			} else
				ext2fs_unmark_valid(fs);
		}
	}
	if (free_blocks != fs->super->s_free_blocks_count) {
		pctx.group = 0;
		pctx.blk = fs->super->s_free_blocks_count;
		pctx.blk2 = free_blocks;

		if (fix_problem(ctx, PR_5_FREE_BLOCK_COUNT, &pctx)) {
			fs->super->s_free_blocks_count = free_blocks;
			ext2fs_mark_super_dirty(fs);
		} else
			ext2fs_unmark_valid(fs);
	}
	ext2fs_free_mem(&free_array);
}
			
static void check_inode_bitmaps(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	ext2_ino_t	i;
	unsigned int	free_inodes = 0;
	int		group_free = 0;
	int		dirs_count = 0;
	int		group = 0;
	unsigned int	inodes = 0;
	int		*free_array;
	int		*dir_array;
	int		actual, bitmap;
	errcode_t	retval;
	struct problem_context	pctx;
	int		problem, save_problem, fixit, had_problem;
	
	clear_problem_context(&pctx);
	free_array = (int *) e2fsck_allocate_memory(ctx,
	    fs->group_desc_count * sizeof(int), "free inode count array");
				     
	dir_array = (int *) e2fsck_allocate_memory(ctx,
	   fs->group_desc_count * sizeof(int), "directory count array");
				     
	if ((1 < ext2fs_get_inode_bitmap_start(ctx->inode_used_map)) ||
	    (fs->super->s_inodes_count > 
	     ext2fs_get_inode_bitmap_end(ctx->inode_used_map))) {
		pctx.num = 3;
		pctx.blk = 1;
		pctx.blk2 = fs->super->s_inodes_count;
		pctx.ino = ext2fs_get_inode_bitmap_start(ctx->inode_used_map);
		pctx.ino2 = ext2fs_get_inode_bitmap_end(ctx->inode_used_map);
		fix_problem(ctx, PR_5_BMAP_ENDPOINTS, &pctx);

		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		return;
	}
	if ((1 < ext2fs_get_inode_bitmap_start(fs->inode_map)) ||
	    (fs->super->s_inodes_count > 
	     ext2fs_get_inode_bitmap_end(fs->inode_map))) {
		pctx.num = 4;
		pctx.blk = 1;
		pctx.blk2 = fs->super->s_inodes_count;
		pctx.ino = ext2fs_get_inode_bitmap_start(fs->inode_map);
		pctx.ino2 = ext2fs_get_inode_bitmap_end(fs->inode_map);
		fix_problem(ctx, PR_5_BMAP_ENDPOINTS, &pctx);

		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		return;
	}

redo_counts:
	had_problem = 0;
	save_problem = 0;
	pctx.ino = pctx.ino2 = 0;
	for (i = 1; i <= fs->super->s_inodes_count; i++) {
		actual = ext2fs_fast_test_inode_bitmap(ctx->inode_used_map, i);
		bitmap = ext2fs_fast_test_inode_bitmap(fs->inode_map, i);
		
		if (actual == bitmap)
			goto do_counts;
		
		if (!actual && bitmap) {
			/*
			 * Inode wasn't used, but marked in bitmap
			 */
			problem = PR_5_INODE_UNUSED;
		} else /* if (actual && !bitmap) */ {
			/*
			 * Inode used, but not in bitmap
			 */
			problem = PR_5_INODE_USED;
		}
		if (pctx.ino == 0) {
			pctx.ino = pctx.ino2 = i;
			save_problem = problem;
		} else {
			if ((problem == save_problem) &&
			    (pctx.ino2 == i-1))
				pctx.ino2++;
			else {
				print_bitmap_problem(ctx, save_problem, &pctx);
				pctx.ino = pctx.ino2 = i;
				save_problem = problem;
			}
		}
		ctx->flags |= E2F_FLAG_PROG_SUPPRESS;
		had_problem++;
		
do_counts:
		if (!bitmap) {
			group_free++;
			free_inodes++;
		} else {
			if (ext2fs_test_inode_bitmap(ctx->inode_dir_map, i))
				dirs_count++;
		}
		inodes++;
		if ((inodes == fs->super->s_inodes_per_group) ||
		    (i == fs->super->s_inodes_count)) {
			free_array[group] = group_free;
			dir_array[group] = dirs_count;
			group ++;
			inodes = 0;
			group_free = 0;
			dirs_count = 0;
			if (ctx->progress)
				if ((ctx->progress)(ctx, 5,
					    group + fs->group_desc_count,
					    fs->group_desc_count*2))
					return;
		}
	}
	if (pctx.ino)
		print_bitmap_problem(ctx, save_problem, &pctx);
	
	if (had_problem)
		fixit = end_problem_latch(ctx, PR_LATCH_IBITMAP);
	else
		fixit = -1;
	ctx->flags &= ~E2F_FLAG_PROG_SUPPRESS;
	
	if (fixit == 1) {
		ext2fs_free_inode_bitmap(fs->inode_map);
		retval = ext2fs_copy_bitmap(ctx->inode_used_map,
						  &fs->inode_map);
		if (retval) {
			clear_problem_context(&pctx);
			fix_problem(ctx, PR_5_COPY_IBITMAP_ERROR, &pctx);
			ctx->flags |= E2F_FLAG_ABORT;
			return;
		}
		ext2fs_set_bitmap_padding(fs->inode_map);
		ext2fs_mark_ib_dirty(fs);

		/* redo counts */
		inodes = 0; free_inodes = 0; group_free = 0;
		dirs_count = 0; group = 0;
		memset(free_array, 0, fs->group_desc_count * sizeof(int));
		memset(dir_array, 0, fs->group_desc_count * sizeof(int));
		goto redo_counts;
	} else if (fixit == 0)
		ext2fs_unmark_valid(fs);
	
	for (i = 0; i < fs->group_desc_count; i++) {
		if (free_array[i] != fs->group_desc[i].bg_free_inodes_count) {
			pctx.group = i;
			pctx.ino = fs->group_desc[i].bg_free_inodes_count;
			pctx.ino2 = free_array[i];
			if (fix_problem(ctx, PR_5_FREE_INODE_COUNT_GROUP,
					&pctx)) {
				fs->group_desc[i].bg_free_inodes_count =
					free_array[i];
				ext2fs_mark_super_dirty(fs);
			} else
				ext2fs_unmark_valid(fs);
		}
		if (dir_array[i] != fs->group_desc[i].bg_used_dirs_count) {
			pctx.group = i;
			pctx.ino = fs->group_desc[i].bg_used_dirs_count;
			pctx.ino2 = dir_array[i];

			if (fix_problem(ctx, PR_5_FREE_DIR_COUNT_GROUP,
					&pctx)) {
				fs->group_desc[i].bg_used_dirs_count =
					dir_array[i];
				ext2fs_mark_super_dirty(fs);
			} else
				ext2fs_unmark_valid(fs);
		}
	}
	if (free_inodes != fs->super->s_free_inodes_count) {
		pctx.group = -1;
		pctx.ino = fs->super->s_free_inodes_count;
		pctx.ino2 = free_inodes;

		if (fix_problem(ctx, PR_5_FREE_INODE_COUNT, &pctx)) {
			fs->super->s_free_inodes_count = free_inodes;
			ext2fs_mark_super_dirty(fs);
		} else
			ext2fs_unmark_valid(fs);
	}
	ext2fs_free_mem(&free_array);
	ext2fs_free_mem(&dir_array);
}

static void check_inode_end(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	ext2_ino_t	end, save_inodes_count, i;
	struct problem_context	pctx;

	clear_problem_context(&pctx);

	end = EXT2_INODES_PER_GROUP(fs->super) * fs->group_desc_count;
	pctx.errcode = ext2fs_fudge_inode_bitmap_end(fs->inode_map, end,
						     &save_inodes_count);
	if (pctx.errcode) {
		pctx.num = 1;
		fix_problem(ctx, PR_5_FUDGE_BITMAP_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		return;
	}
	if (save_inodes_count == end)
		return;
	
	for (i = save_inodes_count + 1; i <= end; i++) {
		if (!ext2fs_test_inode_bitmap(fs->inode_map, i)) {
			if (fix_problem(ctx, PR_5_INODE_BMAP_PADDING, &pctx)) {
				for (i = save_inodes_count + 1; i <= end; i++)
					ext2fs_mark_inode_bitmap(fs->inode_map,
								 i);
				ext2fs_mark_ib_dirty(fs);
			} else
				ext2fs_unmark_valid(fs);
			break;
		}
	}

	pctx.errcode = ext2fs_fudge_inode_bitmap_end(fs->inode_map,
						     save_inodes_count, 0);
	if (pctx.errcode) {
		pctx.num = 2;
		fix_problem(ctx, PR_5_FUDGE_BITMAP_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		return;
	}
}

static void check_block_end(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	blk_t	end, save_blocks_count, i;
	struct problem_context	pctx;

	clear_problem_context(&pctx);

	end = fs->block_map->start +
		(EXT2_BLOCKS_PER_GROUP(fs->super) * fs->group_desc_count) - 1;
	pctx.errcode = ext2fs_fudge_block_bitmap_end(fs->block_map, end,
						     &save_blocks_count);
	if (pctx.errcode) {
		pctx.num = 3;
		fix_problem(ctx, PR_5_FUDGE_BITMAP_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		return;
	}
	if (save_blocks_count == end)
		return;
	
	for (i = save_blocks_count + 1; i <= end; i++) {
		if (!ext2fs_test_block_bitmap(fs->block_map, i)) {
			if (fix_problem(ctx, PR_5_BLOCK_BMAP_PADDING, &pctx)) {
				for (i = save_blocks_count + 1; i <= end; i++)
					ext2fs_mark_block_bitmap(fs->block_map,
								 i);
				ext2fs_mark_bb_dirty(fs);
			} else
				ext2fs_unmark_valid(fs);
			break;
		}
	}

	pctx.errcode = ext2fs_fudge_block_bitmap_end(fs->block_map,
						     save_blocks_count, 0);
	if (pctx.errcode) {
		pctx.num = 4;
		fix_problem(ctx, PR_5_FUDGE_BITMAP_ERROR, &pctx);
		ctx->flags |= E2F_FLAG_ABORT; /* fatal */
		return;
	}
}



