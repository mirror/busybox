/*
	devfsd implementation for busybox

	Copyright (C) 2003 by Tito Ragusa <farmatito@tiscali.it>

	Busybox version is based on some previous work and ideas
	Copyright (C) [2003] by [Matteo Croce] <3297627799@wind.it>

	devfsd.c

	Main file for  devfsd  (devfs daemon for Linux).

    Copyright (C) 1998-2002  Richard Gooch

	devfsd.h

    Header file for  devfsd  (devfs daemon for Linux).

    Copyright (C) 1998-2000  Richard Gooch

	compat_name.c

    Compatibility name file for  devfsd  (build compatibility names).

    Copyright (C) 1998-2002  Richard Gooch

	expression.c

    This code provides Borne Shell-like expression expansion.

    Copyright (C) 1997-1999  Richard Gooch

	This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    Richard Gooch may be reached by email at  rgooch@atnf.csiro.au
    The postal address is:
      Richard Gooch, c/o ATNF, P. O. Box 76, Epping, N.S.W., 2121, Australia.
*/

#include "libbb.h"
#include "busybox.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dirent.h>
#include <fcntl.h>
#include <syslog.h>
#include <signal.h>
#include <regex.h>
#include <errno.h>
#include <sys/sysmacros.h>


/* Various defines taken from linux/major.h */
#define IDE0_MAJOR	3
#define IDE1_MAJOR	22
#define IDE2_MAJOR	33
#define IDE3_MAJOR	34
#define IDE4_MAJOR	56
#define IDE5_MAJOR	57
#define IDE6_MAJOR	88
#define IDE7_MAJOR	89
#define IDE8_MAJOR	90
#define IDE9_MAJOR	91


/* Various defines taken from linux/devfs_fs.h */
#define DEVFSD_PROTOCOL_REVISION_KERNEL  5
#define	DEVFSD_IOCTL_BASE	'd'
/*  These are the various ioctls  */
#define DEVFSDIOC_GET_PROTO_REV         _IOR(DEVFSD_IOCTL_BASE, 0, int)
#define DEVFSDIOC_SET_EVENT_MASK        _IOW(DEVFSD_IOCTL_BASE, 2, int)
#define DEVFSDIOC_RELEASE_EVENT_QUEUE   _IOW(DEVFSD_IOCTL_BASE, 3, int)
#define DEVFSDIOC_SET_DEBUG_MASK        _IOW(DEVFSD_IOCTL_BASE, 4, int)
#define DEVFSD_NOTIFY_REGISTERED    0
#define DEVFSD_NOTIFY_UNREGISTERED  1
#define DEVFSD_NOTIFY_ASYNC_OPEN    2
#define DEVFSD_NOTIFY_CLOSE         3
#define DEVFSD_NOTIFY_LOOKUP        4
#define DEVFSD_NOTIFY_CHANGE        5
#define DEVFSD_NOTIFY_CREATE        6
#define DEVFSD_NOTIFY_DELETE        7
#define DEVFS_PATHLEN               1024  /*  Never change this otherwise the
					      binary interface will change   */
struct devfsd_notify_struct
{   /*  Use native C types to ensure same types in kernel and user space     */
    unsigned int type;           /*  DEVFSD_NOTIFY_* value                   */
    unsigned int mode;           /*  Mode of the inode or device entry       */
    unsigned int major;          /*  Major number of device entry            */
    unsigned int minor;          /*  Minor number of device entry            */
    unsigned int uid;            /*  Uid of process, inode or device entry   */
    unsigned int gid;            /*  Gid of process, inode or device entry   */
    unsigned int overrun_count;  /*  Number of lost events                   */
    unsigned int namelen;        /*  Number of characters not including '\0' */
    /*  The device name MUST come last                                       */
    char devname[DEVFS_PATHLEN]; /*  This will be '\0' terminated            */
};



/* These are now in Config.in */
/* define this if you want to have more output on stderr and syslog at the same time */
/*#define CONFIG_DEVFSD_VERBOSE*/
/* define this if you want to have the function names in output */
/*#define CONFIG_DEVFSD_DEBUG*/

#define BUFFER_SIZE 16384
#define DEVFSD_VERSION "1.3.25"
#define CONFIG_FILE  "/etc/devfsd.conf"
#define MAX_ARGS     (6 + 1)
#define MAX_SUBEXPR  10
#define STRING_LENGTH 255

/* for get_uid_gid() */
#define UID			0
#define GID			1

/* 	for msg_logger(), do_ioctl(),
	fork_and_execute() and xopendir(). */
# define DIE				1
# define NO_DIE			0

/*  Update only after changing code to reflect new protocol  */
#define DEVFSD_PROTOCOL_REVISION_DAEMON  5

/*  Compile-time check  */
#if DEVFSD_PROTOCOL_REVISION_KERNEL != DEVFSD_PROTOCOL_REVISION_DAEMON
#error protocol version mismatch. Update your kernel headers
#endif

#define AC_PERMISSIONS				0
#define AC_MODLOAD					1	/* not supported by busybox */
#define AC_EXECUTE					2
#define AC_MFUNCTION				3	/* not supported by busybox */
#define AC_CFUNCTION				4	/* not supported by busybox */
#define AC_COPY					5
#define AC_IGNORE					6
#define AC_MKOLDCOMPAT				7
#define AC_MKNEWCOMPAT				8
#define AC_RMOLDCOMPAT				9
#define AC_RMNEWCOMPAT				10
#define AC_RESTORE					11


struct permissions_type
{
    mode_t mode;
    uid_t uid;
    gid_t gid;
};

struct execute_type
{
    char *argv[MAX_ARGS + 1];  /*  argv[0] must always be the programme  */
};

struct copy_type
{
    const char *source;
    const char *destination;
};

struct action_type
{
    unsigned int what;
    unsigned int when;
};

struct config_entry_struct
{
    struct action_type action;
    regex_t preg;
    union
    {
	struct permissions_type permissions;
	struct execute_type execute;
	struct copy_type copy;
    }
    u;
    struct config_entry_struct *next;
};

struct get_variable_info
{
    const struct devfsd_notify_struct *info;
    const char *devname;
    char devpath[STRING_LENGTH];
};

static int st_expr_expand(char *, unsigned, const char *, const char *(*) (const char *, void *), void *);
static const char *get_old_name(const char *, unsigned, char *, unsigned, unsigned);
static int mksymlink (const char *oldpath, const char *newpath);
static void read_config_file (const char *path, int optional, unsigned long *event_mask);
static void process_config_line (const char *, unsigned long *);
static int  do_servicing (int, unsigned long);
static void service_name (const struct devfsd_notify_struct *);
static void action_permissions (const struct devfsd_notify_struct *, const struct config_entry_struct *);
static void action_execute (const struct devfsd_notify_struct *, const struct config_entry_struct *,
							const regmatch_t *, unsigned);
static void action_copy (const struct devfsd_notify_struct *, const struct config_entry_struct *,
						 const regmatch_t *, unsigned);
static void action_compat (const struct devfsd_notify_struct *, unsigned);
static void free_config (void);
static void do_restore (const char *, int);
static int copy_inode (const char *, const struct stat *, mode_t, const char *, const struct stat *);
static mode_t get_mode (const char *);
static void signal_handler (int);
static const char *get_variable (const char *, void *);
static void do_scan_and_service (const char *);
static int make_dir_tree (const char *);
static int expand_expression(char *, unsigned, const char *, const char *(*)(const char *, void *), void *,
							 const char *, const regmatch_t *, unsigned );
static void expand_regexp (char *, size_t, const char *, const char *, const regmatch_t *, unsigned );
static const char *expand_variable(	char *, unsigned, unsigned *, const char *,
									const char *(*) (const char *, void *), void * );
static const char *get_variable_v2(const char *, const char *(*) (const char *, void *), void *);
static char get_old_ide_name (unsigned , unsigned);
static char *write_old_sd_name (char *, unsigned, unsigned, char *);

/* busybox functions */
static void msg_logger(int die, int pri, const char * fmt, ... );
static void do_ioctl(int die, int fd, int request, unsigned long event_mask_flag);
static void fork_and_execute(int die, char *arg0, char **arg );
DIR * xopendir(int die, const char * dir_name);
static int get_uid_gid ( int, const char *);
static void safe_memcpy( char * dest, const char * src, int len);

/* Structs and vars */
static struct config_entry_struct *first_config = NULL;
static struct config_entry_struct *last_config = NULL;
static const char *mount_point = NULL;
static volatile int caught_signal = FALSE;
static volatile int caught_sighup = FALSE;
static struct initial_symlink_struct
{
    char *dest;
    char *name;
} initial_symlinks[] =
{
    {"/proc/self/fd", "fd"},
    {"fd/0", "stdin"},
    {"fd/1", "stdout"},
    {"fd/2", "stderr"},
    {NULL, NULL},
};

