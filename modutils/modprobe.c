/* vi: set sw=4 ts=4: */
/*
 * Modprobe written from scratch for BusyBox
 *
 * Copyright (c) 2002 by Robert Griebl, griebl@gmx.de
 * Copyright (c) 2003 by Andrew Dennison, andrew.dennison@motec.com.au
 * Copyright (c) 2005 by Jim Bauer, jfbauer@nfr.com
 *
 * Portions Copyright (c) 2005 by Yann E. MORIN, yann.morin.1998@anciens.enib.fr
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
*/

#include "busybox.h"
#include <sys/utsname.h>
#include <fnmatch.h>

struct mod_opt_t {	/* one-way list of options to pass to a module */
	char *  m_opt_val;
	struct mod_opt_t * m_next;
};

struct dep_t {	/* one-way list of dependency rules */
	/* a dependency rule */
	char *  m_name;                         /* the module name*/
	char *  m_path;                         /* the module file path */
	struct mod_opt_t *  m_options;	        /* the module options */

	int     m_isalias  : 1;                 /* the module is an alias */
	int     m_reserved : 15;                /* stuffin' */

	int     m_depcnt   : 16;                /* the number of dependable module(s) */
	char ** m_deparr;                       /* the list of dependable module(s) */

	struct dep_t * m_next;                  /* the next dependency rule */
};

struct mod_list_t {	/* two-way list of modules to process */
	/* a module description */
	char *  m_name;
	char *  m_path;
	struct mod_opt_t *  m_options;

	struct mod_list_t * m_prev;
	struct mod_list_t * m_next;
};


static struct dep_t *depend;

#define main_options "acdklnqrst:vVC:"
#define INSERT_ALL     1        /* a */
#define DUMP_CONF_EXIT 2        /* c */
#define D_OPT_IGNORED  4        /* d */
#define AUTOCLEAN_FLG  8        /* k */
#define LIST_ALL       16       /* l */
#define SHOW_ONLY      32       /* n */
#define QUIET          64       /* q */
#define REMOVE_OPT     128      /* r */
#define DO_SYSLOG      256      /* s */
#define RESTRICT_DIR   512      /* t */
#define VERBOSE        1024     /* v */
#define VERSION_ONLY   2048     /* V */
#define CONFIG_FILE    4096     /* C */

#define autoclean       (main_opts & AUTOCLEAN_FLG)
#define show_only       (main_opts & SHOW_ONLY)
#define quiet           (main_opts & QUIET)
#define remove_opt      (main_opts & REMOVE_OPT)
#define do_syslog       (main_opts & DO_SYSLOG)
#define verbose         (main_opts & VERBOSE)

static int main_opts;

static int parse_tag_value(char *buffer, char **ptag, char **pvalue)
{
	char *tag, *value;

	buffer = skip_whitespace(buffer);
	tag = value = buffer;
	while (!isspace(*value))
		if (!*value) return 0;
		else value++;
	*value++ = 0;
	value = skip_whitespace(value);
	if (!*value) return 0;

	*ptag = tag;
	*pvalue = value;

	return 1;
}

/*
 * This function appends an option to a list
 */
static struct mod_opt_t *append_option(struct mod_opt_t *opt_list, char *opt)
{
	struct mod_opt_t *ol = opt_list;

	if (ol) {
		while (ol->m_next) {
			ol = ol->m_next;
		}
		ol->m_next = xmalloc(sizeof(struct mod_opt_t));
		ol = ol->m_next;
	} else {
		ol = opt_list = xmalloc(sizeof(struct mod_opt_t));
	}

	ol->m_opt_val = xstrdup(opt);
	ol->m_next = NULL;

	return opt_list;
}

#if ENABLE_FEATURE_MODPROBE_MULTIPLE_OPTIONS
/* static char* parse_command_string(char* src, char **dst);
 *   src: pointer to string containing argument
 *   dst: pointer to where to store the parsed argument
 *   return value: the pointer to the first char after the parsed argument,
 *                 NULL if there was no argument parsed (only trailing spaces).
 *   Note that memory is allocated with xstrdup when a new argument was
 *   parsed. Don't forget to free it!
 */
