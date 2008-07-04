/* vi: set sw=4 ts=4: */
/*
 * simplified modprobe
 *
 * Copyright (c) 2008 Vladimir Dronnikov
 * Copyright (c) 2008 Bernhard Fischer (initial depmod code)
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "unarchive.h"

#include <sys/utsname.h> /* uname() */
#include <fnmatch.h>

/* libbb candidate */
static void *xmalloc_read(int fd, size_t *sizep)
{
	char *buf;
	size_t size, rd_size, total;
	off_t to_read;
	struct stat st;

	to_read = sizep ? *sizep : INT_MAX; /* max to read */

	/* Estimate file size */
	st.st_size = 0; /* in case fstat fails, assume 0 */
	fstat(fd, &st);
	/* /proc/N/stat files report st_size 0 */
	/* In order to make such files readable, we add small const */
	size = (st.st_size | 0x3ff) + 1;

	total = 0;
	buf = NULL;
	while (1) {
		if (to_read < size)
			size = to_read;
		buf = xrealloc(buf, total + size + 1);
		rd_size = full_read(fd, buf + total, size);
		if ((ssize_t)rd_size < 0) { /* error */
			free(buf);
			return NULL;
		}
		total += rd_size;
		if (rd_size < size) /* EOF */
			break;
		to_read -= rd_size;
		if (to_read <= 0)
			break;
		/* grow by 1/8, but in [1k..64k] bounds */
		size = ((total / 8) | 0x3ff) + 1;
		if (size > 64*1024)
			size = 64*1024;
	}
	xrealloc(buf, total + 1);
	buf[total] = '\0';

	if (sizep)
		*sizep = total;
	return buf;
}


//#define dbg1_error_msg(...) ((void)0)
//#define dbg2_error_msg(...) ((void)0)
#define dbg1_error_msg(...) bb_error_msg(__VA_ARGS__)
#define dbg2_error_msg(...) bb_error_msg(__VA_ARGS__)

extern int init_module(void *module, unsigned long len, const char *options);
extern int delete_module(const char *module, unsigned flags);
extern int query_module(const char *name, int which, void *buf, size_t bufsize, size_t *ret);

enum {
	OPT_q = (1 << 0), /* be quiet */
	OPT_r = (1 << 1), /* module removal instead of loading */
};

typedef struct module_info {
	char *pathname;
	char *desc;
} module_info;

/*
 * GLOBALS
 */
struct globals {
	module_info *modinfo;
	char *module_load_options;
	int module_count;
	int module_found_idx;
	int stringbuf_idx;
	char stringbuf[32 * 1024]; /* some modules have lots of stuff */
	/* for example, drivers/media/video/saa7134/saa7134.ko */
};
#define G (*ptr_to_globals)
#define modinfo             (G.modinfo            )
#define module_count        (G.module_count       )
#define module_found_idx    (G.module_found_idx   )
#define module_load_options (G.module_load_options)
#define stringbuf_idx       (G.stringbuf_idx      )
#define stringbuf           (G.stringbuf          )
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
} while (0)


static void appendc(char c)
{
	if (stringbuf_idx < sizeof(stringbuf))
		stringbuf[stringbuf_idx++] = c;
}

static void append(const char *s)
{
	size_t len = strlen(s);
	if (stringbuf_idx + len < sizeof(stringbuf)) {
		memcpy(stringbuf + stringbuf_idx, s, len);
		stringbuf_idx += len;
	}
}

static void reset_stringbuf(void)
{
	stringbuf_idx = 0;
}

static char* copy_stringbuf(void)
{
	char *copy = xmalloc(stringbuf_idx);
	return memcpy(copy, stringbuf, stringbuf_idx);
}

static char* find_keyword(char *ptr, size_t len, const char *word)
{
	int wlen;

	if (!ptr) /* happens if read_module cannot read it */
		return NULL;

	wlen = strlen(word);
	len -= wlen - 1;
	while ((ssize_t)len > 0) {
		char *old = ptr;
		/* search for the first char in word */
		ptr = memchr(ptr, *word, len);
		if (ptr == NULL) /* no occurance left, done */
			break;
		if (strncmp(ptr, word, wlen) == 0)
			return ptr + wlen; /* found, return ptr past it */
		++ptr;
		len -= (ptr - old);
	}
	return NULL;
}