static struct event_type
{
    unsigned int type;        /*  The DEVFSD_NOTIFY_* value                  */
    const char *config_name;  /*  The name used in the config file           */
} event_types[] =
{
    {DEVFSD_NOTIFY_REGISTERED,   "REGISTER"},
    {DEVFSD_NOTIFY_UNREGISTERED, "UNREGISTER"},
    {DEVFSD_NOTIFY_ASYNC_OPEN,   "ASYNC_OPEN"},
    {DEVFSD_NOTIFY_CLOSE,        "CLOSE"},
    {DEVFSD_NOTIFY_LOOKUP,       "LOOKUP"},
    {DEVFSD_NOTIFY_CHANGE,       "CHANGE"},
    {DEVFSD_NOTIFY_CREATE,       "CREATE"},
    {DEVFSD_NOTIFY_DELETE,       "DELETE"},
    {0xffffffff,                 NULL}
};

/* busybox functions and messages */

extern void xregcomp(regex_t * preg, const char *regex, int cflags);

const char * const bb_msg_bad_config   		= "bad %s config file: %s\n";
const char * const bb_msg_proto_rev			= "protocol revision";
#ifdef CONFIG_DEVFSD_VERBOSE
const char * const bb_msg_small_buffer		= "buffer too small\n";
const char * const bb_msg_variable_not_found= "variable: %s not found\n";
#endif

static void msg_logger(int die, int pri, const char * fmt, ... )
{
	va_list ap;

	va_start(ap, fmt);
	if (access ("/dev/log", F_OK) == 0)
	{
		openlog(bb_applet_name, 0, LOG_DAEMON);
		vsyslog( pri , fmt , ap);
		closelog();
 	}
#ifndef CONFIG_DEVFSD_VERBOSE
	else
#endif
		bb_verror_msg(fmt, ap);
	va_end(ap);
	if(die==DIE)
		exit(EXIT_FAILURE);
}


static void do_ioctl(int die, int fd, int request, unsigned long event_mask_flag)
{
	if (ioctl (fd, request, event_mask_flag) != 0)
		msg_logger(die, LOG_ERR, "ioctl() failed: %m\n");
}

static void fork_and_execute(int die, char *arg0, char **arg )
{
	switch ( fork () )
	{
	case 0:
		/*  Child  */
		break;
	case -1:
		/*  Parent: Error  : die or return */
		msg_logger(die, LOG_ERR,(char *) bb_msg_memory_exhausted);
		return;
	default:
		/*  Parent : ok : return or exit  */
		if(arg0 != NULL)
		{
			wait (NULL);
			return;
		}
		exit (EXIT_SUCCESS);
	}
	 /* Child : if arg0 != NULL do execvp */
	if(arg0 != NULL )
	{
		execvp (arg0, arg);
		msg_logger(DIE, LOG_ERR, "execvp() failed: %s: %m\n", arg0);
	}
}


DIR * xopendir(int die, const char * dir_name)
{
	DIR *dp;

	if ( ( dp = opendir (dir_name) ) == NULL )
 		msg_logger( die, LOG_ERR, "opendir() failed: %s: %m\n", dir_name);
	/* if die == DIE not reached else return NULL */
	return dp;
}

static void safe_memcpy( char *dest, const char *src, int len)
{
	memcpy (dest , src , len );
	dest[len] = '\0';
}

/*  Public functions follow  */

int devfsd_main (int argc, char **argv)
{
	int print_version = FALSE;
	int do_daemon = TRUE;
	int no_polling = FALSE;
	int fd, proto_rev, count;
	unsigned long event_mask = 0;
	struct sigaction new_action;
	struct initial_symlink_struct *curr;

	if (argc < 2)
		bb_show_usage();

	for (count = 2; count < argc; ++count)
	{
		if (strcmp (argv[count], "-v") == 0)
			print_version = TRUE;
		else if (strcmp (argv[count], "-fg") == 0)
			do_daemon = FALSE;
		else if (strcmp (argv[count], "-np") == 0)
			no_polling = TRUE;
		else
			bb_show_usage();
	}

	mount_point = argv[1];

	if (chdir (mount_point) != 0)
		bb_error_msg_and_die( " %s: %m", mount_point);

	fd = bb_xopen (".devfsd", O_RDONLY);

	if (fcntl (fd, F_SETFD, FD_CLOEXEC) != 0)
		bb_error_msg( "FD_CLOEXEC");

	do_ioctl(DIE, fd, DEVFSDIOC_GET_PROTO_REV,(int )&proto_rev);

	/*setup initial entries */
    for (curr = initial_symlinks; curr->dest != NULL; ++curr)
		symlink (curr->dest, curr->name);

	/* NB: The check for CONFIG_FILE is done in read_config_file() */

	if ( print_version  || (DEVFSD_PROTOCOL_REVISION_DAEMON != proto_rev) )
	{
		bb_printf( "%s v%s\nDaemon %s:\t%d\nKernel-side %s:\t%d\n",
					 bb_applet_name,DEVFSD_VERSION,bb_msg_proto_rev,
					 DEVFSD_PROTOCOL_REVISION_DAEMON,bb_msg_proto_rev, proto_rev);
		if (DEVFSD_PROTOCOL_REVISION_DAEMON != proto_rev)
			bb_error_msg_and_die( "%s mismatch!",bb_msg_proto_rev);
		exit(EXIT_SUCCESS); /* -v */
	}
	/*  Tell kernel we are special (i.e. we get to see hidden entries)  */
	do_ioctl(DIE, fd, DEVFSDIOC_SET_EVENT_MASK, 0);

	sigemptyset (&new_action.sa_mask);
	new_action.sa_flags = 0;

	/*  Set up SIGHUP and SIGUSR1 handlers  */
	new_action.sa_handler = signal_handler;
	if (sigaction (SIGHUP, &new_action, NULL) != 0 || sigaction (SIGUSR1, &new_action, NULL) != 0 )
		bb_error_msg_and_die( "sigaction()");

	bb_printf("%s v%s  started for %s\n",bb_applet_name, DEVFSD_VERSION, mount_point);

	/*  Set umask so that mknod(2), open(2) and mkdir(2) have complete control 	over permissions  */
	umask (0);
	read_config_file (CONFIG_FILE, FALSE, &event_mask);
	/*  Do the scan before forking, so that boot scripts see the finished product  */
	do_scan_and_service (mount_point);
	if (no_polling)
		exit (0);
	if (do_daemon)
	{
		/*  Release so that the child can grab it  */
		do_ioctl(DIE, fd, DEVFSDIOC_RELEASE_EVENT_QUEUE, 0);
		fork_and_execute(DIE, NULL, NULL);
		setsid ();        /*  Prevent hangups and become pgrp leader         */
	}
	else
		setpgid (0, 0);  /*  Become process group leader                    */

	while (TRUE)
	{
		int do_scan = do_servicing (fd, event_mask);

		free_config ();
		read_config_file (CONFIG_FILE, FALSE, &event_mask);
		if (do_scan)
			do_scan_and_service (mount_point);
	}
}   /*  End Function main  */


/*  Private functions follow  */

static void read_config_file (const char *path, int optional, unsigned long *event_mask)
/*  [SUMMARY] Read a configuration database.
    <path> The path to read the database from. If this is a directory, all
    entries in that directory will be read (except hidden entries).
    <optional> If TRUE, the routine will silently ignore a missing config file.
    <event_mask> The event mask is written here. This is not initialised.
    [RETURNS] Nothing.
*/
{
	struct stat statbuf;
	FILE *fp;
	char buf[STRING_LENGTH];

#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "read_config_file()\n");
#endif

	if (stat (path, &statbuf) != 0 || statbuf.st_size == 0 )
		msg_logger((optional ==  0 )? DIE : NO_DIE, LOG_ERR, " %s: %m\n", path);

	if ( S_ISDIR (statbuf.st_mode) )
	{

		DIR *dp;
		struct dirent *de;

		dp = xopendir(DIE, path);

		while ( ( de = readdir (dp) ) != NULL )
		{
			char fname[STRING_LENGTH];

			if (de->d_name[0] == '.')
				continue;
			snprintf (fname, STRING_LENGTH, "%s/%s", path, de->d_name);
			read_config_file (fname, optional, event_mask);
		}
		closedir (dp);
		return;
	}

	if ( ( fp = fopen (path, "r") ) != NULL )
	{
		while (fgets (buf, STRING_LENGTH, fp) != NULL)
		{
			char *line;

			buf[strlen (buf) - 1] = '\0';
			/*  Skip whitespace  */
			for (line = buf; isspace (*line); ++line)
				/*VOID*/;
			if (line[0] == '\0' || line[0] == '#' )
				continue;
			process_config_line (line, event_mask);
		}
		fclose (fp);
#ifdef CONFIG_DEVFSD_VERBOSE
		msg_logger( NO_DIE, LOG_INFO, "read config file: %s\n", path);
#endif
		return;
	}
	msg_logger(( (optional == 0) && (errno == ENOENT) )? DIE : NO_DIE, LOG_ERR, " %s: %m\n", path);
}   /*  End Function read_config_file   */

