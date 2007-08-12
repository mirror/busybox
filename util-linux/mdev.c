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

/* mknod in /dev based on a path like "/sys/block/hda/hda1" */
static void make_device(char *path, int delete)
{
	const char *device_name;
	int major, minor, type, len;
	int mode = 0660;
	uid_t uid = 0;
	gid_t gid = 0;
	char *temp = path + strlen(path);
	char *command = NULL;

	/* Try to read major/minor string.  Note that the kernel puts \n after
	 * the data, so we don't need to worry about null terminating the string
	 * because sscanf() will stop at the first nondigit, which \n is.  We
	 * also depend on path having writeable space after it. */

	if (!delete) {
		strcat(path, "/dev");
		len = open_read_close(path, temp + 1, 64);
		*temp++ = 0;
		if (len < 1) return;
	}

	/* Determine device name, type, major and minor */

	device_name = bb_basename(path);
	type = path[5]=='c' ? S_IFCHR : S_IFBLK;

	/* If we have a config file, look up permissions for this device */

	if (ENABLE_FEATURE_MDEV_CONF) {
		char *conf, *pos, *end;
		int line, fd;

		/* mmap the config file */
		fd = open("/etc/mdev.conf", O_RDONLY);
		if (fd < 0)
			goto end_parse;
		len = xlseek(fd, 0, SEEK_END);
		conf = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
		close(fd);
		if (!conf)
			goto end_parse;

		line = 0;
		/* Loop through lines in mmaped file*/
		for (pos=conf; pos-conf<len;) {
			int field;
			char *end2;

			line++;
			/* find end of this line */
			for (end=pos; end-conf<len && *end!='\n'; end++)
				;

			/* Three fields: regex, uid:gid, mode */
			for (field=0; field < (3 + ENABLE_FEATURE_MDEV_EXEC);
					field++)
			{
				/* Skip whitespace */
				while (pos<end && isspace(*pos)) pos++;
				if (pos==end || *pos=='#') break;
				for (end2=pos;
					end2<end && !isspace(*end2) && *end2!='#'; end2++)
					;

				if (field == 0) {
					/* Regex to match this device */

					char *regex = xstrndup(pos, end2-pos);
					regex_t match;
					regmatch_t off;
					int result;

					/* Is this it? */
					xregcomp(&match,regex, REG_EXTENDED);
					result = regexec(&match, device_name, 1, &off, 0);
					regfree(&match);
					free(regex);

					/* If not this device, skip rest of line */
					if (result || off.rm_so
							|| off.rm_eo != strlen(device_name))
						break;
				}
				if (field == 1) {
					/* uid:gid */

					char *s, *s2;

					/* Find : */
					for (s=pos; s<end2 && *s!=':'; s++)
						;
					if (s == end2) break;

					/* Parse UID */
					uid = strtoul(pos, &s2, 10);
					if (s != s2) {
						struct passwd *pass;
						char *_unam = xstrndup(pos, s-pos);
						pass = getpwnam(_unam);
						free(_unam);
						if (!pass) break;
						uid = pass->pw_uid;
					}
					s++;
					/* parse GID */
					gid = strtoul(s, &s2, 10);
					if (end2 != s2) {
						struct group *grp;
						char *_grnam = xstrndup(s, end2-s);
						grp = getgrnam(_grnam);
						free(_grnam);
						if (!grp) break;
						gid = grp->gr_gid;
					}
				}
				if (field == 2) {
					/* mode */

					mode = strtoul(pos, &pos, 8);
					if (pos != end2) break;
				}
				if (ENABLE_FEATURE_MDEV_EXEC && field == 3) {
					// Command to run
					const char *s = "@$*";
					const char *s2;
					s2 = strchr(s, *pos++);
					if (!s2) {
						// Force error
						field = 1;
						break;
					}
					if ((s2-s+1) & (1<<delete))
						command = xstrndup(pos, end-pos);
				}

				pos = end2;
			}

			/* Did everything parse happily? */

			if (field > 2) break;
			if (field) bb_error_msg_and_die("bad line %d",line);

			/* Next line */
			pos = ++end;
		}
		munmap(conf, len);
 end_parse:	/* nothing */ ;
	}

	umask(0);
	if (!delete) {
		if (sscanf(temp, "%d:%d", &major, &minor) != 2) return;
		if (mknod(device_name, mode | type, makedev(major, minor)) && errno != EEXIST)
			bb_perror_msg_and_die("mknod %s", device_name);

		if (major == root_major && minor == root_minor)
			symlink(device_name, "root");

		if (ENABLE_FEATURE_MDEV_CONF) chown(device_name, uid, gid);
	}
	if (command) {
		/* setenv will leak memory, so use putenv */
		char *s = xasprintf("MDEV=%s", device_name);
		putenv(s);
		if (system(command) == -1)
			bb_perror_msg_and_die("cannot run %s", command);
		s[4] = '\0';
		unsetenv(s);
		free(s);
		free(command);
	}
	if (delete) unlink(device_name);
}

