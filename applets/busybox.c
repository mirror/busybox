/* vi: set sw=4 ts=4: */
#include "internal.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

static int been_there_done_that = 0;

/* It has been alledged that doing such things can
 * help reduce binary size when staticly linking,
 * of course with glibc, this is unlikely as long
 * as we use things like printf -- perhaps a printf
 * replacement may be in order 
 */
#if 0
void exit(int status) __attribute__ ((noreturn));
void exit(int status)
{
	_exit(status);
};
void abort(void) __attribute__ ((__noreturn__));
void abort(void)
{
	_exit(0);
};
int atexit(void (*__func) (void))
{
	_exit(0);
};
void *__libc_stack_end;
#endif

const struct BB_applet applets[] = {

#ifdef BB_AR
	{"ar", ar_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_BASENAME
	{"basename", basename_main, _BB_DIR_USR_BIN},
#endif
	{"busybox", busybox_main, _BB_DIR_BIN},
#ifdef BB_CAT
	{"cat", cat_main, _BB_DIR_BIN},
#endif
#ifdef BB_CHMOD_CHOWN_CHGRP
	{"chgrp", chmod_chown_chgrp_main, _BB_DIR_BIN},
#endif
#ifdef BB_CHMOD_CHOWN_CHGRP
	{"chmod", chmod_chown_chgrp_main, _BB_DIR_BIN},
#endif
#ifdef BB_CHMOD_CHOWN_CHGRP
	{"chown", chmod_chown_chgrp_main, _BB_DIR_BIN},
#endif
#ifdef BB_CHROOT
	{"chroot", chroot_main, _BB_DIR_SBIN},
#endif
#ifdef BB_CLEAR
	{"clear", clear_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_CHVT
	{"chvt", chvt_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_CP_MV
	{"cp", cp_mv_main, _BB_DIR_BIN},
#endif
#ifdef BB_CUT
	{"cut", cut_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_DATE
	{"date", date_main, _BB_DIR_BIN},
#endif
#ifdef BB_DC
	{"dc", dc_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_DD
	{"dd", dd_main, _BB_DIR_BIN},
#endif
#ifdef BB_DF
	{"df", df_main, _BB_DIR_BIN},
#endif
#ifdef BB_DIRNAME
	{"dirname", dirname_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_DMESG
	{"dmesg", dmesg_main, _BB_DIR_BIN},
#endif
#ifdef BB_DU
	{"du", du_main, _BB_DIR_BIN},
#endif
#ifdef BB_DUTMP
	{"dutmp", dutmp_main, _BB_DIR_USR_SBIN},
#endif
#ifdef BB_ECHO
	{"echo", echo_main, _BB_DIR_BIN},
#endif
#ifdef BB_TRUE_FALSE
	{"false", false_main, _BB_DIR_BIN},
#endif
#ifdef BB_FBSET
	{"fbset", fbset_main, _BB_DIR_USR_SBIN},
#endif
#ifdef BB_FDFLUSH
	{"fdflush", fdflush_main, _BB_DIR_BIN},
#endif
#ifdef BB_FIND
	{"find", find_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_FREE
	{"free", free_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_FREERAMDISK
	{"freeramdisk", freeramdisk_main, _BB_DIR_SBIN},
#endif
#ifdef BB_DEALLOCVT
	{"deallocvt", deallocvt_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_FSCK_MINIX
	{"fsck.minix", fsck_minix_main, _BB_DIR_SBIN},
#endif
#ifdef BB_GREP
	{"grep", grep_main, _BB_DIR_BIN},
#endif
#ifdef BB_GUNZIP
	{"gunzip", gunzip_main, _BB_DIR_BIN},
#endif
#ifdef BB_GZIP
	{"gzip", gzip_main, _BB_DIR_BIN},
#endif
#ifdef BB_HALT
	{"halt", halt_main, _BB_DIR_SBIN},
#endif
#ifdef BB_HEAD
	{"head", head_main, _BB_DIR_BIN},
#endif
#ifdef BB_HOSTID
	{"hostid", hostid_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_HOSTNAME
	{"hostname", hostname_main, _BB_DIR_BIN},
#endif
#ifdef BB_ID
	{"id", id_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_INIT
	{"init", init_main, _BB_DIR_SBIN},
#endif
#ifdef BB_INSMOD
	{"insmod", insmod_main, _BB_DIR_SBIN},
#endif
#ifdef BB_KILL
	{"kill", kill_main, _BB_DIR_BIN},
#endif
#ifdef BB_KILLALL
	{"killall", kill_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_LENGTH
	{"length", length_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_LINUXRC
	{"linuxrc", init_main, _BB_DIR_ROOT},
#endif
#ifdef BB_LN
	{"ln", ln_main, _BB_DIR_BIN},
#endif
#ifdef BB_LOADACM
	{"loadacm", loadacm_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_LOADFONT
	{"loadfont", loadfont_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_LOADKMAP
	{"loadkmap", loadkmap_main, _BB_DIR_SBIN},
#endif
#ifdef BB_LOGGER
	{"logger", logger_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_LOGNAME
	{"logname", logname_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_LS
	{"ls", ls_main, _BB_DIR_BIN},
#endif
#ifdef BB_LSMOD
	{"lsmod", lsmod_main, _BB_DIR_SBIN},
#endif
#ifdef BB_MAKEDEVS
	{"makedevs", makedevs_main, _BB_DIR_SBIN},
#endif
#ifdef BB_MD5SUM
	{"md5sum", md5sum_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_MKDIR
	{"mkdir", mkdir_main, _BB_DIR_BIN},
#endif
#ifdef BB_MKFIFO
	{"mkfifo", mkfifo_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_MKFS_MINIX
	{"mkfs.minix", mkfs_minix_main, _BB_DIR_SBIN},
#endif
#ifdef BB_MKNOD
	{"mknod", mknod_main, _BB_DIR_BIN},
#endif
#ifdef BB_MKSWAP
	{"mkswap", mkswap_main, _BB_DIR_SBIN},
#endif
#ifdef BB_MKTEMP
	{"mktemp", mktemp_main, _BB_DIR_BIN},
#endif
#ifdef BB_NC
	{"nc", nc_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_MORE
	{"more", more_main, _BB_DIR_BIN},
#endif
#ifdef BB_MOUNT
	{"mount", mount_main, _BB_DIR_BIN},
#endif
#ifdef BB_MT
	{"mt", mt_main, _BB_DIR_BIN},
#endif
#ifdef BB_CP_MV
	{"mv", cp_mv_main, _BB_DIR_BIN},
#endif
#ifdef BB_NSLOOKUP
	{"nslookup", nslookup_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_PING
	{"ping", ping_main, _BB_DIR_BIN},
#endif
#ifdef BB_POWEROFF
	{"poweroff", poweroff_main, _BB_DIR_SBIN},
#endif
#ifdef BB_PRINTF
	{"printf", printf_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_PS
	{"ps", ps_main, _BB_DIR_BIN},
#endif
#ifdef BB_PWD
	{"pwd", pwd_main, _BB_DIR_BIN},
#endif
#ifdef BB_REBOOT
	{"reboot", reboot_main, _BB_DIR_SBIN},
#endif
#ifdef BB_RM
	{"rm", rm_main, _BB_DIR_BIN},
#endif
#ifdef BB_RMDIR
	{"rmdir", rmdir_main, _BB_DIR_BIN},
#endif
#ifdef BB_RMMOD
	{"rmmod", rmmod_main, _BB_DIR_SBIN},
#endif
#ifdef BB_SED
	{"sed", sed_main, _BB_DIR_BIN},
#endif
#ifdef BB_SETKEYCODES
	{"setkeycodes", setkeycodes_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_SH
	{"sh", shell_main, _BB_DIR_BIN},
#endif
#ifdef BB_SLEEP
	{"sleep", sleep_main, _BB_DIR_BIN},
#endif
#ifdef BB_SORT
	{"sort", sort_main, _BB_DIR_BIN},
#endif
#ifdef BB_SYNC
	{"sync", sync_main, _BB_DIR_BIN},
#endif
#ifdef BB_SYSLOGD
	{"syslogd", syslogd_main, _BB_DIR_SBIN},
#endif
#ifdef BB_SWAPONOFF
	{"swapon", swap_on_off_main, _BB_DIR_SBIN},
#endif
#ifdef BB_SWAPONOFF
	{"swapoff", swap_on_off_main, _BB_DIR_SBIN},
#endif
#ifdef BB_TAIL
	{"tail", tail_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_TAR
	{"tar", tar_main, _BB_DIR_BIN},
#endif
#ifdef BB_TELNET
	{"telnet", telnet_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_TEST
	{"test", test_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_TEE
	{"tee", tee_main, _BB_DIR_BIN},
#endif
#ifdef BB_TOUCH
	{"touch", touch_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_TR
	{"tr", tr_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_TRUE_FALSE
	{"true", true_main, _BB_DIR_BIN},
#endif
#ifdef BB_TTY
	{"tty", tty_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_UMOUNT
	{"umount", umount_main, _BB_DIR_BIN},
#endif
#ifdef BB_UNAME
	{"uname", uname_main, _BB_DIR_BIN},
#endif
#ifdef BB_UNIQ
	{"uniq", uniq_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_UPDATE
	{"update", update_main, _BB_DIR_SBIN},
#endif
#ifdef BB_UPTIME
	{"uptime", uptime_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_UUENCODE
	{"uuencode", uuencode_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_UUDECODE
	{"uudecode", uudecode_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_USLEEP
	{"usleep", usleep_main, _BB_DIR_BIN},
#endif
#ifdef BB_WC
	{"wc", wc_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_WHICH
	{"which", which_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_WHOAMI
	{"whoami", whoami_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_YES
	{"yes", yes_main, _BB_DIR_USR_BIN},
#endif
#ifdef BB_GUNZIP
	{"zcat", gunzip_main, _BB_DIR_BIN},
#endif
#ifdef BB_TEST
	{"[", test_main, _BB_DIR_USR_BIN},
#endif
	{0}
};


#ifdef BB_FEATURE_INSTALLER
/* 
 * directory table
 *		this should be consistent w/ the enum, internal.h::Location,
 *		or else...
 */
static char* install_dir[] = {
	"/",
	"/bin",
	"/sbin",
	"/usr/bin",
	"/usr/sbin",
};

/* abstract link() */
typedef int (*__link_f)(const char *, const char *);

/* 
 * Where in the filesystem is this busybox?
 * [return]
 *		malloc'd string w/ full pathname of busybox's location
 *		NULL on failure
 */
static char *busybox_fullpath()
{
	pid_t pid;
	char path[256];
	char proc[256];
	int len;

	pid = getpid();
	sprintf(proc, "/proc/%d/exe", pid);
	len = readlink(proc, path, 256);
	if (len != -1) {
		path[len] = 0;
	} else {
		fprintf(stderr, "busybox : %s : %s\n", proc, strerror(errno));
		return NULL;
	}
	return strdup(path);
}

/* create (sym)links for each applet */
static int install_links(const char *busybox, int use_symbolic_links)
{
	__link_f Link = link;

	char command[256];
	int i;
	int rc = 0;

	if (use_symbolic_links) Link = symlink;

	for (i = 0; applets[i].name != NULL; i++) {
		sprintf (
			command, 
			"%s/%s", 
			install_dir[applets[i].location], 
			applets[i].name
		);
#if 1
		rc |= Link(busybox, command);
#else
		puts(command);
#endif
		if (rc) {
			fprintf(stderr,"busybox : %s : %s\n", command, strerror(errno));
		}
	}
	return rc;
}

#if 0
int uninstall_links() ?
#endif
#endif /* BB_FEATURE_INSTALLER */


int main(int argc, char **argv)
{
	char				*s;
	char				*name;
	const struct BB_applet	*a		= applets;

#ifdef BB_FEATURE_INSTALLER	
	/* 
	 * This style of argument parsing doesn't scale well 
	 * in the event that busybox starts wanting more --options.
	 * If someone has a cleaner approach, by all means implement it.
	 */
	if (argc > 1 && (strcmp(argv[1], "--install") == 0)) {
		int use_symbolic_links = 0;
		int rc = 0;
		char *busybox;

		/* to use symlinks, or not to use symlinks... */
		if (argc > 2) {
			if ((strcmp(argv[2], "-s") == 0)) { 
				use_symbolic_links = 1; 
			}
		}

		/* link */
		busybox = busybox_fullpath();
		if (busybox) {
			install_links(busybox, use_symbolic_links);
			free(busybox);
		} else {
			rc = 1;
		}
		return rc;
	}
#endif /* BB_FEATURE_INSTALLER */

	for (s = name = argv[0]; *s != '\0';) {
		if (*s++ == '/')
			name = s;
	}

	*argv = name;

#ifdef BB_SH
	/* Add in a special case hack -- whenever **argv == '-'
	 * (i.e. '-su' or '-sh') always invoke the shell */
	if (**argv == '-')
		exit(((*(shell_main)) (argc, argv)));
#endif

	while (a->name != 0) {
		if (strcmp(name, a->name) == 0) {
			exit(((*(a->main)) (argc, argv)));
		}
		a++;
	}
	return(busybox_main(argc, argv));
}


int busybox_main(int argc, char **argv)
{
	int col = 0;

	argc--;
	argv++;

	if (been_there_done_that == 1 || argc < 1) {
		const struct BB_applet *a = applets;

		fprintf(stderr, "BusyBox v%s (%s) multi-call binary -- GPL2\n\n"
				"Usage: busybox [function] [arguments]...\n"
				"   or: [function] [arguments]...\n\n"
				"\tBusyBox is a multi-call binary that combines many common Unix\n"
				"\tutilities into a single executable.  Most people will create a\n"
				"\tlink to busybox for each function they wish to use, and BusyBox\n"
				"\twill act like whatever it was invoked as.\n" 
				"\nCurrently defined functions:\n", BB_VER, BB_BT);

		while (a->name != 0) {
			col +=
				fprintf(stderr, "%s%s", ((col == 0) ? "\t" : ", "),
						(a++)->name);
			if (col > 60 && a->name != 0) {
				fprintf(stderr, ",\n");
				col = 0;
			}
		}
		fprintf(stderr, "\n\n");
		exit(-1);
	}
	/* If we've already been here once, exit now */
	been_there_done_that = 1;
	return (main(argc, argv));
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