static void process_config_line (const char *line, unsigned long *event_mask)
/*  [SUMMARY] Process a line from a configuration file.
    <line> The configuration line.
    <event_mask> The event mask is written here. This is not initialised.
    [RETURNS] Nothing.
*/
{
	int  num_args, count;
	struct config_entry_struct *new;
	char p[MAX_ARGS][STRING_LENGTH];
	char when[STRING_LENGTH], what[STRING_LENGTH];
	char name[STRING_LENGTH];
	char * msg="";
#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "process_config_line()\n");
#endif

	for (count = 0; count < MAX_ARGS; ++count) p[count][0] = '\0';
	num_args = sscanf (line, "%s %s %s %s %s %s %s %s %s %s",
			when, name, what,
			p[0], p[1], p[2], p[3], p[4], p[5], p[6]);
	if (strcasecmp (when, "CLEAR_CONFIG") == 0)
	{
		free_config ();
		*event_mask = 0;
		return;
	}
	if (num_args < 2)
		goto process_config_line_err;

	if ( (strcasecmp (when, "INCLUDE") == 0) ||
		 (strcasecmp (when, "OPTIONAL_INCLUDE") == 0) )
	{
		st_expr_expand (name, STRING_LENGTH, name, get_variable, NULL );
		read_config_file (name, (toupper (when[0]) == 'I') ? FALSE : TRUE, event_mask);
		return;
	}
	if (strcasecmp (when, "RESTORE") == 0)
	{
		do_restore ( name, strlen (name) );
		return;
	}
	if (num_args < 3)
		goto process_config_line_err;

	new = xmalloc (sizeof *new);
	memset (new, 0, sizeof *new);

	for (count = 0; event_types[count].config_name != NULL; ++count)
	{
		if (strcasecmp (when, event_types[count].config_name) != 0)
			continue;
		new->action.when = event_types[count].type;
		break;
	}
	if (event_types[count].config_name == NULL)
	{
		msg="WHEN in";
		goto process_config_line_err;
	}

	if (strcasecmp (what, "PERMISSIONS") == 0)
	{
		char *ptr;

		new->action.what = AC_PERMISSIONS;
		/*  Get user and group  */
		if ( ( ptr = strchr (p[0], '.') ) == NULL )
		{
			msg="UID.GID";
			goto process_config_line_err; /*"missing '.' in UID.GID */
		}

		*ptr++ = '\0';
		new->u.permissions.uid = get_uid_gid (UID, p[0]);
		new->u.permissions.gid = get_uid_gid (GID, ptr);
		/*  Get mode  */
		new->u.permissions.mode = get_mode (p[1]);
	}
	else if (strcasecmp (what, "EXECUTE") == 0)
	{
		new->action.what = AC_EXECUTE;
		num_args -= 3;

		for (count = 0; count < num_args; ++count)
			new->u.execute.argv[count] = bb_xstrdup (p[count]);

		new->u.execute.argv[num_args] = NULL;
	}
	else if (strcasecmp (what, "COPY") == 0)
	{
		new->action.what = AC_COPY;
		num_args -= 3;
		if (num_args != 2)
			goto process_config_line_err; /* missing path and function in line */

		new->u.copy.source = bb_xstrdup (p[0]);
		new->u.copy.destination = bb_xstrdup (p[1]);
	}
	else if (strcasecmp (what, "IGNORE") == 0)
		new->action.what = AC_IGNORE;
	else if (strcasecmp (what, "MKOLDCOMPAT") == 0)
		new->action.what = AC_MKOLDCOMPAT;
	else if (strcasecmp (what, "MKNEWCOMPAT") == 0)
		new->action.what = AC_MKNEWCOMPAT;
	else if (strcasecmp (what, "RMOLDCOMPAT") == 0)
		new->action.what = AC_RMOLDCOMPAT;
	else if (strcasecmp (what, "RMNEWCOMPAT") == 0)
		new->action.what = AC_RMNEWCOMPAT;
	else
	{
		msg ="WHAT in";
		goto process_config_line_err;
	}

	xregcomp( &new->preg, name, REG_EXTENDED);

	*event_mask |= 1 << new->action.when;
	new->next = NULL;
	if (first_config == NULL)
		first_config = new;
	else
		last_config->next = new;
	last_config = new;
	return;
process_config_line_err:
	msg_logger( DIE, LOG_ERR, bb_msg_bad_config, msg , line);
}  /*  End Function process_config_line   */

static int do_servicing (int fd, unsigned long event_mask)
/*  [SUMMARY] Service devfs changes until a signal is received.
    <fd> The open control file.
    <event_mask> The event mask.
    [RETURNS] TRUE if SIGHUP was caught, else FALSE.
*/
{
	ssize_t bytes;
	struct devfsd_notify_struct info;
	unsigned long tmp_event_mask;

#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "do_servicing()\n");
#endif
	/*  Tell devfs what events we care about  */
	tmp_event_mask = event_mask;
	/*  May need to trap inode creates to watch for syslogd(8) starting  */
	/*if (!syslog_is_open &&  !no_syslog)
	{
		tmp_event_mask |= 1 << DEVFSD_NOTIFY_CREATE; *//*FIXME  I'm not sure if this line is needed. TITO */
	/*}*/
	do_ioctl(DIE, fd, DEVFSDIOC_SET_EVENT_MASK, tmp_event_mask);
	while (!caught_signal)
	{
		errno = 0;
		bytes = read (fd, (char *) &info, sizeof info);
		if (caught_signal)
			break;      /*  Must test for this first     */
		if (errno == EINTR)
			continue;  /*  Yes, the order is important  */
		if (bytes < 1)
			break;
		/*  Special trap for "/dev/log" creation  */
		/*  Open syslog, now that "/dev/log" exists  */
		/*if (!syslog_is_open && !no_syslog &&
			 (info.type == DEVFSD_NOTIFY_CREATE) &&(strcmp (info.devname, "log") == 0) )
		{
			do_open_syslog ();
			do_ioctl(DIE, fd, DEVFSDIOC_SET_EVENT_MASK, event_mask);*/ /*FIXME I'm not sure if this line is needed. TITO */
		/*}*/
		service_name (&info);
	}
	if (caught_signal)
	{
		int c_sighup = caught_sighup;

		caught_signal = FALSE;
		caught_sighup = FALSE;
		return (c_sighup);
	}
#ifdef CONFIG_DEVFSD_VERBOSE
	msg_logger( NO_DIE, LOG_ERR, "read error on control file: %m\n");
#endif
	/* This is to shut up a compiler warning */
	exit(1);
}   /*  End Function do_servicing  */

static void service_name (const struct devfsd_notify_struct *info)
/*  [SUMMARY] Service a single devfs change.
    <info> The devfs change.
    [RETURNS] Nothing.
*/
{
	unsigned int n;
	regmatch_t mbuf[MAX_SUBEXPR];
	struct config_entry_struct *entry;

#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "service_name()\n");
#endif
#ifdef CONFIG_DEVFSD_VERBOSE
	if (info->overrun_count > 0)
		msg_logger( NO_DIE, LOG_ERR, "lost %u events\n", info->overrun_count);
#endif

	/*  Discard lookups on "/dev/log" and "/dev/initctl"  */
	if	(info->type == DEVFSD_NOTIFY_LOOKUP &&
		(( (strcmp (info->devname, "log") == 0) ||
		(strcmp (info->devname, "initctl") == 0))))
			return;
	for (entry = first_config; entry != NULL; entry = entry->next)
	{
		/*  First check if action matches the type, then check if name matches */
		if (info->type != entry->action.when || regexec (&entry->preg, info->devname, MAX_SUBEXPR, mbuf, 0) != 0 )
			continue;
		for (n = 0; (n < MAX_SUBEXPR) && (mbuf[n].rm_so != -1); ++n)
			/* VOID */;
		switch (entry->action.what)
		{
			case AC_PERMISSIONS:
#ifdef CONFIG_DEVFSD_DEBUG
				msg_logger( NO_DIE, LOG_INFO, "AC_PERMISSIONS\n");
#endif
				action_permissions (info, entry);
				break;
			case AC_EXECUTE:
#ifdef CONFIG_DEVFSD_DEBUG
				msg_logger( NO_DIE, LOG_INFO, "AC_EXECUTE\n");
#endif
				action_execute (info, entry, mbuf, n);
				break;
			case AC_COPY:
#ifdef CONFIG_DEVFSD_DEBUG
				msg_logger( NO_DIE, LOG_INFO, "AC_COPY\n");
#endif
				action_copy (info, entry, mbuf, n);
				break;
			case AC_IGNORE:
#ifdef CONFIG_DEVFSD_DEBUG
				msg_logger( NO_DIE, LOG_INFO, "AC_IGNORE\n");
#endif
				return;
				/*break;*/
			case AC_MKOLDCOMPAT:
			case AC_MKNEWCOMPAT:
			case AC_RMOLDCOMPAT:
			case AC_RMNEWCOMPAT:
#ifdef CONFIG_DEVFSD_DEBUG
				msg_logger( NO_DIE, LOG_INFO, "AC_COMPAT\n");
#endif
				action_compat (info, entry->action.what);
				break;
			default:
				msg_logger( DIE, LOG_ERR, "Unknown action\n");
				/*break;*/
		}
	}
}   /*  End Function service_name  */

