/* vi: set sw=4 ts=4: */
/*
 * Modprobe written from scratch for BusyBox
 *
 * Copyright (c) 2008 Timo Teras <timo.teras@iki.fi>
 * Copyright (c) 2008 Vladimir Dronnikov
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "modutils.h"
#include <sys/utsname.h>
#include <fnmatch.h>

#define MODULE_FLAG_LOADED		0x0001
#define MODULE_FLAG_NEED_DEPS		0x0002
//Misnomer? Seems to mean "was seen in modules.dep":
#define MODULE_FLAG_EXISTS		0x0004
#define MODULE_FLAG_BLACKLISTED		0x0008

struct module_entry { /* I'll call it ME. */
	unsigned flags;
	char *modname; /* stripped of /path/, .ext and s/-/_/g */
	const char *probed_name; /* verbatim as seen on cmdline */
	llist_t *aliases; /* strings. aliases from config files */
	llist_t *options; /* strings. options from config files */
	llist_t *deps; /* strings. modules we depend on */
};

#define MODPROBE_OPTS  "acdlnrt:VC:" USE_FEATURE_MODPROBE_BLACKLIST("b")
enum {
	MODPROBE_OPT_INSERT_ALL	= (INSMOD_OPT_UNUSED << 0), /* a */
	MODPROBE_OPT_DUMP_ONLY	= (INSMOD_OPT_UNUSED << 1), /* c */
	MODPROBE_OPT_D		= (INSMOD_OPT_UNUSED << 2), /* d */
	MODPROBE_OPT_LIST_ONLY	= (INSMOD_OPT_UNUSED << 3), /* l */
	MODPROBE_OPT_SHOW_ONLY	= (INSMOD_OPT_UNUSED << 4), /* n */
	MODPROBE_OPT_REMOVE	= (INSMOD_OPT_UNUSED << 5), /* r */
	MODPROBE_OPT_RESTRICT	= (INSMOD_OPT_UNUSED << 6), /* t */
	MODPROBE_OPT_VERONLY	= (INSMOD_OPT_UNUSED << 7), /* V */
	MODPROBE_OPT_CONFIGFILE	= (INSMOD_OPT_UNUSED << 8), /* C */
	MODPROBE_OPT_BLACKLIST	= (INSMOD_OPT_UNUSED << 9) * ENABLE_FEATURE_MODPROBE_BLACKLIST,
};

struct globals {
	llist_t *db; /* MEs of all modules ever seen (caching for speed) */
	llist_t *probes; /* MEs of module(s) requested on cmdline */
	llist_t *cmdline_mopts; /* strings. module options (from cmdline) */
	int num_deps; /* what is this? */
	/* bool. "Did we have 'symbol:FOO' requested on cmdline?" */
	smallint need_symbols;
};
#define G (*(struct globals*)&bb_common_bufsiz1)
#define INIT_G() do { } while (0)


static int read_config(const char *path);

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
	m->probed_name = name;
	m->flags |= MODULE_FLAG_NEED_DEPS;
	llist_add_to(&G.probes, m);

	G.num_deps++;
	if (ENABLE_FEATURE_MODUTILS_SYMBOLS
	 && strncmp(m->modname, "symbol:", 7) == 0)
		G.need_symbols = 1;
}

static int FAST_FUNC config_file_action(const char *filename,
					struct stat *statbuf UNUSED_PARAM,
					void *userdata UNUSED_PARAM,
					int depth UNUSED_PARAM)
{
	RESERVE_CONFIG_BUFFER(modname, MODULE_NAME_LEN);
	char *tokens[3], *rmod;
	parser_t *p;
	llist_t *l;
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
			filename2modname(tokens[1], modname);
			if (tokens[2] == NULL)
				continue;

			for (l = G.probes; l != NULL; l = l->link) {
				m = (struct module_entry *) l->data;
				if (fnmatch(modname, m->modname, 0) != 0)
					continue;
				rmod = filename2modname(tokens[2], NULL);
				llist_add_to(&m->aliases, rmod);

				if (m->flags & MODULE_FLAG_NEED_DEPS) {
					m->flags &= ~MODULE_FLAG_NEED_DEPS;
					G.num_deps--;
				}

				m = get_or_add_modentry(rmod);
				m->flags |= MODULE_FLAG_NEED_DEPS;
				G.num_deps++;
			}
		} else if (strcmp(tokens[0], "options") == 0) {
			if (tokens[2] == NULL)
				continue;
			m = get_or_add_modentry(tokens[1]);
			llist_add_to(&m->options, xstrdup(tokens[2]));
		} else if (strcmp(tokens[0], "include") == 0) {
			read_config(tokens[1]);
		} else if (ENABLE_FEATURE_MODPROBE_BLACKLIST
		 && strcmp(tokens[0], "blacklist") == 0
		) {
			get_or_add_modentry(tokens[1])->flags |= MODULE_FLAG_BLACKLISTED;
		}
	}
	config_close(p);
error:
	RELEASE_CONFIG_BUFFER(modname);
	return rc;
}

static int read_config(const char *path)
{
	return recursive_action(path, ACTION_RECURSE | ACTION_QUIET,
				config_file_action, NULL, NULL, 1);
}

static char *gather_options(char *opts, llist_t *o)
{
	int optlen;

	if (opts == NULL)
		opts = xstrdup("");
	optlen = strlen(opts);

	for (; o != NULL; o = o->link) {
		opts = xrealloc(opts, optlen + strlen(o->data) + 2);
		optlen += sprintf(opts + optlen, "%s ", o->data);
	}
	return opts;
}

