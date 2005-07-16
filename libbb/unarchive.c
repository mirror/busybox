/*
 *  Copyright (C) 2000 by Glenn McGrath
 *  Copyright (C) 2001 by Laurence Anderson
 *	
 *  Based on previous work by busybox developers and others.
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

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>
#include "libbb.h"

extern void seek_sub_file(FILE *src_stream, const int count);
extern char *extract_archive(FILE *src_stream, FILE *out_stream, const file_header_t *file_entry,
 const int function, const char *prefix);


#ifdef L_archive_offset
off_t archive_offset;
#else
extern off_t archive_offset;
#endif	

#ifdef L_seek_sub_file
void seek_sub_file(FILE *src_stream, const int count)
{
	int i;
	/* Try to fseek as faster */
	archive_offset += count;
	if (fseek(src_stream, count, SEEK_CUR) != 0 && errno == ESPIPE) {
	for (i = 0; i < count; i++) {
		fgetc(src_stream);
		}
	}
	return;
}
#endif	



#ifdef L_extract_archive
/* Extract the data postioned at src_stream to either filesystem, stdout or 
 * buffer depending on the value of 'function' which is defined in libbb.h 
 *
 * prefix doesnt have to be just a directory, it may prefix the filename as well.
 *
 * e.g. '/var/lib/dpkg/info/dpkg.' will extract all files to the base bath 
 * '/var/lib/dpkg/info/' and all files/dirs created in that dir will have 
 * 'dpkg.' as their prefix
 *
 * For this reason if prefix does point to a dir then it must end with a
 * trailing '/' or else the last dir will be assumed to be the file prefix 
 */
