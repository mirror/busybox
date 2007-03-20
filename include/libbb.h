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
#include <netdb.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
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

#if ENABLE_SELINUX
#include <selinux/selinux.h>
#include <selinux/context.h>
#endif

#if ENABLE_LOCALE_SUPPORT
#include <locale.h>
#else
#define setlocale(x,y) ((void)0)
#endif

#include "pwd_.h"
#include "grp_.h"
/* ifdef it out, because it may include <shadow.h> */
/* and we may not even _have_ <shadow.h>! */
#if ENABLE_FEATURE_SHADOWPASSWDS
#include "shadow_.h"
#endif

/* Try to pull in PATH_MAX */
#include <limits.h>
#include <sys/param.h>
#ifndef PATH_MAX
#define PATH_MAX 256
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
#  define XATOOFF(a) xatoul_range(a, 0, LONG_MAX)
/* usage: sz = BB_STRTOOFF(s, NULL, 10); if (errno || sz < 0) die(); */
#  define BB_STRTOOFF bb_strtoul
#  define STRTOOFF strtoul
/* usage: printf("size: %"OFF_FMT"d (%"OFF_FMT"x)\n", sz, sz); */
#  define OFF_FMT "l"
# else
/* "long" is too short, need "long long" */
#  define XATOOFF(a) xatoull_range(a, 0, LLONG_MAX)
#  define BB_STRTOOFF bb_strtoull
#  define STRTOOFF strtoull
#  define OFF_FMT "ll"
# endif
#else
/* CONFIG_LFS is off */
# if UINT_MAX == 0xffffffff
/* While sizeof(off_t) == sizeof(int), off_t is typedef'ed to long anyway.
 * gcc will throw warnings on printf("%d", off_t). Crap... */
#  define XATOOFF(a) xatoi_u(a)
#  define BB_STRTOOFF bb_strtou
#  define STRTOOFF strtol
#  define OFF_FMT "l"
# else
#  define XATOOFF(a) xatoul_range(a, 0, LONG_MAX)
#  define BB_STRTOOFF bb_strtoul
#  define STRTOOFF strtol
#  define OFF_FMT "l"
# endif
#endif
/* scary. better ideas? (but do *test* them first!) */
#define OFF_T_MAX  ((off_t)~((off_t)1 << (sizeof(off_t)*8-1)))

/* This structure defines protocol families and their handlers. */
struct aftype {
	const char *name;
	const char *title;
	int af;
	int alen;
	char *(*print) (unsigned char *);
	const char *(*sprint) (struct sockaddr *, int numeric);
	int (*input) (/*int type,*/ const char *bufp, struct sockaddr *);
	void (*herror) (char *text);
	int (*rprint) (int options);
	int (*rinput) (int typ, int ext, char **argv);

	/* may modify src */
	int (*getmask) (char *src, struct sockaddr * mask, char *name);
};

/* This structure defines hardware protocols and their handlers. */
struct hwtype {
	const char *name;
	const char *title;
	int type;
	int alen;
	char *(*print) (unsigned char *);
	int (*input) (const char *, struct sockaddr *);
	int (*activate) (int fd);
	int suppress_null_addr;
};

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
#if ENABLE_FEATURE_BUFFERS_GO_ON_STACK
#define RESERVE_CONFIG_BUFFER(buffer,len)           char buffer[len]
#define RESERVE_CONFIG_UBUFFER(buffer,len) unsigned char buffer[len]
#define RELEASE_CONFIG_BUFFER(buffer)      ((void)0)
#else
#if ENABLE_FEATURE_BUFFERS_GO_IN_BSS
#define RESERVE_CONFIG_BUFFER(buffer,len)  static          char buffer[len]
#define RESERVE_CONFIG_UBUFFER(buffer,len) static unsigned char buffer[len]
#define RELEASE_CONFIG_BUFFER(buffer)      ((void)0)
#else
#define RESERVE_CONFIG_BUFFER(buffer,len)           char *buffer=xmalloc(len)
#define RESERVE_CONFIG_UBUFFER(buffer,len) unsigned char *buffer=xmalloc(len)
#define RELEASE_CONFIG_BUFFER(buffer)      free (buffer)
#endif
#endif


