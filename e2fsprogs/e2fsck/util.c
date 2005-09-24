/*
 * util.c --- miscellaneous utilities
 * 
 * Copyright (C) 1993, 1994, 1995, 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_CONIO_H
#undef HAVE_TERMIOS_H
#include <conio.h>
#define read_a_char()	getch()
#else
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#include <stdio.h>
#endif

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "e2fsck.h"

extern e2fsck_t e2fsck_global_ctx;   /* Try your very best not to use this! */

#include <sys/time.h>
#include <sys/resource.h>

#if 0
void fatal_error(e2fsck_t ctx, const char *msg)
{
	if (msg) 
		fprintf (stderr, "e2fsck: %s\n", msg);
	if (ctx->fs && ctx->fs->io) {
		if (ctx->fs->io->magic == EXT2_ET_MAGIC_IO_CHANNEL)
			io_channel_flush(ctx->fs->io);
		else
			fprintf(stderr, "e2fsck: io manager magic bad!\n");
	}
	ctx->flags |= E2F_FLAG_ABORT;
	if (ctx->flags & E2F_FLAG_SETJMP_OK)
		longjmp(ctx->abort_loc, 1);
	exit(FSCK_ERROR);
}
#endif

void *e2fsck_allocate_memory(e2fsck_t ctx, unsigned int size,
			     const char *description)
{
	void *ret;
	char buf[256];

#ifdef DEBUG_ALLOCATE_MEMORY
	printf("Allocating %d bytes for %s...\n", size, description);
#endif
	ret = malloc(size);
	if (!ret) {
		sprintf(buf, "Can't allocate %s\n", description);
		fatal_error(ctx, buf);
	}
	memset(ret, 0, size);
	return ret;
}

char *string_copy(e2fsck_t ctx EXT2FS_ATTR((unused)), 
		  const char *str, int len)
{
	char	*ret;
	
	if (!str)
		return NULL;
	if (!len)
		len = strlen(str);
	ret = malloc(len+1);
	if (ret) {
		strncpy(ret, str, len);
		ret[len] = 0;
	}
	return ret;
}

#ifndef HAVE_STRNLEN
/*
 * Incredibly, libc5 doesn't appear to have strnlen.  So we have to
 * provide our own.
 */
int e2fsck_strnlen(const char * s, int count)
{
	const char *cp = s;

	while (count-- && *cp)
		cp++;
	return cp - s;
}
#endif

#ifndef HAVE_CONIO_H
static int read_a_char(void)
{
	char	c;
	int	r;
	int	fail = 0;

	while(1) {
		if (e2fsck_global_ctx &&
		    (e2fsck_global_ctx->flags & E2F_FLAG_CANCEL)) {
			return 3;
		}
		r = read(0, &c, 1);
		if (r == 1)
			return c;
		if (fail++ > 100)
			break;
	}
	return EOF;
}
#endif

int ask_yn(const char * string, int def)
{
	int		c;
	const char	*defstr;
	const char	*short_yes = _("yY");
	const char	*short_no = _("nN");

#ifdef HAVE_TERMIOS_H
	struct termios	termios, tmp;

	tcgetattr (0, &termios);
	tmp = termios;
	tmp.c_lflag &= ~(ICANON | ECHO);
	tmp.c_cc[VMIN] = 1;
	tmp.c_cc[VTIME] = 0;
	tcsetattr (0, TCSANOW, &tmp);
#endif

	if (def == 1)
		defstr = _(_("<y>"));
	else if (def == 0)
		defstr = _(_("<n>"));
	else
		defstr = _(" (y/n)");
	printf("%s%s? ", string, defstr);
	while (1) {
		fflush (stdout);
		if ((c = read_a_char()) == EOF)
			break;
		if (c == 3) {
#ifdef HAVE_TERMIOS_H
			tcsetattr (0, TCSANOW, &termios);
#endif
			if (e2fsck_global_ctx &&
			    e2fsck_global_ctx->flags & E2F_FLAG_SETJMP_OK) {
				puts("\n");
				longjmp(e2fsck_global_ctx->abort_loc, 1);
			}
			puts(_("cancelled!\n"));
			return 0;
		}
		if (strchr(short_yes, (char) c)) {
			def = 1;
			break;
		}
		else if (strchr(short_no, (char) c)) {
			def = 0;
			break;
		}
		else if ((c == ' ' || c == '\n') && (def != -1))
			break;
	}
	if (def)
		puts(_("yes\n"));
	else
		puts (_("no\n"));
#ifdef HAVE_TERMIOS_H
	tcsetattr (0, TCSANOW, &termios);
#endif
	return def;
}

