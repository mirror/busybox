/* vi: set sw=4 ts=4: */
/*
 * nfsmount.c -- Linux NFS mount
 * Copyright (C) 1993 Rick Sladkey <jrs@world.std.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * Wed Feb  8 12:51:48 1995, biro@yggdrasil.com (Ross Biro): allow all port
 * numbers to be specified on the command line.
 *
 * Fri, 8 Mar 1996 18:01:39, Swen Thuemmler <swen@uni-paderborn.de>:
 * Omit the call to connect() for Linux version 1.3.11 or later.
 *
 * Wed Oct  1 23:55:28 1997: Dick Streefland <dick_streefland@tasking.com>
 * Implemented the "bg", "fg" and "retry" mount options for NFS.
 *
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@misiek.eu.org>
 * - added Native Language Support
 *
 * Modified by Olaf Kirch and Trond Myklebust for new NFS code,
 * plus NFSv3 stuff.
 */

#include "busybox.h"
#include <sys/utsname.h>
#undef TRUE
#undef FALSE
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>

/* This is just a warning of a common mistake.  Possibly this should be a
 * uclibc faq entry rather than in busybox... */
#if ENABLE_FEATURE_MOUNT_NFS && defined(__UCLIBC__) && ! defined(__UCLIBC_HAS_RPC__)
#error "You need to build uClibc with UCLIBC_HAS_RPC for NFS support."
#endif

/* former nfsmount.h */

#define MOUNTPORT 635
#define MNTPATHLEN 1024
#define MNTNAMLEN 255
#define FHSIZE 32
#define FHSIZE3 64

typedef char fhandle[FHSIZE];

typedef struct {
	unsigned int fhandle3_len;
	char *fhandle3_val;
} fhandle3;

enum mountstat3 {
	MNT_OK = 0,
	MNT3ERR_PERM = 1,
	MNT3ERR_NOENT = 2,
	MNT3ERR_IO = 5,
	MNT3ERR_ACCES = 13,
	MNT3ERR_NOTDIR = 20,
	MNT3ERR_INVAL = 22,
	MNT3ERR_NAMETOOLONG = 63,
	MNT3ERR_NOTSUPP = 10004,
	MNT3ERR_SERVERFAULT = 10006,
};
typedef enum mountstat3 mountstat3;

struct fhstatus {
	unsigned int fhs_status;
	union {
		fhandle fhs_fhandle;
	} fhstatus_u;
};
typedef struct fhstatus fhstatus;

struct mountres3_ok {
	fhandle3 fhandle;
	struct {
		unsigned int auth_flavours_len;
		char *auth_flavours_val;
	} auth_flavours;
};
typedef struct mountres3_ok mountres3_ok;

struct mountres3 {
	mountstat3 fhs_status;
	union {
		mountres3_ok mountinfo;
	} mountres3_u;
};
typedef struct mountres3 mountres3;

typedef char *dirpath;

typedef char *name;

typedef struct mountbody *mountlist;

struct mountbody {
	name ml_hostname;
	dirpath ml_directory;
	mountlist ml_next;
};
typedef struct mountbody mountbody;

typedef struct groupnode *groups;

struct groupnode {
	name gr_name;
	groups gr_next;
};
typedef struct groupnode groupnode;

typedef struct exportnode *exports;

struct exportnode {
	dirpath ex_dir;
	groups ex_groups;
	exports ex_next;
};
typedef struct exportnode exportnode;

struct ppathcnf {
	int pc_link_max;
	short pc_max_canon;
	short pc_max_input;
	short pc_name_max;
	short pc_path_max;
	short pc_pipe_buf;
	u_char pc_vdisable;
	char pc_xxx;
	short pc_mask[2];
};
typedef struct ppathcnf ppathcnf;

#define MOUNTPROG 100005
#define MOUNTVERS 1

#define MOUNTPROC_NULL 0
#define MOUNTPROC_MNT 1
#define MOUNTPROC_DUMP 2
#define MOUNTPROC_UMNT 3
#define MOUNTPROC_UMNTALL 4
#define MOUNTPROC_EXPORT 5
#define MOUNTPROC_EXPORTALL 6

