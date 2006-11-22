/* vi: set sw=4 ts=4: */
/*
 * Busybox main internal header file
 *
 * Based in part on code from sash, Copyright (c) 1999 by David I. Bell
 * Permission has been granted to redistribute this code under the GPL.
 * 
 * Licensed under the GPL version 2, see the file LICENSE in this tarball.
 */
#ifndef	__LIBBUSYBOX_H__
#define	__LIBBUSYBOX_H__    1

#include "platform.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <malloc.h>
#include <netdb.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#ifdef CONFIG_SELINUX
#include <selinux/selinux.h>
#endif

#ifdef CONFIG_LOCALE_SUPPORT
#include <locale.h>
#else
#define setlocale(x,y)
#endif

#include "pwd_.h"
#include "grp_.h"
#include "shadow_.h"

/* Try to pull in PATH_MAX */
#include <limits.h>
#include <sys/param.h>
#ifndef PATH_MAX
#define  PATH_MAX         256
#endif

/* Tested to work correctly (IIRC :]) */
#define MAXINT(T) (T)( \
	((T)-1) > 0 \
	? (T)-1 \
	: (T)~((T)1 << (sizeof(T)*8-1)) \
	)

#define MININT(T) (T)( \
	((T)-1) > 0 \
	? (T)0 \
	: ((T)1 << (sizeof(T)*8-1)) \
	)

/* Large file support */
/* Note that CONFIG_LFS forces bbox to be built with all common ops
 * (stat, lseek etc) mapped to "largefile" variants by libc.
 * Practically it means that open() automatically has O_LARGEFILE added
 * and all filesize/file_offset parameters and struct members are "large"
 * (in today's world - signed 64bit). For full support of large files,
 * we need a few helper #defines (below) and careful use of off_t
 * instead of int/ssize_t. No lseek64(), O_LARGEFILE etc necessary */
#if ENABLE_LFS
/* CONFIG_LFS is on */
# if ULONG_MAX > 0xffffffff
/* "long" is long enough on this system */
#  define STRTOOFF strtol
#  define SAFE_STRTOOFF safe_strtol
#  define XSTRTOUOFF xstrtoul
#  define OFF_FMT "ld"
# else
/* "long" is too short, need "long long" */
#  define STRTOOFF strtoll
#  define SAFE_STRTOOFF safe_strtoll
#  define XSTRTOUOFF xstrtoull
#  define OFF_FMT "lld"
# endif
#else
# if 0 /* #if UINT_MAX == 0xffffffff */
/* Doesn't work. off_t is a long. gcc will throw warnings on printf("%d", off_t)
 * even if long==int on this arch. Crap... */
#  define STRTOOFF strtol
#  define SAFE_STRTOOFF safe_strtoi
#  define XSTRTOUOFF xstrtou
#  define OFF_FMT "d"
# else
#  define STRTOOFF strtol
#  define SAFE_STRTOOFF safe_strtol
#  define XSTRTOUOFF xstrtoul
#  define OFF_FMT "ld"
# endif
#endif
/* scary. better ideas? (but do *test* them first!) */
#define OFF_T_MAX  ((off_t)~((off_t)1 << (sizeof(off_t)*8-1)))

/* Some useful definitions */
#undef FALSE
#define FALSE   ((int) 0)
#undef TRUE
#define TRUE    ((int) 1)
#undef SKIP
#define SKIP	((int) 2)

/* for mtab.c */
#define MTAB_GETMOUNTPT '1'
#define MTAB_GETDEVICE  '2'

#define BUF_SIZE        8192
#define EXPAND_ALLOC    1024

/* Macros for min/max.  */
#ifndef MIN
#define	MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
#define	MAX(a,b) (((a)>(b))?(a):(b))
#endif

