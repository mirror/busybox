/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) tons of folks.  Tracking down who wrote what
 * isn't something I'm going to worry about...  If you wrote something
 * here, please feel free to acknowledge your work.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Based in part on code from sash, Copyright (c) 1999 by David I. Bell
 * Permission has been granted to redistribute this code under the GPL.
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "busybox.h"

#undef APPLET
#undef APPLET_NOUSAGE
#undef PROTOTYPES
#include "applets.h"

static struct BB_applet *applet_using;

/* The -1 arises because of the {0,NULL,0,-1} entry above. */
const size_t NUM_APPLETS = (sizeof (applets) / sizeof (struct BB_applet) - 1);


#ifdef CONFIG_FEATURE_SUID

static void check_suid (struct BB_applet *app);

#ifdef CONFIG_FEATURE_SUID_CONFIG

#include <sys/stat.h>
#include <ctype.h>
#include "pwd_.h"
#include "grp_.h"

static void parse_config_file (void);

#define CONFIG_FILE "/etc/busybox.conf"

/* applets [] is const, so we have to define this "override" structure */
struct BB_suid_config
{
  struct BB_applet *m_applet;

  uid_t m_uid;
  gid_t m_gid;
  mode_t m_mode;

  struct BB_suid_config *m_next;
};

static struct BB_suid_config *suid_config;
static int suid_cfg_readable;

#endif /* CONFIG_FEATURE_SUID_CONFIG */

#endif /* CONFIG_FEATURE_SUID */



extern void
bb_show_usage (void)
{
  const char *format_string;
  const char *usage_string = usage_messages;
  int i;

  for (i = applet_using - applets; i > 0;) {
	if (!*usage_string++) {
	  --i;
	}
  }

  format_string = "%s\n\nUsage: %s %s\n\n";
  if (*usage_string == '\b')
	format_string = "%s\n\nNo help available.\n\n";
  fprintf (stderr, format_string, bb_msg_full_version, applet_using->name,
		   usage_string);

  exit (EXIT_FAILURE);
}

static int
applet_name_compare (const void *x, const void *y)
{
  const char *name = x;
  const struct BB_applet *applet = y;

  return strcmp (name, applet->name);
}

extern const size_t NUM_APPLETS;

struct BB_applet *
find_applet_by_name (const char *name)
{
  return bsearch (name, applets, NUM_APPLETS, sizeof (struct BB_applet),
				  applet_name_compare);
}

void
run_applet_by_name (const char *name, int argc, char **argv)
{
  static int recurse_level = 0;
  extern int been_there_done_that;      /* From busybox.c */

#ifdef CONFIG_FEATURE_SUID_CONFIG
  if (recurse_level == 0)
	parse_config_file ();
#endif

  recurse_level++;
  /* Do a binary search to find the applet entry given the name. */
  if ((applet_using = find_applet_by_name (name)) != NULL) {
	bb_applet_name = applet_using->name;
	if (argv[1] && strcmp (argv[1], "--help") == 0) {
	  if (strcmp (applet_using->name, "busybox") == 0) {
		if (argv[2])
		  applet_using = find_applet_by_name (argv[2]);
		else
		  applet_using = NULL;
	  }
	  if (applet_using)
		bb_show_usage ();
	  been_there_done_that = 1;
	  busybox_main (0, NULL);
	}
#ifdef CONFIG_FEATURE_SUID
	check_suid (applet_using);
#endif

	exit ((*(applet_using->main)) (argc, argv));
  }
  /* Just in case they have renamed busybox - Check argv[1] */
  if (recurse_level == 1) {
	run_applet_by_name ("busybox", argc, argv);
  }
  recurse_level--;
}


#ifdef CONFIG_FEATURE_SUID

#ifdef CONFIG_FEATURE_SUID_CONFIG

/* check if u is member of group g */
static int
ingroup (uid_t u, gid_t g)
{
  struct group *grp = getgrgid (g);

  if (grp) {
	char **mem;

	for (mem = grp->gr_mem; *mem; mem++) {
	  struct passwd *pwd = getpwnam (*mem);

	  if (pwd && (pwd->pw_uid == u))
		return 1;
	}
  }
  return 0;
}

#endif


