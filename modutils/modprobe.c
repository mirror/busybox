/* vi: set sw=4 ts=4: */
/*
 * Modprobe written from scratch for BusyBox
 *
 * Copyright (c) 2008 Timo Teras <timo.teras@iki.fi>
 * Copyright (c) 2008 Vladimir Dronnikov
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* Note that unlike older versions of modules.dep/depmod (busybox and m-i-t),
 * we expect the full dependency list to be specified in modules.dep.
 * Older versions would only export the direct dependency list.
 */
#include "libbb.h"
#include "modutils.h"
#include <sys/utsname.h>
#include <fnmatch.h>

//#define DBG(fmt, ...) bb_error_msg("%s: " fmt, __func__, ## __VA_ARGS__)
#define DBG(...) ((void)0)

#define MODULE_FLAG_LOADED              0x0001
#define MODULE_FLAG_NEED_DEPS           0x0002
/* "was seen in modules.dep": */
#define MODULE_FLAG_FOUND_IN_MODDEP     0x0004
#define MODULE_FLAG_BLACKLISTED         0x0008

struct module_entry { /* I'll call it ME. */
	unsigned flags;
	char *modname; /* stripped of /path/, .ext and s/-/_/g */
	const char *probed_name; /* verbatim as seen on cmdline */
	char *options; /* options from config files */
	llist_t *realnames; /* strings. if this module is an alias, */
	/* real module name is one of these. */
//Can there really be more than one? Example from real kernel?
	llist_t *deps; /* strings. modules we depend on */
};

/* NB: INSMOD_OPT_SILENT bit suppresses ONLY non-existent modules,
 * not deleted ones (those are still listed in modules.dep).
 * module-init-tools version 3.4:
 * # modprobe bogus
 * FATAL: Module bogus not found. [exitcode 1]
 * # modprobe -q bogus            [silent, exitcode still 1]
 * but:
 * # rm kernel/drivers/net/dummy.ko
 * # modprobe -q dummy
 * FATAL: Could not open '/lib/modules/xxx/kernel/drivers/net/dummy.ko': No such file or directory
 * [exitcode 1]
 */
#define MODPROBE_OPTS  "acdlnrt:VC:" IF_FEATURE_MODPROBE_BLACKLIST("b")
enum {
	MODPROBE_OPT_INSERT_ALL = (INSMOD_OPT_UNUSED << 0), /* a */
	MODPROBE_OPT_DUMP_ONLY  = (INSMOD_OPT_UNUSED << 1), /* c */
	MODPROBE_OPT_D          = (INSMOD_OPT_UNUSED << 2), /* d */
	MODPROBE_OPT_LIST_ONLY  = (INSMOD_OPT_UNUSED << 3), /* l */
	MODPROBE_OPT_SHOW_ONLY  = (INSMOD_OPT_UNUSED << 4), /* n */
	MODPROBE_OPT_REMOVE     = (INSMOD_OPT_UNUSED << 5), /* r */
	MODPROBE_OPT_RESTRICT   = (INSMOD_OPT_UNUSED << 6), /* t */
	MODPROBE_OPT_VERONLY    = (INSMOD_OPT_UNUSED << 7), /* V */
	MODPROBE_OPT_CONFIGFILE = (INSMOD_OPT_UNUSED << 8), /* C */
	MODPROBE_OPT_BLACKLIST  = (INSMOD_OPT_UNUSED << 9) * ENABLE_FEATURE_MODPROBE_BLACKLIST,
};

struct globals {
	llist_t *db; /* MEs of all modules ever seen (caching for speed) */
	llist_t *probes; /* MEs of module(s) requested on cmdline */
	char *cmdline_mopts; /* module options from cmdline */
	int num_unresolved_deps;
	/* bool. "Did we have 'symbol:FOO' requested on cmdline?" */
	smallint need_symbols;
};
#define G (*(struct globals*)&bb_common_bufsiz1)
#define INIT_G() do { } while (0)


static int read_config(const char *path);

