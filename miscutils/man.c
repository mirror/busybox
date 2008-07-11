/* mini man implementation for busybox
 * Copyright (C) 2008 Denys Vlasenko <vda.linux@googlemail.com>
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

enum {
	OPT_a = 1, /* all */
	OPT_w = 2, /* print path */
};

/* This is what I see on my desktop system deing executed:

(
echo ".ll 12.4i"
echo ".nr LL 12.4i"
echo ".pl 1100i"
gunzip -c '/usr/man/man1/bzip2.1.gz'
echo ".\\\""
echo ".pl \n(nlu+10"
) | gtbl | nroff -Tlatin1 -mandoc | less

*/

static int run_pipe(const char *unpacker, const char *pager, char *man_filename)
{
	char *cmd;

	if (access(man_filename, R_OK) != 0)
		return 0;

	if (option_mask32 & OPT_w) {
		puts(man_filename);
		return 1;
	}

	cmd = xasprintf("%s '%s' | gtbl | nroff -Tlatin1 -mandoc | %s",
			unpacker, man_filename, pager);
	system(cmd);
	free(cmd);
	return 1;
}

/* man_filename is of the form "/dir/dir/dir/name.s.bz2" */
static int show_manpage(const char *pager, char *man_filename)
{
	int len;

	if (run_pipe("bunzip2 -c", pager, man_filename))
		return 1;

	len = strlen(man_filename) - 1;

	man_filename[len] = '\0'; /* ".bz2" -> ".gz" */
	man_filename[len - 2] = 'g';
	if (run_pipe("gunzip -c", pager, man_filename))
		return 1;

	man_filename[len - 3] = '\0'; /* ".gz" -> "" */
	if (run_pipe("cat", pager, man_filename))
		return 1;

	return 0;
}

int man_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int man_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	FILE *cf;
	const char *pager;
	char **man_path_list;
	char *sec_list;
	char *cur_path, *cur_sect;
	char *line, *value;
	int count_mp, cur_mp;
	int opt;

	opt_complementary = "-1"; /* at least one argument */
	opt = getopt32(argv, "+aw");
	argv += optind;

	sec_list = xstrdup("1:2:3:4:5:6:7:8:9");
	/* Last valid man_path_list[] is [0x10] */
	man_path_list = xzalloc(0x11 * sizeof(man_path_list[0]));
	count_mp = 0;
	man_path_list[0] = xstrdup(getenv("MANPATH"));
	if (man_path_list[0])
		count_mp++;
	pager = getenv("MANPAGER");
	if (!pager) {
		pager = getenv("PAGER");
		if (!pager)
			pager = "more";
	}

	/* Parse man.conf */
	cf = fopen_or_warn("/etc/man.conf", "r");
	if (cf) {
		/* go through man configuration file and search relevant paths, sections */
		while ((line = xmalloc_fgetline(cf)) != NULL) {
			trim(line); /* remove whitespace at the beginning/end */
			if (isspace(line[7])) {
				line[7] = '\0';
				value = skip_whitespace(&line[8]);
				*skip_non_whitespace(value) = '\0';
				if (strcmp("MANPATH", line) == 0) {
					man_path_list[count_mp] = xstrdup(value);
					count_mp++;
					/* man_path_list is NULL terminated */
					man_path_list[count_mp] = NULL;
					if (!(count_mp & 0xf)) { /* 0x10, 0x20 etc */
						/* so that last valid man_path_list[] is [count_mp + 0x10] */
						man_path_list = xrealloc(man_path_list,
							(count_mp + 0x11) * sizeof(man_path_list[0]));
					}
				}
				if (strcmp("MANSECT", line) == 0) {
					free(sec_list);
					sec_list = xstrdup(value);
				}
			}
			free(line);
		}
		fclose(cf);
	}

	do { /* for each argv[] */
		cur_mp = 0;
		while ((cur_path = man_path_list[cur_mp++]) != NULL) {
			/* for each MANPATH */
			do { /* for each MANPATH item */
				char *next_path = strchrnul(cur_path, ':');
				int path_len = next_path - cur_path;
				cur_sect = sec_list;
				do { /* for each section */
					char *next_sect = strchrnul(cur_sect, ':');
					int sect_len = next_sect - cur_sect;

					char *man_filename = xasprintf("%.*s/man%.*s/%s.%.*s" ".bz2",
								path_len, cur_path,
								sect_len, cur_sect,
								*argv,
								sect_len, cur_sect);
					int found = show_manpage(pager, man_filename);
					free(man_filename);
					if (found && !(opt & OPT_a))
						goto next_arg;
					cur_sect = next_sect;
					while (*cur_sect == ':')
						cur_sect++;
				} while (*cur_sect);
				cur_path = next_path;
				while (*cur_path == ':')
					cur_path++;
			} while (*cur_path);
		}
 next_arg:
		argv++;
	} while (*argv);

	return EXIT_SUCCESS;
}
