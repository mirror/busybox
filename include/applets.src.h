/* vi: set sw=4 ts=4: */
/*
 * applets.h - a listing of all busybox applets.
 *
 * If you write a new applet, you need to add an entry to this list to make
 * busybox aware of it.
 */

/*
name  - applet name as it is typed on command line
help  - applet name, converted to C (ether-wake: help = ether_wake)
main  - corresponding <applet>_main to call (bzcat: main = bunzip2)
l     - location to install link to: [/usr]/[s]bin
s     - suid type:
        BB_SUID_REQUIRE: will complain if busybox isn't suid
        and is run by non-root (applet_main() will not be called at all)
        BB_SUID_DROP: will drop suid prior to applet_main()
        BB_SUID_MAYBE: neither of the above
        (every instance of BB_SUID_REQUIRE and BB_SUID_MAYBE
        needs to be justified in comment)
        NB: please update FEATURE_SUID help text whenever you add/remove
        BB_SUID_REQUIRE or BB_SUID_MAYBE applet.
*/

#if defined(PROTOTYPES)
# define APPLET(name,l,s)                    int name##_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
# define APPLET_ODDNAME(name,main,l,s,help)  int main##_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
# define APPLET_NOEXEC(name,main,l,s,help)   int main##_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
# define APPLET_NOFORK(name,main,l,s,help)   int main##_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;

#elif defined(NAME_MAIN)
# define APPLET(name,l,s)                    name name##_main
# define APPLET_ODDNAME(name,main,l,s,help)  name main##_main
# define APPLET_NOEXEC(name,main,l,s,help)   name main##_main
# define APPLET_NOFORK(name,main,l,s,help)   name main##_main

#elif defined(MAKE_USAGE) && ENABLE_FEATURE_VERBOSE_USAGE
# define APPLET(name,l,s)                    MAKE_USAGE(#name, name##_trivial_usage name##_full_usage)
# define APPLET_ODDNAME(name,main,l,s,help)  MAKE_USAGE(#name, help##_trivial_usage help##_full_usage)
# define APPLET_NOEXEC(name,main,l,s,help)   MAKE_USAGE(#name, help##_trivial_usage help##_full_usage)
# define APPLET_NOFORK(name,main,l,s,help)   MAKE_USAGE(#name, help##_trivial_usage help##_full_usage)

#elif defined(MAKE_USAGE) && !ENABLE_FEATURE_VERBOSE_USAGE
# define APPLET(name,l,s)                    MAKE_USAGE(#name, name##_trivial_usage)
# define APPLET_ODDNAME(name,main,l,s,help)  MAKE_USAGE(#name, help##_trivial_usage)
# define APPLET_NOEXEC(name,main,l,s,help)   MAKE_USAGE(#name, help##_trivial_usage)
# define APPLET_NOFORK(name,main,l,s,help)   MAKE_USAGE(#name, help##_trivial_usage)

#elif defined(MAKE_LINKS)
# define APPLET(name,l,c)                    LINK l name
# define APPLET_ODDNAME(name,main,l,s,help)  LINK l name
# define APPLET_NOEXEC(name,main,l,s,help)   LINK l name
# define APPLET_NOFORK(name,main,l,s,help)   LINK l name

#elif defined(MAKE_SUID)
# define APPLET(name,l,s)                    SUID s l name
# define APPLET_ODDNAME(name,main,l,s,help)  SUID s l name
# define APPLET_NOEXEC(name,main,l,s,help)   SUID s l name
# define APPLET_NOFORK(name,main,l,s,help)   SUID s l name

