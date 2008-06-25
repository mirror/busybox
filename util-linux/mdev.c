/* vi: set sw=4 ts=4: */
/*
 *
 * mdev - Mini udev for busybox
 *
 * Copyright 2005 Rob Landley <rob@landley.net>
 * Copyright 2005 Frank Sorenson <frank@tuxrocks.com>
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "xregex.h"

struct globals {
	int root_major, root_minor;
};
#define G (*(struct globals*)&bb_common_bufsiz1)
#define root_major (G.root_major)
#define root_minor (G.root_minor)

#define MAX_SYSFS_DEPTH 3 /* prevent infinite loops in /sys symlinks */

/* We use additional 64+ bytes in make_device() */
#define SCRATCH_SIZE 80

static char *next_field(char *s)
{
	char *end = skip_non_whitespace(s);
	s = skip_whitespace(end);
	*end = '\0';
	if (*s == '\0')
		s = NULL;
	return s;
}

/* mknod in /dev based on a path like "/sys/block/hda/hda1" */
/* NB: "mdev -s" may call us many times, do not leak memory/fds! */
static void make_device(char *path, int delete)
{
	const char *device_name;
	int major, minor, type, len;
	int mode = 0660;
	uid_t uid = 0;
	gid_t gid = 0;
	char *dev_maj_min = path + strlen(path);
	char *command = NULL;
	char *alias = NULL;
	char aliaslink = aliaslink; /* for compiler */

	/* Force the configuration file settings exactly. */
	umask(0);

	/* Try to read major/minor string.  Note that the kernel puts \n after
	 * the data, so we don't need to worry about null terminating the string
	 * because sscanf() will stop at the first nondigit, which \n is.  We
	 * also depend on path having writeable space after it.
	 */
	if (!delete) {
		strcpy(dev_maj_min, "/dev");
		len = open_read_close(path, dev_maj_min + 1, 64);
		*dev_maj_min++ = '\0';
		if (len < 1) {
			if (!ENABLE_FEATURE_MDEV_EXEC)
				return;
			/* no "dev" file, so just try to run script */
			*dev_maj_min = '\0';
		}
	}

	/* Determine device name, type, major and minor */
	device_name = bb_basename(path);
	/* http://kernel.org/doc/pending/hotplug.txt says that only
	 * "/sys/block/..." is for block devices. "/sys/bus" etc is not!
	 * Since kernel 2.6.25 block devices are also in /sys/class/block. */
	/* TODO: would it be acceptable to just use strstr(path, "/block/")? */
	if (strncmp(&path[5], "class/block/"+6, 6) != 0
	 && strncmp(&path[5], "class/block/", 12) != 0)
	        type = S_IFCHR;
	else
	        type = S_IFBLK;

	if (ENABLE_FEATURE_MDEV_CONF) {
		FILE *fp;
		char *line, *val, *next;
		unsigned lineno = 0;

		/* If we have config file, look up user settings */
		fp = fopen_or_warn("/etc/mdev.conf", "r");
		if (!fp)
			goto end_parse;

		while ((line = xmalloc_fgetline(fp)) != NULL) {
			regmatch_t off[1+9*ENABLE_FEATURE_MDEV_RENAME_REGEXP];

			++lineno;
			trim(line);
			if (!line[0])
				goto next_line;

			/* Fields: regex uid:gid mode [alias] [cmd] */

			/* 1st field: regex to match this device */
			next = next_field(line);
			{
				regex_t match;
				int result;

				/* Is this it? */
				xregcomp(&match, line, REG_EXTENDED);
				result = regexec(&match, device_name, ARRAY_SIZE(off), off, 0);
				regfree(&match);

				//bb_error_msg("matches:");
				//for (int i = 0; i < ARRAY_SIZE(off); i++) {
				//	if (off[i].rm_so < 0) continue;
				//	bb_error_msg("match %d: '%.*s'\n", i,
				//		(int)(off[i].rm_eo - off[i].rm_so),
				//		device_name + off[i].rm_so);
				//}

				/* If not this device, skip rest of line */
				/* (regexec returns whole pattern as "range" 0) */
				if (result || off[0].rm_so
				 || ((int)off[0].rm_eo != (int)strlen(device_name))
				) {
					goto next_line;
				}
			}

			/* This line matches: stop parsing the file
			 * after parsing the rest of fields */

			/* 2nd field: uid:gid - device ownership */
			if (!next) /* field must exist */
				bb_error_msg_and_die("bad line %u", lineno);
			val = next;
			next = next_field(val);
			{
				struct passwd *pass;
				struct group *grp;
				char *str_uid = val;
				char *str_gid = strchrnul(val, ':');

				if (*str_gid)
					*str_gid++ = '\0';
				/* Parse UID */
				pass = getpwnam(str_uid);
				if (pass)
					uid = pass->pw_uid;
				else
					uid = strtoul(str_uid, NULL, 10);
				/* Parse GID */
				grp = getgrnam(str_gid);
				if (grp)
					gid = grp->gr_gid;
				else
					gid = strtoul(str_gid, NULL, 10);
			}

			/* 3rd field: mode - device permissions */
			if (!next) /* field must exist */
				bb_error_msg_and_die("bad line %u", lineno);
			val = next;
			next = next_field(val);
			mode = strtoul(val, NULL, 8);

			/* 4th field (opt): >alias */
#if ENABLE_FEATURE_MDEV_RENAME
			if (!next)
				break;
			if (*next == '>' || *next == '=') {
#if ENABLE_FEATURE_MDEV_RENAME_REGEXP
				char *s, *p;
				unsigned i, n;

				aliaslink = *next;
				val = next;
				next = next_field(val);
				/* substitute %1..9 with off[1..9], if any */
				n = 0;
				s = val;
				while (*s)
					if (*s++ == '%')
						n++;

				p = alias = xzalloc(strlen(val) + n * strlen(device_name));
				s = val + 1;
				while (*s) {
					*p = *s;
					if ('%' == *s) {
						i = (s[1] - '0');
						if (i <= 9 && off[i].rm_so >= 0) {
							n = off[i].rm_eo - off[i].rm_so;
							strncpy(p, device_name + off[i].rm_so, n);
							p += n - 1;
							s++;
						}
					}
					p++;
					s++;
				}
#else
				aliaslink = *next;
				val = next;
				next = next_field(val);
				alias = xstrdup(val + 1);
#endif
			}
#endif /* ENABLE_FEATURE_MDEV_RENAME */

			/* The rest (opt): command to run */
			if (!next)
				break;
			val = next;
			if (ENABLE_FEATURE_MDEV_EXEC) {
				const char *s = "@$*";
				const char *s2 = strchr(s, *val);

				if (!s2)
					bb_error_msg_and_die("bad line %u", lineno);

				/* Correlate the position in the "@$*" with the delete
				 * step so that we get the proper behavior:
				 * @cmd: run on create
				 * $cmd: run on delete
				 * *cmd: run on both
				 */
				if ((s2 - s + 1) /*1/2/3*/ & /*1/2*/ (1 + delete)) {
					command = xstrdup(val + 1);
				}
			}
			/* end of field parsing */
			break; /* we found matching line, stop */
 next_line:
			free(line);
		} /* end of "while line is read from /etc/mdev.conf" */

		free(line); /* in case we used "break" to get here */
		fclose(fp);
	}
 end_parse:

	if (!delete && sscanf(dev_maj_min, "%u:%u", &major, &minor) == 2) {

		if (ENABLE_FEATURE_MDEV_RENAME)
			unlink(device_name);

		if (mknod(device_name, mode | type, makedev(major, minor)) && errno != EEXIST)
			bb_perror_msg_and_die("mknod %s", device_name);

		if (major == root_major && minor == root_minor)
			symlink(device_name, "root");

		if (ENABLE_FEATURE_MDEV_CONF) {
			chown(device_name, uid, gid);

			if (ENABLE_FEATURE_MDEV_RENAME && alias) {
				char *dest;

				/* ">bar/": rename to bar/device_name */
				/* ">bar[/]baz": rename to bar[/]baz */
				dest = strrchr(alias, '/');
				if (dest) { /* ">bar/[baz]" ? */
					*dest = '\0'; /* mkdir bar */
					bb_make_directory(alias, 0755, FILEUTILS_RECUR);
					*dest = '/';
					if (dest[1] == '\0') { /* ">bar/" => ">bar/device_name" */
						dest = alias;
						alias = concat_path_file(alias, device_name);
						free(dest);
					}
				}

				/* move the device, and optionally
				 * make a symlink to moved device node */
				if (rename(device_name, alias) == 0 && aliaslink == '>')
					symlink(alias, device_name);

				free(alias);
			}
		}
	}

	if (ENABLE_FEATURE_MDEV_EXEC && command) {
		/* setenv will leak memory, use putenv/unsetenv/free */
		char *s = xasprintf("MDEV=%s", device_name);
		putenv(s);
		if (system(command) == -1)
			bb_perror_msg_and_die("can't run '%s'", command);
		s[4] = '\0';
		unsetenv(s);
		free(s);
		free(command);
	}

	if (delete)
		unlink(device_name);
}