int ask (e2fsck_t ctx, const char * string, int def)
{
	if (ctx->options & E2F_OPT_NO) {
		printf (_("%s? no\n\n"), string);
		return 0;
	}
	if (ctx->options & E2F_OPT_YES) {
		printf (_("%s? yes\n\n"), string);
		return 1;
	}
	if (ctx->options & E2F_OPT_PREEN) {
		printf ("%s? %s\n\n", string, def ? _("yes") : _("no"));
		return def;
	}
	return ask_yn(string, def);
}

void e2fsck_read_bitmaps(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	errcode_t	retval;

	if (ctx->invalid_bitmaps) {
		com_err(ctx->program_name, 0,
		    _("e2fsck_read_bitmaps: illegal bitmap block(s) for %s"),
			ctx->device_name);
		fatal_error(ctx, 0);
	}

	ehandler_operation(_("reading inode and block bitmaps"));
	retval = ext2fs_read_bitmaps(fs);
	ehandler_operation(0);
	if (retval) {
		com_err(ctx->program_name, retval,
			_("while retrying to read bitmaps for %s"),
			ctx->device_name);
		fatal_error(ctx, 0);
	}
}

void e2fsck_write_bitmaps(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	errcode_t	retval;

	if (ext2fs_test_bb_dirty(fs)) {
		ehandler_operation(_("writing block bitmaps"));
		retval = ext2fs_write_block_bitmap(fs);
		ehandler_operation(0);
		if (retval) {
			com_err(ctx->program_name, retval,
			    _("while retrying to write block bitmaps for %s"),
				ctx->device_name);
			fatal_error(ctx, 0);
		}
	}

	if (ext2fs_test_ib_dirty(fs)) {
		ehandler_operation(_("writing inode bitmaps"));
		retval = ext2fs_write_inode_bitmap(fs);
		ehandler_operation(0);
		if (retval) {
			com_err(ctx->program_name, retval,
			    _("while retrying to write inode bitmaps for %s"),
				ctx->device_name);
			fatal_error(ctx, 0);
		}
	}
}

void preenhalt(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;

	if (!(ctx->options & E2F_OPT_PREEN))
		return;
	fprintf(stderr, _("\n\n%s: UNEXPECTED INCONSISTENCY; "
		"RUN fsck MANUALLY.\n\t(i.e., without -a or -p options)\n"),
	       ctx->device_name);
	if (fs != NULL) {
		fs->super->s_state |= EXT2_ERROR_FS;
		ext2fs_mark_super_dirty(fs);
		ext2fs_close(fs);
	}
	exit(FSCK_UNCORRECTED);
}

#ifdef RESOURCE_TRACK
void init_resource_track(struct resource_track *track)
{
#ifdef HAVE_GETRUSAGE
	struct rusage r;
#endif
	
	track->brk_start = sbrk(0);
	gettimeofday(&track->time_start, 0);
#ifdef HAVE_GETRUSAGE
#ifdef sun
	memset(&r, 0, sizeof(struct rusage));
#endif
	getrusage(RUSAGE_SELF, &r);
	track->user_start = r.ru_utime;
	track->system_start = r.ru_stime;
#else
	track->user_start.tv_sec = track->user_start.tv_usec = 0;
	track->system_start.tv_sec = track->system_start.tv_usec = 0;
#endif
}

#ifdef __GNUC__
#define _INLINE_ __inline__
#else
#define _INLINE_
#endif

static _INLINE_ float timeval_subtract(struct timeval *tv1,
				       struct timeval *tv2)
{
	return ((tv1->tv_sec - tv2->tv_sec) +
		((float) (tv1->tv_usec - tv2->tv_usec)) / 1000000);
}

void print_resource_track(const char *desc, struct resource_track *track)
{
#ifdef HAVE_GETRUSAGE
	struct rusage r;
#endif
#ifdef HAVE_MALLINFO
	struct mallinfo	malloc_info;
#endif
	struct timeval time_end;

	gettimeofday(&time_end, 0);

	if (desc)
		printf("%s: ", desc);

#ifdef HAVE_MALLINFO
#define kbytes(x)	(((x) + 1023) / 1024)
	
	malloc_info = mallinfo();
	printf(_("Memory used: %dk/%dk (%dk/%dk), "),
	       kbytes(malloc_info.arena), kbytes(malloc_info.hblkhd),
	       kbytes(malloc_info.uordblks), kbytes(malloc_info.fordblks));
#else
	printf(_("Memory used: %d, "),
	       (int) (((char *) sbrk(0)) - ((char *) track->brk_start)));
#endif	
#ifdef HAVE_GETRUSAGE
	getrusage(RUSAGE_SELF, &r);

	printf(_("time: %5.2f/%5.2f/%5.2f\n"),
	       timeval_subtract(&time_end, &track->time_start),
	       timeval_subtract(&r.ru_utime, &track->user_start),
	       timeval_subtract(&r.ru_stime, &track->system_start));
#else
	printf(_("elapsed time: %6.3f\n"),
	       timeval_subtract(&time_end, &track->time_start));
#endif
}
#endif /* RESOURCE_TRACK */