static int do_modprobe(struct module_entry *m)
{
	struct module_entry *m2;
	char *fn, *options;
	int rc = -1;

	if (!(m->flags & MODULE_FLAG_EXISTS))
		return -ENOENT;

	if (!(option_mask32 & MODPROBE_OPT_REMOVE))
		m->deps = llist_rev(m->deps);

	rc = 0;
	while (m->deps && rc == 0) {
		fn = llist_pop(&m->deps);
		m2 = get_or_add_modentry(fn);
		if (option_mask32 & MODPROBE_OPT_REMOVE) {
			if (bb_delete_module(m->modname, O_EXCL) != 0)
				rc = errno;
		} else if (!(m2->flags & MODULE_FLAG_LOADED)) {
			options = gather_options(NULL, m2->options);
			if (m == m2)
				options = gather_options(options, G.cmdline_mopts);
//TODO: looks like G.cmdline_mopts can contain either NULL or just one string, not more?
			rc = bb_init_module(fn, options);
			if (rc == 0)
				m2->flags |= MODULE_FLAG_LOADED;
			free(options);
		}

		free(fn);
	}

	if (rc > 0 && !(option_mask32 & INSMOD_OPT_SILENT)) {
		bb_error_msg("failed to %sload module %s: %s",
			(option_mask32 & MODPROBE_OPT_REMOVE) ? "un" : "",
			m->probed_name ? m->probed_name : m->modname,
			moderror(rc)
		);
	}

	return rc;
}

static void load_modules_dep(void)
{
	struct module_entry *m;
	char *colon, *tokens[2];
	parser_t *p;

	p = config_open2(CONFIG_DEFAULT_DEPMOD_FILE, xfopen_for_read);
//Still true?
	/* Modprobe does not work at all without modprobe.dep,
	 * even if the full module name is given. Returning error here
	 * was making us later confuse user with this message:
	 * "module /full/path/to/existing/file/module.ko not found".
	 * It's better to die immediately, with good message: */
//	if (p == NULL)
//		bb_perror_msg_and_die("can't open '%s'", CONFIG_DEFAULT_DEPMOD_FILE);

	while (G.num_deps
	 && config_read(p, tokens, 2, 1, "# \t", PARSE_NORMAL)
	) {
		colon = last_char_is(tokens[0], ':');
		if (colon == NULL)
			continue;
		*colon = 0;

		m = get_modentry(tokens[0]);
		if (m == NULL)
			continue;
//Can we skip it too if it is MODULE_FLAG_LOADED?

		m->flags |= MODULE_FLAG_EXISTS;
		if ((m->flags & MODULE_FLAG_NEED_DEPS) && (m->deps == NULL)) {
			G.num_deps--;
			llist_add_to(&m->deps, xstrdup(tokens[0]));
			if (tokens[1])
				string_to_llist(tokens[1], &m->deps, " ");
		}
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

	/* Goto modules location */
	xchdir(CONFIG_DEFAULT_MODULES_DIR);
	uname(&uts);
	xchdir(uts.release);

	if (!argv[0]) {
		if (opt & MODPROBE_OPT_REMOVE) {
			/* TODO: comment here */
			if (bb_delete_module(NULL, O_NONBLOCK|O_EXCL) != 0)
				bb_perror_msg_and_die("rmmod");
		}
		return EXIT_SUCCESS;
	}

	if (opt & MODPROBE_OPT_INSERT_ALL) {
		/* Each argument is a module name */
		do {
			add_probe(*argv++);
		} while (*argv);
		G.probes = llist_rev(G.probes);
	} else {
		/* First argument is module name, rest are parameters */
		add_probe(argv[0]);
//TODO: looks like G.cmdline_mopts can contain either NULL or just one string, not more?
//optimize it them
		llist_add_to(&G.cmdline_mopts, parse_cmdline_module_options(argv));
	}

	/* Retrieve module names of already loaded modules */
	{
		char *s;
		parser_t *parser = config_open2("/proc/modules", fopen_for_read);
		while (config_read(parser, &s, 1, 1, "# \t", PARSE_NORMAL & ~PARSE_GREEDY))
			get_or_add_modentry(s)->flags |= MODULE_FLAG_LOADED;
		config_close(parser);
	}

	read_config("/etc/modprobe.conf");
	read_config("/etc/modprobe.d");
	if (ENABLE_FEATURE_MODUTILS_SYMBOLS && G.need_symbols)
		read_config("modules.symbols");
	load_modules_dep();
	if (ENABLE_FEATURE_MODUTILS_ALIAS && G.num_deps) {
		read_config("modules.alias");
		load_modules_dep();
	}

	while ((me = llist_pop(&G.probes)) != NULL) {
		if (me->aliases == NULL) {
			/* Try if module by literal name is found; literal
			 * names are blacklisted only if '-b' is given. */
			if (!(opt & MODPROBE_OPT_BLACKLIST)
			 || !(me->flags & MODULE_FLAG_BLACKLISTED)
			) {
				rc = do_modprobe(me);
				if (rc < 0 && !(opt & INSMOD_OPT_SILENT))
					bb_error_msg("module %s not found",
						     me->probed_name);
			}
		} else {
			/* Probe all aliases */
			do {
				char *realname = llist_pop(&me->aliases);
				struct module_entry *m2;

				m2 = get_or_add_modentry(realname);
				if (!(m2->flags & MODULE_FLAG_BLACKLISTED))
					do_modprobe(m2);
				free(realname);
			} while (me->aliases != NULL);
		}
	}

	return EXIT_SUCCESS;
}