#define ARG_EMPTY      0x00
#define ARG_IN_DQUOTES 0x01
#define ARG_IN_SQUOTES 0x02
static char *parse_command_string(char *src, char **dst)
{
	int opt_status = ARG_EMPTY;
	char* tmp_str;

	/* Dumb you, I have nothing to do... */
	if (src == NULL) return src;

	/* Skip leading spaces */
	while (*src == ' ') {
		src++;
	}
	/* Is the end of string reached? */
	if (*src == '\0') {
		return NULL;
	}
	/* Reached the start of an argument
	 * By the way, we duplicate a little too much
	 * here but what is too much is freed later. */
	*dst = tmp_str = xstrdup(src);
	/* Get to the end of that argument */
	while (*tmp_str != '\0'
	 && (*tmp_str != ' ' || (opt_status & (ARG_IN_DQUOTES | ARG_IN_SQUOTES)))
	) {
		switch (*tmp_str) {
		case '\'':
			if (opt_status & ARG_IN_DQUOTES) {
				/* Already in double quotes, keep current char as is */
			} else {
				/* shift left 1 char, until end of string: get rid of the opening/closing quotes */
				memmove(tmp_str, tmp_str + 1, strlen(tmp_str));
				/* mark me: we enter or leave single quotes */
				opt_status ^= ARG_IN_SQUOTES;
				/* Back one char, as we need to re-scan the new char there. */
				tmp_str--;
			}
			break;
		case '"':
			if (opt_status & ARG_IN_SQUOTES) {
				/* Already in single quotes, keep current char as is */
			} else {
				/* shift left 1 char, until end of string: get rid of the opening/closing quotes */
				memmove(tmp_str, tmp_str + 1, strlen(tmp_str));
				/* mark me: we enter or leave double quotes */
				opt_status ^= ARG_IN_DQUOTES;
				/* Back one char, as we need to re-scan the new char there. */
				tmp_str--;
			}
			break;
		case '\\':
			if (opt_status & ARG_IN_SQUOTES) {
				/* Between single quotes: keep as is. */
			} else {
				switch (*(tmp_str+1)) {
				case 'a':
				case 'b':
				case 't':
				case 'n':
				case 'v':
				case 'f':
				case 'r':
				case '0':
					/* We escaped a special character. For now, keep
					 * both the back-slash and the following char. */
					tmp_str++; src++;
					break;
				default:
					/* We escaped a space or a single or double quote,
					 * or a back-slash, or a non-escapable char. Remove
					 * the '\' and keep the new current char as is. */
					memmove(tmp_str, tmp_str + 1, strlen(tmp_str));
					break;
				}
			}
			break;
		/* Any other char that is special shall appear here.
		 * Example: $ starts a variable
		case '$':
			do_variable_expansion();
			break;
		 * */
		default:
			/* any other char is kept as is. */
			break;
		}
		tmp_str++; /* Go to next char */
		src++; /* Go to next char to find the end of the argument. */
	}
	/* End of string, but still no ending quote */
	if (opt_status & (ARG_IN_DQUOTES | ARG_IN_SQUOTES)) {
		bb_error_msg_and_die("unterminated (single or double) quote in options list: %s", src);
	}
	*tmp_str++ = '\0';
	*dst = xrealloc(*dst, (tmp_str - *dst));
	return src;
}
#else
#define parse_command_string(src, dst)	(0)
#endif /* ENABLE_FEATURE_MODPROBE_MULTIPLE_OPTIONS */

/*
 * This function reads aliases and default module options from a configuration file
 * (/etc/modprobe.conf syntax). It supports includes (only files, no directories).
 */
