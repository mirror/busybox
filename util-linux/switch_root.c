/* vi: set sw=4 ts=4: */
/* Copyright 2005 Rob Landley <rob@landley.net>
 *
 * Switch from rootfs to another filesystem as the root of the mount tree.
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"
#include <sys/vfs.h>

// Make up for header deficiencies
#ifndef RAMFS_MAGIC
#define RAMFS_MAGIC ((unsigned)0x858458f6)
#endif

#ifndef TMPFS_MAGIC
#define TMPFS_MAGIC ((unsigned)0x01021994)
#endif

#ifndef MS_MOVE
#define MS_MOVE     8192
#endif

// Recursively delete contents of rootfs
static void delete_contents(const char *directory, dev_t rootdev)
{
	DIR *dir;
	struct dirent *d;
	struct stat st;

	// Don't descend into other filesystems
	if (lstat(directory, &st) || st.st_dev != rootdev)
		return;

	// Recursively delete the contents of directories
	if (S_ISDIR(st.st_mode)) {
		dir = opendir(directory);
		if (dir) {
			while ((d = readdir(dir))) {
				char *newdir = d->d_name;

				// Skip . and ..
				if (DOT_OR_DOTDOT(newdir))
					continue;

				// Recurse to delete contents
				newdir = concat_path_file(directory, newdir);
				delete_contents(newdir, rootdev);
				free(newdir);
			}
			closedir(dir);

			// Directory should now be empty, zap it
			rmdir(directory);
		}
	} else {
		// It wasn't a directory, zap it
		unlink(directory);
	}
}

int switch_root_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int switch_root_main(int argc UNUSED_PARAM, char **argv)
{
	char *newroot, *console = NULL;
	struct stat st;
	struct statfs stfs;
	dev_t rootdev;

	// Parse args (-c console)
	opt_complementary = "-2"; // minimum 2 params
	getopt32(argv, "+c:", &console); // '+': stop at first non-option
	argv += optind;
	newroot = *argv++;

	// Change to new root directory and verify it's a different fs
	xchdir(newroot);
	xstat("/", &st);
	rootdev = st.st_dev;
	xstat(".", &st);
	if (st.st_dev == rootdev || getpid() != 1) {
		// Show usage, it says new root must be a mountpoint
		// and we must be PID 1
		bb_show_usage();
	}

	// Additional sanity checks: we're about to rm -rf /, so be REALLY SURE
	// we mean it. I could make this a CONFIG option, but I would get email
	// from all the people who WILL destroy their filesystems.
	statfs("/", &stfs); // this never fails
	if (lstat("/init", &st) != 0 || !S_ISREG(st.st_mode)
	 || ((unsigned)stfs.f_type != RAMFS_MAGIC
	     && (unsigned)stfs.f_type != TMPFS_MAGIC)
	) {
		bb_error_msg_and_die("not rootfs");
	}

	// Zap everything out of rootdev
	delete_contents("/", rootdev);

	// Overmount / with newdir and chroot into it
	if (mount(".", "/", NULL, MS_MOVE, NULL)) {
		// For example, fails when newroot is not a mountpoint
		bb_perror_msg_and_die("error moving root");
	}
	// The chdir is needed to recalculate "." and ".." links
	xchroot(".");
	xchdir("/");

	// If a new console specified, redirect stdin/stdout/stderr to it
	if (console) {
		close(0);
		xopen(console, O_RDWR);
		xdup2(0, 1);
		xdup2(0, 2);
	}

	// Exec real init
	execv(argv[0], argv);
	bb_perror_msg_and_die("can't execute '%s'", argv[0]);
}
