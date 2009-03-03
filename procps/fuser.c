/* vi: set sw=4 ts=4: */
/*
 * tiny fuser implementation
 *
 * Copyright 2004 Tony J. White
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

#define MAX_LINE 255

#define OPTION_STRING "mks64"
enum {
	OPT_MOUNT  = (1 << 0),
	OPT_KILL   = (1 << 1),
	OPT_SILENT = (1 << 2),
	OPT_IP6    = (1 << 3),
	OPT_IP4    = (1 << 4),
};

typedef struct inode_list {
	struct inode_list *next;
	ino_t inode;
	dev_t dev;
} inode_list;

typedef struct pid_list {
	struct pid_list *next;
	pid_t pid;
} pid_list;

static dev_t find_socket_dev(void)
{
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd >= 0) {
		struct stat buf;
		int r = fstat(fd, &buf);
		close(fd);
		if (r == 0)
			return buf.st_dev;
	}
	return 0;
}

static int file_to_dev_inode(const char *filename, dev_t *dev, ino_t *inode)
{
	struct stat f_stat;
	if (stat(filename, &f_stat))
		return 0;
	*inode = f_stat.st_ino;
	*dev = f_stat.st_dev;
	return 1;
}

static char *parse_net_arg(const char *arg, unsigned *port)
{
	char path[20], tproto[5];

	if (sscanf(arg, "%u/%4s", port, tproto) != 2)
		return NULL;
	sprintf(path, "/proc/net/%s", tproto);
	if (access(path, R_OK) != 0)
		return NULL;
	return xstrdup(tproto);
}

static pid_list *add_pid(pid_list *plist, pid_t pid)
{
	pid_list *curr = plist;
	while (curr != NULL) {
		if (curr->pid == pid)
			return plist;
		curr = curr->next;
	}
	curr = xmalloc(sizeof(pid_list));
	curr->pid = pid;
	curr->next = plist;
	return curr;
}

static inode_list *add_inode(inode_list *ilist, dev_t dev, ino_t inode)
{
	inode_list *curr = ilist;
	while (curr != NULL) {
		if (curr->inode == inode && curr->dev == dev)
			return ilist;
		curr = curr->next;
	}
	curr = xmalloc(sizeof(inode_list));
	curr->dev = dev;
	curr->inode = inode;
	curr->next = ilist;
	return curr;
}

static inode_list *scan_proc_net(const char *proto,
				unsigned port, inode_list *ilist)
{
	char path[20], line[MAX_LINE + 1];
	ino_t tmp_inode;
	dev_t tmp_dev;
	long long uint64_inode;
	unsigned tmp_port;
	FILE *f;

	tmp_dev = find_socket_dev();

	sprintf(path, "/proc/net/%s", proto);
	f = fopen_for_read(path);
	if (!f)
		return ilist;

	while (fgets(line, MAX_LINE, f)) {
		char addr[68];
		if (sscanf(line, "%*d: %64[0-9A-Fa-f]:%x %*x:%*x %*x %*x:%*x "
				"%*x:%*x %*x %*d %*d %llu",
				addr, &tmp_port, &uint64_inode) == 3
		) {
			int len = strlen(addr);
			if (len == 8 && (option_mask32 & OPT_IP6))
				continue;
			if (len > 8 && (option_mask32 & OPT_IP4))
				continue;
			if (tmp_port == port) {
				tmp_inode = uint64_inode;
				ilist = add_inode(ilist, tmp_dev, tmp_inode);
			}
		}
	}
	fclose(f);
	return ilist;
}

static int search_dev_inode(inode_list *ilist, dev_t dev, ino_t inode)
{
	while (ilist) {
		if (ilist->dev == dev) {
			if (option_mask32 & OPT_MOUNT)
				return 1;
			if (ilist->inode == inode)
				return 1;
		}
		ilist = ilist->next;
	}
	return 0;
}

static pid_list *scan_pid_maps(const char *fname, pid_t pid,
				inode_list *ilist, pid_list *plist)
{
	FILE *file;
	char line[MAX_LINE + 1];
	int major, minor;
	ino_t inode;
	long long uint64_inode;
	dev_t dev;

	file = fopen_for_read(fname);
	if (!file)
		return plist;
	while (fgets(line, MAX_LINE, file)) {
		if (sscanf(line, "%*s %*s %*s %x:%x %llu", &major, &minor, &uint64_inode) != 3)
			continue;
		inode = uint64_inode;
		if (major == 0 && minor == 0 && inode == 0)
			continue;
		dev = makedev(major, minor);
		if (search_dev_inode(ilist, dev, inode))
			plist = add_pid(plist, pid);
	}
	fclose(file);
	return plist;
}

static pid_list *scan_link(const char *lname, pid_t pid,
				inode_list *ilist, pid_list *plist)
{
	ino_t inode;
	dev_t dev;

	if (!file_to_dev_inode(lname, &dev, &inode))
		return plist;
	if (search_dev_inode(ilist, dev, inode))
		plist = add_pid(plist, pid);
	return plist;
}

static pid_list *scan_dir_links(const char *dname, pid_t pid,
				inode_list *ilist, pid_list *plist)
{
	DIR *d;
	struct dirent *de;
	char *lname;

	d = opendir(dname);
	if (!d)
		return plist;
	while ((de = readdir(d)) != NULL) {
		lname = concat_subpath_file(dname, de->d_name);
		if (lname == NULL)
			continue;
		plist = scan_link(lname, pid, ilist, plist);
		free(lname);
	}
	closedir(d);
	return plist;
}

/* NB: does chdir internally */
static pid_list *scan_proc_pids(inode_list *ilist)
{
	DIR *d;
	struct dirent *de;
	pid_t pid;
	pid_list *plist;

	xchdir("/proc");
	d = opendir("/proc");
	if (!d)
		return NULL;

	plist = NULL;
	while ((de = readdir(d)) != NULL) {
		pid = (pid_t)bb_strtou(de->d_name, NULL, 10);
		if (errno)
			continue;
		if (chdir(de->d_name) < 0)
			continue;
		plist = scan_link("cwd", pid, ilist, plist);
		plist = scan_link("exe", pid, ilist, plist);
		plist = scan_link("root", pid, ilist, plist);
		plist = scan_dir_links("fd", pid, ilist, plist);
		plist = scan_dir_links("lib", pid, ilist, plist);
		plist = scan_dir_links("mmap", pid, ilist, plist);
		plist = scan_pid_maps("maps", pid, ilist, plist);
		xchdir("/proc");
	}
	closedir(d);
	return plist;
}

