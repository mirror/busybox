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

//#define PACKAGE "udpkg"
//#define VERSION "0.1"

/*
 * Should we do full dependency checking?
 */
#define DODEPENDS 1

/*
 * Should we do debugging?
 */
#define DODEBUG 0

#ifdef DODEBUG
#include <assert.h>
#define ASSERT(x) assert(x)
#define SYSTEM(x) do_system(x)
#define DPRINTF(fmt,args...) fprintf(stderr, fmt, ##args)
#else
#define ASSERT(x) /* nothing */
#define SYSTEM(x) system(x)
#define DPRINTF(fmt,args...) /* nothing */
#endif

#define BUFSIZE		4096
#define ADMINDIR "/var/lib/dpkg"
#define STATUSFILE	ADMINDIR ## "/status"
#define DPKGCIDIR	ADMINDIR ## "/tmp.ci/"
#define INFODIR		ADMINDIR ## "/info/"
#define UDPKG_QUIET	"UDPKG_QUIET"
#define DEPENDSMAX	64	/* maximum number of depends we can handle */

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

const int color_white	= 0;
const int color_grey	= 1;
const int color_black	= 2;

/* data structures */
struct package_t {
	char *file;
	char *package;
	char *version;
	char *depends;
	char *provides;
	char *description;
	int installer_menu_item;
	unsigned long status;
	char color; /* for topo-sort */
	struct package_t *requiredfor[DEPENDSMAX]; 
	unsigned short requiredcount;
	struct package_t *next;
};

/* function prototypes */
void *status_read(void);
void control_read(FILE *f, struct package_t *p);
int status_merge(void *status, struct package_t *pkgs);
int package_compare(const void *p1, const void *p2);
struct package_t *depends_resolve(struct package_t *pkgs, void *status);

#ifdef DODEPENDS
#include <ctype.h>

static char **depends_split(const char *dependsstr)
{
	static char *dependsvec[DEPENDSMAX];
	char *p;
	int i = 0;

	dependsvec[0] = 0;

	if (dependsstr != 0)
	{
		p = strdup(dependsstr);
		while (*p != 0 && *p != '\n')
		{
			if (*p != ' ')
			{
				if (*p == ',')
				{
					*p = 0;
					dependsvec[++i] = 0;
				}
				else if (dependsvec[i] == 0)
					dependsvec[i] = p;
			}
			else
				*p = 0; /* eat the space... */
			p++;
		}
		*p = 0;
	}
	dependsvec[i+1] = 0;
	return dependsvec;
}

static void depends_sort_visit(struct package_t **ordered, 
	struct package_t *pkgs, struct package_t *pkg)
{
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
	unsigned short i;

	/* mark node as processing */
	pkg->color = color_grey;

	/* visit each not-yet-visited node */
	for (i = 0; i < pkg->requiredcount; i++)
		if (pkg->requiredfor[i]->color == color_white)
			depends_sort_visit(ordered, pkgs, pkg->requiredfor[i]);

#if 0
	/* add it to the list */
	newnode = (struct package_t *)malloc(sizeof(struct package_t));
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

static struct package_t *depends_sort(struct package_t *pkgs)
{
	/* TODO: it needs to break cycles in the to-be-installed package 
	 * graph... */
	struct package_t *ordered = NULL;
	struct package_t *pkg;

	for (pkg = pkgs; pkg != 0; pkg = pkg->next)
		pkg->color = color_white;

	for (pkg = pkgs; pkg != 0; pkg = pkg->next)
		if (pkg->color == color_white)
			depends_sort_visit(&ordered, pkgs, pkg);

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
struct package_t *depends_resolve(struct package_t *pkgs, void *status)
{
	struct package_t *pkg, *chk;
	struct package_t dependpkg;
	char **dependsvec;
	int i;
	void *found;

