/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) tons of folks.  Tracking down who wrote what
 * isn't something I'm going to worry about...  If you wrote something
 * here, please feel free to acknowledge your work.
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

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include "libbb.h"

/* From gunzip.c */
extern int gz_open(FILE *compressed_file, int *pid);
extern void gz_close(int gunzip_pid);

extern int tar_unzip_init(int tarFd);
extern int readTarFile(int tarFd, int extractFlag, int listFlag, 
	int tostdoutFlag, int verboseFlag, char** extractList, char** excludeList);

extern int deb_extract(int optflags, const char *dir_name, const char *deb_filename)
{
	const int dpkg_deb_contents = 1;
	const int dpkg_deb_control = 2;
//	const int dpkg_deb_info = 4;
	const int dpkg_deb_extract = 8;
	const int dpkg_deb_verbose_extract = 16;
	const int dpkg_deb_list = 32;

	char **extract_list = NULL;
	ar_headers_t *ar_headers = NULL;
	char ar_filename[15];
	int extract_flag = FALSE;
	int list_flag = FALSE;
	int verbose_flag = FALSE;
	int extract_to_stdout = FALSE;
	int srcFd = 0;
	int status;
	pid_t pid;
	FILE *comp_file = NULL;

	if (dpkg_deb_contents == (dpkg_deb_contents & optflags)) {
		strcpy(ar_filename, "data.tar.gz");
		verbose_flag = TRUE;
		list_flag = TRUE;
	}
	if (dpkg_deb_list == (dpkg_deb_list & optflags)) {
		strcpy(ar_filename, "data.tar.gz");
		list_flag = TRUE;
	}
	if (dpkg_deb_control == (dpkg_deb_control & optflags)) {
		strcpy(ar_filename, "control.tar.gz");	
		extract_flag = TRUE;
	}
	if (dpkg_deb_extract == (dpkg_deb_extract & optflags)) {
		strcpy(ar_filename, "data.tar.gz");
		extract_flag = TRUE;
	}
	if (dpkg_deb_verbose_extract == (dpkg_deb_verbose_extract & optflags)) {
		strcpy(ar_filename, "data.tar.gz");
		extract_flag = TRUE;
		list_flag = TRUE;
	}

	ar_headers = (ar_headers_t *) xmalloc(sizeof(ar_headers_t));	
	srcFd = open(deb_filename, O_RDONLY);
	
	*ar_headers = get_ar_headers(srcFd);
	if (ar_headers->next == NULL) {
		error_msg_and_die("Couldnt find %s in %s", ar_filename, deb_filename);
	}

	while (ar_headers->next != NULL) {
		if (strcmp(ar_headers->name, ar_filename) == 0) {
			break;
		}
		ar_headers = ar_headers->next;
	}
	lseek(srcFd, ar_headers->offset, SEEK_SET);
	/* Uncompress the file */
	comp_file = fdopen(srcFd, "r");
	if ((srcFd = gz_open(comp_file, &pid)) == EXIT_FAILURE) {
	    error_msg_and_die("Couldnt unzip file");
	}
	if ( dir_name != NULL) { 
		if (is_directory(dir_name, TRUE, NULL)==FALSE) {
			mkdir(dir_name, 0755);
		}
		if (chdir(dir_name)==-1) {
			error_msg_and_die("Cannot change to dir %s", dir_name);
		}
	}
	status = readTarFile(srcFd, extract_flag, list_flag, 
		extract_to_stdout, verbose_flag, NULL, extract_list);

	/* we are deliberately terminating the child so we can safely ignore this */
	signal(SIGTERM, SIG_IGN);
	gz_close(pid);
	close(srcFd);
	fclose(comp_file);

	return status;
}