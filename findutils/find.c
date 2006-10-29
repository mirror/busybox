/* vi: set sw=4 ts=4: */
/*
 * Mini find implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Reworked by David Douthitt <n9ubh@callsign.net> and
 *  Matt Kraai <kraai@alumni.carnegiemellon.edu>.
 *
 * Licensed under the GPL version 2, see the file LICENSE in this tarball.
 */

/* findutils-4.1.20:
 *
 * # find file.txt -exec 'echo {}' '{}  {}' ';'
 * find: echo file.txt: No such file or directory
 * # find file.txt -exec 'echo' '{}  {}' '; '
 * find: missing argument to `-exec'
 * # find file.txt -exec 'echo {}' '{}  {}' ';' junk
 * find: paths must precede expression
 * # find file.txt -exec 'echo {}' '{}  {}' ';' junk ';'
 * find: paths must precede expression
 * # find file.txt -exec 'echo' '{}  {}' ';'
 * file.txt  file.txt
 * (strace: execve("/bin/echo", ["echo", "file.txt  file.txt"], [ 30 vars ]))
 *
 * bboxed find rev 16467: above - works, below - doesn't
 *
 * # find file.txt -exec 'echo' '{}  {}' ';' -print -exec pwd ';'
 * file.txt  file.txt
 * file.txt
 * /tmp
 */

#include "busybox.h"
#include <fnmatch.h>

static char *pattern;
#if ENABLE_FEATURE_FIND_PRINT0
static char printsep = '\n';
#endif

#if ENABLE_FEATURE_FIND_TYPE
static int type_mask = 0;
#endif

#if ENABLE_FEATURE_FIND_PERM
static char perm_char = 0;
static int perm_mask = 0;
#endif

#if ENABLE_FEATURE_FIND_MTIME
static char mtime_char;
static int mtime_days;
#endif

#if ENABLE_FEATURE_FIND_MMIN
static char mmin_char;
static int mmin_mins;
#endif

#if ENABLE_FEATURE_FIND_XDEV
static dev_t *xdev_dev;
static int xdev_count = 0;
#endif

#if ENABLE_FEATURE_FIND_NEWER
static time_t newer_mtime;
#endif

#if ENABLE_FEATURE_FIND_INUM
static ino_t inode_num;
#endif

#if ENABLE_FEATURE_FIND_EXEC
static char **exec_argv;
static int *subst_count;
static int exec_argc;
#endif

static int count_subst(const char *str)
{
	int count = 0;
	while ((str = strstr(str, "{}"))) {
		count++;
		str++;
	}
	return count;
}


static char* subst(const char *src, int count, const char* filename)
{
	char *buf, *dst, *end;
	int flen = strlen(filename);
//puts(src);
	/* we replace each '{}' with filename: growth by strlen-2 */
	buf = dst = xmalloc(strlen(src) + count*(flen-2) + 1);
	while ((end = strstr(src, "{}"))) {
		memcpy(dst, src, end - src);
		dst += end - src;
		src = end + 2;
		memcpy(dst, filename, flen);
		dst += flen;
	}
	strcpy(dst, src);
//puts(buf);
	return buf;
}