	for (pkg = pkgs; pkg != 0; pkg = pkg->next)
	{
		dependsvec = depends_split(pkg->depends);
		i = 0;
		while (dependsvec[i] != 0)
		{
			/* Check for dependencies; first look for installed packages */
			dependpkg.package = dependsvec[i];
			if ((found = tfind(&dependpkg, &status, package_compare)) == 0 ||
			    ((chk = *(struct package_t **)found) &&
			     (chk->status & (status_flagok | status_statusinstalled)) != 
			      (status_flagok | status_statusinstalled)))
			{
				/* if it fails, we look through the list of packages we are going to 
				 * install */
				for (chk = pkgs; chk != 0; chk = chk->next)
				{
					if (strcmp(chk->package, dependsvec[i]) == 0 ||
					    (chk->provides && 
					     strncmp(chk->provides, dependsvec[i], strlen(dependsvec[i])) == 0))
					{
						if (chk->requiredcount >= DEPENDSMAX)
						{
							fprintf(stderr, "Too many dependencies for %s\n", 
								chk->package);
							return 0;
						}
						if (chk != pkg)
							chk->requiredfor[chk->requiredcount++] = pkg;
						break;
					}
				}
				if (chk == 0)
				{
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

int package_compare(const void *p1, const void *p2)
{
	return strcmp(((struct package_t *)p1)->package, 
		((struct package_t *)p2)->package);
}

static unsigned long status_parse(const char *line)
{
	char *p;
	int i, j;
	unsigned long l = 0;
	for (i = 0; i < 3; i++)
	{
		p = strchr(line, ' ');
		if (p) *p = 0; 
		j = 1;
		while (statuswords[i][j] != 0) 
		{
			if (strcmp(line, statuswords[i][j]) == 0) 
			{
				l |= (1 << ((int)statuswords[i][0] + j - 1));
				break;
			}
			j++;
		}
		if (statuswords[i][j] == 0) return 0; /* parse error */
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
	for (i = 0; i < 3; i++)
	{
		j = 1;
		while (statuswords[i][j] != 0)
		{
			if ((flags & (1 << ((int)statuswords[i][0] + j - 1))) != 0)
			{
				strcat(buf, statuswords[i][j]);
				if (i < 2) strcat(buf, " ");
				break;
			}
			j++;
		}
		if (statuswords[i][j] == 0)
		{
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
void control_read(FILE *f, struct package_t *p)
{
	char buf[BUFSIZE];
	while (fgets(buf, BUFSIZE, f) && !feof(f))
	{
		buf[strlen(buf)-1] = 0;
		if (*buf == 0)
			return;
		else if (strstr(buf, "Package: ") == buf)
		{
			p->package = strdup(buf+9);
		}
		else if (strstr(buf, "Status: ") == buf)
		{
			p->status = status_parse(buf+8);
		}
		else if (strstr(buf, "Depends: ") == buf)
		{
			p->depends = strdup(buf+9);
		}
		else if (strstr(buf, "Provides: ") == buf)
		{
			p->provides = strdup(buf+10);
		}
		/* This is specific to the Debian Installer. Ifdef? */
		else if (strstr(buf, "installer-menu-item: ") == buf) 
		{
			p->installer_menu_item = atoi(buf+21);
		}
		else if (strstr(buf, "Description: ") == buf)
		{
			p->description = strdup(buf+13);
		}
		/* TODO: localized descriptions */
	}
}

void *status_read(void)
{
	FILE *f;
	void *status = 0;
	struct package_t *m = 0, *p = 0, *t = 0;

	if ((f = fopen(STATUSFILE, "r")) == NULL)
	{
		perror(STATUSFILE);
		return 0;
	}
	if (getenv(UDPKG_QUIET) == NULL)
		printf("(Reading database...)\n");
	while (!feof(f))
	{
		m = (struct package_t *)malloc(sizeof(struct package_t));
		memset(m, 0, sizeof(struct package_t));
		control_read(f, m);
		if (m->package)
		{
			/*
			 * If there is an item in the tree by this name,
			 * it must be a virtual package; insert real
			 * package in preference.
			 */
			tdelete(m, &status, package_compare);
			tsearch(m, &status, package_compare);
			if (m->provides)
			{
				/* 
				 * A "Provides" triggers the insertion
				 * of a pseudo package into the status
				 * binary-tree.
				 */
				p = (struct package_t *)malloc(sizeof(struct package_t));
				memset(p, 0, sizeof(struct package_t));
				p->package = strdup(m->provides);

				t = *(struct package_t **)tsearch(p, &status, package_compare);
				if (!(t == p))
				{
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
		else
		{
			free(m);
		}
	}
	fclose(f);
	return status;
}

int status_merge(void *status, struct package_t *pkgs)
{
	FILE *fin, *fout;
	char buf[BUFSIZE];
	struct package_t *pkg = 0, *statpkg = 0;
	struct package_t locpkg;
	int r = 0;

	if ((fin = fopen(STATUSFILE, "r")) == NULL)
	{
		perror(STATUSFILE);
		return 0;
	}
	if ((fout = fopen(STATUSFILE ".new", "w")) == NULL)
	{
		perror(STATUSFILE ".new");
		return 0;
	}
	if (getenv(UDPKG_QUIET) == NULL)
		printf("(Updating database...)\n");
	while (fgets(buf, BUFSIZE, fin) && !feof(fin))
	{
		buf[strlen(buf)-1] = 0; /* trim newline */
		/* If we see a package header, find out if it's a package
		 * that we have processed. if so, we skip that block for
		 * now (write it at the end).
		 *
		 * we also look at packages in the status cache and update
		 * their status fields
		 */
		if (strstr(buf, "Package: ") == buf)
		{
			for (pkg = pkgs; pkg != 0 && strncmp(buf+9,
					pkg->package, strlen(pkg->package))!=0;
			     pkg = pkg->next) ;

			locpkg.package = buf+9;
			statpkg = tfind(&locpkg, &status, package_compare);
			
			/* note: statpkg should be non-zero, unless the status
			 * file was changed while we are processing (no locking
			 * is currently done...
			 */
			if (statpkg != 0) statpkg = *(struct package_t **)statpkg;
		}
		if (pkg != 0) continue;

		if (strstr(buf, "Status: ") == buf && statpkg != 0)
		{
			  snprintf(buf, sizeof(buf), "Status: %s",
				 status_print(statpkg->status));
		}
		fputs(buf, fout);
		fputc('\n', fout);
	}

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

	r = rename(STATUSFILE, STATUSFILE ".bak");
	if (r == 0) r = rename(STATUSFILE ".new", STATUSFILE);
	return 0;
}

#include <errno.h>
#include <fcntl.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>

/* 
 * Main udpkg implementation routines
 */

#ifdef DODEBUG
static int do_system(const char *cmd)
{
	DPRINTF("cmd is %s\n", cmd);
	return system(cmd);
}
#else
#define do_system(cmd) system(cmd)
#endif

static int is_file(const char *fn)
{
	struct stat statbuf;

	if (stat(fn, &statbuf) < 0) return 0;
	return S_ISREG(statbuf.st_mode);
}

static int dpkg_copyfile(const char *src, const char *dest)
{
	/* copy a (regular) file if it exists, preserving the mode, mtime 
	 * and atime */
	char buf[8192];
	int infd, outfd;
	int r;
	struct stat srcStat;
	struct utimbuf times;

	if (stat(src, &srcStat) < 0) 
	{
		if (errno == 2) return 0; else return -1;
	}
	if ((infd = open(src, O_RDONLY)) < 0) 
		return -1;
	if ((outfd = open(dest, O_WRONLY|O_CREAT|O_TRUNC, srcStat.st_mode)) < 0)
		return -1;
	while ((r = read(infd, buf, sizeof(buf))) > 0)
	{
		if (write(outfd, buf, r) < 0)
			return -1;
	}
	close(outfd);
	close(infd);
	if (r < 0) return -1;
	times.actime = srcStat.st_atime;
	times.modtime = srcStat.st_mtime;
	if (utime(dest, &times) < 0) return -1;
	return 1;
}

static int dpkg_doconfigure(struct package_t *pkg)
{
	int r;
	char postinst[1024];
	char buf[1024];
	DPRINTF("Configuring %s\n", pkg->package);
	pkg->status &= status_statusmask;
	snprintf(postinst, sizeof(postinst), "%s%s.postinst", INFODIR, pkg->package);
	if (is_file(postinst))
	{
		snprintf(buf, sizeof(buf), "%s configure", postinst);
		if ((r = do_system(buf)) != 0)
		{
			fprintf(stderr, "postinst exited with status %d\n", r);
			pkg->status |= status_statushalfconfigured;
			return 1;
		}
	}

	pkg->status |= status_statusinstalled;
	
	return 0;
}

static int dpkg_dounpack(struct package_t *pkg)
{
	int r = 0;
	char *cwd, *p;
	FILE *infp, *outfp;
	char buf[1024], buf2[1024];
	int i;
	char *adminscripts[] = { "prerm", "postrm", "preinst", "postinst",
	                         "conffiles", "md5sums", "shlibs", 
				 "templates" };

	DPRINTF("Unpacking %s\n", pkg->package);

	cwd = getcwd(0, 0);
	chdir("/");
	snprintf(buf, sizeof(buf), "ar -p %s data.tar.gz|zcat|tar -xf -", pkg->file);
	if (SYSTEM(buf) == 0)
	{
		/* Installs the package scripts into the info directory */
		for (i = 0; i < sizeof(adminscripts) / sizeof(adminscripts[0]);
		     i++)
		{
			snprintf(buf, sizeof(buf), "%s%s/%s",
				DPKGCIDIR, pkg->package, adminscripts[i]);
			snprintf(buf2, sizeof(buf), "%s%s.%s", 
				INFODIR, pkg->package, adminscripts[i]);
			if (dpkg_copyfile(buf, buf2) < 0)
			{
				fprintf(stderr, "Cannot copy %s to %s: %s\n", 
					buf, buf2, strerror(errno));
				r = 1;
				break;
			}
			else
			{
				/* ugly hack to create the list file; should
				 * probably do something more elegant
				 *
				 * why oh why does dpkg create the list file
				 * so oddly...
				 */
				snprintf(buf, sizeof(buf), 
					"ar -p %s data.tar.gz|zcat|tar -tf -", 
					pkg->file);
				snprintf(buf2, sizeof(buf2),
					"%s%s.list", INFODIR, pkg->package);
				if ((infp = popen(buf, "r")) == NULL ||
				    (outfp = fopen(buf2, "w")) == NULL)
				{
					fprintf(stderr, "Cannot create %s\n",
						buf2);
					r = 1;
					break;
				}
				while (fgets(buf, sizeof(buf), infp) &&
				       !feof(infp))
				{
					p = buf;
					if (*p == '.') p++;
					if (*p == '/' && *(p+1) == '\n')
					{
						*(p+1) = '.';
						*(p+2) = '\n';
						*(p+3) = 0;
					}
					if (p[strlen(p)-2] == '/')
					{
						p[strlen(p)-2] = '\n';
						p[strlen(p)-1] = 0;
					}
					fputs(p, outfp);
				}
				fclose(infp);
				fclose(outfp);
			}
		}
		pkg->status &= status_wantmask;
		pkg->status |= status_wantinstall;
		pkg->status &= status_flagmask;
		pkg->status |= status_flagok;
		pkg->status &= status_statusmask;
		if (r == 0)
			pkg->status |= status_statusunpacked;
		else
			pkg->status |= status_statushalfinstalled;
	}
	chdir(cwd);
	return r;
}

static int dpkg_doinstall(struct package_t *pkg)
{
	DPRINTF("Installing %s\n", pkg->package);
	return (dpkg_dounpack(pkg) || dpkg_doconfigure(pkg));
}

static int dpkg_unpackcontrol(struct package_t *pkg)
{
	int r = 1;
	char *cwd = 0;
	char *p;
	char buf[1024];
	FILE *f;

	p = strrchr(pkg->file, '/');
	if (p) p++; else p = pkg->file;
	p = pkg->package = strdup(p);
	while (*p != 0 && *p != '_' && *p != '.') p++;
	*p = 0;

	cwd = getcwd(0, 0);
	snprintf(buf, sizeof(buf), "%s%s", DPKGCIDIR, pkg->package);
	DPRINTF("dir = %s\n", buf);
	if (mkdir(buf, S_IRWXU) == 0 && chdir(buf) == 0)
	{
		snprintf(buf, sizeof(buf), "ar -p %s control.tar.gz|zcat|tar -xf -",
			pkg->file);
		if (SYSTEM(buf) == 0)
		{
			if ((f = fopen("control", "r")) != NULL) {
				control_read(f, pkg);
				r = 0;
			}
		}
	}

	chdir(cwd);
	free(cwd);
	return r;
}

static int dpkg_unpack(struct package_t *pkgs)
{
	int r = 0;
	struct package_t *pkg;
	void *status = status_read();

	if (SYSTEM("rm -rf -- " DPKGCIDIR) != 0 ||
	    mkdir(DPKGCIDIR, S_IRWXU) != 0)
	{
		perror("mkdir");
		return 1;
	}
	
	for (pkg = pkgs; pkg != 0; pkg = pkg->next)
	{
		dpkg_unpackcontrol(pkg);
		r = dpkg_dounpack(pkg);
		if (r != 0) break;
	}
	status_merge(status, pkgs);
	SYSTEM("rm -rf -- " DPKGCIDIR);
	return r;
}

static int dpkg_configure(struct package_t *pkgs)
{
	int r = 0;
	void *found;
	struct package_t *pkg;
	void *status = status_read();
	for (pkg = pkgs; pkg != 0 && r == 0; pkg = pkg->next)
	{
		found = tfind(pkg, &status, package_compare);
		if (found == 0)
		{
			fprintf(stderr, "Trying to configure %s, but it is not installed\n", pkg->package);
			r = 1;
		}
		else
		{
			/* configure the package listed in the status file;
			 * not pkg, as we have info only for the latter */
			r = dpkg_doconfigure(*(struct package_t **)found);
		}
	}
	status_merge(status, 0);
	return r;
}

static int dpkg_install(struct package_t *pkgs)
{
	struct package_t *p, *ordered = 0;
	void *status = status_read();
	if (SYSTEM("rm -rf -- " DPKGCIDIR) != 0 ||
	    mkdir(DPKGCIDIR, S_IRWXU) != 0)
	{
		perror("mkdir");
		return 1;
	}
	
	/* Stage 1: parse all the control information */
	for (p = pkgs; p != 0; p = p->next)
		if (dpkg_unpackcontrol(p) != 0)
		{
			perror(p->file);
			/* force loop break, and prevents further ops */
			pkgs = 0;
		}
	
	/* Stage 2: resolve dependencies */
#ifdef DODEPENDS
	ordered = depends_resolve(pkgs, status);
#else
	ordered = pkgs;
#endif
	
	/* Stage 3: install */
	for (p = ordered; p != 0; p = p->next)
	{
		p->status &= status_wantmask;
		p->status |= status_wantinstall;

		/* for now the flag is always set to ok... this is probably
		 * not what we want
		 */
		p->status &= status_flagmask;
		p->status |= status_flagok;

		if (dpkg_doinstall(p) != 0)
		{
			perror(p->file);
		}
	}
	
	if (ordered != 0)
		status_merge(status, pkgs);
	SYSTEM("rm -rf -- " DPKGCIDIR);
	return 0;
}

static int dpkg_remove(struct package_t *pkgs)
{
	struct package_t *p;
	void *status = status_read();
	for (p = pkgs; p != 0; p = p->next)
	{
	}
	status_merge(status, 0);
	return 0;
}

int dpkg_main(int argc, char **argv)
{
	char opt = 0;
	char *s;
	struct package_t *p, *packages = NULL;
	char *cwd = getcwd(0, 0);
	while (*++argv)
	{
		if (**argv == '-') {
			/* Nasty little hack to "parse" long options. */
			s = *argv;
			while (*s == '-')
				s++;
			opt=s[0];
		}
		else
		{
			p = (struct package_t *)malloc(sizeof(struct package_t));
			memset(p, 0, sizeof(struct package_t));
			if (**argv == '/')
				p->file = *argv;
			else if (opt != 'c')
			{
				p->file = malloc(strlen(cwd) + strlen(*argv) + 2);
				sprintf(p->file, "%s/%s", cwd, *argv);
			}
			else {
				p->package = strdup(*argv);
			}
			p->next = packages;
			packages = p;
		}
			
	}
	switch (opt)
	{
		case 'i': return dpkg_install(packages); break;
		case 'r': return dpkg_remove(packages); break;
		case 'u': return dpkg_unpack(packages); break;
		case 'c': return dpkg_configure(packages); break;
	}

	/* if it falls through to here, some of the command line options were
	   wrong */
	usage(dpkg_usage);
	return 0;
}