/*
 * util.c --- helper functions used by tune2fs and mke2fs
 * 
 * Copyright 1995, 1996, 1997, 1998, 1999, 2000 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <linux/major.h>
#include <sys/stat.h>

#include "e2fsbb.h"
#include "e2p/e2p.h"
#include "ext2fs/ext2_fs.h"
#include "ext2fs/ext2fs.h"
#include "blkid/blkid.h"
#include "util.h"

void proceed_question(void)
{
	fputs("Proceed anyway? (y,n) ", stdout);
	if (bb_ask_confirmation() == 0)
		exit(1);
}

void check_plausibility(const char *device)
{
	int val;
#ifdef CONFIG_LFS
	struct stat64 s;
	val = stat64(device, &s);
#else
	struct stat s;
	val = stat(device, &s);
#endif

	if(val == -1)
		bb_perror_msg_and_die("Could not stat %s", device);
	if (!S_ISBLK(s.st_mode)) {
		printf("%s is not a block special device.\n", device);
		proceed_question();
		return;
	}

#ifdef HAVE_LINUX_MAJOR_H
#ifndef MAJOR
#define MAJOR(dev)	((dev)>>8)
#define MINOR(dev)	((dev) & 0xff)
#endif
#ifndef SCSI_BLK_MAJOR
#ifdef SCSI_DISK0_MAJOR
#ifdef SCSI_DISK8_MAJOR
#define SCSI_DISK_MAJOR(M) ((M) == SCSI_DISK0_MAJOR || \
  ((M) >= SCSI_DISK1_MAJOR && (M) <= SCSI_DISK7_MAJOR) || \
  ((M) >= SCSI_DISK8_MAJOR && (M) <= SCSI_DISK15_MAJOR))
#else
#define SCSI_DISK_MAJOR(M) ((M) == SCSI_DISK0_MAJOR || \
  ((M) >= SCSI_DISK1_MAJOR && (M) <= SCSI_DISK7_MAJOR))
#endif /* defined(SCSI_DISK8_MAJOR) */
#define SCSI_BLK_MAJOR(M) (SCSI_DISK_MAJOR((M)) || (M) == SCSI_CDROM_MAJOR)
#else
#define SCSI_BLK_MAJOR(M)  ((M) == SCSI_DISK_MAJOR || (M) == SCSI_CDROM_MAJOR)
#endif /* defined(SCSI_DISK0_MAJOR) */
#endif /* defined(SCSI_BLK_MAJOR) */
	if (((MAJOR(s.st_rdev) == HD_MAJOR &&
	      MINOR(s.st_rdev)%64 == 0) ||
	     (SCSI_BLK_MAJOR(MAJOR(s.st_rdev)) &&
	      MINOR(s.st_rdev)%16 == 0))) {
		printf("%s is entire device, not just one partition!\n", device);
		proceed_question();
	}
#endif
}

void check_mount(const char *device, int force, const char *type)
{
	errcode_t retval;
	int mount_flags;

	retval = ext2fs_check_if_mounted(device, &mount_flags);
	if (retval) {
		bb_error_msg("Could not determine if %s is mounted", device);
		return;
	}
	if (!(mount_flags & EXT2_MF_MOUNTED))
		return;

	bb_error_msg("%s is mounted !", device);
	if (force)
		bb_error_msg("forcing anyways and ignoring /etc/mtab status");
	else
		bb_error_msg_and_die("will not make a %s here!", type);
}

void parse_journal_opts(char **journal_device, int *journal_flags, 
                        int *journal_size, const char *opts)
{
	char *buf, *token, *next, *p, *arg;
	int journal_usage = 0;
#if 0
	int	len;
	len = strlen(opts);
	buf = xmalloc(len+1);
	strcpy(buf, opts);
#else
	buf = bb_xstrdup(opts);
#endif
	for (token = buf; token && *token; token = next) {
		p = strchr(token, ',');
		next = 0;
		if (p) {
			*p = 0;
			next = p+1;
		} 
		arg = strchr(token, '=');
		if (arg) {
			*arg = 0;
			arg++;
		}
		if (strcmp(token, "device") == 0) {
			*journal_device = blkid_get_devname(NULL, arg, NULL);
			if (!journal_device) {
				journal_usage++;
				continue;
			}
		} else if (strcmp(token, "size") == 0) {
			if (!arg) {
				journal_usage++;
				continue;
			}
			(*journal_size) = strtoul(arg, &p, 0);
			if (*p)
				journal_usage++;
		} else if (strcmp(token, "v1_superblock") == 0) {
			(*journal_flags) |= EXT2_MKJOURNAL_V1_SUPER;
			continue;
		} else
			journal_usage++;
	}
	if (journal_usage)
		bb_error_msg_and_die(
			"\nBad journal options specified.\n\n"
			"Journal options are separated by commas, "
			"and may take an argument which\n"
			"\tis set off by an equals ('=') sign.\n\n"
			"Valid journal options are:\n"
			"\tsize=<journal size in megabytes>\n"
			"\tdevice=<journal device>\n\n"
			"The journal size must be between "
			"1024 and 102400 filesystem blocks.\n\n");
}	

/*
 * Determine the number of journal blocks to use, either via
 * user-specified # of megabytes, or via some intelligently selected
 * defaults.
 * 
 * Find a reasonable journal file size (in blocks) given the number of blocks
 * in the filesystem.  For very small filesystems, it is not reasonable to
 * have a journal that fills more than half of the filesystem.
 */
int figure_journal_size(int size, ext2_filsys fs)
{
	blk_t j_blocks;

	if (fs->super->s_blocks_count < 2048) {
		bb_error_msg("Filesystem too small for a journal");
		return 0;
	}

	if (size >= 0) {
		j_blocks = size * 1024 / (fs->blocksize	/ 1024);
		if (j_blocks < 1024 || j_blocks > 102400)
			bb_error_msg_and_die("\nThe requested journal "
				"size is %d blocks;\n it must be "
				"between 1024 and 102400 blocks; Aborting",
				j_blocks);
		if (j_blocks > fs->super->s_free_blocks_count)
			bb_error_msg_and_die("Journal size too big for filesystem");
		return j_blocks;
	}

	if (fs->super->s_blocks_count < 32768)
		j_blocks = 1024;
	else if (fs->super->s_blocks_count < 262144)
		j_blocks = 4096;
	else
		j_blocks = 8192;

	return j_blocks;
}

void print_check_message(ext2_filsys fs)
{
	printf("This filesystem will be automatically "
		 "checked every %d mounts or\n"
		 "%g days, whichever comes first.  "
		 "Use tune2fs -c or -i to override.\n",
	       fs->super->s_max_mnt_count,
	       (double)fs->super->s_checkinterval / (3600 * 24));
}