static void action_permissions (const struct devfsd_notify_struct *info,
				const struct config_entry_struct *entry)
/*  [SUMMARY] Update permissions for a device entry.
    <info> The devfs change.
    <entry> The config file entry.
    [RETURNS] Nothing.
*/
{
	struct stat statbuf;

#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "action_permission()\n");
#endif

	if ( stat (info->devname, &statbuf) != 0	||
		 chmod (info->devname,(statbuf.st_mode & S_IFMT) | (entry->u.permissions.mode & ~S_IFMT)) != 0 ||
		 chown (info->devname, entry->u.permissions.uid, entry->u.permissions.gid) != 0)
	{
#ifdef CONFIG_DEVFSD_VERBOSE
			msg_logger( NO_DIE, LOG_ERR, "chmod() or chown() error for: %s: %m\n",info->devname);
#endif
		return;
	}

}   /*  End Function action_permissions  */

static void action_execute (const struct devfsd_notify_struct *info,
			    const struct config_entry_struct *entry,
			    const regmatch_t *regexpr, unsigned int numexpr)
/*  [SUMMARY] Execute a programme.
    <info> The devfs change.
    <entry> The config file entry.
    <regexpr> The number of subexpression (start, end) offsets within the
    device name.
    <numexpr> The number of elements within <<regexpr>>.
    [RETURNS] Nothing.
*/
{
	unsigned int count;
	struct get_variable_info gv_info;
	char *argv[MAX_ARGS + 1];
	char largv[MAX_ARGS + 1][STRING_LENGTH];

#ifdef CONFIG_DEVFSD_DEBUG
	int i;
	char buff[1024];

	msg_logger( NO_DIE, LOG_INFO, "action_execute()\n");
#endif

	gv_info.info = info;
	gv_info.devname = info->devname;
	snprintf (gv_info.devpath, sizeof (gv_info.devpath), "%s/%s", mount_point, info->devname);
	for (count = 0; entry->u.execute.argv[count] != NULL; ++count)
	{
		expand_expression (largv[count], STRING_LENGTH,
				entry->u.execute.argv[count],
				get_variable, &gv_info,
				gv_info.devname, regexpr, numexpr );
		argv[count] = largv[count];
	}
	argv[count] = NULL;

#ifdef CONFIG_DEVFSD_DEBUG
	buff[0]='\0';
	for(i=0;argv[i]!=NULL;i++) /* argv[i] < MAX_ARGS + 1 */
	{
		strcat(buff," ");
		if( (strlen(buff)+ 1 + strlen(argv[i])) >= 1024)
			break;
		strcat(buff,argv[i]);
	}
	strcat(buff,"\n");
	msg_logger( NO_DIE, LOG_INFO, "action_execute(): %s",buff);
#endif

	fork_and_execute(NO_DIE, argv[0], argv);
}   /*  End Function action_execute  */


static void action_copy (const struct devfsd_notify_struct *info,
			 const struct config_entry_struct *entry,
                         const regmatch_t *regexpr, unsigned int numexpr)
/*  [SUMMARY] Copy permissions.
    <info> The devfs change.
    <entry> The config file entry.
    <regexpr> This list of subexpression (start, end) offsets within the
    device name.
    <numexpr> The number of elements in <<regexpr>>.
    [RETURNS] Nothing.
*/
{
	mode_t new_mode;
	struct get_variable_info gv_info;
	struct stat source_stat, dest_stat;
	char source[STRING_LENGTH], destination[STRING_LENGTH];
	dest_stat.st_mode = 0;

#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "action_copy()\n");
#endif

	if ( (info->type == DEVFSD_NOTIFY_CHANGE) && S_ISLNK (info->mode) )
		return;
	gv_info.info = info;
	gv_info.devname = info->devname;
	snprintf (gv_info.devpath, sizeof (gv_info.devpath), "%s/%s", mount_point, info->devname);

	expand_expression (source, STRING_LENGTH, entry->u.copy.source,
				get_variable, &gv_info, gv_info.devname,
				regexpr, numexpr);

	expand_expression (destination, STRING_LENGTH, entry->u.copy.destination,
				get_variable, &gv_info, gv_info.devname,
				regexpr, numexpr);

	if ( !make_dir_tree (destination) || lstat (source, &source_stat) != 0)
		goto action_copy_error;

	lstat (destination, &dest_stat);
	new_mode = source_stat.st_mode & ~S_ISVTX;
	if (info->type == DEVFSD_NOTIFY_CREATE)
		new_mode |= S_ISVTX;
	else if ( (info->type == DEVFSD_NOTIFY_CHANGE) && (dest_stat.st_mode & S_ISVTX) )
		new_mode |= S_ISVTX;
#ifdef CONFIG_DEVFSD_VERBOSE
	if ( !copy_inode (destination, &dest_stat, new_mode, source, &source_stat) )
action_copy_error:
		msg_logger( NO_DIE, LOG_ERR, "error copying: %s to %s: %m\n", source, destination);
#else
	copy_inode (destination, &dest_stat, new_mode, source,&source_stat);
action_copy_error:
	return;
#endif
}   /*  End Function action_copy  */

static void action_compat (const struct devfsd_notify_struct *info,
			   unsigned int action)
/*  [SUMMARY] Process a compatibility request.
    <info> The devfs change.
    <action> The action to take.
    [RETURNS] Nothing.
*/
{
	const char *compat_name = NULL;
	const char *dest_name = info->devname;
	char *ptr;
	char compat_buf[STRING_LENGTH], dest_buf[STRING_LENGTH];

#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "action_compat()\n");
#endif

	/*  First construct compatibility name  */
	switch (action)
	{
		case AC_MKOLDCOMPAT:
		case AC_RMOLDCOMPAT:
			compat_name = get_old_name (info->devname, info->namelen, compat_buf, info->major, info->minor);
			break;
		case AC_MKNEWCOMPAT:
		case AC_RMNEWCOMPAT:
		if (strncmp (info->devname, "scsi/", 5) == 0)
		{
			int mode, host, bus, target, lun;

			sscanf (info->devname + 5, "host%d/bus%d/target%d/lun%d/",
				&host, &bus, &target, &lun);
			compat_name = compat_buf;
			snprintf (dest_buf, sizeof (dest_buf), "../%s", info->devname);
			dest_name = dest_buf;
			if (strncmp (ptr = (strrchr (info->devname, '/') + 1), "mt", 2)== 0)
			{
				char rewind_ = info->devname[info->namelen - 1];

				if (rewind_ != 'n')
					rewind_ = '\0';

				switch (ptr[2])
				{
					default:
						mode = 0;
						break;
					case 'l':
						mode = 1;
						break;
					case 'm':
						mode = 2;
						break;
					case 'a':
						mode = 3;
						break;
				}
				sprintf (compat_buf, "st/c%db%dt%du%dm%d%c", host, bus, target, lun, mode, rewind_);
			}
			else if (strcmp (info->devname + info->namelen - 7,"generic") == 0)
				sprintf (compat_buf, "sg/c%db%dt%du%d",
						host, bus, target, lun);
			else if (strcmp (info->devname + info->namelen - 2, "cd") == 0)
				sprintf (compat_buf, "sr/c%db%dt%du%d",
						host, bus, target, lun);
			else if (strcmp (info->devname + info->namelen - 4, "disc") == 0)
				sprintf (compat_buf, "sd/c%db%dt%du%d",
						host, bus, target, lun);
			else if (strncmp (ptr = (strrchr (info->devname, '/') + 1), "part", 4) == 0)
				sprintf ( compat_buf, "sd/c%db%dt%du%dp%d",
						host, bus, target, lun, atoi (ptr + 4) );
			else compat_name = NULL;
		}
		else if (strncmp (info->devname, "ide/host", 8) == 0)
		{
			int host, bus, target, lun;

			sscanf (info->devname + 4, "host%d/bus%d/target%d/lun%d/",
				&host, &bus, &target, &lun);
			compat_name = compat_buf;
			snprintf (dest_buf, sizeof (dest_buf), "../%s", info->devname + 4);
			dest_name = dest_buf;
			if (strcmp (info->devname + info->namelen - 4, "disc") == 0)
				sprintf (compat_buf, "ide/hd/c%db%dt%du%d",
					host, bus, target, lun);
			else if (strncmp (ptr = (strrchr (info->devname, '/') + 1), "part", 4) == 0)
				sprintf ( compat_buf, "ide/hd/c%db%dt%du%dp%d",
					host, bus, target, lun, atoi (ptr + 4) );
			else if (strcmp (info->devname + info->namelen - 2, "cd") == 0)
				sprintf (compat_buf, "ide/cd/c%db%dt%du%d",
					host, bus, target,lun);
			else if (strncmp (ptr = (strrchr (info->devname, '/') + 1), "mt", 2) == 0)
				snprintf (compat_buf, sizeof (compat_buf), "ide/mt/c%db%dt%du%d%s",
					host, bus, target, lun, ptr + 2);
			else compat_name = NULL;
		}
		break;
	}
	if (compat_name == NULL)
		return;
	/*  Now decide what to do with it  */
	switch (action)
	{
		case AC_MKOLDCOMPAT:
		case AC_MKNEWCOMPAT:
			mksymlink (dest_name, compat_name);
			break;
		case AC_RMOLDCOMPAT:
		case AC_RMNEWCOMPAT:
#ifdef CONFIG_DEVFSD_VERBOSE
			if (unlink (compat_name) != 0)
				msg_logger( NO_DIE, LOG_ERR, "unlink(): %s: %m\n", compat_name);
#else
			unlink (compat_name);
#endif
			break;
	}
}   /*  End Function action_compat  */