/* buffer allocation schemes */
#ifdef CONFIG_FEATURE_BUFFERS_GO_ON_STACK
#define RESERVE_CONFIG_BUFFER(buffer,len)           char buffer[len]
#define RESERVE_CONFIG_UBUFFER(buffer,len) unsigned char buffer[len]
#define RELEASE_CONFIG_BUFFER(buffer)      ((void)0)
#else
#ifdef CONFIG_FEATURE_BUFFERS_GO_IN_BSS
#define RESERVE_CONFIG_BUFFER(buffer,len)  static          char buffer[len]
#define RESERVE_CONFIG_UBUFFER(buffer,len) static unsigned char buffer[len]
#define RELEASE_CONFIG_BUFFER(buffer)      ((void)0)
#else
#define RESERVE_CONFIG_BUFFER(buffer,len)           char *buffer=xmalloc(len)
#define RESERVE_CONFIG_UBUFFER(buffer,len) unsigned char *buffer=xmalloc(len)
#define RELEASE_CONFIG_BUFFER(buffer)      free (buffer)
#endif
#endif


#if (__GLIBC__ < 2)
int vdprintf(int d, const char *format, va_list ap);
#endif
// This is declared here rather than #including <libgen.h> in order to avoid
// confusing the two versions of basename.  See the dirname/basename man page
// for details.
char *dirname(char *path);
/* Include our own copy of struct sysinfo to avoid binary compatibility
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
extern int sysinfo(struct sysinfo* info);


extern void chomp(char *s);
extern void trim(char *s);
extern char *skip_whitespace(const char *);

extern const char *bb_mode_string(int mode);
extern int is_directory(const char *name, int followLinks, struct stat *statBuf);
extern int remove_file(const char *path, int flags);
extern int copy_file(const char *source, const char *dest, int flags);
extern int recursive_action(const char *fileName, int recurse,
	int followLinks, int depthFirst,
	int (*fileAction) (const char *fileName, struct stat* statbuf, void* userData, int depth),
	int (*dirAction) (const char *fileName, struct stat* statbuf, void* userData, int depth),
	void* userData, int depth);
extern int device_open(const char *device, int mode);
extern int get_console_fd(void);
extern char *find_block_device(char *path);
extern off_t bb_copyfd_size(int fd1, int fd2, off_t size);
extern off_t bb_copyfd_eof(int fd1, int fd2);
extern char bb_process_escape_sequence(const char **ptr);
extern char *bb_get_last_path_component(char *path);
extern int ndelay_on(int fd);


extern DIR *xopendir(const char *path);
extern DIR *warn_opendir(const char *path);

char *xgetcwd(char *cwd);
char *xreadlink(const char *path);
extern void xstat(char *filename, struct stat *buf);
extern pid_t spawn(char **argv);
extern pid_t xspawn(char **argv);
extern int wait4pid(int pid);
extern void xsetgid(gid_t gid);
extern void xsetuid(uid_t uid);
extern void xdaemon(int nochdir, int noclose);
extern void xchdir(const char *path);
extern void xsetenv(const char *key, const char *value);
extern int xopen(const char *pathname, int flags);
extern int xopen3(const char *pathname, int flags, int mode);
extern off_t xlseek(int fd, off_t offset, int whence);
extern off_t fdlength(int fd);


extern int xsocket(int domain, int type, int protocol);
extern void xbind(int sockfd, struct sockaddr *my_addr, socklen_t addrlen);
extern void xlisten(int s, int backlog);
extern void xconnect(int s, const struct sockaddr *s_addr, socklen_t addrlen);
extern int xconnect_tcp_v4(struct sockaddr_in *s_addr);
extern struct hostent *xgethostbyname(const char *name);
extern struct hostent *xgethostbyname2(const char *name, int af);
extern int xsocket_stream_ip4or6(sa_family_t *fp);
typedef union {
	struct sockaddr sa;
	struct sockaddr_in sin;
#if ENABLE_FEATURE_IPV6
	struct sockaddr_in6 sin6;
#endif
} sockaddr_inet;
extern int dotted2sockaddr(const char *dotted, struct sockaddr* sp, int socklen);
extern int create_and_bind_socket_ip4or6(const char *hostaddr, int port);


extern char *xstrdup(const char *s);
extern char *xstrndup(const char *s, int n);
extern char *safe_strncpy(char *dst, const char *src, size_t size);
extern char *xasprintf(const char *format, ...) __attribute__ ((format (printf, 1, 2)));

/* dmalloc will redefine these to it's own implementation. It is safe
 * to have the prototypes here unconditionally.  */
