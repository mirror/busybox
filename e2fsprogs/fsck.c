/* vi: set sw=4 ts=4: */
/*
 * fsck --- A generic, parallelizing front-end for the fsck program.
 * It will automatically try to run fsck programs in parallel if the
 * devices are on separate spindles.  It is based on the same ideas as
 * the generic front end for fsck by David Engel and Fred van Kempen,
 * but it has been completely rewritten from scratch to support
 * parallel execution.
 *
 * Written by Theodore Ts'o, <tytso@mit.edu>
 *
 * Miquel van Smoorenburg (miquels@drinkel.ow.org) 20-Oct-1994:
 *   o Changed -t fstype to behave like with mount when -A (all file
 *     systems) or -M (like mount) is specified.
 *   o fsck looks if it can find the fsck.type program to decide
 *     if it should ignore the fs type. This way more fsck programs
 *     can be added without changing this front-end.
 *   o -R flag skip root file system.
 *
 * Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
 *      2001, 2002, 2003, 2004, 2005 by  Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

/* All filesystem specific hooks have been removed.
 * If filesystem cannot be determined, we will execute
 * "fsck.auto". Currently this also happens if you specify
 * UUID=xxx or LABEL=xxx as an object to check.
 * Detection code for that is also probably has to be in fsck.auto.
 *
 * In other words, this is _really_ is just a driver program which
 * spawns actual fsck.something for each filesystem to check.
 * It doesn't guess filesystem types from on-disk format.
 */

#include "busybox.h"

#define EXIT_OK          0
#define EXIT_NONDESTRUCT 1
#define EXIT_DESTRUCT    2
#define EXIT_UNCORRECTED 4
#define EXIT_ERROR       8
#define EXIT_USAGE       16
#define FSCK_CANCELED    32     /* Aborted with a signal or ^C */

/*
 * Internal structure for mount table entries.
 */

struct fs_info {
	struct fs_info *next;
	char	*device;
	char	*mountpt;
	char	*type;
	char	*opts;
	int	freq;
	int	passno;
	int	flags;
};

#define FLAG_DONE 1
#define FLAG_PROGRESS 2
/*
 * Structure to allow exit codes to be stored
 */
struct fsck_instance {
	struct fsck_instance *next;
	int	pid;
	int	flags;
	int	exit_status;
	time_t	start_time;
	char	*prog;
	char	*type;
	char	*device;
	char	*base_device; /* /dev/hda for /dev/hdaN etc */
};

static const char *const ignored_types[] = {
	"ignore",
	"iso9660",
	"nfs",
	"proc",
	"sw",
	"swap",
	"tmpfs",
	"devpts",
	NULL
};

#if 0
static const char *const really_wanted[] = {
	"minix",
	"ext2",
	"ext3",
	"jfs",
	"reiserfs",
	"xiafs",
	"xfs",
	NULL
};
#endif

#define BASE_MD "/dev/md"

static char **devices;
static char **args;
static int num_devices;
static int num_args;
static int verbose;

#define FS_TYPE_FLAG_NORMAL 0
#define FS_TYPE_FLAG_OPT    1
#define FS_TYPE_FLAG_NEGOPT 2
static char **fs_type_list;
static uint8_t *fs_type_flag;
static smallint fs_type_negated;

static volatile smallint cancel_requested;
static smallint doall;
static smallint noexecute;
static smallint serialize;
static smallint skip_root;
/* static smallint like_mount; */
static smallint notitle;
static smallint parallel_root;
static smallint force_all_parallel;

/* "progress indicator" code is somewhat buggy and ext[23] specific.
 * We should be filesystem agnostic. IOW: there should be a well-defined
 * API for fsck.something, NOT ad-hoc hacks in generic fsck. */
#define DO_PROGRESS_INDICATOR 0
#if DO_PROGRESS_INDICATOR
static smallint progress;
static int progress_fd;
#endif

static int num_running;
static int max_running;
static char *fstype;
static struct fs_info *filesys_info;
static struct fs_info *filesys_last;
static struct fsck_instance *instance_list;

