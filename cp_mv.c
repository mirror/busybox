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
#include <setjmp.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>

#define is_cp 0
#define is_mv 1
static int         dz_i;		/* index into cp_mv_usage */

const char cp_usage[] =
	"cp [OPTION]... SOURCE DEST\n"
	"   or: cp [OPTION]... SOURCE... DIRECTORY\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\nCopies SOURCE to DEST, or multiple SOURCE(s) to DIRECTORY.\n"
	"\n"
	"\t-a\tSame as -dpR\n"
	"\t-d\tPreserves links\n"
	"\t-p\tPreserves file attributes if possible\n"
	"\t-f\tforce (implied; ignored) - always set\n"
	"\t-R\tCopies directories recursively\n"
#endif
	;

const char mv_usage[] =
	"mv SOURCE DEST\n"
	"   or: mv SOURCE... DIRECTORY\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\nRename SOURCE to DEST, or move SOURCE(s) to DIRECTORY.\n"
#endif
	;

static const char *cp_mv_usage[] =	/* .rodata */
{
	cp_usage,
	mv_usage
};

static int recursiveFlag;
static int followLinks;
static int preserveFlag;
static int forceFlag;

static const char *baseSrcName;
static int		   srcDirFlag;
static struct stat srcStatBuf;

static char		   baseDestName[BUFSIZ + 1];
static size_t	   baseDestLen;
static int		   destDirFlag;
static struct stat destStatBuf;

static jmp_buf      catch;
static volatile int mv_Action_first_time;

static void name_too_long__exit (void) __attribute__((noreturn));

static
void name_too_long__exit (void)
{
	fatalError(name_too_long);
}

static void
fill_baseDest_buf(char *_buf, size_t * _buflen) {
	const char *srcBasename;
	if ((srcBasename = strrchr(baseSrcName, '/')) == NULL) {
		srcBasename = baseSrcName;
		if (_buf[*_buflen - 1] != '/') {
			if (++(*_buflen) > BUFSIZ)
				name_too_long__exit();
			strcat(_buf, "/");
		}
	}
	if (*_buflen + strlen(srcBasename) > BUFSIZ)
		name_too_long__exit();
	strcat(_buf, srcBasename);
	return;
	
}

static int
cp_mv_Action(const char *fileName, struct stat *statbuf, void* junk)
{
	char		destName[BUFSIZ + 1];
	size_t		destLen;
	const char *srcBasename;
	char	   *name;

	strcpy(destName, baseDestName);
	destLen = strlen(destName);

	if (srcDirFlag == TRUE) {
		if (recursiveFlag == FALSE) {
			errorMsg(omitting_directory, baseSrcName);
			return TRUE;
		}
		srcBasename = (strstr(fileName, baseSrcName)
					   + strlen(baseSrcName));

		if (destLen + strlen(srcBasename) > BUFSIZ) {
			errorMsg(name_too_long);
			return FALSE;
		}
		strcat(destName, srcBasename);
	}
	else if (destDirFlag == TRUE) {
		fill_baseDest_buf(&destName[0], &destLen);
	}
	else {
		srcBasename = baseSrcName;
	}
	if (mv_Action_first_time && (dz_i == is_mv)) {
		mv_Action_first_time = errno = 0;
		if (rename(fileName, destName) < 0 && errno != EXDEV) {
			errorMsg("rename(%s, %s): %s\n", fileName, destName, 
					strerror(errno));
			goto do_copyFile;	/* Try anyway... */
		}
		else if (errno == EXDEV)
			goto do_copyFile;
		else
			longjmp(catch, 1);	/* succeeded with rename() */
	}
 do_copyFile:
	if (preserveFlag == TRUE && statbuf->st_nlink > 1) {
		if (is_in_ino_dev_hashtable(statbuf, &name)) {
			if (link(name, destName) < 0) {
				errorMsg("link(%s, %s): %s\n", name, destName, strerror(errno));
				return FALSE;
			}
			return TRUE;
		}
		else {
			add_to_ino_dev_hashtable(statbuf, destName);
		}
	}
	return copyFile(fileName, destName, preserveFlag, followLinks, forceFlag);
}

static int
rm_Action(const char *fileName, struct stat *statbuf, void* junk)
{
	int status = TRUE;

	if (S_ISDIR(statbuf->st_mode)) {
		if (rmdir(fileName) < 0) {
			errorMsg("rmdir(%s): %s\n", fileName, strerror(errno));
			status = FALSE;
		}
	} else if (unlink(fileName) < 0) {
		errorMsg("unlink(%s): %s\n", fileName, strerror(errno));
		status = FALSE;
	}
	return status;
}

