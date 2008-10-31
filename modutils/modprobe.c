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

struct modprobe_option {
	char *module;
	char *option;
};

struct modprobe_conf {
	char probename[MODULE_NAME_LEN];
	llist_t *options;
	llist_t *aliases;
#if ENABLE_FEATURE_MODPROBE_BLACKLIST
#define add_to_blacklist(conf, name) llist_add_to(&conf->blacklist, name)
#define check_blacklist(conf, name) (llist_find(conf->blacklist, name) == NULL)
	llist_t *blacklist;
#else
#define add_to_blacklist(conf, name) do {} while (0)
#define check_blacklist(conf, name) (1)
#endif
};

#define MODPROBE_OPTS	"acdlnrt:VC:" USE_FEATURE_MODPROBE_BLACKLIST("b")
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

static llist_t *loaded;

static int read_config(struct modprobe_conf *conf, const char *path);

static void add_option(llist_t **all_opts, const char *module, const char *opts)
{
	struct modprobe_option *o;

	o = xzalloc(sizeof(struct modprobe_option));
	if (module)
		o->module = filename2modname(module, NULL);
	o->option = xstrdup(opts);
	llist_add_to(all_opts, o);
}

static int FAST_FUNC config_file_action(const char *filename,
					struct stat *statbuf UNUSED_PARAM,
					void *userdata,
					int depth UNUSED_PARAM)
{
	struct modprobe_conf *conf = (struct modprobe_conf *) userdata;
	RESERVE_CONFIG_BUFFER(modname, MODULE_NAME_LEN);
	char *tokens[3];
	parser_t *p;
	int rc = TRUE;

	if (bb_basename(filename)[0] == '.')
		goto error;

	p = config_open2(filename, fopen_for_read);
	if (p == NULL) {
		rc = FALSE;
		goto error;
	}

	while (config_read(p, tokens, 3, 2, "# \t", PARSE_NORMAL)) {
		if (strcmp(tokens[0], "alias") == 0) {
			filename2modname(tokens[1], modname);
			if (tokens[2] &&
			    fnmatch(modname, conf->probename, 0) == 0)
				llist_add_to(&conf->aliases,
					filename2modname(tokens[2], NULL));
		} else if (strcmp(tokens[0], "options") == 0) {
			if (tokens[2])
				add_option(&conf->options, tokens[1], tokens[2]);
		} else if (strcmp(tokens[0], "include") == 0) {
			read_config(conf, tokens[1]);
		} else if (ENABLE_FEATURE_MODPROBE_BLACKLIST &&
			   strcmp(tokens[0], "blacklist") == 0) {
			add_to_blacklist(conf, xstrdup(tokens[1]));
		}
	}
	config_close(p);
error:
	if (ENABLE_FEATURE_CLEAN_UP)
		RELEASE_CONFIG_BUFFER(modname);
	return rc;
}

static int read_config(struct modprobe_conf *conf, const char *path)
{
	return recursive_action(path, ACTION_RECURSE | ACTION_QUIET,
				config_file_action, NULL, conf, 1);
}

static char *gather_options(llist_t *first, const char *module, int usecmdline)
{
	struct modprobe_option *opt;
	llist_t *n;
	char *opts = xstrdup("");
	int optlen = 0;

	for (n = first; n != NULL; n = n->link) {
		opt = (struct modprobe_option *) n->data;

		if (opt->module == NULL && !usecmdline)
			continue;
		if (opt->module != NULL && strcmp(opt->module, module) != 0)
			continue;

		opts = xrealloc(opts, optlen + strlen(opt->option) + 2);
		optlen += sprintf(opts + optlen, "%s ", opt->option);
	}
	return opts;
}