#if defined(__GLIBC__) && __GLIBC__ < 2
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
extern char *skip_non_whitespace(const char *);

//TODO: supply a pointer to char[11] buffer (avoid statics)?
extern const char *bb_mode_string(mode_t mode);
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
extern char *find_block_device(const char *path);
/* bb_copyfd_XX print read/write errors and return -1 if they occur */
extern off_t bb_copyfd_eof(int fd1, int fd2);
extern off_t bb_copyfd_size(int fd1, int fd2, off_t size);
extern void bb_copyfd_exact_size(int fd1, int fd2, off_t size);
/* "short" copy can be detected by return value < size */
/* this helper yells "short read!" if param is not -1 */
extern void complain_copyfd_and_die(off_t sz) ATTRIBUTE_NORETURN;
extern char bb_process_escape_sequence(const char **ptr);
/* TODO: sometimes modifies its parameter, which
 * makes it rather inconvenient at times: */
extern char *bb_get_last_path_component(char *path);
extern int ndelay_on(int fd);
extern int ndelay_off(int fd);


extern DIR *xopendir(const char *path);
extern DIR *warn_opendir(const char *path);

char *xrealloc_getcwd_or_warn(char *cwd);
char *xmalloc_readlink_or_warn(const char *path);
char *xmalloc_realpath(const char *path);
extern void xstat(const char *filename, struct stat *buf);
extern pid_t spawn(char **argv);
extern pid_t xspawn(char **argv);
extern int wait4pid(int pid);
extern void xsetgid(gid_t gid);
extern void xsetuid(uid_t uid);
extern void xdaemon(int nochdir, int noclose);
/* More clever/thorough xdaemon */
extern void bb_sanitize_stdio_maybe_daemonize(int daemonize);
extern void bb_sanitize_stdio(void);
/* NB: be careful: dont open syslog/network sockets before bb_daemonize */
extern void bb_daemonize(void);
extern void xchdir(const char *path);
extern void xsetenv(const char *key, const char *value);
extern int xopen(const char *pathname, int flags);
extern int xopen3(const char *pathname, int flags, int mode);
extern off_t xlseek(int fd, off_t offset, int whence);
extern off_t fdlength(int fd);


int xsocket(int domain, int type, int protocol);
void xbind(int sockfd, struct sockaddr *my_addr, socklen_t addrlen);
void xlisten(int s, int backlog);
void xconnect(int s, const struct sockaddr *s_addr, socklen_t addrlen);
int setsockopt_reuseaddr(int fd);
int setsockopt_broadcast(int fd);
/* NB: returns port in host byte order */
unsigned bb_lookup_port(const char *port, const char *protocol, unsigned default_port);
typedef struct len_and_sockaddr {
	socklen_t len;
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
#if ENABLE_FEATURE_IPV6
		struct sockaddr_in6 sin6;
#endif
	};
} len_and_sockaddr;
/* Create stream socket, and allocated suitable lsa
 * (lsa of correct size and lsa->sa.sa_family (AF_INET/AF_INET6)) */
int xsocket_stream(len_and_sockaddr **lsap);
/* Create server TCP socket bound to bindaddr:port. bindaddr can be NULL,
 * numeric IP ("N.N.N.N") or numeric IPv6 address,
 * and can have ":PORT" suffix (for IPv6 use "[X:X:...:X]:PORT").
 * If there is no suffix, port argument is used */
int create_and_bind_stream_or_die(const char *bindaddr, int port);
/* Create client TCP socket connected to peer:port. Peer cannot be NULL.
 * Peer can be numeric IP ("N.N.N.N"), numeric IPv6 address or hostname,
 * and can have ":PORT" suffix (for IPv6 use "[X:X:...:X]:PORT").
 * If there is no suffix, port argument is used */
