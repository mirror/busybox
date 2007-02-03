/* vi: set sw=4 ts=4: */
/*
 * Busybox main internal header file
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */
#ifndef	_BB_INTERNAL_H_
#define	_BB_INTERNAL_H_    1

#include "libbb.h"

/* order matters: used as index into "install_dir[]" in busybox.c */
enum Location {
	_BB_DIR_ROOT = 0,
	_BB_DIR_BIN,
	_BB_DIR_SBIN,
	_BB_DIR_USR_BIN,
	_BB_DIR_USR_SBIN
};

enum SUIDRoot {
	_BB_SUID_NEVER = 0,
	_BB_SUID_MAYBE,
	_BB_SUID_ALWAYS
};

struct BB_applet {
	const char *name;
	int (*main) (int argc, char **argv);
	__extension__ enum Location location:4;
	__extension__ enum SUIDRoot need_suid:4;
};

/* Defined in busybox.c and applet.c */
extern int busybox_main(int argc, char **argv);
extern const struct BB_applet applets[];
extern const unsigned short NUM_APPLETS;

#endif	/* _BB_INTERNAL_H_ */
