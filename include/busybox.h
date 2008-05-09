/* vi: set sw=4 ts=4: */
/*
 * Busybox main internal header file
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */
#ifndef	_BB_INTERNAL_H_
#define	_BB_INTERNAL_H_    1

#include "libbb.h"

#if __GNUC_PREREQ(4,1)
# pragma GCC visibility push(hidden)
#endif

/* order matters: used as index into "install_dir[]" in appletlib.c */
typedef enum bb_install_loc_t {
	_BB_DIR_ROOT = 0,
	_BB_DIR_BIN,
	_BB_DIR_SBIN,
	_BB_DIR_USR_BIN,
	_BB_DIR_USR_SBIN
} bb_install_loc_t;

typedef enum bb_suid_t {
	_BB_SUID_NEVER = 0,
	_BB_SUID_MAYBE,
	_BB_SUID_ALWAYS
} bb_suid_t;


/* Defined in appletlib.c (by including generated applet_tables.h) */
/* Keep in sync with applets/applet_tables.c! */
extern const char applet_names[];
extern int (*const applet_main[])(int argc, char **argv);
extern const uint16_t applet_nameofs[];
extern const uint8_t applet_install_loc[];

#if ENABLE_FEATURE_SUID || ENABLE_FEATURE_PREFER_APPLETS
#define APPLET_NAME(i) (applet_names + (applet_nameofs[i] & 0x0fff))
#else
#define APPLET_NAME(i) (applet_names + applet_nameofs[i])
#endif

#if ENABLE_FEATURE_PREFER_APPLETS
#define APPLET_IS_NOFORK(i) (applet_nameofs[i] & (1 << 12))
#define APPLET_IS_NOEXEC(i) (applet_nameofs[i] & (1 << 13))
#endif

#if ENABLE_FEATURE_SUID
#define APPLET_SUID(i) ((applet_nameofs[i] >> 14) & 0x3)
#endif

#if ENABLE_FEATURE_INSTALLER
#define APPLET_INSTALL_LOC(i) ({ \
	unsigned v = (i); \
	if (v & 1) v = applet_install_loc[v/2] >> 4; \
	else v = applet_install_loc[v/2] & 0xf; \
	v; })
#endif


/* Length of these names has effect on size of libbusybox
 * and "individual" binaries. Keep them short.
 */
#if ENABLE_BUILD_LIBBUSYBOX
#if ENABLE_FEATURE_SHARED_BUSYBOX
int lbb_main(char **argv) EXTERNALLY_VISIBLE;
#else
int lbb_main(char **argv);
#endif
#endif

#if __GNUC_PREREQ(4,1)
# pragma GCC visibility pop
#endif

#endif	/* _BB_INTERNAL_H_ */
