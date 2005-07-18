/* vi: set sw=4 ts=4: */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysmacros.h>     /* major() and minor() */
#include "busybox.h"

#ifdef CONFIG_FEATURE_MAKEDEVS_LEAF
/*
 * public domain -- Dave 'Kill a Cop' Cinege <dcinege@psychosis.com>
 *
 * makedevs
 * Make ranges of device files quickly.
 * known bugs: can't deal with alpha ranges
 */
int makedevs_main(int argc, char **argv)
{
	mode_t mode;
	char *basedev, *type, *nodname, buf[255];
	int Smajor, Sminor, S, E;

	if (argc < 7 || *argv[1]=='-')
		bb_show_usage();

	basedev = argv[1];
	type = argv[2];
	Smajor = atoi(argv[3]);
	Sminor = atoi(argv[4]);
	S = atoi(argv[5]);
	E = atoi(argv[6]);
	nodname = argc == 8 ? basedev : buf;

	mode = 0660;

	switch (type[0]) {
	case 'c':
		mode |= S_IFCHR;
		break;
	case 'b':
		mode |= S_IFBLK;
		break;
	case 'f':
		mode |= S_IFIFO;
		break;
	default:
		bb_show_usage();
	}

	while (S <= E) {
		int sz;

		sz = snprintf(buf, sizeof(buf), "%s%d", basedev, S);
		if(sz<0 || sz>=sizeof(buf))  /* libc different */
			bb_error_msg_and_die("%s too large", basedev);

	/* if mode != S_IFCHR and != S_IFBLK third param in mknod() ignored */

		if (mknod(nodname, mode, makedev(Smajor, Sminor)))
			bb_error_msg("Failed to create: %s", nodname);

		if (nodname == basedev) /* ex. /dev/hda - to /dev/hda1 ... */
			nodname = buf;
		S++;
		Sminor++;
	}

	return 0;
}

#elif defined CONFIG_FEATURE_MAKEDEVS_TABLE

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

extern int makedevs_main(int argc, char **argv)
{
	int opt;
	FILE *table = stdin;
	char *rootdir = NULL;
	char *line = NULL;
	int linenum = 0;
	int ret = EXIT_SUCCESS;

	while ((opt = getopt(argc, argv, "d:")) != -1) {
		switch(opt) {
			case 'd':
				table = bb_xfopen((line=optarg), "r");
				break;
			default:
				bb_show_usage();
		}
	}

	if (optind >= argc || (rootdir=argv[optind])==NULL) {
		bb_error_msg_and_die("root directory not speficied");
	}

	if (chdir(rootdir) != 0) {
		bb_perror_msg_and_die("Couldnt chdir to %s", rootdir);
	}

	umask(0);

	printf("rootdir=%s\n", rootdir);
	if (line) {
		printf("table='%s'\n", line);
	} else {
		printf("table=<stdin>\n");
	}

	while ((line = bb_get_chomped_line_from_file(table))) {
		char type;
		unsigned int mode = 0755;
		unsigned int major = 0;
		unsigned int minor = 0;
		unsigned int count = 0;
		unsigned int increment = 0;
		unsigned int start = 0;
		char name[41];
		char user[41];
		char group[41];
		char *full_name;
		uid_t uid;
		gid_t gid;

		linenum++;

		if ((2 > sscanf(line, "%40s %c %o %40s %40s %u %u %u %u %u", name,
						&type, &mode, user, group, &major,
						&minor, &start, &increment, &count)) ||
				((major | minor | start | count | increment) > 255))
		{
			if (*line=='\0' || *line=='#' || isspace(*line))
				continue;
			bb_error_msg("line %d invalid: '%s'\n", linenum, line);
			ret = EXIT_FAILURE;
			continue;
		}
		if (name[0] == '#') {
			continue;
		}
		if (group) {
			gid = get_ug_id(group, my_getgrnam);
		} else {
			gid = getgid();
		}
		if (user) {
			uid = get_ug_id(user, my_getpwnam);
		} else {
			uid = getuid();
		}
		full_name = concat_path_file(rootdir, name);

		if (type == 'd') {
			bb_make_directory(full_name, mode | S_IFDIR, FILEUTILS_RECUR);
			if (chown(full_name, uid, gid) == -1) {
				bb_perror_msg("line %d: chown failed for %s", linenum, full_name);
				ret = EXIT_FAILURE;
				goto loop;
			}
		} else {
			dev_t rdev;

			if (type == 'p') {
				mode |= S_IFIFO;
			}
			else if (type == 'c') {
				mode |= S_IFCHR;
			}
			else if (type == 'b') {
				mode |= S_IFBLK;
			} else {
				bb_error_msg("line %d: Unsupported file type %c", linenum, type);
				ret = EXIT_FAILURE;
				goto loop;
			}

			if (count > 0) {
				int i;
				char *full_name_inc;

				full_name_inc = xmalloc(strlen(full_name) + 4);
				for (i = start; i < count; i++) {
					sprintf(full_name_inc, "%s%d", full_name, i);
					rdev = (major << 8) + minor + (i * increment - start);
					if (mknod(full_name_inc, mode, rdev) == -1) {
						bb_perror_msg("line %d: Couldnt create node %s", linenum, full_name_inc);
						ret = EXIT_FAILURE;
					}
					else if (chown(full_name_inc, uid, gid) == -1) {
						bb_perror_msg("line %d: chown failed for %s", linenum, full_name_inc);
						ret = EXIT_FAILURE;
					}
				}
				free(full_name_inc);
			} else {
				rdev = (major << 8) + minor;
				if (mknod(full_name, mode, rdev) == -1) {
					bb_perror_msg("line %d: Couldnt create node %s", linenum, full_name);
					ret = EXIT_FAILURE;
				}
				else if (chown(full_name, uid, gid) == -1) {
					bb_perror_msg("line %d: chown failed for %s", linenum, full_name);
					ret = EXIT_FAILURE;
				}
			}
		}
loop:
		free(line);
		free(full_name);
	}
	fclose(table);

	return 0;
}

#else
# error makdedevs configuration error, either leaf or table must be selected
#endif
