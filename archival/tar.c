/* vi: set sw=4 ts=4: */
/*
 * Mini tar implementation for busybox 
 *
 * Modifed to use common extraction code used by ar, cpio, dpkg-deb, dpkg
 *  Glenn McGrath <bug1@optushome.com.au>
 *
 * Note, that as of BusyBox-0.43, tar has been completely rewritten from the
 * ground up.  It still has remnents of the old code lying about, but it is
 * very different now (i.e., cleaner, less global variables, etc.)
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 1999-2002 by Erik Andersen <andersee@debian.org>
 *
 * Based in part in the tar implementation in sash
 *  Copyright (c) 1999 by David I. Bell
 *  Permission is granted to use, distribute, or modify this source,
 *  provided that this copyright notice remains intact.
 *  Permission to distribute sash derived code under the GPL has been granted.
 *
 * Based in part on the tar implementation from busybox-0.28
 *  Copyright (C) 1995 Bruce Perens
 *  This is free software under the GNU General Public License.
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

#include <fcntl.h>
#include <getopt.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fnmatch.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include "unarchive.h"
#include "busybox.h"

#ifdef CONFIG_FEATURE_TAR_CREATE

/* Tar file constants  */
# define TAR_MAGIC          "ustar"	/* ustar and a null */
# define TAR_VERSION        "  "	/* Be compatable with GNU tar format */

# ifndef MAJOR
#  define MAJOR(dev) (((dev)>>8)&0xff)
#  define MINOR(dev) ((dev)&0xff)
# endif

static const int TAR_BLOCK_SIZE = 512;
static const int TAR_MAGIC_LEN = 6;
static const int TAR_VERSION_LEN = 2;

/* POSIX tar Header Block, from POSIX 1003.1-1990  */
enum { NAME_SIZE = 100 };	/* because gcc won't let me use 'static const int' */
struct TarHeader {		/* byte offset */
	char name[NAME_SIZE];	/*   0-99 */
	char mode[8];		/* 100-107 */
	char uid[8];		/* 108-115 */
	char gid[8];		/* 116-123 */
	char size[12];		/* 124-135 */
	char mtime[12];		/* 136-147 */
	char chksum[8];		/* 148-155 */
	char typeflag;		/* 156-156 */
	char linkname[NAME_SIZE];	/* 157-256 */
	char magic[6];		/* 257-262 */
	char version[2];	/* 263-264 */
	char uname[32];		/* 265-296 */
	char gname[32];		/* 297-328 */
	char devmajor[8];	/* 329-336 */
	char devminor[8];	/* 337-344 */
	char prefix[155];	/* 345-499 */
	char padding[12];	/* 500-512 (pad to exactly the TAR_BLOCK_SIZE) */
};
typedef struct TarHeader TarHeader;

/*
** writeTarFile(),  writeFileToTarball(), and writeTarHeader() are
** the only functions that deal with the HardLinkInfo structure.
** Even these functions use the xxxHardLinkInfo() functions.
*/
typedef struct HardLinkInfo HardLinkInfo;
struct HardLinkInfo {
	HardLinkInfo *next;	/* Next entry in list */
	dev_t dev;			/* Device number */
	ino_t ino;			/* Inode number */
	short linkCount;	/* (Hard) Link Count */
	char name[1];		/* Start of filename (must be last) */
};

/* Some info to be carried along when creating a new tarball */
struct TarBallInfo {
	char *fileName;		/* File name of the tarball */
	int tarFd;			/* Open-for-write file descriptor
						   for the tarball */
	struct stat statBuf;	/* Stat info for the tarball, letting
							   us know the inode and device that the
							   tarball lives, so we can avoid trying 
							   to include the tarball into itself */
	int verboseFlag;	/* Whether to print extra stuff or not */
	const llist_t *excludeList;	/* List of files to not include */
	HardLinkInfo *hlInfoHead;	/* Hard Link Tracking Information */
	HardLinkInfo *hlInfo;	/* Hard Link Info for the current file */
};
typedef struct TarBallInfo TarBallInfo;