static void include_conf(struct dep_t **first, struct dep_t **current, char *buffer, int buflen, int fd)
{
	int continuation_line = 0;

	// alias parsing is not 100% correct (no correct handling of continuation lines within an alias) !

	while (reads(fd, buffer, buflen)) {
		int l;
		char *p;

		p = strchr(buffer, '#');
		if (p)
			*p = 0;

		l = strlen(buffer);

		while (l && isspace(buffer[l-1])) {
			buffer[l-1] = 0;
			l--;
		}

		if (l == 0) {
			continuation_line = 0;
			continue;
		}

		if (!continuation_line) {
			if ((strncmp(buffer, "alias", 5) == 0) && isspace(buffer[5])) {
				char *alias, *mod;

				if (parse_tag_value(buffer + 6, &alias, &mod)) {
					/* handle alias as a module dependent on the aliased module */
					if (!*current) {
						(*first) = (*current) = xzalloc(sizeof(struct dep_t));
					}
					else {
						(*current)->m_next = xzalloc(sizeof(struct dep_t));
						(*current) = (*current)->m_next;
					}
					(*current)->m_name  = xstrdup(alias);
					(*current)->m_isalias = 1;

					if ((strcmp(mod, "off") == 0) || (strcmp(mod, "null") == 0)) {
						(*current)->m_depcnt = 0;
						(*current)->m_deparr = 0;
					}
					else {
						(*current)->m_depcnt  = 1;
						(*current)->m_deparr  = xmalloc(1 * sizeof(char *));
						(*current)->m_deparr[0] = xstrdup(mod);
					}
					(*current)->m_next    = 0;
				}
			}
			else if ((strncmp(buffer, "options", 7) == 0) && isspace(buffer[7])) {
				char *mod, *opt;

				/* split the line in the module/alias name, and options */
				if (parse_tag_value(buffer + 8, &mod, &opt)) {
					struct dep_t *dt;

					/* find the corresponding module */
					for (dt = *first; dt; dt = dt->m_next) {
						if (strcmp(dt->m_name, mod) == 0)
							break;
					}
					if (dt) {
						if (ENABLE_FEATURE_MODPROBE_MULTIPLE_OPTIONS) {
							char* new_opt = NULL;
							while ((opt = parse_command_string(opt, &new_opt))) {
								dt->m_options = append_option(dt->m_options, new_opt);
							}
						} else {
							dt->m_options = append_option(dt->m_options, opt);
						}
					}
				}
			}
			else if ((strncmp(buffer, "include", 7) == 0) && isspace(buffer[7])) {
				int fdi; char *filename;

				filename = skip_whitespace(buffer + 8);

				if ((fdi = open(filename, O_RDONLY)) >= 0) {
					include_conf(first, current, buffer, buflen, fdi);
					close(fdi);
				}
			}
		}
	}
}

/*
 * This function builds a list of dependency rules from /lib/modules/`uname -r`\modules.dep.
 * It then fills every modules and aliases with their default options, found by parsing
 * modprobe.conf (or modules.conf, or conf.modules).
 */
static struct dep_t *build_dep(void)
{
	int fd;
	struct utsname un;
	struct dep_t *first = 0;
	struct dep_t *current = 0;
	char buffer[2048];
	char *filename;
	int continuation_line = 0;
	int k_version;

	if (uname(&un))
		bb_error_msg_and_die("can't determine kernel version");

	k_version = 0;
	if (un.release[0] == '2') {
		k_version = un.release[2] - '0';
	}

	filename = xasprintf("/lib/modules/%s/modules.dep", un.release);
	fd = open(filename, O_RDONLY);
	if (ENABLE_FEATURE_CLEAN_UP)
		free(filename);
	if (fd < 0) {
		/* Ok, that didn't work.  Fall back to looking in /lib/modules */
		fd = open("/lib/modules/modules.dep", O_RDONLY);
		if (fd < 0) {
			return 0;
		}
	}

	while (reads(fd, buffer, sizeof(buffer))) {
		int l = strlen(buffer);
		char *p = 0;

		while (l > 0 && isspace(buffer[l-1])) {
			buffer[l-1] = 0;
			l--;
		}

		if (l == 0) {
			continuation_line = 0;
			continue;
		}

		/* Is this a new module dep description? */
		if (!continuation_line) {
			/* find the dep beginning */
			char *col = strchr(buffer, ':');
			char *dot = col;

			if (col) {
				/* This line is a dep description */
				char *mods;
				char *modpath;
				char *mod;

				/* Find the beginning of the module file name */
				*col = 0;
				mods = strrchr(buffer, '/');

				if (!mods)
					mods = buffer; /* no path for this module */
				else
					mods++; /* there was a path for this module... */

				/* find the path of the module */
				modpath = strchr(buffer, '/'); /* ... and this is the path */
				if (!modpath)
					modpath = buffer; /* module with no path */
				/* find the end of the module name in the file name */
				if (ENABLE_FEATURE_2_6_MODULES &&
				     (k_version > 4) && (*(col-3) == '.') &&
				    (*(col-2) == 'k') && (*(col-1) == 'o'))
					dot = col - 3;
				else
					if ((*(col-2) == '.') && (*(col-1) == 'o'))
						dot = col - 2;

				mod = xstrndup(mods, dot - mods);

				/* enqueue new module */
				if (!current) {
					first = current = xmalloc(sizeof(struct dep_t));
				}
				else {
					current->m_next = xmalloc(sizeof(struct dep_t));
					current = current->m_next;
				}
				current->m_name    = mod;
				current->m_path    = xstrdup(modpath);
				current->m_options = NULL;
				current->m_isalias = 0;
				current->m_depcnt  = 0;
				current->m_deparr  = 0;
				current->m_next    = 0;

				p = col + 1;
			}
			else
				/* this line is not a dep description */
				p = 0;
		}
		else
			/* It's a dep description continuation */
			p = buffer;

		while (p && *p && isblank(*p))
			p++;

		/* p points to the first dependable module; if NULL, no dependable module */
		if (p && *p) {
			char *end = &buffer[l-1];
			char *deps;
			char *dep;
			char *next;
			int ext = 0;

			while (isblank(*end) || (*end == '\\'))
				end--;

			do {
				/* search the end of the dependency */
				next = strchr(p, ' ');
				if (next) {
					*next = 0;
					next--;
				}
				else
					next = end;

				/* find the beginning of the module file name */
				deps = strrchr(p, '/');

				if (!deps || (deps < p)) {
					deps = p;

					while (isblank(*deps))
						deps++;
				} else
					deps++;

				/* find the end of the module name in the file name */
				if (ENABLE_FEATURE_2_6_MODULES
				 && (k_version > 4) && (*(next-2) == '.')
				 && (*(next-1) == 'k') && (*next == 'o'))
					ext = 3;
				else
					if ((*(next-1) == '.') && (*next == 'o'))
						ext = 2;

				/* Cope with blank lines */
				if ((next-deps-ext+1) <= 0)
					continue;
				dep = xstrndup(deps, next - deps - ext + 1);

				/* Add the new dependable module name */
				current->m_depcnt++;
				current->m_deparr = xrealloc(current->m_deparr,
						sizeof(char *) * current->m_depcnt);
				current->m_deparr[current->m_depcnt - 1] = dep;

				p = next + 2;
			} while (next < end);
		}

		/* is there other dependable module(s) ? */
		if (buffer[l-1] == '\\')
			continuation_line = 1;
		else
			continuation_line = 0;
	}
	close(fd);