char *extract_archive(FILE *src_stream, FILE *out_stream, const file_header_t *file_entry,
 const int function, const char *prefix)
{
	FILE *dst_stream = NULL;
	char *full_name = NULL;
	char *buffer = NULL;
	struct utimbuf t;

	/* prefix doesnt have to be a proper path it may prepend 
	 * the filename as well */
	if (prefix != NULL) {
		/* strip leading '/' in filename to extract as prefix may not be dir */
		/* Cant use concat_path_file here as prefix might not be a directory */
		char *path = file_entry->name;
		if (strncmp("./", path, 2) == 0) {
			path += 2;
			if (strlen(path) == 0) {
				return(NULL);
			}
		}
		full_name = xmalloc(strlen(prefix) + strlen(path) + 1);
		strcpy(full_name, prefix);
		strcat(full_name, path);
	} else {
		full_name = file_entry->name;
	}
	if (function & extract_to_stdout) {
		if (S_ISREG(file_entry->mode)) {
			copy_file_chunk(src_stream, out_stream, file_entry->size);			
			archive_offset += file_entry->size;
		}
	}
	else if (function & extract_one_to_buffer) { 
		if (S_ISREG(file_entry->mode)) {
			buffer = (char *) xmalloc(file_entry->size + 1);
			fread(buffer, 1, file_entry->size, src_stream);
			buffer[file_entry->size] = '\0';
			archive_offset += file_entry->size;
			return(buffer);
		}
	}
	else if (function & extract_all_to_fs) {
		struct stat oldfile;
		int stat_res;
		stat_res = lstat (full_name, &oldfile);
		if (stat_res == 0) { /* The file already exists */
			if ((function & extract_unconditional) || (oldfile.st_mtime < file_entry->mtime)) {
				if (!S_ISDIR(oldfile.st_mode)) {
					unlink(full_name); /* Directories might not be empty etc */
				}
			} else {
				if ((function & extract_quiet) != extract_quiet) {
					error_msg("%s not created: newer or same age file exists", file_entry->name);
				}
				seek_sub_file(src_stream, file_entry->size);
				return (NULL);
			}
		}
		if (function & extract_create_leading_dirs) { /* Create leading directories with default umask */
			char *buf, *parent;
			buf = xstrdup(full_name);
			parent = dirname(buf);
			if (make_directory (parent, -1, FILEUTILS_RECUR) != 0) {
				if ((function & extract_quiet) != extract_quiet) {
					error_msg("couldn't create leading directories");
				}
			}
			free (buf);
		}
		switch(file_entry->mode & S_IFMT) {
			case S_IFREG:
				if (file_entry->link_name) { /* Found a cpio hard link */
					if (link(file_entry->link_name, full_name) != 0) {
						if ((function & extract_quiet) != extract_quiet) {
							perror_msg("Cannot link from %s to '%s'",
								file_entry->name, file_entry->link_name);
						}
					}
				} else {
					if ((dst_stream = wfopen(full_name, "w")) == NULL) {
						seek_sub_file(src_stream, file_entry->size);
						return NULL;
					}
					archive_offset += file_entry->size;
					copy_file_chunk(src_stream, dst_stream, file_entry->size);			
					fclose(dst_stream);
				}
				break;
			case S_IFDIR:
				if (stat_res != 0) {
					if (mkdir(full_name, file_entry->mode) < 0) {
						if ((function & extract_quiet) != extract_quiet) {
							perror_msg("extract_archive: %s", full_name);
						}
					}
				}
				break;
			case S_IFLNK:
				if (symlink(file_entry->link_name, full_name) < 0) {
					if ((function & extract_quiet) != extract_quiet) {
						perror_msg("Cannot create symlink from %s to '%s'", file_entry->name, file_entry->link_name);
					}
					return NULL;
				}
				break;
			case S_IFSOCK:
			case S_IFBLK:
			case S_IFCHR:
			case S_IFIFO:
				if (mknod(full_name, file_entry->mode, file_entry->device) == -1) {
					if ((function & extract_quiet) != extract_quiet) {
						perror_msg("Cannot create node %s", file_entry->name);
					}
					return NULL;
				}
				break;
		}

		/* Changing a symlink's properties normally changes the properties of the 
		 * file pointed to, so dont try and change the date or mode, lchown does
		 * does the right thing, but isnt available in older versions of libc */
		if (S_ISLNK(file_entry->mode)) {
#if (__GLIBC__ > 2) && (__GLIBC_MINOR__ > 1)
			lchown(full_name, file_entry->uid, file_entry->gid);
#endif
		} else {
			if (function & extract_preserve_date) {
				t.actime = file_entry->mtime;
				t.modtime = file_entry->mtime;
				utime(full_name, &t);
			}
			chmod(full_name, file_entry->mode);
			chown(full_name, file_entry->uid, file_entry->gid);
		}
	} else {
		/* If we arent extracting data we have to skip it, 
		 * if data size is 0 then then just do it anyway
		 * (saves testing for it) */
		seek_sub_file(src_stream, file_entry->size);
	}

	/* extract_list and extract_verbose_list can be used in conjunction
	 * with one of the above four extraction functions, so do this seperately */
	if (function & extract_verbose_list) {
		fprintf(out_stream, "%s %d/%d %8d %s ", mode_string(file_entry->mode), 
			file_entry->uid, file_entry->gid,
			(int) file_entry->size, time_string(file_entry->mtime));
	}
	if ((function & extract_list) || (function & extract_verbose_list)){
		/* fputs doesnt add a trailing \n, so use fprintf */
		fprintf(out_stream, "%s\n", file_entry->name);
	}

	free(full_name);

	return(NULL); /* Maybe we should say if failed */
}
#endif

#ifdef L_unarchive
char *unarchive(FILE *src_stream, FILE *out_stream, file_header_t *(*get_headers)(FILE *),
	const int extract_function, const char *prefix, char **extract_names)
{
	file_header_t *file_entry;
	int extract_flag;
	int i;
	char *buffer = NULL;

	archive_offset = 0;
	while ((file_entry = get_headers(src_stream)) != NULL) {
		extract_flag = TRUE;
		if (extract_names != NULL) {
			int found_flag = FALSE;
			for(i = 0; extract_names[i] != 0; i++) {
				if (strcmp(extract_names[i], file_entry->name) == 0) {
					found_flag = TRUE;
					break;
				}
			}
			if (extract_function & extract_exclude_list) {
				if (found_flag == TRUE) {
					extract_flag = FALSE;
				}
			} else {
				/* If its not found in the include list dont extract it */
				if (found_flag == FALSE) {
					extract_flag = FALSE;
				}
			}

		}

		if (extract_flag == TRUE) {
			buffer = extract_archive(src_stream, out_stream, file_entry, extract_function, prefix);
		} else {
			/* seek past the data entry */
			seek_sub_file(src_stream, file_entry->size);
		}
		free(file_entry->name); /* may be null, but doesn't matter */
		free(file_entry->link_name);
		free(file_entry);
	}
	return(buffer);
}
#endif