static void replace(char *s, char what, char with)
{
	while (*s) {
		if (what == *s)
			*s = with;
		++s;
	}
}

#if ENABLE_FEATURE_MODPROBE_SMALL_ZIPPED
static char *xmalloc_open_zipped_read_close(const char *fname, size_t *sizep)
{
	size_t len;
	char *image;
	char *suffix;

	int fd = open_or_warn(fname, O_RDONLY);
	if (fd < 0)
		return NULL;

	suffix = strrchr(fname, '.');
	if (suffix) {
		if (strcmp(suffix, ".gz") == 0)
			fd = open_transformer(fd, unpack_gz_stream, "gunzip");
		else if (strcmp(suffix, ".bz2") == 0)
			fd = open_transformer(fd, unpack_bz2_stream, "bunzip2");
	}

	len = (sizep) ? *sizep : 64 * 1024 * 1024;
	image = xmalloc_read(fd, &len);
	if (!image)
		bb_perror_msg("read error from '%s'", fname);
	close(fd);

	if (sizep)
		*sizep = len;
	return image;
}
# define read_module xmalloc_open_zipped_read_close
#else
# define read_module xmalloc_open_read_close
#endif

/* We use error numbers in a loose translation... */
static const char *moderror(int err)
{
	switch (err) {
	case ENOEXEC:
		return "invalid module format";
	case ENOENT:
		return "unknown symbol in module or invalid parameter";
	case ESRCH:
		return "module has wrong symbol version";
	case EINVAL: /* "invalid parameter" */
		return "unknown symbol in module or invalid parameter"
		+ sizeof("unknown symbol in module or");
	default:
		return strerror(err);
	}
}

static int load_module(const char *fname, const char *options)
{
#if 1
	int r;
	size_t len = MAXINT(ssize_t);
	char *module_image;
	dbg1_error_msg("load_module('%s','%s')", fname, options);

	module_image = read_module(fname, &len);
	r = (!module_image || init_module(module_image, len, options ? options : "") != 0);
	free(module_image);
	dbg1_error_msg("load_module:%d", r);
	return r; /* 0 = success */
#else
	/* For testing */
	dbg1_error_msg("load_module('%s','%s')", fname, options);
	return 1;
#endif
}

static char* parse_module(const char *pathname, const char *name)
{
	char *module_image;
	char *ptr;
	size_t len;
	size_t pos;
	dbg1_error_msg("parse_module('%s','%s')", pathname, name);

	/* Read (possibly compressed) module */
	len = 64 * 1024 * 1024; /* 64 Mb at most */
	module_image = read_module(pathname, &len);

	reset_stringbuf();

	/* First desc line's format is
	 * "modname alias1 symbol:sym1 alias2 symbol:sym2 " (note trailing ' ')
	 */
	append(name);
	appendc(' ');
	/* Aliases */
	pos = 0;
	while (1) {
		ptr = find_keyword(module_image + pos, len - pos, "alias=");
		if (!ptr) {
			ptr = find_keyword(module_image + pos, len - pos, "__ksymtab_");
			if (!ptr)
				break;
			/* DOCME: __ksymtab_gpl and __ksymtab_strings occur
			 * in many modules. What do they mean? */
			if (strcmp(ptr, "gpl") != 0 && strcmp(ptr, "strings") != 0) {
				dbg2_error_msg("alias: 'symbol:%s'", ptr);
				append("symbol:");
			}
		} else {
			dbg2_error_msg("alias: '%s'", ptr);
		}
		append(ptr);
		appendc(' ');
		pos = (ptr - module_image);
	}
	appendc('\0');

	/* Second line: "dependency1 depandency2 " (note trailing ' ') */
	ptr = find_keyword(module_image, len, "depends=");
	if (ptr && *ptr) {
		replace(ptr, ',', ' ');
		replace(ptr, '-', '_');
		dbg2_error_msg("dep:'%s'", ptr);
		append(ptr);
	}
	appendc(' '); appendc('\0');

	free(module_image);
	return copy_stringbuf();
}

