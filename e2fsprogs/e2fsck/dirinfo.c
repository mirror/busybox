/*
 * dirinfo.c --- maintains the directory information table for e2fsck.
 *
 * Copyright (C) 1993 Theodore Ts'o.  This file may be redistributed
 * under the terms of the GNU Public License.
 */

#include "e2fsck.h"

/*
 * This subroutine is called during pass1 to create a directory info
 * entry.  During pass1, the passed-in parent is 0; it will get filled
 * in during pass2.  
 */
void e2fsck_add_dir_info(e2fsck_t ctx, ext2_ino_t ino, ext2_ino_t parent)
{
	struct dir_info *dir;
	int		i, j;
	ext2_ino_t	num_dirs;
	errcode_t	retval;
	unsigned long	old_size;

#if 0
	printf("add_dir_info for inode %lu...\n", ino);
#endif
	if (!ctx->dir_info) {
		ctx->dir_info_count = 0;
		retval = ext2fs_get_num_dirs(ctx->fs, &num_dirs);
		if (retval)
			num_dirs = 1024;	/* Guess */
		ctx->dir_info_size = num_dirs + 10;
		ctx->dir_info  = (struct dir_info *)
			e2fsck_allocate_memory(ctx, ctx->dir_info_size
					       * sizeof (struct dir_info),
					       "directory map");
	}
	
	if (ctx->dir_info_count >= ctx->dir_info_size) {
		old_size = ctx->dir_info_size * sizeof(struct dir_info);
		ctx->dir_info_size += 10;
		retval = ext2fs_resize_mem(old_size, ctx->dir_info_size *
					   sizeof(struct dir_info),
					   &ctx->dir_info);
		if (retval) {
			ctx->dir_info_size -= 10;
			return;
		}
	}

	/*
	 * Normally, add_dir_info is called with each inode in
	 * sequential order; but once in a while (like when pass 3
	 * needs to recreate the root directory or lost+found
	 * directory) it is called out of order.  In those cases, we
	 * need to move the dir_info entries down to make room, since
	 * the dir_info array needs to be sorted by inode number for
	 * get_dir_info()'s sake.
	 */
	if (ctx->dir_info_count &&
	    ctx->dir_info[ctx->dir_info_count-1].ino >= ino) {
		for (i = ctx->dir_info_count-1; i > 0; i--)
			if (ctx->dir_info[i-1].ino < ino)
				break;
		dir = &ctx->dir_info[i];
		if (dir->ino != ino) 
			for (j = ctx->dir_info_count++; j > i; j--)
				ctx->dir_info[j] = ctx->dir_info[j-1];
	} else
		dir = &ctx->dir_info[ctx->dir_info_count++];
	
	dir->ino = ino;
	dir->dotdot = parent;
	dir->parent = parent;
}

/*
 * get_dir_info() --- given an inode number, try to find the directory
 * information entry for it.
 */
struct dir_info *e2fsck_get_dir_info(e2fsck_t ctx, ext2_ino_t ino)
{
	int	low, high, mid;

	low = 0;
	high = ctx->dir_info_count-1;
	if (!ctx->dir_info)
		return 0;
	if (ino == ctx->dir_info[low].ino)
		return &ctx->dir_info[low];
	if  (ino == ctx->dir_info[high].ino)
		return &ctx->dir_info[high];

	while (low < high) {
		mid = (low+high)/2;
		if (mid == low || mid == high)
			break;
		if (ino == ctx->dir_info[mid].ino)
			return &ctx->dir_info[mid];
		if (ino < ctx->dir_info[mid].ino)
			high = mid;
		else
			low = mid;
	}
	return 0;
}

/*
 * Free the dir_info structure when it isn't needed any more.
 */
void e2fsck_free_dir_info(e2fsck_t ctx)
{
	if (ctx->dir_info) {
		ext2fs_free_mem(&ctx->dir_info);
		ctx->dir_info = 0;
	}
	ctx->dir_info_size = 0;
	ctx->dir_info_count = 0;
}

/*
 * Return the count of number of directories in the dir_info structure
 */
int e2fsck_get_num_dirinfo(e2fsck_t ctx)
{
	return ctx->dir_info_count;
}

/*
 * A simple interator function
 */
struct dir_info *e2fsck_dir_info_iter(e2fsck_t ctx, int *control)
{
	if (*control >= ctx->dir_info_count)
		return 0;

	return(ctx->dir_info + (*control)++);
}
