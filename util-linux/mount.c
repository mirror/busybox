/* vi: set sw=4 ts=4: */
/*
 * Mini mount implementation for busybox
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2005-2006 by Rob Landley <rob@landley.net>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* Design notes: There is no spec for mount.  Remind me to write one.

   mount_main() calls singlemount() which calls mount_it_now().

   mount_main() can loop through /etc/fstab for mount -a
   singlemount() can loop through /etc/filesystems for fstype detection.
   mount_it_now() does the actual mount.
*/

#include "busybox.h"
#include <mntent.h>

/* Needed for nfs support only... */
#include <syslog.h>
#include <sys/utsname.h>
#undef TRUE
#undef FALSE
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>


#if defined(__dietlibc__)
/* 16.12.2006, Sampo Kellomaki (sampo@iki.fi)
 * dietlibc-0.30 does not have implementation of getmntent_r() */
/* OTOH: why we use getmntent_r instead of getmntent? TODO... */
struct mntent *getmntent_r(FILE* stream, struct mntent* result, char* buffer, int bufsize)
{
	/* *** XXX FIXME WARNING: This hack is NOT thread safe. --Sampo */
	struct mntent* ment = getmntent(stream);
	memcpy(result, ment, sizeof(struct mntent));
	return result;
}
#endif


// Not real flags, but we want to be able to check for this.
enum {
	MOUNT_USERS  = (1<<28)*ENABLE_DESKTOP,
	MOUNT_NOAUTO = (1<<29),
	MOUNT_SWAP   = (1<<30),
};
// TODO: more "user" flag compatibility.
// "user" option (from mount manpage):
// Only the user that mounted a filesystem can unmount it again.
// If any user should be able to unmount, then use users instead of user
// in the fstab line.  The owner option is similar to the user option,
// with the restriction that the user must be the owner of the special file.
// This may be useful e.g. for /dev/fd if a login script makes
// the console user owner of this device.

/* Standard mount options (from -o options or --options), with corresponding
 * flags */

struct {
	char *name;
	long flags;
} static mount_options[] = {
	// MS_FLAGS set a bit.  ~MS_FLAGS disable that bit.  0 flags are NOPs.

	USE_FEATURE_MOUNT_LOOP(
		{"loop", 0},
	)

	USE_FEATURE_MOUNT_FSTAB(
		{"defaults", 0},
		/* {"quiet", 0}, - do not filter out, vfat wants to see it */
		{"noauto", MOUNT_NOAUTO},
		{"swap", MOUNT_SWAP},
		USE_DESKTOP({"user",  MOUNT_USERS},)
		USE_DESKTOP({"users", MOUNT_USERS},)
	)

	USE_FEATURE_MOUNT_FLAGS(
		// vfs flags
		{"nosuid", MS_NOSUID},
		{"suid", ~MS_NOSUID},
		{"dev", ~MS_NODEV},
		{"nodev", MS_NODEV},
		{"exec", ~MS_NOEXEC},
		{"noexec", MS_NOEXEC},
		{"sync", MS_SYNCHRONOUS},
		{"async", ~MS_SYNCHRONOUS},
		{"atime", ~MS_NOATIME},
		{"noatime", MS_NOATIME},
		{"diratime", ~MS_NODIRATIME},
		{"nodiratime", MS_NODIRATIME},
		{"loud", ~MS_SILENT},

		// action flags

		{"bind", MS_BIND},
		{"move", MS_MOVE},
		{"shared", MS_SHARED},
		{"slave", MS_SLAVE},
		{"private", MS_PRIVATE},
		{"unbindable", MS_UNBINDABLE},
		{"rshared", MS_SHARED|MS_RECURSIVE},
		{"rslave", MS_SLAVE|MS_RECURSIVE},
		{"rprivate", MS_SLAVE|MS_RECURSIVE},
		{"runbindable", MS_UNBINDABLE|MS_RECURSIVE},
	)

	// Always understood.

	{"ro", MS_RDONLY},        // vfs flag
	{"rw", ~MS_RDONLY},       // vfs flag
	{"remount", MS_REMOUNT},  // action flag
};

#define VECTOR_SIZE(v) (sizeof(v) / sizeof((v)[0]))