#ifdef L_get_header_ar
file_header_t *get_header_ar(FILE *src_stream)
{
	file_header_t *typed;
	union {
		char raw[60];
	 	struct {
 			char name[16];
 			char date[12];
 			char uid[6];
 			char gid[6];
 			char mode[8];
 			char size[10];
 			char magic[2];
 		} formated;
	} ar;
	static char *ar_long_names;

	if (fread(ar.raw, 1, 60, src_stream) != 60) {
		return(NULL);
	}
	archive_offset += 60;
	/* align the headers based on the header magic */
	if ((ar.formated.magic[0] != '`') || (ar.formated.magic[1] != '\n')) {
		/* some version of ar, have an extra '\n' after each data entry,
		 * this puts the next header out by 1 */
		if (ar.formated.magic[1] != '`') {
			error_msg("Invalid magic");
			return(NULL);
		}
		/* read the next char out of what would be the data section,
		 * if its a '\n' then it is a valid header offset by 1*/
		archive_offset++;
		if (fgetc(src_stream) != '\n') {
			error_msg("Invalid magic");
			return(NULL);
		}
		/* fix up the header, we started reading 1 byte too early */
		/* raw_header[60] wont be '\n' as it should, but it doesnt matter */
		memmove(ar.raw, &ar.raw[1], 59);
	}
		
	typed = (file_header_t *) xcalloc(1, sizeof(file_header_t));

	typed->size = (size_t) atoi(ar.formated.size);
	/* long filenames have '/' as the first character */
	if (ar.formated.name[0] == '/') {
		if (ar.formated.name[1] == '/') {
			/* If the second char is a '/' then this entries data section
			 * stores long filename for multiple entries, they are stored
			 * in static variable long_names for use in future entries */
			ar_long_names = (char *) xrealloc(ar_long_names, typed->size);
			fread(ar_long_names, 1, typed->size, src_stream);
			archive_offset += typed->size;
			/* This ar entries data section only contained filenames for other records
			 * they are stored in the static ar_long_names for future reference */
			return (get_header_ar(src_stream)); /* Return next header */
		} else if (ar.formated.name[1] == ' ') {
			/* This is the index of symbols in the file for compilers */
			seek_sub_file(src_stream, typed->size);
			return (get_header_ar(src_stream)); /* Return next header */
		} else {
			/* The number after the '/' indicates the offset in the ar data section
			(saved in variable long_name) that conatains the real filename */
			if (!ar_long_names) {
				error_msg("Cannot resolve long file name");
				return (NULL);
			}
			typed->name = xstrdup(ar_long_names + atoi(&ar.formated.name[1]));
		}
	} else {
		/* short filenames */
		typed->name = xcalloc(1, 16);
		strncpy(typed->name, ar.formated.name, 16);
	}
	typed->name[strcspn(typed->name, " /")]='\0';

	/* convert the rest of the now valid char header to its typed struct */	
	parse_mode(ar.formated.mode, &typed->mode);
	typed->mtime = atoi(ar.formated.date);
	typed->uid = atoi(ar.formated.uid);
	typed->gid = atoi(ar.formated.gid);

	return(typed);
}
#endif

#ifdef L_get_header_cpio
struct hardlinks {
	file_header_t *entry;
	int inode;
	struct hardlinks *next;
};