static int print_pid_list(pid_list *plist)
{
	while (plist != NULL) {
		printf("%u ", (unsigned)plist->pid);
		plist = plist->next;
	}
	bb_putchar('\n');
	return 1;
}

static int kill_pid_list(pid_list *plist, int sig)
{
	pid_t mypid = getpid();
	int success = 1;

	while (plist != NULL) {
		if (plist->pid != mypid) {
			if (kill(plist->pid, sig) != 0) {
				bb_perror_msg("kill pid %u", (unsigned)plist->pid);
				success = 0;
			}
		}
		plist = plist->next;
	}
	return success;
}

int fuser_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int fuser_main(int argc UNUSED_PARAM, char **argv)
{
	pid_list *plist;
	inode_list *ilist;
	char **pp;
	dev_t dev;
	ino_t inode;
	unsigned port;
	int opt;
	int success;
	int killsig;
/*
fuser [options] FILEs or PORT/PROTOs
Find processes which use FILEs or PORTs
        -m      Find processes which use same fs as FILEs
        -4      Search only IPv4 space
        -6      Search only IPv6 space
        -s      Silent: just exit with 0 if any processes are found
        -k      Kill found processes (otherwise display PIDs)
        -SIGNAL Signal to send (default: TERM)
*/
	/* Handle -SIGNAL. Oh my... */
	killsig = SIGTERM;
	pp = argv;
	while (*++pp) {
		char *arg = *pp;
		if (arg[0] != '-')
			continue;
		if (arg[1] == '-' && arg[2] == '\0') /* "--" */
			break;
		if ((arg[1] == '4' || arg[1] == '6') && arg[2] == '\0')
			continue; /* it's "-4" or "-6" */
		opt = get_signum(&arg[1]);
		if (opt < 0)
			continue;
		/* "-SIGNAL" option found. Remove it and bail out */
		killsig = opt;
		do {
			pp[0] = arg = pp[1];
			pp++;
		} while (arg);
		break;
	}

	opt = getopt32(argv, OPTION_STRING);
	argv += optind;

	ilist = NULL;
	pp = argv;
	while (*pp) {
		char *proto = parse_net_arg(*pp, &port);
		if (proto) { /* PORT/PROTO */
			ilist = scan_proc_net(proto, port, ilist);
			free(proto);
		} else { /* FILE */
			if (!file_to_dev_inode(*pp, &dev, &inode))
				bb_perror_msg_and_die("can't open '%s'", *pp);
			ilist = add_inode(ilist, dev, inode);
		}
		pp++;
	}

	plist = scan_proc_pids(ilist); /* changes dir to "/proc" */

	if (!plist)
		return EXIT_FAILURE;
	success = 1;
	if (opt & OPT_KILL) {
		success = kill_pid_list(plist, killsig);
	} else if (!(opt & OPT_SILENT)) {
		success = print_pid_list(plist);
	}
	return (success != 1); /* 0 == success */
}