/* File callback for /sys/ traversal */
static int fileAction(const char *fileName,
		struct stat *statbuf ATTRIBUTE_UNUSED,
		void *userData,
		int depth ATTRIBUTE_UNUSED)
{
	size_t len = strlen(fileName) - 4; /* can't underflow */
	char *scratch = userData;

	/* len check is for paranoid reasons */
	if (strcmp(fileName + len, "/dev") || len >= PATH_MAX)
		return FALSE;

	strcpy(scratch, fileName);
	scratch[len] = '\0';
	make_device(scratch, 0);

	return TRUE;
}

/* Directory callback for /sys/ traversal */
static int dirAction(const char *fileName ATTRIBUTE_UNUSED,
		struct stat *statbuf ATTRIBUTE_UNUSED,
		void *userData ATTRIBUTE_UNUSED,
		int depth)
{
	return (depth >= MAX_SYSFS_DEPTH ? SKIP : TRUE);
}

/* For the full gory details, see linux/Documentation/firmware_class/README
 *
 * Firmware loading works like this:
 * - kernel sets FIRMWARE env var
 * - userspace checks /lib/firmware/$FIRMWARE
 * - userspace waits for /sys/$DEVPATH/loading to appear
 * - userspace writes "1" to /sys/$DEVPATH/loading
 * - userspace copies /lib/firmware/$FIRMWARE into /sys/$DEVPATH/data
 * - userspace writes "0" (worked) or "-1" (failed) to /sys/$DEVPATH/loading
 * - kernel loads firmware into device
 */
