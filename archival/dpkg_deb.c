/* vi: set sw=4 ts=4: */
/*
 * dpkg-deb packs, unpacks and provides information about Debian archives.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
#include "busybox.h"
#include "unarchive.h"

#define DPKG_DEB_OPT_CONTENTS	1
#define DPKG_DEB_OPT_CONTROL	2
#define DPKG_DEB_OPT_FIELD	4
#define DPKG_DEB_OPT_EXTRACT	8
#define DPKG_DEB_OPT_EXTRACT_VERBOSE	16

int dpkg_deb_main(int argc, char **argv)
{
	archive_handle_t *ar_archive;
	archive_handle_t *tar_archive;
	llist_t *control_tar_llist = NULL;
	unsigned opt;
	char *extract_dir = NULL;
	short argcount = 1;

	/* Setup the tar archive handle */
	tar_archive = init_handle();

	/* Setup an ar archive handle that refers to the gzip sub archive */
	ar_archive = init_handle();
	ar_archive->sub_archive = tar_archive;
	ar_archive->filter = filter_accept_list_reassign;

#ifdef CONFIG_FEATURE_DEB_TAR_GZ
	llist_add_to(&(ar_archive->accept), "data.tar.gz");
	llist_add_to(&control_tar_llist, "control.tar.gz");
#endif

#ifdef CONFIG_FEATURE_DEB_TAR_BZ2
	llist_add_to(&(ar_archive->accept), "data.tar.bz2");
	llist_add_to(&control_tar_llist, "control.tar.bz2");
#endif

	opt_complementary = "?c--efXx:e--cfXx:f--ceXx:X--cefx:x--cefX";
	opt = getopt32(argc, argv, "cefXx");

	if (opt & DPKG_DEB_OPT_CONTENTS) {
		tar_archive->action_header = header_verbose_list;
	}
	if (opt & DPKG_DEB_OPT_CONTROL) {
		ar_archive->accept = control_tar_llist;
		tar_archive->action_data = data_extract_all;
		if (optind + 1 == argc) {
			extract_dir = "./DEBIAN";
		} else {
			argcount++;
		}
	}
	if (opt & DPKG_DEB_OPT_FIELD) {
		/* Print the entire control file
		 * it should accept a second argument which specifies a
		 * specific field to print */
		ar_archive->accept = control_tar_llist;
		llist_add_to(&(tar_archive->accept), "./control");
		tar_archive->filter = filter_accept_list;
		tar_archive->action_data = data_extract_to_stdout;
	}
	if (opt & DPKG_DEB_OPT_EXTRACT) {
		tar_archive->action_header = header_list;
	}
	if (opt & (DPKG_DEB_OPT_EXTRACT_VERBOSE | DPKG_DEB_OPT_EXTRACT)) {
		tar_archive->action_data = data_extract_all;
		argcount = 2;
	}

	if ((optind + argcount) != argc) {
		bb_show_usage();
	}

	tar_archive->src_fd = ar_archive->src_fd = xopen(argv[optind++], O_RDONLY);

	/* Workout where to extract the files */
	/* 2nd argument is a dir name */
	if (argv[optind]) {
		extract_dir = argv[optind];
	}
	if (extract_dir) {
		mkdir(extract_dir, 0777); /* bb_make_directory(extract_dir, 0777, 0) */
		xchdir(extract_dir);
	}
	unpack_ar_archive(ar_archive);

	/* Cleanup */
	close(ar_archive->src_fd);

	return EXIT_SUCCESS;
}