static void do_restore (const char *dir_name, int rootlen)
/*  [SUMMARY] Restore state from a directory.
    <dir_name> The directory containing the backing store.
    <rootlen> The length of the root of the state directory hierarchy.
    [RETURNS] Nothing.
*/
{
	DIR *dp;
	struct dirent *de;

#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "do_restore()\n");
#endif

	if( (dp = xopendir(NO_DIE, dir_name))== NULL)
		return;

	while ( (de = readdir (dp) ) != NULL )
	{
		char spath[STRING_LENGTH], dpath[STRING_LENGTH];

		struct stat source_stat, dest_stat;
		dest_stat.st_mode = 0;

		if ( (strcmp (de->d_name, ".") == 0) || (strcmp (de->d_name, "..") == 0) )
			continue;

		snprintf (spath, sizeof spath, "%s/%s", dir_name, de->d_name);

		if (lstat (spath, &source_stat) != 0)
		{
#ifdef CONFIG_DEVFSD_VERBOSE
			msg_logger( NO_DIE, LOG_ERR, "%s: %m\n", spath);
#endif
			continue;
		}
		snprintf (dpath, sizeof dpath, "%s%s", mount_point, spath + rootlen);
		lstat (dpath, &dest_stat);

		if ( S_ISLNK (source_stat.st_mode) || (source_stat.st_mode & S_ISVTX) )
			copy_inode (dpath, &dest_stat, (source_stat.st_mode & ~S_ISVTX) , spath, &source_stat);

		if ( S_ISDIR (source_stat.st_mode) )
			do_restore (spath, rootlen);
	}
	closedir (dp);
}   /*  End Function do_restore  */

static int copy_inode (const char *destpath, const struct stat *dest_stat,
			mode_t new_mode,
			const char *sourcepath, const struct stat *source_stat)
/*  [SUMMARY] Copy an inode.
    <destpath> The destination path. An existing inode may be deleted.
    <dest_stat> The destination stat(2) information.
    <new_mode> The desired new mode for the destination.
    <sourcepath> The source path.
    <source_stat> The source stat(2) information.
    [RETURNS] TRUE on success, else FALSE.
*/
{

#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "copy_inode()\n");
#endif

	if ( (source_stat->st_mode & S_IFMT) == (dest_stat->st_mode & S_IFMT) )
	{
		/*  Same type  */
		if ( S_ISLNK (source_stat->st_mode) )
		{
			int source_len, dest_len;
			char source_link[STRING_LENGTH], dest_link[STRING_LENGTH];


			if (( source_len = readlink (sourcepath, source_link, STRING_LENGTH - 1) ) < 0 ||
				( dest_len   = readlink (destpath  , dest_link  , STRING_LENGTH - 1) ) < 0 )
				return (FALSE);
			source_link[source_len]	= '\0';
			dest_link[dest_len] 	= '\0';
			if ( (source_len != dest_len) || (strcmp (source_link, dest_link) != 0) )
			{
				unlink (destpath);
				symlink (source_link, destpath);
			}
			return (TRUE);
		}   /*  Else not a symlink  */
		chmod (destpath, new_mode & ~S_IFMT);
		chown (destpath, source_stat->st_uid, source_stat->st_gid);
		return (TRUE);
	}
	/*  Different types: unlink and create  */
	unlink (destpath);
	switch (source_stat->st_mode & S_IFMT)
	{
		int fd, val;
		struct sockaddr_un un_addr;
		char symlink_val[STRING_LENGTH];

		case S_IFSOCK:
			if ( ( fd = socket (AF_UNIX, SOCK_STREAM, 0) ) < 0 )
				break;
			un_addr.sun_family = AF_UNIX;
			snprintf (un_addr.sun_path, sizeof (un_addr.sun_path), "%s", destpath);
			val = bind (fd, (struct sockaddr *) &un_addr, (int) sizeof un_addr);
			close (fd);
			if (val != 0 || chmod (destpath, new_mode & ~S_IFMT) != 0)
				break;
			if (chown (destpath, source_stat->st_uid, source_stat->st_gid) == 0)
				return (TRUE);
			break;
		case S_IFLNK:
			if ( ( val = readlink (sourcepath, symlink_val, STRING_LENGTH - 1) ) < 0 )
				break;
			symlink_val[val] = '\0';
			if (symlink (symlink_val, destpath) == 0)
				return (TRUE);
			break;
		case S_IFREG:
			if ( ( fd = open (destpath, O_RDONLY | O_CREAT, new_mode & ~S_IFMT) ) < 0 )
				break;
			close (fd);
			if (chmod (destpath, new_mode & ~S_IFMT) != 0)
				break;
			if (chown (destpath, source_stat->st_uid, source_stat->st_gid) == 0)
				return (TRUE);
			break;
		case S_IFBLK:
		case S_IFCHR:
		case S_IFIFO:
			if (mknod (destpath, new_mode, source_stat->st_rdev) != 0)
				break;
			if (chown (destpath, source_stat->st_uid, source_stat->st_gid) == 0)
				return (TRUE);
			break;
		case S_IFDIR:
			if (mkdir (destpath, new_mode & ~S_IFMT) != 0)
				break;
			if (chown (destpath, source_stat->st_uid, source_stat->st_gid) == 0)
				return (TRUE);
			return (TRUE);
		/*break;*/
	}
	return (FALSE);
}   /*  End Function copy_inode  */

static void free_config ()
/*  [SUMMARY] Free the configuration information.
    [RETURNS] Nothing.
*/
{
	struct config_entry_struct *c_entry;
	void *next;

#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "free_config()\n");
#endif

	for (c_entry = first_config; c_entry != NULL; c_entry = next)
	{
		unsigned int count;

		next = c_entry->next;
		regfree (&c_entry->preg);
		if (c_entry->action.what == AC_EXECUTE)
		{
			for (count = 0; count < MAX_ARGS; ++count)
			{
				if (c_entry->u.execute.argv[count] == NULL)
					break;
				free (c_entry->u.execute.argv[count]);
			}
		}
		free (c_entry);
	}
	first_config = NULL;
	last_config = NULL;
}   /*  End Function free_config  */

static int get_uid_gid (int flag, const char *string)
/*  [SUMMARY] Convert a string to a UID or GID value.
	<flag> "UID" or "GID".
	<string> The string.
    [RETURNS] The UID or GID value.
*/
{
	struct passwd *pw_ent;
	struct group *grp_ent;
	char * msg_a="user";
	char * msg_b="U";

#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "get_uid_gid()\n");


	if(flag != UID && flag != GID )
		msg_logger( DIE, LOG_ERR,"flag != UID && flag != GID\n");
#endif

	if ( isdigit (string[0]) || ( (string[0] == '-') && isdigit (string[1]) ) )
		return atoi (string);

	if ( flag == UID && ( pw_ent  = getpwnam (string) ) != NULL )
		return (pw_ent->pw_uid);

	if ( flag == GID && ( grp_ent = getgrnam (string) ) != NULL )
		return (grp_ent->gr_gid);
	else
	{
		msg_a="group";
		msg_b="G";
	}

	msg_logger( NO_DIE, LOG_ERR,"unknown %s: %s, defaulting to %cID=0\n", msg_a, string, msg_b);
	return (0);
}/*  End Function get_uid_gid  */

