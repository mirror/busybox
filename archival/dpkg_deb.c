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
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include "unarchive.h"
#include "busybox.h"

extern int dpkg_deb_main(int argc, char **argv)
{
	archive_handle_t *ar_archive;
	archive_handle_t *tar_archive;
	int opt = 0;
#ifndef CONFIG_FEATURE_DPKG_DEB_EXTRACT_ONLY
	llist_t *control_tar_llist = NULL;
#endif
	
	/* Setup the tar archive handle */
	tar_archive = init_handle();

	/* Setup an ar archive handle that refers to the gzip sub archive */	
	ar_archive = init_handle();
	ar_archive->sub_archive = tar_archive;
	ar_archive->filter = filter_accept_list_reassign;

#ifdef CONFIG_FEATURE_DEB_TAR_GZ
	ar_archive->accept = llist_add_to(NULL, "data.tar.gz");
# ifndef CONFIG_FEATURE_DPKG_DEB_EXTRACT_ONLY
	control_tar_llist = llist_add_to(NULL, "control.tar.gz");
# endif
#endif

#ifdef CONFIG_FEATURE_DEB_TAR_BZ2
	ar_archive->accept = llist_add_to(ar_archive->accept, "data.tar.bz2");
# ifndef CONFIG_FEATURE_DPKG_DEB_EXTRACT_ONLY
	control_tar_llist = llist_add_to(control_tar_llist, "control.tar.bz2");
# endif
#endif

#ifndef CONFIG_FEATURE_DPKG_DEB_EXTRACT_ONLY
	while ((opt = getopt(argc, argv, "cefXx")) != -1) {
#else
	while ((opt = getopt(argc, argv, "x")) != -1) {
#endif
		switch (opt) {
#ifndef CONFIG_FEATURE_DPKG_DEB_EXTRACT_ONLY
			case 'c':
				tar_archive->action_header = header_verbose_list;
				break;
			case 'e':
				ar_archive->accept = control_tar_llist;
				tar_archive->action_data = data_extract_all;
				break;
			case 'f':
				/* Print the entire control file
				 * it should accept a second argument which specifies a 
				 * specific field to print */
				ar_archive->accept = control_tar_llist;
				tar_archive->accept = llist_add_to(NULL, "./control");;
				tar_archive->filter = filter_accept_list;
				tar_archive->action_data = data_extract_to_stdout;
				break;
			case 'X':
				tar_archive->action_header = header_list;
#endif
			case 'x':
				tar_archive->action_data = data_extract_all;
				break;
			default:
				bb_show_usage();
		}
	}

	if (optind + 2 < argc)  {
		bb_show_usage();
	}

	tar_archive->src_fd = ar_archive->src_fd = bb_xopen(argv[optind++], O_RDONLY);

	/* Workout where to extract the files */
	/* 2nd argument is a dir name */
	mkdir(argv[optind], 0777);
	chdir(argv[optind]);

	unpack_ar_archive(ar_archive);

	/* Cleanup */
	close (ar_archive->src_fd);

	return(EXIT_SUCCESS);
}

