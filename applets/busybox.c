#include "internal.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

static int been_there_done_that = 0;

static const struct Applet applets[] = {

#ifdef BB_BUSYBOX		//bin
    {"busybox", busybox_main},
#endif
#ifdef BB_BLOCK_DEVICE		//sbin
    {"block_device", block_device_main},
#endif
#ifdef BB_CAT			//bin
    {"cat", cat_main},
#endif
#ifdef BB_CHMOD_CHOWN_CHGRP	//bin
    {"chmod", chmod_chown_chgrp_main},
    {"chown", chmod_chown_chgrp_main},
    {"chgrp", chmod_chown_chgrp_main},
#endif
#ifdef BB_CHROOT		//sbin
    {"chroot", chroot_main},
#endif
#ifdef BB_CLEAR			//usr/bin
    {"clear", clear_main},
#endif
#ifdef BB_CP			//bin
    {"cp", cp_main},
#endif
#ifdef BB_DATE			//bin
    {"date", date_main},
#endif
#ifdef BB_DD			//bin
    {"dd", dd_main},
#endif
#ifdef BB_DF			//bin
    {"df", df_main},
#endif
#ifdef BB_DMESG			//bin
    {"dmesg", dmesg_main},
#endif
#ifdef BB_DUTMP			//usr/sbin
    {"dutmp", dutmp_main},
#endif
#ifdef BB_FDFLUSH		//bin
    {"fdflush", fdflush_main},
#endif
#ifdef BB_FIND			//usr/bin
    {"find", find_main},
#endif
#ifdef BB_CHVT			//usr/bin
    {"chvt", chvt_main},
#endif
#ifdef BB_DEALLOCVT			//usr/bin
    {"deallocvt", deallocvt_main},
#endif
#ifdef BB_FSCK_MINIX		//sbin
    {"fsck.minix", fsck_minix_main},
#endif
#ifdef BB_MKFS_MINIX		//sbin
    {"mkfs.minix", mkfs_minix_main},
#endif
#ifdef BB_GREP			//bin
    {"grep", grep_main},
#endif
#ifdef BB_HALT			//sbin
    {"halt", halt_main},
#endif
#ifdef BB_INIT			//sbin
    {"init", init_main},
#endif
#ifdef BB_KILL			//bin
    {"kill", kill_main},
#endif
#ifdef BB_LENGTH		//usr/bin
    {"length", length_main},
#endif
#ifdef BB_LN			//bin
    {"ln", ln_main},
#endif
#ifdef BB_LOADFONT		//usr/bin
    {"loadfont", loadfont_main},
#endif
#ifdef BB_LOADKMAP		//sbin
    {"loadkmap", loadkmap_main},
#endif
#ifdef BB_LS			//bin
    {"ls", ls_main},
#endif
#ifdef BB_MAKEDEVS		//sbin
    {"makedevs", makedevs_main},
#endif
#ifdef BB_MATH			//usr/bin
    {"math", math_main},
#endif
#ifdef BB_MKDIR			//bin
    {"mkdir", mkdir_main},
#endif
#ifdef BB_MKNOD			//bin
    {"mknod", mknod_main},
#endif
#ifdef BB_MKSWAP		//sbin
    {"mkswap", mkswap_main},
#endif
#ifdef BB_MNC			//usr/bin
    {"mnc", mnc_main},
#endif
#ifdef BB_MORE			//bin
    {"more", more_main},
#endif
#ifdef BB_MOUNT			//bin
    {"mount", mount_main},
#endif
#ifdef BB_MT			//bin
    {"mt", mt_main},
#endif
#ifdef BB_MV			//bin
    {"mv", mv_main},
#endif
#ifdef BB_PRINTF		//usr/bin
    {"printf", printf_main},
#endif
#ifdef BB_PS			//bin
    {"ps", ps_main},
#endif
#ifdef BB_PWD			//bin
    {"pwd", pwd_main},
#endif
#ifdef BB_REBOOT		//sbin
    {"reboot", reboot_main},
#endif
#ifdef BB_RM			//bin
    {"rm", rm_main},
#endif
#ifdef BB_RMDIR			//bin
    {"rmdir", rmdir_main},
#endif
#ifdef BB_SFDISK		//sbin
    {"fdisk", sfdisk_main},
    {"sfdisk", sfdisk_main},
#endif
#ifdef BB_SED			//bin
    {"sed", sed_main},
#endif
#ifdef BB_SLEEP			//bin
    {"sleep", sleep_main},
#endif
#ifdef BB_TAR			//bin
    {"tar", tar_main},
#endif
#ifdef BB_SWAPONOFF		//sbin
    {"swapon", swap_on_off_main},
    {"swapoff", swap_on_off_main},
#endif
#ifdef BB_SYNC			//bin
    {"sync", sync_main},
#endif
#ifdef BB_TOUCH			//usr/bin
    {"touch", touch_main},
#endif
#ifdef BB_TRUE_FALSE		//bin
    {"true", true_main},
    {"false", false_main},
#endif
#ifdef BB_UNAME			//bin
    {"uname",  uname_main},
#endif
#ifdef BB_UMOUNT		//bin
    {"umount",  umount_main},
#endif
#ifdef BB_UPDATE		//sbin
    {"update", update_main},
#endif
#ifdef BB_ZCAT			//bin
    {"zcat", zcat_main},
    {"gunzip", zcat_main},
#endif
#ifdef BB_GZIP			//bin
    {"gzip", gzip_main},
#endif
    {0}
};

int main(int argc, char **argv)
{
    char *s = argv[0];
    char *name = argv[0];
    const struct Applet *a = applets;

    while (*s != '\0') {
	if (*s++ == '/')
	    name = s;
    }

    while (a->name != 0) {
	if (strcmp(name, a->name) == 0) {
	    int status;

	    status = ((*(a->main)) (argc, argv));
	    if (status < 0) {
		fprintf(stderr, "%s: %s\n", a->name, strerror(errno));
	    }
	    fprintf(stderr, "\n");
	    exit(status);
	}
	a++;
    }
    exit (busybox_main(argc, argv));
}


int busybox_main(int argc, char **argv)
{
    int col=0;
    argc--;
    argv++;

    if (been_there_done_that == 1 || argc < 1) {
	const struct Applet *a = applets;
	fprintf(stderr, "BusyBox v%s (%s) multi-call binary -- GPL2\n",
		BB_VER, BB_BT);
	fprintf(stderr, "\nUsage:\t[function] [arguments]...\n");
	fprintf(stderr, "\tbusybox [function] [arguments]...\n");
	fprintf(stderr,
		"\n\tMost people will create a symlink to busybox for each\n"
		"\tfunction name, and busybox will act like whatever you invoke it as.\n");
	fprintf(stderr, "\nCurrently defined functions:\n");

	while (a->name != 0) {
	    col+=fprintf(stderr, "%s%s", ((col==0)? "\t":", "), (a++)->name);
	    if (col>60) {
		fprintf(stderr, ",\n");
		col=0;
	    }
	}
	fprintf(stderr, "\n\n");
	exit(-1);
    } else {
	/* If we've already been here once, exit now */
	been_there_done_that = 1;
	return (main(argc, argv));
    }
}