#define MOUNTVERS_POSIX 2

#define MOUNTPROC_PATHCONF 7

#define MOUNT_V3 3

#define MOUNTPROC3_NULL 0
#define MOUNTPROC3_MNT 1
#define MOUNTPROC3_DUMP 2
#define MOUNTPROC3_UMNT 3
#define MOUNTPROC3_UMNTALL 4
#define MOUNTPROC3_EXPORT 5

/* former nfsmount.h ends */

enum {
#ifndef NFS_FHSIZE
	NFS_FHSIZE = 32,
#endif
#ifndef NFS_PORT
	NFS_PORT = 2049
#endif
};

//enum {
//	S_QUOTA = 128,     /* Quota initialized for file/directory/symlink */
//};


/*
 * We want to be able to compile mount on old kernels in such a way
 * that the binary will work well on more recent kernels.
 * Thus, if necessary we teach nfsmount.c the structure of new fields
 * that will come later.
 *
 * Moreover, the new kernel includes conflict with glibc includes
 * so it is easiest to ignore the kernel altogether (at compile time).
 */

struct nfs2_fh {
	char                    data[32];
};
struct nfs3_fh {
	unsigned short          size;
	unsigned char           data[64];
};

struct nfs_mount_data {
	int		version;		/* 1 */
	int		fd;			/* 1 */
	struct nfs2_fh	old_root;		/* 1 */
	int		flags;			/* 1 */
	int		rsize;			/* 1 */
	int		wsize;			/* 1 */
	int		timeo;			/* 1 */
	int		retrans;		/* 1 */
	int		acregmin;		/* 1 */
	int		acregmax;		/* 1 */
	int		acdirmin;		/* 1 */
	int		acdirmax;		/* 1 */
	struct sockaddr_in addr;		/* 1 */
	char		hostname[256];		/* 1 */
	int		namlen;			/* 2 */
	unsigned int	bsize;			/* 3 */
	struct nfs3_fh	root;			/* 4 */
};

/* bits in the flags field */
enum {
	NFS_MOUNT_SOFT = 0x0001,	/* 1 */
	NFS_MOUNT_INTR = 0x0002,	/* 1 */
	NFS_MOUNT_SECURE = 0x0004,	/* 1 */
	NFS_MOUNT_POSIX = 0x0008,	/* 1 */
	NFS_MOUNT_NOCTO = 0x0010,	/* 1 */
	NFS_MOUNT_NOAC = 0x0020,	/* 1 */
	NFS_MOUNT_TCP = 0x0040,		/* 2 */
	NFS_MOUNT_VER3 = 0x0080,	/* 3 */
	NFS_MOUNT_KERBEROS = 0x0100,	/* 3 */
	NFS_MOUNT_NONLM = 0x0200	/* 3 */
};

#define HAVE_inet_aton

/*
 * We need to translate between nfs status return values and
 * the local errno values which may not be the same.
 *
 * Andreas Schwab <schwab@LS5.informatik.uni-dortmund.de>: change errno:
 * "after #include <errno.h> the symbol errno is reserved for any use,
 *  it cannot even be used as a struct tag or field name".
 */

#ifndef EDQUOT
#define EDQUOT	ENOSPC
#endif

// Convert each NFSERR_BLAH into EBLAH

static const struct {
	int stat;
	int errnum;
} nfs_errtbl[] = {
	{0,0}, {1,EPERM}, {2,ENOENT}, {5,EIO}, {6,ENXIO}, {13,EACCES}, {17,EEXIST},
	{19,ENODEV}, {20,ENOTDIR}, {21,EISDIR}, {22,EINVAL}, {27,EFBIG},
	{28,ENOSPC}, {30,EROFS}, {63,ENAMETOOLONG}, {66,ENOTEMPTY}, {69,EDQUOT},
	{70,ESTALE}, {71,EREMOTE}, {-1,EIO}
};

