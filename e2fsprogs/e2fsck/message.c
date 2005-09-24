/*
 * message.c --- print e2fsck messages (with compression)
 *
 * Copyright 1996, 1997 by Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 *
 * print_e2fsck_message() prints a message to the user, using
 * compression techniques and expansions of abbreviations.
 *
 * The following % expansions are supported:
 *
 * 	%b	<blk>			block number
 * 	%B	<blkcount>		integer
 * 	%c	<blk2>			block number
 * 	%Di	<dirent>->ino		inode number
 * 	%Dn	<dirent>->name		string
 * 	%Dr	<dirent>->rec_len
 * 	%Dl	<dirent>->name_len
 * 	%Dt	<dirent>->filetype
 * 	%d	<dir> 			inode number
 * 	%g	<group>			integer
 * 	%i	<ino>			inode number
 * 	%Is	<inode> -> i_size
 * 	%IS	<inode> -> i_extra_isize
 * 	%Ib	<inode> -> i_blocks
 * 	%Il	<inode> -> i_links_count
 * 	%Im	<inode> -> i_mode
 * 	%IM	<inode> -> i_mtime
 * 	%IF	<inode> -> i_faddr
 * 	%If	<inode> -> i_file_acl
 * 	%Id	<inode> -> i_dir_acl
 * 	%Iu	<inode> -> i_uid
 * 	%Ig	<inode> -> i_gid
 * 	%j	<ino2>			inode number
 * 	%m	<com_err error message>
 * 	%N	<num>
 *	%p	ext2fs_get_pathname of directory <ino>
 * 	%P	ext2fs_get_pathname of <dirent>->ino with <ino2> as
 * 			the containing directory.  (If dirent is NULL
 * 			then return the pathname of directory <ino2>)
 * 	%q	ext2fs_get_pathname of directory <dir>
 * 	%Q	ext2fs_get_pathname of directory <ino> with <dir> as
 * 			the containing directory.
 * 	%s	<str>			miscellaneous string
 * 	%S	backup superblock
 * 	%X	<num> hexadecimal format
 *
 * The following '@' expansions are supported:
 *
 * 	@a	extended attribute
 * 	@A	error allocating
 * 	@b	block
 * 	@B	bitmap
 * 	@c	compress
 * 	@C	conflicts with some other fs block
 * 	@D	deleted
 * 	@d	directory
 * 	@e	entry
 * 	@E	Entry '%Dn' in %p (%i)
 * 	@f	filesystem
 * 	@F	for @i %i (%Q) is
 * 	@g	group
 * 	@h	HTREE directory inode
 * 	@i	inode
 * 	@I	illegal
 * 	@j	journal
 * 	@l	lost+found
 * 	@L	is a link
 * 	@o	orphaned
 * 	@p	problem in
 * 	@r	root inode
 * 	@s	should be 
 * 	@S	superblock
 * 	@u	unattached
 * 	@v	device
 * 	@z	zero-length
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>

#include "e2fsck.h"

#include "problem.h"

#ifdef __GNUC__
#define _INLINE_ __inline__
#else
#define _INLINE_
#endif

/*
 * This structure defines the abbreviations used by the text strings
 * below.  The first character in the string is the index letter.  An
 * abbreviation of the form '@<i>' is expanded by looking up the index
 * letter <i> in the table below.
 */
static const char *abbrevs[] = {
	N_("aextended attribute"),
	N_("Aerror allocating"),
	N_("bblock"),
	N_("Bbitmap"),
	N_("ccompress"),
	N_("Cconflicts with some other fs @b"),
	N_("iinode"),
	N_("Iillegal"),
	N_("jjournal"),
	N_("Ddeleted"),
	N_("ddirectory"),
	N_("eentry"),
	N_("E@e '%Dn' in %p (%i)"),
	N_("ffilesystem"),
	N_("Ffor @i %i (%Q) is"),
	N_("ggroup"),
	N_("hHTREE @d @i"),
	N_("llost+found"),
	N_("Lis a link"),
	N_("oorphaned"),
	N_("pproblem in"),
	N_("rroot @i"),
	N_("sshould be"),
	N_("Ssuper@b"),
	N_("uunattached"),
	N_("vdevice"),
	N_("zzero-length"),
	"@@",
	0
	};

/*
 * Give more user friendly names to the "special" inodes.
 */
