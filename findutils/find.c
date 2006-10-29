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
 * # find file.txt -exec 'echo' '{}  {}' ';' -print -exec pwd ';'
 * file.txt  file.txt
 * file.txt
 * /tmp
 * # find -name '*.c' -o -name '*.h'
 * [shows files, *.c and *.h intermixed]
 * # find file.txt -name '*f*' -o -name '*t*'
 * file.txt
 * # find file.txt -name '*z*' -o -name '*t*'
 * file.txt
 * # find file.txt -name '*f*' -o -name '*z*'
 * file.txt
 */

#include "busybox.h"
#include <fnmatch.h>

USE_FEATURE_FIND_XDEV(static dev_t *xdev_dev;)
USE_FEATURE_FIND_XDEV(static int xdev_count;)

typedef int (*action_fp)(const char *fileName, struct stat *statbuf, void *);

typedef struct {
	action_fp f;
} action;
#define SACT(name, arg...) typedef struct { action a; arg; } action_##name;
#define SFUNC(name)        static int func_##name(const char *fileName, struct stat *statbuf, action_##name* ap)
                        SACT(print)
                        SACT(name,  char *pattern;)
USE_FEATURE_FIND_PRINT0(SACT(print0))
USE_FEATURE_FIND_TYPE(  SACT(type,  int type_mask;))
USE_FEATURE_FIND_PERM(  SACT(perm,  char perm_char; int perm_mask;))
USE_FEATURE_FIND_MTIME( SACT(mtime, char mtime_char; int mtime_days;))
USE_FEATURE_FIND_MMIN(  SACT(mmin,  char mmin_char; int mmin_mins;))
USE_FEATURE_FIND_NEWER( SACT(newer, time_t newer_mtime;))
USE_FEATURE_FIND_INUM(  SACT(inum,  ino_t inode_num;))
USE_FEATURE_FIND_EXEC(  SACT(exec,  char **exec_argv; int *subst_count; int exec_argc;))

static action ***actions;


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
	return buf;
}


SFUNC(name)
{
	const char *tmp = strrchr(fileName, '/');
	if (tmp == NULL)
		tmp = fileName;
	else
		tmp++;
	return fnmatch(ap->pattern, tmp, FNM_PERIOD) == 0;
}
#if ENABLE_FEATURE_FIND_TYPE
SFUNC(type)
{
	return !((statbuf->st_mode & S_IFMT) == ap->type_mask);
}
#endif
#if ENABLE_FEATURE_FIND_PERM
SFUNC(perm)
{
	return !((isdigit(ap->perm_char) && (statbuf->st_mode & 07777) == ap->perm_mask)
	        || (ap->perm_char == '-' && (statbuf->st_mode & ap->perm_mask) == ap->perm_mask)
	        || (ap->perm_char == '+' && (statbuf->st_mode & ap->perm_mask) != 0));
}
#endif
#if ENABLE_FEATURE_FIND_MTIME
SFUNC(mtime)
{
	time_t file_age = time(NULL) - statbuf->st_mtime;
	time_t mtime_secs = ap->mtime_days * 24 * 60 * 60;
	return !((isdigit(ap->mtime_char) && file_age >= mtime_secs
	                                  && file_age < mtime_secs + 24 * 60 * 60)
	        || (ap->mtime_char == '+' && file_age >= mtime_secs + 24 * 60 * 60)
	        || (ap->mtime_char == '-' && file_age < mtime_secs));
}
#endif
#if ENABLE_FEATURE_FIND_MMIN
SFUNC(mmin)
{
	time_t file_age = time(NULL) - statbuf->st_mtime;
	time_t mmin_secs = ap->mmin_mins * 60;
	return !((isdigit(ap->mmin_char) && file_age >= mmin_secs
	                                 && file_age < mmin_secs + 60)
	        || (ap->mmin_char == '+' && file_age >= mmin_secs + 60)
	        || (ap->mmin_char == '-' && file_age < mmin_secs));
}
#endif
#if ENABLE_FEATURE_FIND_NEWER
SFUNC(newer)
{
	return (ap->newer_mtime >= statbuf->st_mtime);
}
#endif
#if ENABLE_FEATURE_FIND_INUM
SFUNC(inum)
{
	return (statbuf->st_ino != ap->inode_num);
}
#endif
#if ENABLE_FEATURE_FIND_EXEC
SFUNC(exec)
{
	int i, rc;
	char *argv[ap->exec_argc+1];
	for (i = 0; i < ap->exec_argc; i++)
		argv[i] = subst(ap->exec_argv[i], ap->subst_count[i], fileName);
	argv[i] = NULL; /* terminate the list */
	errno = 0;
	rc = wait4pid(spawn(argv));
	if (errno)
		bb_perror_msg("%s", argv[0]);
	for (i = 0; i < ap->exec_argc; i++)
		free(argv[i]);
	return rc == 0; /* return 1 if success */
}
#endif

