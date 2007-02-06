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
 *
 * # find t z -name '*t*' -print -o -name '*z*'
 * t
 * # find t z t z -name '*t*' -o -name '*z*' -print
 * z
 * z
 * # find t z t z '(' -name '*t*' -o -name '*z*' ')' -o -print
 * (no output)
 */

#include <fnmatch.h>
#include "busybox.h"

USE_FEATURE_FIND_XDEV(static dev_t *xdev_dev;)
USE_FEATURE_FIND_XDEV(static int xdev_count;)

typedef int (*action_fp)(const char *fileName, struct stat *statbuf, void *);

typedef struct {
	action_fp f;
#if ENABLE_FEATURE_FIND_NOT
	smallint invert;
#endif
} action;
#define ACTS(name, arg...) typedef struct { action a; arg; } action_##name;
#define ACTF(name)         static int func_##name(const char *fileName, struct stat *statbuf, action_##name* ap)
                        ACTS(print)
                        ACTS(name,  const char *pattern;)
USE_FEATURE_FIND_PRINT0(ACTS(print0))
USE_FEATURE_FIND_TYPE(  ACTS(type,  int type_mask;))
USE_FEATURE_FIND_PERM(  ACTS(perm,  char perm_char; mode_t perm_mask;))
USE_FEATURE_FIND_MTIME( ACTS(mtime, char mtime_char; unsigned mtime_days;))
USE_FEATURE_FIND_MMIN(  ACTS(mmin,  char mmin_char; unsigned mmin_mins;))
USE_FEATURE_FIND_NEWER( ACTS(newer, time_t newer_mtime;))
USE_FEATURE_FIND_INUM(  ACTS(inum,  ino_t inode_num;))
USE_FEATURE_FIND_EXEC(  ACTS(exec,  char **exec_argv; unsigned int *subst_count; int exec_argc;))
USE_FEATURE_FIND_USER(  ACTS(user,  int uid;))
USE_DESKTOP(            ACTS(paren, action ***subexpr;))
USE_DESKTOP(            ACTS(size,  off_t size;))
USE_DESKTOP(            ACTS(prune))

static action ***actions;
static smalluint need_print = 1;


#if ENABLE_FEATURE_FIND_EXEC
static unsigned int count_subst(const char *str)
{
	unsigned int count = 0;
	while ((str = strstr(str, "{}"))) {
		count++;
		str++;
	}
	return count;
}


static char* subst(const char *src, unsigned int count, const char* filename)
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
#endif


static int exec_actions(action ***appp, const char *fileName, struct stat *statbuf)
{
	int cur_group;
	int cur_action;
	int rc = TRUE;
	action **app, *ap;

	cur_group = -1;
	while ((app = appp[++cur_group])) {
		cur_action = -1;
		while (1) {
			ap = app[++cur_action];
			if (!ap) {
				/* all actions in group were successful */
				return rc;
			}
			rc = ap->f(fileName, statbuf, ap);
#if ENABLE_FEATURE_FIND_NOT
			if (ap->invert) rc = !rc;
#endif
			if (!rc) {
				/* current group failed, try next */
				break;
			}
		}
	}
	return rc;
}