int create_and_connect_stream_or_die(const char *peer, int port);
/* Connect to peer identified by lsa */
int xconnect_stream(const len_and_sockaddr *lsa);
/* Return malloc'ed len_and_sockaddr with socket address of host:port
 * Currently will return IPv4 or IPv6 sockaddrs only
 * (depending on host), but in theory nothing prevents e.g.
 * UNIX socket address being returned, IPX sockaddr etc...
 * On error does bb_error_msg and returns NULL */
len_and_sockaddr* host2sockaddr(const char *host, int port);
/* Version which dies on error */
len_and_sockaddr* xhost2sockaddr(const char *host, int port);
len_and_sockaddr* xdotted2sockaddr(const char *host, int port);
#if ENABLE_FEATURE_IPV6
/* Same, useful if you want to force family (e.g. IPv6) */
len_and_sockaddr* host_and_af2sockaddr(const char *host, int port, sa_family_t af);
len_and_sockaddr* xhost_and_af2sockaddr(const char *host, int port, sa_family_t af);
#else
/* [we evaluate af: think about "host_and_af2sockaddr(..., af++)"] */
#define host_and_af2sockaddr(host, port, af) ((void)(af), host2sockaddr((host), (port)))
#define xhost_and_af2sockaddr(host, port, af) ((void)(af), xhost2sockaddr((host), (port)))
#endif
/* Assign sin[6]_port member if the socket is of corresponding type,
 * otherwise no-op. Useful for ftp.
 * NB: does NOT do htons() internally, just direct assignment. */
void set_nport(len_and_sockaddr *lsa, unsigned port);
/* Retrieve sin[6]_port or return -1 for non-INET[6] lsa's */
int get_nport(const len_and_sockaddr *lsa);
/* Reverse DNS. Returns NULL on failure. */
char* xmalloc_sockaddr2host(const struct sockaddr *sa, socklen_t salen);
/* This one doesn't append :PORTNUM */
char* xmalloc_sockaddr2host_noport(const struct sockaddr *sa, socklen_t salen);
/* This one also doesn't fall back to dotted IP (returns NULL) */
char* xmalloc_sockaddr2hostonly_noport(const struct sockaddr *sa, socklen_t salen);
/* inet_[ap]ton on steroids */
char* xmalloc_sockaddr2dotted(const struct sockaddr *sa, socklen_t salen);
char* xmalloc_sockaddr2dotted_noport(const struct sockaddr *sa, socklen_t salen);
// "old" (ipv4 only) API
// users: traceroute.c hostname.c
struct hostent *xgethostbyname(const char *name);
// Also inetd.c and inetd.c are using gethostbyname(),
// + inet_common.c has additional IPv4-only stuff


