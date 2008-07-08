/* mini man implementation for busybox
 * Copyright (C) 2008 Denys Vlasenko <vda.linux@googlemail.com>
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

enum {
	OPT_a = 1, /* all */
	OPT_w = 2, /* print path */
};

/* This is what I see on my desktop system being executed:

(
echo ".ll 12.4i"
echo ".nr LL 12.4i"
echo ".pl 1100i"
gunzip -c '/usr/man/man1/bzip2.1.gz'
echo ".\\\""
echo ".pl \n(nlu+10"
) | gtbl | nroff -Tlatin1 -mandoc | less

*/

/* Trick gcc to reuse "cat" string. */
#define STR_catNULmanNUL "cat\0man"
#define STR_cat          "cat\0man"

static int run_pipe(const char *unpacker, const char *pager, char *man_filename, int man)
{
	char *cmd;

	if (access(man_filename, R_OK) != 0)
		return 0;

	if (option_mask32 & OPT_w) {
		puts(man_filename);
		return 1;
	}

	/* "2>&1" is added so that nroff errors are shown in pager too.
	 * Otherwise it may show just empty screen */
	cmd = xasprintf(
		man ? "%s '%s' | gtbl | nroff -Tlatin1 -mandoc 2>&1 | %s"
		    : "%s '%s' | %s",
		unpacker, man_filename, pager);
	system(cmd);
	free(cmd);
	return 1;
}

/* man_filename is of the form "/dir/dir/dir/name.s.bz2" */
static int show_manpage(const char *pager, char *man_filename, int man)
{
	int len;

	if (run_pipe("bunzip2 -c", pager, man_filename, man))
		return 1;

	len = strlen(man_filename) - 1;

	man_filename[len] = '\0'; /* ".bz2" -> ".gz" */
	man_filename[len - 2] = 'g';
	if (run_pipe("gunzip -c", pager, man_filename, man))
		return 1;

	man_filename[len - 3] = '\0'; /* ".gz" -> "" */
	if (run_pipe(STR_cat, pager, man_filename, man))
		return 1;

	return 0;
}

int man_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int man_main(int argc UNUSED_PARAM, char **argv)
{
	FILE *cf;
	const char *pager;
	char **man_path_list;
	char *sec_list;
	char *cur_path, *cur_sect;
	char *line, *value;
	int count_mp, cur_mp;
	int opt, not_found;

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
					man_path_list = xrealloc_vector(man_path_list, 4, count_mp);
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

// TODO: my man3/getpwuid.3.gz contains just one line:
// .so man3/getpwnam.3
// (and I _dont_ have man3/getpwnam.3, I have man3/getpwnam.3.gz)
// need to support this...

	not_found = 0;
	do { /* for each argv[] */
		int found = 0;
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
					char *man_filename;
					int cat0man1 = 0;

					/* Search for cat, then man page */
					while (cat0man1 < 2) {
						int found_here;
						man_filename = xasprintf("%.*s/%s%.*s/%s.%.*s" ".bz2",
								path_len, cur_path,
								STR_catNULmanNUL + cat0man1 * 4,
								sect_len, cur_sect,
								*argv,
								sect_len, cur_sect);
						found_here = show_manpage(pager, man_filename, cat0man1);
						found |= found_here;
						cat0man1 += found_here + 1;
						free(man_filename);
					}

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
		if (!found) {
			bb_error_msg("no manual entry for '%s'", *argv);
			not_found = 1;
		}
 next_arg:
		argv++;
	} while (*argv);

	return not_found;
}