ACTF(name)
{
	const char *tmp = strrchr(fileName, '/');
	if (tmp == NULL)
		tmp = fileName;
	else
		tmp++;
	return fnmatch(ap->pattern, tmp, FNM_PERIOD) == 0;
}
#if ENABLE_FEATURE_FIND_TYPE
ACTF(type)
{
	return ((statbuf->st_mode & S_IFMT) == ap->type_mask);
}
#endif
#if ENABLE_FEATURE_FIND_PERM
ACTF(perm)
{
	/* -perm +mode: at least one of perm_mask bits are set */
	if (ap->perm_char == '+')
		return (statbuf->st_mode & ap->perm_mask) != 0;
	/* -perm -mode: all of perm_mask are set */
	if (ap->perm_char == '-')
		return (statbuf->st_mode & ap->perm_mask) == ap->perm_mask;
	/* -perm mode: file mode must match perm_mask */
	return (statbuf->st_mode & 07777) == ap->perm_mask;
}
#endif
#if ENABLE_FEATURE_FIND_MTIME
ACTF(mtime)
{
	time_t file_age = time(NULL) - statbuf->st_mtime;
	time_t mtime_secs = ap->mtime_days * 24*60*60;
	if (ap->mtime_char == '+')
		return file_age >= mtime_secs + 24*60*60;
	if (ap->mtime_char == '-')
		return file_age < mtime_secs;
	/* just numeric mtime */
	return file_age >= mtime_secs && file_age < (mtime_secs + 24*60*60);
}
#endif
#if ENABLE_FEATURE_FIND_MMIN
ACTF(mmin)
{
	time_t file_age = time(NULL) - statbuf->st_mtime;
	time_t mmin_secs = ap->mmin_mins * 60;
	if (ap->mmin_char == '+')
		return file_age >= mmin_secs + 60;
	if (ap->mmin_char == '-')
		return file_age < mmin_secs;
	/* just numeric mmin */
	return file_age >= mmin_secs && file_age < (mmin_secs + 60);
}
#endif
#if ENABLE_FEATURE_FIND_NEWER
ACTF(newer)
{
	return (ap->newer_mtime < statbuf->st_mtime);
}
#endif
#if ENABLE_FEATURE_FIND_INUM
ACTF(inum)
{
	return (statbuf->st_ino == ap->inode_num);
}
#endif
#if ENABLE_FEATURE_FIND_EXEC
ACTF(exec)
{
	int i, rc;
	char *argv[ap->exec_argc+1];
	for (i = 0; i < ap->exec_argc; i++)
		argv[i] = subst(ap->exec_argv[i], ap->subst_count[i], fileName);
	argv[i] = NULL; /* terminate the list */
	rc = wait4pid(spawn(argv));
	if (rc)
		bb_perror_msg("%s", argv[0]);
	for (i = 0; i < ap->exec_argc; i++)
		free(argv[i]);
	return rc == 0; /* return 1 if success */
}
#endif

#if ENABLE_FEATURE_FIND_USER
ACTF(user)
{
	return (statbuf->st_uid == ap->uid);
}
#endif

#if ENABLE_FEATURE_FIND_PRINT0
ACTF(print0)
{
	printf("%s%c", fileName, '\0');
	return TRUE;
}
#endif

ACTF(print)
{
	puts(fileName);
	return TRUE;
}

#if ENABLE_DESKTOP
ACTF(paren)
{
	return exec_actions(ap->subexpr, fileName, statbuf);
}

/*
 * -prune: if -depth is not given, return true and do not descend
 * current dir; if -depth is given, return false with no effect.
 * Example:
 * find dir -name 'asm-*' -prune -o -name '*.[chS]' -print
 */
ACTF(prune)
{
	return SKIP;
}

ACTF(size)
{
	return statbuf->st_size == ap->size;
}
#endif


static int fileAction(const char *fileName, struct stat *statbuf, void* junk, int depth)
{
	int rc;
#ifdef CONFIG_FEATURE_FIND_XDEV
	if (S_ISDIR(statbuf->st_mode) && xdev_count) {
		int i;
		for (i = 0; i < xdev_count; i++) {
			if (xdev_dev[i] != statbuf->st_dev)
				return SKIP;
		}
	}
#endif
	rc = exec_actions(actions, fileName, statbuf);
	/* Had no explicit -print[0] or -exec? then print */
	if (rc && need_print)
		puts(fileName);
	/* Cannot return 0: our caller, recursive_action(),
	 * will perror() and skip dirs (if called on dir) */
	return rc == 0 ? TRUE : rc;
}


#if ENABLE_FEATURE_FIND_TYPE
static int find_type(const char *type)
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

#if ENABLE_FEATURE_FIND_PERM || ENABLE_FEATURE_FIND_MTIME \
 || ENABLE_FEATURE_FIND_MMIN
static const char* plus_minus_num(const char* str)
{
	if (*str == '-' || *str == '+')
		str++;
	return str;
}
#endif

