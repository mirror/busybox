#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <search.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "busybox.h"


/* Stupid libc doesn't have a reliable way for use to know 
 * that libc5 is being used.   Assume this is good enough */ 
#if !defined __GLIBC__ || !defined __UCLIBC__
#error It looks like you are using libc5, which does not support
#error tfind().  tfind() is used by busybox dpkg.
#error Please disable BB_DPKG.  Sorry.
#endif	

#define DEPENDSMAX	64	/* maximum number of depends we can handle */

/* Should we do full dependency checking? */
#define DODEPENDS 1

/* Should we do debugging? */
#define DODEBUG 1

#ifdef DODEBUG
#define SYSTEM(x) do_system(x)
#define DPRINTF(fmt,args...) fprintf(stderr, fmt, ##args)
#else
#define SYSTEM(x) system(x)
#define DPRINTF(fmt,args...) /* nothing */
#endif

/* from dpkg-deb.c */
extern int deb_extract(int optflags, const char *dir_name, const char *deb_filename);
static const int dpkg_deb_contents = 1;
static const int dpkg_deb_control = 2;
//	const int dpkg_deb_info = 4;
static const int dpkg_deb_extract = 8;
static const int dpkg_deb_verbose_extract = 16;
static const int dpkg_deb_list = 32;

static const char statusfile[] = "/var/lib/dpkg/status.udeb";
static const char new_statusfile[] = "/var/lib/dpkg/status.udeb.new";
static const char bak_statusfile[] = "/var/lib/dpkg/status.udeb.bak";

static const char dpkgcidir[] = "/var/lib/dpkg/tmp.ci/";

static const char infodir[] = "/var/lib/dpkg/info/";
static const char udpkg_quiet[] = "UDPKG_QUIET";

//static const int status_wantstart	= 0;
//static const int status_wantunknown	= (1 << 0);
static const int status_wantinstall	= (1 << 1);
//static const int status_wanthold	= (1 << 2);
//static const int status_wantdeinstall	= (1 << 3);
//static const int status_wantpurge	= (1 << 4);
static const int status_wantmask	= 31;

//static const int status_flagstart	= 5;
static const int status_flagok	= (1 << 5);	/* 32 */
//static const int status_flagreinstreq	= (1 << 6); 
//static const int status_flaghold	= (1 << 7);
//static const int status_flagholdreinstreq	= (1 << 8);
static const int status_flagmask	= 480;

//static const int status_statusstart	= 9;
//static const int status_statusnoninstalled	= (1 << 9); /* 512 */
static const int status_statusunpacked	= (1 << 10);
static const int status_statushalfconfigured	= (1 << 11); 
static const int status_statusinstalled	= (1 << 12);
static const int status_statushalfinstalled	= (1 << 13);
//static const int status_statusconfigfiles	= (1 << 14);
//static const int status_statuspostinstfailed	= (1 << 15);
//static const int status_statusremovalfailed	= (1 << 16);
static const int status_statusmask =  130560; /* i assume status_statusinstalled is supposed to be included */

static const char *statuswords[][10] = {
	{ (char *) 0, "unknown", "install", "hold", "deinstall", "purge", 0 },
	{ (char *) 5, "ok", "reinstreq", "hold", "hold-reinstreq", 0 },
	{ (char *) 9, "not-installed", "unpacked", "half-configured",
		"installed", "half-installed", "config-files",
		"post-inst-failed", "removal-failed", 0 }
};

static const int color_white	= 0;
static const int color_grey	= 1;
static const int color_black	= 2;

/* data structures */
typedef struct package_s {
	char *file;
	char *package;
	char *version;
	char *depends;
	char *provides;
	char *description;
	int installer_menu_item;
	unsigned long status;
	char color; /* for topo-sort */
	struct package_s *requiredfor[DEPENDSMAX]; 
	unsigned short requiredcount;
	struct package_s *next;
} package_t;

#ifdef DODEBUG
static int do_system(const char *cmd)
{
	DPRINTF("cmd is %s\n", cmd);
	return system(cmd);
}
#else
#define do_system(cmd) system(cmd)
#endif