static char *gather_options_str(char *opts, const char *append)
{
	/* Speed-optimized. We call gather_options_str many times. */
	if (append) {
		if (opts == NULL) {
			opts = xstrdup(append);
		} else {
			int optlen = strlen(opts);
			opts = xrealloc(opts, optlen + strlen(append) + 2);
			sprintf(opts + optlen, " %s", append);
		}
	}
	return opts;
}

static struct module_entry *helper_get_module(const char *module, int create)
{
	char modname[MODULE_NAME_LEN];
	struct module_entry *e;
	llist_t *l;

	filename2modname(module, modname);
	for (l = G.db; l != NULL; l = l->link) {
		e = (struct module_entry *) l->data;
		if (strcmp(e->modname, modname) == 0)
			return e;
	}
	if (!create)
		return NULL;

	e = xzalloc(sizeof(*e));
	e->modname = xstrdup(modname);
	llist_add_to(&G.db, e);

	return e;
}
static struct module_entry *get_or_add_modentry(const char *module)
{
	return helper_get_module(module, 1);
}
static struct module_entry *get_modentry(const char *module)
{
	return helper_get_module(module, 0);
}

static void add_probe(const char *name)
{
	struct module_entry *m;

	m = get_or_add_modentry(name);
	if (!(option_mask32 & MODPROBE_OPT_REMOVE)
	 && (m->flags & MODULE_FLAG_LOADED)
	) {
		DBG("skipping %s, it is already loaded", name);
		return;
	}

	DBG("queuing %s", name);
	m->probed_name = name;
	m->flags |= MODULE_FLAG_NEED_DEPS;
	llist_add_to_end(&G.probes, m);
	G.num_unresolved_deps++;
	if (ENABLE_FEATURE_MODUTILS_SYMBOLS
	 && strncmp(m->modname, "symbol:", 7) == 0
	) {
		G.need_symbols = 1;
	}
}

static int FAST_FUNC config_file_action(const char *filename,
					struct stat *statbuf UNUSED_PARAM,
					void *userdata UNUSED_PARAM,
					int depth UNUSED_PARAM)
{
	char *tokens[3];
	parser_t *p;
	struct module_entry *m;
	int rc = TRUE;

	if (bb_basename(filename)[0] == '.')
		goto error;

	p = config_open2(filename, fopen_for_read);
	if (p == NULL) {
		rc = FALSE;
		goto error;
	}

	while (config_read(p, tokens, 3, 2, "# \t", PARSE_NORMAL)) {
//Use index_in_strings?
		if (strcmp(tokens[0], "alias") == 0) {
			/* alias <wildcard> <modulename> */
			llist_t *l;
			char wildcard[MODULE_NAME_LEN];
			char *rmod;

			if (tokens[2] == NULL)
				continue;
			filename2modname(tokens[1], wildcard);

			for (l = G.probes; l != NULL; l = l->link) {
				m = (struct module_entry *) l->data;
				if (fnmatch(wildcard, m->modname, 0) != 0)
					continue;
				rmod = filename2modname(tokens[2], NULL);
				llist_add_to(&m->realnames, rmod);

				if (m->flags & MODULE_FLAG_NEED_DEPS) {
					m->flags &= ~MODULE_FLAG_NEED_DEPS;
					G.num_unresolved_deps--;
				}

				m = get_or_add_modentry(rmod);
				if (!(m->flags & MODULE_FLAG_NEED_DEPS)) {
					m->flags |= MODULE_FLAG_NEED_DEPS;
					G.num_unresolved_deps++;
				}
			}
		} else if (strcmp(tokens[0], "options") == 0) {
			/* options <modulename> <option...> */
			if (tokens[2] == NULL)
				continue;
			m = get_or_add_modentry(tokens[1]);
			m->options = gather_options_str(m->options, tokens[2]);
		} else if (strcmp(tokens[0], "include") == 0) {
			/* include <filename> */
			read_config(tokens[1]);
		} else if (ENABLE_FEATURE_MODPROBE_BLACKLIST
		 && strcmp(tokens[0], "blacklist") == 0
		) {
			/* blacklist <modulename> */
			get_or_add_modentry(tokens[1])->flags |= MODULE_FLAG_BLACKLISTED;
		}
	}
	config_close(p);
 error:
	return rc;
}