static char* pathname_2_modname(const char *pathname)
{
	const char *fname = bb_get_last_path_component_nostrip(pathname);
	const char *suffix = strrstr(fname, ".ko");
	char *name = xstrndup(fname, suffix - fname);
	replace(name, '-', '_');
	return name;
}

static FAST_FUNC int fileAction(const char *pathname,
		struct stat *sb ATTRIBUTE_UNUSED,
		void *data,
		int depth ATTRIBUTE_UNUSED)
{
	int cur;
	char *name;
	const char *fname;

	pathname += 2; /* skip "./" */
	fname = bb_get_last_path_component_nostrip(pathname);
	if (!strrstr(fname, ".ko")) {
		dbg1_error_msg("'%s' is not a module", pathname);
		return TRUE; /* not a module, continue search */
	}

	cur = module_count++;
	if (!(cur & 0xfff)) {
		modinfo = xrealloc(modinfo, sizeof(modinfo[0]) * (cur + 0x1001));
	}
	modinfo[cur].pathname = xstrdup(pathname);
	modinfo[cur].desc = NULL;
	modinfo[cur+1].pathname = NULL;
	modinfo[cur+1].desc = NULL;

	name = pathname_2_modname(fname);
	if (strcmp(name, data) != 0) {
		free(name);
		dbg1_error_msg("'%s' module name doesn't match", pathname);
		return TRUE; /* module name doesn't match, continue search */
	}

	dbg1_error_msg("'%s' module name matches", pathname);
	module_found_idx = cur;
	modinfo[cur].desc = parse_module(pathname, name);

	if (!(option_mask32 & OPT_r)) {
		if (load_module(pathname, module_load_options) == 0) {
			/* Load was successful, there is nothing else to do.
			 * This can happen ONLY for "top-level" module load,
			 * not a dep, because deps dont do dirscan. */
			exit(EXIT_SUCCESS);
			/*free(name);return RECURSE_RESULT_ABORT;*/
		}
	}

	free(name);
	return TRUE;
}

static module_info* find_alias(const char *alias)
{
	int i;
	dbg1_error_msg("find_alias('%s')", alias);

	/* First try to find by name (cheaper) */
	i = 0;
	while (modinfo[i].pathname) {
		char *name = pathname_2_modname(modinfo[i].pathname);
		if (strcmp(name, alias) == 0) {
			dbg1_error_msg("found '%s' in module '%s'",
					alias, modinfo[i].pathname);
			if (!modinfo[i].desc)
				modinfo[i].desc = parse_module(modinfo[i].pathname, name);
			free(name);
			return &modinfo[i];
		}
		free(name);
		i++;
	}

	/* Scan all module bodies, extract modinfo (it contains aliases) */
	i = 0;
	while (modinfo[i].pathname) {
		char *desc, *s;
		if (!modinfo[i].desc) {
			char *name = pathname_2_modname(modinfo[i].pathname);
			modinfo[i].desc = parse_module(modinfo[i].pathname, name);
			free(name);
		}
		/* "modname alias1 symbol:sym1 alias2 symbol:sym2 " */
		desc = xstrdup(modinfo[i].desc);
		/* Does matching substring exist? */
		replace(desc, ' ', '\0');
		for (s = desc; *s; s += strlen(s) + 1) {
			if (strcmp(s, alias) == 0) {
				free(desc);
				dbg1_error_msg("found alias '%s' in module '%s'",
						alias, modinfo[i].pathname);
				return &modinfo[i];
			}
		}
		free(desc);
		i++;
	}
	dbg1_error_msg("find_alias '%s' returns NULL", alias);
	return NULL;
}