/* A nice enum with all the possible tar file content types */
enum TarFileType {
	REGTYPE = '0',		/* regular file */
	REGTYPE0 = '\0',	/* regular file (ancient bug compat) */
	LNKTYPE = '1',		/* hard link */
	SYMTYPE = '2',		/* symbolic link */
	CHRTYPE = '3',		/* character special */
	BLKTYPE = '4',		/* block special */
	DIRTYPE = '5',		/* directory */
	FIFOTYPE = '6',		/* FIFO special */
	CONTTYPE = '7',		/* reserved */
	GNULONGLINK = 'K',	/* GNU long (>100 chars) link name */
	GNULONGNAME = 'L',	/* GNU long (>100 chars) file name */
};
typedef enum TarFileType TarFileType;

/* Might be faster (and bigger) if the dev/ino were stored in numeric order;) */
static inline void addHardLinkInfo(HardLinkInfo ** hlInfoHeadPtr, dev_t dev,
								   ino_t ino, short linkCount,
								   const char *name)
{
	/* Note: hlInfoHeadPtr can never be NULL! */
	HardLinkInfo *hlInfo;

	hlInfo =
		(HardLinkInfo *) xmalloc(sizeof(HardLinkInfo) + strlen(name) + 1);
	if (hlInfo) {
		hlInfo->next = *hlInfoHeadPtr;
		*hlInfoHeadPtr = hlInfo;
		hlInfo->dev = dev;
		hlInfo->ino = ino;
		hlInfo->linkCount = linkCount;
		strcpy(hlInfo->name, name);
	}
	return;
}

static void freeHardLinkInfo(HardLinkInfo ** hlInfoHeadPtr)
{
	HardLinkInfo *hlInfo = NULL;
	HardLinkInfo *hlInfoNext = NULL;

	if (hlInfoHeadPtr) {
		hlInfo = *hlInfoHeadPtr;
		while (hlInfo) {
			hlInfoNext = hlInfo->next;
			free(hlInfo);
			hlInfo = hlInfoNext;
		}
		*hlInfoHeadPtr = NULL;
	}
	return;
}

/* Might be faster (and bigger) if the dev/ino were stored in numeric order;) */
static inline HardLinkInfo *findHardLinkInfo(HardLinkInfo * hlInfo, dev_t dev,
											 ino_t ino)
{
	while (hlInfo) {
		if ((ino == hlInfo->ino) && (dev == hlInfo->dev))
			break;
		hlInfo = hlInfo->next;
	}
	return (hlInfo);
}

/* Put an octal string into the specified buffer.
 * The number is zero and space padded and possibly null padded.
 * Returns TRUE if successful.  */
static int putOctal(char *cp, int len, long value)
{
	int tempLength;
	char tempBuffer[32];
	char *tempString = tempBuffer;

	/* Create a string of the specified length with an initial space,
	 * leading zeroes and the octal number, and a trailing null.  */
	sprintf(tempString, "%0*lo", len - 1, value);

	/* If the string is too large, suppress the leading space.  */
	tempLength = strlen(tempString) + 1;
	if (tempLength > len) {
		tempLength--;
		tempString++;
	}

	/* If the string is still too large, suppress the trailing null.  */
	if (tempLength > len)
		tempLength--;

	/* If the string is still too large, fail.  */
	if (tempLength > len)
		return FALSE;

	/* Copy the string to the field.  */
	memcpy(cp, tempString, len);

	return TRUE;
}