void
check_suid (struct BB_applet *applet)
{
  uid_t ruid = getuid ();               /* real [ug]id */
  uid_t rgid = getgid ();

#ifdef CONFIG_FEATURE_SUID_CONFIG
  if (suid_cfg_readable) {
	struct BB_suid_config *sct;

	for (sct = suid_config; sct; sct = sct->m_next) {
	  if (sct->m_applet == applet)
		break;
	}
	if (sct) {
	  mode_t m = sct->m_mode;

	  if (sct->m_uid == ruid)       /* same uid */
		m >>= 6;
	  else if ((sct->m_gid == rgid) || ingroup (ruid, sct->m_gid))  /* same group / in group */
		m >>= 3;

	  if (!(m & S_IXOTH))           /* is x bit not set ? */
		bb_error_msg_and_die ("You have no permission to run this applet!");

	  if ((sct->m_mode & (S_ISGID | S_IXGRP)) == (S_ISGID | S_IXGRP)) {     /* *both* have to be set for sgid */
		if (setegid (sct->m_gid))
		  bb_error_msg_and_die
			("BusyBox binary has insufficient rights to set proper GID for applet!");
	  } else
		setgid (rgid);                  /* no sgid -> drop */

	  if (sct->m_mode & S_ISUID) {
		if (seteuid (sct->m_uid))
		  bb_error_msg_and_die
			("BusyBox binary has insufficient rights to set proper UID for applet!");
	  } else
		setuid (ruid);                  /* no suid -> drop */
	} else {
		/* default: drop all priviledges */
	  setgid (rgid);
	  setuid (ruid);
	}
	return;
  } else {
#ifndef CONFIG_FEATURE_SUID_CONFIG_QUIET
	static int onetime = 0;

	if (!onetime) {
	  onetime = 1;
	  fprintf (stderr, "Using fallback suid method\n");
	}
#endif
  }
#endif

  if (applet->need_suid == _BB_SUID_ALWAYS) {
	if (geteuid () != 0)
	  bb_error_msg_and_die ("This applet requires root priviledges!");
  } else if (applet->need_suid == _BB_SUID_NEVER) {
	setgid (rgid);                          /* drop all priviledges */
	setuid (ruid);
  }
}

#ifdef CONFIG_FEATURE_SUID_CONFIG

/* This should probably be a libbb routine.  In that case,
 * I'd probably rename it to something like bb_trimmed_slice.
 */
static char *get_trimmed_slice(char *s, char *e)
{
	/* First, consider the value at e to be nul and back up until we
	 * reach a non-space char.  Set the char after that (possibly at
	 * the original e) to nul. */
	while (e-- > s) {
		if (!isspace(*e)) {
			break;
		}
	}
	e[1] = 0;

	/* Next, advance past all leading space and return a ptr to the
	 * first non-space char; possibly the terminating nul. */
	return (char *) bb_skip_whitespace(s);
}


#define parse_error(x)  { err=x; goto pe_label; }

/* Don't depend on the tools to combine strings. */
static const char config_file[] = CONFIG_FILE;

/* There are 4 chars + 1 nul for each of user/group/other. */
static const char mode_chars[] = "Ssx-\0Ssx-\0Ttx-";

/* We don't supply a value for the nul, so an index adjustment is
 * necessary below.  Also, we use unsigned short here to save some
 * space even though these are really mode_t values. */
static const unsigned short mode_mask[] = {
	/*  SST         sst                 xxx   --- */
	S_ISUID,    S_ISUID|S_IXUSR,    S_IXUSR,    0,	/* user */
	S_ISGID,    S_ISGID|S_IXGRP,    S_IXGRP,    0,	/* group */
	0,          S_IXOTH,            S_IXOTH,    0	/* other */
};

