/*
 * Busybox main header file
 *
 * Copyright (C) 1998 by Erik Andersen <andersee@debian.org>
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
#ifndef	_INTERNAL_H_
#define	_INTERNAL_H_

#include "busybox.def.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>


/* Some useful definitions */
typedef int     BOOL;
#define STDIN	0
#define STDOUT	1
#define FALSE   ((BOOL) 1)
#define TRUE    ((BOOL) 0)

#define PATH_LEN        1024
#define BUF_SIZE        8192
#define EXPAND_ALLOC    1024

#define isBlank(ch)     (((ch) == ' ') || ((ch) == '\t'))
#define isDecimal(ch)   (((ch) >= '0') && ((ch) <= '9'))
#define isOctal(ch)     (((ch) >= '0') && ((ch) <= '7'))
#define isWildCard(ch)  (((ch) == '*') || ((ch) == '?') || ((ch) == '['))



struct FileInfo {
	unsigned int	complainInPostProcess:1;
	unsigned int	changeUserID:1;
	unsigned int	changeGroupID:1;
	unsigned int	changeMode:1;
	unsigned int	create:1;
	unsigned int	force:1;
	unsigned int	recursive:1;
	unsigned int	processDirectoriesAfterTheirContents;
	unsigned int	makeParentDirectories:1;
	unsigned int	didOperation:1;
	unsigned int	isSymbolicLink:1;
	unsigned int	makeSymbolicLink:1;
	unsigned int	dyadic:1;
	const char*	source;
	const char*	destination;
	int				directoryLength;
	uid_t			userID;
	gid_t			groupID;
	mode_t			andWithMode;
	mode_t			orWithMode;
	struct stat		stat;
	const struct Applet *
					applet;
};

struct Applet {
	const	char*	name;
	int	(*main)(int argc, char** argv);
};

extern void	name_and_error(const char*);
extern int	is_a_directory(const char*);
extern char*	join_paths(char *, const char *, const char *);

extern int	descend(
		 struct FileInfo *o
		,int 		(*function)(const struct FileInfo * i));

extern struct mntent *
		findMountPoint(const char*, const char *);

extern void usage(const char*);
extern int busybox_main(int argc, char** argv);
extern int block_device_main(int argc, char** argv);
extern int cat_more_main(int argc, char** argv);
extern int more_main(int argc, char** argv);
extern int chgrp_main(int argc, char** argv);
extern int chmod_main(int argc, char** argv);
extern int chown_main(int argc, char** argv);
extern int chroot_main(int argc, char** argv);
extern int clear_main(int argc, char** argv);
extern int date_main(int argc, char** argv);
extern int dd_main(int argc, char** argv);
extern int df_main(int argc, char** argv);
extern int dmesg_main(int argc, char** argv);
extern int dyadic_main(int argc, char** argv);
extern int false_main(int argc, char** argv);
extern int fdisk_main(int argc, char** argv);
extern int find_main(int argc, char** argv);
extern int grep_main(int argc, char** argv);
extern int halt_main(int argc, char** argv);
extern int init_main(int argc, char** argv);
extern int kill_main(int argc, char** argv);
extern int length_main(int argc, char** argv);
extern int ln_main(int argc, char** argv);
extern int loadkmap_main(int argc, char** argv);
extern int losetup_main(int argc, char** argv);
extern int ls_main(int argc, char** argv);
extern int makedevs_main(int argc, char** argv);
extern int math_main(int argc, char** argv);
extern int mknod_main(int argc, char** argv);
extern int mkswap_main(int argc, char** argv);
extern int mnc_main(int argc, char** argv);
extern int monadic_main(int argc, char** argv);
extern int mount_main(int argc, char** argv);
extern int mt_main(int argc, char** argv);
extern int printf_main(int argc, char** argv);
extern int pwd_main(int argc, char** argv);
extern int reboot_main(int argc, char** argv);
extern int rm_main(int argc, char** argv);
extern int scan_partitions_main(int argc, char** argv);
extern int sh_main(int argc, char** argv);
extern int sleep_main(int argc, char** argv);
extern int tar_main(int argc, char** argv);
extern int sync_main(int argc, char** argv);
extern int tput_main(int argc, char** argv);
extern int true_main(int argc, char** argv);
extern int tryopen_main(int argc, char** argv);
extern int umount_main(int argc, char** argv);
extern int update_main(int argc, char** argv);
extern int zcat_main(int argc, char** argv);
extern int gzip_main(int argc, char** argv);

extern int
parse_mode(
 const char*	s
,mode_t *		or
,mode_t *		and
,int *			group_execute);

extern int		parse_user_name(const char* string, struct FileInfo * i);


/*
 * A chunk of data.
 * Chunks contain data which is allocated as needed, but which is
 * not freed until all of the data needs freeing, such as at
 * the beginning of the next command.
 */
typedef struct chunk CHUNK;
#define CHUNK_INIT_SIZE 4

struct chunk {
    CHUNK *next;
    char data[CHUNK_INIT_SIZE];	/* actually of varying length */
};

const char *modeString(int mode);
const char *timeString(time_t timeVal);
BOOL isDirectory(const char *name);
BOOL isDevice(const char *name);
BOOL copyFile(const char *srcName, const char *destName, BOOL setModes);
const char *buildName(const char *dirName, const char *fileName);
BOOL makeString(int argc, const char **argv, char *buf, int bufLen);
char *getChunk(int size);
char *chunkstrdup(const char *str);
void freeChunks(void);
int fullWrite(int fd, const char *buf, int len);
int fullRead(int fd, char *buf, int len);
int recursiveAction(const char *fileName, BOOL followLinks,
	  int (*fileAction) (const char *fileName),
	  int (*dirAction) (const char *fileName));


#endif

