/* vi: set sw=4 ts=4: */
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
#ifndef	_BB_INTERNAL_H_
#define	_BB_INTERNAL_H_    1

#include "Config.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <mntent.h>
#include <regex.h>
/* for the _syscall() macros */
#include <sys/syscall.h>
#include <linux/unistd.h>


/* Some useful definitions */
#define FALSE   ((int) 1)
#define TRUE    ((int) 0)

/* for mtab.c */
#define MTAB_GETMOUNTPT '1'
#define MTAB_GETDEVICE  '2'

#define BUF_SIZE        8192
#define EXPAND_ALLOC    1024


#define isBlank(ch)     (((ch) == ' ') || ((ch) == '\t'))
#define isDecimal(ch)   (((ch) >= '0') && ((ch) <= '9'))
#define isOctal(ch)     (((ch) >= '0') && ((ch) <= '7'))
#define isWildCard(ch)  (((ch) == '*') || ((ch) == '?') || ((ch) == '['))

/* Macros for min/max.  */
#ifndef MIN
#define	MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
#define	MAX(a,b) (((a)>(b))?(a):(b))
#endif


/* I don't like nested includes, but the string and io functions are used
 * too often
 */
#include <stdio.h>
#if !defined(NO_STRING_H) || defined(STDC_HEADERS)
#  include <string.h>
#  if !defined(STDC_HEADERS) && !defined(NO_MEMORY_H) && !defined(__GNUC__)
#    include <memory.h>
#  endif
#  define memzero(s, n)     memset ((void *)(s), 0, (n))
#else
#  include <strings.h>
#  define strchr            index
#  define strrchr           rindex
#  define memcpy(d, s, n)   bcopy((s), (d), (n))
#  define memcmp(s1, s2, n) bcmp((s1), (s2), (n))
#  define memzero(s, n)     bzero((s), (n))
#endif


enum Location {
	_BB_DIR_ROOT = 0,
	_BB_DIR_BIN,
	_BB_DIR_SBIN,
	_BB_DIR_USR_BIN,
	_BB_DIR_USR_SBIN
};

struct BB_applet {
	const	char*	name;
	int	(*main)(int argc, char** argv);
	enum	Location	location;
	const	char*	usage;
};
/* From busybox.c */
extern const struct BB_applet applets[];