/* Write out a tar header for the specified file/directory/whatever */
static inline int writeTarHeader(struct TarBallInfo *tbInfo,
								 const char *header_name,
								 const char *real_name, struct stat *statbuf)
{
	long chksum = 0;
	struct TarHeader header;
	const unsigned char *cp = (const unsigned char *) &header;
	ssize_t size = sizeof(struct TarHeader);

	memset(&header, 0, size);

	strncpy(header.name, header_name, sizeof(header.name));

	putOctal(header.mode, sizeof(header.mode), statbuf->st_mode);
	putOctal(header.uid, sizeof(header.uid), statbuf->st_uid);
	putOctal(header.gid, sizeof(header.gid), statbuf->st_gid);
	putOctal(header.size, sizeof(header.size), 0);	/* Regular file size is handled later */
	putOctal(header.mtime, sizeof(header.mtime), statbuf->st_mtime);
	strncpy(header.magic, TAR_MAGIC TAR_VERSION,
			TAR_MAGIC_LEN + TAR_VERSION_LEN);

	/* Enter the user and group names (default to root if it fails) */
	if (my_getpwuid(header.uname, statbuf->st_uid) == NULL)
		strcpy(header.uname, "root");
	if (my_getgrgid(header.gname, statbuf->st_gid) == NULL)
		strcpy(header.gname, "root");

	if (tbInfo->hlInfo) {
		/* This is a hard link */
		header.typeflag = LNKTYPE;
		strncpy(header.linkname, tbInfo->hlInfo->name,
				sizeof(header.linkname));
	} else if (S_ISLNK(statbuf->st_mode)) {
		char *lpath = xreadlink(real_name);

		if (!lpath)		/* Already printed err msg inside xreadlink() */
			return (FALSE);
		header.typeflag = SYMTYPE;
		strncpy(header.linkname, lpath, sizeof(header.linkname));
		free(lpath);
	} else if (S_ISDIR(statbuf->st_mode)) {
		header.typeflag = DIRTYPE;
		strncat(header.name, "/", sizeof(header.name));
	} else if (S_ISCHR(statbuf->st_mode)) {
		header.typeflag = CHRTYPE;
		putOctal(header.devmajor, sizeof(header.devmajor),
				 MAJOR(statbuf->st_rdev));
		putOctal(header.devminor, sizeof(header.devminor),
				 MINOR(statbuf->st_rdev));
	} else if (S_ISBLK(statbuf->st_mode)) {
		header.typeflag = BLKTYPE;
		putOctal(header.devmajor, sizeof(header.devmajor),
				 MAJOR(statbuf->st_rdev));
		putOctal(header.devminor, sizeof(header.devminor),
				 MINOR(statbuf->st_rdev));
	} else if (S_ISFIFO(statbuf->st_mode)) {
		header.typeflag = FIFOTYPE;
	} else if (S_ISREG(statbuf->st_mode)) {
		header.typeflag = REGTYPE;
		putOctal(header.size, sizeof(header.size), statbuf->st_size);
	} else {
		error_msg("%s: Unknown file type", real_name);
		return (FALSE);
	}

	/* Calculate and store the checksum (i.e., the sum of all of the bytes of
	 * the header).  The checksum field must be filled with blanks for the
	 * calculation.  The checksum field is formatted differently from the
	 * other fields: it has [6] digits, a null, then a space -- rather than
	 * digits, followed by a null like the other fields... */
	memset(header.chksum, ' ', sizeof(header.chksum));
	cp = (const unsigned char *) &header;
	while (size-- > 0)
		chksum += *cp++;
	putOctal(header.chksum, 7, chksum);

	/* Now write the header out to disk */
	if ((size =
		 full_write(tbInfo->tarFd, (char *) &header,
					sizeof(struct TarHeader))) < 0) {
		error_msg(io_error, real_name);
		return (FALSE);
	}
	/* Pad the header up to the tar block size */
	for (; size < TAR_BLOCK_SIZE; size++) {
		write(tbInfo->tarFd, "\0", 1);
	}
	/* Now do the verbose thing (or not) */

	if (tbInfo->verboseFlag) {
		FILE *vbFd = stdout;

		if (tbInfo->verboseFlag == 2)	/* If the archive goes to stdout, verbose to stderr */
			vbFd = stderr;
		fprintf(vbFd, "%s\n", header.name);
	}

	return (TRUE);
}

# if defined CONFIG_FEATURE_TAR_EXCLUDE
static inline int exclude_file(const llist_t *excluded_files, const char *file)
{
	if (excluded_files == NULL) {
		return 0;
	}

	while (excluded_files) {
		if (excluded_files->data[0] == '/') {
			if (fnmatch(excluded_files->data, file,
						FNM_PATHNAME | FNM_LEADING_DIR) == 0)
				return 1;
		} else {
			const char *p;

			for (p = file; p[0] != '\0'; p++) {
				if ((p == file || p[-1] == '/') && p[0] != '/' &&
					fnmatch(excluded_files->data, p,
							FNM_PATHNAME | FNM_LEADING_DIR) == 0)
					return 1;
			}
		}
		excluded_files = excluded_files->link;
	}

	return 0;
}
#endif