static char *nfs_strerror(int status)
{
	int i;
	static char buf[256];

	for (i = 0; nfs_errtbl[i].stat != -1; i++) {
		if (nfs_errtbl[i].stat == status)
			return strerror(nfs_errtbl[i].errnum);
	}
	sprintf(buf, "unknown nfs status return value: %d", status);
	return buf;
}

static bool_t xdr_fhandle(XDR *xdrs, fhandle objp)
{
	if (!xdr_opaque(xdrs, objp, FHSIZE))
		 return FALSE;
	return TRUE;
}

static bool_t xdr_fhstatus(XDR *xdrs, fhstatus *objp)
{
	if (!xdr_u_int(xdrs, &objp->fhs_status))
		 return FALSE;
	switch (objp->fhs_status) {
	case 0:
		if (!xdr_fhandle(xdrs, objp->fhstatus_u.fhs_fhandle))
			 return FALSE;
		break;
	default:
		break;
	}
	return TRUE;
}

static bool_t xdr_dirpath(XDR *xdrs, dirpath *objp)
{
	if (!xdr_string(xdrs, objp, MNTPATHLEN))
		 return FALSE;
	return TRUE;
}

static bool_t xdr_fhandle3(XDR *xdrs, fhandle3 *objp)
{
	if (!xdr_bytes(xdrs, (char **)&objp->fhandle3_val, (unsigned int *) &objp->fhandle3_len, FHSIZE3))
		 return FALSE;
	return TRUE;
}

static bool_t xdr_mountres3_ok(XDR *xdrs, mountres3_ok *objp)
{
	if (!xdr_fhandle3(xdrs, &objp->fhandle))
		return FALSE;
	if (!xdr_array(xdrs, &(objp->auth_flavours.auth_flavours_val), &(objp->auth_flavours.auth_flavours_len), ~0,
				sizeof (int), (xdrproc_t) xdr_int))
		return FALSE;
	return TRUE;
}

static bool_t xdr_mountstat3(XDR *xdrs, mountstat3 *objp)
{
	if (!xdr_enum(xdrs, (enum_t *) objp))
		 return FALSE;
	return TRUE;
}

static bool_t xdr_mountres3(XDR *xdrs, mountres3 *objp)
{
	if (!xdr_mountstat3(xdrs, &objp->fhs_status))
		return FALSE;
	switch (objp->fhs_status) {
	case MNT_OK:
		if (!xdr_mountres3_ok(xdrs, &objp->mountres3_u.mountinfo))
			 return FALSE;
		break;
	default:
		break;
	}
	return TRUE;
}


#define MAX_NFSPROT ((nfs_mount_version >= 4) ? 3 : 2)

enum {
	EX_FAIL = 32,       /* mount failure */
	EX_BG = 256        /* retry in background (internal only) */
};


/*
 * nfs_mount_version according to the sources seen at compile time.
 */
static int nfs_mount_version;
static int kernel_version;

/*
 * Unfortunately, the kernel prints annoying console messages
 * in case of an unexpected nfs mount version (instead of
 * just returning some error).  Therefore we'll have to try
 * and figure out what version the kernel expects.
 *
 * Variables:
 *	KERNEL_NFS_MOUNT_VERSION: kernel sources at compile time
 *	NFS_MOUNT_VERSION: these nfsmount sources at compile time
 *	nfs_mount_version: version this source and running kernel can handle
 */
static void
find_kernel_nfs_mount_version(void)
{
	if (kernel_version)
		return;

	nfs_mount_version = 4; /* default */

	kernel_version = get_linux_version_code();
	if (kernel_version) {
		if (kernel_version < KERNEL_VERSION(2,1,32))
			nfs_mount_version = 1;
		else if (kernel_version < KERNEL_VERSION(2,2,18) ||
				(kernel_version >= KERNEL_VERSION(2,3,0) &&
				 kernel_version < KERNEL_VERSION(2,3,99)))
			nfs_mount_version = 3;
		/* else v4 since 2.3.99pre4 */
	}
}

