/* vi: set sw=4 ts=4: */
/*
 * Mini run-parts implementation for busybox
 *
 * Copyright (C) 2007 Bernhard Fischer
 *
 * Based on a older version that was in busybox which was 1k big..
 *   Copyright (C) 2001 by Emanuele Aina <emanuele.aina@tiscali.it>
 *
 * Based on the Debian run-parts program, version 1.15
 *   Copyright (C) 1996 Jeff Noxon <jeff@router.patch.net>,
 *   Copyright (C) 1996-1999 Guy Maor <maor@debian.org>
 *
 *
 * Licensed under GPL v2 or later, see file LICENSE in this tarball for details.
 */

/* This is my first attempt to write a program in C (well, this is my first
 * attempt to write a program! :-) . */

/* This piece of code is heavily based on the original version of run-parts,
 * taken from debian-utils. I've only removed the long options and a the
 * report mode. As the original run-parts support only long options, I've
 * broken compatibility because the BusyBox policy doesn't allow them.
 * The supported options are:
 * -t			test. Print the name of the files to be executed, without
 *				execute them.
 * -a ARG		argument. Pass ARG as an argument the program executed. It can
 *				be repeated to pass multiple arguments.
 * -u MASK		umask. Set the umask of the program executed to MASK.
 */

#include <getopt.h>

#include "libbb.h"

#if ENABLE_FEATURE_RUN_PARTS_LONG_OPTIONS
static const char runparts_longopts[] ALIGN1 =
	"arg\0"     Required_argument "a"
	"umask\0"   Required_argument "u"
	"test\0"    No_argument       "t"
#if ENABLE_FEATURE_RUN_PARTS_FANCY
	"list\0"    No_argument       "l"
//TODO: "reverse\0" No_argument       "r"
//TODO: "verbose\0" No_argument       "v"
#endif
	;
#endif

struct globals {
	smalluint mode;
	char *cmd[10]; /* merely arbitrary arg count */
};
#define G (*(struct globals*)&bb_common_bufsiz1)

/* valid_name */
/* True or false? Is this a valid filename (upper/lower alpha, digits,
 * underscores, and hyphens only?)
 */
static bool invalid_name(const char *c)
{
	c = bb_basename(c);

	while (*c && (isalnum(*c) || *c == '_' || *c == '-'))
		c++;

	return *c; /* TRUE (!0) if terminating NUL is not reached */
}

#define RUN_PARTS_OPT_a (1<<0)
#define RUN_PARTS_OPT_u (1<<1)
#define RUN_PARTS_OPT_t (1<<2)
#if ENABLE_FEATURE_RUN_PARTS_FANCY
#define RUN_PARTS_OPT_l (1<<3)
#endif

#define test_mode (G.mode & RUN_PARTS_OPT_t)
#if ENABLE_FEATURE_RUN_PARTS_FANCY
#define list_mode (G.mode & RUN_PARTS_OPT_l)
#else
#define list_mode (0)
#endif

static int act(const char *file, struct stat *statbuf, void *args, int depth)
{
	int ret;

	if (depth == 1)
		return TRUE;

	if (depth == 2 &&
		((!list_mode && access(file, X_OK)) ||
		 invalid_name(file) ||
		 !(statbuf->st_mode & (S_IFREG | S_IFLNK))) )
		return SKIP;

	if (test_mode || list_mode) {
		puts(file);
		return TRUE;
	}
	G.cmd[0] = (char*)file;
	ret = wait4pid(spawn(G.cmd));
	if (ret < 0) {
		bb_perror_msg("failed to exec %s", file);
	} else if (ret > 0) {
		bb_error_msg("%s exited with return code %d", file, ret);
	}
	return !ret;
}

int run_parts_main(int argc, char **argv);
int run_parts_main(int argc, char **argv)
{
	char *umask_p;
	llist_t *arg_list = NULL;
	unsigned tmp;

	umask(022);
	/* We require exactly one argument: the directory name */
	opt_complementary = "=1:a::";
#if ENABLE_FEATURE_RUN_PARTS_LONG_OPTIONS
	applet_long_options = runparts_longopts;
#endif
	tmp = getopt32(argv, "a:u:t"USE_FEATURE_RUN_PARTS_FANCY("l"), &arg_list, &umask_p);
	G.mode = tmp &~ (RUN_PARTS_OPT_a|RUN_PARTS_OPT_u);
	if (tmp & RUN_PARTS_OPT_u) {
		/* Check and set the umask of the program executed.
		 * As stated in the original run-parts, the octal conversion in
		 * libc is not foolproof; it will take the 8 and 9 digits under
		 * some circumstances. We'll just have to live with it.
		 */
		umask(xstrtoul_range(umask_p, 8, 0, 07777));
	}
	for (tmp = 1; arg_list; arg_list = arg_list->link, tmp++)
		G.cmd[tmp] = arg_list->data;
	/* G.cmd[tmp] = NULL; - G is already zeroed out */
	if (!recursive_action(argv[argc - 1],
			ACTION_RECURSE|ACTION_FOLLOWLINKS,
			act,		/* file action */
			act,		/* dir action */
			NULL,		/* user data */
			1			/* depth */
			))
			return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
