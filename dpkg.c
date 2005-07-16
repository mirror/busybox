/*
 *  Mini dpkg implementation for busybox.
 *  This is not meant as a replacemnt for dpkg
 *
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
 * Bugs that need to be fixed
 *  - (unknown, please let me know when you find any)
 *
 */

#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "busybox.h"

/* NOTE: If you vary HASH_PRIME sizes be aware,
 * 1) Tweaking these will have a big effect on how much memory this program uses.
 * 2) For computational efficiency these hash tables should be at least 20%
 *    larger than the maximum number of elements stored in it.
 * 3) All _HASH_PRIME's must be a prime number or chaos is assured, if your looking
 *    for a prime, try http://www.utm.edu/research/primes/lists/small/10000.txt
 * 4) If you go bigger than 15 bits you may get into trouble (untested) as its
 *    sometimes cast to an unsigned int, if you go to 16 bit you will overlap
 *    int's and chaos is assured, 16381 is the max prime for 14 bit field
 */

/* NAME_HASH_PRIME, Stores package names and versions, 
 * I estimate it should be at least 50% bigger than PACKAGE_HASH_PRIME,
 * as there a lot of duplicate version numbers */
#define NAME_HASH_PRIME 16381
char *name_hashtable[NAME_HASH_PRIME + 1];

/* PACKAGE_HASH_PRIME, Maximum number of unique packages,
 * It must not be smaller than STATUS_HASH_PRIME,
 * Currently only packages from status_hashtable are stored in here, but in
 * future this may be used to store packages not only from a status file,
 * but an available_hashtable, and even multiple packages files.
 * Package can be stored more than once if they have different versions.
 * e.g. The same package may have different versions in the status file
 *      and available file */
#define PACKAGE_HASH_PRIME 10007
typedef struct edge_s {
	unsigned int operator:3;
	unsigned int type:4;
	unsigned int name:14;
	unsigned int version:14;
} edge_t;

typedef struct common_node_s {
	unsigned int name:14;
	unsigned int version:14;
	unsigned int num_of_edges:14;
	edge_t **edge;
} common_node_t;
common_node_t *package_hashtable[PACKAGE_HASH_PRIME + 1];

/* Currently it doesnt store packages that have state-status of not-installed
 * So it only really has to be the size of the maximum number of packages
 * likely to be installed at any one time, so there is a bit of leaway here */
#define STATUS_HASH_PRIME 8191
typedef struct status_node_s {
	unsigned int package:14;	/* has to fit PACKAGE_HASH_PRIME */
	unsigned int status:14;		/* has to fit STATUS_HASH_PRIME */
} status_node_t;
status_node_t *status_hashtable[STATUS_HASH_PRIME + 1];

/* Even numbers are for 'extras', like ored dependecies or null */
enum edge_type_e {
	EDGE_NULL = 0,
	EDGE_PRE_DEPENDS = 1,
	EDGE_OR_PRE_DEPENDS = 2,
	EDGE_DEPENDS = 3,
	EDGE_OR_DEPENDS = 4,
	EDGE_REPLACES = 5,
	EDGE_PROVIDES = 7,
	EDGE_CONFLICTS = 9,
	EDGE_SUGGESTS = 11,
	EDGE_RECOMMENDS = 13,
	EDGE_ENHANCES = 15
};
enum operator_e {
	VER_NULL = 0,
	VER_EQUAL = 1,
	VER_LESS = 2,
	VER_LESS_EQUAL = 3,
	VER_MORE = 4,
	VER_MORE_EQUAL = 5,
	VER_ANY = 6
};

enum dpkg_opt_e {
	dpkg_opt_purge = 1,
	dpkg_opt_remove = 2,
	dpkg_opt_unpack = 4,
	dpkg_opt_configure = 8,
	dpkg_opt_install = 16,
	dpkg_opt_package_name = 32,
	dpkg_opt_filename = 64,
	dpkg_opt_list_installed = 128,
	dpkg_opt_force_ignore_depends = 256
};

typedef struct deb_file_s {
	char *control_file;
	char *filename;
	unsigned int package:14;
} deb_file_t;


void make_hash(const char *key, unsigned int *start, unsigned int *decrement, const int hash_prime)
{
	unsigned long int hash_num = key[0];
	int len = strlen(key);
	int i;

	/* Maybe i should have uses a "proper" hashing algorithm here instead
	 * of making one up myself, seems to be working ok though. */
	for(i = 1; i < len; i++) {
		/* shifts the ascii based value and adds it to previous value
		 * shift amount is mod 24 because long int is 32 bit and data
		 * to be shifted is 8, dont want to shift data to where it has
		 * no effect*/
		hash_num += ((key[i] + key[i-1]) << ((key[i] * i) % 24)); 
	}
	*start = (unsigned int) hash_num % hash_prime;
	*decrement = (unsigned int) 1 + (hash_num % (hash_prime - 1));
}

/* this adds the key to the hash table */
int search_name_hashtable(const char *key)
{
	unsigned int probe_address = 0;
	unsigned int probe_decrement = 0;
//	char *temp;

	make_hash(key, &probe_address, &probe_decrement, NAME_HASH_PRIME);
	while(name_hashtable[probe_address] != NULL) {
		if (strcmp(name_hashtable[probe_address], key) == 0) {
			return(probe_address);
		} else {
			probe_address -= probe_decrement;
			if ((int)probe_address < 0) {
				probe_address += NAME_HASH_PRIME;
			}
		}
	}
	name_hashtable[probe_address] = xstrdup(key);
	return(probe_address);
}

/* this DOESNT add the key to the hashtable
 * TODO make it consistent with search_name_hashtable
 */
unsigned int search_status_hashtable(const char *key)
{
	unsigned int probe_address = 0;
	unsigned int probe_decrement = 0;

	make_hash(key, &probe_address, &probe_decrement, STATUS_HASH_PRIME);
	while(status_hashtable[probe_address] != NULL) {
		if (strcmp(key, name_hashtable[package_hashtable[status_hashtable[probe_address]->package]->name]) == 0) {
			break;
		} else {
			probe_address -= probe_decrement;
			if ((int)probe_address < 0) {
				probe_address += STATUS_HASH_PRIME;
			}
		}
	}
	return(probe_address);
}