	/*
	 * First parse system-specific options and aliases
	 * as they take precedence over the kernel ones.
	 */
	if (!ENABLE_FEATURE_2_6_MODULES
	 || (fd = open("/etc/modprobe.conf", O_RDONLY)) < 0)
		if ((fd = open("/etc/modules.conf", O_RDONLY)) < 0)
			fd = open("/etc/conf.modules", O_RDONLY);

	if (fd >= 0) {
		include_conf(&first, &current, buffer, sizeof(buffer), fd);
		close(fd);
	}

	/* Only 2.6 has a modules.alias file */
	if (ENABLE_FEATURE_2_6_MODULES) {
		/* Parse kernel-declared aliases */
		filename = xasprintf("/lib/modules/%s/modules.alias", un.release);
		fd = open(filename, O_RDONLY);
		if (fd < 0) {
			/* Ok, that didn't work.  Fall back to looking in /lib/modules */
			fd = open("/lib/modules/modules.alias", O_RDONLY);
		}
		if (ENABLE_FEATURE_CLEAN_UP)
			free(filename);

		if (fd >= 0) {
			include_conf(&first, &current, buffer, sizeof(buffer), fd);
			close(fd);
		}
	}

	return first;
}

/* return 1 = loaded, 0 = not loaded, -1 = can't tell */
static int already_loaded(const char *name)
{
	int fd, ret = 0;
	char buffer[4096];

	fd = open("/proc/modules", O_RDONLY);
	if (fd < 0)
		return -1;

	while (reads(fd, buffer, sizeof(buffer))) {
		char *p;

		p = strchr (buffer, ' ');
		if (p) {
			const char *n;

			// Truncate buffer at first space and check for matches, with
			// the idiosyncrasy that _ and - are interchangeable because the
			// 2.6 kernel does weird things.

			*p = 0;
			for (p = buffer, n = name; ; p++, n++) {
				if (*p != *n) {
					if ((*p == '_' || *p == '-') && (*n == '_' || *n == '-'))
						continue;
					break;
				}
				// If we made it to the end, that's a match.
				if (!*p) {
					ret = 1;
					goto done;
				}
			}
		}
	}
done:
	close (fd);
	return ret;
}

