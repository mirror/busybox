/*
 * Utility routines.
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

#include "internal.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
//#include <sys/types.h>
//#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <utime.h>

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
BOOL match(const char *text, const char *pattern);
BOOL makeArgs(const char *cmd, int *retArgc, const char ***retArgv);
BOOL makeString(int argc, const char **argv, char *buf, int bufLen);
char *getChunk(int size);
char *chunkstrdup(const char *str);
void freeChunks(void);
int fullWrite(int fd, const char *buf, int len);
int fullRead(int fd, char *buf, int len);
int
recursive(const char *fileName, BOOL followLinks, const char *pattern,
	  int (*fileAction) (const char *fileName,
			     const struct stat * statbuf),
	  int (*dirAction) (const char *fileName,
			    const struct stat * statbuf));

int nameSort(const void *p1, const void *p2);
int expandWildCards(const char *fileNamePattern,
		    const char ***retFileTable);
