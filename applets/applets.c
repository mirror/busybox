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
#include "busybox.h"

#undef APPLET
#undef APPLET_NOUSAGE
#undef PROTOTYPES
#include "applets.h"

struct BB_applet *applet_using;

/* The -1 arises because of the {0,NULL,0,-1} entry above. */
const size_t NUM_APPLETS = (sizeof (applets) / sizeof (struct BB_applet) - 1);


#ifdef CONFIG_FEATURE_SUID

static void check_suid (struct BB_applet *app);

#ifdef CONFIG_FEATURE_SUID_CONFIG

#include <sys/stat.h>
#include <ctype.h>
#include "pwd_.h"
#include "grp_.h"

static int parse_config_file (void);

static int config_ok;

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
	config_ok = parse_config_file ();
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
  if (config_ok) {
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


#define parse_error(x)  { err=x; goto pe_label; }


int
parse_config_file (void)
{
  struct stat st;
  char *err = 0;
  FILE *f = 0;
  int lc = 0;

  suid_config = 0;

  /* is there a config file ? */
  if (stat (CONFIG_FILE, &st) == 0) {
	/* is it owned by root with no write perm. for group and others ? */
	if (S_ISREG (st.st_mode) && (st.st_uid == 0)
		&& (!(st.st_mode & (S_IWGRP | S_IWOTH)))) {
	  /* that's ok .. then try to open it */
	  f = fopen (CONFIG_FILE, "r");

	  if (f) {
		char buffer[256];
		int section = 0;

		while (fgets (buffer, sizeof (buffer) - 1, f)) {
		  char c = buffer[0];
		  char *p;

		  lc++;

		  p = strchr (buffer, '#');
		  if (p)
			*p = 0;
		  p = buffer + bb_strlen (buffer);
		  while ((p > buffer) && isspace (*--p))
			*p = 0;

		  if (p == buffer)
			continue;

		  if (c == '[') {
			p = strchr (buffer, ']');

			if (!p || (p == (buffer + 1)))  /* no matching ] or empty [] */
			  parse_error ("malformed section header");

			*p = 0;

			if (strcasecmp (buffer + 1, "SUID") == 0)
			  section = 1;
			else
			  section = -1;         /* unknown section - just skip */
		  } else if (section) {
			switch (section) {
			case 1:{                        /* SUID */
				int l;
				struct BB_applet *applet;

				p = strchr (buffer, '=');       /* <key>[::space::]*=[::space::]*<value> */

				if (!p || (p == (buffer + 1)))  /* no = or key is empty */
				  parse_error ("malformed keyword");

				l = p - buffer;
				while (isspace (buffer[--l])) {
					/* skip whitespace */
				}

				buffer[l + 1] = 0;

				if ((applet = find_applet_by_name (buffer))) {
				  struct BB_suid_config *sct =
					xmalloc (sizeof (struct BB_suid_config));

				  sct->m_applet = applet;
				  sct->m_next = suid_config;
				  suid_config = sct;

				  while (isspace (*++p)) {
					/* skip whitespace */
				  }

				  sct->m_mode = 0;

				  switch (*p++) {
				  case 'S':
					sct->m_mode |= S_ISUID;
					break;
				  case 's':
					sct->m_mode |= S_ISUID;
					/* no break */
				  case 'x':
					sct->m_mode |= S_IXUSR;
					break;
				  case '-':
					break;
				  default:
					parse_error ("invalid user mode");
				  }

				  switch (*p++) {
				  case 's':
					sct->m_mode |= S_ISGID;
					/* no break */
				  case 'x':
					sct->m_mode |= S_IXGRP;
					break;
				  case 'S':
					break;
				  case '-':
					break;
				  default:
					parse_error ("invalid group mode");
				  }

				  switch (*p) {
				  case 't':
				  case 'x':
					sct->m_mode |= S_IXOTH;
					break;
				  case 'T':
				  case '-':
					break;
				  default:
					parse_error ("invalid other mode");
				  }

				  while (isspace (*++p)) {
					/* skip whitespace */
				  }

				  if (isdigit (*p)) {
					sct->m_uid = strtol (p, &p, 10);
					if (*p++ != '.')
					  parse_error ("parsing <uid>.<gid>");
				  } else {
					struct passwd *pwd;
					char *p2 = strchr (p, '.');

					if (!p2)
					  parse_error ("parsing <uid>.<gid>");

					*p2 = 0;
					pwd = getpwnam (p);

					if (!pwd)
					  parse_error ("invalid user name");

					sct->m_uid = pwd->pw_uid;
					p = p2 + 1;
				  }
				  if (isdigit (*p))
					sct->m_gid = strtol (p, &p, 10);
				  else {
					struct group *grp = getgrnam (p);

					if (!grp)
					  parse_error ("invalid group name");

					sct->m_gid = grp->gr_gid;
				  }
				}
				break;
			  }
			default:                        /* unknown - skip */
			  break;
			}
		  } else
			parse_error ("keyword not within section");
		}
		fclose (f);
		return 1;
	  }
	}
  }
  return 0;     /* no config file or not readable (not an error) */

pe_label:
  fprintf (stderr, "Parse error in %s, line %d: %s\n", CONFIG_FILE, lc, err);

  if (f)
	fclose (f);
  return 0;
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
