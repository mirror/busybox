// This file defines the feature set to be compiled into busybox.
// When you turn things off here, they won't be compiled in at all.
//
//// This file is parsed by sed. You MUST use single line comments.
//   i.e.  //#define BB_BLAH
//
//
// BusyBox Applications
#define BB_BUSYBOX
#define BB_CAT
#define BB_CHMOD_CHOWN_CHGRP
#define BB_CHROOT
#define BB_CHVT
#define BB_CLEAR
#define BB_CP
#define BB_DATE
#define BB_DD
#define BB_DEALLOCVT
#define BB_DF
#define BB_DMESG
//#define BB_DUTMP
#define BB_DU
#define BB_FBSET
//#define BB_FDFLUSH
#define BB_FIND
#define BB_FREE
#define BB_FSCK_MINIX
#define BB_GREP
#define BB_GUNZIP
#define BB_GZIP
//#define BB_HALT
#define BB_HEAD
//#define BB_HOSTID
#define BB_HOSTNAME
#define BB_INIT
// Don't turn BB_INSMOD on.  It doesn't work.
//#define BB_INSMOD
#define BB_KILL
#define BB_KLOGD
//#define BB_LENGTH
#define BB_LN
//#define BB_LOADACM
//#define BB_LOADFONT
//#define BB_LOADKMAP
//#define BB_LOGGER
#define BB_LOGNAME
#define BB_LS
#define BB_LSMOD
//#define BB_MAKEDEVS
#define BB_MKFS_MINIX
//#define BB_MATH
#define BB_MKDIR
//#define BB_MKFIFO
#define BB_MKNOD
#define BB_MKSWAP
//#define BB_MNC
#define BB_MORE
#define BB_MOUNT
#define BB_NFSMOUNT
//#define BB_MT
//#define BB_MTAB
#define BB_MV
#define BB_PING
#define BB_POWEROFF
//#define BB_PRINTF
#define BB_PS
#define BB_PWD
#define BB_REBOOT
#define BB_REGEXP
#define BB_RM
#define BB_RMDIR
//#define BB_RMMOD
#define BB_SED
//#define BB_SFDISK
#define BB_SLEEP
#define BB_SORT
#define BB_SWAPONOFF
#define BB_SYNC
#define BB_SYSLOGD
#define BB_TAIL
#define BB_TAR
#define BB_TEE
#define BB_TOUCH
#define BB_TRUE_FALSE
#define BB_TTY
#define BB_WC
#define BB_WHOAMI
#define BB_UMOUNT
#define BB_UNIQ
#define BB_UNAME
#define BB_UPDATE
#define BB_YES
// End of Applications List
//
//
//
// Don't turn BB_UTILITY off.  It contains support code 
// that compiles to 0 if everything else if turned off.
#define BB_UTILITY
//
//
//
// This is where feature definitions go.  Generally speaking,
// turning this stuff off makes things a bit smaller (and less 
// pretty/useful).
//
//
// enable features that use the /proc filesystem (apps that 
// break without this will tell you on compile)...
#define BB_FEATURE_USE_PROCFS
//
// Use termios to manipulate the screen ('more' is prettier with this on)
#define BB_FEATURE_USE_TERMIOS
//
// calculate terminal & column widths (for more and ls)
#define BB_FEATURE_AUTOWIDTH
//
// show username/groupnames (bypasses libc6 NSS) for ls
#define BB_FEATURE_LS_USERNAME	
//
// show file timestamps in ls
#define BB_FEATURE_LS_TIMESTAMPS
//
// enable ls -p and -F
#define BB_FEATURE_LS_FILETYPES	
//
// Change ping implementation -- simplified, featureless, but really small.
//#define BB_SIMPLE_PING
////
// Make init use a simplified /etc/inittab file (recommended).
#define BB_FEATURE_USE_INITTAB
//
//Enable init being called as /linuxrc
#define BB_FEATURE_LINUXRC
//
// Enable support for loop devices in mount
#define BB_FEATURE_MOUNT_LOOP
//
// Enable support for creation of tar files.
//#define BB_FEATURE_TAR_CREATE
//
// Allow init to permenently chroot, and umount the old root fs
// just like an initrd does.  Requires a kernel patch by Werner Almesberger.
// ftp://icaftp.epfl.ch/pub/people/almesber/misc/umount-root-*.tar.gz
#ifdef BB_MOUNT
#define BB_FEATURE_INIT_CHROOT
#endif
//