/* Append mount options to string */
static void append_mount_options(char **oldopts, char *newopts)
{
	if (*oldopts && **oldopts) {
		/* do not insert options which are already there */
		while (newopts[0]) {
			char *p;
			int len = strlen(newopts);
			p = strchr(newopts, ',');
			if (p) len = p - newopts;
			p = *oldopts;
			while (1) {
				if (!strncmp(p, newopts, len)
				 && (p[len]==',' || p[len]==0))
					goto skip;
				p = strchr(p,',');
				if(!p) break;
				p++;
			}
			p = xasprintf("%s,%.*s", *oldopts, len, newopts);
			free(*oldopts);
			*oldopts = p;
skip:
			newopts += len;
			while (newopts[0] == ',') newopts++;
		}
	} else {
		if (ENABLE_FEATURE_CLEAN_UP) free(*oldopts);
		*oldopts = xstrdup(newopts);
	}
}

/* Use the mount_options list to parse options into flags.
 * Also return list of unrecognized options if unrecognized!=NULL */
static int parse_mount_options(char *options, char **unrecognized)
{
	int flags = MS_SILENT;

	// Loop through options
	for (;;) {
		int i;
		char *comma = strchr(options, ',');

		if (comma) *comma = 0;

		// Find this option in mount_options
		for (i = 0; i < VECTOR_SIZE(mount_options); i++) {
			if (!strcasecmp(mount_options[i].name, options)) {
				long fl = mount_options[i].flags;
				if (fl < 0) flags &= fl;
				else flags |= fl;
				break;
			}
		}
		// If unrecognized not NULL, append unrecognized mount options */
		if (unrecognized && i == VECTOR_SIZE(mount_options)) {
			// Add it to strflags, to pass on to kernel
			i = *unrecognized ? strlen(*unrecognized) : 0;
			*unrecognized = xrealloc(*unrecognized, i+strlen(options)+2);

			// Comma separated if it's not the first one
			if (i) (*unrecognized)[i++] = ',';
			strcpy((*unrecognized)+i, options);
		}

		// Advance to next option, or finish
		if (comma) {
			*comma = ',';
			options = ++comma;
		} else break;
	}

	return flags;
}

// Return a list of all block device backed filesystems

static llist_t *get_block_backed_filesystems(void)
{
	static const char *const filesystems[] = {
		"/etc/filesystems",
		"/proc/filesystems",
		0
	};
	char *fs, *buf;
	llist_t *list = 0;
	int i;
	FILE *f;

	for (i = 0; filesystems[i]; i++) {
		f = fopen(filesystems[i], "r");
		if (!f) continue;

		while ((buf = xmalloc_getline(f)) != 0) {
			if (!strncmp(buf, "nodev", 5) && isspace(buf[5]))
				continue;
			fs = skip_whitespace(buf);
			if (*fs=='#' || *fs=='*' || !*fs) continue;

			llist_add_to_end(&list, xstrdup(fs));
			free(buf);
		}
		if (ENABLE_FEATURE_CLEAN_UP) fclose(f);
	}

	return list;
}

llist_t *fslist = 0;

#if ENABLE_FEATURE_CLEAN_UP
static void delete_block_backed_filesystems(void)
{
	llist_free(fslist, free);
}
#else
void delete_block_backed_filesystems(void);
#endif

#if ENABLE_FEATURE_MTAB_SUPPORT
static int useMtab = 1;
static int fakeIt;
#else
#define useMtab 0
#define fakeIt 0
#endif

// Perform actual mount of specific filesystem at specific location.
// NB: mp->xxx fields may be trashed on exit
static int mount_it_now(struct mntent *mp, int vfsflags, char *filteropts)
{
	int rc = 0;

	if (fakeIt) goto mtab;

	// Mount, with fallback to read-only if necessary.

	for (;;) {
		rc = mount(mp->mnt_fsname, mp->mnt_dir, mp->mnt_type,
				vfsflags, filteropts);
		if (!rc || (vfsflags&MS_RDONLY) || (errno!=EACCES && errno!=EROFS))
			break;
		bb_error_msg("%s is write-protected, mounting read-only",
				mp->mnt_fsname);
		vfsflags |= MS_RDONLY;
	}

	// Abort entirely if permission denied.

	if (rc && errno == EPERM)
		bb_error_msg_and_die(bb_msg_perm_denied_are_you_root);

	/* If the mount was successful, and we're maintaining an old-style
	 * mtab file by hand, add the new entry to it now. */
 mtab:
	if (ENABLE_FEATURE_MTAB_SUPPORT && useMtab && !rc && !(vfsflags & MS_REMOUNT)) {
		char *fsname;
		FILE *mountTable = setmntent(bb_path_mtab_file, "a+");
		int i;

		if (!mountTable) {
			bb_error_msg("no %s",bb_path_mtab_file);
			goto ret;
		}

		// Add vfs string flags

		for (i=0; mount_options[i].flags != MS_REMOUNT; i++)
			if (mount_options[i].flags > 0 && (mount_options[i].flags & vfsflags))
				append_mount_options(&(mp->mnt_opts), mount_options[i].name);

		// Remove trailing / (if any) from directory we mounted on

		i = strlen(mp->mnt_dir) - 1;
		if (i > 0 && mp->mnt_dir[i] == '/') mp->mnt_dir[i] = 0;

		// Convert to canonical pathnames as needed

		mp->mnt_dir = bb_simplify_path(mp->mnt_dir);
		fsname = 0;
		if (!mp->mnt_type || !*mp->mnt_type) { /* bind mount */
			mp->mnt_fsname = fsname = bb_simplify_path(mp->mnt_fsname);
			mp->mnt_type = "bind";
		}
		mp->mnt_freq = mp->mnt_passno = 0;

		// Write and close.

		addmntent(mountTable, mp);
		endmntent(mountTable);
		if (ENABLE_FEATURE_CLEAN_UP) {
			free(mp->mnt_dir);
			free(fsname);
		}
	}
 ret:
	return rc;
}

