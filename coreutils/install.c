/* vi: set sw=4 ts=4: */
/*
 *  Copyright (C) 2003 by Glenn McGrath <bug1@iinet.net.au>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * TODO: -d option, need a way of recursively making directories and changing
 *           owner/group, will probably modify bb_make_directory(...)
 */

#include "busybox.h"
#include "libcoreutils/coreutils.h"
#include <libgen.h>
#include <getopt.h> /* struct option */

#if ENABLE_FEATURE_INSTALL_LONG_OPTIONS
static const struct option install_long_options[] = {
	{ "directory",           0, NULL, 'd' },
	{ "preserve-timestamps", 0, NULL, 'p' },
	{ "strip",               0, NULL, 's' },
	{ "group",               0, NULL, 'g' },
	{ "mode",                0, NULL, 'm' },
	{ "owner",               0, NULL, 'o' },
	{ 0, 0, 0, 0 }
};
#endif

int install_main(int argc, char **argv)
{
	struct stat statbuf;
	mode_t mode;
	uid_t uid;
	gid_t gid;
	const char *gid_str;
	const char *uid_str;
	const char *mode_str;
	int copy_flags = FILEUTILS_DEREFERENCE | FILEUTILS_FORCE;
	int ret = EXIT_SUCCESS, flags, i, isdir;

	enum {
		OPT_CMD           =  0x1,
		OPT_DIRECTORY     =  0x2,
		OPT_PRESERVE_TIME =  0x4,
		OPT_STRIP         =  0x8,
		OPT_GROUP         = 0x10,
		OPT_MODE          = 0x20,
		OPT_OWNER         = 0x40,
	};

#if ENABLE_FEATURE_INSTALL_LONG_OPTIONS
	applet_long_options = install_long_options;
#endif
	opt_complementary = "?:s--d:d--s";
	/* -c exists for backwards compatibility, its needed */
	flags = getopt32(argc, argv, "cdpsg:m:o:", &gid_str, &mode_str, &uid_str);

	/* preserve access and modification time, this is GNU behaviour, BSD only preserves modification time */
	if (flags & OPT_PRESERVE_TIME) {
		copy_flags |= FILEUTILS_PRESERVE_STATUS;
	}
	mode = 0666;
	if (flags & OPT_MODE) bb_parse_mode(mode_str, &mode);
	uid = (flags & OPT_OWNER) ? get_ug_id(uid_str, xuname2uid) : getuid();
	gid = (flags & OPT_GROUP) ? get_ug_id(gid_str, xgroup2gid) : getgid();
	if (flags & (OPT_OWNER|OPT_GROUP)) umask(0);

	/* Create directories
	 * don't use bb_make_directory() as it can't change uid or gid
	 * perhaps bb_make_directory() should be improved.
	 */
	if (flags & OPT_DIRECTORY) {
		for (argv += optind; *argv; argv++) {
			char *old_argv_ptr = *argv + 1;
			char *argv_ptr;
			do {
				argv_ptr = strchr(old_argv_ptr, '/');
				old_argv_ptr = argv_ptr;
				if (argv_ptr) {
					*argv_ptr = '\0';
					old_argv_ptr++;
				}
				if (mkdir(*argv, mode | 0111) == -1) {
					if (errno != EEXIST) {
						bb_perror_msg("cannot create %s", *argv);
						ret = EXIT_FAILURE;
						break;
					}
				}
				if ((flags & (OPT_OWNER|OPT_GROUP))
				 && lchown(*argv, uid, gid) == -1
				) {
					bb_perror_msg("cannot change ownership of %s", *argv);
					ret = EXIT_FAILURE;
					break;
				}
				if (argv_ptr) {
					*argv_ptr = '/';
				}
			} while (old_argv_ptr);
		}
		return ret;
	}

	isdir = lstat(argv[argc - 1], &statbuf) < 0 ? 0 : S_ISDIR(statbuf.st_mode);

	for (i = optind; i < argc - 1; i++) {
		char *dest;

		dest = argv[argc - 1];
		if (isdir)
			dest = concat_path_file(argv[argc - 1], basename(argv[i]));
		ret |= copy_file(argv[i], dest, copy_flags);

		/* Set the file mode */
		if ((flags & OPT_MODE) && chmod(dest, mode) == -1) {
			bb_perror_msg("cannot change permissions of %s", dest);
			ret = EXIT_FAILURE;
		}

		/* Set the user and group id */
		if ((flags & (OPT_OWNER|OPT_GROUP))
		 && lchown(dest, uid, gid) == -1
		) {
			bb_perror_msg("cannot change ownership of %s", dest);
			ret = EXIT_FAILURE;
		}
		if (flags & OPT_STRIP) {
			if (execlp("strip", "strip", dest, NULL) == -1) {
				bb_perror_msg("strip");
				ret = EXIT_FAILURE;
			}
		}
		if (ENABLE_FEATURE_CLEAN_UP && isdir) free(dest);
	}

	return ret;
}
