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

const struct BB_applet applets[] = {

#ifdef BB_TEST
	{"[", test_main, _BB_DIR_USR_BIN, test_usage},
#endif
#ifdef BB_AR
	{"ar", ar_main, _BB_DIR_USR_BIN, ar_usage},
#endif
#ifdef BB_BASENAME
	{"basename", basename_main, _BB_DIR_USR_BIN, basename_usage},
#endif
	{"busybox", busybox_main, _BB_DIR_BIN, NULL},
#ifdef BB_CAT
	{"cat", cat_main, _BB_DIR_BIN, cat_usage},
#endif
#ifdef BB_CHMOD_CHOWN_CHGRP
	{"chgrp", chmod_chown_chgrp_main, _BB_DIR_BIN, chgrp_usage},
#endif
#ifdef BB_CHMOD_CHOWN_CHGRP
	{"chmod", chmod_chown_chgrp_main, _BB_DIR_BIN, chmod_usage},
#endif
#ifdef BB_CHMOD_CHOWN_CHGRP
	{"chown", chmod_chown_chgrp_main, _BB_DIR_BIN, chown_usage},
#endif
#ifdef BB_CHROOT
	{"chroot", chroot_main, _BB_DIR_USR_SBIN, chroot_usage},
#endif
#ifdef BB_CHVT
	{"chvt", chvt_main, _BB_DIR_USR_BIN, chvt_usage},
#endif
#ifdef BB_CLEAR
	{"clear", clear_main, _BB_DIR_USR_BIN, clear_usage},
#endif
#ifdef BB_CMP
	{"cmp", cmp_main, _BB_DIR_USR_BIN, cmp_usage},
#endif
#ifdef BB_CP_MV
	{"cp", cp_mv_main, _BB_DIR_BIN, cp_usage},
#endif
#ifdef BB_CUT
	{"cut", cut_main, _BB_DIR_USR_BIN, cut_usage},
#endif
#ifdef BB_DATE
	{"date", date_main, _BB_DIR_BIN, date_usage},
#endif
#ifdef BB_DC
	{"dc", dc_main, _BB_DIR_USR_BIN, dc_usage},
#endif
#ifdef BB_DD
	{"dd", dd_main, _BB_DIR_BIN, dd_usage},
#endif
#ifdef BB_DEALLOCVT
	{"deallocvt", deallocvt_main, _BB_DIR_USR_BIN, deallocvt_usage},
#endif
#ifdef BB_DF
	{"df", df_main, _BB_DIR_BIN, df_usage},
#endif
#ifdef BB_DIRNAME
	{"dirname", dirname_main, _BB_DIR_USR_BIN, dirname_usage},
#endif
#ifdef BB_DMESG
	{"dmesg", dmesg_main, _BB_DIR_BIN, dmesg_usage},
#endif
#ifdef BB_DOS2UNIX
	{"dos2unix", dos2unix_main, _BB_DIR_USR_BIN, dos2unix_usage},
#endif
#ifdef BB_DU
	{"du", du_main, _BB_DIR_USR_BIN, du_usage},
#endif
#ifdef BB_DUMPKMAP
	{"dumpkmap", dumpkmap_main, _BB_DIR_BIN, dumpkmap_usage},
#endif
#ifdef BB_DUTMP
	{"dutmp", dutmp_main, _BB_DIR_USR_SBIN, dutmp_usage},
#endif
#ifdef BB_ECHO
	{"echo", echo_main, _BB_DIR_BIN, echo_usage},
#endif
#ifdef BB_EXPR
	{"expr", expr_main, _BB_DIR_USR_BIN, expr_usage},
#endif
#ifdef BB_TRUE_FALSE
	{"false", false_main, _BB_DIR_BIN, false_usage},
#endif
#ifdef BB_FBSET
	{"fbset", fbset_main, _BB_DIR_USR_SBIN, NULL},
#endif
#ifdef BB_FDFLUSH
	{"fdflush", fdflush_main, _BB_DIR_BIN, fdflush_usage},
#endif
#ifdef BB_FIND
	{"find", find_main, _BB_DIR_USR_BIN, find_usage},
#endif
#ifdef BB_FREE
	{"free", free_main, _BB_DIR_USR_BIN, free_usage},
#endif
#ifdef BB_FREERAMDISK
	{"freeramdisk", freeramdisk_main, _BB_DIR_SBIN, freeramdisk_usage},
#endif
#ifdef BB_FSCK_MINIX
	{"fsck.minix", fsck_minix_main, _BB_DIR_SBIN, fsck_minix_usage},
#endif
#ifdef BB_GETOPT
	{"getopt", getopt_main, _BB_DIR_BIN, getopt_usage},
#endif
#ifdef BB_GREP
	{"grep", grep_main, _BB_DIR_BIN, grep_usage},
#endif
#ifdef BB_GUNZIP
	{"gunzip", gunzip_main, _BB_DIR_BIN, gunzip_usage},
#endif
#ifdef BB_GZIP
	{"gzip", gzip_main, _BB_DIR_BIN, gzip_usage},
#endif
#ifdef BB_HALT
	{"halt", halt_main, _BB_DIR_SBIN, halt_usage},
#endif
#ifdef BB_HEAD
	{"head", head_main, _BB_DIR_USR_BIN, head_usage},
#endif
#ifdef BB_HOSTID
	{"hostid", hostid_main, _BB_DIR_USR_BIN, hostid_usage},
#endif
#ifdef BB_HOSTNAME
	{"hostname", hostname_main, _BB_DIR_BIN, hostname_usage},
#endif
#ifdef BB_ID
	{"id", id_main, _BB_DIR_USR_BIN, id_usage},
#endif
#ifdef BB_INIT
	{"init", init_main, _BB_DIR_SBIN, NULL},
#endif
#ifdef BB_INSMOD
	{"insmod", insmod_main, _BB_DIR_SBIN, insmod_usage},
#endif
#ifdef BB_KILL
	{"kill", kill_main, _BB_DIR_BIN, kill_usage},
#endif
#ifdef BB_KILLALL
	{"killall", kill_main, _BB_DIR_USR_BIN, kill_usage},
#endif
#ifdef BB_LENGTH
	{"length", length_main, _BB_DIR_USR_BIN, length_usage},
#endif
#ifdef BB_LINUXRC
	{"linuxrc", init_main, _BB_DIR_ROOT, NULL},
#endif
#ifdef BB_LN
	{"ln", ln_main, _BB_DIR_BIN, ln_usage},
#endif
#ifdef BB_LOADACM
	{"loadacm", loadacm_main, _BB_DIR_USR_BIN, loadacm_usage},
#endif
#ifdef BB_LOADFONT
	{"loadfont", loadfont_main, _BB_DIR_USR_BIN, loadfont_usage},
#endif
#ifdef BB_LOADKMAP
	{"loadkmap", loadkmap_main, _BB_DIR_SBIN, loadkmap_usage},
#endif
#ifdef BB_LOGGER
	{"logger", logger_main, _BB_DIR_USR_BIN, logger_usage},
#endif
#ifdef BB_LOGNAME
	{"logname", logname_main, _BB_DIR_USR_BIN, logname_usage},
#endif
#ifdef BB_LS
	{"ls", ls_main, _BB_DIR_BIN, ls_usage},
#endif
#ifdef BB_LSMOD
	{"lsmod", lsmod_main, _BB_DIR_SBIN, lsmod_usage},
#endif
#ifdef BB_MAKEDEVS
	{"makedevs", makedevs_main, _BB_DIR_SBIN, makedevs_usage},
#endif
#ifdef BB_MD5SUM
	{"md5sum", md5sum_main, _BB_DIR_USR_BIN, md5sum_usage},
#endif
#ifdef BB_MKDIR
	{"mkdir", mkdir_main, _BB_DIR_BIN, mkdir_usage},
#endif
#ifdef BB_MKFIFO
	{"mkfifo", mkfifo_main, _BB_DIR_USR_BIN, mkfifo_usage},
#endif
#ifdef BB_MKFS_MINIX
	{"mkfs.minix", mkfs_minix_main, _BB_DIR_SBIN, mkfs_minix_usage},
#endif
#ifdef BB_MKNOD
	{"mknod", mknod_main, _BB_DIR_BIN, mknod_usage},
#endif
#ifdef BB_MKSWAP
	{"mkswap", mkswap_main, _BB_DIR_SBIN, mkswap_usage},
#endif
#ifdef BB_MKTEMP
	{"mktemp", mktemp_main, _BB_DIR_BIN, mktemp_usage},
#endif
#ifdef BB_MORE
	{"more", more_main, _BB_DIR_BIN, more_usage},
#endif
#ifdef BB_MOUNT
	{"mount", mount_main, _BB_DIR_BIN, mount_usage},
#endif
#ifdef BB_MT
	{"mt", mt_main, _BB_DIR_BIN, mt_usage},
#endif
#ifdef BB_CP_MV
	{"mv", cp_mv_main, _BB_DIR_BIN, mv_usage},
#endif
#ifdef BB_NC
	{"nc", nc_main, _BB_DIR_USR_BIN, nc_usage},
#endif
#ifdef BB_NSLOOKUP
	{"nslookup", nslookup_main, _BB_DIR_USR_BIN, nslookup_usage},
#endif
#ifdef BB_PING
	{"ping", ping_main, _BB_DIR_BIN, ping_usage},
#endif
#ifdef BB_POWEROFF
	{"poweroff", poweroff_main, _BB_DIR_SBIN, poweroff_usage},
#endif
#ifdef BB_PRINTF
	{"printf", printf_main, _BB_DIR_USR_BIN, printf_usage},
#endif
#ifdef BB_PS
	{"ps", ps_main, _BB_DIR_BIN, ps_usage},
#endif
#ifdef BB_PWD
	{"pwd", pwd_main, _BB_DIR_BIN, pwd_usage},
#endif
#ifdef BB_RDATE
	{"rdate", rdate_main, _BB_DIR_USR_BIN, rdate_usage},
#endif
#ifdef BB_READLINK
	{"readlink", readlink_main, _BB_DIR_USR_BIN, readlink_usage},
#endif
#ifdef BB_REBOOT
	{"reboot", reboot_main, _BB_DIR_SBIN, reboot_usage},
#endif
#ifdef BB_RENICE
	{"renice", renice_main, _BB_DIR_USR_BIN, renice_usage},
#endif
#ifdef BB_RESET
	{"reset", reset_main, _BB_DIR_USR_BIN, reset_usage},
#endif
#ifdef BB_RM
	{"rm", rm_main, _BB_DIR_BIN, rm_usage},
#endif
#ifdef BB_RMDIR
	{"rmdir", rmdir_main, _BB_DIR_BIN, rmdir_usage},
#endif
#ifdef BB_RMMOD
	{"rmmod", rmmod_main, _BB_DIR_SBIN, rmmod_usage},
#endif
#ifdef BB_SED
	{"sed", sed_main, _BB_DIR_BIN, sed_usage},
#endif
#ifdef BB_SETKEYCODES
	{"setkeycodes", setkeycodes_main, _BB_DIR_USR_BIN, setkeycodes_usage},
#endif
#ifdef BB_SH
	{"sh", shell_main, _BB_DIR_BIN, shell_usage},
#endif
#ifdef BB_SLEEP
	{"sleep", sleep_main, _BB_DIR_BIN, sleep_usage},
#endif
#ifdef BB_SORT
	{"sort", sort_main, _BB_DIR_USR_BIN, sort_usage},
#endif
#ifdef BB_SWAPONOFF
	{"swapoff", swap_on_off_main, _BB_DIR_SBIN, swapoff_usage},
#endif
#ifdef BB_SWAPONOFF
	{"swapon", swap_on_off_main, _BB_DIR_SBIN, swapon_usage},
#endif
#ifdef BB_SYNC
	{"sync", sync_main, _BB_DIR_BIN, sync_usage},
#endif
#ifdef BB_SYSLOGD
	{"syslogd", syslogd_main, _BB_DIR_SBIN, syslogd_usage},
#endif
#ifdef BB_TAIL
	{"tail", tail_main, _BB_DIR_USR_BIN, tail_usage},
#endif
#ifdef BB_TAR
	{"tar", tar_main, _BB_DIR_BIN, tar_usage},
#endif
#ifdef BB_TEE
	{"tee", tee_main, _BB_DIR_USR_BIN, tee_usage},
#endif
#ifdef BB_TELNET
	{"telnet", telnet_main, _BB_DIR_USR_BIN, telnet_usage},
#endif
#ifdef BB_TEST
	{"test", test_main, _BB_DIR_USR_BIN, test_usage},
#endif
#ifdef BB_TOUCH
	{"touch", touch_main, _BB_DIR_BIN, touch_usage},
#endif
#ifdef BB_TR
	{"tr", tr_main, _BB_DIR_USR_BIN, tr_usage},
#endif
#ifdef BB_TRUE_FALSE
	{"true", true_main, _BB_DIR_BIN, true_usage},
#endif
#ifdef BB_TTY
	{"tty", tty_main, _BB_DIR_USR_BIN, tty_usage},
#endif
#ifdef BB_UMOUNT
	{"umount", umount_main, _BB_DIR_BIN, umount_usage},
#endif
#ifdef BB_UNAME
	{"uname", uname_main, _BB_DIR_BIN, uname_usage},
#endif
#ifdef BB_UNIQ
	{"uniq", uniq_main, _BB_DIR_USR_BIN, uniq_usage},
#endif
#ifdef BB_UNIX2DOS
	{"unix2dos", unix2dos_main, _BB_DIR_USR_BIN, unix2dos_usage},
#endif
#ifdef BB_UNRPM
	{"unrpm", unrpm_main, _BB_DIR_USR_BIN, unrpm_usage},
#endif
#ifdef BB_UPDATE
	{"update", update_main, _BB_DIR_SBIN, update_usage},
#endif
#ifdef BB_UPTIME
	{"uptime", uptime_main, _BB_DIR_USR_BIN, uptime_usage},
#endif
#ifdef BB_USLEEP
	{"usleep", usleep_main, _BB_DIR_BIN, usleep_usage},
#endif
#ifdef BB_UUDECODE
	{"uudecode", uudecode_main, _BB_DIR_USR_BIN, uudecode_usage},
#endif
#ifdef BB_UUENCODE
	{"uuencode", uuencode_main, _BB_DIR_USR_BIN, uuencode_usage},
#endif
#ifdef BB_WC
	{"wc", wc_main, _BB_DIR_USR_BIN, wc_usage},
#endif
#ifdef BB_WGET
	{"wget", wget_main, _BB_DIR_USR_BIN, wget_usage},
#endif
#ifdef BB_WHICH
	{"which", which_main, _BB_DIR_USR_BIN, which_usage},
#endif
#ifdef BB_WHOAMI
	{"whoami", whoami_main, _BB_DIR_USR_BIN, whoami_usage},
#endif
#ifdef BB_XARGS
	{"xargs", xargs_main, _BB_DIR_USR_BIN, xargs_usage},
#endif
#ifdef BB_YES
	{"yes", yes_main, _BB_DIR_USR_BIN, yes_usage},
#endif
#ifdef BB_GUNZIP
	{"zcat", gunzip_main, _BB_DIR_BIN, gunzip_usage},
#endif
	{0,NULL,0,NULL}
};

/* The -1 arises because of the {0,NULL,0,NULL} entry above. */
#define NUM_APPLETS (sizeof (applets) / sizeof (struct BB_applet) - 1)