static action*** parse_params(char **argv)
{
	action*** appp;
	unsigned cur_group = 0;
	unsigned cur_action = 0;
	USE_FEATURE_FIND_NOT( smallint invert_flag = 0; )

	action* alloc_action(int sizeof_struct, action_fp f)
	{
		action *ap;
		appp[cur_group] = xrealloc(appp[cur_group], (cur_action+2) * sizeof(*appp));
		appp[cur_group][cur_action++] = ap = xmalloc(sizeof_struct);
		appp[cur_group][cur_action] = NULL;
		ap->f = f;
		USE_FEATURE_FIND_NOT( ap->invert = invert_flag; )
		USE_FEATURE_FIND_NOT( invert_flag = 0; )
		return ap;
	}
#define ALLOC_ACTION(name) (action_##name*)alloc_action(sizeof(action_##name), (action_fp) func_##name)

	appp = xzalloc(2 * sizeof(appp[0])); /* appp[0],[1] == NULL */

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
// We implement: (), -a, -o

//XXX: TODO: Use index_in_str_array here
	while (*argv) {
		const char *arg = argv[0];
		const char *arg1 = argv[1];
	/* --- Operators --- */
		if (strcmp(arg, "-a") == 0
		    USE_DESKTOP(|| strcmp(arg, "-and") == 0)
		) {
			/* no further special handling required */
		}
		else if (strcmp(arg, "-o") == 0
		         USE_DESKTOP(|| strcmp(arg, "-or") == 0)
		) {
			/* start new OR group */
			cur_group++;
			appp = xrealloc(appp, (cur_group+2) * sizeof(*appp));
			/*appp[cur_group] = NULL; - already NULL */
			appp[cur_group+1] = NULL;
			cur_action = 0;
		}
#if ENABLE_FEATURE_FIND_NOT
		else if (LONE_CHAR(arg, '!')
		         USE_DESKTOP(|| strcmp(arg, "-not") == 0)
		) {
			/* also handles "find ! ! -name 'foo*'" */
			invert_flag ^= 1;
		}
#endif

	/* --- Tests and actions --- */
		else if (strcmp(arg, "-print") == 0) {
			need_print = 0;
			/* GNU find ignores '!' here: "find ! -print" */
			USE_FEATURE_FIND_NOT( invert_flag = 0; )
			(void) ALLOC_ACTION(print);
		}
#if ENABLE_FEATURE_FIND_PRINT0
		else if (strcmp(arg, "-print0") == 0) {
			need_print = 0;
			USE_FEATURE_FIND_NOT( invert_flag = 0; )
			(void) ALLOC_ACTION(print0);
		}
#endif
		else if (strcmp(arg, "-name") == 0) {
			action_name *ap;
			if (!*++argv)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			ap = ALLOC_ACTION(name);
			ap->pattern = arg1;
		}
#if ENABLE_FEATURE_FIND_TYPE
		else if (strcmp(arg, "-type") == 0) {
			action_type *ap;
			if (!*++argv)
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
			if (!*++argv)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			ap = ALLOC_ACTION(perm);
			ap->perm_char = arg1[0];
			arg1 = plus_minus_num(arg1);
			ap->perm_mask = 0;
			if (!bb_parse_mode(arg1, &ap->perm_mask))
				bb_error_msg_and_die("invalid mode: %s", arg1);
		}
#endif
#if ENABLE_FEATURE_FIND_MTIME
		else if (strcmp(arg, "-mtime") == 0) {
			action_mtime *ap;
			if (!*++argv)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			ap = ALLOC_ACTION(mtime);
			ap->mtime_char = arg1[0];
			ap->mtime_days = xatoul(plus_minus_num(arg1));
		}
#endif
#if ENABLE_FEATURE_FIND_MMIN
		else if (strcmp(arg, "-mmin") == 0) {
			action_mmin *ap;
			if (!*++argv)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			ap = ALLOC_ACTION(mmin);
			ap->mmin_char = arg1[0];
			ap->mmin_mins = xatoul(plus_minus_num(arg1));
		}
#endif
#if ENABLE_FEATURE_FIND_NEWER
		else if (strcmp(arg, "-newer") == 0) {
			action_newer *ap;
			struct stat stat_newer;
			if (!*++argv)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			xstat(arg1, &stat_newer);
			ap = ALLOC_ACTION(newer);
			ap->newer_mtime = stat_newer.st_mtime;
		}
#endif
#if ENABLE_FEATURE_FIND_INUM
		else if (strcmp(arg, "-inum") == 0) {
			action_inum *ap;
			if (!*++argv)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			ap = ALLOC_ACTION(inum);
			ap->inode_num = xatoul(arg1);
		}
#endif
#if ENABLE_FEATURE_FIND_EXEC
		else if (strcmp(arg, "-exec") == 0) {
			int i;
			action_exec *ap;
			need_print = 0;
			USE_FEATURE_FIND_NOT( invert_flag = 0; )
			ap = ALLOC_ACTION(exec);
			ap->exec_argv = ++argv; /* first arg after -exec */
			ap->exec_argc = 0;
			while (1) {
				if (!*argv) /* did not see ';' till end */
					bb_error_msg_and_die(bb_msg_requires_arg, arg);
				if (LONE_CHAR(argv[0], ';'))
					break;
				argv++;
				ap->exec_argc++;
			}
			if (ap->exec_argc == 0)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			ap->subst_count = xmalloc(ap->exec_argc * sizeof(int));
			i = ap->exec_argc;
			while (i--)
				ap->subst_count[i] = count_subst(ap->exec_argv[i]);
		}
#endif
#if ENABLE_FEATURE_FIND_USER
		else if (strcmp(arg, "-user") == 0) {
			action_user *ap;
			if (!*++argv)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			ap = ALLOC_ACTION(user);
			ap->uid = bb_strtou(arg1, NULL, 10);
			if (errno)
				ap->uid = xuname2uid(arg1);
		}
#endif
#if ENABLE_DESKTOP
		else if (LONE_CHAR(arg, '(')) {
			action_paren *ap;
			char **endarg;
			unsigned nested = 1;

			endarg = argv;
			while (1) {
				if (!*++endarg)
					bb_error_msg_and_die("unpaired '('");
				if (LONE_CHAR(*endarg, '('))
					nested++;
				else if (LONE_CHAR(*endarg, ')') && !--nested) {
					*endarg = NULL;
					break;
				}
			}
			ap = ALLOC_ACTION(paren);
			ap->subexpr = parse_params(argv + 1);
			*endarg = (char*) ")"; /* restore NULLed parameter */
			argv = endarg;
		}
		else if (strcmp(arg, "-prune") == 0) {
			USE_FEATURE_FIND_NOT( invert_flag = 0; )
			(void) ALLOC_ACTION(prune);
		}
		else if (strcmp(arg, "-size") == 0) {
			action_size *ap;
			if (!*++argv)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			ap = ALLOC_ACTION(size);
			ap->size = XATOOFF(arg1);
		}
#endif
		else
			bb_show_usage();
		argv++;
	}

	return appp;
#undef ALLOC_ACTION
}