#if ENABLE_FEATURE_FIND_PRINT0
SFUNC(print0)
{
	printf("%s%c", fileName, '\0');
	return TRUE;
}
#endif
SFUNC(print)
{
	puts(fileName);
	return TRUE;
}


static int fileAction(const char *fileName, struct stat *statbuf, void* junk, int depth)
{
	int cur_group;
	int cur_action;
	action **app, *ap;

	cur_group = -1;
	while ((app = actions[++cur_group])) {
		cur_action = -1;
		do {
			ap = app[++cur_action];
		} while (ap && ap->f(fileName, statbuf, ap));
		if (!ap) {
			/* all actions in group were successful */
			break;
		}
	}
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
	int cur_group;
	int cur_action;
	int need_default = 1;

	action* alloc_action(int sizeof_struct, action_fp f)
	{
		action *ap;
		actions[cur_group] = xrealloc(actions[cur_group], (cur_action+2) * sizeof(*actions));
		actions[cur_group][cur_action++] = ap = xmalloc(sizeof_struct);
		actions[cur_group][cur_action] = NULL;
		ap->f = f;
		return ap;
	}
#define ALLOC_ACTION(name) (action_##name*)alloc_action(sizeof(action_##name), (action_fp) func_##name)

	for (firstopt = 1; firstopt < argc; firstopt++) {
		if (argv[firstopt][0] == '-')
			break;
	}
	if (firstopt == 1) {
		argv[0] = ".";
		argv--;
		argc++;
		firstopt++;
	}

// All options always return true. They always take effect,
// rather than being processed only when their place in the
// expression is reached
// We implement: -follow, -xdev

// Actions have side effects and return a true or false value
// We implement: -print, -print0, -exec

// The rest are tests.

// Tests and actions are grouped by operators
// ( expr )              Force precedence
// ! expr                True if expr is false
// -not expr             Same as ! expr
// expr1 [-a[nd]] expr2  And; expr2 is not evaluated if expr1 is false
// expr1 -o[r] expr2     Or; expr2 is not evaluated if expr1 is true
// expr1 , expr2         List; both expr1 and expr2 are always evaluated
// We implement: -a, -o

	cur_group = 0;
	cur_action = 0;
	actions = xzalloc(sizeof(*actions)); /* actions[0] == NULL */

	/* Parse any options */
	for (i = firstopt; i < argc; i++) {
		char *arg = argv[i];
		char *arg1 = argv[i+1];

	/* --- Operators --- */
		if (ENABLE_DESKTOP
		 && (strcmp(arg, "-a") == 0 || strcmp(arg, "-and") == 0)
		) {
			/* no special handling required */
		}
		else if (strcmp(arg, "-o") == 0
			USE_DESKTOP(|| strcmp(arg, "-or") == 0)
		) {
			if (need_default)
				(void) ALLOC_ACTION(print);
			cur_group++;
			actions = xrealloc(actions, (cur_group+1) * sizeof(*actions));
			actions[cur_group] = NULL;
			actions[cur_group+1] = NULL;
			cur_action = 0;
			need_default = 1;
		}

	/* --- Options --- */
		else if (strcmp(arg, "-follow") == 0)
			dereference = TRUE;
#if ENABLE_FEATURE_FIND_XDEV
		else if (strcmp(arg, "-xdev") == 0) {
			struct stat stbuf;

			xdev_count = firstopt - 1;
			xdev_dev = xmalloc(xdev_count * sizeof(dev_t));
			for (j = 1; j < firstopt; i++) {
				/* not xstat(): shouldn't bomd out on
				 * "find not_exist exist -xdev" */
				if (stat(argv[j], &stbuf)) stbuf.st_dev = -1L;
				xdev_dev[j-1] = stbuf.st_dev;
			}
		}
#endif
	/* --- Tests and actions --- */
		else if (strcmp(arg, "-print") == 0) {
			need_default = 0;
			(void) ALLOC_ACTION(print);
		}
#if ENABLE_FEATURE_FIND_PRINT0
		else if (strcmp(arg, "-print0") == 0) {
			need_default = 0;
			(void) ALLOC_ACTION(print0);
		}
#endif
		else if (strcmp(arg, "-name") == 0) {
			action_name *ap;
			if (++i == argc)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			ap = ALLOC_ACTION(name);
			ap->pattern = arg1;
		}
#if ENABLE_FEATURE_FIND_TYPE
		else if (strcmp(arg, "-type") == 0) {
			action_type *ap;
			if (++i == argc)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			ap = ALLOC_ACTION(type);
			ap->type_mask = find_type(arg1);
		}
#endif
#if ENABLE_FEATURE_FIND_PERM
/* TODO:
 * -perm mode   File's permission bits are exactly mode (octal or symbolic).
 *              Symbolic modes use mode 0 as a point of departure.
 * -perm -mode  All of the permission bits mode are set for the file.
 * -perm +mode  Any of the permission bits mode are set for the file.
 */
		else if (strcmp(arg, "-perm") == 0) {
			action_perm *ap;
			if (++i == argc)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			ap = ALLOC_ACTION(perm);
			ap->perm_mask = xstrtol_range(arg1, 8, 0, 07777);
			ap->perm_char = arg1[0];
			if (ap->perm_char == '-')
				ap->perm_mask = -ap->perm_mask;
		}
#endif
#if ENABLE_FEATURE_FIND_MTIME
		else if (strcmp(arg, "-mtime") == 0) {
			action_mtime *ap;
			if (++i == argc)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			ap = ALLOC_ACTION(mtime);
			ap->mtime_days = xatol(arg1);
			ap->mtime_char = arg1[0];
			if (ap->mtime_char == '-')
				ap->mtime_days = -ap->mtime_days;
		}
#endif
#if ENABLE_FEATURE_FIND_MMIN
		else if (strcmp(arg, "-mmin") == 0) {
			action_mmin *ap;
			if (++i == argc)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			ap = ALLOC_ACTION(mmin);
			ap->mmin_mins = xatol(arg1);
			ap->mmin_char = arg1[0];
			if (ap->mmin_char == '-')
				ap->mmin_mins = -ap->mmin_mins;
		}
#endif
#if ENABLE_FEATURE_FIND_NEWER
		else if (strcmp(arg, "-newer") == 0) {
			action_newer *ap;
			struct stat stat_newer;
			if (++i == argc)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			xstat(arg1, &stat_newer);
			ap = ALLOC_ACTION(newer);
			ap->newer_mtime = stat_newer.st_mtime;
		}
#endif
#if ENABLE_FEATURE_FIND_INUM
		else if (strcmp(arg, "-inum") == 0) {
			action_inum *ap;
			if (++i == argc)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			ap = ALLOC_ACTION(inum);
			ap->inode_num = xatoul(arg1);
		}
#endif
#if ENABLE_FEATURE_FIND_EXEC
		else if (strcmp(arg, "-exec") == 0) {
			action_exec *ap;
			need_default = 0;
			ap = ALLOC_ACTION(exec);
			i++; /* now: argv[i] is the first arg after -exec */
			ap->exec_argv = &argv[i];
			ap->exec_argc = i;
			while (1) {
				if (i == argc) /* did not see ';' till end */
					bb_error_msg_and_die(bb_msg_requires_arg, arg);
				if (argv[i][0] == ';' && argv[i][1] == '\0')
					break;
				i++;
			}
			ap->exec_argc = i - ap->exec_argc; /* number of --exec arguments */
			if (ap->exec_argc == 0)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			ap->subst_count = xmalloc(ap->exec_argc * sizeof(int));
			j = ap->exec_argc;
			while (j--)
				ap->subst_count[j] = count_subst(ap->exec_argv[j]);
		}
#endif
		else
			bb_show_usage();
	}

	if (need_default)
		(void) ALLOC_ACTION(print);

	for (i = 1; i < firstopt; i++) {
		if (!recursive_action(argv[i],
				TRUE,           // recurse
				dereference,    // follow links
				FALSE,          // depth first
				fileAction,     // file action
				fileAction,     // dir action
				NULL,           // user data
				0))             // depth
			status = EXIT_FAILURE;
	}
	return status;
}
