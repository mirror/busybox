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
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
/* Try to pull in PATH_MAX */
#include <limits.h>
#include <sys/param.h>
#ifndef PATH_MAX
#define PATH_MAX 256
#endif

#ifdef HAVE_MNTENT_H
#include <mntent.h>
#endif

#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif

#if ENABLE_SELINUX
#include <selinux/selinux.h>
#include <selinux/context.h>
#include <selinux/flask.h>
#include <selinux/av_permissions.h>
#endif

#if ENABLE_LOCALE_SUPPORT
#include <locale.h>
#else
#define setlocale(x,y) ((void)0)
#endif

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#if !ENABLE_USE_BB_PWD_GRP
# include <pwd.h>
# include <grp.h>
#endif
#if ENABLE_FEATURE_SHADOWPASSWDS
# if !ENABLE_USE_BB_SHADOW
#  include <shadow.h>
# endif
#endif

/* Some libc's forget to declare these, do it ourself */
extern char **environ;

/* Set the group set for the current user to GROUPS (N of them).  */
int setgroups(size_t n, const gid_t *groups);
#if defined(__GLIBC__) && __GLIBC__ < 2
int vdprintf(int d, const char *format, va_list ap);
#endif
/* klogctl is in libc's klog.h, but we cheat and not #include that */
int klogctl(int type, char *b, int len);
/* This is declared here rather than #including <libgen.h> in order to avoid
 * confusing the two versions of basename.  See the dirname/basename man page
 * for details. */
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
	char _f[20 - 2*sizeof(long) - sizeof(int)]; /* Padding: libc5 uses this.. */
};
int sysinfo(struct sysinfo* info);


/* Make all declarations hidden (-fvisibility flag only affects definitions) */
/* (don't include system headers after this until corresponding pop!) */
#if __GNUC_PREREQ(4,1)
# pragma GCC visibility push(hidden)
#endif


#if ENABLE_USE_BB_PWD_GRP
# include "pwd_.h"
# include "grp_.h"
#endif
#if ENABLE_FEATURE_SHADOWPASSWDS
# if ENABLE_USE_BB_SHADOW
#  include "shadow_.h"
# endif
#endif

/* Tested to work correctly with all int types (IIRC :]) */
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
/* Note that CONFIG_LFS=y forces bbox to be built with all common ops
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
#define RESERVE_CONFIG_BUFFER(buffer,len)  char buffer[len]
#define RESERVE_CONFIG_UBUFFER(buffer,len) unsigned char buffer[len]
#define RELEASE_CONFIG_BUFFER(buffer)      ((void)0)
#else
#if ENABLE_FEATURE_BUFFERS_GO_IN_BSS
#define RESERVE_CONFIG_BUFFER(buffer,len)  static          char buffer[len]
#define RESERVE_CONFIG_UBUFFER(buffer,len) static unsigned char buffer[len]
#define RELEASE_CONFIG_BUFFER(buffer)      ((void)0)
#else
#define RESERVE_CONFIG_BUFFER(buffer,len)  char *buffer = xmalloc(len)
#define RESERVE_CONFIG_UBUFFER(buffer,len) unsigned char *buffer = xmalloc(len)
#define RELEASE_CONFIG_BUFFER(buffer)      free(buffer)
#endif
#endif

#if defined(__GLIBC__)
/* glibc uses __errno_location() to get a ptr to errno */
/* We can just memorize it once - no multithreading in busybox :) */
extern int *const bb_errno;
#undef errno
#define errno (*bb_errno)
#endif

unsigned long long monotonic_us(void);
unsigned monotonic_sec(void);

extern void chomp(char *s);
extern void trim(char *s);
extern char *skip_whitespace(const char *);
extern char *skip_non_whitespace(const char *);
extern char *strrstr(const char *haystack, const char *needle);

//TODO: supply a pointer to char[11] buffer (avoid statics)?
extern const char *bb_mode_string(mode_t mode);
extern int is_directory(const char *name, int followLinks, struct stat *statBuf);
enum {	/* DO NOT CHANGE THESE VALUES!  cp.c, mv.c, install.c depend on them. */
	FILEUTILS_PRESERVE_STATUS = 1,
	FILEUTILS_DEREFERENCE = 2,
	FILEUTILS_RECUR = 4,
	FILEUTILS_FORCE = 8,
	FILEUTILS_INTERACTIVE = 0x10,
	FILEUTILS_MAKE_HARDLINK = 0x20,
	FILEUTILS_MAKE_SOFTLINK = 0x40,
	FILEUTILS_DEREF_SOFTLINK = 0x80,
#if ENABLE_SELINUX
	FILEUTILS_PRESERVE_SECURITY_CONTEXT = 0x100,
	FILEUTILS_SET_SECURITY_CONTEXT = 0x200
#endif
};
#define FILEUTILS_CP_OPTSTR "pdRfilsL" USE_SELINUX("c")
extern int remove_file(const char *path, int flags);
/* NB: without FILEUTILS_RECUR in flags, it will basically "cat"
 * the source, not copy (unless "source" is a directory).
 * This makes "cp /dev/null file" and "install /dev/null file" (!!!)
 * work coreutils-compatibly. */
extern int copy_file(const char *source, const char *dest, int flags);

enum {
	ACTION_RECURSE        = (1 << 0),
	ACTION_FOLLOWLINKS    = (1 << 1),
	ACTION_FOLLOWLINKS_L0 = (1 << 2),
	ACTION_DEPTHFIRST     = (1 << 3),
	/*ACTION_REVERSE      = (1 << 4), - unused */
};
extern int recursive_action(const char *fileName, unsigned flags,
	int (*fileAction) (const char *fileName, struct stat* statbuf, void* userData, int depth),
	int (*dirAction) (const char *fileName, struct stat* statbuf, void* userData, int depth),
	void* userData, unsigned depth);