int find_main(int argc, char **argv);
int find_main(int argc, char **argv)
{
	int dereference = FALSE;
	char *arg;
	char **argp;
	int i, firstopt, status = EXIT_SUCCESS;

	for (firstopt = 1; firstopt < argc; firstopt++) {
		if (argv[firstopt][0] == '-')
			break;
		if (ENABLE_FEATURE_FIND_NOT && LONE_CHAR(argv[firstopt], '!'))
			break;
#if ENABLE_DESKTOP
		if (LONE_CHAR(argv[firstopt], '('))
			break;
#endif
	}
	if (firstopt == 1) {
		argv[0] = (char*)".";
		argv--;
		firstopt++;
	}

// All options always return true. They always take effect,
// rather than being processed only when their place in the
// expression is reached
// We implement: -follow, -xdev

	/* Process options, and replace then with -a */
	/* (-a will be ignored by recursive parser later) */
	argp = &argv[firstopt];
	while ((arg = argp[0])) {
		if (strcmp(arg, "-follow") == 0) {
			dereference = TRUE;
			argp[0] = (char*)"-a";
		}
#if ENABLE_FEATURE_FIND_XDEV
		else if (strcmp(arg, "-xdev") == 0) {
			struct stat stbuf;
			if (!xdev_count) {
				xdev_count = firstopt - 1;
				xdev_dev = xmalloc(xdev_count * sizeof(dev_t));
				for (i = 1; i < firstopt; i++) {
					/* not xstat(): shouldn't bomb out on
					 * "find not_exist exist -xdev" */
					if (stat(argv[i], &stbuf))
						stbuf.st_dev = -1L;
					xdev_dev[i-1] = stbuf.st_dev;
				}
			}
			argp[0] = (char*)"-a";
		}
#endif
		argp++;
	}

	actions = parse_params(&argv[firstopt]);

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