extern void *xmalloc(size_t size);
extern void *xrealloc(void *old, size_t size);
extern void *xzalloc(size_t size);

extern ssize_t safe_read(int fd, void *buf, size_t count);
extern ssize_t full_read(int fd, void *buf, size_t count);
extern void xread(int fd, void *buf, size_t count);
extern unsigned char xread_char(int fd);
extern char *reads(int fd, char *buf, size_t count);
extern ssize_t read_close(int fd, void *buf, size_t count);
extern ssize_t open_read_close(const char *filename, void *buf, size_t count);
extern void *xmalloc_open_read_close(const char *filename, size_t *sizep);

extern ssize_t safe_write(int fd, const void *buf, size_t count);
extern ssize_t full_write(int fd, const void *buf, size_t count);
extern void xwrite(int fd, void *buf, size_t count);

/* Reads and prints to stdout till eof, then closes FILE. Exits on error: */
extern void xprint_and_close_file(FILE *file);
extern char *xmalloc_fgets(FILE *file);
/* Read up to (and including) TERMINATING_STRING: */
extern char *xmalloc_fgets_str(FILE *file, const char *terminating_string);
/* Chops off '\n' from the end, unlike fgets: */
extern char *xmalloc_getline(FILE *file);
extern char *bb_get_chunk_from_file(FILE *file, int *end);
extern void die_if_ferror(FILE *file, const char *msg);
extern void die_if_ferror_stdout(void);
extern void xfflush_stdout(void);
extern void fflush_stdout_and_exit(int retval) ATTRIBUTE_NORETURN;
extern int fclose_if_not_stdin(FILE *file);
extern FILE *xfopen(const char *filename, const char *mode);
/* Prints warning to stderr and returns NULL on failure: */
extern FILE *fopen_or_warn(const char *filename, const char *mode);
/* "Opens" stdin if filename is special, else just opens file: */
extern FILE *fopen_or_warn_stdin(const char *filename);


extern void smart_ulltoa5(unsigned long long ul, char buf[5]);
extern void utoa_to_buf(unsigned n, char *buf, unsigned buflen);
extern char *utoa(unsigned n);
extern void itoa_to_buf(int n, char *buf, unsigned buflen);
extern char *itoa(int n);

// FIXME: the prototype doesn't match libc strtoXX -> confusion
// FIXME: alot of unchecked strtoXXX are still in tree
// FIXME: atoi_or_else(str, N)?
extern int safe_strtoi(const char *arg, int* value);
extern int safe_strtou(const char *arg, unsigned* value);
extern int safe_strtod(const char *arg, double* value);
extern int safe_strtol(const char *arg, long* value);
extern int safe_strtoll(const char *arg, long long* value);
extern int safe_strtoul(const char *arg, unsigned long* value);
extern int safe_strtoull(const char *arg, unsigned long long* value);
extern int safe_strtou32(const char *arg, uint32_t* value);

struct suffix_mult {
	const char *suffix;
	unsigned mult;
};
unsigned long long xstrtoull(const char *numstr, int base);
unsigned long long xatoull(const char *numstr);
unsigned long xstrtoul_range_sfx(const char *numstr, int base,
		unsigned long lower,
		unsigned long upper,
		const struct suffix_mult *suffixes);
unsigned long xstrtoul_range(const char *numstr, int base,
		unsigned long lower,
		unsigned long upper);
unsigned long xstrtoul_sfx(const char *numstr, int base,
		const struct suffix_mult *suffixes);
unsigned long xstrtoul(const char *numstr, int base);
unsigned long xatoul_range_sfx(const char *numstr,
		unsigned long lower,
		unsigned long upper,
		const struct suffix_mult *suffixes);
unsigned long xatoul_sfx(const char *numstr,
		const struct suffix_mult *suffixes);
unsigned long xatoul_range(const char *numstr,
		unsigned long lower,
		unsigned long upper);