extern char *xstrdup(const char *s);
extern char *xstrndup(const char *s, int n);
extern char *safe_strncpy(char *dst, const char *src, size_t size);
extern char *xasprintf(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
// gcc-4.1.1 still isn't good enough at optimizing it
// (+200 bytes compared to macro)
//static ATTRIBUTE_ALWAYS_INLINE
//int LONE_DASH(const char *s) { return s[0] == '-' && !s[1]; }
//static ATTRIBUTE_ALWAYS_INLINE
//int NOT_LONE_DASH(const char *s) { return s[0] != '-' || s[1]; }
#define LONE_DASH(s) ((s)[0] == '-' && !(s)[1])
#define NOT_LONE_DASH(s) ((s)[0] != '-' || (s)[1])
#define LONE_CHAR(s,c) ((s)[0] == (c) && !(s)[1])
#define NOT_LONE_CHAR(s,c) ((s)[0] != (c) || (s)[1])

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
extern void xwrite(int fd, const void *buf, size_t count);

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


extern void utoa_to_buf(unsigned n, char *buf, unsigned buflen);
extern char *utoa(unsigned n);
extern void itoa_to_buf(int n, char *buf, unsigned buflen);
extern char *itoa(int n);
extern void smart_ulltoa5(unsigned long long ul, char buf[5]);
/* Put a string of hex bytes (ala "1b"), return advanced pointer */
extern char *bin2hex(char *buf, const char *cp, int count);

struct suffix_mult {
	const char *suffix;
	unsigned mult;
};
#include "xatonum.h"
/* Specialized: */
/* Using xatoi() instead of naive atoi() is not always convenient -
 * in many places people want *non-negative* values, but store them
 * in signed int. Therefore we need this one:
 * dies if input is not in [0, INT_MAX] range. Also will reject '-0' etc */
int xatoi_u(const char *numstr);
/* Useful for reading port numbers */
uint16_t xatou16(const char *numstr);


/* These parse entries in /etc/passwd and /etc/group.  This is desirable
 * for BusyBox since we want to avoid using the glibc NSS stuff, which
 * increases target size and is often not needed on embedded systems.  */
long xuname2uid(const char *name);
long xgroup2gid(const char *name);
/* wrapper: allows string to contain numeric uid or gid */
unsigned long get_ug_id(const char *s, long (*xname2id)(const char *));
/* from chpst. Does not die, returns 0 on failure */
struct bb_uidgid_t {
	uid_t uid;
	gid_t gid;
};
/* always sets uid and gid */
int get_uidgid(struct bb_uidgid_t*, const char*, int numeric_ok);
/* chown-like handling of "user[:[group]" */
void parse_chown_usergroup_or_die(struct bb_uidgid_t *u, char *user_group);
/* what is this? */
/*extern char *bb_getug(char *buffer, char *idname, long id, int bufsize, char prefix);*/
char *bb_getpwuid(char *name, long uid, int bufsize);
char *bb_getgrgid(char *group, long gid, int bufsize);
/* versions which cache results (useful for ps, ls etc) */
const char* get_cached_username(uid_t uid);
const char* get_cached_groupname(gid_t gid);
void clear_username_cache(void);
/* internally usernames are saved in fixed-sized char[] buffers */
enum { USERNAME_MAX_SIZE = 16 - sizeof(int) };


enum { BB_GETOPT_ERROR = 0x80000000 };
extern const char *opt_complementary;
#if ENABLE_GETOPT_LONG
extern const struct option *applet_long_options;
#endif
extern uint32_t option_mask32;
extern uint32_t getopt32(int argc, char **argv, const char *applet_opts, ...);


typedef struct llist_s {
	char *data;
	struct llist_s *link;
} llist_t;
extern void llist_add_to(llist_t **old_head, void *data);
extern void llist_add_to_end(llist_t **list_head, void *data);
extern void *llist_pop(llist_t **elm);
extern void llist_unlink(llist_t **head, llist_t *elm);
extern void llist_free(llist_t *elm, void (*freeit)(void *data));
extern llist_t* llist_rev(llist_t *list);

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
extern void sleep_and_die(void) ATTRIBUTE_NORETURN;
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
/* These are used internally -- you shouldn't need to use them */
extern void bb_verror_msg(const char *s, va_list p, const char *strerr);
extern void bb_vperror_msg(const char *s, va_list p);
extern void bb_vinfo_msg(const char *s, va_list p);


/* applets which are useful from another applets */
int bb_cat(char** argv);
int bb_echo(char** argv);
int bb_test(int argc, char** argv);
#if ENABLE_ROUTE
void bb_displayroutes(int noresolve, int netstatfmt);
#endif
int chown_main(int argc, char **argv);
#if ENABLE_GUNZIP
int gunzip_main(int argc, char **argv);
#endif
int bbunpack(char **argv,
	char* (*make_new_name)(char *filename),
	USE_DESKTOP(long long) int (*unpacker)(void)
);


/* Networking */
int create_icmp_socket(void);
int create_icmp6_socket(void);
/* interface.c */
extern int interface_opt_a;
int display_interfaces(char *ifname);
const struct aftype *get_aftype(const char *name);
const struct hwtype *get_hwtype(const char *name);
const struct hwtype *get_hwntype(int type);


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


//TODO: provide pointer to buf (avoid statics)?
const char *make_human_readable_str(unsigned long long size,
		unsigned long block_size, unsigned long display_unit);

//TODO: pass buf pointer or return allocated buf (avoid statics)?
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

#if ENABLE_FEATURE_EXEC_PREFER_APPLETS
int bb_execvp(const char *file, char *const argv[]);
#define BB_EXECVP(prog,cmd) bb_execvp(prog,cmd)
#define BB_EXECLP(prog,cmd,...) \
	execlp((find_applet_by_name(prog)) ? CONFIG_BUSYBOX_EXEC_PATH : prog, \
		cmd, __VA_ARGS__)
#else
#define BB_EXECVP(prog,cmd)     execvp(prog,cmd)
#define BB_EXECLP(prog,cmd,...) execlp(prog,cmd, __VA_ARGS__)
#endif

USE_DESKTOP(long long) int uncompress(int fd_in, int fd_out);
int inflate(int in, int out);


int bb_make_directory(char *path, long mode, int flags);

int get_signum(const char *name);
const char *get_signame(int number);

char *bb_simplify_path(const char *path);

#define FAIL_DELAY 3
extern void bb_do_delay(int seconds);
extern void change_identity(const struct passwd *pw);
extern const char *change_identity_e2str(const struct passwd *pw);
extern void run_shell(const char *shell, int loginshell, const char *command, const char **additional_args);
#if ENABLE_SELINUX
extern void renew_current_security_context(void);
extern void set_current_security_context(security_context_t sid);
extern context_t set_security_context_component(security_context_t cur_context,
						char *user, char *role, char *type, char *range);
extern void setfscreatecon_or_die(security_context_t scontext);
#endif
extern void selinux_or_die(void);
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
extern int get_terminal_width_height(const int fd, int *width, int *height);

char *is_in_ino_dev_hashtable(const struct stat *statbuf);
void add_to_ino_dev_hashtable(const struct stat *statbuf, const char *name);
void reset_ino_dev_hashtable(void);
#ifdef __GLIBC__
/* At least glibc has horrendously large inline for this, so wrap it */
extern unsigned long long bb_makedev(unsigned int major, unsigned int minor);
#undef makedev
#define makedev(a,b) bb_makedev(a,b)
#endif


#if ENABLE_FEATURE_EDITING
/* It's NOT just ENABLEd or disabled. It's a number: */
#ifdef CONFIG_FEATURE_EDITING_HISTORY
#define MAX_HISTORY (CONFIG_FEATURE_EDITING_HISTORY + 0)
#else
#define MAX_HISTORY 0
#endif
struct line_input_t {
	int flags;
	const char *path_lookup;
#if MAX_HISTORY
	int cnt_history;
	int cur_history;
	USE_FEATURE_EDITING_SAVEHISTORY(const char *hist_file;)
	char *history[MAX_HISTORY + 1];
#endif
};
enum {
	DO_HISTORY = 1 * (MAX_HISTORY > 0),
	SAVE_HISTORY = 2 * (MAX_HISTORY > 0) * ENABLE_FEATURE_EDITING_SAVEHISTORY,
	TAB_COMPLETION = 4 * ENABLE_FEATURE_TAB_COMPLETION,
	USERNAME_COMPLETION = 8 * ENABLE_FEATURE_USERNAME_COMPLETION,
	VI_MODE = 0x10 * ENABLE_FEATURE_EDITING_VI,
	WITH_PATH_LOOKUP = 0x20,
	FOR_SHELL = DO_HISTORY | SAVE_HISTORY | TAB_COMPLETION | USERNAME_COMPLETION,
};
typedef struct line_input_t line_input_t;
line_input_t *new_line_input_t(int flags);
int read_line_input(const char* prompt, char* command, int maxsize, line_input_t *state);
#else
int read_line_input(const char* prompt, char* command, int maxsize);
#define read_line_input(prompt, command, maxsize, state) \
	read_line_input(prompt, command, maxsize)
#endif


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
	unsigned long vsz;
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
	PSSCAN_VSZ      = 1 << 8,
	PSSCAN_STIME    = 1 << 9,
	PSSCAN_UTIME    = 1 << 10,
	/* These are all retrieved from proc/NN/stat in one go: */
	PSSCAN_STAT     = PSSCAN_PPID | PSSCAN_PGID | PSSCAN_SID
	                | PSSCAN_COMM | PSSCAN_STATE
	                | PSSCAN_VSZ | PSSCAN_STIME | PSSCAN_UTIME,
};
procps_status_t* alloc_procps_scan(int flags);
void free_procps_scan(procps_status_t* sp);
procps_status_t* procps_scan(procps_status_t* sp, int flags);
pid_t *find_pid_by_name(const char* procName);
pid_t *pidlist_reverse(pid_t *pidList);


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