static void load_firmware(const char *const firmware, const char *const sysfs_path)
{
	int cnt;
	int firmware_fd, loading_fd, data_fd;

	/* check for /lib/firmware/$FIRMWARE */
	xchdir("/lib/firmware");
	firmware_fd = xopen(firmware, O_RDONLY);

	/* in case we goto out ... */
	data_fd = -1;

	/* check for /sys/$DEVPATH/loading ... give 30 seconds to appear */
	xchdir(sysfs_path);
	for (cnt = 0; cnt < 30; ++cnt) {
		loading_fd = open("loading", O_WRONLY);
		if (loading_fd != -1)
			goto loading;
		sleep(1);
	}
	goto out;

 loading:
	/* tell kernel we're loading by `echo 1 > /sys/$DEVPATH/loading` */
	if (full_write(loading_fd, "1", 1) != 1)
		goto out;

	/* load firmware by `cat /lib/firmware/$FIRMWARE > /sys/$DEVPATH/data */
	data_fd = open("data", O_WRONLY);
	if (data_fd == -1)
		goto out;
	cnt = bb_copyfd_eof(firmware_fd, data_fd);

	/* tell kernel result by `echo [0|-1] > /sys/$DEVPATH/loading` */
	if (cnt > 0)
		full_write(loading_fd, "0", 1);
	else
		full_write(loading_fd, "-1", 2);

 out:
	if (ENABLE_FEATURE_CLEAN_UP) {
		close(firmware_fd);
		close(loading_fd);
		close(data_fd);
	}
}

int mdev_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int mdev_main(int argc, char **argv)
{
	char *action;
	char *env_path;
	RESERVE_CONFIG_BUFFER(temp, PATH_MAX + SCRATCH_SIZE);

#ifdef YOU_WANT_TO_DEBUG_HOTPLUG_EVENTS
	/* Kernel cannot provide suitable stdio fds for us, do it ourself */
	/* Replace LOGFILE by other file or device name if you need */
#define LOGFILE "/dev/console"
	xmove_fd(xopen("/dev/null", O_RDONLY), STDIN_FILENO);
	xmove_fd(xopen(LOGFILE, O_WRONLY|O_APPEND), STDOUT_FILENO);
	xmove_fd(xopen(LOGFILE, O_WRONLY|O_APPEND), STDERR_FILENO);
#endif

	xchdir("/dev");

	if (argc == 2 && !strcmp(argv[1], "-s")) {
		/* Scan:
		 * mdev -s
		 */
		struct stat st;

		xstat("/", &st);
		root_major = major(st.st_dev);
		root_minor = minor(st.st_dev);

		recursive_action("/sys/block",
			ACTION_RECURSE | ACTION_FOLLOWLINKS,
			fileAction, dirAction, temp, 0);

		recursive_action("/sys/class",
			ACTION_RECURSE | ACTION_FOLLOWLINKS,
			fileAction, dirAction, temp, 0);

	} else {
		/* Hotplug:
		 * env ACTION=... DEVPATH=... mdev
		 * ACTION can be "add" or "remove"
		 * DEVPATH is like "/block/sda" or "/class/input/mice"
		 */
		action = getenv("ACTION");
		env_path = getenv("DEVPATH");
		if (!action || !env_path)
			bb_show_usage();

		snprintf(temp, PATH_MAX, "/sys%s", env_path);
		if (!strcmp(action, "remove"))
			make_device(temp, 1);
		else if (!strcmp(action, "add")) {
			make_device(temp, 0);

			if (ENABLE_FEATURE_MDEV_LOAD_FIRMWARE) {
				char *fw = getenv("FIRMWARE");
				if (fw)
					load_firmware(fw, temp);
			}
		}
	}

	if (ENABLE_FEATURE_CLEAN_UP)
		RELEASE_CONFIG_BUFFER(temp);

	return 0;
}