#define num_special_inodes	11
static const char *special_inode_name[] =
{
	N_("<The NULL inode>"),			/* 0 */
	N_("<The bad blocks inode>"),		/* 1 */
	"/",					/* 2 */
	N_("<The ACL index inode>"),		/* 3 */
	N_("<The ACL data inode>"),		/* 4 */
	N_("<The boot loader inode>"),		/* 5 */
	N_("<The undelete directory inode>"),	/* 6 */
	N_("<The group descriptor inode>"),	/* 7 */
	N_("<The journal inode>"),		/* 8 */
	N_("<Reserved inode 9>"),		/* 9 */
	N_("<Reserved inode 10>"),		/* 10 */
};

/*
 * This function does "safe" printing.  It will convert non-printable
 * ASCII characters using '^' and M- notation.
 */
static void safe_print(const char *cp, int len)
{
	unsigned char	ch;

	if (len < 0)
		len = strlen(cp);

	while (len--) {
		ch = *cp++;
		if (ch > 128) {
			fputs("M-", stdout);
			ch -= 128;
		}
		if ((ch < 32) || (ch == 0x7f)) {
			fputc('^', stdout);
			ch ^= 0x40; /* ^@, ^A, ^B; ^? for DEL */
		}
		fputc(ch, stdout);
	}
}


/*
 * This function prints a pathname, using the ext2fs_get_pathname
 * function
 */
static void print_pathname(ext2_filsys fs, ext2_ino_t dir, ext2_ino_t ino)
{
	errcode_t	retval;
	char		*path;

	if (!dir && (ino < num_special_inodes)) {
		fputs(_(special_inode_name[ino]), stdout);
		return;
	}
	
	retval = ext2fs_get_pathname(fs, dir, ino, &path);
	if (retval)
		fputs("???", stdout);
	else {
		safe_print(path, -1);
		ext2fs_free_mem(&path);
	}
}

/*
 * This function handles the '@' expansion.  We allow recursive
 * expansion; an @ expression can contain further '@' and '%'
 * expressions. 
 */
static _INLINE_ void expand_at_expression(e2fsck_t ctx, char ch,
					  struct problem_context *pctx,
					  int *first)
{
	const char **cpp, *str;
	
	/* Search for the abbreviation */
	for (cpp = abbrevs; *cpp; cpp++) {
		if (ch == *cpp[0])
			break;
	}
	if (*cpp) {
		str = _(*cpp) + 1;
		if (*first && islower(*str)) {
			*first = 0;
			fputc(toupper(*str++), stdout);
		}
		print_e2fsck_message(ctx, str, pctx, *first);
	} else
		printf("@%c", ch);
}

/*
 * This function expands '%IX' expressions
 */
static _INLINE_ void expand_inode_expression(char ch, 
					     struct problem_context *ctx)
{
	struct ext2_inode	*inode;
	struct ext2_inode_large	*large_inode;
	char *			time_str;
	time_t			t;
	int			do_gmt = -1;

	if (!ctx || !ctx->inode)
		goto no_inode;
	
	inode = ctx->inode;
	large_inode = (struct ext2_inode_large *) inode;

	switch (ch) {
	case 's':
		if (LINUX_S_ISDIR(inode->i_mode))
			printf("%u", inode->i_size);
		else {
#ifdef EXT2_NO_64_TYPE
			if (inode->i_size_high)
				printf("0x%x%08x", inode->i_size_high,
				       inode->i_size);
			else
				printf("%u", inode->i_size);
#else
			printf("%llu", (inode->i_size | 
					((__u64) inode->i_size_high << 32)));
#endif
		}
		break;
	case 'S':
		printf("%u", large_inode->i_extra_isize);
		break;
	case 'b':
		printf("%u", inode->i_blocks);
		break;
	case 'l':
		printf("%d", inode->i_links_count);
		break;
	case 'm':
		printf("0%o", inode->i_mode);
		break;
	case 'M':
		/* The diet libc doesn't respect the TZ environemnt variable */
		if (do_gmt == -1) {
			time_str = getenv("TZ");
			if (!time_str)
				time_str = "";
			do_gmt = !strcmp(time_str, "GMT");
		}
		t = inode->i_mtime;
		time_str = asctime(do_gmt ? gmtime(&t) : localtime(&t));
		printf("%.24s", time_str);
		break;
	case 'F':
		printf("%u", inode->i_faddr);
		break;
	case 'f':
		printf("%u", inode->i_file_acl);
		break;
	case 'd':
		printf("%u", (LINUX_S_ISDIR(inode->i_mode) ?
			      inode->i_dir_acl : 0));
		break;
	case 'u':
		printf("%d", (inode->i_uid |
			      (inode->osd2.linux2.l_i_uid_high << 16)));
		break;
	case 'g':
		printf("%d", (inode->i_gid |
			      (inode->osd2.linux2.l_i_gid_high << 16)));
		break;
	default:
	no_inode:
		printf("%%I%c", ch);
		break;
	}
}

