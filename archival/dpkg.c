/*
 *  Mini dpkg implementation for busybox.
 *  This is not meant as a replacemnt for dpkg
 *
 *  Written By Glenn McGrath with the help of others
 *  Copyright (C) 2001 by Glenn McGrath
 * 		
 *  Started life as a busybox implementation of udpkg
 *
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

/*
 * Known difference between busybox dpkg and the official dpkg that i dont
 * consider important, its worth keeping a note of differences anyway, just to
 * make it easier to maintain.
 *  - The first value for the Confflile: field isnt placed on a new line.
 *  - The <package>.control file is extracted and kept in the info dir.
 *  - When installing a package the Status: field is placed at the end of the
 *      section, rather than just after the Package: field.
 *  - Packages with previously unknown status are inserted at the begining of
 *      the status file
 *
 * Work to be done
 *  - (bugs, unknown, please let me know when you find them)
 *  - dependency checking is incomplete
 *  - provides, replaces, conflicts fields arent considered when installing
 */

#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../libbb/deb_functs.h"
#include "libbb.h"
#include "busybox.h"

enum dpkg_opt_e {
	DPKG_OPT_PURGE = 1,
	DPKG_OPT_REMOVE = 2,
	DPKG_OPT_UNPACK = 4,
	DPKG_OPT_CONFIGURE = 8,
	DPKG_OPT_INSTALL = 16,
	DPKG_OPT_PACKAGENAME = 32,
	DPKG_OPT_FILENAME = 64,
	DPKG_OPT_LIST_INSTALLED = 128,
	DPKG_OPT_FORCE_IGNORE_DEPENDS = 256,
};
extern unsigned short char_name_hash_prime;
extern unsigned short char_ver_hash_prime;
extern unsigned short char_rev_hash_prime;
extern unsigned short char_filename_hash_prime;
extern unsigned short char_source_hash_prime;
extern unsigned short edge_hash_prime;
extern unsigned short node_hash_prime;

/* This function lists information on the installed packages. It loops through
 * the status_hashtable to retrieve the info. This results in smaller code than
 * scanning the status file. The resulting list, however, is unsorted. 
 */
/* TODO: check the character flags ive assigned match real dpkg */
#ifdef BB_FEATURE_DPKG_LIST
extern void list_packages(void)
{
	int i;

	printf("    Name           Version\n");
	printf("+++-==============-==============\n");
	
	/* go through status hash, dereference package hash and finally strings */
	for (i = 0; i < node_hash_prime; i++) {
		node_t *node = get_node_ht(i);
		if (node != NULL) {
			char *name_str = get_name_ht(node->name);
			char *ver_rev;
			char want_flag;
			char status_flag;

			ver_rev = version_revision(node->version, node->revision);
			if (strlen(ver_rev) > 14) {
				ver_rev[14] = '\0';
			}

			switch (node->state_want) {
				case(STATE_WANT_HOLD):
					want_flag = 'h';
					break;
				case(STATE_WANT_INSTALL):
					want_flag = 'i';
					break;
				case(STATE_WANT_DEINSTALL):
					want_flag = 'r';
					break;
				case(STATE_WANT_PURGE):
					want_flag = 'p';
					break;
				default:
					want_flag = '?';
			}
			switch(node->state_status) {
				case(STATE_STATUS_NOTINSTALLED):
					status_flag = 'n';
					break;
				case(STATE_STATUS_UNPACKED):
					status_flag = 'u';
					break;
				case(STATE_STATUS_HALFCONFIGURED):
					status_flag = 'h';
					break;
				case(STATE_STATUS_INSTALLED):
					status_flag = 'i';
					break;
				case(STATE_STATUS_HALFINSTALLED):
					status_flag = 'h';
					break;
				case(STATE_STATUS_CONFIGFILES):
					status_flag = 'c';
					break;
				default:
					status_flag = '?';
			}

			/* print out the line formatted like Debian dpkg */
			if ((status_flag != '?') && (want_flag != '?')) {
#ifdef BB_FEATURE_DPKG_LIST_SHORT_DESCRIPTIONS
				printf("%c%c  %-14s %-14s %s\n", want_flag, status_flag, name_str, ver_rev, node->description_short);
#else
				printf("%c%c  %-14s %-14s\n", want_flag, status_flag, name_str, ver_rev);
#endif
			}
			free(ver_rev);
		}
    }
}
#endif