static struct pmap *
get_mountport(struct sockaddr_in *server_addr,
	long unsigned prog,
	long unsigned version,
	long unsigned proto,
	long unsigned port)
{
	struct pmaplist *pmap;
	static struct pmap p = {0, 0, 0, 0};

	server_addr->sin_port = PMAPPORT;
	pmap = pmap_getmaps(server_addr);

	if (version > MAX_NFSPROT)
		version = MAX_NFSPROT;
	if (!prog)
		prog = MOUNTPROG;
	p.pm_prog = prog;
	p.pm_vers = version;
	p.pm_prot = proto;
	p.pm_port = port;
	
	while (pmap) {
		if (pmap->pml_map.pm_prog != prog)
			goto next;
		if (!version && p.pm_vers > pmap->pml_map.pm_vers)
			goto next;
		if (version > 2 && pmap->pml_map.pm_vers != version)
			goto next;
		if (version && version <= 2 && pmap->pml_map.pm_vers > 2)
			goto next;
		if (pmap->pml_map.pm_vers > MAX_NFSPROT ||
		    (proto && p.pm_prot && pmap->pml_map.pm_prot != proto) ||
		    (port && pmap->pml_map.pm_port != port))
			goto next;
		memcpy(&p, &pmap->pml_map, sizeof(p));
next:
		pmap = pmap->pml_next;
	}
	if (!p.pm_vers)
		p.pm_vers = MOUNTVERS;
	if (!p.pm_port)
		p.pm_port = MOUNTPORT;
	if (!p.pm_prot)
		p.pm_prot = IPPROTO_TCP;
	return &p;
}

