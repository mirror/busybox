/* vi: set sw=4 ts=4: */
/*
 * See README for additional information
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

#include "libbb.h"
#include "e2fs_lib.h"

#if INT_MAX == LONG_MAX
#define IF_LONG_IS_SAME(...) __VA_ARGS__
#define IF_LONG_IS_WIDER(...)
#else
#define IF_LONG_IS_SAME(...)
#define IF_LONG_IS_WIDER(...) __VA_ARGS__
#endif

/* Iterate a function on each entry of a directory */
int iterate_on_dir(const char *dir_name,
		int FAST_FUNC (*func)(const char *, struct dirent *, void *),
		void *private)
{
	DIR *dir;
	struct dirent *de;

	dir = opendir(dir_name);
	if (dir == NULL) {
		return -1;
	}
	while ((de = readdir(dir)) != NULL) {
		func(dir_name, de, private);
	}
	closedir(dir);
	return 0;
}

/* Print file attributes on an ext2 file system */
const uint32_t e2attr_flags_value[] ALIGN4 = {
#ifdef ENABLE_COMPRESSION
	EXT2_COMPRBLK_FL,
	EXT2_DIRTY_FL,
	EXT2_NOCOMPR_FL,
#endif
	EXT2_SECRM_FL,
	EXT2_UNRM_FL,
	EXT2_SYNC_FL,
	EXT2_DIRSYNC_FL,
	EXT2_IMMUTABLE_FL,
	EXT2_APPEND_FL,
	EXT2_NODUMP_FL,
	EXT2_NOATIME_FL,
	EXT2_COMPR_FL,
	EXT2_ECOMPR_FL,
	EXT3_JOURNAL_DATA_FL,
	EXT2_INDEX_FL,
	EXT2_NOTAIL_FL,
	EXT2_TOPDIR_FL,
	EXT2_EXTENT_FL,
	EXT2_NOCOW_FL,
	EXT2_CASEFOLD_FL,
	EXT2_INLINE_DATA_FL,
	EXT2_PROJINHERIT_FL,
	EXT2_VERITY_FL,
};

const char e2attr_flags_sname[] ALIGN1 =
#ifdef ENABLE_COMPRESSION
	"BZX"
#endif
	"suSDiadAcEjItTeCFNPV";

static const char e2attr_flags_lname[] ALIGN1 =
#ifdef ENABLE_COMPRESSION
	"Compressed_File" "\0"
	"Compressed_Dirty_File" "\0"
	"Compression_Raw_Access" "\0"
#endif
	"Secure_Deletion" "\0"
	"Undelete" "\0"
	"Synchronous_Updates" "\0"
	"Synchronous_Directory_Updates" "\0"
	"Immutable" "\0"
	"Append_Only" "\0"
	"No_Dump" "\0"
	"No_Atime" "\0"
	"Compression_Requested" "\0"
	"Encrypted" "\0"
	"Journaled_Data" "\0"
	"Indexed_directory" "\0"
	"No_Tailmerging" "\0"
	"Top_of_Directory_Hierarchies" "\0"
	"Extents" "\0"
	"No_COW" "\0"
	"Casefold" "\0"
	"Inline_Data" "\0"
	"Project_Hierarchy" "\0"
	"Verity" "\0"
	/* Another trailing NUL is added by compiler */;

void print_e2flags(FILE *f, unsigned flags, unsigned options)
{
	const uint32_t *fv;
	const char *fn;

	fv = e2attr_flags_value;
	if (options & PFOPT_LONG) {
		int first = 1;
		fn = e2attr_flags_lname;
		do {
			if (flags & *fv) {
				if (!first)
					fputs(", ", f);
				fputs(fn, f);
				first = 0;
			}
			fv++;
			fn += strlen(fn) + 1;
		} while (*fn);
		if (first)
			fputs("---", f);
	} else {
		fn = e2attr_flags_sname;
		do  {
			char c = '-';
			if (flags & *fv)
				c = *fn;
			fputc(c, f);
			fv++;
			fn++;
		} while (*fn);
	}
}