#else
  static struct bb_applet applets[] = { /*    name, main, location, need_suid */
# define APPLET(name,l,s)                    { #name, #name, l, s },
# define APPLET_ODDNAME(name,main,l,s,help)  { #name, #main, l, s },
# define APPLET_NOEXEC(name,main,l,s,help)   { #name, #main, l, s, 1 },
# define APPLET_NOFORK(name,main,l,s,help)   { #name, #main, l, s, 1, 1 },
#endif

#if ENABLE_INSTALL_NO_USR
# define BB_DIR_USR_BIN BB_DIR_BIN
# define BB_DIR_USR_SBIN BB_DIR_SBIN
#endif


INSERT
IF_TEST(APPLET_NOFORK([,  test, BB_DIR_USR_BIN, BB_SUID_DROP, test))
IF_TEST(APPLET_NOFORK([[, test, BB_DIR_USR_BIN, BB_SUID_DROP, test))
IF_BASENAME(APPLET_NOFORK(basename, basename, BB_DIR_USR_BIN, BB_SUID_DROP, basename))
IF_CAL(APPLET(cal, BB_DIR_USR_BIN, BB_SUID_DROP))
IF_CAT(APPLET_NOFORK(cat, cat, BB_DIR_BIN, BB_SUID_DROP, cat))
IF_CATV(APPLET(catv, BB_DIR_BIN, BB_SUID_DROP))
IF_CHCON(APPLET(chcon, BB_DIR_USR_BIN, BB_SUID_DROP))
IF_CHGRP(APPLET_NOEXEC(chgrp, chgrp, BB_DIR_BIN, BB_SUID_DROP, chgrp))
IF_CHMOD(APPLET_NOEXEC(chmod, chmod, BB_DIR_BIN, BB_SUID_DROP, chmod))
IF_CHOWN(APPLET_NOEXEC(chown, chown, BB_DIR_BIN, BB_SUID_DROP, chown))
IF_CHROOT(APPLET(chroot, BB_DIR_USR_SBIN, BB_SUID_DROP))
IF_CKSUM(APPLET_NOEXEC(cksum, cksum, BB_DIR_USR_BIN, BB_SUID_DROP, cksum))
IF_COMM(APPLET(comm, BB_DIR_USR_BIN, BB_SUID_DROP))
IF_CP(APPLET_NOEXEC(cp, cp, BB_DIR_BIN, BB_SUID_DROP, cp))
IF_CUT(APPLET_NOEXEC(cut, cut, BB_DIR_USR_BIN, BB_SUID_DROP, cut))
IF_DD(APPLET_NOEXEC(dd, dd, BB_DIR_BIN, BB_SUID_DROP, dd))
IF_DF(APPLET(df, BB_DIR_BIN, BB_SUID_DROP))
IF_DHCPRELAY(APPLET(dhcprelay, BB_DIR_USR_SBIN, BB_SUID_DROP))
IF_DIRNAME(APPLET_NOFORK(dirname, dirname, BB_DIR_USR_BIN, BB_SUID_DROP, dirname))
IF_DOS2UNIX(APPLET_NOEXEC(dos2unix, dos2unix, BB_DIR_USR_BIN, BB_SUID_DROP, dos2unix))
IF_DU(APPLET(du, BB_DIR_USR_BIN, BB_SUID_DROP))
IF_DUMPLEASES(APPLET(dumpleases, BB_DIR_USR_BIN, BB_SUID_DROP))
//IF_E2FSCK(APPLET(e2fsck, BB_DIR_SBIN, BB_SUID_DROP))
//IF_E2LABEL(APPLET_ODDNAME(e2label, tune2fs, BB_DIR_SBIN, BB_SUID_DROP, e2label))
IF_ECHO(APPLET_NOFORK(echo, echo, BB_DIR_BIN, BB_SUID_DROP, echo))
IF_ENV(APPLET_NOEXEC(env, env, BB_DIR_USR_BIN, BB_SUID_DROP, env))
IF_EXPAND(APPLET(expand, BB_DIR_USR_BIN, BB_SUID_DROP))
IF_EXPR(APPLET(expr, BB_DIR_USR_BIN, BB_SUID_DROP))
IF_FALSE(APPLET_NOFORK(false, false, BB_DIR_BIN, BB_SUID_DROP, false))
IF_FOLD(APPLET_NOEXEC(fold, fold, BB_DIR_USR_BIN, BB_SUID_DROP, fold))
//IF_E2FSCK(APPLET_ODDNAME(fsck.ext2, e2fsck, BB_DIR_SBIN, BB_SUID_DROP, fsck_ext2))
//IF_E2FSCK(APPLET_ODDNAME(fsck.ext3, e2fsck, BB_DIR_SBIN, BB_SUID_DROP, fsck_ext3))
IF_FSYNC(APPLET_NOFORK(fsync, fsync, BB_DIR_BIN, BB_SUID_DROP, fsync))
IF_GETENFORCE(APPLET(getenforce, BB_DIR_USR_SBIN, BB_SUID_DROP))
IF_GETSEBOOL(APPLET(getsebool, BB_DIR_USR_SBIN, BB_SUID_DROP))
IF_HEAD(APPLET_NOEXEC(head, head, BB_DIR_USR_BIN, BB_SUID_DROP, head))
IF_INSTALL(APPLET(install, BB_DIR_USR_BIN, BB_SUID_DROP))
//IF_LENGTH(APPLET_NOFORK(length, length, BB_DIR_USR_BIN, BB_SUID_DROP, length))
IF_LN(APPLET_NOEXEC(ln, ln, BB_DIR_BIN, BB_SUID_DROP, ln))
IF_LOAD_POLICY(APPLET(load_policy, BB_DIR_USR_SBIN, BB_SUID_DROP))
IF_LOGNAME(APPLET_NOFORK(logname, logname, BB_DIR_USR_BIN, BB_SUID_DROP, logname))
IF_LS(APPLET_NOEXEC(ls, ls, BB_DIR_BIN, BB_SUID_DROP, ls))
IF_MATCHPATHCON(APPLET(matchpathcon, BB_DIR_USR_SBIN, BB_SUID_DROP))
IF_MKDIR(APPLET_NOFORK(mkdir, mkdir, BB_DIR_BIN, BB_SUID_DROP, mkdir))
IF_MKFIFO(APPLET_NOEXEC(mkfifo, mkfifo, BB_DIR_USR_BIN, BB_SUID_DROP, mkfifo))
IF_MKNOD(APPLET_NOEXEC(mknod, mknod, BB_DIR_BIN, BB_SUID_DROP, mknod))
IF_MV(APPLET(mv, BB_DIR_BIN, BB_SUID_DROP))
IF_NICE(APPLET(nice, BB_DIR_BIN, BB_SUID_DROP))
IF_NOHUP(APPLET(nohup, BB_DIR_USR_BIN, BB_SUID_DROP))
IF_OD(APPLET(od, BB_DIR_USR_BIN, BB_SUID_DROP))
//IF_PARSE(APPLET(parse, BB_DIR_USR_BIN, BB_SUID_DROP))
IF_PRINTENV(APPLET_NOFORK(printenv, printenv, BB_DIR_BIN, BB_SUID_DROP, printenv))
IF_PRINTF(APPLET_NOFORK(printf, printf, BB_DIR_USR_BIN, BB_SUID_DROP, printf))
IF_PWD(APPLET_NOFORK(pwd, pwd, BB_DIR_BIN, BB_SUID_DROP, pwd))
IF_READLINK(APPLET(readlink, BB_DIR_USR_BIN, BB_SUID_DROP))
IF_REALPATH(APPLET(realpath, BB_DIR_USR_BIN, BB_SUID_DROP))
IF_RESTORECON(APPLET_ODDNAME(restorecon, setfiles, BB_DIR_SBIN, BB_SUID_DROP, restorecon))
IF_RM(APPLET_NOFORK(rm, rm, BB_DIR_BIN, BB_SUID_DROP, rm))
IF_RMDIR(APPLET_NOFORK(rmdir, rmdir, BB_DIR_BIN, BB_SUID_DROP, rmdir))
IF_RUNCON(APPLET(runcon, BB_DIR_USR_BIN, BB_SUID_DROP))
IF_SELINUXENABLED(APPLET(selinuxenabled, BB_DIR_USR_SBIN, BB_SUID_DROP))
IF_SEQ(APPLET_NOFORK(seq, seq, BB_DIR_USR_BIN, BB_SUID_DROP, seq))
IF_SESTATUS(APPLET(sestatus, BB_DIR_USR_SBIN, BB_SUID_DROP))
IF_SETENFORCE(APPLET(setenforce, BB_DIR_USR_SBIN, BB_SUID_DROP))
IF_SETFILES(APPLET(setfiles, BB_DIR_SBIN, BB_SUID_DROP))
IF_SETSEBOOL(APPLET(setsebool, BB_DIR_USR_SBIN, BB_SUID_DROP))
/* Do not make this applet NOFORK. It breaks ^C-ing of pauses in shells: */
IF_SLEEP(APPLET(sleep, BB_DIR_BIN, BB_SUID_DROP))
IF_SORT(APPLET_NOEXEC(sort, sort, BB_DIR_USR_BIN, BB_SUID_DROP, sort))
IF_SPLIT(APPLET(split, BB_DIR_USR_BIN, BB_SUID_DROP))
IF_STAT(APPLET(stat, BB_DIR_BIN, BB_SUID_DROP))
IF_STTY(APPLET(stty, BB_DIR_BIN, BB_SUID_DROP))
IF_SUM(APPLET(sum, BB_DIR_USR_BIN, BB_SUID_DROP))
IF_TAC(APPLET_NOEXEC(tac, tac, BB_DIR_USR_BIN, BB_SUID_DROP, tac))
IF_TAIL(APPLET(tail, BB_DIR_USR_BIN, BB_SUID_DROP))
IF_TEE(APPLET(tee, BB_DIR_USR_BIN, BB_SUID_DROP))
IF_TEST(APPLET_NOFORK(test, test, BB_DIR_USR_BIN, BB_SUID_DROP, test))
IF_TR(APPLET(tr, BB_DIR_USR_BIN, BB_SUID_DROP))
IF_TRUE(APPLET_NOFORK(true, true, BB_DIR_BIN, BB_SUID_DROP, true))
IF_TTY(APPLET(tty, BB_DIR_USR_BIN, BB_SUID_DROP))
IF_UDHCPC(APPLET(udhcpc, BB_DIR_SBIN, BB_SUID_DROP))
IF_UDHCPD(APPLET(udhcpd, BB_DIR_USR_SBIN, BB_SUID_DROP))
IF_UNAME(APPLET(uname, BB_DIR_BIN, BB_SUID_DROP))
IF_UNEXPAND(APPLET_ODDNAME(unexpand, expand, BB_DIR_USR_BIN, BB_SUID_DROP, unexpand))
IF_UNIQ(APPLET(uniq, BB_DIR_USR_BIN, BB_SUID_DROP))
IF_UNIX2DOS(APPLET_NOEXEC(unix2dos, dos2unix, BB_DIR_USR_BIN, BB_SUID_DROP, unix2dos))
IF_USLEEP(APPLET_NOFORK(usleep, usleep, BB_DIR_BIN, BB_SUID_DROP, usleep))
IF_UUDECODE(APPLET(uudecode, BB_DIR_USR_BIN, BB_SUID_DROP))
IF_UUENCODE(APPLET(uuencode, BB_DIR_USR_BIN, BB_SUID_DROP))
IF_WC(APPLET(wc, BB_DIR_USR_BIN, BB_SUID_DROP))
IF_WHOAMI(APPLET_NOFORK(whoami, whoami, BB_DIR_USR_BIN, BB_SUID_DROP, whoami))
IF_YES(APPLET_NOFORK(yes, yes, BB_DIR_USR_BIN, BB_SUID_DROP, yes))

#if !defined(PROTOTYPES) && !defined(NAME_MAIN) && !defined(MAKE_USAGE) \
	&& !defined(MAKE_LINKS) && !defined(MAKE_SUID)
};
#endif

#undef APPLET
#undef APPLET_ODDNAME
#undef APPLET_NOEXEC
#undef APPLET_NOFORK