#if ENABLE_FEATURE_MOUNT_NFS

/*
 * Linux NFS mount
 * Copyright (C) 1993 Rick Sladkey <jrs@world.std.com>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
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

/* This is just a warning of a common mistake.  Possibly this should be a
 * uclibc faq entry rather than in busybox... */
#if defined(__UCLIBC__) && ! defined(__UCLIBC_HAS_RPC__)
#error "You need to build uClibc with UCLIBC_HAS_RPC for NFS support."
#endif

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
	uint8_t pc_vdisable;
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

enum {
#ifndef NFS_FHSIZE
	NFS_FHSIZE = 32,
#endif
#ifndef NFS_PORT
	NFS_PORT = 2049
#endif
};

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
	static char buf[sizeof("unknown nfs status return value: ") + sizeof(int)*3];

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

static int daemonize(void)
{
	int fd;
	int pid = fork();
	if (pid < 0) /* error */
		return -errno;
	if (pid > 0) /* parent */
		return 0;
	/* child */
	fd = xopen(bb_dev_null, O_RDWR);
	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);
	while (fd > 2) close(fd--);
	setsid();
	openlog(applet_name, LOG_PID, LOG_DAEMON);
	logmode = LOGMODE_SYSLOG;
	return 1;
}

// TODO
static inline int we_saw_this_host_before(const char *hostname)
{
	return 0;
}

/* RPC strerror analogs are terminally idiotic:
 * *mandatory* prefix and \n at end.
 * This hopefully helps. Usage:
 * error_msg_rpc(clnt_*error*(" ")) */
static void error_msg_rpc(const char *msg)
{
	int len;
	while (msg[0] == ' ' || msg[0] == ':') msg++;
	len = strlen(msg);
	while (len && msg[len-1] == '\n') len--;
	bb_error_msg("%.*s", len, msg);
}