/*
 * Return the "base device" given a particular device; this is used to
 * assure that we only fsck one partition on a particular drive at any
 * one time.  Otherwise, the disk heads will be seeking all over the
 * place.  If the base device cannot be determined, return NULL.
 *
 * The base_device() function returns an allocated string which must
 * be freed.
 */
#if ENABLE_FEATURE_DEVFS
/*
 * Required for the uber-silly devfs /dev/ide/host1/bus2/target3/lun3
 * pathames.
 */
static const char *const devfs_hier[] = {
	"host", "bus", "target", "lun", NULL
};
#endif

static char *base_device(const char *device)
{
	char *str, *cp;
#if ENABLE_FEATURE_DEVFS
	const char *const *hier;
	const char *disk;
	int len;
#endif
	cp = str = xstrdup(device);

	/* Skip over /dev/; if it's not present, give up. */
	if (strncmp(cp, "/dev/", 5) != 0)
		goto errout;
	cp += 5;

	/*
	 * For md devices, we treat them all as if they were all
	 * on one disk, since we don't know how to parallelize them.
	 */
	if (cp[0] == 'm' && cp[1] == 'd') {
		cp[2] = 0;
		return str;
	}

	/* Handle DAC 960 devices */
	if (strncmp(cp, "rd/", 3) == 0) {
		cp += 3;
		if (cp[0] != 'c' || !isdigit(cp[1])
		 || cp[2] != 'd' || !isdigit(cp[3]))
			goto errout;
		cp[4] = 0;
		return str;
	}

	/* Now let's handle /dev/hd* and /dev/sd* devices.... */
	if ((cp[0] == 'h' || cp[0] == 's') && cp[1] == 'd') {
		cp += 2;
		/* If there's a single number after /dev/hd, skip it */
		if (isdigit(*cp))
			cp++;
		/* What follows must be an alpha char, or give up */
		if (!isalpha(*cp))
			goto errout;
		cp[1] = 0;
		return str;
	}

#if ENABLE_FEATURE_DEVFS
	/* Now let's handle devfs (ugh) names */
	len = 0;
	if (strncmp(cp, "ide/", 4) == 0)
		len = 4;
	if (strncmp(cp, "scsi/", 5) == 0)
		len = 5;
	if (len) {
		cp += len;
		/*
		 * Now we proceed down the expected devfs hierarchy.
		 * i.e., .../host1/bus2/target3/lun4/...
		 * If we don't find the expected token, followed by
		 * some number of digits at each level, abort.
		 */
		for (hier = devfs_hier; *hier; hier++) {
			len = strlen(*hier);
			if (strncmp(cp, *hier, len) != 0)
				goto errout;
			cp += len;
			while (*cp != '/' && *cp != 0) {
				if (!isdigit(*cp))
					goto errout;
				cp++;
			}
			cp++;
		}
		cp[-1] = 0;
		return str;
	}

	/* Now handle devfs /dev/disc or /dev/disk names */
	disk = 0;
	if (strncmp(cp, "discs/", 6) == 0)
		disk = "disc";
	else if (strncmp(cp, "disks/", 6) == 0)
		disk = "disk";
	if (disk) {
		cp += 6;
		if (strncmp(cp, disk, 4) != 0)
			goto errout;
		cp += 4;
		while (*cp != '/' && *cp != 0) {
			if (!isdigit(*cp))
				goto errout;
			cp++;
		}
		*cp = 0;
		return str;
	}
#endif
 errout:
	free(str);
	return NULL;
}

static void free_instance(struct fsck_instance *p)
{
	free(p->prog);
	free(p->device);
	free(p->base_device);
	free(p);
}

static struct fs_info *create_fs_device(const char *device, const char *mntpnt,
					const char *type, const char *opts,
					int freq, int passno)
{
	struct fs_info *fs;

	fs = xzalloc(sizeof(*fs));
	fs->device = xstrdup(device);
	fs->mountpt = xstrdup(mntpnt);
	fs->type = xstrdup(type);
	fs->opts = xstrdup(opts ? opts : "");
	fs->freq = freq;
	fs->passno = passno;
	/*fs->flags = 0; */
	/*fs->next = NULL; */

	if (!filesys_info)
		filesys_info = fs;
	else
		filesys_last->next = fs;
	filesys_last = fs;

	return fs;
}