extern int ar_main(int argc, char **argv);
extern int basename_main(int argc, char **argv);
extern int bogomips_main(int argc, char **argv);
extern int busybox_main(int argc, char** argv);
extern int cat_main(int argc, char** argv);
extern int chmod_chown_chgrp_main(int argc, char** argv);
extern int chroot_main(int argc, char** argv);
extern int chvt_main(int argc, char** argv);
extern int clear_main(int argc, char** argv);
extern int cmp_main(int argc, char** argv);
extern int cp_mv_main(int argc, char** argv);
extern int cut_main(int argc, char** argv);
extern int date_main(int argc, char** argv);
extern int dc_main(int argc, char** argv);
extern int dd_main(int argc, char** argv);
extern int dirname_main(int argc, char** argv);
extern int deallocvt_main(int argc, char** argv);
extern int df_main(int argc, char** argv);
extern int dmesg_main(int argc, char** argv);
extern int dos2unix_main(int argc, char** argv);
extern int du_main(int argc, char** argv);
extern int dumpkmap_main(int argc, char** argv);
extern int dutmp_main(int argc, char** argv);
extern int echo_main(int argc, char** argv);
extern int expr_main(int argc, char** argv);
extern int false_main(int argc, char** argv);
extern int fbset_main(int argc, char** argv);
extern int fdisk_main(int argc, char** argv);
extern int fdflush_main(int argc, char **argv);
extern int fsck_minix_main(int argc, char **argv);
extern int find_main(int argc, char** argv);
extern int free_main(int argc, char** argv);
extern int freeramdisk_main(int argc, char** argv);
extern int getopt_main(int argc, char** argv);
extern int grep_main(int argc, char** argv);
extern int gunzip_main (int argc, char** argv);
extern int gzip_main(int argc, char** argv);
extern int halt_main(int argc, char** argv);
extern int head_main(int argc, char** argv);
extern int hostid_main(int argc, char** argv);
extern int hostname_main(int argc, char** argv);
extern int id_main(int argc, char** argv);
extern int init_main(int argc, char** argv);
extern int insmod_main(int argc, char** argv);
extern int kill_main(int argc, char** argv);
extern int length_main(int argc, char** argv);
extern int ln_main(int argc, char** argv);
extern int loadacm_main(int argc, char** argv);
extern int loadfont_main(int argc, char** argv);
extern int loadkmap_main(int argc, char** argv);
extern int losetup_main(int argc, char** argv);
extern int logger_main(int argc, char **argv);
extern int logname_main(int argc, char **argv);
extern int ls_main(int argc, char** argv);
extern int lsmod_main(int argc, char** argv);
extern int makedevs_main(int argc, char** argv);
extern int md5sum_main(int argc, char** argv);
extern int mkdir_main(int argc, char** argv);
extern int mkfifo_main(int argc, char **argv);
extern int mkfs_minix_main(int argc, char **argv);
extern int mknod_main(int argc, char** argv);
extern int mkswap_main(int argc, char** argv);
extern int mktemp_main(int argc, char **argv);
extern int nc_main(int argc, char** argv);
extern int more_main(int argc, char** argv);
extern int mount_main(int argc, char** argv);
extern int mt_main(int argc, char** argv);
extern int nslookup_main(int argc, char **argv);
extern int ping_main(int argc, char **argv);
extern int poweroff_main(int argc, char **argv);
extern int printf_main(int argc, char** argv);
extern int ps_main(int argc, char** argv);
extern int pwd_main(int argc, char** argv);
extern int rdate_main(int argc, char** argv);
extern int readlink_main(int argc, char** argv);
extern int reboot_main(int argc, char** argv);
extern int renice_main(int argc, char** argv);
extern int reset_main(int argc, char** argv);
extern int rm_main(int argc, char** argv);
extern int rmdir_main(int argc, char **argv);
extern int rmmod_main(int argc, char** argv);
extern int sed_main(int argc, char** argv);
extern int sfdisk_main(int argc, char** argv);
extern int setkeycodes_main(int argc, char** argv);
extern int shell_main(int argc, char** argv);
extern int sleep_main(int argc, char** argv);
extern int sort_main(int argc, char** argv);
extern int swap_on_off_main(int argc, char** argv);
extern int sync_main(int argc, char** argv);
extern int syslogd_main(int argc, char **argv);
extern int tail_main(int argc, char** argv);
extern int tar_main(int argc, char** argv);
extern int tee_main(int argc, char** argv);
extern int test_main(int argc, char** argv);
extern int telnet_main(int argc, char** argv);
extern int touch_main(int argc, char** argv);
extern int tr_main(int argc, char** argv);
extern int true_main(int argc, char** argv);
extern int tput_main(int argc, char** argv);
extern int tryopen_main(int argc, char** argv);
extern int tty_main(int argc, char** argv);
extern int umount_main(int argc, char** argv);
extern int uname_main(int argc, char** argv);
extern int uniq_main(int argc, char** argv);
extern int unix2dos_main(int argc, char** argv);
extern int unrpm_main(int argc, char** argv);
extern int update_main(int argc, char** argv);
extern int uptime_main(int argc, char** argv);
extern int usleep_main(int argc, char** argv);
extern int uuencode_main(int argc, char** argv);
extern int uudecode_main(int argc, char** argv);
extern int wc_main(int argc, char** argv);
extern int wget_main(int argc, char** argv);
extern int which_main(int argc, char** argv);
extern int whoami_main(int argc, char** argv);
extern int xargs_main(int argc, char** argv);
extern int yes_main(int argc, char** argv);