/* Need to rethink version comparison, maybe the official dpkg has something i can use ? */
int version_compare_part(const char *version1, const char *version2)
{
	int upstream_len1 = 0;
	int upstream_len2 = 0;
	char *name1_char;
	char *name2_char;
	int len1 = 0;
	int len2 = 0;
	int tmp_int;
	int ver_num1;
	int ver_num2;
	int ret;

	if (version1 == NULL) {
		version1 = xstrdup("");
	}
	if (version2 != NULL) {
		version2 = xstrdup("");
	}
	upstream_len1 = strlen(version1);
	upstream_len2 = strlen(version2);

	while ((len1 < upstream_len1) || (len2 < upstream_len2)) {
		/* Compare non-digit section */
		tmp_int = strcspn(&version1[len1], "0123456789");
		name1_char = xstrndup(&version1[len1], tmp_int);
		len1 += tmp_int;
		tmp_int = strcspn(&version2[len2], "0123456789");
		name2_char = xstrndup(&version2[len2], tmp_int);
		len2 += tmp_int;
		tmp_int = strcmp(name1_char, name2_char);
		free(name1_char);
		free(name2_char);
		if (tmp_int != 0) {
			ret = tmp_int;
			goto cleanup_version_compare_part;
		}

		/* Compare digits */
		tmp_int = strspn(&version1[len1], "0123456789");
		name1_char = xstrndup(&version1[len1], tmp_int);
		len1 += tmp_int;
		tmp_int = strspn(&version2[len2], "0123456789");
		name2_char = xstrndup(&version2[len2], tmp_int);
		len2 += tmp_int;
		ver_num1 = atoi(name1_char);
		ver_num2 = atoi(name2_char);
		free(name1_char);
		free(name2_char);
		if (ver_num1 < ver_num2) {
			ret = -1;
			goto cleanup_version_compare_part;
		}
		else if (ver_num1 > ver_num2) {
			ret = 1;
			goto cleanup_version_compare_part;
		}
	}
	ret = 0;
cleanup_version_compare_part:
	return(ret);
}

/* if ver1 < ver2 return -1,
 * if ver1 = ver2 return 0,
 * if ver1 > ver2 return 1,
 */
int version_compare(const unsigned int ver1, const unsigned int ver2)
{
	char *ch_ver1 = name_hashtable[ver1];
	char *ch_ver2 = name_hashtable[ver2];

	char epoch1, epoch2;
	char *deb_ver1, *deb_ver2;
	char *ver1_ptr, *ver2_ptr;
	char *upstream_ver1;
	char *upstream_ver2;
	int result;

	/* Compare epoch */
	if (ch_ver1[1] == ':') {
		epoch1 = ch_ver1[0];
		ver1_ptr = strchr(ch_ver1, ':') + 1;
	} else {
		epoch1 = '0';
		ver1_ptr = ch_ver1;
	}
	if (ch_ver2[1] == ':') {
		epoch2 = ch_ver2[0];
		ver2_ptr = strchr(ch_ver2, ':') + 1;
	} else {
		epoch2 = '0';
		ver2_ptr = ch_ver2;
	}
	if (epoch1 < epoch2) {
		return(-1);
	}
	else if (epoch1 > epoch2) {
		return(1);
	}

	/* Compare upstream version */
	upstream_ver1 = xstrdup(ver1_ptr);
	upstream_ver2 = xstrdup(ver2_ptr);

	/* Chop off debian version, and store for later use */
	deb_ver1 = strrchr(upstream_ver1, '-');
	deb_ver2 = strrchr(upstream_ver2, '-');
	if (deb_ver1) {
		deb_ver1[0] = '\0';
		deb_ver1++;
	}
	if (deb_ver2) {
		deb_ver2[0] = '\0';
		deb_ver2++;
	}
	result = version_compare_part(upstream_ver1, upstream_ver2);

	free(upstream_ver1);
	free(upstream_ver2);

	if (result != 0) {
		return(result);
	}

	/* Compare debian versions */
	return(version_compare_part(deb_ver1, deb_ver2));
}

int test_version(const unsigned int version1, const unsigned int version2, const unsigned int operator)
{
	const int version_result = version_compare(version1, version2);
	switch(operator) {
		case (VER_ANY):
			return(TRUE);
		case (VER_EQUAL):
			if (version_result == 0) {
				return(TRUE);
			}
			break;
		case (VER_LESS):
			if (version_result < 0) {
				return(TRUE);
			}
			break;
		case (VER_LESS_EQUAL):
			if (version_result <= 0) {
				return(TRUE);
			}
			break;
		case (VER_MORE):
			if (version_result > 0) {
				return(TRUE);
			}
			break;
		case (VER_MORE_EQUAL):
			if (version_result >= 0) {
				return(TRUE);
			}
			break;
	}
	return(FALSE);
}


int search_package_hashtable(const unsigned int name, const unsigned int version, const unsigned int operator)
{
	unsigned int probe_address = 0;
	unsigned int probe_decrement = 0;

	make_hash(name_hashtable[name], &probe_address, &probe_decrement, PACKAGE_HASH_PRIME);
	while(package_hashtable[probe_address] != NULL) {
		if (package_hashtable[probe_address]->name == name) {
			if (operator == VER_ANY) {
				return(probe_address);
			}
			if (test_version(package_hashtable[probe_address]->version, version, operator)) {
				return(probe_address);
			}
		}
		probe_address -= probe_decrement;
		if ((int)probe_address < 0) {
			probe_address += PACKAGE_HASH_PRIME;
		}
	}
	return(probe_address);
}

/*
 * Create one new node and one new edge for every dependency.
 */