/*
 * This function expands '%dX' expressions
 */
static _INLINE_ void expand_dirent_expression(char ch,
					      struct problem_context *ctx)
{
	struct ext2_dir_entry	*dirent;
	int	len;
	
	if (!ctx || !ctx->dirent)
		goto no_dirent;
	
	dirent = ctx->dirent;
	
	switch (ch) {
	case 'i':
		printf("%u", dirent->inode);
		break;
	case 'n':
		len = dirent->name_len & 0xFF;
		if (len > EXT2_NAME_LEN)
			len = EXT2_NAME_LEN;
		if (len > dirent->rec_len)
			len = dirent->rec_len;
		safe_print(dirent->name, len);
		break;
	case 'r':
		printf("%u", dirent->rec_len);
		break;
	case 'l':
		printf("%u", dirent->name_len & 0xFF);
		break;
	case 't':
		printf("%u", dirent->name_len >> 8);
		break;
	default:
	no_dirent:
		printf("%%D%c", ch);
		break;
	}
}

static _INLINE_ void expand_percent_expression(ext2_filsys fs, char ch,
					       struct problem_context *ctx)
{
	if (!ctx)
		goto no_context;
	
	switch (ch) {
	case '%':
		fputc('%', stdout);
		break;
	case 'b':
		printf("%u", ctx->blk);
		break;
	case 'B':
#ifdef EXT2_NO_64_TYPE
		printf("%d", ctx->blkcount);
#else
		printf("%lld", ctx->blkcount);
#endif
		break;
	case 'c':
		printf("%u", ctx->blk2);
		break;
	case 'd':
		printf("%u", ctx->dir);
		break;
	case 'g':
		printf("%d", ctx->group);
		break;
	case 'i':
		printf("%u", ctx->ino);
		break;
	case 'j':
		printf("%u", ctx->ino2);
		break;
	case 'm':
		printf("%s", error_message(ctx->errcode));
		break;
	case 'N':
#ifdef EXT2_NO_64_TYPE
		printf("%u", ctx->num);
#else
		printf("%llu", ctx->num);
#endif
		break;
	case 'p':
		print_pathname(fs, ctx->ino, 0);
		break;
	case 'P':
		print_pathname(fs, ctx->ino2,
			       ctx->dirent ? ctx->dirent->inode : 0);
		break;
	case 'q':
		print_pathname(fs, ctx->dir, 0);
		break;
	case 'Q':
		print_pathname(fs, ctx->dir, ctx->ino);
		break;
	case 'S':
		printf("%d", get_backup_sb(NULL, fs, NULL, NULL));
		break;
	case 's':
		printf("%s", ctx->str ? ctx->str : "NULL");
		break;
	case 'X':
#ifdef EXT2_NO_64_TYPE
		printf("0x%x", ctx->num);
#else
		printf("0x%llx", ctx->num);
#endif
		break;
	default:
	no_context:
		printf("%%%c", ch);
		break;
	}
}	

void print_e2fsck_message(e2fsck_t ctx, const char *msg,
			  struct problem_context *pctx, int first)
{
	ext2_filsys fs = ctx->fs;
	const char *	cp;
	int		i;

	e2fsck_clear_progbar(ctx);
	for (cp = msg; *cp; cp++) {
		if (cp[0] == '@') {
			cp++;
			expand_at_expression(ctx, *cp, pctx, &first);
		} else if (cp[0] == '%' && cp[1] == 'I') {
			cp += 2;
			expand_inode_expression(*cp, pctx);
		} else if (cp[0] == '%' && cp[1] == 'D') {
			cp += 2;
			expand_dirent_expression(*cp, pctx);
		} else if ((cp[0] == '%')) {
			cp++;
			expand_percent_expression(fs, *cp, pctx);
		} else {
			for (i=0; cp[i]; i++)
				if ((cp[i] == '@') || cp[i] == '%')
					break;
			printf("%.*s", i, cp);
			cp += i-1;
		}
		first = 0;
	}
}