unsigned long xatoul(const char *numstr);
long xstrtol_range_sfx(const char *numstr, int base,
		long lower,
		long upper,
		const struct suffix_mult *suffixes);
long xstrtol_range(const char *numstr, int base, long lower, long upper);
long xatol_range_sfx(const char *numstr,
		long lower,
		long upper,
		const struct suffix_mult *suffixes);
long xatol_range(const char *numstr, long lower, long upper);
long xatol_sfx(const char *numstr, const struct suffix_mult *suffixes);
long xatol(const char *numstr);
/* Specialized: */
unsigned xatou_range(const char *numstr, unsigned lower, unsigned upper);
unsigned xatou_sfx(const char *numstr, const struct suffix_mult *suffixes);
unsigned xatou(const char *numstr);
int xatoi_range(const char *numstr, int lower, int upper);
int xatoi(const char *numstr);
/* Using xatoi() instead of naive atoi() is not always convenient -
 * in many places people want *non-negative* values, but store them
 * in signed int. Therefore we need this one:
 * dies if input is not in [0, INT_MAX] range. Also will reject '-0' etc */
int xatoi_u(const char *numstr);
uint32_t xatou32(const char *numstr);
/* Useful for reading port numbers */
uint16_t xatou16(const char *numstr);


/* These parse entries in /etc/passwd and /etc/group.  This is desirable
 * for BusyBox since we want to avoid using the glibc NSS stuff, which
 * increases target size and is often not needed on embedded systems.  */
extern long bb_xgetpwnam(const char *name);
extern long bb_xgetgrnam(const char *name);
extern char *bb_getug(char *buffer, char *idname, long id, int bufsize, char prefix);
extern char *bb_getpwuid(char *name, long uid, int bufsize);
extern char *bb_getgrgid(char *group, long gid, int bufsize);
/* from chpst */
struct bb_uidgid_t {
        uid_t uid;
        gid_t gid;
};
extern unsigned uidgid_get(struct bb_uidgid_t*, const char* /*, unsigned*/);


enum { BB_GETOPT_ERROR = 0x80000000 };
extern const char *opt_complementary;
extern const struct option *applet_long_options;
extern uint32_t option_mask32;
extern uint32_t getopt32(int argc, char **argv, const char *applet_opts, ...);


typedef struct llist_s {
	char *data;
	struct llist_s *link;
} llist_t;
extern void llist_add_to(llist_t **old_head, void *data);
extern void llist_add_to_end(llist_t **list_head, void *data);
extern void *llist_pop(llist_t **elm);
extern void llist_free(llist_t *elm, void (*freeit)(void *data));
extern llist_t* rev_llist(llist_t *list);

enum {
	LOGMODE_NONE = 0,
	LOGMODE_STDIO = 1<<0,
	LOGMODE_SYSLOG = 1<<1,
	LOGMODE_BOTH = LOGMODE_SYSLOG + LOGMODE_STDIO,
};
extern const char *msg_eol;
extern int logmode;
extern int die_sleep;
extern int xfunc_error_retval;
extern void bb_show_usage(void) ATTRIBUTE_NORETURN ATTRIBUTE_EXTERNALLY_VISIBLE;
extern void bb_error_msg(const char *s, ...) __attribute__ ((format (printf, 1, 2)));
extern void bb_error_msg_and_die(const char *s, ...) __attribute__ ((noreturn, format (printf, 1, 2)));
extern void bb_perror_msg(const char *s, ...) __attribute__ ((format (printf, 1, 2)));
extern void bb_perror_msg_and_die(const char *s, ...) __attribute__ ((noreturn, format (printf, 1, 2)));
extern void bb_vherror_msg(const char *s, va_list p);
extern void bb_herror_msg(const char *s, ...) __attribute__ ((format (printf, 1, 2)));
extern void bb_herror_msg_and_die(const char *s, ...) __attribute__ ((noreturn, format (printf, 1, 2)));
extern void bb_perror_nomsg_and_die(void) ATTRIBUTE_NORETURN;
extern void bb_perror_nomsg(void);
extern void bb_info_msg(const char *s, ...) __attribute__ ((format (printf, 1, 2)));
/* These two are used internally -- you shouldn't need to use them */
extern void bb_verror_msg(const char *s, va_list p, const char *strerr);
extern void bb_vperror_msg(const char *s, va_list p);
extern void bb_vinfo_msg(const char *s, va_list p);


