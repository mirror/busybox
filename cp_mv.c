/* vi: set sw=4 ts=4: */
/*
 * Mini `cp' and `mv' implementation for BusyBox.
 *
 *
 * Copyright (C) 1999,2000,2001 by Lineo, inc.
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
#include <stdlib.h>
#include "busybox.h"
#define BB_DECLARE_EXTERN
#define bb_need_name_too_long
#define bb_need_omitting_directory
#define bb_need_not_a_directory
#include "messages.c"


static const int is_cp = 0;
static const int is_mv = 1;
static int         dz_i;		/* index into cp_mv_usage */


static int recursiveFlag;
static int followLinks;
static int preserveFlag;
static int forceFlag;

static const char *baseSrcName;
static int		   srcDirFlag;
static struct stat srcStatBuf;

static char		   *pBaseDestName;
static size_t	   baseDestLen;
static int		   destDirFlag;
static struct stat destStatBuf;

static jmp_buf      catch;
static volatile int mv_Action_first_time;

static void name_too_long__exit (void) __attribute__((noreturn));

static
void name_too_long__exit (void)
{
	error_msg_and_die(name_too_long);
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

	strcpy(destName, pBaseDestName);
	destLen = strlen(destName);

	if (srcDirFlag == TRUE) {
		if (recursiveFlag == FALSE) {
			error_msg(omitting_directory, baseSrcName);
			return TRUE;
		}
		srcBasename = (strstr(fileName, baseSrcName)
					   + strlen(baseSrcName));

		if (destLen + strlen(srcBasename) > BUFSIZ) {
			error_msg(name_too_long);
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
			perror_msg("rename(%s, %s)", fileName, destName);
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
				perror_msg("link(%s, %s)", name, destName);
				return FALSE;
			}
			return TRUE;
		}
		else {
			add_to_ino_dev_hashtable(statbuf, destName);
		}
	}
	return copy_file(fileName, destName, preserveFlag, followLinks, forceFlag);
}

static int
rm_Action(const char *fileName, struct stat *statbuf, void* junk)
{
	int status = TRUE;

	if (S_ISDIR(statbuf->st_mode)) {
		if (rmdir(fileName) < 0) {
			perror_msg("rmdir(%s)", fileName);
			status = FALSE;
		}
	} else if (unlink(fileName) < 0) {
		perror_msg("unlink(%s)", fileName);
		status = FALSE;
	}
	return status;
}

extern int cp_mv_main(int argc, char **argv)
{
	volatile int i;
	int c;
	RESERVE_BB_BUFFER(baseDestName,BUFSIZ + 1);
	pBaseDestName = baseDestName; /* available globally */

	if (*applet_name == 'c' && *(applet_name + 1) == 'p')
		dz_i = is_cp;
	else
		dz_i = is_mv;
	if (argc < 3)
		show_usage();

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
					show_usage();
				}
		}
		if ((argc - optind) < 2) {
			show_usage();
		}
	} else {					/* (dz_i == is_mv) */
		/* Initialize optind to 1, since in libc5 optind
		 * is not initialized until getopt() is called
		 * (or until sneaky programmers force it...). */
		optind = 1;
		recursiveFlag = preserveFlag = TRUE;
		followLinks = FALSE;
	}
	

	if (strlen(argv[argc - 1]) > BUFSIZ) {
		error_msg(name_too_long);
		goto exit_false;
	}
	strcpy(baseDestName, argv[argc - 1]);
	baseDestLen = strlen(baseDestName);
	if (baseDestLen == 0)
		goto exit_false;

	destDirFlag = is_directory(baseDestName, TRUE, &destStatBuf);
	if (argc - optind > 2 && destDirFlag == FALSE) {
		error_msg(not_a_directory, baseDestName);
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

		srcDirFlag = is_directory(baseSrcName, followLinks, &srcStatBuf);

		if ((flags_memo = (recursiveFlag == TRUE &&
						   srcDirFlag == TRUE && destDirFlag == TRUE))) {

			struct stat sb;
			int			state = 0;
			char		*pushd, *d, *p;

			if ((pushd = getcwd(NULL, BUFSIZ + 1)) == NULL) {
				perror_msg("getcwd()");
				continue;
			}
			if (chdir(baseDestName) < 0) {
				perror_msg("chdir(%s)", baseSrcName);
				continue;
			}
			if ((d = getcwd(NULL, BUFSIZ + 1)) == NULL) {
				perror_msg("getcwd()");
				continue;
			}
			while (!state && *d != '\0') {
				if (stat(d, &sb) < 0) {	/* stat not lstat - always dereference targets */
					perror_msg("stat(%s)", d);
					state = -1;
					continue;
				}
				if ((sb.st_ino == srcStatBuf.st_ino) &&
					(sb.st_dev == srcStatBuf.st_dev)) {
					error_msg("Cannot %s `%s' into a subdirectory of itself, "
							"`%s/%s'", applet_name, baseSrcName,
							baseDestName, baseSrcName);
					state = -1;
					continue;
				}
				if ((p = strrchr(d, '/')) != NULL) {
					*p = '\0';
				}
			}
			if (chdir(pushd) < 0) {
				perror_msg("chdir(%s)", pushd);
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
			if (recursive_action(baseSrcName,
								recursiveFlag, followLinks, FALSE,
								cp_mv_Action, cp_mv_Action, NULL) == FALSE) goto exit_false;
			if (dz_i == is_mv &&
				recursive_action(baseSrcName,
								recursiveFlag, followLinks, TRUE,
								rm_Action, rm_Action, NULL) == FALSE) goto exit_false;
		}		
		if (flags_memo)
			*(baseDestName + baseDestLen) = '\0';
	}
	return EXIT_SUCCESS;
 exit_false:
	return EXIT_FAILURE;
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