// NB: mp->xxx fields may be trashed on exit
static int nfsmount(struct mntent *mp, int vfsflags, char *filteropts)
{
	CLIENT *mclient;
	char *hostname;
	char *pathname;
	char *mounthost;
	struct nfs_mount_data data;
	char *opt;
	struct hostent *hp;
	struct sockaddr_in server_addr;
	struct sockaddr_in mount_server_addr;
	int msock, fsock;
	union {
		struct fhstatus nfsv2;
		struct mountres3 nfsv3;
	} status;
	int daemonized;
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

	find_kernel_nfs_mount_version();

	daemonized = 0;
	mounthost = NULL;
	retval = ETIMEDOUT;
	msock = fsock = -1;
	mclient = NULL;

	/* NB: hostname, mounthost, filteropts must be free()d prior to return */

	filteropts = xstrdup(filteropts); /* going to trash it later... */

	hostname = xstrdup(mp->mnt_fsname);
	/* mount_main() guarantees that ':' is there */
	s = strchr(hostname, ':');
	pathname = s + 1;
	*s = '\0';
	/* Ignore all but first hostname in replicated mounts
	   until they can be fully supported. (mack@sgi.com) */
	s = strchr(hostname, ',');
	if (s) {
		*s = '\0';
		bb_error_msg("warning: multiple hostnames not supported");
	}

	server_addr.sin_family = AF_INET;
	if (!inet_aton(hostname, &server_addr.sin_addr)) {
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

	if (!mp->mnt_opts) { /* TODO: actually mp->mnt_opts is never NULL */
		mp->mnt_opts = xasprintf("addr=%s", inet_ntoa(server_addr.sin_addr));
	} else {
		char *tmp = xasprintf("%s%saddr=%s", mp->mnt_opts,
					mp->mnt_opts[0] ? "," : "",
					inet_ntoa(server_addr.sin_addr));
		free(mp->mnt_opts);
		mp->mnt_opts = tmp;
	}

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

	for (opt = strtok(filteropts, ","); opt; opt = strtok(NULL, ",")) {
		char *opteq = strchr(opt, '=');
		if (opteq) {
			const char *const options[] = {
				/* 0 */ "rsize",
				/* 1 */ "wsize",
				/* 2 */ "timeo",
				/* 3 */ "retrans",
				/* 4 */ "acregmin",
				/* 5 */ "acregmax",
				/* 6 */ "acdirmin",
				/* 7 */ "acdirmax",
				/* 8 */ "actimeo",
				/* 9 */ "retry",
				/* 10 */ "port",
				/* 11 */ "mountport",
				/* 12 */ "mounthost",
				/* 13 */ "mountprog",
				/* 14 */ "mountvers",
				/* 15 */ "nfsprog",
				/* 16 */ "nfsvers",
				/* 17 */ "vers",
				/* 18 */ "proto",
				/* 19 */ "namlen",
				/* 20 */ "addr",
				NULL
			};
			int val = xatoi_u(opteq + 1);
			*opteq = '\0';
			switch (index_in_str_array(options, opt)) {
			case 0: // "rsize"
				data.rsize = val;
				break;
			case 1: // "wsize"
				data.wsize = val;
				break;
			case 2: // "timeo"
				data.timeo = val;
				break;
			case 3: // "retrans"
				data.retrans = val;
				break;
			case 4: // "acregmin"
				data.acregmin = val;
				break;
			case 5: // "acregmax"
				data.acregmax = val;
				break;
			case 6: // "acdirmin"
				data.acdirmin = val;
				break;
			case 7: // "acdirmax"
				data.acdirmax = val;
				break;
			case 8: // "actimeo"
				data.acregmin = val;
				data.acregmax = val;
				data.acdirmin = val;
				data.acdirmax = val;
				break;
			case 9: // "retry"
				retry = val;
				break;
			case 10: // "port"
				port = val;
				break;
			case 11: // "mountport"
				mountport = val;
				break;
			case 12: // "mounthost"
				mounthost = xstrndup(opteq+1,
						strcspn(opteq+1," \t\n\r,"));
				break;
			case 13: // "mountprog"
				mountprog = val;
				break;
			case 14: // "mountvers"
				mountvers = val;
				break;
			case 15: // "nfsprog"
				nfsprog = val;
				break;
			case 16: // "nfsvers"
			case 17: // "vers"
				nfsvers = val;
				break;
			case 18: // "proto"
				if (!strncmp(opteq+1, "tcp", 3))
					tcp = 1;
				else if (!strncmp(opteq+1, "udp", 3))
					tcp = 0;
				else
					bb_error_msg("warning: unrecognized proto= option");
				break;
			case 19: // "namlen"
				if (nfs_mount_version >= 2)
					data.namlen = val;
				else
					bb_error_msg("warning: option namlen is not supported\n");
				break;
			case 20: // "addr" - ignore
				break;
			default:
				bb_error_msg("unknown nfs mount parameter: %s=%d", opt, val);
				goto fail;
			}
		}
		else {
			const char *const options[] = {
				"bg",
				"fg",
				"soft",
				"hard",
				"intr",
				"posix",
				"cto",
				"ac",
				"tcp",
				"udp",
				"lock",
				NULL
			};
			int val = 1;
			if (!strncmp(opt, "no", 2)) {
				val = 0;
				opt += 2;
			}
			switch (index_in_str_array(options, opt)) {
			case 0: // "bg"
				bg = val;
				break;
			case 1: // "fg"
				bg = !val;
				break;
			case 2: // "soft"
				soft = val;
				break;
			case 3: // "hard"
				soft = !val;
				break;
			case 4: // "intr"
				intr = val;
				break;
			case 5: // "posix"
				posix = val;
				break;
			case 6: // "cto"
				nocto = !val;
				break;
			case 7: // "ac"
				noac = !val;
				break;
			case 8: // "tcp"
				tcp = val;
				break;
			case 9: // "udp"
				tcp = !val;
				break;
			case 10: // "lock"
				if (nfs_mount_version >= 3)
					nolock = !val;
				else
					bb_error_msg("warning: option nolock is not supported");
				break;
			default:
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

	data.version = nfs_mount_version;

	if (vfsflags & MS_REMOUNT)
		goto do_mount;

	/*
	 * If the previous mount operation on the same host was
	 * backgrounded, and the "bg" for this mount is also set,
	 * give up immediately, to avoid the initial timeout.
	 */
	if (bg && we_saw_this_host_before(hostname)) {
		daemonized = daemonize(); /* parent or error */
		if (daemonized <= 0) { /* parent or error */
			retval = -daemonized;
			goto ret;
		}
	}

	/* create mount daemon client */
	/* See if the nfs host = mount host. */
	if (mounthost) {
		if (mounthost[0] >= '0' && mounthost[0] <= '9') {
			mount_server_addr.sin_family = AF_INET;
			mount_server_addr.sin_addr.s_addr = inet_addr(hostname);
		} else {
			hp = gethostbyname(mounthost);
			if (hp == NULL) {
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
	 * The following loop implements the mount retries. When the mount
	 * times out, and the "bg" option is set, we background ourself
	 * and continue trying.
	 *
	 * The case where the mount point is not present and the "bg"
	 * option is set, is treated as a timeout. This is done to
	 * support nested mounts.
	 *
	 * The "retry" count specified by the user is the number of
	 * minutes to retry before giving up.
	 */
	{
		struct timeval total_timeout;
		struct timeval retry_timeout;
		struct pmap* pm_mnt;
		time_t t;
		time_t prevt;
		time_t timeout;

		retry_timeout.tv_sec = 3;
		retry_timeout.tv_usec = 0;
		total_timeout.tv_sec = 20;
		total_timeout.tv_usec = 0;
		timeout = time(NULL) + 60 * retry;
		prevt = 0;
		t = 30;
retry:
		/* be careful not to use too many CPU cycles */
		if (t - prevt < 30)
			sleep(30);

		pm_mnt = get_mountport(&mount_server_addr,
				mountprog,
				mountvers,
				proto,
				mountport);
		nfsvers = (pm_mnt->pm_vers < 2) ? 2 : pm_mnt->pm_vers;

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
		if (!mclient) {
			if (!daemonized && prevt == 0)
				error_msg_rpc(clnt_spcreateerror(" "));
		} else {
			enum clnt_stat clnt_stat;
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
				goto prepare_kernel_data; /* we're done */
			if (errno != ECONNREFUSED) {
				error_msg_rpc(clnt_sperror(mclient, " "));
				goto fail;	/* don't retry */
			}
			/* Connection refused */
			if (!daemonized && prevt == 0) /* print just once */
				error_msg_rpc(clnt_sperror(mclient, " "));
			auth_destroy(mclient->cl_auth);
			clnt_destroy(mclient);
			mclient = 0;
			close(msock);
		}

		/* Timeout. We are going to retry... maybe */

		if (!bg)
			goto fail;
		if (!daemonized) {
			daemonized = daemonize();
			if (daemonized <= 0) { /* parent or error */
				retval = -daemonized;
				goto ret;
			}
		}
		prevt = t;
		t = time(NULL);
		if (t >= timeout)
			/* TODO error message */
			goto fail;

		goto retry;
	}

prepare_kernel_data:

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
		bb_perror_msg("nfs socket");
		goto fail;
	}
	if (bindresvport(fsock, 0) < 0) {
		bb_perror_msg("nfs bindresvport");
		goto fail;
	}
	if (port == 0) {
		server_addr.sin_port = PMAPPORT;
		port = pmap_getport(&server_addr, nfsprog, nfsvers,
					tcp ? IPPROTO_TCP : IPPROTO_UDP);
		if (port == 0)
			port = NFS_PORT;
	}
	server_addr.sin_port = htons(port);

	/* prepare data structure for kernel */

	data.fd = fsock;
	memcpy((char *) &data.addr, (char *) &server_addr, sizeof(data.addr));
	strncpy(data.hostname, hostname, sizeof(data.hostname));

	/* clean up */

	auth_destroy(mclient->cl_auth);
	clnt_destroy(mclient);
	close(msock);

	if (bg) {
		/* We must wait until mount directory is available */
		struct stat statbuf;
		int delay = 1;
		while (stat(mp->mnt_dir, &statbuf) == -1) {
			if (!daemonized) {
				daemonized = daemonize();
				if (daemonized <= 0) { /* parent or error */
					retval = -daemonized;
					goto ret;
				}
			}
			sleep(delay);	/* 1, 2, 4, 8, 16, 30, ... */
			delay *= 2;
			if (delay > 30)
				delay = 30;
		}
	}

do_mount: /* perform actual mount */

	mp->mnt_type = "nfs";
	retval = mount_it_now(mp, vfsflags, (char*)&data);
	goto ret;

fail:	/* abort */

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
	free(hostname);
	free(mounthost);
	free(filteropts);
	return retval;
}

#else /* !ENABLE_FEATURE_MOUNT_NFS */

/* Never called. Call should be optimized out. */
int nfsmount(struct mntent *mp, int vfsflags, char *filteropts);

#endif /* !ENABLE_FEATURE_MOUNT_NFS */

// Mount one directory.  Handles CIFS, NFS, loopback, autobind, and filesystem
// type detection.  Returns 0 for success, nonzero for failure.
// NB: mp->xxx fields may be trashed on exit
static int singlemount(struct mntent *mp, int ignore_busy)
{
	int rc = -1, vfsflags;
	char *loopFile = 0, *filteropts = 0;
	llist_t *fl = 0;
	struct stat st;

	vfsflags = parse_mount_options(mp->mnt_opts, &filteropts);

	// Treat fstype "auto" as unspecified.

	if (mp->mnt_type && !strcmp(mp->mnt_type,"auto")) mp->mnt_type = 0;

	// Might this be an CIFS filesystem?

	if (ENABLE_FEATURE_MOUNT_CIFS &&
		(!mp->mnt_type || !strcmp(mp->mnt_type,"cifs")) &&
		(mp->mnt_fsname[0]==mp->mnt_fsname[1] && (mp->mnt_fsname[0]=='/' || mp->mnt_fsname[0]=='\\')))
	{
		struct hostent *he;
		char ip[32], *s;

		rc = 1;
		// Replace '/' with '\' and verify that unc points to "//server/share".

		for (s = mp->mnt_fsname; *s; ++s)
			if (*s == '/') *s = '\\';

		// get server IP

		s = strrchr(mp->mnt_fsname, '\\');
		if (s == mp->mnt_fsname+1) goto report_error;
		*s = 0;
		he = gethostbyname(mp->mnt_fsname+2);
		*s = '\\';
		if (!he) goto report_error;

		// Insert ip=... option into string flags.  (NOTE: Add IPv6 support.)

		sprintf(ip, "ip=%d.%d.%d.%d", he->h_addr[0], he->h_addr[1],
				he->h_addr[2], he->h_addr[3]);
		parse_mount_options(ip, &filteropts);

		// compose new unc '\\server-ip\share'

		mp->mnt_fsname = xasprintf("\\\\%s%s", ip+3,
					strchr(mp->mnt_fsname+2,'\\'));

		// lock is required
		vfsflags |= MS_MANDLOCK;

		mp->mnt_type = "cifs";
		rc = mount_it_now(mp, vfsflags, filteropts);
		if (ENABLE_FEATURE_CLEAN_UP) free(mp->mnt_fsname);
		goto report_error;
	}

	// Might this be an NFS filesystem?

	if (ENABLE_FEATURE_MOUNT_NFS &&
		(!mp->mnt_type || !strcmp(mp->mnt_type,"nfs")) &&
		strchr(mp->mnt_fsname, ':') != NULL)
	{
		rc = nfsmount(mp, vfsflags, filteropts);
		goto report_error;
	}

	// Look at the file.  (Not found isn't a failure for remount, or for
	// a synthetic filesystem like proc or sysfs.)

	if (!lstat(mp->mnt_fsname, &st) && !(vfsflags & (MS_REMOUNT | MS_BIND | MS_MOVE)))
	{
		// Do we need to allocate a loopback device for it?

		if (ENABLE_FEATURE_MOUNT_LOOP && S_ISREG(st.st_mode)) {
			loopFile = bb_simplify_path(mp->mnt_fsname);
			mp->mnt_fsname = 0;
			switch (set_loop(&(mp->mnt_fsname), loopFile, 0)) {
			case 0:
			case 1:
				break;
			default:
				bb_error_msg( errno == EPERM || errno == EACCES
					? bb_msg_perm_denied_are_you_root
					: "cannot setup loop device");
				return errno;
			}

		// Autodetect bind mounts

		} else if (S_ISDIR(st.st_mode) && !mp->mnt_type)
			vfsflags |= MS_BIND;
	}

	/* If we know the fstype (or don't need to), jump straight
	 * to the actual mount. */

	if (mp->mnt_type || (vfsflags & (MS_REMOUNT | MS_BIND | MS_MOVE)))
		rc = mount_it_now(mp, vfsflags, filteropts);

	// Loop through filesystem types until mount succeeds or we run out

	else {

		/* Initialize list of block backed filesystems.  This has to be
		 * done here so that during "mount -a", mounts after /proc shows up
		 * can autodetect. */

		if (!fslist) {
			fslist = get_block_backed_filesystems();
			if (ENABLE_FEATURE_CLEAN_UP && fslist)
				atexit(delete_block_backed_filesystems);
		}

		for (fl = fslist; fl; fl = fl->link) {
			mp->mnt_type = fl->data;
			rc = mount_it_now(mp, vfsflags, filteropts);
			if (!rc) break;
		}
	}

	// If mount failed, clean up loop file (if any).

	if (ENABLE_FEATURE_MOUNT_LOOP && rc && loopFile) {
		del_loop(mp->mnt_fsname);
		if (ENABLE_FEATURE_CLEAN_UP) {
			free(loopFile);
			free(mp->mnt_fsname);
		}
	}

report_error:
	if (ENABLE_FEATURE_CLEAN_UP) free(filteropts);

	if (rc && errno == EBUSY && ignore_busy) rc = 0;
	if (rc < 0)
		/* perror here sometimes says "mounting ... on ... failed: Success" */
		bb_error_msg("mounting %s on %s failed", mp->mnt_fsname, mp->mnt_dir);

	return rc;
}

// Parse options, if necessary parse fstab/mtab, and call singlemount for
// each directory to be mounted.

const char must_be_root[] = "you must be root";

int mount_main(int argc, char **argv)
{
	enum { OPT_ALL = 0x10 };

	char *cmdopts = xstrdup(""), *fstype=0, *storage_path=0;
	char *opt_o;
	const char *fstabname;
	FILE *fstab;
	int i, j, rc = 0;
	unsigned opt;
	struct mntent mtpair[2], *mtcur = mtpair;
	SKIP_DESKTOP(const int nonroot = 0;)
	USE_DESKTOP( int nonroot = (getuid() != 0);)

	/* parse long options, like --bind and --move.  Note that -o option
	 * and --option are synonymous.  Yes, this means --remount,rw works. */

	for (i = j = 0; i < argc; i++) {
		if (argv[i][0] == '-' && argv[i][1] == '-') {
			append_mount_options(&cmdopts, argv[i]+2);
		} else argv[j++] = argv[i];
	}
	argv[j] = 0;
	argc = j;

	// Parse remaining options

	opt = getopt32(argc, argv, "o:t:rwanfvs", &opt_o, &fstype);
	if (opt & 0x1) append_mount_options(&cmdopts, opt_o); // -o
	//if (opt & 0x2) // -t
	if (opt & 0x4) append_mount_options(&cmdopts, "ro"); // -r
	if (opt & 0x8) append_mount_options(&cmdopts, "rw"); // -w
	//if (opt & 0x10) // -a
	if (opt & 0x20) USE_FEATURE_MTAB_SUPPORT(useMtab = 0); // -n
	if (opt & 0x40) USE_FEATURE_MTAB_SUPPORT(fakeIt = 1); // -f
	//if (opt & 0x80) // -v: verbose (ignore)
	//if (opt & 0x100) // -s: sloppy (ignore)
	argv += optind;
	argc -= optind;

	// Three or more non-option arguments?  Die with a usage message.

	if (argc > 2) bb_show_usage();

	// If we have no arguments, show currently mounted filesystems

	if (!argc) {
		if (!(opt & OPT_ALL)) {
			FILE *mountTable = setmntent(bb_path_mtab_file, "r");

			if (!mountTable) bb_error_msg_and_die("no %s", bb_path_mtab_file);

			while (getmntent_r(mountTable, mtpair, bb_common_bufsiz1,
								sizeof(bb_common_bufsiz1)))
			{
				// Don't show rootfs. FIXME: why??
				// util-linux 2.12a happily shows rootfs...
				//if (!strcmp(mtpair->mnt_fsname, "rootfs")) continue;

				if (!fstype || !strcmp(mtpair->mnt_type, fstype))
					printf("%s on %s type %s (%s)\n", mtpair->mnt_fsname,
							mtpair->mnt_dir, mtpair->mnt_type,
							mtpair->mnt_opts);
			}
			if (ENABLE_FEATURE_CLEAN_UP) endmntent(mountTable);
			return EXIT_SUCCESS;
		}
	} else storage_path = bb_simplify_path(argv[0]);

	// When we have two arguments, the second is the directory and we can
	// skip looking at fstab entirely.  We can always abspath() the directory
	// argument when we get it.

	if (argc == 2) {
		if (nonroot)
			bb_error_msg_and_die(must_be_root);
		mtpair->mnt_fsname = argv[0];
		mtpair->mnt_dir = argv[1];
		mtpair->mnt_type = fstype;
		mtpair->mnt_opts = cmdopts;
		rc = singlemount(mtpair, 0);
		goto clean_up;
	}

	i = parse_mount_options(cmdopts, 0);
	if (nonroot && (i & ~MS_SILENT)) // Non-root users cannot specify flags
		bb_error_msg_and_die(must_be_root);

	// If we have a shared subtree flag, don't worry about fstab or mtab.

	if (ENABLE_FEATURE_MOUNT_FLAGS &&
			(i & (MS_SHARED | MS_PRIVATE | MS_SLAVE | MS_UNBINDABLE)))
	{
		rc = mount("", argv[0], "", i, "");
		if (rc) bb_perror_msg_and_die("%s", argv[0]);
		goto clean_up;
	}

	// Open either fstab or mtab

	fstabname = "/etc/fstab";
	if (i & MS_REMOUNT) {
		fstabname = bb_path_mtab_file;
	}
	fstab = setmntent(fstabname, "r");
	if (!fstab)
		bb_perror_msg_and_die("cannot read %s", fstabname);

	// Loop through entries until we find what we're looking for.

	memset(mtpair, 0, sizeof(mtpair));
	for (;;) {
		struct mntent *mtnext = (mtcur==mtpair ? mtpair+1 : mtpair);

		// Get next fstab entry

		if (!getmntent_r(fstab, mtcur, bb_common_bufsiz1
					+ (mtcur==mtpair ? sizeof(bb_common_bufsiz1)/2 : 0),
				sizeof(bb_common_bufsiz1)/2))
		{
			// Were we looking for something specific?

			if (argc) {

				// If we didn't find anything, complain.

				if (!mtnext->mnt_fsname)
					bb_error_msg_and_die("can't find %s in %s",
						argv[0], fstabname);

				mtcur = mtnext;
				if (nonroot) {
					// fstab must have "users" or "user"
					if (!(parse_mount_options(mtcur->mnt_opts, 0) & MOUNT_USERS))
						bb_error_msg_and_die(must_be_root);
				}

				// Mount the last thing we found.

				mtcur->mnt_opts = xstrdup(mtcur->mnt_opts);
				append_mount_options(&(mtcur->mnt_opts), cmdopts);
				rc = singlemount(mtcur, 0);
				free(mtcur->mnt_opts);
			}
			goto clean_up;
		}

		/* If we're trying to mount something specific and this isn't it,
		 * skip it.  Note we must match both the exact text in fstab (ala
		 * "proc") or a full path from root */

		if (argc) {

			// Is this what we're looking for?

			if (strcmp(argv[0], mtcur->mnt_fsname) &&
			   strcmp(storage_path, mtcur->mnt_fsname) &&
			   strcmp(argv[0], mtcur->mnt_dir) &&
			   strcmp(storage_path, mtcur->mnt_dir)) continue;

			// Remember this entry.  Something later may have overmounted
			// it, and we want the _last_ match.

			mtcur = mtnext;

		// If we're mounting all.

		} else {
			// Do we need to match a filesystem type?
			// TODO: support "-t type1,type2"; "-t notype1,type2"

			if (fstype && strcmp(mtcur->mnt_type, fstype)) continue;

			// Skip noauto and swap anyway.

			if (parse_mount_options(mtcur->mnt_opts, 0)
				& (MOUNT_NOAUTO | MOUNT_SWAP)) continue;

			// No, mount -a won't mount anything,
			// even user mounts, for mere humans.

			if (nonroot)
				bb_error_msg_and_die(must_be_root);

			// Mount this thing.

			// NFS mounts want this to be xrealloc-able
			mtcur->mnt_opts = xstrdup(mtcur->mnt_opts);
			if (singlemount(mtcur, 1)) {
				/* Count number of failed mounts */
				rc++;
			}
			free(mtcur->mnt_opts);
		}
	}
	if (ENABLE_FEATURE_CLEAN_UP) endmntent(fstab);

clean_up:

	if (ENABLE_FEATURE_CLEAN_UP) {
		free(storage_path);
		free(cmdopts);
	}

	return rc;
}