extern int bb_echo(int argc, char** argv);
extern int bb_test(int argc, char** argv);

#ifndef BUILD_INDIVIDUAL
extern struct BB_applet *find_applet_by_name(const char *name);
extern void run_applet_by_name(const char *name, int argc, char **argv);
#endif

extern struct mntent *find_mount_point(const char *name, const char *table);
extern void erase_mtab(const char * name);
extern unsigned int tty_baud_to_value(speed_t speed);
extern speed_t tty_value_to_baud(unsigned int value);
extern void bb_warn_ignoring_args(int n);

extern int get_linux_version_code(void);

extern char *query_loop(const char *device);
extern int del_loop(const char *device);
extern int set_loop(char **device, const char *file, unsigned long long offset);


const char *make_human_readable_str(unsigned long long size,
		unsigned long block_size, unsigned long display_unit);

char *bb_askpass(int timeout, const char * prompt);
int bb_ask_confirmation(void);
int klogctl(int type, char * b, int len);

extern int bb_parse_mode(const char* s, mode_t* theMode);

char *concat_path_file(const char *path, const char *filename);
char *concat_subpath_file(const char *path, const char *filename);
char *last_char_is(const char *s, int c);

int execable_file(const char *name);
char *find_execable(const char *filename);
int exists_execable(const char *filename);

USE_DESKTOP(long long) int uncompress(int fd_in, int fd_out);
int inflate(int in, int out);

int create_icmp_socket(void);
int create_icmp6_socket(void);

unsigned short bb_lookup_port(const char *port, const char *protocol, unsigned short default_port);
void bb_lookup_host(struct sockaddr_in *s_in, const char *host);

int bb_make_directory(char *path, long mode, int flags);

int get_signum(const char *name);
const char *get_signame(int number);

char *bb_simplify_path(const char *path);

#define FAIL_DELAY 3
extern void bb_do_delay(int seconds);
extern void change_identity(const struct passwd *pw);
extern const char *change_identity_e2str(const struct passwd *pw);
extern void run_shell(const char *shell, int loginshell, const char *command, const char **additional_args);
#ifdef CONFIG_SELINUX
extern void renew_current_security_context(void);
extern void set_current_security_context(security_context_t sid);
#endif
extern int run_parts(char **args, const unsigned char test_mode, char **env);
extern int restricted_shell(const char *shell);
extern void setup_environment(const char *shell, int loginshell, int changeenv, const struct passwd *pw);
extern int correct_password(const struct passwd *pw);
extern char *pw_encrypt(const char *clear, const char *salt);
extern int obscure(const char *old, const char *newval, const struct passwd *pwdp);
extern int index_in_str_array(const char * const string_array[], const char *key);
extern int index_in_substr_array(const char * const string_array[], const char *key);
extern void print_login_issue(const char *issue_file, const char *tty);
extern void print_login_prompt(void);
#ifdef BB_NOMMU
extern void vfork_daemon(int nochdir, int noclose);
extern void vfork_daemon_rexec(int nochdir, int noclose,
		int argc, char **argv, char *foreground_opt);
#endif
extern int get_terminal_width_height(int fd, int *width, int *height);
extern unsigned long get_ug_id(const char *s, long (*__bb_getxxnam)(const char *));

int is_in_ino_dev_hashtable(const struct stat *statbuf, char **name);
void add_to_ino_dev_hashtable(const struct stat *statbuf, const char *name);
void reset_ino_dev_hashtable(void);