static void strip_line(char *line)
{
	char *p = line + strlen(line) - 1;

	while (*line) {
		if (*p != '\n' && *p != '\r')
			break;
		*p-- = '\0';
	}
}

static char *parse_word(char **buf)
{
	char *word, *next;

	word = *buf;
	if (*word == '\0')
		return NULL;

	word = skip_whitespace(word);
	next = skip_non_whitespace(word);
	if (*next)
		*next++ = '\0';
	*buf = next;
	return word;
}

static void parse_escape(char *word)
{
	char *q, c;
	const char *p;

	if (!word)
		return;

	for (p = q = word; *p; q++) {
		c = *p++;
		if (c != '\\') {
			*q = c;
		} else {
			*q = bb_process_escape_sequence(&p);
		}
	}
	*q = '\0';
}

static int parse_fstab_line(char *line, struct fs_info **ret_fs)
{
	char *device, *mntpnt, *type, *opts, *freq, *passno, *cp;
	struct fs_info *fs;

	*ret_fs = 0;
	strip_line(line);
	cp = strchr(line, '#');
	if (cp)
		*cp = '\0'; /* Ignore everything after the comment char */
	cp = line;

	device = parse_word(&cp);
	if (!device) return 0; /* Allow blank lines */
	mntpnt = parse_word(&cp);
	type = parse_word(&cp);
	opts = parse_word(&cp);
	freq = parse_word(&cp);
	passno = parse_word(&cp);

	if (!mntpnt || !type)
		return -1;

	parse_escape(device);
	parse_escape(mntpnt);
	parse_escape(type);
	parse_escape(opts);
	parse_escape(freq);
	parse_escape(passno);

	if (strchr(type, ','))
		type = NULL;

	fs = create_fs_device(device, mntpnt, type ? type : "auto", opts,
			    freq ? atoi(freq) : -1,
			    passno ? atoi(passno) : -1);
	*ret_fs = fs;
	return 0;
}

/* Load the filesystem database from /etc/fstab */
static void load_fs_info(const char *filename)
{
	FILE *f;
	int lineno = 0;
	int old_fstab = 1;
	struct fs_info *fs;

	f = fopen_or_warn(filename, "r");
	if (f == NULL) {
		/*bb_perror_msg("WARNING: cannot open %s", filename);*/
		return;
	}
	while (1) {
		int r;
		char *buf = xmalloc_getline(f);
		if (!buf) break;
		r = parse_fstab_line(buf, &fs);
		free(buf);
		lineno++;
		if (r < 0) {
			bb_error_msg("WARNING: bad format "
				"on line %d of %s", lineno, filename);
			continue;
		}
		if (!fs)
			continue;
		if (fs->passno < 0)
			fs->passno = 0;
		else
			old_fstab = 0;
	}
	fclose(f);

	if (old_fstab) {
		fputs("\007"
"WARNING: Your /etc/fstab does not contain the fsck passno field.\n"
"I will kludge around things for you, but you should fix\n"
"your /etc/fstab file as soon as you can.\n\n", stderr);
		for (fs = filesys_info; fs; fs = fs->next) {
			fs->passno = 1;
		}
	}
}

/* Lookup filesys in /etc/fstab and return the corresponding entry. */
static struct fs_info *lookup(char *filesys)
{
	struct fs_info *fs;

	for (fs = filesys_info; fs; fs = fs->next) {
		if (strcmp(filesys, fs->device) == 0
		 || (fs->mountpt && strcmp(filesys, fs->mountpt) == 0)
		)
			break;
	}

	return fs;
}

#if DO_PROGRESS_INDICATOR
static int progress_active(void)
{
	struct fsck_instance *inst;

	for (inst = instance_list; inst; inst = inst->next) {
		if (inst->flags & FLAG_DONE)
			continue;
		if (inst->flags & FLAG_PROGRESS)
			return 1;
	}
	return 0;
}
#endif


/*
 * Send a signal to all outstanding fsck child processes
 */