static int mod_process(struct mod_list_t *list, int do_insert)
{
	int rc = 0;
	char **argv = NULL;
	struct mod_opt_t *opts;
	int argc_malloc; /* never used when CONFIG_FEATURE_CLEAN_UP not defined */
	int argc;

	while (list) {
		argc = 0;
		if (ENABLE_FEATURE_CLEAN_UP)
			argc_malloc = 0;
		/* If CONFIG_FEATURE_CLEAN_UP is not defined, then we leak memory
		 * each time we allocate memory for argv.
		 * But it is (quite) small amounts of memory that leak each
		 * time a module is loaded,  and it is reclaimed when modprobe
		 * exits anyway (even when standalone shell?).
		 * This could become a problem when loading a module with LOTS of
		 * dependencies, with LOTS of options for each dependencies, with
		 * very little memory on the target... But in that case, the module
		 * would not load because there is no more memory, so there's no
		 * problem. */
		/* enough for minimal insmod (5 args + NULL) or rmmod (3 args + NULL) */
		argv = xmalloc(6 * sizeof(char*));
		if (do_insert) {
			if (already_loaded(list->m_name) != 1) {
				argv[argc++] = "insmod";
				if (ENABLE_FEATURE_2_4_MODULES) {
					if (do_syslog)
						argv[argc++] = "-s";
					if (autoclean)
						argv[argc++] = "-k";
					if (quiet)
						argv[argc++] = "-q";
					else if (verbose) /* verbose and quiet are mutually exclusive */
						argv[argc++] = "-v";
				}
				argv[argc++] = list->m_path;
				if (ENABLE_FEATURE_CLEAN_UP)
					argc_malloc = argc;
				opts = list->m_options;
				while (opts) {
					/* Add one more option */
					argc++;
					argv = xrealloc(argv,(argc + 1)* sizeof(char*));
					argv[argc-1] = opts->m_opt_val;
					opts = opts->m_next;
				}
			}
		} else {
			/* modutils uses short name for removal */
			if (already_loaded(list->m_name) != 0) {
				argv[argc++] = "rmmod";
				if (do_syslog)
					argv[argc++] = "-s";
				argv[argc++] = list->m_name;
				if (ENABLE_FEATURE_CLEAN_UP)
					argc_malloc = argc;
			}
		}
		argv[argc] = NULL;

		if (argc) {
			if (verbose) {
				printf("%s module %s\n", do_insert?"Loading":"Unloading", list->m_name);
			}
			if (!show_only) {
				int rc2 = wait4pid(spawn(argv));

				if (do_insert) {
					rc = rc2; /* only last module matters */
				}
				else if (!rc2) {
					rc = 0; /* success if remove any mod */
				}
			}
			if (ENABLE_FEATURE_CLEAN_UP) {
				/* the last value in the array has index == argc, but
				 * it is the terminating NULL, so we must not free it. */
				while (argc_malloc < argc) {
					free(argv[argc_malloc++]);
				}
			}
		}
		if (ENABLE_FEATURE_CLEAN_UP) {
			free(argv);
			argv = NULL;
		}
		list = do_insert ? list->m_prev : list->m_next;
	}
	return (show_only) ? 0 : rc;
}

/*
 * Check the matching between a pattern and a module name.
 * We need this as *_* is equivalent to *-*, even in pattern matching.
 */
static int check_pattern(const char* pat_src, const char* mod_src) {
	int ret;

	if (ENABLE_FEATURE_MODPROBE_FANCY_ALIAS) {
		char* pat;
		char* mod;
		char* p;

		pat = xstrdup (pat_src);
		mod = xstrdup (mod_src);

		for (p = pat; (p = strchr(p, '-')); *p++ = '_');
		for (p = mod; (p = strchr(p, '-')); *p++ = '_');

		ret = fnmatch(pat, mod, 0);

		if (ENABLE_FEATURE_CLEAN_UP) {
			free (pat);
			free (mod);
		}

		return ret;
	} else {
		return fnmatch(pat_src, mod_src, 0);
	}
}

/*
 * Builds the dependency list (aka stack) of a module.
 * head: the highest module in the stack (last to insmod, first to rmmod)
 * tail: the lowest module in the stack (first to insmod, last to rmmod)
 */
