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

#undef APPLET
#undef APPLET_ODDNAME
#undef APPLET_NOUSAGE


#if defined(PROTOTYPES)
  #define APPLET(a,b,c) extern int b(int argc, char **argv);
  #define APPLET_NOUSAGE(a,b,c) extern int b(int argc, char **argv);
  #define APPLET_ODDNAME(a,b,c,d) extern int b(int argc, char **argv);
  extern const char usage_messages[];
#elif defined(MAKE_USAGE)
  #ifdef BB_FEATURE_VERBOSE_USAGE
    #define APPLET(a,b,c) a##_trivial_usage "\n\n" a##_full_usage "\0"
    #define APPLET_NOUSAGE(a,b,c) "\0"
    #define APPLET_ODDNAME(a,b,c,d) d##_trivial_usage "\n\n" d##_full_usage "\0"
  #else
    #define APPLET(a,b,c) a##_trivial_usage "\0"
    #define APPLET_NOUSAGE(a,b,c) "\0"
    #define APPLET_ODDNAME(a,b,c,d) d##_trivial_usage "\0"
  #endif
#elif defined(MAKE_LINKS)
#  define APPLET(a,b,c) LINK c a
#  define APPLET_NOUSAGE(a,b,c) LINK c a
#  define APPLET_ODDNAME(a,b,c,d) LINK c a
#else
  const struct BB_applet applets[] = {
  #define APPLET(a,b,c) {#a,b,c},
  #define APPLET_NOUSAGE(a,b,c) {a,b,c},
  #define APPLET_ODDNAME(a,b,c,d) {a,b,c},
#endif



#ifdef BB_TEST
	APPLET_NOUSAGE("[", test_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_ADJTIMEX
	APPLET(adjtimex, adjtimex_main, _BB_DIR_SBIN)
#endif
#ifdef BB_AR
	APPLET(ar, ar_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_ASH
	APPLET_NOUSAGE("ash", ash_main, _BB_DIR_BIN)
#endif
#ifdef BB_BASENAME
	APPLET(basename, basename_main, _BB_DIR_USR_BIN)
#endif
	APPLET_NOUSAGE("busybox", busybox_main, _BB_DIR_BIN)
#ifdef BB_CAT
	APPLET(cat, cat_main, _BB_DIR_BIN)
#endif
#ifdef BB_CHGRP
	APPLET(chgrp, chgrp_main, _BB_DIR_BIN)
#endif
#ifdef BB_CHMOD
	APPLET(chmod, chmod_main, _BB_DIR_BIN)
#endif
#ifdef BB_CHOWN
	APPLET(chown, chown_main, _BB_DIR_BIN)
#endif
#ifdef BB_CHROOT
	APPLET(chroot, chroot_main, _BB_DIR_USR_SBIN)
#endif
#ifdef BB_CHVT
	APPLET(chvt, chvt_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_CLEAR
	APPLET(clear, clear_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_CMP
	APPLET(cmp, cmp_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_CP
	APPLET(cp, cp_main, _BB_DIR_BIN)
#endif
#ifdef BB_CPIO
	APPLET(cpio, cpio_main, _BB_DIR_BIN)
#endif
#ifdef BB_CUT
	APPLET(cut, cut_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_DATE
	APPLET(date, date_main, _BB_DIR_BIN)
#endif
#ifdef BB_DC
	APPLET(dc, dc_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_DD
	APPLET(dd, dd_main, _BB_DIR_BIN)
#endif
#ifdef BB_DEALLOCVT
	APPLET(deallocvt, deallocvt_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_DF
	APPLET(df, df_main, _BB_DIR_BIN)
#endif
#ifdef BB_DIRNAME
	APPLET(dirname, dirname_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_DMESG
	APPLET(dmesg, dmesg_main, _BB_DIR_BIN)
#endif
#ifdef BB_DOS2UNIX
	APPLET(dos2unix, dos2unix_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_DPKG
	APPLET(dpkg, dpkg_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_DPKG_DEB
	APPLET_ODDNAME("dpkg-deb", dpkg_deb_main, _BB_DIR_USR_BIN, dpkg_deb)
#endif
#ifdef BB_DU
	APPLET(du, du_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_DUMPKMAP
	APPLET(dumpkmap, dumpkmap_main, _BB_DIR_BIN)
#endif
#ifdef BB_DUTMP
	APPLET(dutmp, dutmp_main, _BB_DIR_USR_SBIN)
#endif
#ifdef BB_ECHO
	APPLET(echo, echo_main, _BB_DIR_BIN)
#endif
#if defined(BB_FEATURE_GREP_EGREP_ALIAS) && defined(BB_GREP)
	APPLET_NOUSAGE("egrep", grep_main, _BB_DIR_BIN)
#endif
#ifdef BB_ENV
	APPLET(env, env_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_EXPR
	APPLET(expr, expr_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_TRUE_FALSE
	APPLET(false, false_main, _BB_DIR_BIN)
#endif
#ifdef BB_FBSET
	APPLET(fbset, fbset_main, _BB_DIR_USR_SBIN)
#endif
#ifdef BB_FDFLUSH
	APPLET(fdflush, fdflush_main, _BB_DIR_BIN)
#endif
#ifdef BB_FIND
	APPLET(find, find_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_FREE
	APPLET(free, free_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_FREERAMDISK
	APPLET(freeramdisk, freeramdisk_main, _BB_DIR_SBIN)
#endif
#ifdef BB_FSCK_MINIX
	APPLET_ODDNAME("fsck.minix", fsck_minix_main, _BB_DIR_SBIN, fsck_minix)
#endif
#ifdef BB_GETOPT
	APPLET(getopt, getopt_main, _BB_DIR_BIN)
#endif
#ifdef BB_GREP
	APPLET(grep, grep_main, _BB_DIR_BIN)
#endif
#ifdef BB_GUNZIP
	APPLET(gunzip, gunzip_main, _BB_DIR_BIN)
#endif
#ifdef BB_GZIP
	APPLET(gzip, gzip_main, _BB_DIR_BIN)
#endif
#ifdef BB_HALT
	APPLET(halt, halt_main, _BB_DIR_SBIN)
#endif
#ifdef BB_HEAD
	APPLET(head, head_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_HOSTID
	APPLET(hostid, hostid_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_HOSTNAME
	APPLET(hostname, hostname_main, _BB_DIR_BIN)
#endif
#ifdef BB_HUSH
	APPLET_NOUSAGE("hush", hush_main, _BB_DIR_BIN)
#endif
#ifdef BB_ID
	APPLET(id, id_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_IFCONFIG
	APPLET(ifconfig, ifconfig_main, _BB_DIR_SBIN)
#endif
#ifdef BB_INIT
	APPLET(init, init_main, _BB_DIR_SBIN)
#endif
#ifdef BB_INSMOD
	APPLET(insmod, insmod_main, _BB_DIR_SBIN)
#endif
#ifdef BB_KILL
	APPLET(kill, kill_main, _BB_DIR_BIN)
#endif
#ifdef BB_KILLALL
	APPLET(killall, kill_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_KLOGD
	APPLET(klogd, klogd_main, _BB_DIR_SBIN)
#endif
#ifdef BB_LASH
	APPLET(lash, lash_main, _BB_DIR_BIN)
#endif
#ifdef BB_LENGTH
	APPLET(length, length_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_FEATURE_LINUXRC
	APPLET_NOUSAGE("linuxrc", init_main, _BB_DIR_ROOT)
#endif
#ifdef BB_LN
	APPLET(ln, ln_main, _BB_DIR_BIN)
#endif
#ifdef BB_LOADACM
	APPLET(loadacm, loadacm_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_LOADFONT
	APPLET(loadfont, loadfont_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_LOADKMAP
	APPLET(loadkmap, loadkmap_main, _BB_DIR_SBIN)
#endif
#ifdef BB_LOGGER
	APPLET(logger, logger_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_LOGNAME
	APPLET(logname, logname_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_LOGREAD
	APPLET(logread, logread_main, _BB_DIR_SBIN)
#endif
#ifdef BB_LOSETUP
	APPLET(losetup, losetup_main, _BB_DIR_SBIN)
#endif
#ifdef BB_LS
	APPLET(ls, ls_main, _BB_DIR_BIN)
#endif
#ifdef BB_LSMOD
	APPLET(lsmod, lsmod_main, _BB_DIR_SBIN)
#endif
#ifdef BB_MAKEDEVS
	APPLET(makedevs, makedevs_main, _BB_DIR_SBIN)
#endif
#ifdef BB_MD5SUM
	APPLET(md5sum, md5sum_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_MKDIR
	APPLET(mkdir, mkdir_main, _BB_DIR_BIN)
#endif
#ifdef BB_MKFIFO
	APPLET(mkfifo, mkfifo_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_MKFS_MINIX
	APPLET_ODDNAME("mkfs.minix", mkfs_minix_main, _BB_DIR_SBIN, mkfs_minix)
#endif
#ifdef BB_MKNOD
	APPLET(mknod, mknod_main, _BB_DIR_BIN)
#endif
#ifdef BB_MKSWAP
	APPLET(mkswap, mkswap_main, _BB_DIR_SBIN)
#endif
#ifdef BB_MKTEMP
	APPLET(mktemp, mktemp_main, _BB_DIR_BIN)
#endif
#ifdef BB_MODPROBE
	APPLET(modprobe, modprobe_main, _BB_DIR_SBIN)
#endif
#ifdef BB_MORE
	APPLET(more, more_main, _BB_DIR_BIN)
#endif
#ifdef BB_MOUNT
	APPLET(mount, mount_main, _BB_DIR_BIN)
#endif
#ifdef BB_MSH
	APPLET_NOUSAGE("msh", msh_main, _BB_DIR_BIN)
#endif
#ifdef BB_MT
	APPLET(mt, mt_main, _BB_DIR_BIN)
#endif
#ifdef BB_MV
	APPLET(mv, mv_main, _BB_DIR_BIN)
#endif
#ifdef BB_NC
	APPLET(nc, nc_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_NSLOOKUP
	APPLET(nslookup, nslookup_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_PIDOF
	APPLET(pidof, pidof_main, _BB_DIR_BIN)
#endif
#ifdef BB_PING
	APPLET(ping, ping_main, _BB_DIR_BIN)
#endif
#ifdef BB_PIVOT_ROOT
 	APPLET(pivot_root, pivot_root_main, _BB_DIR_SBIN)
#endif
#ifdef BB_POWEROFF
	APPLET(poweroff, poweroff_main, _BB_DIR_SBIN)
#endif
#ifdef BB_PRINTF
	APPLET(printf, printf_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_PS
	APPLET(ps, ps_main, _BB_DIR_BIN)
#endif
#ifdef BB_PWD
	APPLET(pwd, pwd_main, _BB_DIR_BIN)
#endif
#ifdef BB_RDATE
	APPLET(rdate, rdate_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_READLINK
	APPLET(readlink, readlink_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_REBOOT
	APPLET(reboot, reboot_main, _BB_DIR_SBIN)
#endif
#ifdef BB_RENICE
	APPLET(renice, renice_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_RESET
	APPLET(reset, reset_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_RM
	APPLET(rm, rm_main, _BB_DIR_BIN)
#endif
#ifdef BB_RMDIR
	APPLET(rmdir, rmdir_main, _BB_DIR_BIN)
#endif
#ifdef BB_RMMOD
	APPLET(rmmod, rmmod_main, _BB_DIR_SBIN)
#endif
#ifdef BB_ROUTE
 	APPLET(route, route_main, _BB_DIR_SBIN)
#endif
#ifdef BB_RPM2CPIO
	APPLET(rpm2cpio, rpm2cpio_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_SED
	APPLET(sed, sed_main, _BB_DIR_BIN)
#endif
#ifdef BB_SETKEYCODES
	APPLET(setkeycodes, setkeycodes_main, _BB_DIR_USR_BIN)
#endif
#if defined(BB_FEATURE_SH_IS_ASH) && defined(BB_ASH)
	APPLET_NOUSAGE("sh", ash_main, _BB_DIR_BIN)
#elif defined(BB_FEATURE_SH_IS_HUSH) && defined(BB_HUSH)
	APPLET_NOUSAGE("sh", hush_main, _BB_DIR_BIN)
#elif defined(BB_FEATURE_SH_IS_LASH) && defined(BB_LASH)
	APPLET_NOUSAGE("sh", lash_main, _BB_DIR_BIN)
#elif defined(BB_FEATURE_SH_IS_MSH) && defined(BB_MSH)
	APPLET_NOUSAGE("sh", msh_main, _BB_DIR_BIN)
#endif
#ifdef BB_SLEEP
	APPLET(sleep, sleep_main, _BB_DIR_BIN)
#endif
#ifdef BB_SORT
	APPLET(sort, sort_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_STTY
	APPLET(stty, stty_main, _BB_DIR_BIN)
#endif
#ifdef BB_SWAPONOFF
	APPLET(swapoff, swap_on_off_main, _BB_DIR_SBIN)
#endif
#ifdef BB_SWAPONOFF
	APPLET(swapon, swap_on_off_main, _BB_DIR_SBIN)
#endif
#ifdef BB_SYNC
	APPLET(sync, sync_main, _BB_DIR_BIN)
#endif
#ifdef BB_SYSLOGD
	APPLET(syslogd, syslogd_main, _BB_DIR_SBIN)
#endif
#ifdef BB_TAIL
	APPLET(tail, tail_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_TAR
	APPLET(tar, tar_main, _BB_DIR_BIN)
#endif
#ifdef BB_TEE
	APPLET(tee, tee_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_TELNET
	APPLET(telnet, telnet_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_TEST
	APPLET(test, test_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_TFTP
	APPLET(tftp, tftp_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_TIME
	APPLET(time, time_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_TOP
	APPLET(top, top_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_TOUCH
	APPLET(touch, touch_main, _BB_DIR_BIN)
#endif
#ifdef BB_TR
	APPLET(tr, tr_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_TRACEROUTE
	APPLET(traceroute, traceroute_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_TRUE_FALSE
	APPLET(true, true_main, _BB_DIR_BIN)
#endif
#ifdef BB_TTY
	APPLET(tty, tty_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_UMOUNT
	APPLET(umount, umount_main, _BB_DIR_BIN)
#endif
#ifdef BB_UNAME
	APPLET(uname, uname_main, _BB_DIR_BIN)
#endif
#ifdef BB_UNIQ
	APPLET(uniq, uniq_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_UNIX2DOS
	APPLET(unix2dos, dos2unix_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_UPDATE
	APPLET(update, update_main, _BB_DIR_SBIN)
#endif
#ifdef BB_UPTIME
	APPLET(uptime, uptime_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_USLEEP
	APPLET(usleep, usleep_main, _BB_DIR_BIN)
#endif
#ifdef BB_UUDECODE
	APPLET(uudecode, uudecode_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_UUENCODE
	APPLET(uuencode, uuencode_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_VI
	APPLET(vi, vi_main, _BB_DIR_BIN)
#endif
#ifdef BB_WATCHDOG
	APPLET(watchdog, watchdog_main, _BB_DIR_SBIN)
#endif
#ifdef BB_WC
	APPLET(wc, wc_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_WGET
	APPLET(wget, wget_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_WHICH
	APPLET(which, which_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_WHOAMI
	APPLET(whoami, whoami_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_XARGS
	APPLET(xargs, xargs_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_YES
	APPLET(yes, yes_main, _BB_DIR_USR_BIN)
#endif
#ifdef BB_GUNZIP
	APPLET(zcat, gunzip_main, _BB_DIR_BIN)
#endif

#if !defined(PROTOTYPES) && !defined(MAKE_USAGE)
	{ 0,NULL,0 }
};

#endif
