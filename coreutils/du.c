/* vi: set sw=4 ts=4: */
/*
 * Mini du implementation for busybox
 *
 *
 * Copyright (C) 1999 by Lineo, inc.
 * Written by John Beppu <beppu@lineo.com>
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
 */

#include "internal.h"
#define BB_DECLARE_EXTERN
#define bb_need_name_too_long
#include "messages.c"

#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>
#include <sys/param.h>			/* for PATH_MAX */

typedef void (Display) (long, char *);

typedef struct inode_type {
	struct inode_type *next;
	ino_t ino;
} INODETYPE;

#define HASH_SIZE	311		/* Should be prime */
#define hash_inode(i)	((i) % HASH_SIZE)

static INODETYPE *inode_hash_list[HASH_SIZE];

static const char du_usage[] =
	"du [OPTION]... [FILE]...\n\n"
	"Summarize disk space used for each FILE and/or directory.\n"
	"Disk space is printed in units of 1024 bytes.\n\n"
	"Options:\n"
	"\t-l\tcount sizes many times if hard linked\n"
	"\t-s\tdisplay only a total for each argument\n";

static int du_depth = 0;
static int count_hardlinks = 0;

static Display *print;

static void print_normal(long size, char *filename)
{
	fprintf(stdout, "%ld\t%s\n", size, filename);
}

static void print_summary(long size, char *filename)
{
	if (du_depth == 1) {
		print_normal(size, filename);
	}
}

/* Return 1 if inode is in inode hash list, else return 0 */
static int is_in_list(const ino_t ino)
{
	INODETYPE *inode;

	inode = inode_hash_list[hash_inode(ino)];
	while (inode != NULL) {
		if (inode->ino == ino)
			return 1;
		inode = inode->next;
	}

	return 0;
}

/* Add inode to inode hash list */
static void add_inode(const ino_t ino)
{
	int i;
	INODETYPE *inode;
    
	i = hash_inode(ino);
	inode = malloc(sizeof(INODETYPE));
	if (inode == NULL)
		fatalError("du: Not enough memory.");

	inode->ino = ino;
	inode->next = inode_hash_list[i];
	inode_hash_list[i] = inode;
}

/* Clear inode hash list */
static void reset_inode_list(void)
{
	int i;
	INODETYPE *inode;

	for (i = 0; i < HASH_SIZE; i++) {
		while (inode_hash_list[i] != NULL) {
			inode = inode_hash_list[i]->next;
			free(inode_hash_list[i]);
			inode_hash_list[i] = inode;
		}
	}
}

/* tiny recursive du */
static long du(char *filename)
{
	struct stat statbuf;
	long sum;
	int len;

	if ((lstat(filename, &statbuf)) != 0) {
		printf("du: %s: %s\n", filename, strerror(errno));
		return 0;
	}

	du_depth++;
	sum = (statbuf.st_blocks >> 1);

	/* Don't add in stuff pointed to by symbolic links */
	if (S_ISLNK(statbuf.st_mode)) {
		sum = 0L;
		if (du_depth == 1)
			print(sum, filename);
	}
	if (S_ISDIR(statbuf.st_mode)) {
		DIR *dir;
		struct dirent *entry;

		dir = opendir(filename);
		if (!dir) {
			du_depth--;
			return 0;
		}

		len = strlen(filename);
		if (filename[len - 1] == '/')
			filename[--len] = '\0';

		while ((entry = readdir(dir))) {
			char newfile[PATH_MAX + 1];
			char *name = entry->d_name;

			if ((strcmp(name, "..") == 0)
				|| (strcmp(name, ".") == 0)) {
				continue;
			}

			if (len + strlen(name) + 1 > PATH_MAX) {
				fprintf(stderr, name_too_long, "du");
				du_depth--;
				return 0;
			}
			sprintf(newfile, "%s/%s", filename, name);

			sum += du(newfile);
		}
		closedir(dir);
		print(sum, filename);
	}
	else if (statbuf.st_nlink > 1 && !count_hardlinks) {
		/* Add files with hard links only once */
		if (is_in_list(statbuf.st_ino)) {
			sum = 0L;
			if (du_depth == 1)
				print(sum, filename);
		}
		else {
			add_inode(statbuf.st_ino);
		}
	}
	du_depth--;
	return sum;
}

int du_main(int argc, char **argv)
{
	int i;
	char opt;

	/* default behaviour */
	print = print_normal;

	/* parse argv[] */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			opt = argv[i][1];
			switch (opt) {
			case 's':
				print = print_summary;
				break;
			case 'l':
				count_hardlinks = 1;
				break;
			case 'h':
			case '-':
				usage(du_usage);
				break;
			default:
				fprintf(stderr, "du: invalid option -- %c\n", opt);
				usage(du_usage);
			}
		} else {
			break;
		}
	}

	/* go through remaining args (if any) */
	if (i >= argc) {
		du(".");
	} else {
		long sum;

		for (; i < argc; i++) {
			sum = du(argv[i]);
			if (sum && isDirectory(argv[i], FALSE, NULL)) {
				print_normal(sum, argv[i]);
			}
			reset_inode_list();
		}
	}

	exit(0);
}

/* $Id: du.c,v 1.15 2000/02/21 17:27:17 erik Exp $ */
