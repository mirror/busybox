/* vi: set sw=4 ts=4: */
/*
 * Sysctl 1.01 - A utility to read and manipulate the sysctl parameters
 *
 * Copyright 1999 George Staikos
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * Changelog:
 *	v1.01:
 *		- added -p <preload> to preload values from a file
 *	v1.01.1
 *		- busybox applet aware by <solar@gentoo.org>
 *
 */

#include "libbb.h"

/*
 *    Function Prototypes
 */
static int sysctl_read_setting(const char *setting, int output);
static int sysctl_write_setting(const char *setting, int output);
static int sysctl_preload_file(const char *filename, int output);
static int sysctl_display_all(const char *path, int output, int show_table);

/*
 *    Globals...
 */
static const char ETC_SYSCTL_CONF[] ALIGN1 = "/etc/sysctl.conf";
static const char PROC_SYS[] ALIGN1 = "/proc/sys/";
enum { strlen_PROC_SYS = sizeof(PROC_SYS) - 1 };

/* error messages */
static const char ERR_UNKNOWN_PARAMETER[] ALIGN1 =
	"error: unknown parameter '%s'";
static const char ERR_MALFORMED_SETTING[] ALIGN1 =
	"error: malformed setting '%s'";
static const char ERR_NO_EQUALS[] ALIGN1 =
	"error: '%s' must be of the form name=value";
static const char ERR_INVALID_KEY[] ALIGN1 =
	"error: '%s' is an unknown key";
static const char ERR_UNKNOWN_WRITING[] ALIGN1 =
	"error setting key '%s'";
static const char ERR_UNKNOWN_READING[] ALIGN1 =
	"error reading key '%s'";
static const char ERR_PERMISSION_DENIED[] ALIGN1 =
	"error: permission denied on key '%s'";
static const char ERR_PRELOAD_FILE[] ALIGN1 =
	"error: cannot open preload file '%s'";
static const char WARN_BAD_LINE[] ALIGN1 =
	"warning: %s(%d): invalid syntax, continuing";


static void dwrite_str(int fd, const char *buf)
{
	write(fd, buf, strlen(buf));
}

/*
 *    sysctl_main()...
 */
int sysctl_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int sysctl_main(int argc, char **argv)
{
	int retval = 0;
	int output = 1;
	int write_mode = 0;
	int switches_allowed = 1;

	if (argc < 2)
		bb_show_usage();

	argv++;

	for (; *argv /*&& **argv*/; argv++) {
		if (switches_allowed && **argv == '-') {	/* we have a switch */
			switch ((*argv)[1]) {
			case 'n':
				output = 0;
				break;
			case 'w':
				write_mode = 1;
				switches_allowed = 0;
				break;
			case 'p':
				argv++;
				return sysctl_preload_file(((*argv /*&& **argv*/) ? *argv : ETC_SYSCTL_CONF),
							output);
			case 'a':
			case 'A':
				return sysctl_display_all(PROC_SYS, output,
							((*argv)[1] == 'A'));
			default:
				bb_error_msg(ERR_UNKNOWN_PARAMETER, *argv);
				/* fall through */
			//case 'h':
			//case '?':
				bb_show_usage();
			}
		} else {
			switches_allowed = 0;
			if (write_mode)
				retval |= sysctl_write_setting(*argv, output);
			else
				sysctl_read_setting(*argv, output);
		}
	}
	return retval;
} /* end sysctl_main() */

/*
 * sysctl_preload_file
 * preload the sysctl's from a conf file
 * - we parse the file and then reform it (strip out whitespace)
 */
#define PRELOAD_BUF 256

static int sysctl_preload_file(const char *filename, int output)
{
	int lineno;
	char oneline[PRELOAD_BUF];
	char buffer[PRELOAD_BUF];
	char *name, *value;
	FILE *fp;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		bb_error_msg_and_die(ERR_PRELOAD_FILE, filename);
	}

	lineno = 0;
	while (fgets(oneline, sizeof(oneline) - 1, fp)) {
		lineno++;
		trim(oneline);
		if (oneline[0] == '#' || oneline[0] == ';')
			continue;
		if (!oneline[0] || !oneline[1])
			continue;

		name = strtok(oneline, "=");
		if (!name) {
			bb_error_msg(WARN_BAD_LINE, filename, lineno);
			continue;
		}
		trim(name);
		if (!*name) {
			bb_error_msg(WARN_BAD_LINE, filename, lineno);
			continue;
		}

		value = strtok(NULL, "\n\r");
		if (!value) {
			bb_error_msg(WARN_BAD_LINE, filename, lineno);
			continue;
		}
		while (*value == ' ' || *value == '\t')
			value++;
		if (!*value) {
			bb_error_msg(WARN_BAD_LINE, filename, lineno);
			continue;
		}

		/* safe because sizeof(oneline) == sizeof(buffer) */
		sprintf(buffer, "%s=%s", name, value);
		sysctl_write_setting(buffer, output);
	}
	fclose(fp);
	return 0;
} /* end sysctl_preload_file() */