extern int device_open(const char *device, int mode);
enum { GETPTY_BUFSIZE = 16 }; /* more than enough for "/dev/ttyXXX" */
extern int xgetpty(char *line);
extern int get_console_fd(void);
extern void console_make_active(int fd, const int vt_num);
extern char *find_block_device(const char *path);
/* bb_copyfd_XX print read/write errors and return -1 if they occur */
extern off_t bb_copyfd_eof(int fd1, int fd2);
extern off_t bb_copyfd_size(int fd1, int fd2, off_t size);
extern void bb_copyfd_exact_size(int fd1, int fd2, off_t size);
/* "short" copy can be detected by return value < size */
/* this helper yells "short read!" if param is not -1 */
extern void complain_copyfd_and_die(off_t sz) ATTRIBUTE_NORETURN;
extern char bb_process_escape_sequence(const char **ptr);
/* xxxx_strip version can modify its parameter:
 * "/"        -> "/"
 * "abc"      -> "abc"
 * "abc/def"  -> "def"
 * "abc/def/" -> "def" !!
 */
extern char *bb_get_last_path_component_strip(char *path);
/* "abc/def/" -> "" and it never modifies 'path' */
extern char *bb_get_last_path_component_nostrip(const char *path);

int ndelay_on(int fd);
int ndelay_off(int fd);
int close_on_exec_on(int fd);
void xdup2(int, int);
void xmove_fd(int, int);


DIR *xopendir(const char *path);
DIR *warn_opendir(const char *path);

/* UNUSED: char *xmalloc_realpath(const char *path); */
char *xmalloc_readlink(const char *path);
char *xmalloc_readlink_or_warn(const char *path);
char *xrealloc_getcwd_or_warn(char *cwd);

char *xmalloc_follow_symlinks(const char *path);


enum {
	/* bb_signals(BB_FATAL_SIGS, handler) catches all signals which
	 * otherwise would kill us, except for those resulting from bugs:
	 * SIGSEGV, SIGILL, SIGFPE.
	 * Other fatal signals not included (TODO?):
	 * SIGBUS   Bus error (bad memory access)
	 * SIGPOLL  Pollable event. Synonym of SIGIO
	 * SIGPROF  Profiling timer expired
	 * SIGSYS   Bad argument to routine
	 * SIGTRAP  Trace/breakpoint trap
	 *
	 * The only known arch with some of these sigs not fitting
	 * into 32 bits is parisc (SIGXCPU=33, SIGXFSZ=34, SIGSTKFLT=36).
	 * Dance around with long long to guard against that...
	 */
	BB_FATAL_SIGS = (int)(0
		+ (1LL << SIGHUP)
		+ (1LL << SIGINT)
		+ (1LL << SIGTERM)
		+ (1LL << SIGPIPE)   // Write to pipe with no readers
		+ (1LL << SIGQUIT)   // Quit from keyboard
		+ (1LL << SIGABRT)   // Abort signal from abort(3)
		+ (1LL << SIGALRM)   // Timer signal from alarm(2)
		+ (1LL << SIGVTALRM) // Virtual alarm clock
		+ (1LL << SIGXCPU)   // CPU time limit exceeded
		+ (1LL << SIGXFSZ)   // File size limit exceeded
		+ (1LL << SIGUSR1)   // Yes kids, these are also fatal!
		+ (1LL << SIGUSR2)
		+ 0),
};
void bb_signals(int sigs, void (*f)(int));
/* Unlike signal() and bb_signals, sets handler with sigaction()
 * and in a way that while signal handler is run, no other signals
 * will be blocked: */
void bb_signals_recursive(int sigs, void (*f)(int));
/* syscalls like read() will be interrupted with EINTR: */
void signal_no_SA_RESTART_empty_mask(int sig, void (*handler)(int));
/* syscalls like read() won't be interrupted (though select/poll will be): */
void signal_SA_RESTART_empty_mask(int sig, void (*handler)(int));
void wait_for_any_sig(void);
void kill_myself_with_sig(int sig) ATTRIBUTE_NORETURN;
void sig_block(int sig);
void sig_unblock(int sig);
/* Will do sigaction(signum, act, NULL): */
int sigaction_set(int sig, const struct sigaction *act);
/* SIG_BLOCK/SIG_UNBLOCK all signals: */
int sigprocmask_allsigs(int how);


void xsetgid(gid_t gid);
void xsetuid(uid_t uid);
void xchdir(const char *path);
void xchroot(const char *path);
void xsetenv(const char *key, const char *value);
void xunlink(const char *pathname);
void xstat(const char *pathname, struct stat *buf);
int xopen(const char *pathname, int flags);
int xopen3(const char *pathname, int flags, int mode);
int open_or_warn(const char *pathname, int flags);
int open3_or_warn(const char *pathname, int flags, int mode);
int open_or_warn_stdin(const char *pathname);
void xrename(const char *oldpath, const char *newpath);
int rename_or_warn(const char *oldpath, const char *newpath);
off_t xlseek(int fd, off_t offset, int whence);
off_t fdlength(int fd);

void xpipe(int filedes[2]);
/* In this form code with pipes is much more readable */
struct fd_pair { int rd; int wr; };
#define piped_pair(pair)  pipe(&((pair).rd))
#define xpiped_pair(pair) xpipe(&((pair).rd))

/* Useful for having small structure members/global variables */
typedef int8_t socktype_t;
typedef int8_t family_t;
struct BUG_too_small {
	char BUG_socktype_t_too_small[(0
			| SOCK_STREAM
			| SOCK_DGRAM
			| SOCK_RDM
			| SOCK_SEQPACKET
			| SOCK_RAW
			) <= 127 ? 1 : -1];
	char BUG_family_t_too_small[(0
			| AF_UNSPEC
			| AF_INET
			| AF_INET6
			| AF_UNIX
#ifdef AF_PACKET
			| AF_PACKET
#endif
#ifdef AF_NETLINK
			| AF_NETLINK
#endif
			/* | AF_DECnet */
			/* | AF_IPX */
			) <= 127 ? 1 : -1];
};