static int fileAction(const char *fileName, struct stat *statbuf, void* junk, int depth)
{
#if ENABLE_FEATURE_FIND_XDEV
	if (S_ISDIR(statbuf->st_mode) && xdev_count) {
		int i;
		for (i=0; i<xdev_count; i++) {
			if (xdev_dev[i] != statbuf->st_dev)
				return SKIP;
		}
	}
#endif
	if (pattern != NULL) {
		const char *tmp = strrchr(fileName, '/');

		if (tmp == NULL)
			tmp = fileName;
		else
			tmp++;
		if (fnmatch(pattern, tmp, FNM_PERIOD) != 0)
			goto no_match;
	}
#if ENABLE_FEATURE_FIND_TYPE
	if (type_mask != 0) {
		if (!((statbuf->st_mode & S_IFMT) == type_mask))
			goto no_match;
	}
#endif
#if ENABLE_FEATURE_FIND_PERM
	if (perm_mask != 0) {
		if (!((isdigit(perm_char) && (statbuf->st_mode & 07777) == perm_mask) ||
			 (perm_char == '-' && (statbuf->st_mode & perm_mask) == perm_mask) ||
			 (perm_char == '+' && (statbuf->st_mode & perm_mask) != 0)))
			goto no_match;
	}
#endif
#if ENABLE_FEATURE_FIND_MTIME
	if (mtime_char != 0) {
		time_t file_age = time(NULL) - statbuf->st_mtime;
		time_t mtime_secs = mtime_days * 24 * 60 * 60;
		if (!((isdigit(mtime_char) && file_age >= mtime_secs &&
						file_age < mtime_secs + 24 * 60 * 60) ||
				(mtime_char == '+' && file_age >= mtime_secs + 24 * 60 * 60) ||
				(mtime_char == '-' && file_age < mtime_secs)))
			goto no_match;
	}
#endif
#if ENABLE_FEATURE_FIND_MMIN
	if (mmin_char != 0) {
		time_t file_age = time(NULL) - statbuf->st_mtime;
		time_t mmin_secs = mmin_mins * 60;
		if (!((isdigit(mmin_char) && file_age >= mmin_secs &&
						file_age < mmin_secs + 60) ||
				(mmin_char == '+' && file_age >= mmin_secs + 60) ||
				(mmin_char == '-' && file_age < mmin_secs)))
			goto no_match;
	}
#endif
#if ENABLE_FEATURE_FIND_NEWER
	if (newer_mtime != 0) {
		time_t file_age = newer_mtime - statbuf->st_mtime;
		if (file_age >= 0)
			goto no_match;
	}
#endif
#if ENABLE_FEATURE_FIND_INUM
	if (inode_num != 0) {
		if (!(statbuf->st_ino == inode_num))
			goto no_match;
	}
#endif
#if ENABLE_FEATURE_FIND_EXEC
	if (exec_argc) {
		int i;
		char *argv[exec_argc+1];
		for (i = 0; i < exec_argc; i++)
			argv[i] = subst(exec_argv[i], subst_count[i], fileName);
		argv[i] = NULL; /* terminate the list */
		errno = 0;
		wait4pid(spawn(argv));
		if (errno)
			bb_perror_msg("%s", argv[0]);
		for (i = 0; i < exec_argc; i++)
			free(argv[i]);
		goto no_match;
	}
#endif

#if ENABLE_FEATURE_FIND_PRINT0
	printf("%s%c", fileName, printsep);
#else
	puts(fileName);
#endif
 no_match:
	return TRUE;
}

#if ENABLE_FEATURE_FIND_TYPE
static int find_type(char *type)
{
	int mask = 0;

	switch (type[0]) {
	case 'b':
		mask = S_IFBLK;
		break;
	case 'c':
		mask = S_IFCHR;
		break;
	case 'd':
		mask = S_IFDIR;
		break;
	case 'p':
		mask = S_IFIFO;
		break;
	case 'f':
		mask = S_IFREG;
		break;
	case 'l':
		mask = S_IFLNK;
		break;
	case 's':
		mask = S_IFSOCK;
		break;
	}

	if (mask == 0 || type[1] != '\0')
		bb_error_msg_and_die(bb_msg_invalid_arg, type, "-type");

	return mask;
}
#endif

