/*
 * This file is parsed by sed. You MUST use single line comments.
 * IE	//#define BB_BLAH
 */

#define BB_BUSYBOX
#define BB_CAT
#define BB_CHMOD_CHOWN_CHGRP
#define BB_CHROOT
#define BB_CLEAR
#define BB_CP
#define BB_DATE
#define BB_DD
#define BB_DF
#define BB_DMESG
//#define BB_DUTMP
//#define BB_FDFLUSH
#define BB_FIND
#define BB_FSCK_MINIX
#define BB_MKFS_MINIX
#define BB_CHVT
#define BB_DEALLOCVT
#define BB_GREP
//#define BB_HALT
#define BB_INIT
#define BB_KILL
//#define BB_LENGTH
#define BB_LN
#define BB_LOADFONT
#define BB_LOADKMAP
#define BB_LS
//#define BB_MAKEDEVS
//#define BB_MATH
#define BB_MKDIR
#define BB_MKNOD
#define BB_MKSWAP
//#define BB_MNC
#define BB_MORE
#define BB_MOUNT
#define BB_NFSMOUNT
//#define BB_MT
//#define BB_MTAB
#define BB_MV
//#define BB_PRINTF
#define BB_PS
#define BB_PWD
#define BB_REGEXP
#define BB_REBOOT
#define BB_RM
#define BB_RMDIR
//#define BB_SFDISK
#define BB_SED
#define BB_SLEEP
#define BB_SWAPONOFF
#define BB_SYNC
#define BB_TAR
#define BB_TOUCH
#define BB_TRUE_FALSE
#define BB_UMOUNT
#define BB_UPDATE
#define BB_UNAME
#define BB_ZCAT
#define BB_GZIP
// Don't turn BB_UTILITY off.  It contains support code 
// that compiles to 0 if everything else if turned off.
#define BB_UTILITY