static void kill_all_if_cancel_requested(void)
{
	static smallint kill_sent;

	struct fsck_instance *inst;

	if (!cancel_requested || kill_sent)
		return;

	for (inst = instance_list; inst; inst = inst->next) {
		if (inst->flags & FLAG_DONE)
			continue;
		kill(inst->pid, SIGTERM);
	}
	kill_sent = 1;
}

/*
 * Wait for one child process to exit; when it does, unlink it from
 * the list of executing child processes, and return it.
 */
static struct fsck_instance *wait_one(int flags)
{
	int status;
	int sig;
	struct fsck_instance *inst, *prev;
	pid_t pid;

	if (!instance_list)
		return NULL;

	if (noexecute) {
		inst = instance_list;
		prev = NULL;
#ifdef RANDOM_DEBUG
		while (inst->next && (random() & 1)) {
			prev = inst;
			inst = inst->next;
		}
#endif
		inst->exit_status = 0;
		goto ret_inst;
	}

	/*
	 * gcc -Wall fails saving throw against stupidity
	 * (inst and prev are thought to be uninitialized variables)
	 */
	inst = prev = NULL;

	do {
		pid = waitpid(-1, &status, flags);
		kill_all_if_cancel_requested();
		if (pid == 0 && (flags & WNOHANG))
			return NULL;
		if (pid < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			if (errno == ECHILD) {
				bb_error_msg("wait: no more child process?!?");
				return NULL;
			}
			bb_perror_msg("wait");
			continue;
		}
		prev = NULL;
		inst = instance_list;
		while (inst) {
			if (inst->pid == pid)
				break;
			prev = inst;
			inst = inst->next;
		}
	} while (!inst);

	if (WIFEXITED(status))
		status = WEXITSTATUS(status);
	else if (WIFSIGNALED(status)) {
		sig = WTERMSIG(status);
		status = EXIT_UNCORRECTED;
		if (sig != SIGINT) {
			printf("Warning... %s %s exited "
				"with signal %d\n",
				inst->prog, inst->device, sig);
			status = EXIT_ERROR;
		}
	} else {
		printf("%s %s: status is %x, should never happen\n",
			inst->prog, inst->device, status);
		status = EXIT_ERROR;
	}
	inst->exit_status = status;

#if DO_PROGRESS_INDICATOR
	if (progress && (inst->flags & FLAG_PROGRESS) && !progress_active()) {
		struct fsck_instance *inst2;
		for (inst2 = instance_list; inst2; inst2 = inst2->next) {
			if (inst2->flags & FLAG_DONE)
				continue;
			if (strcmp(inst2->type, "ext2") != 0
			 && strcmp(inst2->type, "ext3") != 0
			) {
				continue;
			}
			/* ext[23], we will send USR1
			 * (request to start displaying progress bar)
			 *
			 * If we've just started the fsck, wait a tiny
			 * bit before sending the kill, to give it
			 * time to set up the signal handler
			 */
			if (inst2->start_time >= time(NULL) - 1)
				sleep(1);
			kill(inst2->pid, SIGUSR1);
			inst2->flags |= FLAG_PROGRESS;
			break;
		}
	}
#endif

 ret_inst:
	if (prev)
		prev->next = inst->next;
	else
		instance_list = inst->next;
	if (verbose > 1)
		printf("Finished with %s (exit status %d)\n",
		       inst->device, inst->exit_status);
	num_running--;
	return inst;
}

#define FLAG_WAIT_ALL           0
#define FLAG_WAIT_ATLEAST_ONE   1
/*
 * Wait until all executing child processes have exited; return the
 * logical OR of all of their exit code values.
 */
static int wait_many(int flags)
{
	struct fsck_instance *inst;
	int global_status = 0;
	int wait_flags = 0;

	while ((inst = wait_one(wait_flags))) {
		global_status |= inst->exit_status;
		free_instance(inst);
#ifdef RANDOM_DEBUG
		if (noexecute && (flags & WNOHANG) && !(random() % 3))
			break;
#endif
		if (flags & FLAG_WAIT_ATLEAST_ONE)
			wait_flags = WNOHANG;
	}
	return global_status;
}

/*
 * Execute a particular fsck program, and link it into the list of
 * child processes we are waiting for.
 */