extern const char ar_usage[];
extern const char basename_usage[];
extern const char cat_usage[];
extern const char chgrp_usage[];
extern const char chmod_usage[];
extern const char chown_usage[];
extern const char chroot_usage[];
extern const char chvt_usage[];
extern const char clear_usage[];
extern const char cmp_usage[];
extern const char cp_usage[];
extern const char cut_usage[];
extern const char date_usage[];
extern const char dc_usage[];
extern const char dd_usage[];
extern const char deallocvt_usage[];
extern const char df_usage[];
extern const char dirname_usage[];
extern const char dmesg_usage[];
extern const char dos2unix_usage[];
extern const char du_usage[];
extern const char dumpkmap_usage[];
extern const char dutmp_usage[];
extern const char echo_usage[];
extern const char expr_usage[];
extern const char false_usage[];
extern const char fdflush_usage[];
extern const char find_usage[];
extern const char free_usage[];
extern const char freeramdisk_usage[];
extern const char fsck_minix_usage[];
extern const char getopt_usage[];
extern const char grep_usage[];
extern const char gunzip_usage[];
extern const char gzip_usage[];
extern const char halt_usage[];
extern const char head_usage[];
extern const char hostid_usage[];
extern const char hostname_usage[];
extern const char id_usage[];
extern const char insmod_usage[];
extern const char kill_usage[];
extern const char killall_usage[];
extern const char length_usage[];
extern const char ln_usage[];
extern const char loadacm_usage[];
extern const char loadfont_usage[];
extern const char loadkmap_usage[];
extern const char logger_usage[];
extern const char logname_usage[];
extern const char ls_usage[];
extern const char lsmod_usage[];
extern const char makedevs_usage[];
extern const char md5sum_usage[];
extern const char mkdir_usage[];
extern const char mkfifo_usage[];
extern const char mkfs_minix_usage[];
extern const char mknod_usage[];
extern const char mkswap_usage[];
extern const char mktemp_usage[];
extern const char more_usage[];
extern const char mount_usage[]; 
extern const char mt_usage[];
extern const char mv_usage[];
extern const char nc_usage[];
extern const char nslookup_usage[];
extern const char ping_usage[];
extern const char poweroff_usage[];
extern const char printf_usage[];
extern const char ps_usage[];
extern const char pwd_usage[];
extern const char rdate_usage[];
extern const char readlink_usage[];
extern const char reboot_usage[];
extern const char renice_usage[];
extern const char reset_usage[];
extern const char rm_usage[];
extern const char rmdir_usage[];
extern const char rmmod_usage[];
extern const char sed_usage[];
extern const char setkeycodes_usage[];
extern const char shell_usage[];
extern const char sleep_usage[];
extern const char sort_usage[];
extern const char swapoff_usage[];
extern const char swapon_usage[];
extern const char sync_usage[];
extern const char syslogd_usage[];
extern const char tail_usage[];
extern const char tar_usage[];
extern const char tee_usage[];
extern const char telnet_usage[];
extern const char test_usage[];
extern const char touch_usage[];
extern const char tr_usage[];
extern const char true_usage[];
extern const char tty_usage[];
extern const char umount_usage[];
extern const char uname_usage[];
extern const char uniq_usage[];
extern const char unix2dos_usage[];
extern const char unrpm_usage[];
extern const char update_usage[];
extern const char uptime_usage[];
extern const char usleep_usage[];
extern const char uudecode_usage[];
extern const char uuencode_usage[];
extern const char wc_usage[];
extern const char wget_usage[];
extern const char which_usage[];
extern const char whoami_usage[];
extern const char xargs_usage[];
extern const char yes_usage[];

extern const char *applet_name;

extern void usage(const char *usage) __attribute__ ((noreturn));
extern void errorMsg(const char *s, ...) __attribute__ ((format (printf, 1, 2)));
extern void fatalError(const char *s, ...) __attribute__ ((noreturn, format (printf, 1, 2)));
extern void perrorMsg(const char *s, ...) __attribute__ ((format (printf, 1, 2)));
extern void fatalPerror(const char *s, ...) __attribute__ ((noreturn, format (printf, 1, 2)));

const char *modeString(int mode);
const char *timeString(time_t timeVal);
int isDirectory(const char *name, const int followLinks, struct stat *statBuf);
int isDevice(const char *name);

typedef struct ino_dev_hash_bucket_struct {
  struct ino_dev_hash_bucket_struct *next;
  ino_t ino;
  dev_t dev;
  char name[1];
} ino_dev_hashtable_bucket_t;
int is_in_ino_dev_hashtable(const struct stat *statbuf, char **name);
void add_to_ino_dev_hashtable(const struct stat *statbuf, const char *name);
void reset_ino_dev_hashtable(void);

int copyFile(const char *srcName, const char *destName,
		 int setModes, int followLinks, int forceFlag);
int copySubFile(int srcFd, int dstFd, size_t remaining);
char *buildName(const char *dirName, const char *fileName);
int makeString(int argc, const char **argv, char *buf, int bufLen);
char *getChunk(int size);
char *chunkstrdup(const char *str);
void freeChunks(void);
int fullWrite(int fd, const char *buf, int len);
int fullRead(int fd, char *buf, int len);
int recursiveAction(const char *fileName, int recurse, int followLinks, int depthFirst,
	  int (*fileAction) (const char *fileName, struct stat* statbuf, void* userData),
	  int (*dirAction) (const char *fileName, struct stat* statbuf, void* userData),
	  void* userData);