static int read_config(const char *path)
{
	return recursive_action(path, ACTION_RECURSE | ACTION_QUIET,
				config_file_action, NULL, NULL, 1);
}

static const char *humanly_readable_name(struct module_entry *m)
{
	/* probed_name may be NULL. modname always exists. */
	return m->probed_name ? m->probed_name : m->modname;
}

/* Return: similar to bb_init_module:
 * 0 on success,
 * -errno on open/read error,
 * errno on init_module() error
 */
static int do_modprobe(struct module_entry *m)
{
	struct module_entry *m2 = m2; /* for compiler */
	char *fn, *options;
	int rc, first;
	llist_t *l;

	if (!(m->flags & MODULE_FLAG_FOUND_IN_MODDEP)) {
		if (!(option_mask32 & INSMOD_OPT_SILENT))
			bb_error_msg("module %s not found in modules.dep",
				humanly_readable_name(m));
		return -ENOENT;
	}
	DBG("do_modprob'ing %s", m->modname);

	if (!(option_mask32 & MODPROBE_OPT_REMOVE))
		m->deps = llist_rev(m->deps);

	for (l = m->deps; l != NULL; l = l->link)
		DBG("dep: %s", l->data);

	first = 1;
	rc = 0;
	while (m->deps) {
		rc = 0;
		fn = llist_pop(&m->deps); /* we leak it */
		m2 = get_or_add_modentry(fn);

		if (option_mask32 & MODPROBE_OPT_REMOVE) {
			/* modprobe -r */
			if (m2->flags & MODULE_FLAG_LOADED) {
				rc = bb_delete_module(m2->modname, O_EXCL);
				if (rc) {
					if (first) {
						bb_error_msg("failed to unload module %s: %s",
							humanly_readable_name(m2),
							moderror(rc));
						break;
					}
				} else {
					m2->flags &= ~MODULE_FLAG_LOADED;
				}
			}
			/* do not error out if *deps* fail to unload */
			first = 0;
			continue;
		}

		if (m2->flags & MODULE_FLAG_LOADED) {
			DBG("%s is already loaded, skipping", fn);
			continue;
		}

		options = m2->options;
		m2->options = NULL;
		if (m == m2)
			options = gather_options_str(options, G.cmdline_mopts);
		rc = bb_init_module(fn, options);
		DBG("loaded %s '%s', rc:%d", fn, options, rc);
		if (rc == EEXIST)
			rc = 0;
		free(options);
		if (rc) {
			bb_error_msg("failed to load module %s (%s): %s",
				humanly_readable_name(m2),
				fn,
				moderror(rc)
			);
			break;
		}
		m2->flags |= MODULE_FLAG_LOADED;
	}

	return rc;
}

static void load_modules_dep(void)
{
	struct module_entry *m;
	char *colon, *tokens[2];
	parser_t *p;

	/* Modprobe does not work at all without modules.dep,
	 * even if the full module name is given. Returning error here
	 * was making us later confuse user with this message:
	 * "module /full/path/to/existing/file/module.ko not found".
	 * It's better to die immediately, with good message.
	 * xfopen_for_read provides that. */
	p = config_open2(CONFIG_DEFAULT_DEPMOD_FILE, xfopen_for_read);

	while (G.num_unresolved_deps
	 && config_read(p, tokens, 2, 1, "# \t", PARSE_NORMAL)
	) {
		colon = last_char_is(tokens[0], ':');
		if (colon == NULL)
			continue;
		*colon = 0;

		m = get_modentry(tokens[0]);
		if (m == NULL)
			continue;

		/* Optimization... */
		if ((m->flags & MODULE_FLAG_LOADED)
		 && !(option_mask32 & MODPROBE_OPT_REMOVE)
		) {
			DBG("skip deps of %s, it's already loaded", tokens[0]);
			continue;
		}

		m->flags |= MODULE_FLAG_FOUND_IN_MODDEP;
		if ((m->flags & MODULE_FLAG_NEED_DEPS) && (m->deps == NULL)) {
			G.num_unresolved_deps--;
			llist_add_to(&m->deps, xstrdup(tokens[0]));
			if (tokens[1])
				string_to_llist(tokens[1], &m->deps, " \t");
		} else
			DBG("skipping dep line");
	}
	config_close(p);
}