static mode_t get_mode (const char *string)
/*  [SUMMARY] Convert a string to a mode value.
    <string> The string.
    [RETURNS] The mode value.
*/
{
	mode_t mode;

#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "get_mode()\n");
#endif

	if ( isdigit (string[0]) )
		return strtoul (string, NULL, 8);
	if (strlen (string) != 9)
		msg_logger( DIE, LOG_ERR, "bad mode: %s\n", string);
	mode = 0;
	if (string[0] == 'r') mode |= S_IRUSR;
	if (string[1] == 'w') mode |= S_IWUSR;
	if (string[2] == 'x') mode |= S_IXUSR;
	if (string[3] == 'r') mode |= S_IRGRP;
	if (string[4] == 'w') mode |= S_IWGRP;
	if (string[5] == 'x') mode |= S_IXGRP;
	if (string[6] == 'r') mode |= S_IROTH;
	if (string[7] == 'w') mode |= S_IWOTH;
	if (string[8] == 'x') mode |= S_IXOTH;
	return (mode);
}   /*  End Function get_mode  */

static void signal_handler (int sig)
{
#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "signal_handler()\n");
#endif

	caught_signal = TRUE;
	if (sig == SIGHUP)
		caught_sighup = TRUE;
#ifdef CONFIG_DEVFSD_VERBOSE
	msg_logger( NO_DIE, LOG_INFO, "Caught %s\n",(sig == SIGHUP)?"SIGHUP" : "SIGUSR1");
#endif
}   /*  End Function signal_handler  */

static const char *get_variable (const char *variable, void *info)
{
	struct get_variable_info *gv_info = info;
	static char hostname[STRING_LENGTH], sbuf[STRING_LENGTH];

#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "get_variable()\n");
#endif

	if (gethostname (hostname, STRING_LENGTH - 1) != 0)
		msg_logger( DIE, LOG_ERR, "gethostname(): %m\n");
		/* Here on error we should do exit(RV_SYS_ERROR), instead we do exit(EXIT_FAILURE) */
	hostname[STRING_LENGTH - 1] = '\0';
	if (strcmp (variable, "hostname") == 0)
		return (hostname);
	if (strcmp (variable, "mntpnt") == 0)
		return (mount_point);
	if (gv_info == NULL)
		return (NULL);
	if (strcmp (variable, "devpath") == 0)
		return (gv_info->devpath);
	if (strcmp (variable, "devname") == 0)
		return (gv_info->devname);
	if (strcmp (variable, "mode") == 0)
	{
		sprintf (sbuf, "%o", gv_info->info->mode);
		return (sbuf);
	}
	if (strcmp (variable, "uid") == 0)
	{
		sprintf (sbuf, "%u", gv_info->info->uid);
		return (sbuf);
	}
	if (strcmp (variable, "gid") == 0)
	{
		sprintf (sbuf, "%u", gv_info->info->gid);
		return (sbuf);
	}
	return (NULL);
}   /*  End Function get_variable  */

static void do_scan_and_service (const char *dir_name)
/*  [SUMMARY] Scan a directory tree and generate register events on leaf nodes.
    <dp> The directory pointer. This is closed upon completion.
    <dir_name> The name of the directory.
    [RETURNS] Nothing.
*/
{
	struct stat statbuf;
	DIR *dp;
	struct dirent *de;
	char path[STRING_LENGTH];

#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "do_scan_and_service ()\n");
#endif

	if((dp = xopendir(NO_DIE, dir_name))==NULL)
		return;

	while ( (de = readdir (dp) ) != NULL )
	{
		struct devfsd_notify_struct info;

		if ( (strcmp (de->d_name, ".") == 0) || (strcmp (de->d_name, "..") == 0) )
			continue;
		snprintf (path, sizeof (path), "%s/%s", dir_name, de->d_name);
		if (lstat (path, &statbuf) != 0)
		{
#ifdef CONFIG_DEVFSD_VERBOSE
			msg_logger( NO_DIE, LOG_ERR, "%s: %m\n", path);
#endif
			continue;
		}
		memset (&info, 0, sizeof info);
		info.type = DEVFSD_NOTIFY_REGISTERED;
		info.mode = statbuf.st_mode;
		info.major = major (statbuf.st_rdev);
		info.minor = minor (statbuf.st_rdev);
		info.uid = statbuf.st_uid;
		info.gid = statbuf.st_gid;
		snprintf (info.devname, sizeof (info.devname), "%s", path + strlen (mount_point) + 1);
		info.namelen = strlen (info.devname);
		service_name (&info);
		if ( S_ISDIR (statbuf.st_mode) )
			do_scan_and_service (path);
	}
	closedir (dp);
}   /*  End Function do_scan_and_service  */

static int mksymlink (const char *oldpath, const char *newpath)
/*  [SUMMARY] Create a symlink, creating intervening directories as required.
    <oldpath> The string contained in the symlink.
    <newpath> The name of the new symlink.
    [RETURNS] 0 on success, else -1.
*/
{
#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "mksymlink()\n", newpath);
#endif


	if ( !make_dir_tree (newpath) )
		return (-1);

	if (symlink (oldpath, newpath) != 0)
    {
#ifdef CONFIG_DEVFSD_VERBOSE
		if (errno == EEXIST)
			msg_logger( NO_DIE, LOG_INFO, "symlink(): %s: already exists\n", newpath);
		else
		{
			msg_logger( NO_DIE, LOG_ERR, "symlink(): %s: %m\n", newpath);
			return (-1);
		}
#else
		if (errno != EEXIST)
			return (-1);
#endif
	}
    return (0);
}   /*  End Function mksymlink  */


static int make_dir_tree (const char *path)
/*  [SUMMARY] Creating intervening directories for a path as required.
    <path> The full pathname (including the leaf node).
    [RETURNS] TRUE on success, else FALSE.
*/
{
#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "make_dir_tree()\n");
#endif
	if (bb_make_directory( dirname((char *)path),  S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH ,FILEUTILS_RECUR )==-1)
	{
#ifdef CONFIG_DEVFSD_VERBOSE
		msg_logger( NO_DIE, LOG_ERR, "make_dir_tree(): %s: %m\n", path);
#endif
		return (FALSE);
	}
	return(TRUE);
} /*  End Function make_dir_tree  */

static int expand_expression(char *output, unsigned int outsize,
			      const char *input,
			      const char *(*get_variable_func)(const char *variable, void *info),
                              void *info,
                              const char *devname,
                              const regmatch_t *ex, unsigned int numexp)
/*  [SUMMARY] Expand enviroment variables and regular subexpressions in string.
    <output> The output expanded expression is written here.
    <length> The size of the output buffer.
    <input> The input expression. This may equal <<output>>.
    <get_variable> A function which will be used to get variable values. If
    this returns NULL, the environment is searched instead. If this is NULL,
    only the environment is searched.
    <info> An arbitrary pointer passed to <<get_variable>>.
    <devname> Device name; specifically, this is the string that contains all
    of the regular subexpressions.
    <ex> Array of start / end offsets into info->devname for each subexpression
    <numexp> Number of regular subexpressions found in <<devname>>.
    [RETURNS] TRUE on success, else FALSE.
*/
{
	char temp[STRING_LENGTH];

#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "expand_expression()\n");
#endif

	if ( !st_expr_expand (temp, STRING_LENGTH, input, get_variable_func, info) )
		return (FALSE);
	expand_regexp (output, outsize, temp, devname, ex, numexp);
	return (TRUE);
}   /*  End Function expand_expression  */

static void expand_regexp (char *output, size_t outsize, const char *input,
			   const char *devname,
			   const regmatch_t *ex, unsigned int numex )
/*  [SUMMARY] Expand all occurrences of the regular subexpressions \0 to \9.
    <output> The output expanded expression is written here.
    <outsize> The size of the output buffer.
    <input> The input expression. This may NOT equal <<output>>, because
    supporting that would require yet another string-copy. However, it's not
    hard to write a simple wrapper function to add this functionality for those
    few cases that need it.
    <devname> Device name; specifically, this is the string that contains all
    of the regular subexpressions.
    <ex> An array of start and end offsets into <<devname>>, one for each
    subexpression
    <numex> Number of subexpressions in the offset-array <<ex>>.
    [RETURNS] Nothing.
*/
{
	const char last_exp = '0' - 1 + numex;
	int c = -1;

#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "expand_regexp()\n");
#endif

	/*  Guarantee NULL termination by writing an explicit '\0' character into
	the very last byte  */
	if (outsize)
		output[--outsize] = '\0';
	/*  Copy the input string into the output buffer, replacing '\\' with '\'
	and '\0' .. '\9' with subexpressions 0 .. 9, if they exist. Other \x
	codes are deleted  */
	while ( (c != '\0') && (outsize != 0) )
	{
		c = *input;
		++input;
		if (c == '\\')
		{
			c = *input;
			++input;
			if (c != '\\')
			{
				if ((c >= '0') && (c <= last_exp))
				{
					const regmatch_t *subexp = ex + (c - '0');
					unsigned int sublen = subexp->rm_eo - subexp->rm_so;

					/*  Range checking  */
					if (sublen > outsize)
						sublen = outsize;
					strncpy (output, devname + subexp->rm_so, sublen);
					output += sublen;
					outsize -= sublen;
				}
				continue;
			}
		}
		*output = c;
		++output;
		--outsize;
	} /* while */
}   /*  End Function expand_regexp  */