static int writeFileToTarball(const char *fileName, struct stat *statbuf,
							  void *userData)
{
	struct TarBallInfo *tbInfo = (struct TarBallInfo *) userData;
	const char *header_name;

	/*
	   ** Check to see if we are dealing with a hard link.
	   ** If so -
	   ** Treat the first occurance of a given dev/inode as a file while
	   ** treating any additional occurances as hard links.  This is done
	   ** by adding the file information to the HardLinkInfo linked list.
	 */
	tbInfo->hlInfo = NULL;
	if (statbuf->st_nlink > 1) {
		tbInfo->hlInfo = findHardLinkInfo(tbInfo->hlInfoHead, statbuf->st_dev,
										  statbuf->st_ino);
		if (tbInfo->hlInfo == NULL)
			addHardLinkInfo(&tbInfo->hlInfoHead, statbuf->st_dev,
							statbuf->st_ino, statbuf->st_nlink, fileName);
	}

	/* It is against the rules to archive a socket */
	if (S_ISSOCK(statbuf->st_mode)) {
		error_msg("%s: socket ignored", fileName);
		return (TRUE);
	}

	/* It is a bad idea to store the archive we are in the process of creating,
	 * so check the device and inode to be sure that this particular file isn't
	 * the new tarball */
	if (tbInfo->statBuf.st_dev == statbuf->st_dev &&
		tbInfo->statBuf.st_ino == statbuf->st_ino) {
		error_msg("%s: file is the archive; skipping", fileName);
		return (TRUE);
	}

	header_name = fileName;
	while (header_name[0] == '/') {
		static int alreadyWarned = FALSE;

		if (alreadyWarned == FALSE) {
			error_msg("Removing leading '/' from member names");
			alreadyWarned = TRUE;
		}
		header_name++;
	}

	if (strlen(fileName) >= NAME_SIZE) {
		error_msg(name_longer_than_foo, NAME_SIZE);
		return (TRUE);
	}

	if (header_name[0] == '\0')
		return TRUE;

# if defined CONFIG_FEATURE_TAR_EXCLUDE
	if (exclude_file(tbInfo->excludeList, header_name)) {
		return SKIP;
	}
# endif							/* CONFIG_FEATURE_TAR_EXCLUDE */

	if (writeTarHeader(tbInfo, header_name, fileName, statbuf) == FALSE) {
		return (FALSE);
	}

	/* Now, if the file is a regular file, copy it out to the tarball */
	if ((tbInfo->hlInfo == NULL)
		&& (S_ISREG(statbuf->st_mode))) {
		int inputFileFd;
		char buffer[BUFSIZ];
		ssize_t size = 0, readSize = 0;

		/* open the file we want to archive, and make sure all is well */
		if ((inputFileFd = open(fileName, O_RDONLY)) < 0) {
			perror_msg("%s: Cannot open", fileName);
			return (FALSE);
		}

		/* write the file to the archive */
		while ((size = full_read(inputFileFd, buffer, sizeof(buffer))) > 0) {
			if (full_write(tbInfo->tarFd, buffer, size) != size) {
				/* Output file seems to have a problem */
				error_msg(io_error, fileName);
				return (FALSE);
			}
			readSize += size;
		}
		if (size == -1) {
			error_msg(io_error, fileName);
			return (FALSE);
		}
		/* Pad the file up to the tar block size */
		for (; (readSize % TAR_BLOCK_SIZE) != 0; readSize++) {
			write(tbInfo->tarFd, "\0", 1);
		}
		close(inputFileFd);
	}

	return (TRUE);
}