static void parse_config_file(void)
{
	struct BB_suid_config *sct_head;
	struct BB_suid_config *sct;
	struct BB_applet *applet;
	FILE *f;
	char *err;
	char *s;
	char *e;
	int i, lc, section;
	char buffer[256];
	struct stat st;

	assert(!suid_config);		/* Should be set to NULL by bss init. */

	if ((stat(config_file, &st) != 0)			/* No config file? */
		|| !S_ISREG(st.st_mode)					/* Not a regular file? */
		|| (st.st_uid != 0)						/* Not owned by root? */
		|| (st.st_mode & (S_IWGRP | S_IWOTH))	/* Writable by non-root? */
		|| !(f = fopen(config_file, "r"))		/* Can not open? */
		) {
		return;
	}

	suid_cfg_readable = 1;
	sct_head = NULL;
	section = lc = 0;

	do {
		s = buffer;

		if (!fgets(s, sizeof(buffer), f)) { /* Are we done? */
			if (ferror(f)) {   /* Make sure it wasn't a read error. */
				parse_error("reading");
			}
			fclose(f);
			suid_config = sct_head;	/* Success, so set the pointer. */
			return;
		}

		lc++;					/* Got a (partial) line. */

		/* If a line is too long for our buffer, we consider it an error.
		 * The following test does mistreat one corner case though.
		 * If the final line of the file does not end with a newline and
		 * yet exactly fills the buffer, it will be treated as too long
		 * even though there isn't really a problem.  But it isn't really
		 * worth adding code to deal with such an unlikely situation, and
		 * we do err on the side of caution.  Besides, the line would be
		 * too long if it did end with a newline. */
		if (!strchr(s, '\n') && !feof(f)) {
			parse_error("line too long");
		}

		/* Trim leading and trailing whitespace, ignoring comments, and
		 * check if the resulting string is empty. */
		if (!*(s = get_trimmed_slice(s, strchrnul(s, '#')))) {
			continue;
		}

		/* Check for a section header. */

		if (*s == '[') {
			/* Unlike the old code, we ignore leading and trailing
			 * whitespace for the section name.  We also require that
			 * there are no stray characters after the closing bracket. */
			if (!(e = strchr(s, ']'))	/* Missing right bracket? */
				|| e[1]					/* Trailing characters? */
				|| !*(s = get_trimmed_slice(s+1, e)) /* Missing name? */
				) {
				parse_error("section header");
			}
			/* Right now we only have one section so just check it.
			 * If more sections are added in the future, please don't
			 * resort to cascading ifs with multiple strcasecmp calls.
			 * That kind of bloated code is all too common.  A loop
			 * and a string table would be a better choice unless the
			 * number of sections is very small. */
			if (strcasecmp(s, "SUID") == 0) {
				section = 1;
				continue;
			}
			section = -1;	/* Unknown section so set to skip. */
			continue;
		}

		/* Process sections. */

		if (section == 1) {		/* SUID */
			/* Since we trimmed leading and trailing space above, we're
			 * now looking for strings of the form
			 *    <key>[::space::]*=[::space::]*<value>
			 * where both key and value could contain inner whitespace. */

			/* First get the key (an applet name in our case). */
			if (!!(e = strchr(s, '='))) {
				s = get_trimmed_slice(s, e);
			}
			if (!e || !*s) {	/* Missing '=' or empty key. */
				parse_error("keyword");
			}

			/* Ok, we have an applet name.  Process the rhs if this
			 * applet is currently built in and ignore it otherwise.
			 * Note: This can hide config file bugs which only pop
			 * up when the busybox configuration is changed. */
			if ((applet = find_applet_by_name(s))) {
				/* Note: We currently don't check for duplicates!
				 * The last config line for each applet will be the
				 * one used since we insert at the head of the list.
				 * I suppose this could be considered a feature. */
				sct = xmalloc(sizeof(struct BB_suid_config));
				sct->m_applet = applet;
				sct->m_mode = 0;
				sct->m_next = sct_head;
				sct_head = sct;

				/* Get the specified mode. */

				e = (char *) bb_skip_whitespace(e+1);

				for (i=0 ; i < 3 ; i++) {
					const char *q;
					if (!*(q = strchrnul(mode_chars + 5*i, *e++))) {
						parse_error("mode");
					}
					/* Adjust by -i to account for nul. */
					sct->m_mode |= mode_mask[(q - mode_chars) - i];
				}

				/* Now get the the user/group info. */
		 
				s = (char *) bb_skip_whitespace(e);

				/* Note: We require whitespace between the mode and the
				 * user/group info. */
				if ((s == e) || !(e = strchr(s, '.'))) {
					parse_error("<uid>.<gid>");
				}
				*e++ = 0;

				/* We can't use get_ug_id here since it would exit()
				 * if a uid or gid was not found.  Oh well... */
				{
					char *e2;

					sct->m_uid = strtoul(s, &e2, 10);
					if (*e2 || (s == e2)) {
						struct passwd *pwd;
						if (!(pwd = getpwnam(s))) {
							parse_error("user");
						}
						sct->m_uid = pwd->pw_uid;
					}

					sct->m_gid = strtoul(e, &e2, 10);
					if (*e2 || (e == e2)) {
						struct group *grp;
						if (!(grp = getgrnam(e))) {
							parse_error("group");
						}
						sct->m_gid = grp->gr_gid;
					}
				}
			}
			continue;
		}

		/* Unknown sections are ignored. */

		/* Encountering configuration lines prior to seeing a
		 * section header is treated as an error.  This is how
		 * the old code worked, but it may not be desireable.
		 * We may want to simply ignore such lines in case they
		 * are used in some future version of busybox. */
		if (!section) {
			parse_error("keyword outside section");
		}

	} while (1);

 pe_label:
	fprintf(stderr, "Parse error in %s, line %d: %s\n",
			config_file, lc, err);

	fclose(f);
	/* Release any allocated memory before returning. */
	while (sct_head) {
		sct = sct_head->m_next;
		free(sct_head);
		sct_head = sct;
	}
	return;
}

#endif

#endif

/* END CODE */
/*
  Local Variables:
  c-file-style: "linux"
  c-basic-offset: 4
  tab-width: 4
End:
*/