/* from compat_name.c */

struct translate_struct
{
	char *match;    /*  The string to match to (up to length)                */
	char *format;   /*  Format of output, "%s" takes data past match string,
			NULL is effectively "%s" (just more efficient)       */
};

static struct translate_struct translate_table[] =
{
	{"sound/",     NULL},
	{"printers/",  "lp%s"},
	{"v4l/",       NULL},
	{"parports/",  "parport%s"},
	{"fb/",        "fb%s"},
	{"netlink/",   NULL},
	{"loop/",      "loop%s"},
	{"floppy/",    "fd%s"},
	{"rd/",        "ram%s"},
	{"md/",        "md%s"},         /*  Meta-devices                         */
	{"vc/",        "tty%s"},
	{"misc/",      NULL},
	{"isdn/",      NULL},
	{"pg/",        "pg%s"},         /*  Parallel port generic ATAPI interface*/
	{"i2c/",       "i2c-%s"},
	{"staliomem/", "staliomem%s"},  /*  Stallion serial driver control       */
	{"tts/E",      "ttyE%s"},       /*  Stallion serial driver               */
	{"cua/E",      "cue%s"},        /*  Stallion serial driver callout       */
	{"tts/R",      "ttyR%s"},       /*  Rocketport serial driver             */
	{"cua/R",      "cur%s"},        /*  Rocketport serial driver callout     */
	{"ip2/",       "ip2%s"},        /*  Computone serial driver control      */
	{"tts/F",      "ttyF%s"},       /*  Computone serial driver              */
	{"cua/F",      "cuf%s"},        /*  Computone serial driver callout      */
	{"tts/C",      "ttyC%s"},       /*  Cyclades serial driver               */
	{"cua/C",      "cub%s"},        /*  Cyclades serial driver callout       */
	{"tts/",       "ttyS%s"},       /*  Generic serial: must be after others */
	{"cua/",       "cua%s"},        /*  Generic serial: must be after others */
	{"input/js",   "js%s"},         /*  Joystick driver                      */
	{NULL,         NULL}
};

const char *get_old_name (const char *devname, unsigned int namelen,
			  char *buffer, unsigned int major, unsigned int minor)
/*  [SUMMARY] Translate a kernel-supplied name into an old name.
    <devname> The device name provided by the kernel.
    <namelen> The length of the name.
    <buffer> A buffer that may be used. This should be at least 128 bytes long.
    <major> The major number for the device.
    <minor> The minor number for the device.
    [RETURNS] A pointer to the old name if known, else NULL.
*/
{
	const char *compat_name = NULL;
	char *ptr;
	struct translate_struct *trans;

#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "get_old_name()\n");
#endif

	for (trans = translate_table; trans->match != NULL; ++trans)
	{
		size_t len = strlen (trans->match);

		if (strncmp (devname, trans->match, len) == 0)
		{
			if (trans->format == NULL)
				return (devname + len);
			sprintf (buffer, trans->format, devname + len);
			return (buffer);
		}
	}
	if (strncmp (devname, "sbp/", 4) == 0)
	{
		sprintf (buffer, "sbpcd%u", minor);
		compat_name = buffer;
	}
	else if (strncmp (devname, "scsi/", 5) == 0)
	{   /*  All SCSI devices  */
		if (strcmp (devname + namelen - 7, "generic") == 0)
		{
			sprintf (buffer, "sg%u", minor);
			compat_name = buffer;
		}
		else if (strncmp (ptr = (strrchr (devname, '/') + 1), "mt", 2) == 0)
		{
			char mode = ptr[2];

			if (mode == 'n')
				mode = '\0';
			sprintf (buffer, "nst%u%c", minor & 0x1f, mode);
			compat_name = buffer;
			if (devname[namelen - 1] != 'n')
				++compat_name;
		}
		else if (strcmp (devname + namelen - 2, "cd") == 0)
		{
			sprintf (buffer, "sr%u", minor);
			compat_name = buffer;
		}
		else if (strcmp (devname + namelen - 4, "disc") == 0)
			compat_name = write_old_sd_name (buffer, major, minor, "");
		else if (strncmp (ptr = (strrchr (devname, '/') + 1), "part", 4) == 0)
			compat_name = write_old_sd_name (buffer, major, minor, ptr + 4);
		return (compat_name);
	}
	else if (strncmp (devname, "ide/host", 8) == 0)
	{   /*  All IDE devices  */
		if (strncmp (ptr = (strrchr (devname, '/') + 1), "mt", 2) == 0)
		{
			sprintf (buffer, "%sht%d", ptr + 2, minor & 0x7f);
			compat_name = buffer;
		}
		else if (strcmp (devname + namelen - 4, "disc") == 0)
		{
			sprintf ( buffer, "hd%c", get_old_ide_name (major, minor) );
			compat_name = buffer;
		}
		else if (strncmp (ptr = (strrchr (devname, '/') + 1), "part", 4) == 0)
		{
			sprintf (buffer, "hd%c%s", get_old_ide_name (major, minor), ptr + 4);
			compat_name = buffer;
		}
		else if (strcmp (devname + namelen - 2, "cd") == 0)
		{
			sprintf ( buffer, "hd%c", get_old_ide_name (major, minor) );
			compat_name = buffer;
		}
		return (compat_name);
	}
	else if (strncmp (devname, "vcc/", 4) == 0)
	{
		sprintf (buffer, "vcs%s", devname + 4);
		if (buffer[3] == '0')
			buffer[3] = '\0';
		compat_name = buffer;
	}
	else if (strncmp (devname, "pty/", 4) == 0)
	{
		int indexx = atoi (devname + 5);
		const char *pty1 = "pqrstuvwxyzabcde";
		const char *pty2 = "0123456789abcdef";

		sprintf (buffer, "%cty%c%c", (devname[4] == 'm') ? 'p' : 't', pty1[indexx >> 4], pty2[indexx & 0x0f]);
		compat_name = buffer;
	}
	return (compat_name);
}   /*  End Function get_old_name  */

static char get_old_ide_name (unsigned int major, unsigned int minor)
/*  [SUMMARY] Get the old IDE name for a device.
    <major> The major number for the device.
    <minor> The minor number for the device.
    [RETURNS] The drive letter.
*/
{
	char letter='y';	/* 121 */
	char c='a'; 		/*  97 */
	int i=IDE0_MAJOR;


#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "get_old_ide_name()\n");
#endif

	/* I hope it works like the previous code as it saves a few bytes. Tito ;P */
	do {
		if(	i==IDE0_MAJOR || i==IDE1_MAJOR || i==IDE2_MAJOR ||
			i==IDE3_MAJOR || i==IDE4_MAJOR || i==IDE5_MAJOR ||
			i==IDE6_MAJOR || i==IDE7_MAJOR || i==IDE8_MAJOR ||
			i==IDE9_MAJOR )
		{
			if(i==major)
			{
				letter=c;
				break;
			}
			c+=2;
		}
		i++;
	} while(i<=IDE9_MAJOR);

	if (minor > 63)
		++letter;
	return (letter);
}   /*  End Function get_old_ide_name  */

static char *write_old_sd_name (char *buffer,
				unsigned int major, unsigned int minor,
				char *part)
/*  [SUMMARY] Write the old SCSI disc name to a buffer.
    <buffer> The buffer to write to.
    <major> The major number for the device.
    <minor> The minor number for the device.
    <part> The partition string. Must be "" for a whole-disc entry.
    [RETURNS] A pointer to the buffer on success, else NULL.
*/
{
	unsigned int disc_index;

#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "write_old_sd_name()\n");
#endif

	if (major == 8)
	{
		sprintf (buffer, "sd%c%s", 'a' + (minor >> 4), part);
		return (buffer);
	}
	if ( (major > 64) && (major < 72) )
	{
		disc_index = ( (major - 64) << 4 ) + (minor >> 4);
		if (disc_index < 26)
			sprintf (buffer, "sd%c%s", 'a' + disc_index, part);
		else
			sprintf (buffer, "sd%c%c%s", 'a' + (disc_index / 26) - 1, 'a' + disc_index % 26,part);
		return (buffer);
	}
	return (NULL);
}   /*  End Function write_old_sd_name  */


/*  expression.c */

/*EXPERIMENTAL_FUNCTION*/

int st_expr_expand (char *output, unsigned int length, const char *input,
		     const char *(*get_variable_func) (const char *variable,
						  void *info),
		     void *info)