#if ENABLE_FEATURE_MODPROBE_SMALL_CHECK_ALREADY_LOADED
static int already_loaded(const char *name)
{
	int ret = 0;
	int len = strlen(name);
	char *line;
	FILE* modules;

	modules = xfopen("/proc/modules", "r");
	while ((line = xmalloc_fgets(modules)) != NULL) {
		if (strncmp(line, name, len) == 0 && line[len] == ' ') {
			free(line);
			ret = 1;
			break;
		}
		free(line);
	}
	fclose(modules);
	return ret;
}
#else
#define already_loaded(name) is_rmmod
#endif

/*
 Given modules definition and module name (or alias, or symbol)
 load/remove the module respecting dependencies
*/
#if !ENABLE_FEATURE_MODPROBE_SMALL_OPTIONS_ON_CMDLINE
#define process_module(a,b) process_module(a)
#define cmdline_options ""
#endif
static void process_module(char *name, const char *cmdline_options)
{
	char *s, *deps, *options;
	module_info *info;
	int is_rmmod = (option_mask32 & OPT_r) != 0;
	dbg1_error_msg("process_module('%s','%s')", name, cmdline_options);

	replace(name, '-', '_');

	dbg1_error_msg("already_loaded:%d is_rmmod:%d", already_loaded(name), is_rmmod);
	if (already_loaded(name) != is_rmmod) {
		dbg1_error_msg("nothing to do for '%s'", name);
		return;
	}

	options = NULL;
	if (!is_rmmod) {
		char *opt_filename = xasprintf("/etc/modules/%s", name);
		options = xmalloc_open_read_close(opt_filename, NULL);
		if (options)
			replace(options, '\n', ' ');
#if ENABLE_FEATURE_MODPROBE_SMALL_OPTIONS_ON_CMDLINE
		if (cmdline_options) {
			/* NB: cmdline_options always have one leading ' '
			 * (see main()), we remove it here */
			char *op = xasprintf(options ? "%s %s" : "%s %s" + 3,
						cmdline_options + 1, options);
			free(options);
			options = op;
		}
#endif
		free(opt_filename);
		module_load_options = options;
		dbg1_error_msg("process_module('%s'): options:'%s'", name, options);
	}

	if (!module_count) {
		/* Scan module directory. This is done only once.
		 * It will attempt module load, and will exit(EXIT_SUCCESS)
		 * on success. */
		module_found_idx = -1;
		recursive_action(".",
			ACTION_RECURSE, /* flags */
			fileAction, /* file action */
			NULL, /* dir action */
			name, /* user data */
			0); /* depth */
		dbg1_error_msg("dirscan complete");
		/* Module was not found, or load failed, or is_rmmod */
		if (module_found_idx >= 0) { /* module was found */
			info = &modinfo[module_found_idx];
		} else { /* search for alias, not a plain module name */
			info = find_alias(name);
		}
	} else {
		info = find_alias(name);
	}

	/* rmmod? unload it by name */
	if (is_rmmod) {
		if (delete_module(name, O_NONBLOCK|O_EXCL) != 0
		 && !(option_mask32 & OPT_q)
		) {
			bb_perror_msg("remove '%s'", name);
			goto ret;
		}
		/* N.B. we do not stop here -
		 * continue to unload modules on which the module depends:
		 * "-r --remove: option causes modprobe to remove a module.
		 * If the modules it depends on are also unused, modprobe
		 * will try to remove them, too." */
	}

	if (!info) { /* both dirscan and find_alias found nothing */
		goto ret;
	}

	/* Second line of desc contains dependencies */
	deps = xstrdup(info->desc + strlen(info->desc) + 1);

	/* Transform deps to string list */
	replace(deps, ' ', '\0');
	/* Iterate thru dependencies, trying to (un)load them */
	for (s = deps; *s; s += strlen(s) + 1) {
		//if (strcmp(name, s) != 0) // N.B. do loops exist?
		dbg1_error_msg("recurse on dep '%s'", s);
		process_module(s, NULL);
		dbg1_error_msg("recurse on dep '%s' done", s);
	}
	free(deps);

	/* insmod -> load it */
	if (!is_rmmod) {
		errno = 0;
		if (load_module(info->pathname, options) != 0) {
			if (EEXIST != errno) {
				bb_error_msg("insert '%s' %s: %s",
						info->pathname, options,
						moderror(errno));
			} else {
				dbg1_error_msg("insert '%s' %s: %s",
						info->pathname, options,
						moderror(errno));
			}
		}
	}
 ret:
	free(options);
//TODO: return load attempt result from process_module.
//If dep didn't load ok, continuing makes little sense.
}
#undef cmdline_options


