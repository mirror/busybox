/* vi: set sw=4 ts=4: */
/*
 * public domain -- Dave 'Kill a Cop' Cinege <dcinege@psychosis.com>
 *
 * makedevs
 * Make ranges of device files quickly.
 * known bugs: can't deal with alpha ranges
 */

#include "busybox.h"

#ifdef CONFIG_FEATURE_MAKEDEVS_LEAF
int makedevs_main(int argc, char **argv);
int makedevs_main(int argc, char **argv)
{
	mode_t mode;
	char *basedev, *type, *nodname, buf[255];
	int Smajor, Sminor, S, E;

	if (argc < 7 || *argv[1]=='-')
		bb_show_usage();

	basedev = argv[1];
	type = argv[2];
	Smajor = xatoi_u(argv[3]);
	Sminor = xatoi_u(argv[4]);
	S = xatoi_u(argv[5]);
	E = xatoi_u(argv[6]);
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
			bb_error_msg("failed to create: %s", nodname);

		if (nodname == basedev) /* ex. /dev/hda - to /dev/hda1 ... */
			nodname = buf;
		S++;
		Sminor++;
	}

	return 0;
}

#elif defined CONFIG_FEATURE_MAKEDEVS_TABLE

/* Licensed under the GPL v2 or later, see the file LICENSE in this tarball. */

int makedevs_main(int argc, char **argv);
int makedevs_main(int argc, char **argv)
{
	FILE *table = stdin;
	char *rootdir = NULL;
	char *line = NULL;
	int linenum = 0;
	int ret = EXIT_SUCCESS;

	getopt32(argc, argv, "d:", &line);
	if (line)
		table = xfopen(line, "r");

	if (optind >= argc || (rootdir=argv[optind])==NULL) {
		bb_error_msg_and_die("root directory not specified");
	}

	xchdir(rootdir);

	umask(0);

	printf("rootdir=%s\n", rootdir);
	if (line) {
		printf("table='%s'\n", line);
	} else {
		printf("table=<stdin>\n");
	}

	while ((line = xmalloc_getline(table))) {
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
			bb_error_msg("line %d invalid: '%s'", linenum, line);
			ret = EXIT_FAILURE;
			continue;
		}
		if (name[0] == '#') {
			continue;
		}

		gid = (*group) ? get_ug_id(group, xgroup2gid) : getgid();
		uid = (*user) ? get_ug_id(user, xuname2uid) : getuid();
		full_name = concat_path_file(rootdir, name);

		if (type == 'd') {
			bb_make_directory(full_name, mode | S_IFDIR, FILEUTILS_RECUR);
			if (chown(full_name, uid, gid) == -1) {
				bb_perror_msg("line %d: chown failed for %s", linenum, full_name);
				ret = EXIT_FAILURE;
				goto loop;
			}
			if ((mode != -1) && (chmod(full_name, mode) < 0)){
				bb_perror_msg("line %d: chmod failed for %s", linenum, full_name);
				ret = EXIT_FAILURE;
				goto loop;
			}
		} else if (type == 'f') {
			struct stat st;
			if ((stat(full_name, &st) < 0 || !S_ISREG(st.st_mode))) {
				bb_perror_msg("line %d: regular file '%s' does not exist", linenum, full_name);
				ret = EXIT_FAILURE;
				goto loop;
			}
			if (chown(full_name, uid, gid) == -1) {
				bb_perror_msg("line %d: chown failed for %s", linenum, full_name);
				ret = EXIT_FAILURE;
				goto loop;
			}
			if ((mode != -1) && (chmod(full_name, mode) < 0)){
				bb_perror_msg("line %d: chmod failed for %s", linenum, full_name);
				ret = EXIT_FAILURE;
				goto loop;
			}
		} else
		{
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
				bb_error_msg("line %d: unsupported file type %c", linenum, type);
				ret = EXIT_FAILURE;
				goto loop;
			}

			if (count > 0) {
				int i;
				char *full_name_inc;

				full_name_inc = xmalloc(strlen(full_name) + 4);
				for (i = start; i < count; i++) {
					sprintf(full_name_inc, "%s%d", full_name, i);
					rdev = makedev(major, minor + (i * increment - start));
					if (mknod(full_name_inc, mode, rdev) == -1) {
						bb_perror_msg("line %d: cannot create node %s", linenum, full_name_inc);
						ret = EXIT_FAILURE;
					}
					else if (chown(full_name_inc, uid, gid) == -1) {
						bb_perror_msg("line %d: chown failed for %s", linenum, full_name_inc);
						ret = EXIT_FAILURE;
					}
					if ((mode != -1) && (chmod(full_name_inc, mode) < 0)){
						bb_perror_msg("line %d: chmod failed for %s", linenum, full_name_inc);
						ret = EXIT_FAILURE;
					}
				}
				free(full_name_inc);
			} else {
				rdev = makedev(major, minor);
				if (mknod(full_name, mode, rdev) == -1) {
					bb_perror_msg("line %d: cannot create node %s", linenum, full_name);
					ret = EXIT_FAILURE;
				}
				else if (chown(full_name, uid, gid) == -1) {
					bb_perror_msg("line %d: chown failed for %s", linenum, full_name);
					ret = EXIT_FAILURE;
				}
				if ((mode != -1) && (chmod(full_name, mode) < 0)){
					bb_perror_msg("line %d: chmod failed for %s", linenum, full_name);
					ret = EXIT_FAILURE;
				}
			}
		}
loop:
		free(line);
		free(full_name);
	}
	fclose(table);

	return ret;
}

#else
# error makedevs configuration error, either leaf or table must be selected
#endif
