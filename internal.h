/*
 * Busybox main internal header file
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Based in part on code from sash, Copyright (c) 1999 by David I. Bell 
 * Permission has been granted to redistribute this code under the GPL.
 *
 */
#ifndef	_INTERNAL_H_
#define	_INTERNAL_H_

#include "busybox.def.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <mntent.h>


/* Some useful definitions */
#define FALSE   ((int) 1)
#define TRUE    ((int) 0)

#define PATH_LEN        1024
#define BUF_SIZE        8192
#define EXPAND_ALLOC    1024

#define isBlank(ch)     (((ch) == ' ') || ((ch) == '\t'))
#define isDecimal(ch)   (((ch) >= '0') && ((ch) <= '9'))
#define isOctal(ch)     (((ch) >= '0') && ((ch) <= '7'))
#define isWildCard(ch)  (((ch) == '*') || ((ch) == '?') || ((ch) == '['))


struct Applet {
	const	char*	name;
	int	(*main)(int argc, char** argv);
};

extern int busybox_main(int argc, char** argv);
extern int block_device_main(int argc, char** argv);
extern int cat_main(int argc, char** argv);
extern int more_main(int argc, char** argv);
extern int cp_main(int argc, char** argv);
extern int chmod_chown_chgrp_main(int argc, char** argv);
extern int chroot_main(int argc, char** argv);
extern int chvt_main(int argc, char** argv);
extern int clear_main(int argc, char** argv);
extern int date_main(int argc, char** argv);
extern int dd_main(int argc, char** argv);
extern int deallocvt_main(int argc, char** argv);
extern int df_main(int argc, char** argv);
extern int dmesg_main(int argc, char** argv);
extern int du_main(int argc, char** argv);
extern int dutmp_main(int argc, char** argv);
extern int false_main(int argc, char** argv);
extern int fbset_main(int argc, char** argv);
extern int fdisk_main(int argc, char** argv);
extern int fdflush_main(int argc, char **argv);
extern int fsck_minix_main(int argc, char **argv);
extern int find_main(int argc, char** argv);
extern int free_main(int argc, char** argv);
extern int grep_main(int argc, char** argv);
extern int halt_main(int argc, char** argv);
extern int head_main(int argc, char** argv);
extern int hostname_main(int argc, char** argv);
extern int init_main(int argc, char** argv);
extern int insmod_main(int argc, char** argv);
extern int kill_main(int argc, char** argv);
extern int length_main(int argc, char** argv);
extern int ln_main(int argc, char** argv);
extern int loadfont_main(int argc, char** argv);
extern int loadkmap_main(int argc, char** argv);
extern int losetup_main(int argc, char** argv);
extern int ls_main(int argc, char** argv);
extern int lsmod_main(int argc, char** argv);
extern int makedevs_main(int argc, char** argv);
extern int math_main(int argc, char** argv);
extern int mkdir_main(int argc, char** argv);
extern int mkfifo_main(int argc, char **argv);
extern int mkfs_minix_main(int argc, char **argv);
extern int mknod_main(int argc, char** argv);
extern int mkswap_main(int argc, char** argv);
extern int mnc_main(int argc, char** argv);
extern int mount_main(int argc, char** argv);
extern int mt_main(int argc, char** argv);
extern int mv_main(int argc, char** argv);
extern int ping_main(int argc, char **argv);
extern int poweroff_main(int argc, char **argv);
extern int printf_main(int argc, char** argv);
extern int ps_main(int argc, char** argv);
extern int pwd_main(int argc, char** argv);
extern int reboot_main(int argc, char** argv);
extern int rm_main(int argc, char** argv);
extern int rmdir_main(int argc, char **argv);
extern int rmmod_main(int argc, char** argv);
extern int scan_partitions_main(int argc, char** argv);
extern int sh_main(int argc, char** argv);
extern int sfdisk_main(int argc, char** argv);
extern int sed_main(int argc, char** argv);
extern int sleep_main(int argc, char** argv);
extern int sort_main(int argc, char** argv);
extern int swap_on_off_main(int argc, char** argv);
extern int sync_main(int argc, char** argv);
extern int syslogd_main(int argc, char **argv);
extern int logger_main(int argc, char **argv);
extern int tar_main(int argc, char** argv);
extern int tail_main(int argc, char** argv);
extern int tee_main(int argc, char** argv);
extern int touch_main(int argc, char** argv);
extern int tput_main(int argc, char** argv);
extern int true_main(int argc, char** argv);
extern int tryopen_main(int argc, char** argv);
extern int umount_main(int argc, char** argv);
extern int update_main(int argc, char** argv);
extern int uname_main(int argc, char** argv);
extern int gunzip_main (int argc, char** argv);
extern int gzip_main(int argc, char** argv);
extern int loadacm_main(int argc, char** argv);


const char *modeString(int mode);
const char *timeString(time_t timeVal);
int isDirectory(const char *name);
int isDevice(const char *name);
int copyFile(const char *srcName, const char *destName, int setModes,
	        int followLinks);
char *buildName(const char *dirName, const char *fileName);
int makeString(int argc, const char **argv, char *buf, int bufLen);
char *getChunk(int size);
char *chunkstrdup(const char *str);
void freeChunks(void);
int fullWrite(int fd, const char *buf, int len);
int fullRead(int fd, char *buf, int len);
int recursiveAction(const char *fileName, int recurse, int followLinks, int depthFirst,
	  int (*fileAction) (const char *fileName, struct stat* statbuf),
	  int (*dirAction) (const char *fileName, struct stat* statbuf));
const char* timeString(time_t timeVal);

extern void createPath (const char *name, int mode);
extern int parse_mode( const char* s, mode_t* theMode);
extern void usage(const char *usage) __attribute__ ((noreturn));

extern uid_t my_getpwnam(char *name);
extern gid_t my_getgrnam(char *name); 
extern void my_getpwuid(char* name, uid_t uid);
extern void my_getgrgid(char* group, gid_t gid);
extern int get_kernel_revision();
extern int get_console_fd(char* tty_name);
extern struct mntent *findMountPoint(const char *name, const char *table);
extern void write_mtab(char* blockDevice, char* directory, 
	char* filesystemType, long flags, char* string_flags);
extern void erase_mtab(const char * name);
extern int check_wildcard_match(const char* text, const char* pattern);
extern long getNum (const char *cp);
extern pid_t findInitPid();

#if (__GLIBC__ < 2) && defined BB_SYSLOGD
extern int vdprintf(int d, const char *format, va_list ap);
#endif

#if defined BB_NFSMOUNT
int nfsmount(const char *spec, const char *node, unsigned long *flags,
	char **extra_opts, char **mount_opts, int running_bg);
#endif

#if defined (BB_FSCK_MINIX) || defined (BB_MKFS_MINIX)

static inline int bit(char * addr,unsigned int nr) 
{
  return (addr[nr >> 3] & (1<<(nr & 7))) != 0;
}

static inline int setbit(char * addr,unsigned int nr)
{
  int __res = bit(addr, nr);
  addr[nr >> 3] |= (1<<(nr & 7));
  return __res != 0; \
}

static inline int clrbit(char * addr,unsigned int nr)
{
  int __res = bit(addr, nr);
  addr[nr >> 3] &= ~(1<<(nr & 7));
  return __res != 0;
}

#endif /* inline bitops junk */



#endif /* _INTERNAL_H_ */