static void execute(const char *type, const char *device, const char *mntpt,
		int interactive)
{
	char *argv[num_args + 4]; /* see count below: */
	int argc;
	int i;
	struct fsck_instance *inst;
	pid_t pid;

	inst = xzalloc(sizeof(*inst));

	argv[0] = xasprintf("fsck.%s", type); /* 1 */
	for (i = 0; i < num_args; i++)
		argv[i+1] = args[i]; /* num_args */
	argc = num_args + 1;

#if DO_PROGRESS_INDICATOR
	if (progress && !progress_active()) {
		if (strcmp(type, "ext2") == 0
		 || strcmp(type, "ext3") == 0
		) {
			argv[argc++] = xasprintf("-C%d", progress_fd); /* 1 */
			inst->flags |= FLAG_PROGRESS;
		}
	}
#endif

	argv[argc++] = xstrdup(device); /* 1 */
	argv[argc] = NULL; /* 1 */

	if (verbose || noexecute) {
		printf("[%s (%d) -- %s]", argv[0], num_running,
					mntpt ? mntpt : device);
		for (i = 0; i < argc; i++)
			printf(" %s", argv[i]);
		puts("");
	}

	/* Fork and execute the correct program. */
	pid = -1;
	if (!noexecute) {
		pid = fork(); /* TODO: NOMMU friendly way (vfork)? */
		if (pid < 0)
			bb_perror_msg_and_die("fork");
		if (pid == 0) {
			/* Child */
			if (!interactive) {
				/* NB: e2fsck will complain because of this!
				 * Use "fsck -s" to avoid... */
				close(0);
			}
			BB_EXECVP(argv[0], argv);
			bb_perror_msg_and_die("%s", argv[0]);
		}
	}

	for (i = num_args+1; i < argc; i++)
		free(argv[i]);

	inst->pid = pid;
	inst->prog = argv[0];
	inst->type = xstrdup(type);
	inst->device = xstrdup(device);
	inst->base_device = base_device(device);
	inst->start_time = time(NULL);

	/* Add to the list of running fsck's.
	 * (was adding to the end, but adding to the front is simpler...) */
	inst->next = instance_list;
	instance_list = inst;
}

/*
 * Run the fsck program on a particular device
 *
 * If the type is specified using -t, and it isn't prefixed with "no"
 * (as in "noext2") and only one filesystem type is specified, then
 * use that type regardless of what is specified in /etc/fstab.
 *
 * If the type isn't specified by the user, then use either the type
 * specified in /etc/fstab, or "auto".
 */
static void fsck_device(struct fs_info *fs, int interactive)
{
	const char *type;

	if (strcmp(fs->type, "auto") != 0) {
		type = fs->type;
		if (verbose > 2)
			bb_info_msg("using filesystem type '%s' %s",
					type, "from fstab");
	} else if (fstype
	 && (fstype[0] != 'n' || fstype[1] != 'o') /* != "no" */
	 && strncmp(fstype, "opts=", 5) != 0
	 && strncmp(fstype, "loop", 4) != 0
	 && !strchr(fstype, ',')
	) {
		type = fstype;
		if (verbose > 2)
			bb_info_msg("using filesystem type '%s' %s",
					type, "from -t");
	} else {
		type = "auto";
		if (verbose > 2)
			bb_info_msg("using filesystem type '%s' %s",
					type, "(default)");
	}

	num_running++;
	execute(type, fs->device, fs->mountpt, interactive);
}

/*
 * Returns TRUE if a partition on the same disk is already being
 * checked.
 */
static int device_already_active(char *device)
{
	struct fsck_instance *inst;
	char *base;

	if (force_all_parallel)
		return 0;

#ifdef BASE_MD
	/* Don't check a soft raid disk with any other disk */
	if (instance_list
	 && (!strncmp(instance_list->device, BASE_MD, sizeof(BASE_MD)-1)
	     || !strncmp(device, BASE_MD, sizeof(BASE_MD)-1))
	) {
		return 1;
	}
#endif

	base = base_device(device);
	/*
	 * If we don't know the base device, assume that the device is
	 * already active if there are any fsck instances running.
	 */
	if (!base)
		return (instance_list != NULL);

	for (inst = instance_list; inst; inst = inst->next) {
		if (!inst->base_device || !strcmp(base, inst->base_device)) {
			free(base);
			return 1;
		}
	}

	free(base);
	return 0;
}