void add_split_dependencies(common_node_t *parent_node, const char *whole_line, unsigned int edge_type)
{
	char *line = xstrdup(whole_line);
	char *line2;
	char *line_ptr1 = NULL;
	char *line_ptr2 = NULL;
	char *field;
	char *field2;
	char *version;
	edge_t *edge;
	int offset_ch;
	int type;

	field = strtok_r(line, ",", &line_ptr1);
	do {
		line2 = xstrdup(field);
		field2 = strtok_r(line2, "|", &line_ptr2);
		if ((edge_type == EDGE_DEPENDS) && (strcmp(field, field2) != 0)) {
			type = EDGE_OR_DEPENDS;
		}
		else if ((edge_type == EDGE_PRE_DEPENDS) && (strcmp(field, field2) != 0)) {
			type = EDGE_OR_PRE_DEPENDS;
		} else {
			type = edge_type;
		}

		do {
			edge = (edge_t *) xmalloc(sizeof(edge_t));
			edge->type = type;

			/* Skip any extra leading spaces */
			field2 += strspn(field2, " ");

			/* Get dependency version info */
			version = strchr(field2, '(');
			if (version == NULL) {
				edge->operator = VER_ANY;
				/* Get the versions hash number, adding it if the number isnt already in there */
				edge->version = search_name_hashtable("ANY");
			} else {
				/* Skip leading ' ' or '(' */
				version += strspn(field2, " ");
				version += strspn(version, "(");
				/* Calculate length of any operator charactors */
				offset_ch = strspn(version, "<=>");
				/* Determine operator */
				if (offset_ch > 0) {
					if (strncmp(version, "=", offset_ch) == 0) {
						edge->operator = VER_EQUAL;
					}
					else if (strncmp(version, "<<", offset_ch) == 0) {
						edge->operator = VER_LESS;
					}
					else if (strncmp(version, "<=", offset_ch) == 0) {
						edge->operator = VER_LESS_EQUAL;
					}
					else if (strncmp(version, ">>", offset_ch) == 0) {
						edge->operator = VER_MORE;
					}
					else if (strncmp(version, ">=", offset_ch) == 0) {
						edge->operator = VER_MORE_EQUAL;
					} else {
						error_msg_and_die("Illegal operator\n");
					}
				}
				/* skip to start of version numbers */
				version += offset_ch;
				version += strspn(version, " ");

				/* Truncate version at trailing ' ' or ')' */
				version[strcspn(version, " )")] = '\0';
				/* Get the versions hash number, adding it if the number isnt already in there */
				edge->version = search_name_hashtable(version);
			}

			/* Get the dependency name */
			field2[strcspn(field2, " (")] = '\0';
			edge->name = search_name_hashtable(field2);
	
			/* link the new edge to the current node */
			parent_node->num_of_edges++;
			parent_node->edge = xrealloc(parent_node->edge, sizeof(edge_t) * (parent_node->num_of_edges + 1));
			parent_node->edge[parent_node->num_of_edges - 1] = edge;
		} while ((field2 = strtok_r(NULL, "|", &line_ptr2)) != NULL);
		free(line2);
	} while ((field = strtok_r(NULL, ",", &line_ptr1)) != NULL);
	free(line);

	return;
}

void free_package(common_node_t *node)
{
	int i;
	if (node != NULL) {
		for (i = 0; i < node->num_of_edges; i++) {
			if (node->edge[i] != NULL) {
				free(node->edge[i]);
			}
		}
		if (node->edge != NULL) {
			free(node->edge);
		}
		if (node != NULL) {
			free(node);
		}
	}
}

unsigned int fill_package_struct(char *control_buffer)
{
	common_node_t *new_node = (common_node_t *) xcalloc(1, sizeof(common_node_t));

	char **field_name = xmalloc(sizeof(char *));
	char **field_value = xmalloc(sizeof(char *));
	int field_start = 0;
	int num = -1;
	int buffer_length = strlen(control_buffer);

	new_node->version = search_name_hashtable("unknown");
	while (field_start < buffer_length) {
		field_start += read_package_field(&control_buffer[field_start], field_name, field_value);

		if (*field_name == NULL) {
			goto fill_package_struct_cleanup; // Oh no, the dreaded goto statement !!
		}

		if (strcmp(*field_name, "Package") == 0) {
			new_node->name = search_name_hashtable(*field_value);
		}
		else if (strcmp(*field_name, "Version") == 0) {
			new_node->version = search_name_hashtable(*field_value);
		}
		else if (strcmp(*field_name, "Pre-Depends") == 0) {
			add_split_dependencies(new_node, *field_value, EDGE_PRE_DEPENDS);
		}
		else if (strcmp(*field_name, "Depends") == 0) {
			add_split_dependencies(new_node, *field_value, EDGE_DEPENDS);
		}
		else if (strcmp(*field_name, "Replaces") == 0) {
			add_split_dependencies(new_node, *field_value, EDGE_REPLACES);
		}
		else if (strcmp(*field_name, "Provides") == 0) {
			add_split_dependencies(new_node, *field_value, EDGE_PROVIDES);
		}
		else if (strcmp(*field_name, "Conflicts") == 0) {
			add_split_dependencies(new_node, *field_value, EDGE_CONFLICTS);
		}
		else if (strcmp(*field_name, "Suggests") == 0) {
			add_split_dependencies(new_node, *field_value, EDGE_SUGGESTS);
		}
		else if (strcmp(*field_name, "Recommends") == 0) {
			add_split_dependencies(new_node, *field_value, EDGE_RECOMMENDS);
		}
		else if (strcmp(*field_name, "Enhances") == 0) {
			add_split_dependencies(new_node, *field_value, EDGE_ENHANCES);
		}
fill_package_struct_cleanup:
		if (*field_name) {
			free(*field_name);
		}
		if (*field_value) {
			free(*field_value);
		}
	}
	free(field_name);
	free(field_value);

	if (new_node->version == search_name_hashtable("unknown")) {
		free_package(new_node);
		return(-1);
	}
	num = search_package_hashtable(new_node->name, new_node->version, VER_EQUAL);
	if (package_hashtable[num] == NULL) {
		package_hashtable[num] = new_node;
	} else {
		free_package(new_node);
	}
	return(num);
}

/* if num = 1, it returns the want status, 2 returns flag, 3 returns status */
unsigned int get_status(const unsigned int status_node, const int num)
{
	char *status_string = name_hashtable[status_hashtable[status_node]->status];
	char *state_sub_string;
	unsigned int state_sub_num;
	int len;
	int i;

	/* set tmp_string to point to the start of the word number */
	for (i = 1; i < num; i++) {
		/* skip past a word */
		status_string += strcspn(status_string, " ");
		/* skip past the seperating spaces */
		status_string += strspn(status_string, " ");
	}
	len = strcspn(status_string, " \n\0");
	state_sub_string = xstrndup(status_string, len);
	state_sub_num = search_name_hashtable(state_sub_string);
	free(state_sub_string);
	return(state_sub_num);
}

void set_status(const unsigned int status_node_num, const char *new_value, const int position)
{
	const unsigned int new_value_len = strlen(new_value);
	const unsigned int new_value_num = search_name_hashtable(new_value);
	unsigned int want = get_status(status_node_num, 1);
	unsigned int flag = get_status(status_node_num, 2);
	unsigned int status = get_status(status_node_num, 3);
	int want_len = strlen(name_hashtable[want]);
	int flag_len = strlen(name_hashtable[flag]);
	int status_len = strlen(name_hashtable[status]);
	char *new_status;

	switch (position) {
		case (1):
			want = new_value_num;
			want_len = new_value_len;
			break;
		case (2):
			flag = new_value_num;
			flag_len = new_value_len;
			break;
		case (3):
			status = new_value_num;
			status_len = new_value_len;
			break;
		default:
			error_msg_and_die("DEBUG ONLY: this shouldnt happen");
	}

	new_status = (char *) xmalloc(want_len + flag_len + status_len + 3);
	sprintf(new_status, "%s %s %s", name_hashtable[want], name_hashtable[flag], name_hashtable[status]);
	status_hashtable[status_node_num]->status = search_name_hashtable(new_status);
	free(new_status);
	return;
}

void index_status_file(const char *filename)
{
	FILE *status_file;
	char *control_buffer;
	char *status_line;
	status_node_t *status_node = NULL;
	unsigned int status_num;

	status_file = xfopen(filename, "r");
	while ((control_buffer = fgets_str(status_file, "\n\n")) != NULL) {
		const unsigned int package_num = fill_package_struct(control_buffer);
		if (package_num != -1) {
			status_node = xmalloc(sizeof(status_node_t));
			/* fill_package_struct doesnt handle the status field */
			status_line = strstr(control_buffer, "Status:");
			if (status_line != NULL) {
				status_line += 7;
				status_line += strspn(status_line, " \n\t");
				status_line = xstrndup(status_line, strcspn(status_line, "\n\0"));
				status_node->status = search_name_hashtable(status_line);
				free(status_line);
			}
			status_node->package = package_num;
			status_num = search_status_hashtable(name_hashtable[package_hashtable[status_node->package]->name]);
			status_hashtable[status_num] = status_node;
		}
		free(control_buffer);
	}
	fclose(status_file);
	return;
}


char *get_depends_field(common_node_t *package, const int depends_type)
{
	char *depends = NULL;
	char *old_sep = (char *)xcalloc(1, 3);
	char *new_sep = (char *)xcalloc(1, 3);
	int line_size = 0;
	int depends_size;

	int i;

	for (i = 0; i < package->num_of_edges; i++) {
		if ((package->edge[i]->type == EDGE_OR_PRE_DEPENDS) ||
			(package->edge[i]->type == EDGE_OR_DEPENDS)) {
		}

		if ((package->edge[i]->type == depends_type) ||
			(package->edge[i]->type == depends_type + 1)) {
			/* Check if its the first time through */

			depends_size = 8 + strlen(name_hashtable[package->edge[i]->name])
				+ strlen(name_hashtable[package->edge[i]->version]);
			line_size += depends_size;
			depends = (char *) xrealloc(depends, line_size + 1);

			/* Check to see if this dependency is the type we are looking for
			 * +1 to check for 'extra' types, e.g. ored dependecies */
			strcpy(old_sep, new_sep);
			if (package->edge[i]->type == depends_type) {
				strcpy(new_sep, ", ");
			}
			else if (package->edge[i]->type == depends_type + 1) {
				strcpy(new_sep, "| ");
			}

			if (depends_size == line_size) {
				strcpy(depends, "");
			} else {
				if ((strcmp(old_sep, "| ") == 0) && (strcmp(new_sep, "| ") == 0)) {
					strcat(depends, " | ");
				} else {
					strcat(depends, ", ");
				}
			}

			strcat(depends, name_hashtable[package->edge[i]->name]);
			if (strcmp(name_hashtable[package->edge[i]->version], "NULL") != 0) {
				if (package->edge[i]->operator == VER_EQUAL) {
					strcat(depends, " (= ");
				}
				else if (package->edge[i]->operator == VER_LESS) {
					strcat(depends, " (<< ");
				}
				else if (package->edge[i]->operator == VER_LESS_EQUAL) {
					strcat(depends, " (<= ");
				}
				else if (package->edge[i]->operator == VER_MORE) {
					strcat(depends, " (>> ");
				}
				else if (package->edge[i]->operator == VER_MORE_EQUAL) {
					strcat(depends, " (>= ");
				} else {
					strcat(depends, " (");
				}
				strcat(depends, name_hashtable[package->edge[i]->version]);
				strcat(depends, ")");
			}
		}
	}
	return(depends);
}

void write_buffer_no_status(FILE *new_status_file, const char *control_buffer)
{
	char *name;
	char *value;
	int start = 0;
	while (1) {
		start += read_package_field(&control_buffer[start], &name, &value);
		if (name == NULL) {
			break;
		}
		if (strcmp(name, "Status") != 0) {
			fprintf(new_status_file, "%s: %s\n", name, value);
		}
	}
	return;
}

/* This could do with a cleanup */
void write_status_file(deb_file_t **deb_file)
{
	FILE *old_status_file = xfopen("/var/lib/dpkg/status", "r");
	FILE *new_status_file = xfopen("/var/lib/dpkg/status.udeb", "w");
	char *package_name;
	char *status_from_file;
	char *control_buffer = NULL;
	char *tmp_string;
	int status_num;
	int field_start = 0;
	int write_flag;
	int i = 0;

	/* Update previously known packages */
	while ((control_buffer = fgets_str(old_status_file, "\n\n")) != NULL) {
		tmp_string = strstr(control_buffer, "Package:") + 8;
 		tmp_string += strspn(tmp_string, " \n\t");
		package_name = xstrndup(tmp_string, strcspn(tmp_string, "\n\0"));
		write_flag = FALSE;
		tmp_string = strstr(control_buffer, "Status:");
		if (tmp_string != NULL) {
			/* Seperate the status value from the control buffer */
			tmp_string += 7;
			tmp_string += strspn(tmp_string, " \n\t");
			status_from_file = xstrndup(tmp_string, strcspn(tmp_string, "\n"));
		} else {
			status_from_file = NULL;
		}

		/* Find this package in the status hashtable */
		status_num = search_status_hashtable(package_name);
		if (status_hashtable[status_num] != NULL) {
			const char *status_from_hashtable = name_hashtable[status_hashtable[status_num]->status];
			if (strcmp(status_from_file, status_from_hashtable) != 0) {
				/* New status isnt exactly the same as old status */
				const int state_status = get_status(status_num, 3);
				if ((strcmp("installed", name_hashtable[state_status]) == 0) ||
					(strcmp("unpacked", name_hashtable[state_status]) == 0)) {
					/* We need to add the control file from the package */
					i = 0;
					while(deb_file[i] != NULL) {
						if (strcmp(package_name, name_hashtable[package_hashtable[deb_file[i]->package]->name]) == 0) {
							/* Write a status file entry with a modified status */
							/* remove trailing \n's */
							write_buffer_no_status(new_status_file, deb_file[i]->control_file);
							set_status(status_num, "ok", 2);
							fprintf(new_status_file, "Status: %s\n\n", name_hashtable[status_hashtable[status_num]->status]);
							write_flag = TRUE;
							break;
						}
						i++;
					}
					/* This is temperary, debugging only */
					if (deb_file[i] == NULL) {
						error_msg_and_die("ALERT: Couldnt find a control file, your status file may be broken, status may be incorrect for %s", package_name);
					}
				}
				else if (strcmp("not-installed", name_hashtable[state_status]) == 0) {
					/* Only write the Package, Status, Priority and Section lines */
					fprintf(new_status_file, "Package: %s\n", package_name);
					fprintf(new_status_file, "Status: %s\n", status_from_hashtable);

					while (1) {
						char *field_name;
						char *field_value;
						field_start += read_package_field(&control_buffer[field_start], &field_name, &field_value);
						if (field_name == NULL) {
							break;
						}
						if ((strcmp(field_name, "Priority") == 0) ||
							(strcmp(field_name, "Section") == 0)) {
							fprintf(new_status_file, "%s: %s\n", field_name, field_value);
						}
					}
					write_flag = TRUE;
					fputs("\n", new_status_file);
				}
				else if	(strcmp("config-files", name_hashtable[state_status]) == 0) {
					/* only change the status line */
					while (1) {
						char *field_name;
						char *field_value;
						field_start += read_package_field(&control_buffer[field_start], &field_name, &field_value);
						if (field_name == NULL) {
							break;
						}
						/* Setup start point for next field */
						if (strcmp(field_name, "Status") == 0) {
							fprintf(new_status_file, "Status: %s\n", status_from_hashtable);
						} else {
							fprintf(new_status_file, "%s: %s\n", field_name, field_value);
						}
					}
					write_flag = TRUE;
					fputs("\n", new_status_file);
				}
			}
		}
		/* If the package from the status file wasnt handle above, do it now*/
		if (write_flag == FALSE) {
			fprintf(new_status_file, "%s\n\n", control_buffer);
		}

		if (status_from_file != NULL) {
			free(status_from_file);
		}
		free(package_name);
		free(control_buffer);
	}

	/* Write any new packages */
	for(i = 0; deb_file[i] != NULL; i++) {
		status_num = search_status_hashtable(name_hashtable[package_hashtable[deb_file[i]->package]->name]);
		if (strcmp("reinstreq", name_hashtable[get_status(status_num, 2)]) == 0) {
			write_buffer_no_status(new_status_file, deb_file[i]->control_file);
			set_status(status_num, "ok", 2);
			fprintf(new_status_file, "Status: %s\n\n", name_hashtable[status_hashtable[status_num]->status]);
		}
	}
	fclose(old_status_file);
	fclose(new_status_file);


	/* Create a seperate backfile to dpkg */
	if (rename("/var/lib/dpkg/status", "/var/lib/dpkg/status.udeb.bak") == -1) {
		struct stat stat_buf;	
		if (stat("/var/lib/dpkg/status", &stat_buf) == 0) {
			error_msg_and_die("Couldnt create backup status file");
		}
		/* Its ok if renaming the status file fails becasue status 
		 * file doesnt exist, maybe we are starting from scratch */
		error_msg("No status file found, creating new one");
	}

	if (rename("/var/lib/dpkg/status.udeb", "/var/lib/dpkg/status") == -1) {
		error_msg_and_die("DANGER: Couldnt create status file, you need to manually repair your status file");
	}
}

int check_deps(deb_file_t **deb_file, int deb_start, int dep_max_count)
{
	int *conflicts = NULL;
	int conflicts_num = 0;
	int state_status;
	int state_flag;
	int state_want;
	unsigned int status_package_num;
	int i = deb_start;
	int j, k;

	/* Check for conflicts
	 * TODO: TEST if conflicts with other packages to be installed
	 *
	 * Add install packages and the packages they provide
	 * to the list of files to check conflicts for
	 */

	/* Create array of package numbers to check against
	 * installed package for conflicts*/
	while (deb_file[i] != NULL) {
		const unsigned int package_num = deb_file[i]->package;
		conflicts = xrealloc(conflicts, sizeof(int) * (conflicts_num + 1));
		conflicts[conflicts_num] = package_num;
		conflicts_num++;
		/* add provides to conflicts list */
		for (j = 0; j <	package_hashtable[package_num]->num_of_edges; j++) {
			if (package_hashtable[package_num]->edge[j]->type == EDGE_PROVIDES) {
				const int conflicts_package_num = search_package_hashtable(
					package_hashtable[package_num]->edge[j]->name,
					package_hashtable[package_num]->edge[j]->version,
					package_hashtable[package_num]->edge[j]->operator);
				if (package_hashtable[conflicts_package_num] == NULL) {
					/* create a new package */
					common_node_t *new_node = (common_node_t *) xmalloc(sizeof(common_node_t));
					new_node->name = package_hashtable[package_num]->edge[j]->name;
					new_node->version = package_hashtable[package_num]->edge[j]->version;
					new_node->num_of_edges = 0;
					new_node->edge = NULL;
					package_hashtable[conflicts_package_num] = new_node;
				}
				conflicts = xrealloc(conflicts, sizeof(int) * (conflicts_num + 1));
				conflicts[conflicts_num] = conflicts_package_num;
				conflicts_num++;
			}
		}
		i++;
	}

	/* Check conflicts */
	for (i = 0; i < conflicts_num; i++) {
		/* Check for conflicts */
		for (j = 0; j < STATUS_HASH_PRIME; j++) {
			if (status_hashtable[j] == NULL) {
				continue;
			}
			state_flag = get_status(j, 2);
			state_status = get_status(j, 3);
			if ((state_status != search_name_hashtable("installed"))
				&& (state_flag != search_name_hashtable("want-install"))) {
				continue;
			}
			status_package_num = status_hashtable[j]->package;
			for (k = 0; k < package_hashtable[status_package_num]->num_of_edges; k++) {
				const edge_t *package_edge = package_hashtable[status_package_num]->edge[k];
				if (package_edge->type != EDGE_CONFLICTS) {
					continue;
				}
				if (package_edge->name != package_hashtable[conflicts[i]]->name) {
					continue;
				}
				/* There is a conflict against the package name
				 * check if version conflict as well */
				if (test_version(package_hashtable[deb_file[i]->package]->version,
						package_edge->version, package_edge->operator)) {
					error_msg_and_die("Package %s conflict with %s",
						name_hashtable[package_hashtable[deb_file[i]->package]->name],
						name_hashtable[package_hashtable[status_package_num]->name]);
				}
			}
		}
	}

	/* Check dependendcies */
	i = 0;
	while (deb_file[i] != NULL) {
		const common_node_t *package_node = package_hashtable[deb_file[i]->package];
		int status_num = 0;

		for (j = 0; j < package_hashtable[deb_file[i]->package]->num_of_edges; j++) {
			const edge_t *package_edge = package_node->edge[j];
			const unsigned int package_num = search_package_hashtable(package_edge->name,
				package_edge->version, package_edge->operator);

			status_num = search_status_hashtable(name_hashtable[package_hashtable[package_num]->name]);
			state_status = get_status(status_num, 3);
			state_want = get_status(status_num, 1);
			switch (package_edge->type) {
				case(EDGE_PRE_DEPENDS):
				case(EDGE_OR_PRE_DEPENDS):
					/* It must be already installed */
					/* NOTE: This is untested, nothing apropriate in my status file */
					if ((package_hashtable[package_num] == NULL) || (state_status != search_name_hashtable("installed"))) {
						error_msg_and_die("Package %s pre-depends on %s, but it is not installed",
							name_hashtable[package_node->name],
							name_hashtable[package_edge->name]);
					}
					break;
				case(EDGE_DEPENDS):
				case(EDGE_OR_DEPENDS):
					/* It must be already installed, or to be installed */
					if ((package_hashtable[package_num] == NULL) ||
						((state_status != search_name_hashtable("installed")) &&
						(state_want != search_name_hashtable("want_install")))) {
						error_msg_and_die("Package %s depends on %s, but it is not installed, or flaged to be installed",
							name_hashtable[package_node->name],
							name_hashtable[package_edge->name]);
					}
					break;
			}
		}
		i++;
	}
	free(conflicts);
	return(TRUE);
}

char **create_list(const char *filename)
{
	FILE *list_stream;
	char **file_list = xmalloc(sizeof(char *));
	char *line = NULL;
	char *last_char;
	int length = 0;
	int count = 0;

	/* dont use [xw]fopen here, handle error ourself */
	list_stream = fopen(filename, "r");
	if (list_stream == NULL) {
		*file_list = NULL;
		return(file_list);
	}
	while (getline(&line, &length, list_stream) != -1) {
		file_list = xrealloc(file_list, sizeof(char *) * (length + 1));
		last_char = last_char_is(line, '\n');
		if (last_char) {
			*last_char = '\0';
		}
		file_list[count] = xstrdup(line);
		free(line);
		count++;
		length = 0;
	}
	fclose(list_stream);

	if (count == 0) {
		return(NULL);
	} else {
		file_list[count] = NULL;
		return(file_list);
	}
}

/* maybe i should try and hook this into remove_file.c somehow */
int remove_file_array(char **remove_names, char **exclude_names)
{
	struct stat path_stat;
	int match_flag;
	int remove_flag = FALSE;
	int i,j;

	if (remove_names == NULL) {
		return(FALSE);
	}
	for (i = 0; remove_names[i] != NULL; i++) {
		match_flag = FALSE;
		if (exclude_names != NULL) {
			for (j = 0; exclude_names[j] != 0; j++) {
				if (strcmp(remove_names[i], exclude_names[j]) == 0) {
					match_flag = TRUE;
					break;
				}
			}
		}
		if (!match_flag) {
			if (lstat(remove_names[i], &path_stat) < 0) {
				continue;
			}
			if (S_ISDIR(path_stat.st_mode)) {
				if (rmdir(remove_names[i]) != -1) {
					remove_flag = TRUE;
				}
			} else {
				if (unlink(remove_names[i]) != -1) {
					remove_flag = TRUE;
				}
			}
		}
	}
	return(remove_flag);
}

int run_package_script(const char *package_name, const char *script_type)
{
	struct stat path_stat;
	char *script_path;

	script_path = xmalloc(strlen(package_name) + strlen(script_type) + 21);
	sprintf(script_path, "/var/lib/dpkg/info/%s.%s", package_name, script_type);

	/* If the file doesnt exist is isnt a fatal */
	if (lstat(script_path, &path_stat) < 0) {
		free(script_path);
		return(EXIT_SUCCESS);
	} else {
		free(script_path);
		return(system(script_path));
	}
}

void all_control_list(char **remove_files, const char *package_name)
{
	const char *all_extensions[11] = {"preinst", "postinst", "prerm", "postrm",
		"list", "md5sums", "shlibs", "conffiles", "config", "templates", NULL };
	int i;

	/* Create a list of all /var/lib/dpkg/info/<package> files */
	for(i = 0; i < 10; i++) {
		remove_files[i] = xmalloc(strlen(package_name) + strlen(all_extensions[i]) + 21);
		sprintf(remove_files[i], "/var/lib/dpkg/info/%s.%s", package_name, all_extensions[i]);
	}
	remove_files[10] = NULL;
}

void remove_package(const unsigned int package_num)
{
	const char *package_name = name_hashtable[package_hashtable[package_num]->name];
	const unsigned int status_num = search_status_hashtable(package_name);
	const int package_name_length = strlen(package_name);
	char **remove_files;
	char **exclude_files;
	char list_name[package_name_length + 25];
	char conffile_name[package_name_length + 30];
	int return_value;

	printf("Removing %s ...\n", package_name);

	/* run prerm script */
	return_value = run_package_script(package_name, "prerm");
	if (return_value == -1) {
		error_msg_and_die("script failed, prerm failure");
	}

	/* Create a list of files to remove, and a seperate list of those to keep */
	sprintf(list_name, "/var/lib/dpkg/info/%s.list", package_name);
	remove_files = create_list(list_name);

	sprintf(conffile_name, "/var/lib/dpkg/info/%s.conffiles", package_name);
	exclude_files = create_list(conffile_name);

	/* Some directories cant be removed straight away, so do multiple passes */
	while (remove_file_array(remove_files, exclude_files) == TRUE);

	/* Create a list of all /var/lib/dpkg/info/<package> files */
	remove_files = xmalloc(11);
	all_control_list(remove_files, package_name);

	/* Create a list of files in /var/lib/dpkg/info/<package>.* to keep  */
	exclude_files = xmalloc(sizeof(char*) * 3);
	exclude_files[0] = xstrdup(conffile_name);
	exclude_files[1] = xmalloc(package_name_length + 27);
	sprintf(exclude_files[1], "/var/lib/dpkg/info/%s.postrm", package_name);
	exclude_files[2] = NULL;

	remove_file_array(remove_files, exclude_files);

	/* rename <package>.conffile to <package>.list */
	rename(conffile_name, list_name);

	/* Change package status */
	set_status(status_num, "deinstall", 1);
	set_status(status_num, "config-files", 3);
}

void purge_package(const unsigned int package_num)
{
	const char *package_name = name_hashtable[package_hashtable[package_num]->name];
	const unsigned int status_num = search_status_hashtable(package_name);
	char **remove_files;
	char **exclude_files;
	char list_name[strlen(package_name) + 25];

	/* run prerm script */
	if (run_package_script(package_name, "prerm") == -1) {
		error_msg_and_die("script failed, prerm failure");
	}

	/* Create a list of files to remove */
	sprintf(list_name, "/var/lib/dpkg/info/%s.list", package_name);
	remove_files = create_list(list_name);

	exclude_files = xmalloc(1);
	exclude_files[0] = NULL;

	/* Some directories cant be removed straight away, so do multiple passes */
	while (remove_file_array(remove_files, exclude_files) == TRUE);

	/* Create a list of all /var/lib/dpkg/info/<package> files */
	remove_files = xmalloc(11);
	all_control_list(remove_files, package_name);
	remove_file_array(remove_files, exclude_files);

	/* run postrm script */
	if (run_package_script(package_name, "postrm") == -1) {
		error_msg_and_die("postrm fialure.. set status to what?");
	}

	/* Change package status */
	set_status(status_num, "purge", 1);
	set_status(status_num, "not-installed", 3);
}

void unpack_package(deb_file_t *deb_file)
{
	const char *package_name = name_hashtable[package_hashtable[deb_file->package]->name];
	const unsigned int status_num = search_status_hashtable(package_name);
	const unsigned int status_package_num = status_hashtable[status_num]->status;

	FILE *out_stream;
	char *info_prefix;

	/* If existing version, remove it first */
	if (strcmp(name_hashtable[get_status(status_num, 3)], "installed") == 0) {
		/* Package is already installed, remove old version first */
		printf("Preparing to replace %s %s (using %s) ...\n", package_name,
			name_hashtable[package_hashtable[status_package_num]->version],
			deb_file->filename);
		remove_package(status_package_num);
	} else {
		printf("Unpacking %s (from %s) ...\n", package_name, deb_file->filename);
	}

	/* Extract control.tar.gz to /var/lib/dpkg/info/<package>.filename */
	info_prefix = (char *) xmalloc(sizeof(package_name) + 20 + 4 + 1);
	sprintf(info_prefix, "/var/lib/dpkg/info/%s.", package_name);
	deb_extract(deb_file->filename, stdout, (extract_quiet | extract_control_tar_gz | extract_all_to_fs), info_prefix, NULL);

	/* Extract data.tar.gz to the root directory */
	deb_extract(deb_file->filename, stdout, (extract_quiet | extract_data_tar_gz | extract_all_to_fs), "/", NULL);

	/* Create the list file */
	strcat(info_prefix, "list");
	out_stream = xfopen(info_prefix, "w");			
	deb_extract(deb_file->filename, out_stream, (extract_quiet | extract_data_tar_gz | extract_list), NULL, NULL);
	fclose(out_stream);

	/* change status */
	set_status(status_num, "install", 1);
	set_status(status_num, "unpacked", 3);

	free(info_prefix);
}

void configure_package(deb_file_t *deb_file)
{
	const char *package_name = name_hashtable[package_hashtable[deb_file->package]->name];
	const char *package_version = name_hashtable[package_hashtable[deb_file->package]->version];
	const int status_num = search_status_hashtable(package_name);
	int return_value;

	printf("Setting up %s (%s)\n", package_name, package_version);

	/* Run the preinst prior to extracting */
	return_value = run_package_script(package_name, "postinst");
	if (return_value == -1) {
		/* TODO: handle failure gracefully */
		error_msg_and_die("postrm failure.. set status to what?");
	}
	/* Change status to reflect success */
	set_status(status_num, "install", 1);
	set_status(status_num, "installed", 3);
}