file_header_t *get_header_cpio(FILE *src_stream)
{
	file_header_t *cpio_entry = NULL;
	char cpio_header[110];
	int namesize;
 	char dummy[16];
 	int major, minor, nlink, inode;
 	static struct hardlinks *saved_hardlinks = NULL;
 	static int pending_hardlinks = 0;

	if (pending_hardlinks) { /* Deal with any pending hardlinks */
		struct hardlinks *tmp = saved_hardlinks, *oldtmp = NULL;
		while (tmp) {
			if (tmp->entry->link_name) { /* Found a hardlink ready to be extracted */
				cpio_entry = tmp->entry;
				if (oldtmp) oldtmp->next = tmp->next; /* Remove item from linked list */
				else saved_hardlinks = tmp->next;
				free(tmp);
				return (cpio_entry);
			}
			oldtmp = tmp;
			tmp = tmp->next;
		}
		pending_hardlinks = 0; /* No more pending hardlinks, read next file entry */
	}
  
	/* There can be padding before archive header */
	seek_sub_file(src_stream, (4 - (archive_offset % 4)) % 4);
	if (fread(cpio_header, 1, 110, src_stream) == 110) {
		archive_offset += 110;
		if (strncmp(cpio_header, "07070", 5) != 0) {
			error_msg("Unsupported format or invalid magic");
			return(NULL);
		}
		switch (cpio_header[5]) {
			case '2': /* "crc" header format */
				/* Doesnt do the crc check yet */
			case '1': /* "newc" header format */
				cpio_entry = (file_header_t *) xcalloc(1, sizeof(file_header_t));
				sscanf(cpio_header, "%6c%8x%8x%8x%8x%8x%8lx%8lx%16c%8x%8x%8x%8c",
					dummy, &inode, (unsigned int*)&cpio_entry->mode, 
					(unsigned int*)&cpio_entry->uid, (unsigned int*)&cpio_entry->gid,
					&nlink, &cpio_entry->mtime, (unsigned long*)&cpio_entry->size,
					dummy, &major, &minor, &namesize, dummy);

				cpio_entry->name = (char *) xcalloc(1, namesize);
				fread(cpio_entry->name, 1, namesize, src_stream); /* Read in filename */
				archive_offset += namesize;
				/* Skip padding before file contents */
				seek_sub_file(src_stream, (4 - (archive_offset % 4)) % 4);
				if (strcmp(cpio_entry->name, "TRAILER!!!") == 0) {
					printf("%d blocks\n", (int) (archive_offset % 512 ? (archive_offset / 512) + 1 : archive_offset / 512)); /* Always round up */
					if (saved_hardlinks) { /* Bummer - we still have unresolved hardlinks */
						struct hardlinks *tmp = saved_hardlinks, *oldtmp = NULL;
						while (tmp) {
							error_msg("%s not created: cannot resolve hardlink", tmp->entry->name);
							oldtmp = tmp;
							tmp = tmp->next;
							free (oldtmp->entry->name);
							free (oldtmp->entry);
							free (oldtmp);
						}
						saved_hardlinks = NULL;
						pending_hardlinks = 0;
					}
					return(NULL);
				}

				if (S_ISLNK(cpio_entry->mode)) {
					cpio_entry->link_name = (char *) xcalloc(1, cpio_entry->size + 1);
					fread(cpio_entry->link_name, 1, cpio_entry->size, src_stream);
					archive_offset += cpio_entry->size;
					cpio_entry->size = 0; /* Stop possiable seeks in future */
				}
				if (nlink > 1 && !S_ISDIR(cpio_entry->mode)) {
					if (cpio_entry->size == 0) { /* Put file on a linked list for later */
						struct hardlinks *new = xmalloc(sizeof(struct hardlinks));
						new->next = saved_hardlinks;
						new->inode = inode;
						new->entry = cpio_entry;
						saved_hardlinks = new;
					return(get_header_cpio(src_stream)); /* Recurse to next file */
					} else { /* Found the file with data in */
						struct hardlinks *tmp = saved_hardlinks;
						pending_hardlinks = 1;
						while (tmp) {
							if (tmp->inode == inode) {
								tmp->entry->link_name = xstrdup(cpio_entry->name);
								nlink--;
							}
							tmp = tmp->next;
						}
						if (nlink > 1) error_msg("error resolving hardlink: did you create the archive with GNU cpio 2.0-2.2?");
					}
				}
				cpio_entry->device = (major << 8) | minor;
				break;
			default:
				error_msg("Unsupported format");
				return(NULL);
		}
		if (ferror(src_stream) || feof(src_stream)) {
			perror_msg("Stream error");
			return(NULL);
		}
	}
	return(cpio_entry);
}
#endif

