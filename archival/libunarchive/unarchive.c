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

#include <errno.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>
#include <sys/wait.h>
#include <signal.h>
#include "unarchive.h"
#include "busybox.h"

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
					if (file_entry->extract_func) file_entry->extract_func(src_stream, dst_stream);
					else copy_file_chunk(src_stream, dst_stream, file_entry->size);			
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
		fprintf(out_stream, "%s\n", full_name);
	}

	if (prefix != NULL) {
	  free(full_name);
	}

	return(NULL); /* Maybe we should say if failed */
}

char *unarchive(FILE *src_stream, FILE *out_stream, file_header_t *(*get_headers)(FILE *),
	const int extract_function, const char *prefix, char **include_name, char **exclude_name)
{
	file_header_t *file_entry;
	int extract_flag = TRUE;
	int i;
	char *buffer = NULL;
#ifdef CONFIG_FEATURE_UNARCHIVE_TAPE
	int pid, tape_pipe[2];

	if (pipe(tape_pipe) != 0) error_msg_and_die("Can't create pipe\n");
	if ((pid = fork()) == -1) error_msg_and_die("Fork failed\n");
	if (pid==0) { /* child process */
	    close(tape_pipe[0]); /* We don't wan't to read from the pipe */
	    copyfd(fileno(src_stream), tape_pipe[1]);
	    close(tape_pipe[1]); /* Send EOF */
	    exit(0);
	    /* notreached */
	}
	close(tape_pipe[1]); /* Don't want to write down the pipe */
	fclose(src_stream);
	src_stream = fdopen(tape_pipe[0], "r");
#endif
	archive_offset = 0;
	while ((file_entry = get_headers(src_stream)) != NULL) {

		if (include_name != NULL) {
			extract_flag = FALSE;
			for(i = 0; include_name[i] != 0; i++) {
				if (fnmatch(include_name[i], file_entry->name, FNM_LEADING_DIR) == 0) {
					extract_flag = TRUE;
					break;
				}
			}
		} else {
			extract_flag = TRUE;
		}

		/* If the file entry is in the exclude list dont extract it */
		if (exclude_name != NULL) {
			for(i = 0; exclude_name[i] != 0; i++) {
				if (fnmatch(exclude_name[i], file_entry->name, FNM_LEADING_DIR) == 0) {
					extract_flag = FALSE;
					break;
				}
			}
		}

		if (extract_flag) {
			buffer = extract_archive(src_stream, out_stream, file_entry, extract_function, prefix);
		} else {
			/* seek past the data entry */
			seek_sub_file(src_stream, file_entry->size);
		}
		free(file_entry->name);
		free(file_entry->link_name);
		free(file_entry);
	}
#ifdef CONFIG_FEATURE_UNARCHIVE_TAPE
	kill(pid, SIGTERM);
	waitpid(pid, NULL, 0);
#endif
	return(buffer);
}