static int do_modprobe(struct modprobe_conf *conf, const char *module)
{
	RESERVE_CONFIG_BUFFER(modname, MODULE_NAME_LEN);
	llist_t *deps = NULL;
	char *fn, *options, *colon = NULL, *tokens[2];
	parser_t *p;
	int rc = -1;

	p = config_open2(CONFIG_DEFAULT_DEPMOD_FILE, fopen_for_read);
	if (p == NULL)
		goto error;

	while (config_read(p, tokens, 2, 1, "# \t", PARSE_NORMAL)) {
		colon = last_char_is(tokens[0], ':');
		if (colon == NULL)
			continue;

		filename2modname(tokens[0], modname);
		if (strcmp(modname, module) == 0)
			break;

		colon = NULL;
	}
	if (colon == NULL)
		goto error_not_found;

	colon[0] = '\0';
	llist_add_to(&deps, xstrdup(tokens[0]));
	if (tokens[1])
		string_to_llist(tokens[1], &deps, " ");

	if (!(option_mask32 & MODPROBE_OPT_REMOVE))
		deps = llist_rev(deps);

	rc = 0;
	while (deps && rc == 0) {
		fn = llist_pop(&deps);
		filename2modname(fn, modname);
		if (option_mask32 & MODPROBE_OPT_REMOVE) {
			if (bb_delete_module(modname, O_EXCL) != 0)
				rc = errno;
		} else if (llist_find(loaded, modname) == NULL) {
			options = gather_options(conf->options, modname,
						 strcmp(modname, module) == 0);
			rc = bb_init_module(fn, options);
			if (rc == 0)
				llist_add_to(&loaded, xstrdup(modname));
			if (ENABLE_FEATURE_CLEAN_UP)
				free(options);
		}

		if (ENABLE_FEATURE_CLEAN_UP)
			free(fn);
	}

error_not_found:
	config_close(p);
error:
	if (ENABLE_FEATURE_CLEAN_UP)
		RELEASE_CONFIG_BUFFER(modname);
	if (rc > 0 && !(option_mask32 & INSMOD_OPT_SILENT))
		bb_error_msg("Failed to %sload module %s: %s.",
			     (option_mask32 & MODPROBE_OPT_REMOVE) ? "un" : "",
			     module, moderror(rc));
	return rc;
}

int modprobe_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int modprobe_main(int argc UNUSED_PARAM, char **argv)
{
	struct utsname uts;
	int rc;
	unsigned opt;
	llist_t *options = NULL;

	opt_complementary = "q-v:v-q";
	opt = getopt32(argv, INSMOD_OPTS MODPROBE_OPTS INSMOD_ARGS,
		 NULL, NULL);
	argv += optind;

	if (opt & (MODPROBE_OPT_DUMP_ONLY | MODPROBE_OPT_LIST_ONLY |
				MODPROBE_OPT_SHOW_ONLY))
		bb_error_msg_and_die("not supported");

	/* goto modules location */
	xchdir(CONFIG_DEFAULT_MODULES_DIR);
	uname(&uts);
	xchdir(uts.release);

	if (!argv[0]) {
		if (opt & MODPROBE_OPT_REMOVE) {
			if (bb_delete_module(NULL, O_NONBLOCK|O_EXCL) != 0)
				bb_perror_msg_and_die("rmmod");
		}
		return EXIT_SUCCESS;
	}
	if (!(opt & MODPROBE_OPT_INSERT_ALL)) {
		/* If not -a, we have only one module name,
		 * the rest of parameters are options */
		add_option(&options, NULL, parse_cmdline_module_options(argv));
		argv[1] = NULL;
	}

	/* cache modules */
	{
		char *s;
		parser_t *parser = config_open2("/proc/modules", fopen_for_read);
		while (config_read(parser, &s, 1, 1, "# \t", PARSE_NORMAL & ~PARSE_GREEDY))
			llist_add_to(&loaded, xstrdup(s));
		config_close(parser);
	}

	while (*argv) {
		const char *arg = *argv;
		struct modprobe_conf *conf;

		conf = xzalloc(sizeof(*conf));
		conf->options = options;
		filename2modname(arg, conf->probename);
		read_config(conf, "/etc/modprobe.conf");
		read_config(conf, "/etc/modprobe.d");
		if (ENABLE_FEATURE_MODUTILS_SYMBOLS
		 && conf->aliases == NULL
		 && strncmp(arg, "symbol:", 7) == 0
		) {
			read_config(conf, "modules.symbols");
		}

		if (ENABLE_FEATURE_MODUTILS_ALIAS && conf->aliases == NULL)
			read_config(conf, "modules.alias");

		if (conf->aliases == NULL) {
			/* Try if module by literal name is found; literal
			 * names are blacklist only if '-b' is given. */
			if (!(opt & MODPROBE_OPT_BLACKLIST) ||
			    check_blacklist(conf, conf->probename)) {
				rc = do_modprobe(conf, conf->probename);
				if (rc < 0 && !(opt & INSMOD_OPT_SILENT))
					bb_error_msg("Module %s not found.", arg);
			}
		} else {
			/* Probe all aliases */
			while (conf->aliases != NULL) {
				char *realname = llist_pop(&conf->aliases);
				if (check_blacklist(conf, realname))
					do_modprobe(conf, realname);
				if (ENABLE_FEATURE_CLEAN_UP)
					free(realname);
			}
		}
		argv++;
	}

	return EXIT_SUCCESS;
}