extern int dpkg_main(int argc, char **argv)
{
	deb_file_t **deb_file = xcalloc(1, sizeof(deb_file_t *));
	char opt = 0;
	int dpkg_opt = 0;
	int deb_count = 0;
	int i;
	unsigned short state_default_new_want = STATE_WANT_INSTALL;
	unsigned short state_default_new_flag = STATE_FLAG_REINSTREQ;
	unsigned short state_default_new_status = STATE_STATUS_NOTINSTALLED;

	while ((opt = getopt(argc, argv, 
#ifdef BB_FEATURE_DPKG_LIST
	"CF:ilPru")) != -1) {
#else
	"CF:iPru")) != -1) {
#endif
		switch (opt) {
			case 'C': // equivalent to --configure in official dpkg
				dpkg_opt |= DPKG_OPT_CONFIGURE | DPKG_OPT_PACKAGENAME;
				break;
			case 'F': // equivalent to --force in official dpkg
				if (strcmp(optarg, "depends") == 0) {
					dpkg_opt |= DPKG_OPT_FORCE_IGNORE_DEPENDS;
				}				
			case 'i':
				dpkg_opt |= DPKG_OPT_INSTALL | DPKG_OPT_FILENAME;
				break;
#ifdef BB_FEATURE_DPKG_LIST
			case 'l':
				dpkg_opt |= DPKG_OPT_LIST_INSTALLED;
				break;
#endif
			case 'P':
				dpkg_opt |= DPKG_OPT_PURGE | DPKG_OPT_PACKAGENAME;
				state_default_new_want = STATE_WANT_PURGE;
				break;
			case 'r':
				dpkg_opt |= DPKG_OPT_REMOVE | DPKG_OPT_PACKAGENAME;
				state_default_new_want = STATE_WANT_DEINSTALL;
				break;
			case 'u':	/* Equivalent to --unpack in official dpkg */
				dpkg_opt |= DPKG_OPT_UNPACK | DPKG_OPT_FILENAME;
				break;
			default:
				show_usage();
		}
	}

	/* check for non-otion argument if expected  */
	if ((dpkg_opt == 0) || ((dpkg_opt & DPKG_OPT_FILENAME) && (dpkg_opt & DPKG_OPT_PACKAGENAME))) {
		show_usage();
	}

	deb_initialise_hashtables(1);
	index_status_file("/var/lib/dpkg/status");

#ifdef BB_FEATURE_DPKG_LIST
	/* if the list action was given print the installed packages and exit */
	if (dpkg_opt & DPKG_OPT_LIST_INSTALLED) {
		list_packages();
	}
#endif

	/* Read arguments and store relevant info in structs */
	while (optind < argc) {
		const char *arg_name = argv[optind];
		unsigned int node_num;
		deb_file_t *tmp;
		node_t *node;

		tmp = (deb_file_t *) xcalloc(sizeof(deb_file_t), 1);

		if (dpkg_opt & DPKG_OPT_FILENAME) {
			tmp->filename = xstrdup(arg_name);
			/* Extract the control file */
			tmp->control_file = deb_extract(arg_name, stdout, (extract_control_tar_gz | extract_one_to_buffer), NULL, "./control");
			if (tmp->control_file == NULL) {
				error_msg("Couldnt extract control file from, ignoring package %s", arg_name);
				optind++;
				free(tmp->filename);
				free(tmp);
				continue;
			}
			node = parse_package_metadata(tmp->control_file);

			if (node == NULL) {
				error_msg_and_die("node was null\n");
			} else {
				node_num = search_node_ht(node, CMP_ANY);
				node->state_want = state_default_new_want;
				node->state_flag = state_default_new_flag;
				node->state_status = state_default_new_status;

				if (get_node_ht(node_num) == NULL) {
					printf("Selecting previously deselected package %s\n", get_name_ht(node->name));
				}
				add_node(node, node_num);
			}
		} else {
			/* It must be a package name */
			node = initialise_node(arg_name);
			node_num = search_node_ht(node, CMP_ANY);
			free_node(node);

			node = get_node_ht(node_num);
			if (node == NULL) {
				error_msg_and_die("Package %s is uninstalled or unknown\n", arg_name);
			}

			node->state_want = state_default_new_want;

			/* check package status is "installed" */
			if (dpkg_opt & DPKG_OPT_REMOVE) {
				if ((node->state_status == STATE_STATUS_NOTINSTALLED) ||
					(node->state_status == STATE_STATUS_CONFIGFILES)) {
					error_msg_and_die("%s is already removed.", arg_name);
				}
			}
			else if (dpkg_opt & DPKG_OPT_PURGE) {
				if (node->state_status == STATE_STATUS_NOTINSTALLED) { 
					error_msg_and_die("%s is already purged.", arg_name);
				}
			}
		}

		tmp->node = node_num;
		deb_file = realloc(deb_file, sizeof(deb_file_t) * (deb_count + 1));
		deb_file[deb_count] = tmp;

		deb_count++;
		deb_file[deb_count] = NULL;

		optind++;
	}

#if 0
	/* Check that the deb file arguments are installable */
	/* TODO: check dependencies before removing */
	if ((dpkg_opt & dpkg_opt_force_ignore_depends) != dpkg_opt_force_ignore_depends) {
		/* */
		for (i = 0; i < deb_count; i++) {
			/* add filename to list */
		}

		/* Sort packages into groups and add dependencies */
		find_deps(package_name);

		/* Check all packages in groups and sub groups are installed */
	}
#endif
	for (i = 0; i < deb_count; i++) {
		/* Remove or purge packages */
		if (dpkg_opt & DPKG_OPT_REMOVE) {
			remove_package(deb_file[i]->node);
		}
		else if (dpkg_opt & DPKG_OPT_PURGE) {
			purge_package(deb_file[i]->node);
		}
		else if (dpkg_opt & DPKG_OPT_UNPACK) {
			unpack_package(deb_file[i]);
		}
		else if (dpkg_opt & DPKG_OPT_INSTALL) {
			unpack_package(deb_file[i]);
			configure_package(deb_file[i]);
		}
		else if (dpkg_opt & DPKG_OPT_CONFIGURE) {
			configure_package(deb_file[i]);
		}
	}

	write_status_file(deb_file);

	if (deb_file != NULL) {
		for (i = 0; i < deb_count; i++) {
			free(deb_file[i]->control_file);
			free(deb_file[i]->filename);
			free(deb_file[i]);
		}
		free(deb_file);
	}
	free_hashtables();

	return(EXIT_SUCCESS);
}

