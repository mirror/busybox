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
#ifndef	__LIBCONFIG_H__
#define	__LIBCONFIG_H__    1

#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <netdb.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include <features.h>

#include "config.h"

#include "pwd_.h"
#include "grp_.h"
#ifdef CONFIG_FEATURE_SHADOWPASSWDS
#include "shadow_.h"
#endif
#ifdef CONFIG_FEATURE_SHA1_PASSWORDS
# include "sha1.h"
#endif


#if (__GNU_LIBRARY__ < 5) && (!defined __dietlibc__)
/* libc5 doesn't define socklen_t */
typedef unsigned int socklen_t;
/* libc5 doesn't implement BSD 4.4 daemon() */
extern int daemon (int nochdir, int noclose);
/* libc5 doesn't implement strtok_r */
char *strtok_r(char *s, const char *delim, char **ptrptr);
#endif	

/* Some useful definitions */
#define FALSE   ((int) 0)
#define TRUE    ((int) 1)
#define SKIP	((int) 2)

/* for mtab.c */
#define MTAB_GETMOUNTPT '1'
#define MTAB_GETDEVICE  '2'

#define BUF_SIZE        8192
#define EXPAND_ALLOC    1024

static inline int is_decimal(int ch) { return ((ch >= '0') && (ch <= '9')); }
static inline int is_octal(int ch)   { return ((ch >= '0') && (ch <= '7')); }

/* Macros for min/max.  */
#ifndef MIN
#define	MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
#define	MAX(a,b) (((a)>(b))?(a):(b))
#endif



extern void show_usage(void) __attribute__ ((noreturn));
extern void error_msg(const char *s, ...) __attribute__ ((format (printf, 1, 2)));
extern void error_msg_and_die(const char *s, ...) __attribute__ ((noreturn, format (printf, 1, 2)));
extern void perror_msg(const char *s, ...);
extern void perror_msg_and_die(const char *s, ...) __attribute__ ((noreturn));
extern void vherror_msg(const char *s, va_list p);
extern void herror_msg(const char *s, ...);
extern void herror_msg_and_die(const char *s, ...) __attribute__ ((noreturn));

/* These two are used internally -- you shouldn't need to use them */
extern void verror_msg(const char *s, va_list p);
extern void vperror_msg(const char *s, va_list p);

const char *mode_string(int mode);
const char *time_string(time_t timeVal);
int is_directory(const char *name, int followLinks, struct stat *statBuf);
int isDevice(const char *name);

int remove_file(const char *path, int flags);
int copy_file(const char *source, const char *dest, int flags);
int copy_file_chunk(FILE *src_file, FILE *dst_file, unsigned long long chunksize);
char *buildName(const char *dirName, const char *fileName);
int makeString(int argc, const char **argv, char *buf, int bufLen);
char *getChunk(int size);
char *chunkstrdup(const char *str);
void freeChunks(void);
ssize_t safe_read(int fd, void *buf, size_t count);
int full_write(int fd, const char *buf, int len);
int full_read(int fd, char *buf, int len);
int recursive_action(const char *fileName, int recurse, int followLinks, int depthFirst,
	  int (*fileAction) (const char *fileName, struct stat* statbuf, void* userData),
	  int (*dirAction) (const char *fileName, struct stat* statbuf, void* userData),
	  void* userData);

extern int parse_mode( const char* s, mode_t* theMode);

extern int get_kernel_revision(void);

extern int get_console_fd(char* tty_name);
extern struct mntent *find_mount_point(const char *name, const char *table);
extern void write_mtab(char* blockDevice, char* directory, 
	char* filesystemType, long flags, char* string_flags);
extern void erase_mtab(const char * name);
extern long atoi_w_units (const char *cp);
extern long* find_pid_by_name( char* pidName);
extern char *find_real_root_device_name(const char* name);
extern char *get_line_from_file(FILE *file);
extern void print_file(FILE *file);
extern int copyfd(int fd1, int fd2);
extern int print_file_by_name(char *filename);
extern char process_escape_sequence(const char **ptr);
extern char *get_last_path_component(char *path);
extern FILE *wfopen(const char *path, const char *mode);
extern FILE *xfopen(const char *path, const char *mode);
extern void chomp(char *s);
extern void trim(char *s);
extern struct BB_applet *find_applet_by_name(const char *name);
void run_applet_by_name(const char *name, int argc, char **argv);

#ifndef DMALLOC
extern void *xmalloc (size_t size);
extern void *xrealloc(void *old, size_t size);
extern void *xcalloc(size_t nmemb, size_t size);
extern char *xstrdup (const char *s);
#endif
extern char *xstrndup (const char *s, int n);
extern char * safe_strncpy(char *dst, const char *src, size_t size);

struct suffix_mult {
	const char *suffix;
	int mult;
};

extern unsigned long parse_number(const char *numstr,
		const struct suffix_mult *suffixes);


/* These parse entries in /etc/passwd and /etc/group.  This is desirable
 * for BusyBox since we want to avoid using the glibc NSS stuff, which
 * increases target size and is often not needed embedded systems.  */
extern long my_getpwnam(const char *name);
extern long my_getgrnam(const char *name);
extern void my_getpwuid(char *name, long uid);
extern void my_getgrgid(char *group, long gid);
extern long my_getpwnamegid(const char *name);

extern int device_open(char *device, int mode);

extern int del_loop(const char *device);
extern int set_loop(const char *device, const char *file, int offset, int *loopro);
extern char *find_unused_loop_device (void);


#if (__GLIBC__ < 2)
extern int vdprintf(int d, const char *format, va_list ap);
#endif