#ifndef COMM_LEN
#ifdef TASK_COMM_LEN
enum { COMM_LEN = TASK_COMM_LEN };
#else
/* synchronize with sizeof(task_struct.comm) in /usr/include/linux/sched.h */
enum { COMM_LEN = 16 };
#endif
#endif
typedef struct {
	DIR *dir;
/* Fields are set to 0/NULL if failed to determine (or not requested) */
	char *cmd;
	unsigned long rss;
	unsigned long stime, utime;
	unsigned pid;
	unsigned ppid;
	unsigned pgid;
	unsigned sid;
	unsigned uid;
	unsigned gid;
	/* basename of executable file in call to exec(2), size from */
	/* sizeof(task_struct.comm) in /usr/include/linux/sched.h */
	char state[4];
	char comm[COMM_LEN];
//	user/group? - use passwd/group parsing functions
} procps_status_t;
enum {
	PSSCAN_PID      = 1 << 0,
	PSSCAN_PPID     = 1 << 1,
	PSSCAN_PGID     = 1 << 2,
	PSSCAN_SID      = 1 << 3,
	PSSCAN_UIDGID   = 1 << 4,
	PSSCAN_COMM     = 1 << 5,
	PSSCAN_CMD      = 1 << 6,
	PSSCAN_STATE    = 1 << 7,
	PSSCAN_RSS      = 1 << 8,
	PSSCAN_STIME    = 1 << 9,
	PSSCAN_UTIME    = 1 << 10,
	/* These are all retrieved from proc/NN/stat in one go: */
	PSSCAN_STAT     = PSSCAN_PPID | PSSCAN_PGID | PSSCAN_SID
	                | PSSCAN_COMM | PSSCAN_STATE
	                | PSSCAN_RSS | PSSCAN_STIME | PSSCAN_UTIME,
};
procps_status_t* alloc_procps_scan(int flags);
void free_procps_scan(procps_status_t* sp);
procps_status_t* procps_scan(procps_status_t* sp, int flags);
pid_t *find_pid_by_name(const char* procName);
pid_t *pidlist_reverse(pid_t *pidList);
void clear_username_cache(void);
const char* get_cached_username(uid_t uid);


extern const char bb_uuenc_tbl_base64[];
extern const char bb_uuenc_tbl_std[];
void bb_uuencode(const unsigned char *s, char *store, const int length, const char *tbl);

typedef struct sha1_ctx_t {
	uint32_t count[2];
	uint32_t hash[5];
	uint32_t wbuf[16];
} sha1_ctx_t;
void sha1_begin(sha1_ctx_t *ctx);
void sha1_hash(const void *data, size_t length, sha1_ctx_t *ctx);
void *sha1_end(void *resbuf, sha1_ctx_t *ctx);

typedef struct md5_ctx_t {
	uint32_t A;
	uint32_t B;
	uint32_t C;
	uint32_t D;
	uint64_t total;
	uint32_t buflen;
	char buffer[128];
} md5_ctx_t;
void md5_begin(md5_ctx_t *ctx);
void md5_hash(const void *data, size_t length, md5_ctx_t *ctx);
void *md5_end(void *resbuf, md5_ctx_t *ctx);

uint32_t *crc32_filltable(int endian);


enum {	/* DO NOT CHANGE THESE VALUES!  cp.c depends on them. */
	FILEUTILS_PRESERVE_STATUS = 1,
	FILEUTILS_DEREFERENCE = 2,
	FILEUTILS_RECUR = 4,
	FILEUTILS_FORCE = 8,
	FILEUTILS_INTERACTIVE = 0x10,
	FILEUTILS_MAKE_HARDLINK = 0x20,
	FILEUTILS_MAKE_SOFTLINK = 0x40,
};
#define FILEUTILS_CP_OPTSTR "pdRfils"

extern const char *applet_name;
extern const char BB_BANNER[];

extern const char bb_msg_full_version[];
extern const char bb_msg_memory_exhausted[];
extern const char bb_msg_invalid_date[];
extern const char bb_msg_read_error[];
extern const char bb_msg_write_error[];
extern const char bb_msg_name_longer_than_foo[];
extern const char bb_msg_unknown[];
extern const char bb_msg_can_not_create_raw_socket[];
extern const char bb_msg_perm_denied_are_you_root[];
extern const char bb_msg_requires_arg[];
extern const char bb_msg_invalid_arg[];
extern const char bb_msg_standard_input[];
extern const char bb_msg_standard_output[];