int modprobe_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int modprobe_main(int argc UNUSED_PARAM, char **argv)
{
	struct utsname uts;
	int rc;
	unsigned opt;
	struct module_entry *me;

	opt_complementary = "q-v:v-q";
	opt = getopt32(argv, INSMOD_OPTS MODPROBE_OPTS INSMOD_ARGS, NULL, NULL);
	argv += optind;

	if (opt & (MODPROBE_OPT_DUMP_ONLY | MODPROBE_OPT_LIST_ONLY |
				MODPROBE_OPT_SHOW_ONLY))
		bb_error_msg_and_die("not supported");

	if (!argv[0]) {
		if (opt & MODPROBE_OPT_REMOVE) {
			/* "modprobe -r" (w/o params).
			 * "If name is NULL, all unused modules marked
			 * autoclean will be removed".
			 */
			if (bb_delete_module(NULL, O_NONBLOCK | O_EXCL) != 0)
				bb_perror_msg_and_die("rmmod");
		}
		return EXIT_SUCCESS;
	}

	/* Goto modules location */
	xchdir(CONFIG_DEFAULT_MODULES_DIR);
	uname(&uts);
	xchdir(uts.release);

	/* Retrieve module names of already loaded modules */
	{
		char *s;
		parser_t *parser = config_open2("/proc/modules", fopen_for_read);
		while (config_read(parser, &s, 1, 1, "# \t", PARSE_NORMAL & ~PARSE_GREEDY))
			get_or_add_modentry(s)->flags |= MODULE_FLAG_LOADED;
		config_close(parser);
	}

	if (opt & (MODPROBE_OPT_INSERT_ALL | MODPROBE_OPT_REMOVE)) {
		/* Each argument is a module name */
		do {
			DBG("adding module %s", *argv);
			add_probe(*argv++);
		} while (*argv);
	} else {
		/* First argument is module name, rest are parameters */
		DBG("probing just module %s", *argv);
		add_probe(argv[0]);
		G.cmdline_mopts = parse_cmdline_module_options(argv);
	}

	/* Happens if all requested modules are already loaded */
	if (G.probes == NULL)
		return EXIT_SUCCESS;

	read_config("/etc/modprobe.conf");
	read_config("/etc/modprobe.d");
	if (ENABLE_FEATURE_MODUTILS_SYMBOLS && G.need_symbols)
		read_config("modules.symbols");
	load_modules_dep();
	if (ENABLE_FEATURE_MODUTILS_ALIAS && G.num_unresolved_deps) {
		read_config("modules.alias");
		load_modules_dep();
	}

	rc = 0;
	while ((me = llist_pop(&G.probes)) != NULL) {
		if (me->realnames == NULL) {
			DBG("probing by module name");
			/* This is not an alias. Literal names are blacklisted
			 * only if '-b' is given.
			 */
			if (!(opt & MODPROBE_OPT_BLACKLIST)
			 || !(me->flags & MODULE_FLAG_BLACKLISTED)
			) {
				rc |= do_modprobe(me);
			}
			continue;
		}

		/* Probe all real names for the alias */
		do {
			char *realname = llist_pop(&me->realnames);
			struct module_entry *m2;

			DBG("probing alias %s by realname %s", me->modname, realname);
			m2 = get_or_add_modentry(realname);
			if (!(m2->flags & MODULE_FLAG_BLACKLISTED)
			 && (!(m2->flags & MODULE_FLAG_LOADED)
			    || (opt & MODPROBE_OPT_REMOVE))
			) {
//TODO: we can pass "me" as 2nd param to do_modprobe,
//and make do_modprobe emit more meaningful error messages
//with alias name included, not just module name alias resolves to.
				rc |= do_modprobe(m2);
			}
			free(realname);
		} while (me->realnames != NULL);
	}

	return (rc != 0);
}