static int package_compare(const void *p1, const void *p2)
{
	return strcmp(((package_t *)p1)->package, 
		((package_t *)p2)->package);
}

static int remove_dpkgcidir()
{
	char *rm_dpkgcidir = NULL;

	rm_dpkgcidir = (char *) xmalloc(strlen(dpkgcidir) + 8);
	strcpy(rm_dpkgcidir, "rm -rf ");
	strcat(rm_dpkgcidir, dpkgcidir);

	if (SYSTEM(rm_dpkgcidir) != 0) {
		perror("mkdir ");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

#ifdef DODEPENDS
#include <ctype.h>

static char **depends_split(const char *dependsstr)
{
	static char *dependsvec[DEPENDSMAX];
	char *p;
	int i = 0;

	dependsvec[0] = 0;
	if (dependsstr == 0) {
		goto end;
	}

	p = strdup(dependsstr);
	while (*p != 0 && *p != '\n') {
		if (*p != ' ') {
			if (*p == ',') {
				*p = 0;
				dependsvec[++i] = 0;
			} else {
				if (dependsvec[i] == 0) {
					dependsvec[i] = p;
				}
			}
		} else {
			*p = 0; /* eat the space... */
		}
		p++;
	}
	*p = 0;

end:
	dependsvec[i+1] = 0;
	return dependsvec;
}

/* Topological sort algorithm:
 * ordered is the output list, pkgs is the dependency graph, pkg is 
 * the current node
 *
 * recursively add all the adjacent nodes to the ordered list, marking
 * each one as visited along the way
 *
 * yes, this algorithm looks a bit odd when all the params have the
 * same type :-)
 */
static void depends_sort_visit(package_t **ordered, package_t *pkgs,
		package_t *pkg)
{
	unsigned short i;

	/* mark node as processing */
	pkg->color = color_grey;

	/* visit each not-yet-visited node */
	for (i = 0; i < pkg->requiredcount; i++)
		if (pkg->requiredfor[i]->color == color_white)
			depends_sort_visit(ordered, pkgs, pkg->requiredfor[i]);

#if 0
	/* add it to the list */
	newnode = (struct package_t *)xmalloc(sizeof(struct package_t));
	/* make a shallow copy */
	*newnode = *pkg;
	newnode->next = *ordered;
	*ordered = newnode;
#endif

	pkg->next = *ordered;
	*ordered = pkg;

	/* mark node as done */
	pkg->color = color_black;
}

static package_t *depends_sort(package_t *pkgs)
{
	/* TODO: it needs to break cycles in the to-be-installed package 
	 * graph... */
	package_t *ordered = NULL;
	package_t *pkg;

	for (pkg = pkgs; pkg != 0; pkg = pkg->next) {
		pkg->color = color_white;
	}
	for (pkg = pkgs; pkg != 0; pkg = pkg->next) {
		if (pkg->color == color_white) {
			depends_sort_visit(&ordered, pkgs, pkg);
		}
	}

	/* Leaks the old list... return the new one... */
	return ordered;
}


/* resolve package dependencies -- 
 * for each package in the list of packages to be installed, we parse its 
 * dependency info to determine if the dependent packages are either 
 * already installed, or are scheduled to be installed. If both tests fail
 * than bail.
 *
 * The algorithm here is O(n^2*m) where n = number of packages to be 
 * installed and m is the # of dependencies per package. Not a terribly
 * efficient algorithm, but given that at any one time you are unlikely
 * to install a very large number of packages it doesn't really matter
 */
static package_t *depends_resolve(package_t *pkgs, void *status)
{
	package_t *pkg, *chk;
	package_t dependpkg;
	char **dependsvec;
	int i;
	void *found;

	for (pkg = pkgs; pkg != 0; pkg = pkg->next) {
		dependsvec = depends_split(pkg->depends);
		i = 0;
		while (dependsvec[i] != 0) {
			/* Check for dependencies; first look for installed packages */
			dependpkg.package = dependsvec[i];
			if ((found = tfind(&dependpkg, &status, package_compare)) == 0 ||
			    ((chk = *(package_t **)found) &&
			     (chk->status & (status_flagok | status_statusinstalled)) != 
			      (status_flagok | status_statusinstalled))) {

				/* if it fails, we look through the list of packages we are going to 
				 * install */
				for (chk = pkgs; chk != 0; chk = chk->next) {
					if (strcmp(chk->package, dependsvec[i]) == 0 || (chk->provides && 
					     strncmp(chk->provides, dependsvec[i], strlen(dependsvec[i])) == 0)) {
						if (chk->requiredcount >= DEPENDSMAX) {
							fprintf(stderr, "Too many dependencies for %s\n", chk->package);
							return 0;
						}
						if (chk != pkg) {
							chk->requiredfor[chk->requiredcount++] = pkg;
						}
						break;
					}
				}
				if (chk == 0) {
					fprintf(stderr, "%s depends on %s, but it is not going to be installed\n", pkg->package, dependsvec[i]);
					return 0;
				}
			}
			i++;
		}
	}

	return depends_sort(pkgs);
}
#endif

/* Status file handling routines
 * 
 * This is a fairly minimalistic implementation. there are two main functions 
 * that are supported:
 * 
 * 1) reading the entire status file:
 *    the status file is read into memory as a binary-tree, with just the 
 *    package and status info preserved
 *
 * 2) merging the status file
 *    control info from (new) packages is merged into the status file, 
 *    replacing any pre-existing entries. when a merge happens, status info 
 *    read using the status_read function is written back to the status file
 */
static unsigned long status_parse(const char *line)
{
	char *p;
	int i, j;
	unsigned long l = 0;

	for (i = 0; i < 3; i++) {
		if ((p = strchr(line, ' ')) != NULL) {
			*p = 0;
		}
		j = 1;
		while (statuswords[i][j] != 0) {
			if (strcmp(line, statuswords[i][j]) == 0) {
				l |= (1 << ((int)statuswords[i][0] + j - 1));
				break;
			}
			j++;
		}
		/* parse error */
		if (statuswords[i][j] == 0) {
			return 0;
		}
		line = p+1;
	}

	return l;
}

static const char *status_print(unsigned long flags)
{
	/* this function returns a static buffer... */
	static char buf[256];
	int i, j;

	buf[0] = 0;
	for (i = 0; i < 3; i++) {
		j = 1;
		while (statuswords[i][j] != 0) {
			if ((flags & (1 << ((int)statuswords[i][0] + j - 1))) != 0)	{
				strcat(buf, statuswords[i][j]);
				if (i < 2) strcat(buf, " ");
				break;
			}
			j++;
		}
		if (statuswords[i][j] == 0) {
			fprintf(stderr, "corrupted status flag!!\n");
			return NULL;
		}
	}

	return buf;
}

/*
 * Read a control file (or a stanza of a status file) and parse it,
 * filling parsed fields into the package structure
 */
static int control_read(FILE *file, package_t *p)
{
	char *line;

	while ((line = get_line_from_file(file)) != NULL) {
		line[strlen(line)] = 0;

		if (strlen(line) == 0) {
			break;
		} else
			if (strstr(line, "Package: ") == line) {
				p->package = strdup(line + 9);
		} else
			if (strstr(line, "Status: ") == line) {
				p->status = status_parse(line + 8);
		} else
			if (strstr(line, "Depends: ") == line) {
				p->depends = strdup(line + 9);
		} else
			if (strstr(line, "Provides: ") == line) {
				p->provides = strdup(line + 10);
		} else
			if (strstr(line, "Description: ") == line) {
				p->description = strdup(line + 13);
		/* This is specific to the Debian Installer. Ifdef? */
		} else
			if (strstr(line, "installer-menu-item: ") == line) {
				p->installer_menu_item = atoi(line + 21);
		}
		/* TODO: localized descriptions */
	}
	free(line);
	return EXIT_SUCCESS;
}

static void *status_read(void)
{
	FILE *f;
	void *status = 0;
	package_t *m = 0, *p = 0, *t = 0;

	if ((f = fopen(statusfile, "r")) == NULL) {
		perror(statusfile);
		return 0;
	}

	if (getenv(udpkg_quiet) == NULL) {
		printf("(Reading database...)\n");
	}

	while (!feof(f)) {
		m = (package_t *)xmalloc(sizeof(package_t));
		memset(m, 0, sizeof(package_t));
		control_read(f, m);
		if (m->package) {
			/*
			 * If there is an item in the tree by this name,
			 * it must be a virtual package; insert real
			 * package in preference.
			 */
			tdelete(m, &status, package_compare);
			tsearch(m, &status, package_compare);
			if (m->provides) {
				/* 
				 * A "Provides" triggers the insertion
				 * of a pseudo package into the status
				 * binary-tree.
				 */
				p = (package_t *)xmalloc(sizeof(package_t));
				memset(p, 0, sizeof(package_t));
				p->package = strdup(m->provides);

				t = *(package_t **)tsearch(p, &status, package_compare);
				if (t != p) {
					free(p->package);
					free(p);
				}
				else {
					/*
					 * Pseudo package status is the
					 * same as the status of the
					 * package providing it 
					 * FIXME: (not quite right, if 2
					 * packages of different statuses
					 * provide it).
					 */
					t->status = m->status;
				}
			}
		}
		else {
			free(m);
		}
	}
	fclose(f);
	return status;
}

static int status_merge(void *status, package_t *pkgs)
{
	FILE *fin, *fout;
	char *line;
	package_t *pkg = 0, *statpkg = 0;
	package_t locpkg;
	int r = 0;

	if ((fin = fopen(statusfile, "r")) == NULL) {
		perror(statusfile);
		return 0;
	}
	if ((fout = fopen(new_statusfile, "w")) == NULL) {
		perror(new_statusfile);
		return 0;
	}
	if (getenv(udpkg_quiet) == NULL) {
		printf("(Updating database...)\n");
	}

	while (((line = get_line_from_file(fin)) != NULL) && !feof(fin)) { 
		line[strlen(line)] = 0; /* trim newline */
		/* If we see a package header, find out if it's a package
		 * that we have processed. if so, we skip that block for
		 * now (write it at the end).
		 *
		 * we also look at packages in the status cache and update
		 * their status fields
		 */
		if (strstr(line, "Package: ") == line) {
			for (pkg = pkgs; pkg != 0 && strncmp(line + 9,
					pkg->package, strlen(line) - 9) != 0;
			     pkg = pkg->next) ;

			locpkg.package = line + 9;
			statpkg = tfind(&locpkg, &status, package_compare);
			
			/* note: statpkg should be non-zero, unless the status
			 * file was changed while we are processing (no locking
			 * is currently done...
			 */
			if (statpkg != 0) {
				statpkg = *(package_t **)statpkg;
			}
		}
		if (pkg != 0) {
			continue;
		}
		if (strstr(line, "Status: ") == line && statpkg != 0) {
			snprintf(line, sizeof(line), "Status: %s",
				status_print(statpkg->status));
		}
		fputs(line, fout);
		fputc('\n', fout);
	}
	free(line);

	// Print out packages we processed.
	for (pkg = pkgs; pkg != 0; pkg = pkg->next) {
		fprintf(fout, "Package: %s\nStatus: %s\n", 
			pkg->package, status_print(pkg->status));
		if (pkg->depends)
			fprintf(fout, "Depends: %s\n", pkg->depends);
		if (pkg->provides)
			fprintf(fout, "Provides: %s\n", pkg->provides);
		if (pkg->installer_menu_item)
			fprintf(fout, "installer-menu-item: %i\n", pkg->installer_menu_item);
		if (pkg->description)
			fprintf(fout, "Description: %s\n", pkg->description);
		fputc('\n', fout);
	}
	
	fclose(fin);
	fclose(fout);

	r = rename(statusfile, bak_statusfile);
	if (r == 0) {
		r = rename(new_statusfile, statusfile);
	}

	return 0;
}

static int is_file(const char *fn)
{
	struct stat statbuf;

	if (stat(fn, &statbuf) < 0) {
		return 0;
	}
	return S_ISREG(statbuf.st_mode);
}

static int dpkg_doconfigure(package_t *pkg)
{
	int r;
	char postinst[1024];
	char buf[1024];

	DPRINTF("Configuring %s\n", pkg->package);
	pkg->status &= status_statusmask;
	snprintf(postinst, sizeof(postinst), "%s%s.postinst", infodir, pkg->package);

	if (is_file(postinst)) {
		snprintf(buf, sizeof(buf), "%s configure", postinst);
		if ((r = do_system(buf)) != 0) {
			fprintf(stderr, "postinst exited with status %d\n", r);
			pkg->status |= status_statushalfconfigured;
			return 1;
		}
	}
	pkg->status |= status_statusinstalled;
	
	return 0;
}

static int dpkg_dounpack(package_t *pkg)
{
	int r = 0, i;
	char *cwd;
	FILE *outfp;
	char *src_file = NULL;
	char *dst_file = NULL;
	char *lst_file = NULL;
	char *adminscripts[] = { "/prerm", "/postrm", "/preinst", "/postinst",
			"/conffiles", "/md5sums", "/shlibs", "/templates" };
	char buf[1024], buf2[1024];

	DPRINTF("Unpacking %s\n", pkg->package);

	cwd = getcwd(0, 0);
	chdir("/");
	deb_extract(dpkg_deb_extract, "/", pkg->file);

	/* Installs the package scripts into the info directory */
	for (i = 0; i < sizeof(adminscripts) / sizeof(adminscripts[0]); i++) {
		/* The full path of the current location of the admin file */
		src_file = xrealloc(src_file, strlen(dpkgcidir) + strlen(pkg->package) + strlen(adminscripts[i]) + 1);
		strcpy(src_file, dpkgcidir);
		strcat(src_file, pkg->package);
		strcat(src_file, adminscripts[i]);

		/* the full path of where we want the file to be copied to */
		dst_file = xrealloc(dst_file, strlen(infodir) + strlen(pkg->package) + strlen(adminscripts[i]) + 1);
		strcpy(dst_file, infodir);
		strcat(dst_file, pkg->package);
		strcat(dst_file, adminscripts[i]);

		/* copy admin file to permanent home */
		if (copy_file(src_file, dst_file, TRUE, FALSE, FALSE) < 0) {
			error_msg_and_die("Cannot copy %s to %s ", buf, buf2);
		}

		/* create the list file */
		lst_file = (char *) xmalloc(strlen(infodir) + strlen(pkg->package) + 6);
		strcpy(lst_file, infodir);
		strcat(lst_file, pkg->package);
		strcat(lst_file, ".list");
		outfp = freopen(lst_file, "w", stdout);
		deb_extract(dpkg_deb_list, NULL, pkg->file);
		stdout = freopen(NULL, "w", outfp);

		printf("done\n");
		getchar();

		fclose(outfp);
	}

	pkg->status &= status_wantmask;
	pkg->status |= status_wantinstall;
	pkg->status &= status_flagmask;
	pkg->status |= status_flagok;
	pkg->status &= status_statusmask;

	if (r == 0) {
		pkg->status |= status_statusunpacked;
	} else {
		pkg->status |= status_statushalfinstalled;
	}
	chdir(cwd);
	return r;
}

/*
 * Extract and parse the control.tar.gz from the specified package
 */
static int dpkg_unpackcontrol(package_t *pkg)
{
	char *tmp_name;
	FILE *file;
	int length;

	/* clean the temp directory (dpkgcidir) be recreating it */
	remove_dpkgcidir();
	if (mkdir(dpkgcidir, S_IRWXU) != 0) {
		perror("mkdir");
		return EXIT_FAILURE;
	}

	/*
	 * Get the package name from the file name,
	 * first remove the directories
	 */
	if ((tmp_name = strrchr(pkg->file, '/')) == NULL) {
		tmp_name = pkg->file;
	} else {
		tmp_name++;
	}
	/* now remove trailing version numbers etc */
	length = strcspn(tmp_name, "_.");
	pkg->package = (char *) xmalloc(length + 1);
	/* store the package name */
	strncpy(pkg->package, tmp_name, length);

	/* work out the full extraction path */
	tmp_name = (char *) xmalloc(strlen(dpkgcidir) + strlen(pkg->package) + 9);
	memset(tmp_name, 0, strlen(dpkgcidir) + strlen(pkg->package) + 9);
	strcpy(tmp_name, dpkgcidir);
	strcat(tmp_name, pkg->package);

	/* extract control.tar.gz to the full extraction path */
	deb_extract(dpkg_deb_control, tmp_name, pkg->file);

	/* parse the extracted control file */
	strcat(tmp_name, "/control");
	if ((file = fopen(tmp_name, "r")) == NULL) {
		return EXIT_FAILURE;
	}
	if (control_read(file, pkg) == EXIT_FAILURE) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int dpkg_unpack(package_t *pkgs, void *status)
{
	int r = 0;
	package_t *pkg;

	for (pkg = pkgs; pkg != 0; pkg = pkg->next) {
		if (dpkg_unpackcontrol(pkg) == EXIT_FAILURE) {
			return EXIT_FAILURE;
		}
		if ((r = dpkg_dounpack(pkg)) != 0 ) {
			break;
		}
	}
	status_merge(status, pkgs);
	remove_dpkgcidir();

	return r;
}

static int dpkg_configure(package_t *pkgs, void *status)
{
	int r = 0;
	void *found;
	package_t *pkg;

	for (pkg = pkgs; pkg != 0 && r == 0; pkg = pkg->next) {
		found = tfind(pkg, &status, package_compare);

		if (found == 0) {
			fprintf(stderr, "Trying to configure %s, but it is not installed\n", pkg->package);
			r = 1;
		} 
		/* configure the package listed in the status file;
		 * not pkg, as we have info only for the latter
		 */
		else {
			r = dpkg_doconfigure(*(package_t **)found);
		}
	}
	status_merge(status, 0);

	return r;
}

static int dpkg_install(package_t *pkgs, void *status)
{
	package_t *p, *ordered = 0;

	/* Stage 1: parse all the control information */
	for (p = pkgs; p != 0; p = p->next) {
		if (dpkg_unpackcontrol(p) == EXIT_FAILURE) {
			perror(p->file);
			return EXIT_FAILURE;
		}
	}

	/* Stage 2: resolve dependencies */
#ifdef DODEPENDS
	ordered = depends_resolve(pkgs, status);
#else
	ordered = pkgs;
#endif
	
	/* Stage 3: install */
	for (p = ordered; p != 0; p = p->next) {
		p->status &= status_wantmask;
		p->status |= status_wantinstall;

		/* for now the flag is always set to ok... this is probably
		 * not what we want
		 */
		p->status &= status_flagmask;
		p->status |= status_flagok;

		DPRINTF("Installing %s\n", p->package);
		if (dpkg_dounpack(p) != 0) {
			perror(p->file);
		}
		if (dpkg_doconfigure(p) != 0) {
			perror(p->file);
		}
	}
	
	if (ordered != 0) {
		status_merge(status, pkgs);
	}
	remove_dpkgcidir();

	return 0;
}

static int dpkg_remove(package_t *pkgs, void *status)
{
	package_t *p;

	for (p = pkgs; p != 0; p = p->next)
	{
	}
	status_merge(status, 0);

	return 0;
}

extern int dpkg_main(int argc, char **argv)
{
	char opt = 0;
	char *s;
	package_t *p, *packages = NULL;
	char *cwd = getcwd(0, 0);
	void *status = NULL;

	while (*++argv) {
		if (**argv == '-') {
			/* Nasty little hack to "parse" long options. */
			s = *argv;
			while (*s == '-') {
				s++;
			}
			opt=s[0];
		} else {
			p = (package_t *)xmalloc(sizeof(package_t));
			memset(p, 0, sizeof(package_t));

			if (**argv == '/') {
				p->file = *argv;
			} else
				if (opt != 'c') {
					p->file = xmalloc(strlen(cwd) + strlen(*argv) + 2);
					sprintf(p->file, "%s/%s", cwd, *argv);
			} else {
				p->package = strdup(*argv);
			}

			p->next = packages;
			packages = p;
		}		
	}

	status = status_read();

	switch (opt) {
		case 'i':
			return dpkg_install(packages, status);
		case 'r':
			return dpkg_remove(packages, status);
		case 'u':
			return dpkg_unpack(packages, status);
		case 'c':
			return dpkg_configure(packages, status);
		default :
			show_usage();
			return EXIT_FAILURE;
	}
}