int find_main(int argc, char **argv)
{
	int dereference = FALSE;
	int i, j, firstopt, status = EXIT_SUCCESS;

	for (firstopt = 1; firstopt < argc; firstopt++) {
		if (argv[firstopt][0] == '-')
			break;
	}

	/* Parse any options */
	for (i = firstopt; i < argc; i++) {
		char *arg = argv[i];
		char *arg1 = argv[i+1];
		if (strcmp(arg, "-follow") == 0)
			dereference = TRUE;
		else if (strcmp(arg, "-print") == 0) {
			;
		}
#if ENABLE_FEATURE_FIND_PRINT0
		else if (strcmp(arg, "-print0") == 0)
			printsep = '\0';
#endif
		else if (strcmp(arg, "-name") == 0) {
			if (++i == argc)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			pattern = arg1;
#if ENABLE_FEATURE_FIND_TYPE
		} else if (strcmp(arg, "-type") == 0) {
			if (++i == argc)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			type_mask = find_type(arg1);
#endif
#if ENABLE_FEATURE_FIND_PERM
/* TODO:
 * -perm mode   File's permission bits are exactly mode (octal or symbolic).
 *              Symbolic modes use mode 0 as a point of departure.
 * -perm -mode  All of the permission bits mode are set for the file.
 * -perm +mode  Any of the permission bits mode are set for the file.
 */
		} else if (strcmp(arg, "-perm") == 0) {
			if (++i == argc)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			perm_mask = xstrtol_range(arg1, 8, 0, 07777);
			perm_char = arg1[0];
			if (perm_char == '-')
				perm_mask = -perm_mask;
#endif
#if ENABLE_FEATURE_FIND_MTIME
		} else if (strcmp(arg, "-mtime") == 0) {
			if (++i == argc)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			mtime_days = xatol(arg1);
			mtime_char = arg1[0];
			if (mtime_char == '-')
				mtime_days = -mtime_days;
#endif
#if ENABLE_FEATURE_FIND_MMIN
		} else if (strcmp(arg, "-mmin") == 0) {
			if (++i == argc)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			mmin_mins = xatol(arg1);
			mmin_char = arg1[0];
			if (mmin_char == '-')
				mmin_mins = -mmin_mins;
#endif
#if ENABLE_FEATURE_FIND_XDEV
		} else if (strcmp(arg, "-xdev") == 0) {
			struct stat stbuf;

			xdev_count = (firstopt - 1) ? (firstopt - 1) : 1;
			xdev_dev = xmalloc(xdev_count * sizeof(dev_t));

			if (firstopt == 1) {
				xstat(".", &stbuf);
				xdev_dev[0] = stbuf.st_dev;
			} else {
				for (j = 1; j < firstopt; i++) {
					xstat(argv[j], &stbuf);
					xdev_dev[j-1] = stbuf.st_dev;
				}
			}
#endif
#if ENABLE_FEATURE_FIND_NEWER
		} else if (strcmp(arg, "-newer") == 0) {
			struct stat stat_newer;
			if (++i == argc)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			xstat(arg1, &stat_newer);
			newer_mtime = stat_newer.st_mtime;
#endif
#if ENABLE_FEATURE_FIND_INUM
		} else if (strcmp(arg, "-inum") == 0) {
			if (++i == argc)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			inode_num = xatoul(arg1);
#endif
#if ENABLE_FEATURE_FIND_EXEC
		} else if (strcmp(arg, "-exec") == 0) {
			i++; /* now: argv[i] is the first arg after -exec */
			exec_argv = &argv[i];
			exec_argc = i;
			while (1) {
				if (i == argc) /* did not see ';' till end */
					bb_error_msg_and_die(bb_msg_requires_arg, arg);
				if (argv[i][0] == ';' && argv[i][1] == '\0')
					break;
				i++;
			}
			exec_argc = i - exec_argc; /* number of --exec arguments */
			if (exec_argc == 0)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			subst_count = xmalloc(exec_argc * sizeof(int));
			j = exec_argc;
			while (j--)
				subst_count[j] = count_subst(exec_argv[j]);
#endif
		} else
			bb_show_usage();
	}

	if (firstopt == 1) {
		if (!recursive_action(".", TRUE, dereference, FALSE, fileAction,
					fileAction, NULL, 0))
			status = EXIT_FAILURE;
	} else {
		for (i = 1; i < firstopt; i++) {
			if (!recursive_action(argv[i], TRUE, dereference, FALSE,
					fileAction, fileAction, NULL, 0))
				status = EXIT_FAILURE;
		}
	}

	return status;
}