int nfsmount(const char *spec, const char *node, int *flags,
	     char **mount_opts, int running_bg)
{
	/* prev_bg_host is a -o bg support:
	 * "bg: if the first NFS mount attempt times out,
	 * retry the mount in the background.
	 * After a mount operation is backgrounded,
	 * all subsequent mounts on the same NFS server
	 * will be backgrounded immediately, without first
	 * attempting the mount. A missing mount point is treated
	 * as a timeout, to allow for nested NFS mounts."
	 *
	 * But current implementation is a dirty hack -
	 * it works only if all mounts are to one host!
	 * IOW: think what will happen if you have this sequence:
	 * mount a.a.a.a:/dir /mnt/a -o bg
	 * mount b.b.b.b:/dir /mnt/b -o bg
	 * mount a.a.a.a:/dir /mnt/a -o bg
	 * mount b.b.b.b:/dir /mnt/b -o bg
	 */

	static char *prev_bg_host;
	char *hostdir;
	CLIENT *mclient;
	char *hostname;
	char *pathname;
	char *old_opts;
	char *mounthost;
	struct timeval total_timeout;
	enum clnt_stat clnt_stat;
	struct nfs_mount_data data;
	char *opt, *opteq;
	int val;
	struct hostent *hp;
	struct sockaddr_in server_addr;
	struct sockaddr_in mount_server_addr;
	struct pmap* pm_mnt;
	int msock, fsock;
	struct timeval retry_timeout;
	union {
		struct fhstatus nfsv2;
		struct mountres3 nfsv3;
	} status;
	struct stat statbuf;
	char *s;
	int port;
	int mountport;
	int proto;
	int bg;
	int soft;
	int intr;
	int posix;
	int nocto;
	int noac;
	int nolock;
	int retry;
	int tcp;
	int mountprog;
	int mountvers;
	int nfsprog;
	int nfsvers;
	int retval;
	time_t t;
	time_t prevt;
	time_t timeout;

	find_kernel_nfs_mount_version();

	/* NB: old_opts and hostdir must be free()d prior to return! */

	old_opts = NULL;
	mounthost = NULL;
	retval = EX_FAIL;
	msock = fsock = -1;
	mclient = NULL;

	hostdir = xstrdup(spec);
	/* mount_main() guarantees that ':' is there */
	s = strchr(hostdir, ':');
	hostname = hostdir;
	pathname = s + 1;
	*s = '\0';
	/* Ignore all but first hostname in replicated mounts
	   until they can be fully supported. (mack@sgi.com) */
	s = strchr(hostdir, ',');
	if (s) {
		*s = '\0';
		bb_error_msg("warning: multiple hostnames not supported");
	}

	server_addr.sin_family = AF_INET;
#ifdef HAVE_inet_aton
	if (!inet_aton(hostname, &server_addr.sin_addr))
#endif
	{
		hp = gethostbyname(hostname);
		if (hp == NULL) {
			bb_herror_msg("%s", hostname);
			goto fail;
		}
		if (hp->h_length > sizeof(struct in_addr)) {
			bb_error_msg("got bad hp->h_length");
			hp->h_length = sizeof(struct in_addr);
		}
		memcpy(&server_addr.sin_addr,
				hp->h_addr, hp->h_length);
	}

	memcpy(&mount_server_addr, &server_addr, sizeof(mount_server_addr));

	/* add IP address to mtab options for use when unmounting */

	s = inet_ntoa(server_addr.sin_addr);
	old_opts = *mount_opts;
	if (!old_opts)
		old_opts = xstrdup("");
	*mount_opts = xasprintf("%s%saddr=%s",
				old_opts, *old_opts ? "," : "", s);

	/* Set default options.
	 * rsize/wsize (and bsize, for ver >= 3) are left 0 in order to
	 * let the kernel decide.
	 * timeo is filled in after we know whether it'll be TCP or UDP. */
	memset(&data, 0, sizeof(data));
	data.retrans	= 3;
	data.acregmin	= 3;
	data.acregmax	= 60;
	data.acdirmin	= 30;
	data.acdirmax	= 60;
	data.namlen	= NAME_MAX;

	bg = 0;
	soft = 0;
	intr = 0;
	posix = 0;
	nocto = 0;
	nolock = 0;
	noac = 0;
	retry = 10000;		/* 10000 minutes ~ 1 week */
	tcp = 0;

	mountprog = MOUNTPROG;
	mountvers = 0;
	port = 0;
	mountport = 0;
	nfsprog = 100003;
	nfsvers = 0;

	/* parse options */

	for (opt = strtok(old_opts, ","); opt; opt = strtok(NULL, ",")) {
		opteq = strchr(opt, '=');
		if (opteq) {
			val = atoi(opteq + 1);
			*opteq = '\0';
			if (!strcmp(opt, "rsize"))
				data.rsize = val;
			else if (!strcmp(opt, "wsize"))
				data.wsize = val;
			else if (!strcmp(opt, "timeo"))
				data.timeo = val;
			else if (!strcmp(opt, "retrans"))
				data.retrans = val;
			else if (!strcmp(opt, "acregmin"))
				data.acregmin = val;
			else if (!strcmp(opt, "acregmax"))
				data.acregmax = val;
			else if (!strcmp(opt, "acdirmin"))
				data.acdirmin = val;
			else if (!strcmp(opt, "acdirmax"))
				data.acdirmax = val;
			else if (!strcmp(opt, "actimeo")) {
				data.acregmin = val;
				data.acregmax = val;
				data.acdirmin = val;
				data.acdirmax = val;
			}
			else if (!strcmp(opt, "retry"))
				retry = val;
			else if (!strcmp(opt, "port"))
				port = val;
			else if (!strcmp(opt, "mountport"))
				mountport = val;
			else if (!strcmp(opt, "mounthost"))
				mounthost = xstrndup(opteq+1,
						  strcspn(opteq+1," \t\n\r,"));
			else if (!strcmp(opt, "mountprog"))
				mountprog = val;
			else if (!strcmp(opt, "mountvers"))
				mountvers = val;
			else if (!strcmp(opt, "nfsprog"))
				nfsprog = val;
			else if (!strcmp(opt, "nfsvers") ||
				 !strcmp(opt, "vers"))
				nfsvers = val;
			else if (!strcmp(opt, "proto")) {
				if (!strncmp(opteq+1, "tcp", 3))
					tcp = 1;
				else if (!strncmp(opteq+1, "udp", 3))
					tcp = 0;
				else
					bb_error_msg("warning: unrecognized proto= option");
			} else if (!strcmp(opt, "namlen")) {
				if (nfs_mount_version >= 2)
					data.namlen = val;
				else
					bb_error_msg("warning: option namlen is not supported\n");
			} else if (!strcmp(opt, "addr"))
				/* ignore */;
			else {
				bb_error_msg("unknown nfs mount parameter: %s=%d", opt, val);
				goto fail;
			}
		}
		else {
			val = 1;
			if (!strncmp(opt, "no", 2)) {
				val = 0;
				opt += 2;
			}
			if (!strcmp(opt, "bg"))
				bg = val;
			else if (!strcmp(opt, "fg"))
				bg = !val;
			else if (!strcmp(opt, "soft"))
				soft = val;
			else if (!strcmp(opt, "hard"))
				soft = !val;
			else if (!strcmp(opt, "intr"))
				intr = val;
			else if (!strcmp(opt, "posix"))
				posix = val;
			else if (!strcmp(opt, "cto"))
				nocto = !val;
			else if (!strcmp(opt, "ac"))
				noac = !val;
			else if (!strcmp(opt, "tcp"))
				tcp = val;
			else if (!strcmp(opt, "udp"))
				tcp = !val;
			else if (!strcmp(opt, "lock")) {
				if (nfs_mount_version >= 3)
					nolock = !val;
				else
					bb_error_msg("warning: option nolock is not supported");
			} else {
				bb_error_msg("unknown nfs mount option: %s%s", val ? "" : "no", opt);
				goto fail;
			}
		}
	}
	proto = (tcp) ? IPPROTO_TCP : IPPROTO_UDP;

	data.flags = (soft ? NFS_MOUNT_SOFT : 0)
		| (intr ? NFS_MOUNT_INTR : 0)
		| (posix ? NFS_MOUNT_POSIX : 0)
		| (nocto ? NFS_MOUNT_NOCTO : 0)
		| (noac ? NFS_MOUNT_NOAC : 0);
	if (nfs_mount_version >= 2)
		data.flags |= (tcp ? NFS_MOUNT_TCP : 0);
	if (nfs_mount_version >= 3)
		data.flags |= (nolock ? NFS_MOUNT_NONLM : 0);
	if (nfsvers > MAX_NFSPROT || mountvers > MAX_NFSPROT) {
		bb_error_msg("NFSv%d not supported", nfsvers);
		goto fail;
	}
	if (nfsvers && !mountvers)
		mountvers = (nfsvers < 3) ? 1 : nfsvers;
	if (nfsvers && nfsvers < mountvers) {
		mountvers = nfsvers;
	}

	/* Adjust options if none specified */
	if (!data.timeo)
		data.timeo = tcp ? 70 : 7;

#ifdef NFS_MOUNT_DEBUG
	printf("rsize = %d, wsize = %d, timeo = %d, retrans = %d\n",
		data.rsize, data.wsize, data.timeo, data.retrans);
	printf("acreg (min, max) = (%d, %d), acdir (min, max) = (%d, %d)\n",
		data.acregmin, data.acregmax, data.acdirmin, data.acdirmax);
	printf("port = %d, bg = %d, retry = %d, flags = %.8x\n",
		port, bg, retry, data.flags);
	printf("mountprog = %d, mountvers = %d, nfsprog = %d, nfsvers = %d\n",
		mountprog, mountvers, nfsprog, nfsvers);
	printf("soft = %d, intr = %d, posix = %d, nocto = %d, noac = %d\n",
		(data.flags & NFS_MOUNT_SOFT) != 0,
		(data.flags & NFS_MOUNT_INTR) != 0,
		(data.flags & NFS_MOUNT_POSIX) != 0,
		(data.flags & NFS_MOUNT_NOCTO) != 0,
		(data.flags & NFS_MOUNT_NOAC) != 0);
	printf("tcp = %d\n",
		(data.flags & NFS_MOUNT_TCP) != 0);
#endif

	data.version = nfs_mount_version;

	if (*flags & MS_REMOUNT)
		goto copy_data_and_return;

	/*
	 * If the previous mount operation on the same host was
	 * backgrounded, and the "bg" for this mount is also set,
	 * give up immediately, to avoid the initial timeout.
	 */
	if (bg && !running_bg &&
	    prev_bg_host && strcmp(hostname, prev_bg_host) == 0) {
		if (retry > 0)
			retval = EX_BG;
		goto ret;
	}

	/* create mount daemon client */
	/* See if the nfs host = mount host. */
	if (mounthost) {
		if (mounthost[0] >= '0' && mounthost[0] <= '9') {
			mount_server_addr.sin_family = AF_INET;
			mount_server_addr.sin_addr.s_addr = inet_addr(hostname);
		} else {
			if ((hp = gethostbyname(mounthost)) == NULL) {
				bb_herror_msg("%s", mounthost);
				goto fail;
			} else {
				if (hp->h_length > sizeof(struct in_addr)) {
					bb_error_msg("got bad hp->h_length?");
					hp->h_length = sizeof(struct in_addr);
				}
				mount_server_addr.sin_family = AF_INET;
				memcpy(&mount_server_addr.sin_addr,
						hp->h_addr, hp->h_length);
			}
		}
	}

	/*
	 * The following loop implements the mount retries. On the first
	 * call, "running_bg" is 0. When the mount times out, and the
	 * "bg" option is set, the exit status EX_BG will be returned.
	 * For a backgrounded mount, there will be a second call by the
	 * child process with "running_bg" set to 1.
	 *
	 * The case where the mount point is not present and the "bg"
	 * option is set, is treated as a timeout. This is done to
	 * support nested mounts.
	 *
	 * The "retry" count specified by the user is the number of
	 * minutes to retry before giving up.
	 *
	 * Only the first error message will be displayed.
	 */
	retry_timeout.tv_sec = 3;
	retry_timeout.tv_usec = 0;
	total_timeout.tv_sec = 20;
	total_timeout.tv_usec = 0;
	timeout = time(NULL) + 60 * retry;
	prevt = 0;
	t = 30;
	val = 1;
	for (;;) {
		if (bg && stat(node, &statbuf) == -1) {
			if (running_bg) {
				sleep(val);	/* 1, 2, 4, 8, 16, 30, ... */
				val *= 2;
				if (val > 30)
					val = 30;
			}
		} else {
			/* be careful not to use too many CPU cycles */
			if (t - prevt < 30)
				sleep(30);

			pm_mnt = get_mountport(&mount_server_addr,
					mountprog,
					mountvers,
					proto,
					mountport);

			/* contact the mount daemon via TCP */
			mount_server_addr.sin_port = htons(pm_mnt->pm_port);
			msock = RPC_ANYSOCK;

			switch (pm_mnt->pm_prot) {
			case IPPROTO_UDP:
				mclient = clntudp_create(&mount_server_addr,
						 pm_mnt->pm_prog,
						 pm_mnt->pm_vers,
						 retry_timeout,
						 &msock);
				if (mclient)
					break;
				mount_server_addr.sin_port = htons(pm_mnt->pm_port);
				msock = RPC_ANYSOCK;
			case IPPROTO_TCP:
				mclient = clnttcp_create(&mount_server_addr,
						 pm_mnt->pm_prog,
						 pm_mnt->pm_vers,
						 &msock, 0, 0);
				break;
			default:
				mclient = 0;
			}
			if (mclient) {
				/* try to mount hostname:pathname */
				mclient->cl_auth = authunix_create_default();

				/* make pointers in xdr_mountres3 NULL so
				 * that xdr_array allocates memory for us
				 */
				memset(&status, 0, sizeof(status));

				if (pm_mnt->pm_vers == 3)
					clnt_stat = clnt_call(mclient, MOUNTPROC3_MNT,
						      (xdrproc_t) xdr_dirpath,
						      (caddr_t) &pathname,
						      (xdrproc_t) xdr_mountres3,
						      (caddr_t) &status,
						      total_timeout);
				else
					clnt_stat = clnt_call(mclient, MOUNTPROC_MNT,
						      (xdrproc_t) xdr_dirpath,
						      (caddr_t) &pathname,
						      (xdrproc_t) xdr_fhstatus,
						      (caddr_t) &status,
						      total_timeout);

				if (clnt_stat == RPC_SUCCESS)
					break;		/* we're done */
				if (errno != ECONNREFUSED) {
					clnt_perror(mclient, "mount");
					goto fail;	/* don't retry */
				}
				if (!running_bg && prevt == 0)
					clnt_perror(mclient, "mount");
				auth_destroy(mclient->cl_auth);
				clnt_destroy(mclient);
				mclient = 0;
				close(msock);
			} else {
				if (!running_bg && prevt == 0)
					clnt_pcreateerror("mount");
			}
			prevt = t;
		}
		if (!bg)
			goto fail;
		if (!running_bg) {
			prev_bg_host = xstrdup(hostname);
			if (retry > 0)
				retval = EX_BG;
			goto fail;
		}
		t = time(NULL);
		if (t >= timeout)
			goto fail;
	}
	nfsvers = (pm_mnt->pm_vers < 2) ? 2 : pm_mnt->pm_vers;

	if (nfsvers == 2) {
		if (status.nfsv2.fhs_status != 0) {
			bb_error_msg("%s:%s failed, reason given by server: %s",
				hostname, pathname,
				nfs_strerror(status.nfsv2.fhs_status));
			goto fail;
		}
		memcpy(data.root.data,
				(char *) status.nfsv2.fhstatus_u.fhs_fhandle,
				NFS_FHSIZE);
		data.root.size = NFS_FHSIZE;
		memcpy(data.old_root.data,
				(char *) status.nfsv2.fhstatus_u.fhs_fhandle,
				NFS_FHSIZE);
	} else {
		fhandle3 *my_fhandle;
		if (status.nfsv3.fhs_status != 0) {
			bb_error_msg("%s:%s failed, reason given by server: %s",
				hostname, pathname,
				nfs_strerror(status.nfsv3.fhs_status));
			goto fail;
		}
		my_fhandle = &status.nfsv3.mountres3_u.mountinfo.fhandle;
		memset(data.old_root.data, 0, NFS_FHSIZE);
		memset(&data.root, 0, sizeof(data.root));
		data.root.size = my_fhandle->fhandle3_len;
		memcpy(data.root.data,
				(char *) my_fhandle->fhandle3_val,
				my_fhandle->fhandle3_len);

		data.flags |= NFS_MOUNT_VER3;
	}

	/* create nfs socket for kernel */

	if (tcp) {
		if (nfs_mount_version < 3) {
			bb_error_msg("NFS over TCP is not supported");
			goto fail;
		}
		fsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	} else
		fsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (fsock < 0) {
		perror("nfs socket");
		goto fail;
	}
	if (bindresvport(fsock, 0) < 0) {
		perror("nfs bindresvport");
		goto fail;
	}
	if (port == 0) {
		server_addr.sin_port = PMAPPORT;
		port = pmap_getport(&server_addr, nfsprog, nfsvers,
			tcp ? IPPROTO_TCP : IPPROTO_UDP);
		if (port == 0)
			port = NFS_PORT;
#ifdef NFS_MOUNT_DEBUG
		else
			printf("used portmapper to find NFS port\n");
#endif
	}
#ifdef NFS_MOUNT_DEBUG
	printf("using port %d for nfs daemon\n", port);
#endif
	server_addr.sin_port = htons(port);

	/* prepare data structure for kernel */

	data.fd = fsock;
	memcpy((char *) &data.addr, (char *) &server_addr, sizeof(data.addr));
	strncpy(data.hostname, hostname, sizeof(data.hostname));

	/* clean up */

	auth_destroy(mclient->cl_auth);
	clnt_destroy(mclient);
	close(msock);
copy_data_and_return:
	*mount_opts = xrealloc(*mount_opts, sizeof(data));
	memcpy(*mount_opts, &data, sizeof(data));

	retval = 0;
	goto ret;

	/* abort */

fail:
	if (msock != -1) {
		if (mclient) {
			auth_destroy(mclient->cl_auth);
			clnt_destroy(mclient);
		}
		close(msock);
	}
	if (fsock != -1)
		close(fsock);

ret:
	free(hostdir);
	free(old_opts);
	return retval;
}