extern int cp_mv_main(int argc, char **argv)
{
	volatile int i;
	int c;

	if (*applet_name == 'c' && *(applet_name + 1) == 'p')
		dz_i = is_cp;
	else
		dz_i = is_mv;
	if (argc < 3)
		usage(cp_mv_usage[dz_i]);

	if (dz_i == is_cp) {
		recursiveFlag = preserveFlag = forceFlag = FALSE;
		followLinks = TRUE;
		while ((c = getopt(argc, argv, "adpRf")) != EOF) {
				switch (c) {
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
				case 'f':
					forceFlag = TRUE;
					break;
				default:
					usage(cp_mv_usage[is_cp]);
				}
		}
		if ((argc - optind) < 2) {
			usage(cp_mv_usage[dz_i]);
		}
	} else {					/* (dz_i == is_mv) */
		recursiveFlag = preserveFlag = TRUE;
		followLinks = FALSE;
	}
	

	if (strlen(argv[argc - 1]) > BUFSIZ) {
		errorMsg(name_too_long);
		goto exit_false;
	}
	strcpy(baseDestName, argv[argc - 1]);
	baseDestLen = strlen(baseDestName);
	if (baseDestLen == 0)
		goto exit_false;

	destDirFlag = isDirectory(baseDestName, TRUE, &destStatBuf);
	if ((argc > 3) && destDirFlag == FALSE) {
		errorMsg(not_a_directory, baseDestName);
		goto exit_false;
	}

	for (i = optind; i < (argc-1); i++) {
		size_t srcLen;
		volatile int flags_memo;
		int	   status;

		baseSrcName=argv[i];

		if ((srcLen = strlen(baseSrcName)) > BUFSIZ)
			name_too_long__exit();

		if (srcLen == 0) continue; /* "" */

		srcDirFlag = isDirectory(baseSrcName, followLinks, &srcStatBuf);

		if ((flags_memo = (recursiveFlag == TRUE &&
						   srcDirFlag == TRUE && destDirFlag == TRUE))) {

			struct stat sb;
			int			state = 0;
			char		*pushd, *d, *p;

			if ((pushd = getcwd(NULL, BUFSIZ + 1)) == NULL) {
				errorMsg("getcwd(): %s\n", strerror(errno));
				continue;
			}
			if (chdir(baseDestName) < 0) {
				errorMsg("chdir(%s): %s\n", baseSrcName, strerror(errno));
				continue;
			}
			if ((d = getcwd(NULL, BUFSIZ + 1)) == NULL) {
				errorMsg("getcwd(): %s\n", strerror(errno));
				continue;
			}
			while (!state && *d != '\0') {
				if (stat(d, &sb) < 0) {	/* stat not lstat - always dereference targets */
					errorMsg("stat(%s): %s\n", d, strerror(errno));
					state = -1;
					continue;
				}
				if ((sb.st_ino == srcStatBuf.st_ino) &&
					(sb.st_dev == srcStatBuf.st_dev)) {
					errorMsg("Cannot %s `%s' into a subdirectory of itself, "
							"`%s/%s'\n", applet_name, baseSrcName,
							baseDestName, baseSrcName);
					state = -1;
					continue;
				}
				if ((p = strrchr(d, '/')) != NULL) {
					*p = '\0';
				}
			}
			if (chdir(pushd) < 0) {
				errorMsg("chdir(%s): %s\n", pushd, strerror(errno));
				free(pushd);
				free(d);
				continue;
			}
			free(pushd);
			free(d);
			if (state < 0)
				continue;
			else
				fill_baseDest_buf(baseDestName, &baseDestLen);
		}
		status = setjmp(catch);
		if (status == 0) {
			mv_Action_first_time = 1;
			if (recursiveAction(baseSrcName,
								recursiveFlag, followLinks, FALSE,
								cp_mv_Action, cp_mv_Action, NULL) == FALSE) goto exit_false;
			if (dz_i == is_mv &&
				recursiveAction(baseSrcName,
								recursiveFlag, followLinks, TRUE,
								rm_Action, rm_Action, NULL) == FALSE) goto exit_false;
		}		
		if (flags_memo)
			*(baseDestName + baseDestLen) = '\0';
	}
	return( TRUE);
 exit_false:
	return( FALSE);
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
