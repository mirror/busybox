/*
 * applets.h - a listing of all busybox applets.
 *
 * If you write a new applet, you need to add an entry to this list to make
 * busybox aware of it.
 *
 * It is CRUCIAL that this listing be kept in ascii order, otherwise the binary
 * search lookup contributed by Gaute B Strokkenes stops working. If you value
 * your kneecaps, you'll be sure to *make sure* that any changes made to this
 * file result in the listing remaining in ascii order. You have been warned.
 */

#if defined(PROTOTYPES)
#define APPLET(a,b,c,d) \
	extern int b(int argc, char **argv); \
	extern const char d[];
#define APPLET_NOUSAGE(a,b,c) \
	extern int b(int argc, char **argv);
#elif defined(MAKE_LINKS)
#define APPLET(a,b,c,d) LINK c a
#define APPLET_NOUSAGE(a,b,c) LINK c a
#else
const struct BB_applet applets[] = {
#define APPLET(a,b,c,d) {a,b,c,d},
#define APPLET_NOUSAGE(a,b,c) {a,b,c,NULL},
#endif

#ifdef BB_TEST
	APPLET("[", test_main, _BB_DIR_USR_BIN, test_usage)
#endif
#ifdef BB_AR
	APPLET("ar", ar_main, _BB_DIR_USR_BIN, ar_usage)
#endif
#ifdef BB_BASENAME
	APPLET("basename", basename_main, _BB_DIR_USR_BIN, basename_usage)
#endif
	APPLET_NOUSAGE("busybox", busybox_main, _BB_DIR_BIN)
#ifdef BB_CAT
	APPLET("cat", cat_main, _BB_DIR_BIN, cat_usage)
#endif
#ifdef BB_CHMOD_CHOWN_CHGRP
	APPLET("chgrp", chmod_chown_chgrp_main, _BB_DIR_BIN, chgrp_usage)
#endif
#ifdef BB_CHMOD_CHOWN_CHGRP
	APPLET("chmod", chmod_chown_chgrp_main, _BB_DIR_BIN, chmod_usage)
#endif
#ifdef BB_CHMOD_CHOWN_CHGRP
	APPLET("chown", chmod_chown_chgrp_main, _BB_DIR_BIN, chown_usage)
#endif
#ifdef BB_CHROOT
	APPLET("chroot", chroot_main, _BB_DIR_USR_SBIN, chroot_usage)
#endif
#ifdef BB_CHVT
	APPLET("chvt", chvt_main, _BB_DIR_USR_BIN, chvt_usage)
#endif
#ifdef BB_CLEAR
	APPLET("clear", clear_main, _BB_DIR_USR_BIN, clear_usage)
#endif
#ifdef BB_CMP
	APPLET("cmp", cmp_main, _BB_DIR_USR_BIN, cmp_usage)
#endif
#ifdef BB_CP_MV
	APPLET("cp", cp_mv_main, _BB_DIR_BIN, cp_usage)
#endif
#ifdef BB_CUT
	APPLET("cut", cut_main, _BB_DIR_USR_BIN, cut_usage)
#endif
#ifdef BB_DATE
	APPLET("date", date_main, _BB_DIR_BIN, date_usage)
#endif
#ifdef BB_DC
	APPLET("dc", dc_main, _BB_DIR_USR_BIN, dc_usage)
#endif
#ifdef BB_DD
	APPLET("dd", dd_main, _BB_DIR_BIN, dd_usage)
#endif
#ifdef BB_DEALLOCVT
	APPLET("deallocvt", deallocvt_main, _BB_DIR_USR_BIN, deallocvt_usage)
#endif
#ifdef BB_DF
	APPLET("df", df_main, _BB_DIR_BIN, df_usage)
#endif
#ifdef BB_DIRNAME
	APPLET("dirname", dirname_main, _BB_DIR_USR_BIN, dirname_usage)
#endif
#ifdef BB_DMESG
	APPLET("dmesg", dmesg_main, _BB_DIR_BIN, dmesg_usage)
#endif
#ifdef BB_DOS2UNIX
	APPLET("dos2unix", dos2unix_main, _BB_DIR_USR_BIN, dos2unix_usage)
#endif
#ifdef BB_DU
	APPLET("du", du_main, _BB_DIR_USR_BIN, du_usage)
#endif
#ifdef BB_DUMPKMAP
	APPLET("dumpkmap", dumpkmap_main, _BB_DIR_BIN, dumpkmap_usage)
#endif
#ifdef BB_DUTMP
	APPLET("dutmp", dutmp_main, _BB_DIR_USR_SBIN, dutmp_usage)
#endif
#ifdef BB_ECHO
	APPLET("echo", echo_main, _BB_DIR_BIN, echo_usage)
#endif
#ifdef BB_EXPR
	APPLET("expr", expr_main, _BB_DIR_USR_BIN, expr_usage)
#endif
#ifdef BB_TRUE_FALSE
	APPLET("false", false_main, _BB_DIR_BIN, false_usage)
#endif
#ifdef BB_FBSET
	APPLET_NOUSAGE("fbset", fbset_main, _BB_DIR_USR_SBIN)
#endif
#ifdef BB_FDFLUSH
	APPLET("fdflush", fdflush_main, _BB_DIR_BIN, fdflush_usage)
#endif
#ifdef BB_FIND
	APPLET("find", find_main, _BB_DIR_USR_BIN, find_usage)
#endif
#ifdef BB_FREE
	APPLET("free", free_main, _BB_DIR_USR_BIN, free_usage)
#endif
#ifdef BB_FREERAMDISK
	APPLET("freeramdisk", freeramdisk_main, _BB_DIR_SBIN, freeramdisk_usage)
#endif
#ifdef BB_FSCK_MINIX
	APPLET("fsck.minix", fsck_minix_main, _BB_DIR_SBIN, fsck_minix_usage)
#endif
#ifdef BB_GETOPT
	APPLET("getopt", getopt_main, _BB_DIR_BIN, getopt_usage)
#endif
#ifdef BB_GREP
	APPLET("grep", grep_main, _BB_DIR_BIN, grep_usage)
#endif
#ifdef BB_GUNZIP
	APPLET("gunzip", gunzip_main, _BB_DIR_BIN, gunzip_usage)
#endif
#ifdef BB_GZIP
	APPLET("gzip", gzip_main, _BB_DIR_BIN, gzip_usage)
#endif
#ifdef BB_HALT
	APPLET("halt", halt_main, _BB_DIR_SBIN, halt_usage)
#endif
#ifdef BB_HEAD
	APPLET("head", head_main, _BB_DIR_USR_BIN, head_usage)
#endif
#ifdef BB_HOSTID
	APPLET("hostid", hostid_main, _BB_DIR_USR_BIN, hostid_usage)
#endif
#ifdef BB_HOSTNAME
	APPLET("hostname", hostname_main, _BB_DIR_BIN, hostname_usage)
#endif
#ifdef BB_ID
	APPLET("id", id_main, _BB_DIR_USR_BIN, id_usage)
#endif
#ifdef BB_INIT
	APPLET_NOUSAGE("init", init_main, _BB_DIR_SBIN)
#endif
#ifdef BB_INSMOD
	APPLET("insmod", insmod_main, _BB_DIR_SBIN, insmod_usage)
#endif
#ifdef BB_KILL
	APPLET("kill", kill_main, _BB_DIR_BIN, kill_usage)
#endif
#ifdef BB_KILLALL
	APPLET("killall", kill_main, _BB_DIR_USR_BIN, killall_usage)
#endif
#ifdef BB_LENGTH
	APPLET("length", length_main, _BB_DIR_USR_BIN, length_usage)
#endif
#ifdef BB_LINUXRC
	APPLET_NOUSAGE("linuxrc", init_main, _BB_DIR_ROOT)
#endif
#ifdef BB_LN
	APPLET("ln", ln_main, _BB_DIR_BIN, ln_usage)
#endif
#ifdef BB_LOADACM
	APPLET("loadacm", loadacm_main, _BB_DIR_USR_BIN, loadacm_usage)
#endif
#ifdef BB_LOADFONT
	APPLET("loadfont", loadfont_main, _BB_DIR_USR_BIN, loadfont_usage)
#endif
#ifdef BB_LOADKMAP
	APPLET("loadkmap", loadkmap_main, _BB_DIR_SBIN, loadkmap_usage)
#endif
#ifdef BB_LOGGER
	APPLET("logger", logger_main, _BB_DIR_USR_BIN, logger_usage)
#endif
#ifdef BB_LOGNAME
	APPLET("logname", logname_main, _BB_DIR_USR_BIN, logname_usage)
#endif
#ifdef BB_LS
	APPLET("ls", ls_main, _BB_DIR_BIN, ls_usage)
#endif
#ifdef BB_LSMOD
	APPLET("lsmod", lsmod_main, _BB_DIR_SBIN, lsmod_usage)
#endif
#ifdef BB_MAKEDEVS
	APPLET("makedevs", makedevs_main, _BB_DIR_SBIN, makedevs_usage)
#endif
#ifdef BB_MD5SUM
	APPLET("md5sum", md5sum_main, _BB_DIR_USR_BIN, md5sum_usage)
#endif
#ifdef BB_MKDIR
	APPLET("mkdir", mkdir_main, _BB_DIR_BIN, mkdir_usage)
#endif
#ifdef BB_MKFIFO
	APPLET("mkfifo", mkfifo_main, _BB_DIR_USR_BIN, mkfifo_usage)
#endif
#ifdef BB_MKFS_MINIX
	APPLET("mkfs.minix", mkfs_minix_main, _BB_DIR_SBIN, mkfs_minix_usage)
#endif
#ifdef BB_MKNOD
	APPLET("mknod", mknod_main, _BB_DIR_BIN, mknod_usage)
#endif
#ifdef BB_MKSWAP
	APPLET("mkswap", mkswap_main, _BB_DIR_SBIN, mkswap_usage)
#endif
#ifdef BB_MKTEMP
	APPLET("mktemp", mktemp_main, _BB_DIR_BIN, mktemp_usage)
#endif
#ifdef BB_MORE
	APPLET("more", more_main, _BB_DIR_BIN, more_usage)
#endif
#ifdef BB_MOUNT
	APPLET("mount", mount_main, _BB_DIR_BIN, mount_usage)
#endif
#ifdef BB_MT
	APPLET("mt", mt_main, _BB_DIR_BIN, mt_usage)
#endif
#ifdef BB_CP_MV
	APPLET("mv", cp_mv_main, _BB_DIR_BIN, mv_usage)
#endif
#ifdef BB_NC
	APPLET("nc", nc_main, _BB_DIR_USR_BIN, nc_usage)
#endif
#ifdef BB_NSLOOKUP
	APPLET("nslookup", nslookup_main, _BB_DIR_USR_BIN, nslookup_usage)
#endif
#ifdef BB_PING
	APPLET("ping", ping_main, _BB_DIR_BIN, ping_usage)
#endif
#ifdef BB_POWEROFF
	APPLET("poweroff", poweroff_main, _BB_DIR_SBIN, poweroff_usage)
#endif
#ifdef BB_PRINTF
	APPLET("printf", printf_main, _BB_DIR_USR_BIN, printf_usage)
#endif
#ifdef BB_PS
	APPLET("ps", ps_main, _BB_DIR_BIN, ps_usage)
#endif
#ifdef BB_PWD
	APPLET("pwd", pwd_main, _BB_DIR_BIN, pwd_usage)
#endif
#ifdef BB_RDATE
	APPLET("rdate", rdate_main, _BB_DIR_USR_BIN, rdate_usage)
#endif
#ifdef BB_READLINK
	APPLET("readlink", readlink_main, _BB_DIR_USR_BIN, readlink_usage)
#endif
#ifdef BB_REBOOT
	APPLET("reboot", reboot_main, _BB_DIR_SBIN, reboot_usage)
#endif
#ifdef BB_RENICE
	APPLET("renice", renice_main, _BB_DIR_USR_BIN, renice_usage)
#endif
#ifdef BB_RESET
	APPLET("reset", reset_main, _BB_DIR_USR_BIN, reset_usage)
#endif
#ifdef BB_RM
	APPLET("rm", rm_main, _BB_DIR_BIN, rm_usage)
#endif
#ifdef BB_RMDIR
	APPLET("rmdir", rmdir_main, _BB_DIR_BIN, rmdir_usage)
#endif
#ifdef BB_RMMOD
	APPLET("rmmod", rmmod_main, _BB_DIR_SBIN, rmmod_usage)
#endif
#ifdef BB_RPMUNPACK
	APPLET("rpmunpack", rpmunpack_main, _BB_DIR_USR_BIN, rpmunpack_usage)
#endif
#ifdef BB_SED
	APPLET("sed", sed_main, _BB_DIR_BIN, sed_usage)
#endif
#ifdef BB_SETKEYCODES
	APPLET("setkeycodes", setkeycodes_main, _BB_DIR_USR_BIN, setkeycodes_usage)
#endif
#ifdef BB_SH
	APPLET("sh", shell_main, _BB_DIR_BIN, shell_usage)
#endif
#ifdef BB_SLEEP
	APPLET("sleep", sleep_main, _BB_DIR_BIN, sleep_usage)
#endif
#ifdef BB_SORT
	APPLET("sort", sort_main, _BB_DIR_USR_BIN, sort_usage)
#endif
#ifdef BB_SWAPONOFF
	APPLET("swapoff", swap_on_off_main, _BB_DIR_SBIN, swapoff_usage)
#endif
#ifdef BB_SWAPONOFF
	APPLET("swapon", swap_on_off_main, _BB_DIR_SBIN, swapon_usage)
#endif
#ifdef BB_SYNC
	APPLET("sync", sync_main, _BB_DIR_BIN, sync_usage)
#endif
#ifdef BB_SYSLOGD
	APPLET("syslogd", syslogd_main, _BB_DIR_SBIN, syslogd_usage)
#endif
#ifdef BB_TAIL
	APPLET("tail", tail_main, _BB_DIR_USR_BIN, tail_usage)
#endif
#ifdef BB_TAR
	APPLET("tar", tar_main, _BB_DIR_BIN, tar_usage)
#endif
#ifdef BB_TEE
	APPLET("tee", tee_main, _BB_DIR_USR_BIN, tee_usage)
#endif
#ifdef BB_TELNET
	APPLET("telnet", telnet_main, _BB_DIR_USR_BIN, telnet_usage)
#endif
#ifdef BB_TEST
	APPLET("test", test_main, _BB_DIR_USR_BIN, test_usage)
#endif
#ifdef BB_TOUCH
	APPLET("touch", touch_main, _BB_DIR_BIN, touch_usage)
#endif
#ifdef BB_TR
	APPLET("tr", tr_main, _BB_DIR_USR_BIN, tr_usage)
#endif
#ifdef BB_TRUE_FALSE
	APPLET("true", true_main, _BB_DIR_BIN, true_usage)
#endif
#ifdef BB_TTY
	APPLET("tty", tty_main, _BB_DIR_USR_BIN, tty_usage)
#endif
#ifdef BB_UMOUNT
	APPLET("umount", umount_main, _BB_DIR_BIN, umount_usage)
#endif
#ifdef BB_UNAME
	APPLET("uname", uname_main, _BB_DIR_BIN, uname_usage)
#endif
#ifdef BB_UNIQ
	APPLET("uniq", uniq_main, _BB_DIR_USR_BIN, uniq_usage)
#endif
#ifdef BB_UNIX2DOS
	APPLET("unix2dos", unix2dos_main, _BB_DIR_USR_BIN, unix2dos_usage)
#endif
#ifdef BB_UPDATE
	APPLET("update", update_main, _BB_DIR_SBIN, update_usage)
#endif
#ifdef BB_UPTIME
	APPLET("uptime", uptime_main, _BB_DIR_USR_BIN, uptime_usage)
#endif
#ifdef BB_USLEEP
	APPLET("usleep", usleep_main, _BB_DIR_BIN, usleep_usage)
#endif
#ifdef BB_UUDECODE
	APPLET("uudecode", uudecode_main, _BB_DIR_USR_BIN, uudecode_usage)
#endif
#ifdef BB_UUENCODE
	APPLET("uuencode", uuencode_main, _BB_DIR_USR_BIN, uuencode_usage)
#endif
#ifdef BB_WC
	APPLET("wc", wc_main, _BB_DIR_USR_BIN, wc_usage)
#endif
#ifdef BB_WGET
	APPLET("wget", wget_main, _BB_DIR_USR_BIN, wget_usage)
#endif
#ifdef BB_WHICH
	APPLET("which", which_main, _BB_DIR_USR_BIN, which_usage)
#endif
#ifdef BB_WHOAMI
	APPLET("whoami", whoami_main, _BB_DIR_USR_BIN, whoami_usage)
#endif
#ifdef BB_XARGS
	APPLET("xargs", xargs_main, _BB_DIR_USR_BIN, xargs_usage)
#endif
#ifdef BB_YES
	APPLET("yes", yes_main, _BB_DIR_USR_BIN, yes_usage)
#endif
#ifdef BB_GUNZIP
	APPLET("zcat", gunzip_main, _BB_DIR_BIN, gunzip_usage)
#endif

#if !defined(PROTOTYPES) && !defined(MAKE_LINKS)
	{ 0,NULL,0,NULL}
};

/* The -1 arises because of the {0,NULL,0,NULL} entry above. */
size_t NUM_APPLETS = (sizeof (applets) / sizeof (struct BB_applet) - 1);

#endif