int nfsmount(const char *spec, const char *node, int *flags,
	     char **extra_opts, char **mount_opts, int running_bg);

void syslog_msg_with_name(const char *name, int facility, int pri, const char *msg);
void syslog_msg(int facility, int pri, const char *msg);

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
	unsigned short pad;			/* Padding needed for m68k */
	unsigned long totalhigh;	/* Total high memory size */
	unsigned long freehigh;		/* Available high memory size */
	unsigned int mem_unit;		/* Memory unit size in bytes */
	char _f[20-2*sizeof(long)-sizeof(int)];	/* Padding: libc5 uses this.. */
};
extern int sysinfo (struct sysinfo* info);

enum {
	KILOBYTE = 1024,
	MEGABYTE = (KILOBYTE*1024),
	GIGABYTE = (MEGABYTE*1024)
};
const char *make_human_readable_str(unsigned long size, unsigned long block_size, unsigned long display_unit);

int ask_confirmation(void);
int klogctl(int type, char * b, int len);

char *xgetcwd(char *cwd);
char *xreadlink(const char *path);
char *concat_path_file(const char *path, const char *filename);
char *last_char_is(const char *s, int c);

extern long arith (const char *startbuf, int *errcode);

int read_package_field(const char *package_buffer, char **field_name, char **field_value);
char *fgets_str(FILE *file, const char *terminating_string);

extern int inflate(FILE *in, FILE *out);
extern int unzip(FILE *l_in_file, FILE *l_out_file);
extern void gz_close(int gunzip_pid);
extern FILE *gz_open(FILE *compressed_file, int *pid);

extern struct hostent *xgethostbyname(const char *name);
extern struct hostent *xgethostbyname2(const char *name, int af);
extern int create_icmp_socket(void);
extern int create_icmp6_socket(void);
extern int xconnect(const char *host, const char *port);

char *dirname (char *path);

int make_directory (char *path, long mode, int flags);

const char *u_signal_names(const char *str_sig, int *signo, int startnum);
char *simplify_path(const char *path);

#define CT_AUTO	0
#define CT_UNIX2DOS	1
#define CT_DOS2UNIX	2
/* extern int convert(char *fn, int ConvType); */

enum {
	FILEUTILS_PRESERVE_STATUS = 1,
	FILEUTILS_DEREFERENCE = 2,
	FILEUTILS_RECUR = 4,
	FILEUTILS_FORCE = 8,
	FILEUTILS_INTERACTIVE = 16
};

extern const char *applet_name;
extern const char * const full_version;
extern const char * const name_too_long;
extern const char * const omitting_directory;
extern const char * const not_a_directory;
extern const char * const memory_exhausted;
extern const char * const invalid_date;
extern const char * const invalid_option;
extern const char * const io_error;
extern const char * const dash_dash_help;
extern const char * const write_error;
extern const char * const too_few_args;
extern const char * const name_longer_than_foo;
extern const char * const unknown;
extern const char * const can_not_create_raw_socket;
extern const char * const nologin_file;
extern const char * const passwd_file;
extern const char * const shadow_file;
extern const char * const gshadow_file;
extern const char * const group_file;
extern const char * const securetty_file;
extern const char * const motd_file;
extern const char * const issue_file;
extern const char * const _path_login;

#ifdef CONFIG_FEATURE_DEVFS
# define CURRENT_VC "/dev/vc/0"
# define VC_1 "/dev/vc/1"
# define VC_2 "/dev/vc/2"
# define VC_3 "/dev/vc/3"
# define VC_4 "/dev/vc/4"
# define VC_5 "/dev/vc/5"
# define SC_0 "/dev/tts/0"
# define SC_1 "/dev/tts/1"
# define VC_FORMAT "/dev/vc/%d"
# define SC_FORMAT "/dev/tts/%d"
# define LOOP_FORMAT "/dev/loop/%d"
#else
# define CURRENT_VC "/dev/tty0"
# define VC_1 "/dev/tty1"
# define VC_2 "/dev/tty2"
# define VC_3 "/dev/tty3"
# define VC_4 "/dev/tty4"
# define VC_5 "/dev/tty5"
# define SC_0 "/dev/ttyS0"
# define SC_1 "/dev/ttyS1"
# define VC_FORMAT "/dev/tty%d"
# define SC_FORMAT "/dev/ttyS%d"
# define LOOP_FORMAT "/dev/loop%d"
#endif

/* The following devices are the same on devfs and non-devfs systems.  */
#define CURRENT_TTY "/dev/tty"
#define CONSOLE_DEV "/dev/console"

int is_in_ino_dev_hashtable(const struct stat *statbuf, char **name);
void add_to_ino_dev_hashtable(const struct stat *statbuf, const char *name);
void reset_ino_dev_hashtable(void);

/* Stupid gcc always includes its own builtin strlen()... */
extern size_t xstrlen(const char *string);
#define strlen(x)   xstrlen(x)


#define FAIL_DELAY    3
extern void change_identity ( const struct passwd *pw );
extern void run_shell ( const char *shell, int loginshell, const char *command, const char **additional_args );
extern int restricted_shell ( const char *shell );
extern void setup_environment ( const char *shell, int loginshell, int changeenv, const struct passwd *pw );
extern int correct_password ( const struct passwd *pw );
extern char *pw_encrypt(const char *clear, const char *salt);
extern struct spwd *pwd_to_spwd(const struct passwd *pw);
extern int obscure(const char *old, const char *newval, const struct passwd *pwdp);

#endif /* __LIBCONFIG_H__ */
