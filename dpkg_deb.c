/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "busybox.h"

typedef struct ar_headers_s {
	char *name;
	size_t size;
	uid_t uid;
	gid_t gid;
	mode_t mode;
	time_t mtime;
	off_t offset;
	struct ar_headers_s *next;
} ar_headers_t;

extern ar_headers_t get_headers(int srcFd);
extern int tar_unzip_init(int tarFd);
extern int readTarFile(int tarFd, int extractFlag, int listFlag, 
		int tostdoutFlag, int verboseFlag, char** extractList,
		char** excludeList);

extern int dpkg_deb_main(int argc, char **argv)
{
	const int dpkg_deb_contents = 1;
	const int dpkg_deb_control = 2;
	const int dpkg_deb_info = 4;
	const int dpkg_deb_extract = 8;
	const int dpkg_deb_verbose_extract = 16;
	int opt=0;
	int optflag=0;	
	int extract_flag = FALSE;
	int list_flag = FALSE;
	int verbose_flag = FALSE;
	int extract_to_stdout = FALSE;
	char ar_filename[15];
	int srcFd=0;
	int status=0;
	ar_headers_t *ar_headers = NULL;
	char **extract_list=NULL;
	char *target_dir=NULL;
	
	while ((opt = getopt(argc, argv, "cexX")) != -1) {
		switch (opt) {
			case 'c':
				optflag |= dpkg_deb_contents;
				break;
			case 'e':
				optflag |= dpkg_deb_control;
				break;
/*			case 'I':
				optflag |= dpkg_deb_info;
				break;
*/
			case 'x':
				optflag |= dpkg_deb_extract;
				break;
			case 'X':
				optflag |= dpkg_deb_verbose_extract;
				break;
			default:
				usage(dpkg_deb_usage);
				return EXIT_FAILURE;
		}
	}

	if (((optind + 1 ) > argc) || (optflag == 0))  {
		usage(dpkg_deb_usage);
		return(EXIT_FAILURE);
	}
	
	if (optflag & dpkg_deb_contents) {
		list_flag = TRUE;
		verbose_flag = TRUE;
		strcpy(ar_filename, "data.tar.gz");
	}
	else if (optflag & dpkg_deb_control) {
		extract_flag = TRUE;
		strcpy(ar_filename, "control.tar.gz");		
                if ( (optind + 1) == argc ) {
                        target_dir = (char *) xmalloc(7);
                        strcpy(target_dir, "DEBIAN");
               }
                else {
                        target_dir = (char *) xmalloc(strlen(argv[optind+1]));

                        strcpy(target_dir, argv[optind+1]);
                }
	}
/*	else if (optflag & dpkg_deb_info) {
		extract_flag = TRUE;
		extract_to_stdout = TRUE;
		strcpy(ar_filename, "control.tar.gz");
		extract_list = argv+optind+1;
		printf("list one is [%s]\n",extract_list[0]);
	}
*/
	else if (optflag & dpkg_deb_extract) {
		extract_flag = TRUE;
		strcpy(ar_filename, "data.tar.gz");
		if ( (optind + 2) > argc ) {
			error_msg_and_die("No directory specified\n");
		}
		target_dir = (char *) xmalloc(strlen(argv[optind+1]));			
		strcpy(target_dir, argv[optind+1]);
	}	
	else if (optflag & dpkg_deb_verbose_extract) {
		extract_flag = TRUE;
		list_flag = TRUE;
		strcpy(ar_filename, "data.tar.gz");
		if ( (optind + 2) > argc ) {
			error_msg_and_die("No directory specified\n");
		}
		target_dir = (char *) xmalloc(strlen(argv[optind+1]));			
		strcpy(target_dir, argv[optind+1]);
	}
		
	ar_headers = (ar_headers_t *) xmalloc(sizeof(ar_headers_t));	
	srcFd = open(argv[optind], O_RDONLY);
	
	*ar_headers = get_headers(srcFd);
	if (ar_headers->next==NULL)
		error_msg_and_die("Couldnt find %s in %s\n",ar_filename, argv[optind]);

	while (ar_headers->next != NULL) {
		if (strcmp(ar_headers->name, ar_filename)==0)
			break;
		ar_headers = ar_headers->next;
	}

	lseek(srcFd, ar_headers->offset, SEEK_SET);
	srcFd = tar_unzip_init(srcFd);
	if ( target_dir != NULL) { 
		if (is_directory(target_dir, TRUE, NULL)==FALSE) {
			mkdir(target_dir, 0755);
		}
		if (chdir(target_dir)==-1) {
			error_msg_and_die("Cannot change to dir %s\n",argv[optind+1]);
		}
	}
	status = readTarFile(srcFd, extract_flag, list_flag, extract_to_stdout, verbose_flag, NULL, extract_list);
	return(EXIT_SUCCESS);
}