/* For reference, module-init-tools-0.9.15-pre2 options:

# insmod
Usage: insmod filename [args]

# rmmod --help
Usage: rmmod [-fhswvV] modulename ...
 -f (or --force) forces a module unload, and may crash your machine.
 -s (or --syslog) says use syslog, not stderr
 -v (or --verbose) enables more messages
 -w (or --wait) begins a module removal even if it is used
    and will stop new users from accessing the module (so it
    should eventually fall to zero).

# modprobe
Usage: modprobe [--verbose|--version|--config|--remove] filename [options]

# depmod --help
depmod 0.9.15-pre2 -- part of module-init-tools
depmod -[aA] [-n -e -v -q -V -r -u] [-b basedirectory] [forced_version]
depmod [-n -e -v -q -r -u] [-F kernelsyms] module1.o module2.o ...
If no arguments (except options) are given, "depmod -a" is assumed

depmod will output a dependancy list suitable for the modprobe utility.

Options:
        -a, --all               Probe all modules
        -n, --show              Write the dependency file on stdout only
        -b basedirectory
        --basedir basedirectory Use an image of a module tree.
        -F kernelsyms
        --filesyms kernelsyms   Use the file instead of the
                                current kernel symbols.
*/

int modprobe_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int modprobe_main(int ATTRIBUTE_UNUSED argc, char **argv)
{
	struct utsname uts;
	char applet0 = applet_name[0];
	USE_FEATURE_MODPROBE_SMALL_OPTIONS_ON_CMDLINE(char *options;)

	/* depmod is a stub */
	if ('d' == applet0)
		return EXIT_SUCCESS;

	/* are we lsmod? -> just dump /proc/modules */
	if ('l' == applet0) {
		xprint_and_close_file(xfopen("/proc/modules", "r"));
		return EXIT_SUCCESS;
	}

	INIT_G();

	/* insmod, modprobe, rmmod require at least one argument */
	opt_complementary = "-1";
	/* only -q (quiet) and -r (rmmod),
	 * the rest are accepted and ignored (compat) */
	getopt32(argv, "qrfsvw");
	argv += optind;

	/* are we rmmod? -> simulate modprobe -r */
	if ('r' == applet0) {
		option_mask32 |= OPT_r;
	}

	/* goto modules directory */
	xchdir(CONFIG_DEFAULT_MODULES_DIR);
	uname(&uts); /* never fails */
	xchdir(uts.release);

#if ENABLE_FEATURE_MODPROBE_SMALL_OPTIONS_ON_CMDLINE
	/* If not rmmod, parse possible module options given on command line.
	 * insmod/modprobe takes one module name, the rest are parameters. */
	options = NULL;
	if ('r' != applet0) {
		char **arg = argv;
		while (*++arg) {
			/* Enclose options in quotes */
			char *s = options;
			options = xasprintf("%s \"%s\"", s ? s : "", *arg);
			free(s);
			*arg = NULL;
		}
	}
#else
	if ('r' != applet0)
		argv[1] = NULL;
#endif

	/* Load/remove modules.
	 * Only rmmod loops here, insmod/modprobe has only argv[0] */
	do {
		process_module(*argv++, options);
	} while (*argv);

	if (ENABLE_FEATURE_CLEAN_UP) {
		USE_FEATURE_MODPROBE_SMALL_OPTIONS_ON_CMDLINE(free(options);)
	}
	return EXIT_SUCCESS;
}