#ifdef L_get_header_tar
file_header_t *get_header_tar(FILE *tar_stream)
{
	union {
		unsigned char raw[512];
		struct {
			char name[100];		/*   0-99 */
			char mode[8];		/* 100-107 */
			char uid[8];		/* 108-115 */
			char gid[8];		/* 116-123 */
			char size[12];		/* 124-135 */
			char mtime[12];		/* 136-147 */
			char chksum[8];		/* 148-155 */
			char typeflag;		/* 156-156 */
			char linkname[100];	/* 157-256 */
			char magic[6];		/* 257-262 */
			char version[2];	/* 263-264 */
			char uname[32];		/* 265-296 */
			char gname[32];		/* 297-328 */
			char devmajor[8];	/* 329-336 */
			char devminor[8];	/* 337-344 */
			char prefix[155];	/* 345-499 */
			char padding[12];	/* 500-512 */
		} formated;
	} tar;
	file_header_t *tar_entry = NULL;
	long i;
	long sum = 0;

	if (archive_offset % 512 != 0) {
		seek_sub_file(tar_stream, 512 - (archive_offset % 512));
	}

	if (fread(tar.raw, 1, 512, tar_stream) != 512) {
		/* Unfortunatly its common for tar files to have all sorts of
		 * trailing garbage, fail silently */
//		error_msg("Couldnt read header");
		return(NULL);
	}
	archive_offset += 512;

	/* Check header has valid magic, unfortunately some tar files
	 * have empty (0'ed) tar entries at the end, which will
	 * cause this to fail, so fail silently for now
	 */
	if (strncmp(tar.formated.magic, "ustar", 5) != 0) {
		return(NULL);
	}

	/* Do checksum on headers */
	for (i =  0; i < 148 ; i++) {
		sum += tar.raw[i];
	}
	sum += ' ' * 8;
	for (i =  156; i < 512 ; i++) {
		sum += tar.raw[i];
	}
	if (sum != strtol(tar.formated.chksum, NULL, 8)) {
		error_msg("Invalid tar header checksum");
		return(NULL);
	}

	/* convert to type'ed variables */
	tar_entry = xcalloc(1, sizeof(file_header_t));
	tar_entry->name = xstrdup(tar.formated.name);

	parse_mode(tar.formated.mode, &tar_entry->mode);
	tar_entry->uid   = strtol(tar.formated.uid, NULL, 8);
	tar_entry->gid   = strtol(tar.formated.gid, NULL, 8);
	tar_entry->size  = strtol(tar.formated.size, NULL, 8);
	tar_entry->mtime = strtol(tar.formated.mtime, NULL, 8);
	tar_entry->link_name  = strlen(tar.formated.linkname) ? 
	    xstrdup(tar.formated.linkname) : NULL;
	tar_entry->device = (strtol(tar.formated.devmajor, NULL, 8) << 8) +
		strtol(tar.formated.devminor, NULL, 8);

	return(tar_entry);
}
#endif

#ifdef L_deb_extract
char *deb_extract(const char *package_filename, FILE *out_stream, 
	const int extract_function, const char *prefix, const char *filename)
{
	FILE *deb_stream;
	FILE *uncompressed_stream = NULL;
	file_header_t *ar_header = NULL;
	char **file_list = NULL;
	char *output_buffer = NULL;
	char *ared_file = NULL;
	char ar_magic[8];
	int gunzip_pid;

	if (filename != NULL) {
		file_list = xmalloc(sizeof(char *) * 2);
		file_list[0] = xstrdup(filename);
		file_list[1] = NULL;
	}
	
	if (extract_function & extract_control_tar_gz) {
		ared_file = xstrdup("control.tar.gz");
	}
	else if (extract_function & extract_data_tar_gz) {		
		ared_file = xstrdup("data.tar.gz");
	}

	/* open the debian package to be worked on */
	deb_stream = wfopen(package_filename, "r");
	if (deb_stream == NULL) {
		return(NULL);
	}
	/* set the buffer size */
	setvbuf(deb_stream, NULL, _IOFBF, 0x8000);

	/* check ar magic */
	fread(ar_magic, 1, 8, deb_stream);
	if (strncmp(ar_magic,"!<arch>",7) != 0) {
		error_msg_and_die("invalid magic");
	}
	archive_offset = 8;

	while ((ar_header = get_header_ar(deb_stream)) != NULL) {
		if (strcmp(ared_file, ar_header->name) == 0) {
			/* open a stream of decompressed data */
			uncompressed_stream = gz_open(deb_stream, &gunzip_pid);
			archive_offset = 0;
			output_buffer = unarchive(uncompressed_stream, out_stream, get_header_tar, extract_function, prefix, file_list);
		}
		seek_sub_file(deb_stream, ar_header->size);
	}
	gz_close(gunzip_pid);
	fclose(deb_stream);
	fclose(uncompressed_stream);
	free(ared_file);
	return(output_buffer);
}
#endif