void e2fsck_read_inode(e2fsck_t ctx, unsigned long ino,
			      struct ext2_inode * inode, const char *proc)
{
	int retval;

	retval = ext2fs_read_inode(ctx->fs, ino, inode);
	if (retval) {
		com_err("ext2fs_read_inode", retval,
			_("while reading inode %ld in %s"), ino, proc);
		fatal_error(ctx, 0);
	}
}

extern void e2fsck_write_inode_full(e2fsck_t ctx, unsigned long ino,
			       struct ext2_inode * inode, int bufsize,
			       const char *proc)
{
	int retval;

	retval = ext2fs_write_inode_full(ctx->fs, ino, inode, bufsize);
	if (retval) {
		com_err("ext2fs_write_inode", retval,
			_("while writing inode %ld in %s"), ino, proc);
		fatal_error(ctx, 0);
	}
}

extern void e2fsck_write_inode(e2fsck_t ctx, unsigned long ino,
			       struct ext2_inode * inode, const char *proc)
{
	int retval;

	retval = ext2fs_write_inode(ctx->fs, ino, inode);
	if (retval) {
		com_err("ext2fs_write_inode", retval,
			_("while writing inode %ld in %s"), ino, proc);
		fatal_error(ctx, 0);
	}
}

#ifdef MTRACE
void mtrace_print(char *mesg)
{
	FILE	*malloc_get_mallstream();
	FILE	*f = malloc_get_mallstream();

	if (f)
		fprintf(f, "============= %s\n", mesg);
}
#endif

blk_t get_backup_sb(e2fsck_t ctx, ext2_filsys fs, const char *name,
		   io_manager manager)
{
	struct ext2_super_block *sb;
	io_channel		io = NULL;
	void			*buf = NULL;
	int			blocksize;
	blk_t			superblock, ret_sb = 8193;
	
	if (fs && fs->super) {
		ret_sb = (fs->super->s_blocks_per_group +
			  fs->super->s_first_data_block);
		if (ctx) {
			ctx->superblock = ret_sb;
			ctx->blocksize = fs->blocksize;
		}
		return ret_sb;
	}
		
	if (ctx) {
		if (ctx->blocksize) {
			ret_sb = ctx->blocksize * 8;
			if (ctx->blocksize == 1024)
				ret_sb++;
			ctx->superblock = ret_sb;
			return ret_sb;
		}
		ctx->superblock = ret_sb;
		ctx->blocksize = 1024;
	}

	if (!name || !manager)
		goto cleanup;

	if (manager->open(name, 0, &io) != 0)
		goto cleanup;

	if (ext2fs_get_mem(SUPERBLOCK_SIZE, &buf))
		goto cleanup;
	sb = (struct ext2_super_block *) buf;

	for (blocksize = EXT2_MIN_BLOCK_SIZE;
	     blocksize <= EXT2_MAX_BLOCK_SIZE ; blocksize *= 2) {
		superblock = blocksize*8;
		if (blocksize == 1024)
			superblock++;
		io_channel_set_blksize(io, blocksize);
		if (io_channel_read_blk(io, superblock,
					-SUPERBLOCK_SIZE, buf))
			continue;
#ifdef EXT2FS_ENABLE_SWAPFS
		if (sb->s_magic == ext2fs_swab16(EXT2_SUPER_MAGIC))
			ext2fs_swap_super(sb);
#endif
		if (sb->s_magic == EXT2_SUPER_MAGIC) {
			ret_sb = superblock;
			if (ctx) {
				ctx->superblock = superblock;
				ctx->blocksize = blocksize;
			}
			break;
		}
	}

cleanup:
	if (io)
		io_channel_close(io);
	if (buf)
		ext2fs_free_mem(&buf);
	return (ret_sb);
}

/*
 * Given a mode, return the ext2 file type
 */
int ext2_file_type(unsigned int mode)
{
	if (LINUX_S_ISREG(mode))
		return EXT2_FT_REG_FILE;

	if (LINUX_S_ISDIR(mode))
		return EXT2_FT_DIR;
	
	if (LINUX_S_ISCHR(mode))
		return EXT2_FT_CHRDEV;
	
	if (LINUX_S_ISBLK(mode))
		return EXT2_FT_BLKDEV;
	
	if (LINUX_S_ISLNK(mode))
		return EXT2_FT_SYMLINK;

	if (LINUX_S_ISFIFO(mode))
		return EXT2_FT_FIFO;
	
	if (LINUX_S_ISSOCK(mode))
		return EXT2_FT_SOCK;
	
	return 0;
}