enum {	/* DO NOT CHANGE THESE VALUES!  cp.c, mv.c, install.c depend on them. */
	FILEUTILS_PRESERVE_STATUS = 1,
	FILEUTILS_DEREFERENCE = 2,
	FILEUTILS_RECUR = 4,
	FILEUTILS_FORCE = 8,
	FILEUTILS_INTERACTIVE = 0x10,
	FILEUTILS_MAKE_HARDLINK = 0x20,
	FILEUTILS_MAKE_SOFTLINK = 0x40,
#if ENABLE_SELINUX
	FILEUTILS_PRESERVE_SECURITY_CONTEXT = 0x80,
	FILEUTILS_SET_SECURITY_CONTEXT = 0x100
#endif
};

#define FILEUTILS_CP_OPTSTR "pdRfils" USE_SELINUX("c")
extern const char *applet_name;
extern const char BB_BANNER[];

extern const char bb_msg_full_version[];
extern const char bb_msg_memory_exhausted[];
extern const char bb_msg_invalid_date[];
extern const char bb_msg_read_error[];
extern const char bb_msg_write_error[];
extern const char bb_msg_unknown[];
extern const char bb_msg_can_not_create_raw_socket[];
extern const char bb_msg_perm_denied_are_you_root[];
extern const char bb_msg_requires_arg[];
extern const char bb_msg_invalid_arg[];
extern const char bb_msg_standard_input[];
extern const char bb_msg_standard_output[];

