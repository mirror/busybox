/* vi: set sw=4 ts=4: */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include "busybox.h"
#ifdef BB_LOCALE_SUPPORT
#include <locale.h>
#endif

int been_there_done_that = 0; /* Also used in applets.c */
const char *applet_name;

#ifdef BB_FEATURE_INSTALLER
/* 
 * directory table
 *		this should be consistent w/ the enum, busybox.h::Location,
 *		or else...
 */
static char* install_dir[] = {
	"/",
	"/bin",
	"/sbin",
	"/usr/bin",
	"/usr/sbin",
};

/* abstract link() */
typedef int (*__link_f)(const char *, const char *);

/* 
 * Where in the filesystem is this busybox?
 * [return]
 *		malloc'd string w/ full pathname of busybox's location
 *		NULL on failure
 */
static char *busybox_fullpath()
{
	pid_t pid;
	char path[256];
	char proc[256];
	int len;

	pid = getpid();
	sprintf(proc, "/proc/%d/exe", pid);
	len = readlink(proc, path, 256);
	if (len != -1) {
		path[len] = 0;
	} else {
		perror_msg("%s", proc);
		return NULL;
	}
	return strdup(path);
}

/* create (sym)links for each applet */
static void install_links(const char *busybox, int use_symbolic_links)
{
	__link_f Link = link;

	char *fpc;
	int i;
	int rc;

	if (use_symbolic_links) 
		Link = symlink;

	for (i = 0; applets[i].name != NULL; i++) {
		fpc = concat_path_file(
			install_dir[applets[i].location], applets[i].name);
		rc = Link(busybox, fpc);
		if (rc!=0 && errno!=EEXIST) {
			perror_msg("%s", fpc);
		}
		free(fpc);
	}
}

#endif /* BB_FEATURE_INSTALLER */

int main(int argc, char **argv)
{
	const char *s;

	for (s = applet_name = argv[0]; *s != '\0';) {
		if (*s++ == '/')
			applet_name = s;
	}

#ifdef BB_SH
	/* Add in a special case hack -- whenever **argv == '-'
	 * (i.e. '-su' or '-sh') always invoke the shell */
	if (**argv == '-' && *(*argv+1)!= '-') {
		applet_name = "sh";
	}
#endif

#ifdef BB_LOCALE_SUPPORT
	if(getpid()!=1)	/* Do not set locale for `init' */
		setlocale(LC_ALL, "");
#endif

	run_applet_by_name(applet_name, argc, argv);
	error_msg_and_die("applet not found");
}


int busybox_main(int argc, char **argv)
{
	int col = 0, len, i;

#ifdef BB_FEATURE_INSTALLER	
	/* 
	 * This style of argument parsing doesn't scale well 
	 * in the event that busybox starts wanting more --options.
	 * If someone has a cleaner approach, by all means implement it.
	 */
	if (argc > 1 && (strcmp(argv[1], "--install") == 0)) {
		int use_symbolic_links = 0;
		int rc = 0;
		char *busybox;

		/* to use symlinks, or not to use symlinks... */
		if (argc > 2) {
			if ((strcmp(argv[2], "-s") == 0)) { 
				use_symbolic_links = 1; 
			}
		}

		/* link */
		busybox = busybox_fullpath();
		if (busybox) {
			install_links(busybox, use_symbolic_links);
			free(busybox);
		} else {
			rc = 1;
		}
		return rc;
	}
#endif /* BB_FEATURE_INSTALLER */

	argc--;

	/* If we've already been here once, exit now */
	if (been_there_done_that == 1 || argc < 1) {
		const struct BB_applet *a = applets;

		fprintf(stderr, "%s\n\n"
				"Usage: busybox [function] [arguments]...\n"
				"   or: [function] [arguments]...\n\n"
				"\tBusyBox is a multi-call binary that combines many common Unix\n"
				"\tutilities into a single executable.  Most people will create a\n"
				"\tlink to busybox for each function they wish to use, and BusyBox\n"
				"\twill act like whatever it was invoked as.\n" 
				"\nCurrently defined functions:\n", full_version);

		while (a->name != 0) {
			col +=
				fprintf(stderr, "%s%s", ((col == 0) ? "\t" : ", "),
						(a++)->name);
			if (col > 60 && a->name != 0) {
				fprintf(stderr, ",\n");
				col = 0;
			}
		}
		fprintf(stderr, "\n\n");
		exit(0);
	}

	/* Flag that we've been here already */
	been_there_done_that = 1;
	
	/* Move the command line down a notch */
	len = argv[argc] + strlen(argv[argc]) - argv[1];
	memmove(argv[0], argv[1], len);
	memset(argv[0] + len, 0, argv[1] - argv[0]);

	/* Fix up the argv pointers */
	len = argv[1] - argv[0];
	memmove(argv, argv + 1, sizeof(char *) * (argc + 1));
	for (i = 0; i < argc; i++)
		argv[i] -= len;

	return (main(argc, argv));
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
