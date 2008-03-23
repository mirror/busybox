/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 2003 by Glenn McGrath
 * SELinux support: by Yuichi Nakamura <ynakam@hitachisoft.jp>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * TODO: -d option, need a way of recursively making directories and changing
 *           owner/group, will probably modify bb_make_directory(...)
 */

#include "libbb.h"
#include "libcoreutils/coreutils.h"

#if ENABLE_FEATURE_INSTALL_LONG_OPTIONS
static const char install_longopts[] ALIGN1 =
	"directory\0"           No_argument       "d"
	"preserve-timestamps\0" No_argument       "p"
	"strip\0"               No_argument       "s"
	"group\0"               No_argument       "g"
	"mode\0"                No_argument       "m"
	"owner\0"               No_argument       "o"
/* autofs build insists of using -b --suffix=.orig */
/* TODO? (short option for --suffix is -S) */
#if ENABLE_SELINUX
	"context\0"             Required_argument "Z"
	"preserve_context\0"    No_argument       "\xff"
	"preserve-context\0"    No_argument       "\xff"
#endif
	;
#endif


#if ENABLE_SELINUX
static void setdefaultfilecon(const char *path)
{
	struct stat s;
	security_context_t scontext = NULL;

	if (!is_selinux_enabled()) {
		return;
	}
	if (lstat(path, &s) != 0) {
		return;
	}

	if (matchpathcon(path, s.st_mode, &scontext) < 0) {
		goto out;
	}
	if (strcmp(scontext, "<<none>>") == 0) {
		goto out;
	}

	if (lsetfilecon(path, scontext) < 0) {
		if (errno != ENOTSUP) {
			bb_perror_msg("warning: failed to change context of %s to %s", path, scontext);
		}
	}

 out:
	freecon(scontext);
}

#endif

int install_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int install_main(int argc, char **argv)
{
	struct stat statbuf;
	mode_t mode;
	uid_t uid;
	gid_t gid;
	char *arg, *last;
	const char *gid_str;
	const char *uid_str;
	const char *mode_str;
	int copy_flags = FILEUTILS_DEREFERENCE | FILEUTILS_FORCE;
	int flags;
	int ret = EXIT_SUCCESS;
	int isdir;
#if ENABLE_SELINUX
	security_context_t scontext;
	bool use_default_selinux_context = 1;
#endif
	enum {
		OPT_c             = 1 << 0,
		OPT_v             = 1 << 1,
		OPT_b             = 1 << 2,
		OPT_DIRECTORY     = 1 << 3,
		OPT_PRESERVE_TIME = 1 << 4,
		OPT_STRIP         = 1 << 5,
		OPT_GROUP         = 1 << 6,
		OPT_MODE          = 1 << 7,
		OPT_OWNER         = 1 << 8,
#if ENABLE_SELINUX
		OPT_SET_SECURITY_CONTEXT = 1 << 9,
		OPT_PRESERVE_SECURITY_CONTEXT = 1 << 10,
#endif
	};

#if ENABLE_FEATURE_INSTALL_LONG_OPTIONS
	applet_long_options = install_longopts;
#endif
	opt_complementary = "s--d:d--s" USE_SELINUX(":Z--\xff:\xff--Z");
	/* -c exists for backwards compatibility, it's needed */
	/* -v is ignored ("print name of each created directory") */
	/* -b is ignored ("make a backup of each existing destination file") */
	flags = getopt32(argv, "cvb" "dpsg:m:o:" USE_SELINUX("Z:"),
			&gid_str, &mode_str, &uid_str USE_SELINUX(, &scontext));
	argc -= optind;
	argv += optind;

#if ENABLE_SELINUX
	if (flags & (OPT_PRESERVE_SECURITY_CONTEXT|OPT_SET_SECURITY_CONTEXT)) {
		selinux_or_die();
		use_default_selinux_context = 0;
		if (flags & OPT_PRESERVE_SECURITY_CONTEXT) {
			copy_flags |= FILEUTILS_PRESERVE_SECURITY_CONTEXT;
		}
		if (flags & OPT_SET_SECURITY_CONTEXT) {
			setfscreatecon_or_die(scontext);
			copy_flags |= FILEUTILS_SET_SECURITY_CONTEXT;
		}
	}
#endif

	/* preserve access and modification time, this is GNU behaviour, BSD only preserves modification time */
	if (flags & OPT_PRESERVE_TIME) {
		copy_flags |= FILEUTILS_PRESERVE_STATUS;
	}
	mode = 0666;
	if (flags & OPT_MODE)
		bb_parse_mode(mode_str, &mode);
	uid = (flags & OPT_OWNER) ? get_ug_id(uid_str, xuname2uid) : getuid();
	gid = (flags & OPT_GROUP) ? get_ug_id(gid_str, xgroup2gid) : getgid();
	if (flags & (OPT_OWNER|OPT_GROUP))
		umask(0);

	/* Create directories
	 * don't use bb_make_directory() as it can't change uid or gid
	 * perhaps bb_make_directory() should be improved.
	 */
	if (flags & OPT_DIRECTORY) {
		while ((arg = *argv++) != NULL) {
			char *slash = arg;
			while (1) {
				slash = strchr(slash + 1, '/');
				if (slash)
					*slash = '\0';
				if (mkdir(arg, mode | 0111) == -1) {
					if (errno != EEXIST) {
						bb_perror_msg("cannot create %s", arg);
						ret = EXIT_FAILURE;
						break;
					}
				} /* dir was created, chown? */
				else if ((flags & (OPT_OWNER|OPT_GROUP))
				 && lchown(arg, uid, gid) == -1
				) {
					bb_perror_msg("cannot change ownership of %s", arg);
					ret = EXIT_FAILURE;
					break;
				}
				if (!slash)
					break;
				*slash = '/';
			}
		}
		return ret;
	}

	if (argc < 2)
		bb_show_usage();

	last = argv[argc - 1];
	argv[argc - 1] = NULL;
	/* coreutils install resolves link in this case, don't use lstat */
	isdir = stat(last, &statbuf) < 0 ? 0 : S_ISDIR(statbuf.st_mode);

	while ((arg = *argv++) != NULL) {
		char *dest = last;
		if (isdir)
			dest = concat_path_file(last, basename(arg));
		if (copy_file(arg, dest, copy_flags)) {
			/* copy is not made */
			ret = EXIT_FAILURE;
			goto next;
		}

		/* Set the file mode */
		if ((flags & OPT_MODE) && chmod(dest, mode) == -1) {
			bb_perror_msg("cannot change permissions of %s", dest);
			ret = EXIT_FAILURE;
		}
#if ENABLE_SELINUX
		if (use_default_selinux_context)
			setdefaultfilecon(dest);
#endif
		/* Set the user and group id */
		if ((flags & (OPT_OWNER|OPT_GROUP))
		 && lchown(dest, uid, gid) == -1
		) {
			bb_perror_msg("cannot change ownership of %s", dest);
			ret = EXIT_FAILURE;
		}
		if (flags & OPT_STRIP) {
			char *args[3];
			args[0] = (char*)"strip";
			args[1] = dest;
			args[2] = NULL;
			if (spawn_and_wait(args)) {
				bb_perror_msg("strip");
				ret = EXIT_FAILURE;
			}
		}
 next:
		if (ENABLE_FEATURE_CLEAN_UP && isdir)
			free(dest);
	}

	return ret;
}