int xsocket(int domain, int type, int protocol);
void xbind(int sockfd, struct sockaddr *my_addr, socklen_t addrlen);
void xlisten(int s, int backlog);
void xconnect(int s, const struct sockaddr *s_addr, socklen_t addrlen);
ssize_t xsendto(int s, const void *buf, size_t len, const struct sockaddr *to,
				socklen_t tolen);
/* SO_REUSEADDR allows a server to rebind to an address that is already
 * "in use" by old connections to e.g. previous server instance which is
 * killed or crashed. Without it bind will fail until all such connections
 * time out. Linux does not allow multiple live binds on same ip:port
 * regardless of SO_REUSEADDR (unlike some other flavors of Unix).
 * Turn it on before you call bind(). */
void setsockopt_reuseaddr(int fd); /* On Linux this never fails. */
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
	} u;
} len_and_sockaddr;
enum {
	LSA_LEN_SIZE = offsetof(len_and_sockaddr, u),
	LSA_SIZEOF_SA = sizeof(
		union {
			struct sockaddr sa;
			struct sockaddr_in sin;
#if ENABLE_FEATURE_IPV6
			struct sockaddr_in6 sin6;
#endif
		}
	)
};
/* Create stream socket, and allocate suitable lsa.
 * (lsa of correct size and lsa->sa.sa_family (AF_INET/AF_INET6))
 * af == AF_UNSPEC will result in trying to create IPv6 socket,
 * and if kernel doesn't support it, IPv4.
 */
#if ENABLE_FEATURE_IPV6
int xsocket_type(len_and_sockaddr **lsap, int af, int sock_type);
#else
int xsocket_type(len_and_sockaddr **lsap, int sock_type);
#define xsocket_type(lsap, af, sock_type) xsocket_type((lsap), (sock_type))
#endif
int xsocket_stream(len_and_sockaddr **lsap);
/* Create server socket bound to bindaddr:port. bindaddr can be NULL,
 * numeric IP ("N.N.N.N") or numeric IPv6 address,
 * and can have ":PORT" suffix (for IPv6 use "[X:X:...:X]:PORT").
 * Only if there is no suffix, port argument is used */
/* NB: these set SO_REUSEADDR before bind */
int create_and_bind_stream_or_die(const char *bindaddr, int port);
int create_and_bind_dgram_or_die(const char *bindaddr, int port);
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
/* Same, useful if you want to force family (e.g. IPv6) */
#if !ENABLE_FEATURE_IPV6
#define host_and_af2sockaddr(host, port, af) host2sockaddr((host), (port))
#define xhost_and_af2sockaddr(host, port, af) xhost2sockaddr((host), (port))
#else
len_and_sockaddr* host_and_af2sockaddr(const char *host, int port, sa_family_t af);
len_and_sockaddr* xhost_and_af2sockaddr(const char *host, int port, sa_family_t af);
#endif
/* Assign sin[6]_port member if the socket is an AF_INET[6] one,
 * otherwise no-op. Useful for ftp.
 * NB: does NOT do htons() internally, just direct assignment. */
void set_nport(len_and_sockaddr *lsa, unsigned port);
/* Retrieve sin[6]_port or return -1 for non-INET[6] lsa's */
int get_nport(const struct sockaddr *sa);
/* Reverse DNS. Returns NULL on failure. */
char* xmalloc_sockaddr2host(const struct sockaddr *sa);
/* This one doesn't append :PORTNUM */
char* xmalloc_sockaddr2host_noport(const struct sockaddr *sa);
/* This one also doesn't fall back to dotted IP (returns NULL) */
char* xmalloc_sockaddr2hostonly_noport(const struct sockaddr *sa);
/* inet_[ap]ton on steroids */
char* xmalloc_sockaddr2dotted(const struct sockaddr *sa);
char* xmalloc_sockaddr2dotted_noport(const struct sockaddr *sa);
// "old" (ipv4 only) API
// users: traceroute.c hostname.c - use _list_ of all IPs
struct hostent *xgethostbyname(const char *name);
// Also mount.c and inetd.c are using gethostbyname(),
// + inet_common.c has additional IPv4-only stuff


void socket_want_pktinfo(int fd);
ssize_t send_to_from(int fd, void *buf, size_t len, int flags,
		const struct sockaddr *to,
		const struct sockaddr *from,
		socklen_t tolen);
ssize_t recv_from_to(int fd, void *buf, size_t len, int flags,
		struct sockaddr *from,
		struct sockaddr *to,
		socklen_t sa_size);

char *xstrdup(const char *s);
char *xstrndup(const char *s, int n);
char *safe_strncpy(char *dst, const char *src, size_t size);
/* Guaranteed to NOT be a macro (smallest code). Saves nearly 2k on uclibc.
 * But potentially slow, don't use in one-billion-times loops */