/*
 * This function returns true if a particular option appears in a
 * comma-delimited options list
 */
static int opt_in_list(char *opt, char *optlist)
{
	char *s;
	int len;

	if (!optlist)
		return 0;

	len = strlen(opt);
	s = optlist - 1;
	while (1) {
		s = strstr(s + 1, opt);
		if (!s)
			return 0;
		/* neither "opt.." nor "xxx,opt.."? */
		if (s != optlist && s[-1] != ',')
			continue;
		/* neither "..opt" nor "..opt,xxx"? */
		if (s[len] != '\0' && s[len] != ',')
			continue;
		return 1;
	}
}

/* See if the filesystem matches the criteria given by the -t option */
static int fs_match(struct fs_info *fs)
{
	int n, ret, checked_type;
	char *cp;

	if (!fs_type_list)
		return 1;

	ret = 0;
	checked_type = 0;
	n = 0;
	while (1) {
		cp = fs_type_list[n];
		if (!cp)
			break;
		switch (fs_type_flag[n]) {
		case FS_TYPE_FLAG_NORMAL:
			checked_type++;
			if (strcmp(cp, fs->type) == 0)
				ret = 1;
			break;
		case FS_TYPE_FLAG_NEGOPT:
			if (opt_in_list(cp, fs->opts))
				return 0;
			break;
		case FS_TYPE_FLAG_OPT:
			if (!opt_in_list(cp, fs->opts))
				return 0;
			break;
		}
		n++;
	}
	if (checked_type == 0)
		return 1;

	return (fs_type_negated ? !ret : ret);
}

/* Check if we should ignore this filesystem. */
static int ignore(struct fs_info *fs)
{
	/*
	 * If the pass number is 0, ignore it.
	 */
	if (fs->passno == 0)
		return 1;

	/*
	 * If a specific fstype is specified, and it doesn't match,
	 * ignore it.
	 */
	if (!fs_match(fs))
		return 1;

	/* Are we ignoring this type? */
	if (index_in_str_array(ignored_types, fs->type) >= 0)
		return 1;

	/* We can and want to check this file system type. */
	return 0;
}

/* Check all file systems, using the /etc/fstab table. */
static int check_all(void)
{
	struct fs_info *fs;
	int status = EXIT_OK;
	smallint not_done_yet;
	smallint pass_done;
	int passno;

	if (verbose)
		puts("Checking all filesystems");

	/*
	 * Do an initial scan over the filesystem; mark filesystems
	 * which should be ignored as done, and resolve any "auto"
	 * filesystem types (done as a side-effect of calling ignore()).
	 */
	for (fs = filesys_info; fs; fs = fs->next) {
		if (ignore(fs))
			fs->flags |= FLAG_DONE;
	}

	/*
	 * Find and check the root filesystem.
	 */
	if (!parallel_root) {
		for (fs = filesys_info; fs; fs = fs->next) {
			if (LONE_CHAR(fs->mountpt, '/'))
				break;
		}
		if (fs) {
			if (!skip_root && !ignore(fs)) {
				fsck_device(fs, 1);
				status |= wait_many(FLAG_WAIT_ALL);
				if (status > EXIT_NONDESTRUCT)
					return status;
			}
			fs->flags |= FLAG_DONE;
		}
	}
	/*
	 * This is for the bone-headed user who enters the root
	 * filesystem twice.  Skip root will skip all root entries.
	 */
	if (skip_root)
		for (fs = filesys_info; fs; fs = fs->next)
			if (LONE_CHAR(fs->mountpt, '/'))
				fs->flags |= FLAG_DONE;

	not_done_yet = 1;
	passno = 1;
	while (not_done_yet) {
		not_done_yet = 0;
		pass_done = 1;

		for (fs = filesys_info; fs; fs = fs->next) {
			if (cancel_requested)
				break;
			if (fs->flags & FLAG_DONE)
				continue;
			/*
			 * If the filesystem's pass number is higher
			 * than the current pass number, then we don't
			 * do it yet.
			 */
			if (fs->passno > passno) {
				not_done_yet = 1;
				continue;
			}
			/*
			 * If a filesystem on a particular device has
			 * already been spawned, then we need to defer
			 * this to another pass.
			 */
			if (device_already_active(fs->device)) {
				pass_done = 0;
				continue;
			}
			/*
			 * Spawn off the fsck process
			 */
			fsck_device(fs, serialize);
			fs->flags |= FLAG_DONE;

			/*
			 * Only do one filesystem at a time, or if we
			 * have a limit on the number of fsck's extant
			 * at one time, apply that limit.
			 */
			if (serialize
			 || (max_running && (num_running >= max_running))
			) {
				pass_done = 0;
				break;
			}
		}
		if (cancel_requested)
			break;
		if (verbose > 1)
			printf("--waiting-- (pass %d)\n", passno);
		status |= wait_many(pass_done ? FLAG_WAIT_ALL :
				    FLAG_WAIT_ATLEAST_ONE);
		if (pass_done) {
			if (verbose > 1)
				puts("----------------------------------");
			passno++;
		} else
			not_done_yet = 1;
	}
	kill_all_if_cancel_requested();
	status |= wait_many(FLAG_WAIT_ATLEAST_ONE);
	return status;
}