static void check_dep(char *mod, struct mod_list_t **head, struct mod_list_t **tail)
{
	struct mod_list_t *find;
	struct dep_t *dt;
	struct mod_opt_t *opt = 0;
	char *path = 0;

	/* Search for the given module name amongst all dependency rules.
	 * The module name in a dependency rule can be a shell pattern,
	 * so try to match the given module name against such a pattern.
	 * Of course if the name in the dependency rule is a plain string,
	 * then we consider it a pattern, and matching will still work. */
	for (dt = depend; dt; dt = dt->m_next) {
		if (check_pattern(dt->m_name, mod) == 0) {
			break;
		}
	}

	if (!dt) {
		bb_error_msg("module %s not found", mod);
		return;
	}

	// resolve alias names
	while (dt->m_isalias) {
		if (dt->m_depcnt == 1) {
			struct dep_t *adt;

			for (adt = depend; adt; adt = adt->m_next) {
				if (check_pattern(adt->m_name, dt->m_deparr[0]) == 0)
					break;
			}
			if (adt) {
				/* This is the module we are aliased to */
				struct mod_opt_t *opts = dt->m_options;
				/* Option of the alias are appended to the options of the module */
				while (opts) {
					adt->m_options = append_option(adt->m_options, opts->m_opt_val);
					opts = opts->m_next;
				}
				dt = adt;
			}
			else {
				bb_error_msg("module %s not found", mod);
				return;
			}
		}
		else {
			bb_error_msg("bad alias %s", dt->m_name);
			return;
		}
	}

	mod = dt->m_name;
	path = dt->m_path;
	opt = dt->m_options;

	// search for duplicates
	for (find = *head; find; find = find->m_next) {
		if (!strcmp(mod, find->m_name)) {
			// found ->dequeue it

			if (find->m_prev)
				find->m_prev->m_next = find->m_next;
			else
				*head = find->m_next;

			if (find->m_next)
				find->m_next->m_prev = find->m_prev;
			else
				*tail = find->m_prev;

			break; // there can be only one duplicate
		}
	}

	if (!find) { // did not find a duplicate
		find = xmalloc(sizeof(struct mod_list_t));
		find->m_name = mod;
		find->m_path = path;
		find->m_options = opt;
	}

	// enqueue at tail
	if (*tail)
		(*tail)->m_next = find;
	find->m_prev = *tail;
	find->m_next = 0;

	if (!*head)
		*head = find;
	*tail = find;

	if (dt) {
		int i;

		/* Add all dependable module for that new module */
		for (i = 0; i < dt->m_depcnt; i++)
			check_dep(dt->m_deparr[i], head, tail);
	}
}

static int mod_insert(char *mod, int argc, char **argv)
{
	struct mod_list_t *tail = 0;
	struct mod_list_t *head = 0;
	int rc;

	// get dep list for module mod
	check_dep(mod, &head, &tail);

	if (head && tail) {
		if (argc) {
			int i;
			// append module args
			for (i = 0; i < argc; i++)
				head->m_options = append_option(head->m_options, argv[i]);
		}

		// process tail ---> head
		if ((rc = mod_process(tail, 1)) != 0) {
			/*
			 * In case of using udev, multiple instances of modprobe can be
			 * spawned to load the same module (think of two same usb devices,
			 * for example; or cold-plugging at boot time). Thus we shouldn't
			 * fail if the module was loaded, and not by us.
			 */
			if (already_loaded(mod))
				rc = 0;
		}
	}
	else
		rc = 1;

	return rc;
}

static int mod_remove(char *mod)
{
	int rc;
	static struct mod_list_t rm_a_dummy = { "-a", NULL, NULL, NULL, NULL };

	struct mod_list_t *head = 0;
	struct mod_list_t *tail = 0;

	if (mod)
		check_dep(mod, &head, &tail);
	else  // autoclean
		head = tail = &rm_a_dummy;

	if (head && tail)
		rc = mod_process(head, 0);  // process head ---> tail
	else
		rc = 1;
	return rc;

}

int modprobe_main(int argc, char** argv)
{
	int rc = EXIT_SUCCESS;
	char *unused;

	opt_complementary = "?V-:q-v:v-q";
	main_opts = getopt32(argc, argv, "acdklnqrst:vVC:",
							&unused, &unused);
	if (main_opts & (DUMP_CONF_EXIT | LIST_ALL))
		return EXIT_SUCCESS;
	if (main_opts & (RESTRICT_DIR | CONFIG_FILE))
		bb_error_msg_and_die("-t and -C not supported");

	depend = build_dep();

	if (!depend)
		bb_error_msg_and_die("cannot parse modules.dep");

	if (remove_opt) {
		do {
			if (mod_remove(optind < argc ?
						argv[optind] : NULL)) {
				bb_error_msg("failed to remove module %s",
						argv[optind]);
				rc = EXIT_FAILURE;
			}
		} while (++optind < argc);
	} else {
		if (optind >= argc)
			bb_error_msg_and_die("no module or pattern provided");

		if (mod_insert(argv[optind], argc - optind - 1, argv + optind + 1))
			bb_error_msg_and_die("failed to load module %s", argv[optind]);
	}

	/* Here would be a good place to free up memory allocated during the dependencies build. */

	return rc;
}