/* File callback for /sys/ traversal */
static int fileAction(const char *fileName, struct stat *statbuf,
                      void *userData, int depth)
{
	size_t len = strlen(fileName) - 4;
	char *scratch = userData;

	if (strcmp(fileName + len, "/dev"))
		return FALSE;

	strcpy(scratch, fileName);
	scratch[len] = 0;
	make_device(scratch, 0);

	return TRUE;
}

/* Directory callback for /sys/ traversal */
static int dirAction(const char *fileName, struct stat *statbuf,
                      void *userData, int depth)
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

	/* check for $FIRMWARE from kernel */
	/* XXX: dont bother: open(NULL) works same as open("no-such-file")
	 * if (!firmware)
	 *	return;
	 */

	/* check for /lib/firmware/$FIRMWARE */
	xchdir("/lib/firmware");
	firmware_fd = xopen(firmware, O_RDONLY);

	/* in case we goto out ... */
	data_fd = -1;

	/* check for /sys/$DEVPATH/loading ... give 30 seconds to appear */
	xchdir(sysfs_path);
	for (cnt = 0; cnt < 30; ++cnt) {
		loading_fd = open("loading", O_WRONLY);
		if (loading_fd == -1)
			sleep(1);
		else
			break;
	}
	if (loading_fd == -1)
		goto out;

	/* tell kernel we're loading by `echo 1 > /sys/$DEVPATH/loading` */
	if (write(loading_fd, "1", 1) != 1)
		goto out;

	/* load firmware by `cat /lib/firmware/$FIRMWARE > /sys/$DEVPATH/data */
	data_fd = open("data", O_WRONLY);
	if (data_fd == -1)
		goto out;
	cnt = bb_copyfd_eof(firmware_fd, data_fd);

	/* tell kernel result by `echo [0|-1] > /sys/$DEVPATH/loading` */
	if (cnt > 0)
		write(loading_fd, "0", 1);
	else
		write(loading_fd, "-1", 2);

 out:
	if (ENABLE_FEATURE_CLEAN_UP) {
		close(firmware_fd);
		close(loading_fd);
		close(data_fd);
	}
}

int mdev_main(int argc, char **argv);
int mdev_main(int argc, char **argv)
{
	char *action;
	char *env_path;
	RESERVE_CONFIG_BUFFER(temp,PATH_MAX);

	xchdir("/dev");

	/* Scan */

	if (argc == 2 && !strcmp(argv[1],"-s")) {
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

	/* Hotplug */

	} else {
		action = getenv("ACTION");
		env_path = getenv("DEVPATH");
		if (!action || !env_path)
			bb_show_usage();

		sprintf(temp, "/sys%s", env_path);
		if (!strcmp(action, "remove"))
			make_device(temp, 1);
		else if (!strcmp(action, "add")) {
			make_device(temp, 0);

			if (ENABLE_FEATURE_MDEV_LOAD_FIRMWARE)
				load_firmware(getenv("FIRMWARE"), temp);
		}
	}

	if (ENABLE_FEATURE_CLEAN_UP) RELEASE_CONFIG_BUFFER(temp);
	return 0;
}