/*  [SUMMARY] Expand an expression using Borne Shell-like unquoted rules.
    <output> The output expanded expression is written here.
    <length> The size of the output buffer.
    <input> The input expression. This may equal <<output>>.
    <get_variable> A function which will be used to get variable values. If
    this returns NULL, the environment is searched instead. If this is NULL,
    only the environment is searched.
    <info> An arbitrary pointer passed to <<get_variable>>.
    [RETURNS] TRUE on success, else FALSE.
*/
{
	char ch;
	unsigned int len;
	unsigned int out_pos = 0;
	const char *env;
	const char *ptr;
	struct passwd *pwent;
	char buffer[BUFFER_SIZE], tmp[STRING_LENGTH];

#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "st_expr_expand()\n");
#endif

	if (length > BUFFER_SIZE)
		length = BUFFER_SIZE;
	for (; TRUE; ++input)
	{
		switch (ch = *input)
		{
			case '$':
				/*  Variable expansion  */
				input = expand_variable (buffer, length, &out_pos, ++input, get_variable_func, info);
				if (input == NULL)
					return (FALSE);
				break;
			case '~':
				/*  Home directory expansion  */
				ch = input[1];
				if ( isspace (ch) || (ch == '/') || (ch == '\0') )
				{
					/* User's own home directory: leave separator for next time */
					if ( ( env = getenv ("HOME") ) == NULL )
					{
#ifdef CONFIG_DEVFSD_VERBOSE
						msg_logger( NO_DIE, LOG_INFO, bb_msg_variable_not_found, "HOME");
#endif
						return (FALSE);
					}
					len = strlen (env);
					if (len + out_pos >= length)
						goto st_expr_expand_out;
					memcpy (buffer + out_pos, env, len + 1);
					out_pos += len;
					continue;
				}
				/*  Someone else's home directory  */
				for (ptr = ++input; !isspace (ch) && (ch != '/') && (ch != '\0'); ch = *++ptr)
					/* VOID */ ;
				len = ptr - input;
				if (len >= sizeof tmp)
					goto st_expr_expand_out;
				safe_memcpy (tmp, input, len);
				input = ptr - 1;
				if ( ( pwent = getpwnam (tmp) ) == NULL )
				{
#ifdef CONFIG_DEVFSD_VERBOSE
					msg_logger( NO_DIE, LOG_INFO, "getpwnam(): %s\n", tmp);
#endif
					return (FALSE);
				}
				len = strlen (pwent->pw_dir);
				if (len + out_pos >= length)
					goto st_expr_expand_out;
				memcpy (buffer + out_pos, pwent->pw_dir, len + 1);
				out_pos += len;
				break;
			case '\0':
			/* Falltrough */
			default:
				if (out_pos >= length)
					goto st_expr_expand_out;
				buffer[out_pos++] = ch;
				if (ch == '\0')
				{
					memcpy (output, buffer, out_pos);
					return (TRUE);
				}
				break;
			/* esac */
		}
	}
	return (FALSE);
st_expr_expand_out:
#ifdef CONFIG_DEVFSD_VERBOSE
	msg_logger( NO_DIE, LOG_INFO, bb_msg_small_buffer);
#endif
	return (FALSE);
}   /*  End Function st_expr_expand  */


/*  Private functions follow  */

static const char *expand_variable (char *buffer, unsigned int length,
				    unsigned int *out_pos, const char *input,
				    const char *(*func) (const char *variable,
							 void *info),
				    void *info)
/*  [SUMMARY] Expand a variable.
    <buffer> The buffer to write to.
    <length> The length of the output buffer.
    <out_pos> The current output position. This is updated.
    <input> A pointer to the input character pointer.
    <func> A function which will be used to get variable values. If this
    returns NULL, the environment is searched instead. If this is NULL, only
    the environment is searched.
    <info> An arbitrary pointer passed to <<func>>.
    <errfp> Diagnostic messages are written here.
    [RETURNS] A pointer to the end of this subexpression on success, else NULL.
*/
{
	char ch;
	int len;
	unsigned int open_braces;
	const char *env, *ptr;
	char tmp[STRING_LENGTH];

#ifdef CONFIG_DEVFSD_DEBUG
	msg_logger( NO_DIE, LOG_INFO, "expand_variable()\n");
#endif

	ch = input[0];
	if (ch == '$')
	{
		/*  Special case for "$$": PID  */
		sprintf ( tmp, "%d", (int) getpid () );
		len = strlen (tmp);
		if (len + *out_pos >= length)
			goto expand_variable_out;

		memcpy (buffer + *out_pos, tmp, len + 1);
		out_pos += len;
		return (input);
	}
	/*  Ordinary variable expansion, possibly in braces  */
	if (ch != '{')
	{
		/*  Simple variable expansion  */
		for (ptr = input; isalnum (ch) || (ch == '_') || (ch == ':');ch = *++ptr)
			/* VOID */ ;
		len = ptr - input;
		if (len >= sizeof tmp)
			goto expand_variable_out;

		safe_memcpy (tmp, input, len);
		input = ptr - 1;
		if ( ( env = get_variable_v2 (tmp, func, info) ) == NULL )
		{
#ifdef CONFIG_DEVFSD_VERBOSE
			msg_logger( NO_DIE, LOG_INFO, bb_msg_variable_not_found, tmp);
#endif
			return (NULL);
		}
		len = strlen (env);
		if (len + *out_pos >= length)
			goto expand_variable_out;

		memcpy (buffer + *out_pos, env, len + 1);
		*out_pos += len;
		return (input);
	}
	/*  Variable in braces: check for ':' tricks  */
	ch = *++input;
	for (ptr = input; isalnum (ch) || (ch == '_'); ch = *++ptr)
		/* VOID */;
	if (ch == '}')
	{
		/*  Must be simple variable expansion with "${var}"  */
		len = ptr - input;
		if (len >= sizeof tmp)
			goto expand_variable_out;

		safe_memcpy (tmp, input, len);
		ptr = expand_variable (buffer, length, out_pos, tmp, func, info );
		if (ptr == NULL)
			return (NULL);
		return (input + len);
	}
	if (ch != ':' || ptr[1] != '-' )
	{
#ifdef CONFIG_DEVFSD_VERBOSE
		msg_logger( NO_DIE, LOG_INFO,"illegal char in var name\n");
#endif
		return (NULL);
	}
	/*  It's that handy "${var:-word}" expression. Check if var is defined  */
	len = ptr - input;
	if (len >= sizeof tmp)
		goto expand_variable_out;

	safe_memcpy (tmp, input, len);
	/*  Move input pointer to ':'  */
	input = ptr;
	/*  First skip to closing brace, taking note of nested expressions  */
	ptr += 2;
	ch = ptr[0];
	for (open_braces = 1; open_braces > 0; ch = *++ptr)
	{
		switch (ch)
		{
			case '{':
				++open_braces;
				break;
			case '}':
				--open_braces;
				break;
			case '\0':
#ifdef CONFIG_DEVFSD_VERBOSE
				msg_logger( NO_DIE, LOG_INFO,"\"}\" not found in: %s\n", input);
#endif
				return (NULL);
			default:
				break;
		}
	}
	--ptr;
	/*  At this point ptr should point to closing brace of "${var:-word}"  */
	if ( ( env = get_variable_v2 (tmp, func, info) ) != NULL )
	{
		/*  Found environment variable, so skip the input to the closing brace
			and return the variable  */
		input = ptr;
		len = strlen (env);
		if (len + *out_pos >= length)
			goto expand_variable_out;

		memcpy (buffer + *out_pos, env, len + 1);
		*out_pos += len;
		return (input);
	}
	/*  Environment variable was not found, so process word. Advance input
	pointer to start of word in "${var:-word}"  */
	input += 2;
	len = ptr - input;
	if (len >= sizeof tmp)
		goto expand_variable_out;

	safe_memcpy (tmp, input, len);
	input = ptr;
	if ( !st_expr_expand (tmp, STRING_LENGTH, tmp, func, info ) )
		return (NULL);
	len = strlen (tmp);
	if (len + *out_pos >= length)
		goto expand_variable_out;

	memcpy (buffer + *out_pos, tmp, len + 1);
	*out_pos += len;
	return (input);
expand_variable_out:
#ifdef CONFIG_DEVFSD_VERBOSE
	msg_logger( NO_DIE, LOG_INFO, bb_msg_small_buffer);
#endif
	return (NULL);
}   /*  End Function expand_variable  */


static const char *get_variable_v2 (const char *variable,
				  const char *(*func) (const char *variable, void *info),
				 void *info)
/*  [SUMMARY] Get a variable from the environment or .
    <variable> The variable name.
    <func> A function which will be used to get the variable. If this returns
    NULL, the environment is searched instead. If this is NULL, only the
    environment is searched.
    [RETURNS] The value of the variable on success, else NULL.
*/
{
	const char *value;

#ifdef CONFIG_DEVFSD_DEBUG
		msg_logger( NO_DIE, LOG_INFO, "get_variable_v2()\n");
#endif

	if (func != NULL)
	{
		value = (*func) (variable, info);
		if (value != NULL)
			return (value);
	}
	return getenv (variable);
}   /*  End Function get_variable  */

/* END OF CODE */