extern int createPath (const char *name, int mode);
extern int parse_mode( const char* s, mode_t* theMode);

extern int get_kernel_revision(void);

extern int get_console_fd(char* tty_name);
extern struct mntent *findMountPoint(const char *name, const char *table);
extern void write_mtab(char* blockDevice, char* directory, 
	char* filesystemType, long flags, char* string_flags);
extern void erase_mtab(const char * name);
extern void mtab_read(void);
extern char *mtab_first(void **iter);
extern char *mtab_next(void **iter);
extern char *mtab_getinfo(const char *match, const char which);
extern int check_wildcard_match(const char* text, const char* pattern);
extern long getNum (const char *cp);
extern pid_t* findPidByName( char* pidName);
extern int find_real_root_device_name(char* name);
extern char *get_line_from_file(FILE *file);
extern void print_file(FILE *file);
extern int print_file_by_name(char *filename);
extern char process_escape_sequence(char **ptr);
extern char *get_last_path_component(char *path);
extern void xregcomp(regex_t *preg, const char *regex, int cflags);
extern FILE *wfopen(const char *path, const char *mode);
extern FILE *xfopen(const char *path, const char *mode);

#ifndef DMALLOC 
extern void *xmalloc (size_t size);
extern void *xrealloc(void *old, size_t size);
extern void *xcalloc(size_t nmemb, size_t size);
extern char *xstrdup (const char *s);
#endif
extern char *xstrndup (const char *s, int n);


/* These parse entries in /etc/passwd and /etc/group.  This is desirable
 * for BusyBox since we want to avoid using the glibc NSS stuff, which
 * increases target size and is often not needed embedded systems.  */
extern long my_getpwnam(char *name);
extern long my_getgrnam(char *name);
extern void my_getpwuid(char *name, long uid);
extern void my_getgrgid(char *group, long gid);
extern long my_getpwnamegid(char *name);

extern int device_open(char *device, int mode);

#if defined BB_FEATURE_MOUNT_LOOP
extern int del_loop(const char *device);
extern int set_loop(const char *device, const char *file, int offset, int *loopro);
extern char *find_unused_loop_device (void);
#endif


#if (__GLIBC__ < 2) && (defined BB_SYSLOGD || defined BB_INIT)
extern int vdprintf(int d, const char *format, va_list ap);
#endif

#if defined BB_NFSMOUNT
int nfsmount(const char *spec, const char *node, int *flags,
	     char **extra_opts, char **mount_opts, int running_bg);
#endif

#ifndef RB_POWER_OFF
/* Stop system and switch power off if possible.  */
#define RB_POWER_OFF   0x4321fedc
#endif

/* Include our own copy of struct sysinfo to avoid binary compatability
 * problems with Linux 2.4, which changed things.  Grumble, grumble. */
struct sysinfo {
	long uptime;			/* Seconds since boot */
	unsigned long loads[3];		/* 1, 5, and 15 minute load averages */
	unsigned long totalram;		/* Total usable main memory size */
	unsigned long freeram;		/* Available memory size */
	unsigned long sharedram;	/* Amount of shared memory */
	unsigned long bufferram;	/* Memory used by buffers */
	unsigned long totalswap;	/* Total swap space size */
	unsigned long freeswap;		/* swap space still available */
	unsigned short procs;		/* Number of current processes */
	unsigned long totalhigh;	/* Total high memory size */
	unsigned long freehigh;		/* Available high memory size */
	unsigned int mem_unit;		/* Memory unit size in bytes */
	char _f[20-2*sizeof(long)-sizeof(int)];	/* Padding: libc5 uses this.. */
};
extern int sysinfo (struct sysinfo* info);

/* Bit map related macros -- libc5 doens't provide these... sigh.  */
#ifndef setbit
#define NBBY            CHAR_BIT
#define setbit(a,i)     ((a)[(i)/NBBY] |= 1<<((i)%NBBY))
#define clrbit(a,i)     ((a)[(i)/NBBY] &= ~(1<<((i)%NBBY)))
#define isset(a,i)      ((a)[(i)/NBBY] & (1<<((i)%NBBY)))
#define isclr(a,i)      (((a)[(i)/NBBY] & (1<<((i)%NBBY))) == 0)
#endif

#endif /* _BB_INTERNAL_H_ */
