/* vi: set sw=4 ts=4: */
/*
 * Mini `cp' and `mv' implementation for BusyBox.
 *
 *
 * Copyright (C) 1999 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
 *
 * Copyright (C) 2000 by BitterSweet Enterprises, LLC. (GPL)
 * Extensively modified and rewritten by  Karl M. Hegbloom <karlheg@debian.org>
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
#define bb_need_omitting_directory
#define bb_need_not_a_directory
#include "messages.c"

#include <stdio.h>
#include <time.h>
#include <utime.h>
#include <dirent.h>
#include <sys/param.h>

#define is_cp 0
#define is_mv 1
static const char *dz;			/* dollar zero, .bss */
static int dz_i;				/* index,       .bss */
static const char *cp_mv_usage[] =	/* .rodata */
{
	"cp [OPTION]... SOURCE DEST\n"
		"   or: cp [OPTION]... SOURCE... DIRECTORY\n\n"
		"Copy SOURCE to DEST, or multiple SOURCE(s) to DIRECTORY.\n"
		"\n"
		"\t-a\tsame as -dpR\n"
		"\t-d\tpreserve links\n"
		"\t-p\tpreserve file attributes if possible\n"
		"\t-R\tcopy directories recursively\n",
	"mv SOURCE DEST\n"
		"   or: mv SOURCE... DIRECTORY\n\n"
		"Rename SOURCE to DEST, or move SOURCE(s) to DIRECTORY.\n"
		"Warning!!  This is not GNU `mv'.  It does not preserve hard links.\n"
};

extern int cp_mv_main(int argc, char **argv)
{
	__label__ name_too_long__exit;
	__label__ exit_false;

	int recursiveFlag;
	int followLinks;
	int preserveFlag;

	const char *baseSrcName;
	int srcDirFlag;

	char baseDestName[PATH_MAX + 1];
	size_t baseDestLen;
	int destDirFlag;

	void fill_baseDest_buf(char *_buf, size_t * _buflen) {
		const char *srcBasename;
		if ((srcBasename = strrchr(baseSrcName, '/')) == NULL) {
			srcBasename = baseSrcName;
			if (_buf[*_buflen - 1] != '/') {
				if (++(*_buflen) > PATH_MAX)
					goto name_too_long__exit;
				strcat(_buf, "/");
			}
		}
		if (*_buflen + strlen(srcBasename) > PATH_MAX)
			 goto name_too_long__exit;
		strcat(_buf, srcBasename);
		return;
	}

	int fileAction(const char *fileName, struct stat *statbuf) {
		__label__ return_false;
		char destName[PATH_MAX + 1];
		size_t destLen;
		const char *srcBasename;

		 strcpy(destName, baseDestName);
		 destLen = strlen(destName);

		if (srcDirFlag == TRUE) {
			if (recursiveFlag == FALSE) {
				fprintf(stderr, omitting_directory, "cp", baseSrcName);
				return TRUE;
			}
			srcBasename = (strstr(fileName, baseSrcName)
						   + strlen(baseSrcName));

			if (destLen + strlen(srcBasename) > PATH_MAX) {
				fprintf(stderr, name_too_long, "cp");
				goto return_false;
			}
			strcat(destName, srcBasename);
		} else if (destDirFlag == TRUE) {
			fill_baseDest_buf(&destName[0], &destLen);
		} else {
			srcBasename = baseSrcName;
		}
		return copyFile(fileName, destName, preserveFlag, followLinks);

	  return_false:
		return FALSE;
	}

	int rmfileAction(const char *fileName, struct stat *statbuf) {
		if (unlink(fileName) < 0) {
			perror(fileName);
			return FALSE;
		}
		return TRUE;
	}

	int rmdirAction(const char *fileName, struct stat *statbuf) {
		if (rmdir(fileName) < 0) {
			perror(fileName);
			return FALSE;
		}
		return TRUE;
	}

	if ((dz = strrchr(*argv, '/')) == NULL)
		dz = *argv;
	else
		dz++;
	if (*dz == 'c' && *(dz + 1) == 'p')
		dz_i = is_cp;
	else
		dz_i = is_mv;
	if (argc < 3)
		usage(cp_mv_usage[dz_i]);
	argc--;
	argv++;

	if (dz_i == is_cp) {
		recursiveFlag = preserveFlag = FALSE;
		followLinks = TRUE;
		while (**argv == '-') {
			while (*++(*argv)) {
				switch (**argv) {
				case 'a':
					followLinks = FALSE;
					preserveFlag = TRUE;
					recursiveFlag = TRUE;
					break;
				case 'd':
					followLinks = FALSE;
					break;
				case 'p':
					preserveFlag = TRUE;
					break;
				case 'R':
					recursiveFlag = TRUE;
					break;
				default:
					usage(cp_mv_usage[is_cp]);
				}
			}
			argc--;
			argv++;
		}
	} else {					/* (dz_i == is_mv) */

		recursiveFlag = preserveFlag = TRUE;
		followLinks = FALSE;
	}

	if (strlen(argv[argc - 1]) > PATH_MAX) {
		fprintf(stderr, name_too_long, "cp");
		goto exit_false;
	}
	strcpy(baseDestName, argv[argc - 1]);
	baseDestLen = strlen(baseDestName);
	if (baseDestLen == 0)
		goto exit_false;

	destDirFlag = isDirectory(baseDestName, TRUE);
	if ((argc > 3) && destDirFlag == FALSE) {
		fprintf(stderr, not_a_directory, "cp", baseDestName);
		goto exit_false;
	}

	while (argc-- > 1) {
		size_t srcLen;
		int flags_memo;

		baseSrcName = *(argv++);

		if ((srcLen = strlen(baseSrcName)) > PATH_MAX)
			goto name_too_long__exit;

		if (srcLen == 0)
			continue;

		srcDirFlag = isDirectory(baseSrcName, followLinks);

		if ((flags_memo = (recursiveFlag == TRUE &&
						   srcDirFlag == TRUE && destDirFlag == TRUE))) {
			fill_baseDest_buf(&baseDestName[0], &baseDestLen);
		}
		if (recursiveAction(baseSrcName,
							recursiveFlag, followLinks, FALSE,
							fileAction, fileAction) == FALSE)
			goto exit_false;

		if (dz_i == is_mv &&
			recursiveAction(baseSrcName,
							recursiveFlag, followLinks, TRUE,
							rmfileAction, rmdirAction) == FALSE)
			goto exit_false;

		if (flags_memo)
			*(baseDestName + baseDestLen) = '\0';
	}

	exit TRUE;

  name_too_long__exit:
	fprintf(stderr, name_too_long, "cp");
  exit_false:
	exit FALSE;
}

// Local Variables:
// c-file-style: "k&r"
// c-basic-offset: 4
// End:
