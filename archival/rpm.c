/* vi: set sw=4 ts=4: */
/*
 * Mini rpm applet for busybox
 *
 * Copyright (C) 2001,2002 by Laurence Anderson
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
 */

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <netinet/in.h> /* For ntohl & htonl function */
#include <string.h> /* For strncmp */
#include <sys/mman.h> /* For mmap */
#include <time.h> /* For ctime */

#include "busybox.h"
#include "unarchive.h"

#define RPM_HEADER_MAGIC "\216\255\350"
#define RPM_CHAR_TYPE		1
#define RPM_INT8_TYPE		2
#define RPM_INT16_TYPE		3
#define RPM_INT32_TYPE		4
/* #define RPM_INT64_TYPE	5   ---- These aren't supported (yet) */
#define RPM_STRING_TYPE		6
#define RPM_BIN_TYPE		7
#define RPM_STRING_ARRAY_TYPE	8
#define RPM_I18NSTRING_TYPE	9

#define	RPMTAG_NAME  			1000
#define	RPMTAG_VERSION			1001
#define	RPMTAG_RELEASE			1002
#define	RPMTAG_SUMMARY			1004
#define	RPMTAG_DESCRIPTION		1005
#define	RPMTAG_BUILDTIME		1006
#define	RPMTAG_BUILDHOST		1007
#define	RPMTAG_SIZE			1009
#define	RPMTAG_VENDOR			1011
#define	RPMTAG_LICENSE			1014
#define	RPMTAG_PACKAGER			1015
#define	RPMTAG_GROUP			1016
#define RPMTAG_URL			1020
#define	RPMTAG_PREIN			1023
#define	RPMTAG_POSTIN			1024
#define	RPMTAG_FILEFLAGS		1037
#define	RPMTAG_FILEUSERNAME		1039
#define	RPMTAG_FILEGROUPNAME		1040
#define	RPMTAG_SOURCERPM		1044
#define	RPMTAG_PREINPROG		1085
#define	RPMTAG_POSTINPROG		1086
#define	RPMTAG_PREFIXS			1098
#define	RPMTAG_DIRINDEXES		1116
#define	RPMTAG_BASENAMES		1117
#define	RPMTAG_DIRNAMES			1118
#define	RPMFILE_CONFIG			(1 << 0)
#define	RPMFILE_DOC			(1 << 1)

enum rpm_functions_e {
	rpm_query = 1,
	rpm_install = 2,
	rpm_query_info = 4,
	rpm_query_package = 8,
	rpm_query_list = 16,
	rpm_query_list_doc = 32,
	rpm_query_list_config = 64
};

typedef struct {
	uint32_t tag; /* 4 byte tag */
	uint32_t type; /* 4 byte type */
	uint32_t offset; /* 4 byte offset */
	uint32_t count; /* 4 byte count */
} rpm_index;

static void *map;
static rpm_index **mytags;
static int tagcount;

void extract_cpio_gz(int fd);
rpm_index **rpm_gettags(int fd, int *num_tags);
int bsearch_rpmtag(const void *key, const void *item);
char *rpm_getstring(int tag, int itemindex);
int rpm_getint(int tag, int itemindex);
int rpm_getcount(int tag);
void exec_script(int progtag, int datatag, char *prefix);
void fileaction_dobackup(char *filename, int fileref);
void fileaction_setowngrp(char *filename, int fileref);
void fileaction_list(char *filename, int itemno);
void loop_through_files(int filetag, void (*fileaction)(char *filename, int fileref));