static inline int writeTarFile(const char *tarName, const int verboseFlag,
							   const llist_t *include, const llist_t *exclude, const int gzip)
{
#ifdef CONFIG_FEATURE_TAR_GZIP
	int gzipDataPipe[2] = { -1, -1 };
	int gzipStatusPipe[2] = { -1, -1 };
	pid_t gzipPid = 0;
#endif

	int errorFlag = FALSE;
	ssize_t size;
	struct TarBallInfo tbInfo;

	tbInfo.hlInfoHead = NULL;

	/* Make sure there is at least one file to tar up.  */
	if (include == NULL) {
		error_msg_and_die("Cowardly refusing to create an empty archive");
	}

	/* Open the tar file for writing.  */
	if (tarName == NULL || (tarName[0] == '-' && tarName[1] == '\0')) {
		tbInfo.tarFd = fileno(stdout);
		tbInfo.verboseFlag = verboseFlag ? 2 : 0;
	} else {
		tbInfo.tarFd = open(tarName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		tbInfo.verboseFlag = verboseFlag ? 1 : 0;
	}

	if (tbInfo.tarFd < 0) {
		perror_msg("%s: Cannot open", tarName);
		freeHardLinkInfo(&tbInfo.hlInfoHead);
		return (FALSE);
	}

	/* Store the stat info for the tarball's file, so
	 * can avoid including the tarball into itself....  */
	if (fstat(tbInfo.tarFd, &tbInfo.statBuf) < 0)
		error_msg_and_die(io_error, tarName);

#ifdef CONFIG_FEATURE_TAR_GZIP
	if (gzip) {
		if (socketpair(AF_UNIX, SOCK_STREAM, 0, gzipDataPipe) < 0
			|| pipe(gzipStatusPipe) < 0)
			perror_msg_and_die("Failed to create gzip pipe");

		signal(SIGPIPE, SIG_IGN);	/* we only want EPIPE on errors */

		gzipPid = fork();

		if (gzipPid == 0) {
			dup2(gzipDataPipe[0], 0);
			close(gzipDataPipe[1]);

			if (tbInfo.tarFd != 1);
			dup2(tbInfo.tarFd, 1);

			close(gzipStatusPipe[0]);
			fcntl(gzipStatusPipe[1], F_SETFD, FD_CLOEXEC);	/* close on exec shows sucess */

			execl("/bin/gzip", "gzip", "-f", 0);

			write(gzipStatusPipe[1], "", 1);
			close(gzipStatusPipe[1]);

			exit(-1);
		} else if (gzipPid > 0) {
			close(gzipDataPipe[0]);
			close(gzipStatusPipe[1]);

			while (1) {
				char buf;

				int n = read(gzipStatusPipe[0], &buf, 1);

				if (n == 1)
					error_msg_and_die("Could not exec gzip process");	/* socket was not closed => error */
				else if ((n < 0) && (errno == EAGAIN || errno == EINTR))
					continue;	/* try it again */
				break;
			}
			close(gzipStatusPipe[0]);

			tbInfo.tarFd = gzipDataPipe[1];
		} else {
			perror_msg_and_die("Failed to fork gzip process");
		}
	}
#endif

	tbInfo.excludeList = exclude;

	/* Read the directory/files and iterate over them one at a time */
	while (include) {
		if (!recursive_action(include->data, TRUE, FALSE, FALSE,
							  writeFileToTarball, writeFileToTarball,
							  (void *) &tbInfo)) {
			errorFlag = TRUE;
		}
		include = include->link;
	}
	/* Write two empty blocks to the end of the archive */
	for (size = 0; size < (2 * TAR_BLOCK_SIZE); size++) {
		write(tbInfo.tarFd, "\0", 1);
	}

	/* To be pedantically correct, we would check if the tarball
	 * is smaller than 20 tar blocks, and pad it if it was smaller,
	 * but that isn't necessary for GNU tar interoperability, and
	 * so is considered a waste of space */

	/* Hang up the tools, close up shop, head home */
	close(tbInfo.tarFd);
	if (errorFlag)
		error_msg("Error exit delayed from previous errors");

	freeHardLinkInfo(&tbInfo.hlInfoHead);

#ifdef CONFIG_FEATURE_TAR_GZIP
	if (gzip && gzipPid) {
		if (waitpid(gzipPid, NULL, 0) == -1)
			printf("Couldnt wait ?");
	}
#endif

	return !errorFlag;
}
#endif							/* tar_create */

#ifdef CONFIG_FEATURE_TAR_EXCLUDE
static const llist_t *append_file_list_to_list(const char *filename, const llist_t *list)
{
	FILE *src_stream = xfopen(filename, "r");
	char *line;
	while((line = get_line_from_file(src_stream)) != NULL) {
		chomp(line);
		list = add_to_list(list, line);
	}
	fclose(src_stream);

	return (list);
}
#endif

int tar_main(int argc, char **argv)
{
#ifdef CONFIG_FEATURE_TAR_GZIP
	char (*get_header_ptr)(archive_handle_t *) = get_header_tar;
#endif
	archive_handle_t *tar_handle;
	int opt;
	char *base_dir = NULL;
	char *tar_filename = "-";

#ifdef CONFIG_FEATURE_TAR_CREATE
	unsigned char tar_create = FALSE;
#endif

	if (argc < 2) {
		show_usage();
	}

	/* Prepend '-' to the first argument if required */
	if (argv[1][0] != '-') {
		char *tmp = xmalloc(strlen(argv[1]) + 2);
		tmp[0] = '-';
		strcpy(tmp + 1, argv[1]);
		argv[1] = tmp;
	}

	/* Initialise default values */
	tar_handle = init_handle();
	tar_handle->flags = ARCHIVE_CREATE_LEADING_DIRS;

	while ((opt = getopt(argc, argv, "cjtxT:X:C:f:Opvz")) != -1) {
		switch (opt) {
			/* One and only one of these is required */
#ifdef CONFIG_FEATURE_TAR_CREATE
		case 'c':
			tar_create = TRUE;
			break;
#endif
		case 't':
			if ((tar_handle->action_header == header_list) || 
				(tar_handle->action_header == header_verbose_list)) {
				tar_handle->action_header = header_verbose_list;
			} else {
				tar_handle->action_header = header_list;
			}
			break;
		case 'x':
			tar_handle->action_data = data_extract_all;
			break;

			/* These are optional */
			/* Exclude or Include files listed in <filename> */
#ifdef CONFIG_FEATURE_TAR_EXCLUDE
		case 'X':
			tar_handle->reject =
				append_file_list_to_list(optarg, tar_handle->reject);
			break;
#endif
		case 'T':
			/* by default a list is an include list */
			break;
		case 'C':		/* Change to dir <optarg> */
			base_dir = optarg;
			break;
		case 'f':		/* archive filename */
			tar_filename = optarg;
			break;
		case 'O':		/* To stdout */
			tar_handle->action_data = data_extract_to_stdout;
			break;
		case 'p':
			tar_handle->flags |= ARCHIVE_PRESERVE_DATE;
			break;
		case 'v':
			if ((tar_handle->action_header == header_list) || 
				(tar_handle->action_header == header_verbose_list)) {
				tar_handle->action_header = header_verbose_list;
			} else {
				tar_handle->action_header = header_list;
			}
			break;
#ifdef CONFIG_FEATURE_TAR_GZIP
		case 'z':
			get_header_ptr = get_header_tar_gz;
			break;
#endif
#ifdef CONFIG_FEATURE_TAR_BZIP2
		case 'j':
			get_header_ptr = get_header_tar_bz2;
			break;
#endif
		default:
			show_usage();
		}
	}

	/* Check if we are reading from stdin */
	if ((argv[optind]) && (*argv[optind] == '-')) {
		/* Default is to read from stdin, so just skip to next arg */
		optind++;
	}

	/* Setup an array of filenames to work with */
	/* TODO: This is the same as in ar, seperate function ? */
	while (optind < argc) {
		tar_handle->accept = add_to_list(tar_handle->accept, argv[optind]);
		optind++;
	}

	if ((tar_handle->accept) || (tar_handle->reject)) {
		tar_handle->filter = filter_accept_reject_list;
	}

	if ((base_dir) && (chdir(base_dir))) {
		perror_msg_and_die("Couldnt chdir");
	}

#ifdef CONFIG_FEATURE_TAR_CREATE
	/* create an archive */
	if (tar_create == TRUE) {
		int verboseFlag = FALSE;
		int gzipFlag = FALSE;

# ifdef CONFIG_FEATURE_TAR_GZIP
		if (get_header_ptr == get_header_tar_gz) {
			gzipFlag = TRUE;
		}
# endif /* CONFIG_FEATURE_TAR_GZIP */

		if (tar_handle->action_header == header_verbose_list) {
			verboseFlag = TRUE;
		}
		writeTarFile(tar_filename, verboseFlag, tar_handle->accept,
			tar_handle->reject, gzipFlag);
	} else 
#endif /* CONFIG_FEATURE_TAR_CREATE */
	{
		if ((tar_filename[0] == '-') && (tar_filename[1] == '\0')) {
			tar_handle->src_fd = fileno(stdin);
			tar_handle->seek = seek_by_char;
		} else {
			tar_handle->src_fd = xopen(tar_filename, O_RDONLY);
		}
		while (get_header_ptr(tar_handle) == EXIT_SUCCESS);

		/* Ckeck that every file that should have been extracted was */
		while (tar_handle->accept) {
			if (find_list_entry(tar_handle->reject, tar_handle->accept->data) == NULL) {
				if (find_list_entry(tar_handle->passed, tar_handle->accept->data) == NULL) {
					error_msg_and_die("%s: Not found in archive\n", tar_handle->accept->data);
				}
			}
			tar_handle->accept = tar_handle->accept->link;
		}
	}

#ifdef CONFIG_FEATURE_CLEAN_UP
	if (tar_handle->src_fd != fileno(stdin)) {
		close(tar_handle->src_fd);
	}
#endif /* CONFIG_FEATURE_CLEAN_UP */

	return(EXIT_SUCCESS);
}