/*
 *     Write a single sysctl setting
 */
static int sysctl_write_setting(const char *setting, int output)
{
	int retval = 0;
	const char *name;
	const char *value;
	const char *equals;
	char *tmpname, *outname, *cptr;
	int fd = -1;

	name = setting;
	equals = strchr(setting, '=');
	if (!equals) {
		bb_error_msg(ERR_NO_EQUALS, setting);
		return -1;
	}

	value = equals + 1;	/* point to the value in name=value */
	if (name == equals || !*value) {
		bb_error_msg(ERR_MALFORMED_SETTING, setting);
		return -2;
	}

	tmpname = xasprintf("%s%.*s", PROC_SYS, (int)(equals - name), name);
	outname = xstrdup(tmpname + strlen_PROC_SYS);

	while ((cptr = strchr(tmpname, '.')) != NULL)
		*cptr = '/';

	while ((cptr = strchr(outname, '/')) != NULL)
		*cptr = '.';

	fd = open(tmpname, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd < 0) {
		switch (errno) {
		case ENOENT:
			bb_error_msg(ERR_INVALID_KEY, outname);
			break;
		case EACCES:
			bb_perror_msg(ERR_PERMISSION_DENIED, outname);
			break;
		default:
			bb_perror_msg(ERR_UNKNOWN_WRITING, outname);
			break;
		}
		retval = -1;
	} else {
		dwrite_str(fd, value);
		close(fd);
		if (output) {
			dwrite_str(STDOUT_FILENO, outname);
			dwrite_str(STDOUT_FILENO, " = ");
		}
		dwrite_str(STDOUT_FILENO, value);
		dwrite_str(STDOUT_FILENO, "\n");
	}

	free(tmpname);
	free(outname);
	return retval;
} /* end sysctl_write_setting() */

/*
 *     Read a sysctl setting
 */
static int sysctl_read_setting(const char *setting, int output)
{
	int retval = 0;
	char *tmpname, *outname, *cptr;
	char inbuf[1025];
	const char *name;
	FILE *fp;

	if (!*setting)
		bb_error_msg(ERR_INVALID_KEY, setting);

	name = setting;
	tmpname = concat_path_file(PROC_SYS, name);
	outname = xstrdup(tmpname + strlen_PROC_SYS);

	while ((cptr = strchr(tmpname, '.')) != NULL)
		*cptr = '/';
	while ((cptr = strchr(outname, '/')) != NULL)
		*cptr = '.';

	fp = fopen(tmpname, "r");
	if (fp == NULL) {
		switch (errno) {
		case ENOENT:
			bb_error_msg(ERR_INVALID_KEY, outname);
			break;
		case EACCES:
			bb_error_msg(ERR_PERMISSION_DENIED, outname);
			break;
		default:
			bb_perror_msg(ERR_UNKNOWN_READING, outname);
			break;
		}
		retval = -1;
	} else {
		while (fgets(inbuf, sizeof(inbuf) - 1, fp)) {
			if (output) {
				dwrite_str(STDOUT_FILENO, outname);
				dwrite_str(STDOUT_FILENO, " = ");
			}
			dwrite_str(STDOUT_FILENO, inbuf);
		}
		fclose(fp);
	}

	free(tmpname);
	free(outname);
	return retval;
} /* end sysctl_read_setting() */

/*
 *     Display all the sysctl settings
 */
static int sysctl_display_all(const char *path, int output, int show_table)
{
	int retval = 0;
	DIR *dp;
	struct dirent *de;
	char *tmpdir;
	struct stat ts;

	dp = opendir(path);
	if (!dp) {
		return -1;
	}
	while ((de = readdir(dp)) != NULL) {
		tmpdir = concat_subpath_file(path, de->d_name);
		if (tmpdir == NULL)
			continue;
		if (stat(tmpdir, &ts) != 0) {
			bb_perror_msg(tmpdir);
		} else if (S_ISDIR(ts.st_mode)) {
			sysctl_display_all(tmpdir, output, show_table);
		} else {
			retval |= sysctl_read_setting(tmpdir + strlen_PROC_SYS, output);
		}
		free(tmpdir);
	} /* end while */
	closedir(dp);

	return retval;
} /* end sysctl_display_all() */