/*
 * Deal with the fsck -t argument.
 * Huh, for mount "-t novfat,nfs" means "neither vfat nor nfs"!
 * Why here we require "-t novfat,nonfs" ??
 */
static void compile_fs_type(char *fs_type)
{
	char *s;
	int num = 2;
	smallint negate;

	if (fs_type) {
		s = fs_type;
		while ((s = strchr(s, ','))) {
			num++;
			s++;
		}
	}

	fs_type_list = xzalloc(num * sizeof(fs_type_list[0]));
	fs_type_flag = xzalloc(num * sizeof(fs_type_flag[0]));
	fs_type_negated = -1; /* not yet known is it negated or not */

	if (!fs_type)
		return;

	num = 0;
	s = fs_type;
	while (1) {
		char *comma;

		negate = 0;
		if (s[0] == 'n' && s[1] == 'o') { /* "no.." */
			s += 2;
			negate = 1;
		} else if (s[0] == '!') {
			s++;
			negate = 1;
		}

		if (strcmp(s, "loop") == 0)
			/* loop is really short-hand for opts=loop */
			goto loop_special_case;
		if (strncmp(s, "opts=", 5) == 0) {
			s += 5;
 loop_special_case:
			fs_type_flag[num] = negate ? FS_TYPE_FLAG_NEGOPT : FS_TYPE_FLAG_OPT;
		} else {
			if (fs_type_negated == -1)
				fs_type_negated = negate;
			if (fs_type_negated != negate)
				bb_error_msg_and_die(
"either all or none of the filesystem types passed to -t must be prefixed "
"with 'no' or '!'");
		}
		comma = strchr(s, ',');
		fs_type_list[num++] = comma ? xstrndup(s, comma-s) : xstrdup(s);
		if (!comma)
			break;
		s = comma + 1;
	}
}