extern const char bb_str_default[];
/* NB: (bb_hexdigits_upcase[i] | 0x20) -> lowercase hex digit */
extern const char bb_hexdigits_upcase[];

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

extern const int const_int_0;
extern const int const_int_1;

#ifndef BUFSIZ
#define BUFSIZ 4096
#endif
extern char bb_common_bufsiz1[BUFSIZ+1];
/* This struct is deliberately not defined. */
/* See docs/keep_data_small.txt */
struct globals;
/* Magic prevents this from going into rodata */
/* If you want to assign a value, use PTR_TO_GLOBALS = xxx */
extern struct globals *const ptr_to_globals;
#define PTR_TO_GLOBALS (*(struct globals**)&ptr_to_globals)

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


#if ENABLE_FEATURE_DEVFS
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
#define DEV_CONSOLE "/dev/console"


#ifndef RB_POWER_OFF
/* Stop system and switch power off if possible.  */
#define RB_POWER_OFF   0x4321fedc
#endif

/* Make sure we call functions instead of macros.  */
#undef isalnum
#undef isalpha
#undef isascii
#undef isblank
#undef iscntrl
#undef isgraph
#undef islower
#undef isprint
#undef ispunct
#undef isspace
#undef isupper
#undef isxdigit

/* This one is more efficient - we save ~400 bytes */
#undef isdigit
#define isdigit(a) ((unsigned)((a) - '0') <= 9)


#ifdef DMALLOC
#include <dmalloc.h>
#endif

#endif /* __LIBBUSYBOX_H__ */
