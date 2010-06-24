/*
 smemcap - a tool for meaningful memory reporting

 Copyright 2008-2009 Matt Mackall <mpm@selenic.com>

 This software may be used and distributed according to the terms of
 the GNU General Public License version 2 or later, incorporated
 herein by reference.
*/

//applet:IF_SMEMCAP(APPLET(smemcap, _BB_DIR_USR_BIN, _BB_SUID_DROP))

//kbuild:lib-$(CONFIG_SMEMCAP) += smemcap.o

//config:config SMEMCAP
//config:	bool "smemcap"
//config:	default y
//config:	help
//config:	  smemcap is a tool for capturing process data for smem,
//config:	  a memory usage statistic tool.

#include "libbb.h"

struct tar_header {
	char name[100];           /*   0-99 */
	char mode[8];             /* 100-107 */
	char uid[8];              /* 108-115 */
	char gid[8];              /* 116-123 */
	char size[12];            /* 124-135 */
	char mtime[12];           /* 136-147 */
	char chksum[8];           /* 148-155 */
	char typeflag;            /* 156-156 */
	char linkname[100];       /* 157-256 */
	/* POSIX:   "ustar" NUL "00" */
	/* GNU tar: "ustar  " NUL */
	/* Normally it's defined as magic[6] followed by
	 * version[2], but we put them together to save code.
	 */
	char magic[8];            /* 257-264 */
	char uname[32];           /* 265-296 */
	char gname[32];           /* 297-328 */
	char devmajor[8];         /* 329-336 */
	char devminor[8];         /* 337-344 */
	char prefix[155];         /* 345-499 */
	char padding[12];         /* 500-512 (pad to exactly TAR_512) */
};

struct fileblock {
	struct fileblock *next;
	char data[512];
};

static void writeheader(const char *path, struct stat *sb, int type)
{
	struct tar_header header;
	int i, sum;

	memset(&header, 0, 512);
	strcpy(header.name, path);
	sprintf(header.mode, "%o", sb->st_mode & 0777);
	/* careful to not overflow fields! */
	sprintf(header.uid, "%o", sb->st_uid & 07777777);
	sprintf(header.gid, "%o", sb->st_gid & 07777777);
	sprintf(header.size, "%o", (unsigned)sb->st_size);
	sprintf(header.mtime, "%llo", sb->st_mtime & 077777777777LL);
	header.typeflag = type;
	//strcpy(header.magic, "ustar  "); - do we want to be standard-compliant?

	/* Calculate and store the checksum (the sum of all of the bytes of
	 * the header). The checksum field must be filled with blanks for the
	 * calculation. The checksum field is formatted differently from the
	 * other fields: it has 6 digits, a NUL, then a space -- rather than
	 * digits, followed by a NUL like the other fields... */
	header.chksum[7] = ' ';
	sum = ' ' * 7;
	for (i = 0; i < 512; i++)
		sum += ((unsigned char*)&header)[i];
	sprintf(header.chksum, "%06o", sum);

	xwrite(STDOUT_FILENO, &header, 512);
}

static void archivefile(const char *path)
{
	struct fileblock *start, *cur;
	struct fileblock **prev = &start;
	int fd, r;
	unsigned size = 0;
	struct stat s;

	/* buffer the file */
	fd = xopen(path, O_RDONLY);
	do {
		cur = xzalloc(sizeof(*cur));
		*prev = cur;
		prev = &cur->next;
		r = full_read(fd, cur->data, 512);
		if (r > 0)
			size += r;
	} while (r == 512);

	/* write archive header */
	fstat(fd, &s);
	close(fd);
	s.st_size = size;
	writeheader(path, &s, '0');

	/* dump file contents */
	for (cur = start; (int)size > 0; size -= 512) {
		xwrite(STDOUT_FILENO, cur->data, 512);
		start = cur;
		cur = cur->next;
		free(start);
	}
}

static void archivejoin(const char *sub, const char *name)
{
	char path[sizeof(long long)*3 + sizeof("/cmdline")];
	sprintf(path, "%s/%s", sub, name);
	archivefile(path);
}

//usage:#define smemcap_trivial_usage ">SMEMDATA.TAR"
//usage:#define smemcap_full_usage "\n\n"
//usage:       "Collect memory usage data in /proc and write it to stdout"

int smemcap_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int smemcap_main(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	DIR *d;
	struct dirent *de;

	xchdir("/proc");
	d = xopendir(".");

	archivefile("meminfo");
	archivefile("version");
	while ((de = readdir(d)) != NULL) {
		if (isdigit(de->d_name[0])) {
			struct stat s;
			memset(&s, 0, sizeof(s));
			s.st_mode = 0555;
			writeheader(de->d_name, &s, '5');
			archivejoin(de->d_name, "smaps");
			archivejoin(de->d_name, "cmdline");
			archivejoin(de->d_name, "stat");
		}
	}

	return EXIT_SUCCESS;
}