static void parse_args(int argc, char *argv[])
{
	int i, j;
	char *arg, *tmp;
	char *options = NULL;
	int optpos = 0;
	int opts_for_fsck = 0;

	/* in bss, so already zeroed
	num_devices = 0;
	num_args = 0;
	instance_list = NULL;
	*/

/* TODO: getopt32 */
	for (i = 1; i < argc; i++) {
		arg = argv[i];

		/* "/dev/blk" or "/path" or "UUID=xxx" or "LABEL=xxx" */
		if ((arg[0] == '/' && !opts_for_fsck) || strchr(arg, '=')) {
// FIXME: must check that arg is a blkdev, or resolve
// "/path", "UUID=xxx" or "LABEL=xxx" into block device name
// ("UUID=xxx"/"LABEL=xxx" can probably shifted to fsck.auto duties)
			devices = xrealloc(devices, (num_devices+1) * sizeof(devices[0]));
			devices[num_devices++] = xstrdup(arg);
			continue;
		}

		if (arg[0] != '-' || opts_for_fsck) {
			args = xrealloc(args, (num_args+1) * sizeof(args[0]));
			args[num_args++] = xstrdup(arg);
			continue;
		}

		for (j = 1; arg[j]; j++) {
			if (opts_for_fsck) {
				optpos++;
				/* one extra for '\0' */
				options = xrealloc(options, optpos + 2);
				options[optpos] = arg[j];
				continue;
			}
			switch (arg[j]) {
			case 'A':
				doall = 1;
				break;
#if DO_PROGRESS_INDICATOR
			case 'C':
				progress = 1;
				if (arg[++j]) { /* -Cn */
					progress_fd = xatoi_u(&arg[j]);
					goto next_arg;
				}
				/* -C n */
				progress_fd = xatoi_u(argv[++i]);
				goto next_arg;
#endif
			case 'V':
				verbose++;
				break;
			case 'N':
				noexecute = 1;
				break;
			case 'R':
				skip_root = 1;
				break;
			case 'T':
				notitle = 1;
				break;
/*			case 'M':
				like_mount = 1;
				break; */
			case 'P':
				parallel_root = 1;
				break;
			case 's':
				serialize = 1;
				break;
			case 't':
				if (fstype)
					bb_show_usage();
				if (arg[++j])
					tmp = &arg[j];
				else if (++i < argc)
					tmp = argv[i];
				else
					bb_show_usage();
				fstype = xstrdup(tmp);
				compile_fs_type(fstype);
				goto next_arg;
			case '-':
				opts_for_fsck++;
				break;
			case '?':
				bb_show_usage();
				break;
			default:
				optpos++;
				/* one extra for '\0' */
				options = xrealloc(options, optpos + 2);
				options[optpos] = arg[j];
				break;
			}
		}
 next_arg:
		if (optpos) {
			options[0] = '-';
			options[optpos + 1] = '\0';
			args = xrealloc(args, (num_args+1) * sizeof(args[0]));
			args[num_args++] = options;
			optpos = 0;
			options = NULL;
		}
	}
	if (getenv("FSCK_FORCE_ALL_PARALLEL"))
		force_all_parallel = 1;
	tmp = getenv("FSCK_MAX_INST");
	if (tmp)
		max_running = xatoi(tmp);
}

static void signal_cancel(int sig ATTRIBUTE_UNUSED)
{
	cancel_requested = 1;
}

int fsck_main(int argc, char *argv[]);
int fsck_main(int argc, char *argv[])
{
	int i, status = 0;
	int interactive;
	const char *fstab;
	struct fs_info *fs;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = signal_cancel;
	sigaction(SIGINT, &sa, 0);
	sigaction(SIGTERM, &sa, 0);

	setbuf(stdout, NULL);

	parse_args(argc, argv);

	if (!notitle)
		puts("fsck (busybox "BB_VER", "BB_BT")");

	/* Even plain "fsck /dev/hda1" needs fstab to get fs type,
	 * so we are scanning it anyway */
	fstab = getenv("FSTAB_FILE");
	if (!fstab)
		fstab = "/etc/fstab";
	load_fs_info(fstab);

	interactive = (num_devices == 1) | serialize;

	/* If -A was specified ("check all"), do that! */
	if (doall)
		return check_all();

	if (num_devices == 0) {
		serialize = 1;
		interactive = 1;
		return check_all();
	}

	for (i = 0; i < num_devices; i++) {
		if (cancel_requested) {
			kill_all_if_cancel_requested();
			break;
		}

		fs = lookup(devices[i]);
		if (!fs)
			fs = create_fs_device(devices[i], 0, "auto", 0, -1, -1);
		fsck_device(fs, interactive);

		if (serialize
		 || (max_running && (num_running >= max_running))
		) {
			struct fsck_instance *inst;

			inst = wait_one(0);
			if (inst) {
				status |= inst->exit_status;
				free_instance(inst);
			}
			if (verbose > 1)
				puts("----------------------------------");
		}
	}
	status |= wait_many(FLAG_WAIT_ALL);
	return status;
}