extern const char bb_str_default[];

extern const char bb_path_mtab_file[];
extern const char bb_path_nologin_file[];
extern const char bb_path_passwd_file[];
extern const char bb_path_shadow_file[];
extern const char bb_path_gshadow_file[];
extern const char bb_path_group_file[];
extern const char bb_path_securetty_file[];
extern const char bb_path_motd_file[];
extern const char bb_path_wtmp_file[];
extern const char bb_dev_null[];

#ifndef BUFSIZ
#define BUFSIZ 4096
#endif
extern char bb_common_bufsiz1[BUFSIZ+1];

/* You can change LIBBB_DEFAULT_LOGIN_SHELL, but don't use it,
 * use bb_default_login_shell and following defines.
 * If you change LIBBB_DEFAULT_LOGIN_SHELL,
 * don't forget to change increment constant. */
#define LIBBB_DEFAULT_LOGIN_SHELL      "-/bin/sh"
extern const char bb_default_login_shell[];
/* "/bin/sh" */
#define DEFAULT_SHELL     (bb_default_login_shell+1)
/* "sh" */
#define DEFAULT_SHELL_SHORT_NAME     (bb_default_login_shell+6)


#ifdef CONFIG_FEATURE_DEVFS
# define CURRENT_VC "/dev/vc/0"
# define VC_1 "/dev/vc/1"
# define VC_2 "/dev/vc/2"
# define VC_3 "/dev/vc/3"
# define VC_4 "/dev/vc/4"
# define VC_5 "/dev/vc/5"
#if defined(__sh__) || defined(__H8300H__) || defined(__H8300S__)
/* Yes, this sucks, but both SH (including sh64) and H8 have a SCI(F) for their
   respective serial ports .. as such, we can't use the common device paths for
   these. -- PFM */
#  define SC_0 "/dev/ttsc/0"
#  define SC_1 "/dev/ttsc/1"
#  define SC_FORMAT "/dev/ttsc/%d"
#else
#  define SC_0 "/dev/tts/0"
#  define SC_1 "/dev/tts/1"
#  define SC_FORMAT "/dev/tts/%d"
#endif
# define VC_FORMAT "/dev/vc/%d"
# define LOOP_FORMAT "/dev/loop/%d"
# define LOOP_NAME "/dev/loop/"
# define FB_0 "/dev/fb/0"
#else
# define CURRENT_VC "/dev/tty0"
# define VC_1 "/dev/tty1"
# define VC_2 "/dev/tty2"
# define VC_3 "/dev/tty3"
# define VC_4 "/dev/tty4"
# define VC_5 "/dev/tty5"
#if defined(__sh__) || defined(__H8300H__) || defined(__H8300S__)
#  define SC_0 "/dev/ttySC0"
#  define SC_1 "/dev/ttySC1"
#  define SC_FORMAT "/dev/ttySC%d"
#else
#  define SC_0 "/dev/ttyS0"
#  define SC_1 "/dev/ttyS1"
#  define SC_FORMAT "/dev/ttyS%d"
#endif
# define VC_FORMAT "/dev/tty%d"
# define LOOP_FORMAT "/dev/loop%d"
# define LOOP_NAME "/dev/loop"
# define FB_0 "/dev/fb0"
#endif

/* The following devices are the same on devfs and non-devfs systems.  */
#define CURRENT_TTY "/dev/tty"
#define CONSOLE_DEV "/dev/console"


#ifndef RB_POWER_OFF
/* Stop system and switch power off if possible.  */
#define RB_POWER_OFF   0x4321fedc
#endif

// Make sure we call functions instead of macros.
#undef isalnum
#undef isalpha
#undef isascii
#undef isblank
#undef iscntrl
#undef isdigit
#undef isgraph
#undef islower
#undef isprint
#undef ispunct
#undef isspace
#undef isupper
#undef isxdigit

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#endif /* __LIBBUSYBOX_H__ */