int bb_putchar(int ch);
char *xasprintf(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
/* Prints unprintable chars ch as ^C or M-c to file
 * (M-c is used only if ch is ORed with PRINTABLE_META),
 * else it is printed as-is (except for ch = 0x9b) */
enum { PRINTABLE_META = 0x100 };
void fputc_printable(int ch, FILE *file);
// gcc-4.1.1 still isn't good enough at optimizing it
// (+200 bytes compared to macro)
//static ALWAYS_INLINE
//int LONE_DASH(const char *s) { return s[0] == '-' && !s[1]; }
//static ALWAYS_INLINE
//int NOT_LONE_DASH(const char *s) { return s[0] != '-' || s[1]; }
#define LONE_DASH(s)     ((s)[0] == '-' && !(s)[1])
#define NOT_LONE_DASH(s) ((s)[0] != '-' || (s)[1])
#define LONE_CHAR(s,c)     ((s)[0] == (c) && !(s)[1])
#define NOT_LONE_CHAR(s,c) ((s)[0] != (c) || (s)[1])
#define DOT_OR_DOTDOT(s) ((s)[0] == '.' && (!(s)[1] || ((s)[1] == '.' && !(s)[2])))

/* dmalloc will redefine these to it's own implementation. It is safe
 * to have the prototypes here unconditionally.  */
extern void *malloc_or_warn(size_t size);
extern void *xmalloc(size_t size);
extern void *xzalloc(size_t size);
extern void *xrealloc(void *old, size_t size);

extern ssize_t safe_read(int fd, void *buf, size_t count);
extern ssize_t nonblock_safe_read(int fd, void *buf, size_t count);
// NB: will return short read on error, not -1,
// if some data was read before error occurred
extern ssize_t full_read(int fd, void *buf, size_t count);
extern void xread(int fd, void *buf, size_t count);
extern unsigned char xread_char(int fd);
// Reads one line a-la fgets (but doesn't save terminating '\n').
// Uses single full_read() call, works only on seekable streams.
extern char *reads(int fd, char *buf, size_t count);
// Reads one line a-la fgets (but doesn't save terminating '\n').
// Reads byte-by-byte. Useful when it is important to not read ahead.
// Bytes are appended to pfx (which must be malloced, or NULL).
extern char *xmalloc_reads(int fd, char *pfx, size_t *maxsz_p);
extern ssize_t read_close(int fd, void *buf, size_t maxsz);
extern ssize_t open_read_close(const char *filename, void *buf, size_t maxsz);
/* Returns NULL if file can't be opened */
extern void *xmalloc_open_read_close(const char *filename, size_t *maxsz_p);
/* Never returns NULL */
extern void *xmalloc_xopen_read_close(const char *filename, size_t *maxsz_p);

extern ssize_t safe_write(int fd, const void *buf, size_t count);
// NB: will return short write on error, not -1,
// if some data was written before error occurred
extern ssize_t full_write(int fd, const void *buf, size_t count);
extern void xwrite(int fd, const void *buf, size_t count);
extern void xopen_xwrite_close(const char* file, const char *str);

/* Reads and prints to stdout till eof, then closes FILE. Exits on error: */
extern void xprint_and_close_file(FILE *file);
/* Reads up to (and including) TERMINATING_STRING: */
extern char *xmalloc_fgets_str(FILE *file, const char *terminating_string);
/* Chops off TERMINATING_STRING from the end: */
extern char *xmalloc_fgetline_str(FILE *file, const char *terminating_string);
/* Reads up to (and including) "\n" or NUL byte: */
extern char *xmalloc_fgets(FILE *file);
/* Chops off '\n' from the end, unlike fgets: */
extern char *xmalloc_fgetline(FILE *file);
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
extern FILE *xfopen_stdin(const char *filename);
extern FILE *fopen_or_warn_stdin(const char *filename);

int bb_pstrcmp(const void *a, const void *b);
void qsort_string_vector(char **sv, unsigned count);

/* Wrapper which restarts poll on EINTR or ENOMEM.
 * On other errors complains [perror("poll")] and returns.
 * Warning! May take (much) longer than timeout_ms to return!
 * If this is a problem, use bare poll and open-code EINTR/ENOMEM handling */
int safe_poll(struct pollfd *ufds, nfds_t nfds, int timeout_ms);

char *safe_gethostname(void);

/* Convert each alpha char in str to lower-case */
char* str_tolower(char *str);

char *utoa(unsigned n);
char *itoa(int n);
/* Returns a pointer past the formatted number, does NOT null-terminate */
char *utoa_to_buf(unsigned n, char *buf, unsigned buflen);
char *itoa_to_buf(int n, char *buf, unsigned buflen);
/* Intelligent formatters of bignums */
void smart_ulltoa4(unsigned long long ul, char buf[5], const char *scale);
void smart_ulltoa5(unsigned long long ul, char buf[5], const char *scale);
//TODO: provide pointer to buf (avoid statics)?
const char *make_human_readable_str(unsigned long long size,
		unsigned long block_size, unsigned long display_unit);
/* Put a string of hex bytes ("1b2e66fe"...), return advanced pointer */
char *bin2hex(char *buf, const char *cp, int count);

/* Last element is marked by mult == 0 */
struct suffix_mult {
	char suffix[4];
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
/* bb_getpwuid, bb_getgrgid:
 * bb_getXXXid(buf, bufsz, id) - copy user/group name or id
 *              as a string to buf, return user/group name or NULL
 * bb_getXXXid(NULL, 0, id) - return user/group name or NULL
 * bb_getXXXid(NULL, -1, id) - return user/group name or exit
*/
char *bb_getpwuid(char *name, int bufsize, long uid);
char *bb_getgrgid(char *group, int bufsize, long gid);
/* versions which cache results (useful for ps, ls etc) */
const char* get_cached_username(uid_t uid);
const char* get_cached_groupname(gid_t gid);
void clear_username_cache(void);
/* internally usernames are saved in fixed-sized char[] buffers */
enum { USERNAME_MAX_SIZE = 16 - sizeof(int) };
#if ENABLE_FEATURE_CHECK_NAMES
void die_if_bad_username(const char* name);
#else
#define die_if_bad_username(name) ((void)(name))
#endif

int execable_file(const char *name);
char *find_execable(const char *filename, char **PATHp);
int exists_execable(const char *filename);

/* BB_EXECxx always execs (it's not doing NOFORK/NOEXEC stuff),
 * but it may exec busybox and call applet instead of searching PATH.
 */
#if ENABLE_FEATURE_PREFER_APPLETS
int bb_execvp(const char *file, char *const argv[]);
#define BB_EXECVP(prog,cmd) bb_execvp(prog,cmd)
#define BB_EXECLP(prog,cmd,...) \
	execlp((find_applet_by_name(prog) >= 0) ? CONFIG_BUSYBOX_EXEC_PATH : prog, \
		cmd, __VA_ARGS__)
#else
#define BB_EXECVP(prog,cmd)     execvp(prog,cmd)
#define BB_EXECLP(prog,cmd,...) execlp(prog,cmd, __VA_ARGS__)
#endif

/* NOMMU friendy fork+exec */
pid_t spawn(char **argv);
pid_t xspawn(char **argv);

int safe_waitpid(int pid, int *wstat, int options);
/* Unlike waitpid, waits ONLY for one process.
 * It's safe to pass negative 'pids' from failed [v]fork -
 * wait4pid will return -1 (and will not clobber [v]fork's errno).
 * IOW: rc = wait4pid(spawn(argv));
 *      if (rc < 0) bb_perror_msg("%s", argv[0]);
 *      if (rc > 0) bb_error_msg("exit code: %d", rc);
 */
int wait4pid(int pid);
int wait_any_nohang(int *wstat);
#define wait_crashed(w) ((w) & 127)
#define wait_exitcode(w) ((w) >> 8)
#define wait_stopsig(w) ((w) >> 8)
#define wait_stopped(w) (((w) & 127) == 127)
/* wait4pid(spawn(argv)) + NOFORK/NOEXEC (if configured) */
int spawn_and_wait(char **argv);
struct nofork_save_area {
	jmp_buf die_jmp;
	const char *applet_name;
	int xfunc_error_retval;
	uint32_t option_mask32;
	int die_sleep;
	smallint saved;
};
void save_nofork_data(struct nofork_save_area *save);
void restore_nofork_data(struct nofork_save_area *save);
/* Does NOT check that applet is NOFORK, just blindly runs it */
int run_nofork_applet(int applet_no, char **argv);
int run_nofork_applet_prime(struct nofork_save_area *old, int applet_no, char **argv);

/* Helpers for daemonization.
 *
 * bb_daemonize(flags) = daemonize, does not compile on NOMMU
 *
 * bb_daemonize_or_rexec(flags, argv) = daemonizes on MMU (and ignores argv),
 *      rexec's itself on NOMMU with argv passed as command line.
 * Thus bb_daemonize_or_rexec may cause your <applet>_main() to be re-executed
 * from the start. (It will detect it and not reexec again second time).
 * You have to audit carefully that you don't do something twice as a result
 * (opening files/sockets, parsing config files etc...)!
 *
 * Both of the above will redirect fd 0,1,2 to /dev/null and drop ctty
 * (will do setsid()).
 *
 * forkexit_or_rexec(argv) = bare-bones "fork + parent exits" on MMU,
 *      "vfork + re-exec ourself" on NOMMU. No fd redirection, no setsid().
 *      Currently used for openvt and setsid. On MMU ignores argv.
 *
 * Helper for network daemons in foreground mode:
 *
 * bb_sanitize_stdio() = make sure that fd 0,1,2 are opened by opening them
 * to /dev/null if they are not.
 */
enum {
	DAEMON_CHDIR_ROOT = 1,
	DAEMON_DEVNULL_STDIO = 2,
	DAEMON_CLOSE_EXTRA_FDS = 4,
	DAEMON_ONLY_SANITIZE = 8, /* internal use */
};
#if BB_MMU
  void forkexit_or_rexec(void);
  enum { re_execed = 0 };
# define forkexit_or_rexec(argv)            forkexit_or_rexec()
# define bb_daemonize_or_rexec(flags, argv) bb_daemonize_or_rexec(flags)
# define bb_daemonize(flags)                bb_daemonize_or_rexec(flags, bogus)
#else
  void re_exec(char **argv) ATTRIBUTE_NORETURN;
  void forkexit_or_rexec(char **argv);
  extern bool re_execed;
  int  BUG_fork_is_unavailable_on_nommu(void);
  int  BUG_daemon_is_unavailable_on_nommu(void);
  void BUG_bb_daemonize_is_unavailable_on_nommu(void);
# define fork()          BUG_fork_is_unavailable_on_nommu()
# define daemon(a,b)     BUG_daemon_is_unavailable_on_nommu()
# define bb_daemonize(a) BUG_bb_daemonize_is_unavailable_on_nommu()
#endif
void bb_daemonize_or_rexec(int flags, char **argv);
void bb_sanitize_stdio(void);
/* Clear dangerous stuff, set PATH. Return 1 if was run by different user. */
int sanitize_env_if_suid(void);


extern const char *const bb_argv_dash[]; /* "-", NULL */
extern const char *opt_complementary;
#if ENABLE_GETOPT_LONG
#define No_argument "\0"
#define Required_argument "\001"
#define Optional_argument "\002"
extern const char *applet_long_options;
#endif
extern uint32_t option_mask32;
extern uint32_t getopt32(char **argv, const char *applet_opts, ...);


typedef struct llist_t {
	char *data;
	struct llist_t *link;
} llist_t;
void llist_add_to(llist_t **old_head, void *data);
void llist_add_to_end(llist_t **list_head, void *data);
void *llist_pop(llist_t **elm);
void llist_unlink(llist_t **head, llist_t *elm);
void llist_free(llist_t *elm, void (*freeit)(void *data));
llist_t *llist_rev(llist_t *list);
/* BTW, surprisingly, changing API to
 *   llist_t *llist_add_to(llist_t *old_head, void *data)
 * etc does not result in smaller code... */

/* start_stop_daemon and udhcpc are special - they want
 * to create pidfiles regardless of FEATURE_PIDFILE */
#if ENABLE_FEATURE_PIDFILE || defined(WANT_PIDFILE)
/* True only if we created pidfile which is *file*, not /dev/null etc */
extern smallint wrote_pidfile;
void write_pidfile(const char *path);
#define remove_pidfile(path) do { if (wrote_pidfile) unlink(path); } while (0)
#else
enum { wrote_pidfile = 0 };
#define write_pidfile(path)  ((void)0)
#define remove_pidfile(path) ((void)0)
#endif

enum {
	LOGMODE_NONE = 0,
	LOGMODE_STDIO = (1 << 0),
	LOGMODE_SYSLOG = (1 << 1) * ENABLE_FEATURE_SYSLOG,
	LOGMODE_BOTH = LOGMODE_SYSLOG + LOGMODE_STDIO,
};
extern const char *msg_eol;
extern smallint logmode;
extern int die_sleep;
extern int xfunc_error_retval;
extern jmp_buf die_jmp;
extern void xfunc_die(void) ATTRIBUTE_NORETURN;
extern void bb_show_usage(void) ATTRIBUTE_NORETURN;
extern void bb_error_msg(const char *s, ...) __attribute__ ((format (printf, 1, 2)));
extern void bb_error_msg_and_die(const char *s, ...) __attribute__ ((noreturn, format (printf, 1, 2)));
extern void bb_perror_msg(const char *s, ...) __attribute__ ((format (printf, 1, 2)));
extern void bb_simple_perror_msg(const char *s);
extern void bb_perror_msg_and_die(const char *s, ...) __attribute__ ((noreturn, format (printf, 1, 2)));
extern void bb_simple_perror_msg_and_die(const char *s) __attribute__ ((noreturn));
extern void bb_herror_msg(const char *s, ...) __attribute__ ((format (printf, 1, 2)));
extern void bb_herror_msg_and_die(const char *s, ...) __attribute__ ((noreturn, format (printf, 1, 2)));
extern void bb_perror_nomsg_and_die(void) ATTRIBUTE_NORETURN;
extern void bb_perror_nomsg(void);
extern void bb_info_msg(const char *s, ...) __attribute__ ((format (printf, 1, 2)));
extern void bb_verror_msg(const char *s, va_list p, const char *strerr);

/* We need to export XXX_main from libbusybox
 * only if we build "individual" binaries
 */
#if ENABLE_FEATURE_INDIVIDUAL
#define MAIN_EXTERNALLY_VISIBLE EXTERNALLY_VISIBLE
#else
#define MAIN_EXTERNALLY_VISIBLE
#endif


/* Applets which are useful from another applets */
int bb_cat(char** argv);
/* If shell needs them, they exist even if not enabled as applets */
int echo_main(int argc, char** argv) USE_ECHO(MAIN_EXTERNALLY_VISIBLE);
int printf_main(int argc, char **argv) USE_PRINTF(MAIN_EXTERNALLY_VISIBLE);
int test_main(int argc, char **argv) USE_TEST(MAIN_EXTERNALLY_VISIBLE);
int kill_main(int argc, char **argv) USE_KILL(MAIN_EXTERNALLY_VISIBLE);
/* Similar, but used by chgrp, not shell */
int chown_main(int argc, char **argv) USE_CHOWN(MAIN_EXTERNALLY_VISIBLE);
/* Don't need USE_xxx() guard for these */
int gunzip_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int bunzip2_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int bbunpack(char **argv,
	char* (*make_new_name)(char *filename),
	USE_DESKTOP(long long) int (*unpacker)(void)
);
#if ENABLE_ROUTE
void bb_displayroutes(int noresolve, int netstatfmt);
#endif


/* Networking */
int create_icmp_socket(void);
int create_icmp6_socket(void);
/* interface.c */
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
extern smallint interface_opt_a;
int display_interfaces(char *ifname);
#if ENABLE_FEATURE_HWIB
int in_ib(const char *bufp, struct sockaddr *sap);
#else
#define in_ib(a, b) 1 /* fail */
#endif
const struct aftype *get_aftype(const char *name);
const struct hwtype *get_hwtype(const char *name);
const struct hwtype *get_hwntype(int type);


#ifndef BUILD_INDIVIDUAL
extern int find_applet_by_name(const char *name);
/* Returns only if applet is not found. */
extern void run_applet_and_exit(const char *name, char **argv);
extern void run_applet_no_and_exit(int a, char **argv) ATTRIBUTE_NORETURN;
#endif

#ifdef HAVE_MNTENT_H
extern int match_fstype(const struct mntent *mt, const char *fstypes);
extern struct mntent *find_mount_point(const char *name, const char *table);
#endif
extern void erase_mtab(const char * name);
extern unsigned int tty_baud_to_value(speed_t speed);
extern speed_t tty_value_to_baud(unsigned int value);
extern void bb_warn_ignoring_args(int n);

extern int get_linux_version_code(void);

extern char *query_loop(const char *device);
extern int del_loop(const char *device);
/* If *devname is not NULL, use that name, otherwise try to find free one,
 * malloc and return it in *devname.
 * return value: 1: read-only loopdev was setup, 0: rw, < 0: error */
extern int set_loop(char **devname, const char *file, unsigned long long offset);


//TODO: pass buf pointer or return allocated buf (avoid statics)?
char *bb_askpass(int timeout, const char * prompt);
int bb_ask_confirmation(void);

extern int bb_parse_mode(const char* s, mode_t* theMode);

/* Concatenate path and filename to new allocated buffer.
 * Add "/" only as needed (no duplicate "//" are produced).
 * If path is NULL, it is assumed to be "/".
 * filename should not be NULL. */
char *concat_path_file(const char *path, const char *filename);
char *concat_subpath_file(const char *path, const char *filename);
const char *bb_basename(const char *name);
/* NB: can violate const-ness (similarly to strchr) */
char *last_char_is(const char *s, int c);


USE_DESKTOP(long long) int uncompress(int fd_in, int fd_out);
int inflate(int in, int out);


int bb_make_directory(char *path, long mode, int flags);

int get_signum(const char *name);
const char *get_signame(int number);
void print_signames(void);

char *bb_simplify_path(const char *path);

#define FAIL_DELAY 3
extern void bb_do_delay(int seconds);
extern void change_identity(const struct passwd *pw);
extern void run_shell(const char *shell, int loginshell, const char *command, const char **additional_args) ATTRIBUTE_NORETURN;
extern void run_shell(const char *shell, int loginshell, const char *command, const char **additional_args);
#if ENABLE_SELINUX
extern void renew_current_security_context(void);
extern void set_current_security_context(security_context_t sid);
extern context_t set_security_context_component(security_context_t cur_context,
						char *user, char *role, char *type, char *range);
extern void setfscreatecon_or_die(security_context_t scontext);
extern void selinux_preserve_fcontext(int fdesc);
#else
#define selinux_preserve_fcontext(fdesc) ((void)0)
#endif
extern void selinux_or_die(void);
extern int restricted_shell(const char *shell);

/* setup_environment:
 * if clear_env = 1: cd(pw->pw_dir), clear environment, then set
 *   TERM=(old value)
 *   USER=pw->pw_name, LOGNAME=pw->pw_name
 *   PATH=bb_default_[root_]path
 *   HOME=pw->pw_dir
 *   SHELL=shell
 * else if change_env = 1:
 *   if not root (if pw->pw_uid != 0):
 *     USER=pw->pw_name, LOGNAME=pw->pw_name
 *   HOME=pw->pw_dir
 *   SHELL=shell
 * else does nothing
 */
extern void setup_environment(const char *shell, int clear_env, int change_env, const struct passwd *pw);
extern int correct_password(const struct passwd *pw);
/* Returns a malloced string */
#if !ENABLE_USE_BB_CRYPT
#define pw_encrypt(clear, salt, cleanup) pw_encrypt(clear, salt)
#endif
extern char *pw_encrypt(const char *clear, const char *salt, int cleanup);
extern int obscure(const char *old, const char *newval, const struct passwd *pwdp);
/* rnd is additional random input. New one is returned.
 * Useful if you call crypt_make_salt many times in a row:
 * rnd = crypt_make_salt(buf1, 4, 0);
 * rnd = crypt_make_salt(buf2, 4, rnd);
 * rnd = crypt_make_salt(buf3, 4, rnd);
 * (otherwise we risk having same salt generated)
 */
extern int crypt_make_salt(char *p, int cnt, int rnd);
/* Returns number of lines changed, or -1 on error */
extern int update_passwd(const char *filename, const char *username,
			const char *new_pw);

int index_in_str_array(const char *const string_array[], const char *key);
int index_in_strings(const char *strings, const char *key);
int index_in_substr_array(const char *const string_array[], const char *key);
int index_in_substrings(const char *strings, const char *key);
const char *nth_string(const char *strings, int n);

extern void print_login_issue(const char *issue_file, const char *tty);
extern void print_login_prompt(void);

/* NB: typically you want to pass fd 0, not 1. Think 'applet | grep something' */
int get_terminal_width_height(int fd, unsigned *width, unsigned *height);

/* NB: "unsigned request" is crucial! "int request" will break some arches! */
int ioctl_or_perror(int fd, unsigned request, void *argp, const char *fmt,...) __attribute__ ((format (printf, 4, 5)));
int ioctl_or_perror_and_die(int fd, unsigned request, void *argp, const char *fmt,...) __attribute__ ((format (printf, 4, 5)));
#if ENABLE_IOCTL_HEX2STR_ERROR
int bb_ioctl_or_warn(int fd, unsigned request, void *argp, const char *ioctl_name);
int bb_xioctl(int fd, unsigned request, void *argp, const char *ioctl_name);
#define ioctl_or_warn(fd,request,argp) bb_ioctl_or_warn(fd,request,argp,#request)
#define xioctl(fd,request,argp)        bb_xioctl(fd,request,argp,#request)
#else
int bb_ioctl_or_warn(int fd, unsigned request, void *argp);
int bb_xioctl(int fd, unsigned request, void *argp);
#define ioctl_or_warn(fd,request,argp) bb_ioctl_or_warn(fd,request,argp)
#define xioctl(fd,request,argp)        bb_xioctl(fd,request,argp)
#endif

char *is_in_ino_dev_hashtable(const struct stat *statbuf);
void add_to_ino_dev_hashtable(const struct stat *statbuf, const char *name);
void reset_ino_dev_hashtable(void);
#ifdef __GLIBC__
/* At least glibc has horrendously large inline for this, so wrap it */
unsigned long long bb_makedev(unsigned int major, unsigned int minor);
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
typedef struct line_input_t {
	int flags;
	const char *path_lookup;
#if MAX_HISTORY
	int cnt_history;
	int cur_history;
	USE_FEATURE_EDITING_SAVEHISTORY(const char *hist_file;)
	char *history[MAX_HISTORY + 1];
#endif
} line_input_t;
enum {
	DO_HISTORY = 1 * (MAX_HISTORY > 0),
	SAVE_HISTORY = 2 * (MAX_HISTORY > 0) * ENABLE_FEATURE_EDITING_SAVEHISTORY,
	TAB_COMPLETION = 4 * ENABLE_FEATURE_TAB_COMPLETION,
	USERNAME_COMPLETION = 8 * ENABLE_FEATURE_USERNAME_COMPLETION,
	VI_MODE = 0x10 * ENABLE_FEATURE_EDITING_VI,
	WITH_PATH_LOOKUP = 0x20,
	FOR_SHELL = DO_HISTORY | SAVE_HISTORY | TAB_COMPLETION | USERNAME_COMPLETION,
};
line_input_t *new_line_input_t(int flags);
/* Returns:
 * -1 on read errors or EOF, or on bare Ctrl-D,
 * 0  on ctrl-C (the line entered is still returned in 'command'),
 * >0 length of input string, including terminating '\n'
 */
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
typedef struct procps_status_t {
	DIR *dir;
	uint8_t shift_pages_to_bytes;
	uint8_t shift_pages_to_kb;
/* Fields are set to 0/NULL if failed to determine (or not requested) */
	char *argv0;
	USE_SELINUX(char *context;)
	/* Everything below must contain no ptrs to malloc'ed data:
	 * it is memset(0) for each process in procps_scan() */
	unsigned long vsz, rss; /* we round it to kbytes */
	unsigned long stime, utime;
	unsigned long start_time;
	unsigned pid;
	unsigned ppid;
	unsigned pgid;
	unsigned sid;
	unsigned uid;
	unsigned gid;
	unsigned tty_major,tty_minor;
#if ENABLE_FEATURE_TOPMEM
	unsigned long mapped_rw;
	unsigned long mapped_ro;
	unsigned long shared_clean;
	unsigned long shared_dirty;
	unsigned long private_clean;
	unsigned long private_dirty;
	unsigned long stack;
#endif
	char state[4];
	/* basename of executable in exec(2), read from /proc/N/stat
	 * (if executable is symlink or script, it is NOT replaced
	 * by link target or interpreter name) */
	char comm[COMM_LEN];
	/* user/group? - use passwd/group parsing functions */
} procps_status_t;
enum {
	PSSCAN_PID      = 1 << 0,
	PSSCAN_PPID     = 1 << 1,
	PSSCAN_PGID     = 1 << 2,
	PSSCAN_SID      = 1 << 3,
	PSSCAN_UIDGID   = 1 << 4,
	PSSCAN_COMM     = 1 << 5,
	/* PSSCAN_CMD      = 1 << 6, - use read_cmdline instead */
	PSSCAN_ARGV0    = 1 << 7,
	/* PSSCAN_EXE      = 1 << 8, - not implemented */
	PSSCAN_STATE    = 1 << 9,
	PSSCAN_VSZ      = 1 << 10,
	PSSCAN_RSS      = 1 << 11,
	PSSCAN_STIME    = 1 << 12,
	PSSCAN_UTIME    = 1 << 13,
	PSSCAN_TTY      = 1 << 14,
	PSSCAN_SMAPS	= (1 << 15) * ENABLE_FEATURE_TOPMEM,
	PSSCAN_ARGVN    = (1 << 16) * (ENABLE_PGREP | ENABLE_PKILL),
	USE_SELINUX(PSSCAN_CONTEXT = 1 << 17,)
	PSSCAN_START_TIME = 1 << 18,
	/* These are all retrieved from proc/NN/stat in one go: */
	PSSCAN_STAT     = PSSCAN_PPID | PSSCAN_PGID | PSSCAN_SID
	                | PSSCAN_COMM | PSSCAN_STATE
	                | PSSCAN_VSZ | PSSCAN_RSS
			| PSSCAN_STIME | PSSCAN_UTIME | PSSCAN_START_TIME
			| PSSCAN_TTY,
};
//procps_status_t* alloc_procps_scan(void);
void free_procps_scan(procps_status_t* sp);
procps_status_t* procps_scan(procps_status_t* sp, int flags);
/* Format cmdline (up to col chars) into char buf[col+1] */
/* Puts [comm] if cmdline is empty (-> process is a kernel thread) */
void read_cmdline(char *buf, int col, unsigned pid, const char *comm);
pid_t *find_pid_by_name(const char* procName);
pid_t *pidlist_reverse(pid_t *pidList);


extern const char bb_uuenc_tbl_base64[];
extern const char bb_uuenc_tbl_std[];
void bb_uuencode(char *store, const void *s, int length, const char *tbl);

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

uint32_t *crc32_filltable(uint32_t *tbl256, int endian);


extern const char *applet_name;
/* "BusyBox vN.N.N (timestamp or extra_version)" */
extern const char bb_banner[];
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
extern const char bb_path_passwd_file[];
extern const char bb_path_shadow_file[];
extern const char bb_path_gshadow_file[];
extern const char bb_path_group_file[];
extern const char bb_path_motd_file[];
extern const char bb_path_wtmp_file[];
extern const char bb_dev_null[];
extern const char bb_busybox_exec_path[];
/* util-linux manpage says /sbin:/bin:/usr/sbin:/usr/bin,
 * but I want to save a few bytes here */
extern const char bb_PATH_root_path[]; /* "PATH=/sbin:/usr/sbin:/bin:/usr/bin" */
#define bb_default_root_path (bb_PATH_root_path + sizeof("PATH"))
#define bb_default_path      (bb_PATH_root_path + sizeof("PATH=/sbin:/usr/sbin"))

extern const int const_int_0;
extern const int const_int_1;


#ifndef BUFSIZ
#define BUFSIZ 4096
#endif
/* Providing hard guarantee on minimum size (think of BUFSIZ == 128) */
enum { COMMON_BUFSIZE = (BUFSIZ >= 256*sizeof(void*) ? BUFSIZ+1 : 256*sizeof(void*)) };
extern char bb_common_bufsiz1[COMMON_BUFSIZE];
/* This struct is deliberately not defined. */
/* See docs/keep_data_small.txt */
struct globals;
/* '*const' ptr makes gcc optimize code much better.
 * Magic prevents ptr_to_globals from going into rodata.
 * If you want to assign a value, use SET_PTR_TO_GLOBALS(x) */
extern struct globals *const ptr_to_globals;
/* At least gcc 3.4.6 on mipsel system needs optimization barrier */
#define barrier() __asm__ __volatile__("":::"memory")
#define SET_PTR_TO_GLOBALS(x) do { \
	(*(struct globals**)&ptr_to_globals) = (x); \
	barrier(); \
} while (0)

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

typedef struct masks_labels_t {
	const char *labels;
	const int masks[];
} masks_labels_t;

int print_flags_separated(const int *masks, const char *labels,
		int flags, const char *separator);
extern int print_flags(const masks_labels_t *ml, int flags);

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
# define LOOP_NAMESIZE (sizeof("/dev/loop/") + sizeof(int)*3 + 1)
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
# define LOOP_NAMESIZE (sizeof("/dev/loop") + sizeof(int)*3 + 1)
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

#define ARRAY_SIZE(x) ((unsigned)(sizeof(x) / sizeof((x)[0])))


#if __GNUC_PREREQ(4,1)
# pragma GCC visibility pop
#endif


#endif /* __LIBBUSYBOX_H__ */