extern int dpkg_main(int argc, char **argv)
{
	deb_file_t **deb_file = NULL;
	status_node_t *status_node;
	int opt = 0;
	int package_num;
	int dpkg_opt = 0;
	int deb_count = 0;
	int state_status;
	int status_num;
	int i;

	while ((opt = getopt(argc, argv, "CF:ilPru")) != -1) {
		switch (opt) {
			case 'C': // equivalent to --configure in official dpkg
				dpkg_opt |= dpkg_opt_configure;
				dpkg_opt |= dpkg_opt_package_name;
				break;
			case 'F': // equivalent to --force in official dpkg
				if (strcmp(optarg, "depends") == 0) {
					dpkg_opt |= dpkg_opt_force_ignore_depends;
				}				
			case 'i':
				dpkg_opt |= dpkg_opt_install;
				dpkg_opt |= dpkg_opt_filename;
				break;
			case 'l':
				dpkg_opt |= dpkg_opt_list_installed;
			case 'P':
				dpkg_opt |= dpkg_opt_purge;
				dpkg_opt |= dpkg_opt_package_name;
				break;
			case 'r':
				dpkg_opt |= dpkg_opt_remove;
				dpkg_opt |= dpkg_opt_package_name;
				break;
			case 'u':	/* Equivalent to --unpack in official dpkg */
				dpkg_opt |= dpkg_opt_unpack;
				dpkg_opt |= dpkg_opt_filename;
				break;
			default:
				show_usage();
		}
	}

	if ((argc == optind) || (dpkg_opt == 0)) {
		show_usage();
	} 

	puts("(Reading database ... xxxxx files and directories installed.)");
	index_status_file("/var/lib/dpkg/status");

	/* Read arguments and store relevant info in structs */
	deb_file = xmalloc(sizeof(deb_file_t));
	while (optind < argc) {
		deb_file[deb_count] = (deb_file_t *) xmalloc(sizeof(deb_file_t));
		if (dpkg_opt & dpkg_opt_filename) {
			deb_file[deb_count]->filename = xstrdup(argv[optind]);
			deb_file[deb_count]->control_file = deb_extract(argv[optind], stdout, (extract_control_tar_gz | extract_one_to_buffer), NULL, "./control");
			if (deb_file[deb_count]->control_file == NULL) {
				error_msg_and_die("Couldnt extract control file");
			}
			package_num = fill_package_struct(deb_file[deb_count]->control_file);

			if (package_num == -1) {
				error_msg("Invalid control file in %s", argv[optind]);
				continue;
			}
			deb_file[deb_count]->package = (unsigned int) package_num;
			/* Add the package to the status hashtable */
			if ((dpkg_opt & dpkg_opt_unpack) || (dpkg_opt & dpkg_opt_install)) {
				status_node = (status_node_t *) xmalloc(sizeof(status_node_t));
				status_node->package = deb_file[deb_count]->package;
				/* use reinstreq isnt changed to "ok" until the package control info
				 * is written to the status file*/
				status_node->status = search_name_hashtable("install reinstreq not-installed");

				status_num = search_status_hashtable(name_hashtable[package_hashtable[deb_file[deb_count]->package]->name]);
				status_hashtable[status_num] = status_node;
			}
		}
		else if (dpkg_opt & dpkg_opt_package_name) {
			deb_file[deb_count]->filename = NULL;
			deb_file[deb_count]->control_file = NULL;
			deb_file[deb_count]->package = search_package_hashtable(
				search_name_hashtable(argv[optind]),
				search_name_hashtable("ANY"), VER_ANY);
			if (package_hashtable[deb_file[deb_count]->package] == NULL) {
				error_msg_and_die("Package %s is uninstalled or unknown\n", argv[optind]);
			}
			state_status = get_status(search_status_hashtable(name_hashtable[package_hashtable[deb_file[deb_count]->package]->name]), 3);

			/* check package status is "installed" */
			if (dpkg_opt & dpkg_opt_remove) {
				if ((strcmp(name_hashtable[state_status], "not-installed") == 0) ||
					(strcmp(name_hashtable[state_status], "config-files") == 0)) {
					error_msg_and_die("%s is already removed.", name_hashtable[package_hashtable[deb_file[deb_count]->package]->name]);
				}
			}
			else if (dpkg_opt & dpkg_opt_purge) {
				/* if package status is "conf-files" then its ok */
				if (strcmp(name_hashtable[state_status], "not-installed") == 0) {
					error_msg_and_die("%s is already purged.", name_hashtable[package_hashtable[deb_file[deb_count]->package]->name]);
				}
			}
		}
		deb_count++;
		optind++;
	}
	deb_file[deb_count] = NULL;

	/* Check that the deb file arguments are installable */
	/* TODO: check dependencies before removing */
	if ((dpkg_opt & dpkg_opt_force_ignore_depends) != dpkg_opt_force_ignore_depends) {
		if (!check_deps(deb_file, 0, deb_count)) {
			error_msg_and_die("Dependency check failed");
		}
	}

	for (i = 0; i < deb_count; i++) {
		/* Remove or purge packages */
		if (dpkg_opt & dpkg_opt_remove) {
			remove_package(deb_file[i]->package);
		}
		else if (dpkg_opt & dpkg_opt_purge) {
			purge_package(deb_file[i]->package);
		}
		else if (dpkg_opt & dpkg_opt_unpack) {
			unpack_package(deb_file[i]);
		}
		else if (dpkg_opt & dpkg_opt_install) {
			unpack_package(deb_file[i]);
			configure_package(deb_file[i]);
		}
		else if (dpkg_opt & dpkg_opt_configure) {
			configure_package(deb_file[i]);
		}
	}

	write_status_file(deb_file);

	for (i = 0; i < deb_count; i++) {
		free(deb_file[i]->control_file);
		free(deb_file[i]->filename);
		free(deb_file[i]);
	}
	free(deb_file);

	for (i = 0; i < NAME_HASH_PRIME; i++) {
		if (name_hashtable[i] != NULL) {
			free(name_hashtable[i]);
		}
	}

	for (i = 0; i < PACKAGE_HASH_PRIME; i++) {
		free_package(package_hashtable[i]);
	}

	for (i = 0; i < STATUS_HASH_PRIME; i++) {
		if (status_hashtable[i] != NULL) {
			free(status_hashtable[i]);
		}
	}

	return(EXIT_FAILURE);
}

