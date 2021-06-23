/* vi: set sw=4 ts=4: */
/*
 * See README for additional information
 *
 * This file can be redistributed under the terms of the GNU Library General
 * Public License
 */

/* Constants and structures */
#include "bb_e2fs_defs.h"

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

/* Iterate a function on each entry of a directory */
int iterate_on_dir(const char *dir_name,
		int FAST_FUNC (*func)(const char *, struct dirent *, void *),
		void *private);

/* Must be 1 for compatibility with 'int long_format'. */
#define PFOPT_LONG  1
/* Print file attributes on an ext2 file system */
void print_e2flags(FILE *f, unsigned flags, unsigned options);

extern const uint32_t e2attr_flags_value[];
extern const char e2attr_flags_sname[];

/* If you plan to ENABLE_COMPRESSION, see e2fs_lib.c and chattr.c - */
/* make sure that chattr doesn't accept bad options! */
#ifdef ENABLE_COMPRESSION
#define e2attr_flags_value_chattr (&e2attr_flags_value[5])
#define e2attr_flags_sname_chattr (&e2attr_flags_sname[5])
#else
#define e2attr_flags_value_chattr (&e2attr_flags_value[1])
#define e2attr_flags_sname_chattr (&e2attr_flags_sname[1])
#endif

POP_SAVED_FUNCTION_VISIBILITY