int rpm_main(int argc, char **argv)
{
	int opt = 0, func = 0, rpm_fd, offset;

	while ((opt = getopt(argc, argv, "iqpldc")) != -1) {
		switch (opt) {
		case 'i': // First arg: Install mode, with q: Information
			if (!func) func |= rpm_install;
			else func |= rpm_query_info;
			break;
		case 'q': // First arg: Query mode
			if (!func) func |= rpm_query;
			else bb_show_usage();
			break;
		case 'p': // Query a package
			func |= rpm_query_package;
			break;
		case 'l': // List files in a package
			func |= rpm_query_list;
			break;
		case 'd': // List doc files in a package (implies list)
			func |= rpm_query_list;
			func |= rpm_query_list_doc;
			break;
		case 'c': // List config files in a package (implies list)
			func |= rpm_query_list;
			func |= rpm_query_list_config;
			break;
		default:
			bb_show_usage();
		}
	}

	if (optind == argc) bb_show_usage();
	while (optind < argc) {
		rpm_fd = bb_xopen(argv[optind], O_RDONLY);
		mytags = rpm_gettags(rpm_fd, (int *) &tagcount);
		offset = lseek(rpm_fd, 0, SEEK_CUR);
		if (!mytags) { printf("Error reading rpm header\n"); exit(-1); }
		map = mmap(0, offset > getpagesize() ? (offset + offset % getpagesize()) : getpagesize(), PROT_READ, MAP_SHARED, rpm_fd, 0); // Mimimum is one page
		if (func & rpm_install) {
			loop_through_files(RPMTAG_BASENAMES, fileaction_dobackup); /* Backup any config files */
			extract_cpio_gz(rpm_fd); // Extact the archive
			loop_through_files(RPMTAG_BASENAMES, fileaction_setowngrp); /* Set the correct file uid/gid's */
		} else if (func & rpm_query && func & rpm_query_package) {
			if (!((func & rpm_query_info) || (func & rpm_query_list))) { // If just a straight query, just give package name
				printf("%s-%s-%s\n", rpm_getstring(RPMTAG_NAME, 0), rpm_getstring(RPMTAG_VERSION, 0), rpm_getstring(RPMTAG_RELEASE, 0));
			}
			if (func & rpm_query_info) {
				/* Do the nice printout */
				time_t bdate_time;
				struct tm *bdate;
				char bdatestring[50];
				printf("Name        : %-29sRelocations: %s\n", rpm_getstring(RPMTAG_NAME, 0), rpm_getstring(RPMTAG_PREFIXS, 0) ? rpm_getstring(RPMTAG_PREFIXS, 0) : "(not relocateable)");
				printf("Version     : %-34sVendor: %s\n", rpm_getstring(RPMTAG_VERSION, 0), rpm_getstring(RPMTAG_VENDOR, 0) ? rpm_getstring(RPMTAG_VENDOR, 0) : "(none)");
				bdate_time = rpm_getint(RPMTAG_BUILDTIME, 0);
				bdate = localtime((time_t *) &bdate_time);
				strftime(bdatestring, 50, "%a %d %b %Y %T %Z", bdate);
				printf("Release     : %-30sBuild Date: %s\n", rpm_getstring(RPMTAG_RELEASE, 0), bdatestring);
				printf("Install date: %-30sBuild Host: %s\n", "(not installed)", rpm_getstring(RPMTAG_BUILDHOST, 0));
				printf("Group       : %-30sSource RPM: %s\n", rpm_getstring(RPMTAG_GROUP, 0), rpm_getstring(RPMTAG_SOURCERPM, 0));
				printf("Size        : %-33dLicense: %s\n", rpm_getint(RPMTAG_SIZE, 0), rpm_getstring(RPMTAG_LICENSE, 0));
				printf("URL         : %s\n", rpm_getstring(RPMTAG_URL, 0));
				printf("Summary     : %s\n", rpm_getstring(RPMTAG_SUMMARY, 0));
				printf("Description :\n%s\n", rpm_getstring(RPMTAG_DESCRIPTION, 0));
			}
			if (func & rpm_query_list) {
				int count, it, flags;
				count = rpm_getcount(RPMTAG_BASENAMES);
				for (it = 0; it < count; it++) {
					flags = rpm_getint(RPMTAG_FILEFLAGS, it);
					switch ((func & rpm_query_list_doc) + (func & rpm_query_list_config))
					{
						case rpm_query_list_doc: if (!(flags & RPMFILE_DOC)) continue; break;
						case rpm_query_list_config: if (!(flags & RPMFILE_CONFIG)) continue; break;
						case rpm_query_list_doc + rpm_query_list_config: if (!((flags & RPMFILE_CONFIG) || (flags & RPMFILE_DOC))) continue; break;
					}
					printf("%s%s\n", rpm_getstring(RPMTAG_DIRNAMES, rpm_getint(RPMTAG_DIRINDEXES, it)), rpm_getstring(RPMTAG_BASENAMES, it));
				}
			}
		}
		optind++;
		free (mytags);
	}
	return 0;
}

void extract_cpio_gz(int fd) {
	archive_handle_t *archive_handle;
	unsigned char magic[2];

	/* Initialise */
	archive_handle = init_handle();
	archive_handle->seek = seek_by_char;
	//archive_handle->action_header = header_list;
	archive_handle->action_data = data_extract_all;
	archive_handle->flags |= ARCHIVE_PRESERVE_DATE;
	archive_handle->flags |= ARCHIVE_CREATE_LEADING_DIRS;
	archive_handle->src_fd = fd;
	archive_handle->offset = 0;
	
	bb_xread_all(archive_handle->src_fd, &magic, 2);
	if ((magic[0] != 0x1f) || (magic[1] != 0x8b)) {
		bb_error_msg_and_die("Invalid gzip magic");
	}
	check_header_gzip(archive_handle->src_fd);	
	chdir("/"); // Install RPM's to root

	archive_handle->src_fd = open_transformer(archive_handle->src_fd, inflate_gunzip);
	archive_handle->offset = 0;
	while (get_header_cpio(archive_handle) == EXIT_SUCCESS);
}


rpm_index **rpm_gettags(int fd, int *num_tags)
{
	rpm_index **tags = calloc(200, sizeof(struct rpmtag *)); /* We should never need mode than 200, and realloc later */
	int pass, tagindex = 0;
	lseek(fd, 96, SEEK_CUR); /* Seek past the unused lead */

	for (pass = 0; pass < 2; pass++) { /* 1st pass is the signature headers, 2nd is the main stuff */
		struct {
			char magic[3]; /* 3 byte magic: 0x8e 0xad 0xe8 */
			uint8_t version; /* 1 byte version number */
			uint32_t reserved; /* 4 bytes reserved */
			uint32_t entries; /* Number of entries in header (4 bytes) */
			uint32_t size; /* Size of store (4 bytes) */
		} header;
		rpm_index *tmpindex;
		int storepos;

		read(fd, &header, sizeof(header));
		if (strncmp((char *) &header.magic, RPM_HEADER_MAGIC, 3) != 0) return NULL; /* Invalid magic */
		if (header.version != 1) return NULL; /* This program only supports v1 headers */
		header.size = ntohl(header.size);
		header.entries = ntohl(header.entries);
		storepos = lseek(fd,0,SEEK_CUR) + header.entries * 16;

		while (header.entries--) {
			tmpindex = tags[tagindex++] = malloc(sizeof(rpm_index));
			read(fd, tmpindex, sizeof(rpm_index));
			tmpindex->tag = ntohl(tmpindex->tag); tmpindex->type = ntohl(tmpindex->type); tmpindex->count = ntohl(tmpindex->count);
			tmpindex->offset = storepos + ntohl(tmpindex->offset);
			if (pass==0) tmpindex->tag -= 743;
		}
		lseek(fd, header.size, SEEK_CUR); /* Seek past store */
		if (pass==0) lseek(fd, (8 - (lseek(fd,0,SEEK_CUR) % 8)) % 8, SEEK_CUR); /* Skip padding to 8 byte boundary after reading signature headers */
	}
	tags = realloc(tags, tagindex * sizeof(struct rpmtag *)); /* realloc tags to save space */
	*num_tags = tagindex;
	return tags; /* All done, leave the file at the start of the gzipped cpio archive */
}

int bsearch_rpmtag(const void *key, const void *item)
{
	rpm_index **tmp = (rpm_index **) item;
	return ((int) key - tmp[0]->tag);
}

int rpm_getcount(int tag)
{
	rpm_index **found;
	found = bsearch((void *) tag, mytags, tagcount, sizeof(struct rpmtag *), bsearch_rpmtag);
	if (!found) return 0;
	else return found[0]->count;
}

char *rpm_getstring(int tag, int itemindex)
{
	rpm_index **found;
	found = bsearch((void *) tag, mytags, tagcount, sizeof(struct rpmtag *), bsearch_rpmtag);
	if (!found || itemindex >= found[0]->count) return NULL;
	if (found[0]->type == RPM_STRING_TYPE || found[0]->type == RPM_I18NSTRING_TYPE || found[0]->type == RPM_STRING_ARRAY_TYPE) {
		int n;
		char *tmpstr = (char *) (map + found[0]->offset);
		for (n=0; n < itemindex; n++) tmpstr = tmpstr + strlen(tmpstr) + 1;
		return tmpstr;
	} else return NULL;
}

int rpm_getint(int tag, int itemindex)
{
	rpm_index **found;
	int n, *tmpint;
	found = bsearch((void *) tag, mytags, tagcount, sizeof(struct rpmtag *), bsearch_rpmtag);
	if (!found || itemindex >= found[0]->count) return -1;
	tmpint = (int *) (map + found[0]->offset);
	if (found[0]->type == RPM_INT32_TYPE) {
		for (n=0; n<itemindex; n++) tmpint = (int *) ((void *) tmpint + 4);
		return ntohl(*tmpint);
	} else if (found[0]->type == RPM_INT16_TYPE) {
		for (n=0; n<itemindex; n++) tmpint = (int *) ((void *) tmpint + 2);
		return ntohs(*tmpint);
	} else if (found[0]->type == RPM_INT8_TYPE) {
		for (n=0; n<itemindex; n++) tmpint = (int *) ((void *) tmpint + 1);
		return ntohs(*tmpint);
	} else return -1;
}

void fileaction_dobackup(char *filename, int fileref)
{
	struct stat oldfile;
	int stat_res;
	char *newname;
	if (rpm_getint(RPMTAG_FILEFLAGS, fileref) & RPMFILE_CONFIG) { /* Only need to backup config files */
		stat_res = lstat (filename, &oldfile);
		if (stat_res == 0 && S_ISREG(oldfile.st_mode)) { /* File already exists  - really should check MD5's etc to see if different */
			newname = bb_xstrdup(filename);
			newname = strcat(newname, ".rpmorig");
			copy_file(filename, newname, FILEUTILS_RECUR | FILEUTILS_PRESERVE_STATUS);
			remove_file(filename, FILEUTILS_RECUR | FILEUTILS_FORCE);
			free(newname);
		}
	}
}

void fileaction_setowngrp(char *filename, int fileref)
{
	int uid, gid;
	uid = my_getpwnam(rpm_getstring(RPMTAG_FILEUSERNAME, fileref));
	gid = my_getgrnam(rpm_getstring(RPMTAG_FILEGROUPNAME, fileref));
	chown (filename, uid, gid);
}

void fileaction_list(char *filename, int fileref)
{
	printf("%s\n", filename);
}

void loop_through_files(int filetag, void (*fileaction)(char *filename, int fileref))
{
	int count = 0;
	char *filename, *tmp_dirname, *tmp_basename;
	while (rpm_getstring(filetag, count)) {
		tmp_dirname = rpm_getstring(RPMTAG_DIRNAMES, rpm_getint(RPMTAG_DIRINDEXES, count)); /* 1st put on the directory */
		tmp_basename = rpm_getstring(RPMTAG_BASENAMES, count);
		filename = xmalloc(strlen(tmp_basename) + strlen(tmp_dirname) + 1);
		strcpy(filename, tmp_dirname); /* First the directory name */
		strcat(filename, tmp_basename); /* then the filename */
		fileaction(filename, count++);
		free(filename);
	}
}
