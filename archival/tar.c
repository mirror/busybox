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
#include "unarchive.h"
#include "busybox.h"

#ifdef CONFIG_FEATURE_TAR_CREATE

/* Tar file constants  */
# define TAR_MAGIC          "ustar"        /* ustar and a null */
# define TAR_VERSION        "  "           /* Be compatable with GNU tar format */

# ifndef MAJOR
#  define MAJOR(dev) (((dev)>>8)&0xff)
#  define MINOR(dev) ((dev)&0xff)
# endif

static const int TAR_BLOCK_SIZE = 512;
static const int TAR_MAGIC_LEN = 6;
static const int TAR_VERSION_LEN = 2;

/* POSIX tar Header Block, from POSIX 1003.1-1990  */
enum { NAME_SIZE = 100 }; /* because gcc won't let me use 'static const int' */
struct TarHeader
{                            /* byte offset */
	char name[NAME_SIZE];         /*   0-99 */
	char mode[8];                 /* 100-107 */
	char uid[8];                  /* 108-115 */
	char gid[8];                  /* 116-123 */
	char size[12];                /* 124-135 */
	char mtime[12];               /* 136-147 */
	char chksum[8];               /* 148-155 */
	char typeflag;                /* 156-156 */
	char linkname[NAME_SIZE];     /* 157-256 */
	char magic[6];                /* 257-262 */
	char version[2];              /* 263-264 */
	char uname[32];               /* 265-296 */
	char gname[32];               /* 297-328 */
	char devmajor[8];             /* 329-336 */
	char devminor[8];             /* 337-344 */
	char prefix[155];             /* 345-499 */
	char padding[12];             /* 500-512 (pad to exactly the TAR_BLOCK_SIZE) */
};
typedef struct TarHeader TarHeader;

/*
** writeTarFile(),  writeFileToTarball(), and writeTarHeader() are
** the only functions that deal with the HardLinkInfo structure.
** Even these functions use the xxxHardLinkInfo() functions.
*/
typedef struct HardLinkInfo HardLinkInfo;
struct HardLinkInfo
{
	HardLinkInfo *next;           /* Next entry in list */
	dev_t dev;                    /* Device number */
	ino_t ino;                    /* Inode number */
	short linkCount;              /* (Hard) Link Count */
	char name[1];                 /* Start of filename (must be last) */
};

/* Some info to be carried along when creating a new tarball */
struct TarBallInfo
{
	char* fileName;               /* File name of the tarball */
	int tarFd;                    /* Open-for-write file descriptor
									 for the tarball */
	struct stat statBuf;          /* Stat info for the tarball, letting
									 us know the inode and device that the
									 tarball lives, so we can avoid trying 
									 to include the tarball into itself */
	int verboseFlag;              /* Whether to print extra stuff or not */
	char** excludeList;           /* List of files to not include */
	HardLinkInfo *hlInfoHead;     /* Hard Link Tracking Information */
	HardLinkInfo *hlInfo;         /* Hard Link Info for the current file */
};
typedef struct TarBallInfo TarBallInfo;

/* A nice enum with all the possible tar file content types */
enum TarFileType 
{
	REGTYPE  = '0',            /* regular file */
	REGTYPE0 = '\0',           /* regular file (ancient bug compat)*/
	LNKTYPE  = '1',            /* hard link */
	SYMTYPE  = '2',            /* symbolic link */
	CHRTYPE  = '3',            /* character special */
	BLKTYPE  = '4',            /* block special */
	DIRTYPE  = '5',            /* directory */
	FIFOTYPE = '6',            /* FIFO special */
	CONTTYPE = '7',            /* reserved */
	GNULONGLINK = 'K',         /* GNU long (>100 chars) link name */
	GNULONGNAME = 'L',         /* GNU long (>100 chars) file name */
};
typedef enum TarFileType TarFileType;

/* Might be faster (and bigger) if the dev/ino were stored in numeric order;) */
static void
addHardLinkInfo (HardLinkInfo **hlInfoHeadPtr, dev_t dev, ino_t ino,
		short linkCount, const char *name)
{
	/* Note: hlInfoHeadPtr can never be NULL! */
	HardLinkInfo *hlInfo;

	hlInfo = (HardLinkInfo *)xmalloc(sizeof(HardLinkInfo)+strlen(name)+1);
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

static void
freeHardLinkInfo (HardLinkInfo **hlInfoHeadPtr)
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
static HardLinkInfo *
findHardLinkInfo (HardLinkInfo *hlInfo, dev_t dev, ino_t ino)
{
	while(hlInfo) {
		if ((ino == hlInfo->ino) && (dev == hlInfo->dev))
			break;
		hlInfo = hlInfo->next;
	}
	return(hlInfo);
}

/* Put an octal string into the specified buffer.
 * The number is zero and space padded and possibly null padded.
 * Returns TRUE if successful.  */ 
static int putOctal (char *cp, int len, long value)
{
	int tempLength;
	char tempBuffer[32];
	char *tempString = tempBuffer;

	/* Create a string of the specified length with an initial space,
	 * leading zeroes and the octal number, and a trailing null.  */
	sprintf (tempString, "%0*lo", len - 1, value);

	/* If the string is too large, suppress the leading space.  */
	tempLength = strlen (tempString) + 1;
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
	memcpy (cp, tempString, len);

	return TRUE;
}

/* Write out a tar header for the specified file/directory/whatever */
static int
writeTarHeader(struct TarBallInfo *tbInfo, const char *header_name,
		const char *real_name, struct stat *statbuf)
{
	long chksum=0;
	struct TarHeader header;
	const unsigned char *cp = (const unsigned char *) &header;
	ssize_t size = sizeof(struct TarHeader);
		
	memset( &header, 0, size);

	strncpy(header.name, header_name, sizeof(header.name)); 

	putOctal(header.mode, sizeof(header.mode), statbuf->st_mode);
	putOctal(header.uid, sizeof(header.uid), statbuf->st_uid);
	putOctal(header.gid, sizeof(header.gid), statbuf->st_gid);
	putOctal(header.size, sizeof(header.size), 0); /* Regular file size is handled later */
	putOctal(header.mtime, sizeof(header.mtime), statbuf->st_mtime);
	strncpy(header.magic, TAR_MAGIC TAR_VERSION, 
			TAR_MAGIC_LEN + TAR_VERSION_LEN );

	/* Enter the user and group names (default to root if it fails) */
	my_getpwuid(header.uname, statbuf->st_uid);
	if (! *header.uname)
		strcpy(header.uname, "root");
	my_getgrgid(header.gname, statbuf->st_gid);
	if (! *header.uname)
		strcpy(header.uname, "root");

	if (tbInfo->hlInfo) {
		/* This is a hard link */
		header.typeflag = LNKTYPE;
		strncpy(header.linkname, tbInfo->hlInfo->name, sizeof(header.linkname));
	} else if (S_ISLNK(statbuf->st_mode)) {
		char *lpath = xreadlink(real_name);
		if (!lpath) /* Already printed err msg inside xreadlink() */
			return ( FALSE);
		header.typeflag  = SYMTYPE;
		strncpy(header.linkname, lpath, sizeof(header.linkname)); 
		free(lpath);
	} else if (S_ISDIR(statbuf->st_mode)) {
		header.typeflag  = DIRTYPE;
		strncat(header.name, "/", sizeof(header.name)); 
	} else if (S_ISCHR(statbuf->st_mode)) {
		header.typeflag  = CHRTYPE;
		putOctal(header.devmajor, sizeof(header.devmajor), MAJOR(statbuf->st_rdev));
		putOctal(header.devminor, sizeof(header.devminor), MINOR(statbuf->st_rdev));
	} else if (S_ISBLK(statbuf->st_mode)) {
		header.typeflag  = BLKTYPE;
		putOctal(header.devmajor, sizeof(header.devmajor), MAJOR(statbuf->st_rdev));
		putOctal(header.devminor, sizeof(header.devminor), MINOR(statbuf->st_rdev));
	} else if (S_ISFIFO(statbuf->st_mode)) {
		header.typeflag  = FIFOTYPE;
	} else if (S_ISREG(statbuf->st_mode)) {
		header.typeflag  = REGTYPE;
		putOctal(header.size, sizeof(header.size), statbuf->st_size);
	} else {
		error_msg("%s: Unknown file type", real_name);
		return ( FALSE);
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
	if ((size=full_write(tbInfo->tarFd, (char*)&header, sizeof(struct TarHeader))) < 0) {
		error_msg(io_error, real_name, strerror(errno)); 
		return ( FALSE);
	}
	/* Pad the header up to the tar block size */
	for (; size<TAR_BLOCK_SIZE; size++) {
		write(tbInfo->tarFd, "\0", 1);
	}
	/* Now do the verbose thing (or not) */
	if (tbInfo->verboseFlag==TRUE) {
		FILE *vbFd = stdout;
		if (tbInfo->tarFd == fileno(stdout))	// If the archive goes to stdout, verbose to stderr
			vbFd = stderr;
		fprintf(vbFd, "%s\n", header.name);
	}

	return ( TRUE);
}

# if defined CONFIG_FEATURE_TAR_EXCLUDE
static int exclude_file(char **excluded_files, const char *file)
{
	int i;

	if (excluded_files == NULL)
		return 0;

	for (i = 0; excluded_files[i] != NULL; i++) {
		if (excluded_files[i][0] == '/') {
			if (fnmatch(excluded_files[i], file,
						FNM_PATHNAME | FNM_LEADING_DIR) == 0)
				return 1;
		} else {
			const char *p;

			for (p = file; p[0] != '\0'; p++) {
				if ((p == file || p[-1] == '/') && p[0] != '/' &&
						fnmatch(excluded_files[i], p,
							FNM_PATHNAME | FNM_LEADING_DIR) == 0)
					return 1;
			}
		}
	}

	return 0;
}
#endif

static int writeFileToTarball(const char *fileName, struct stat *statbuf, void* userData)
{
	struct TarBallInfo *tbInfo = (struct TarBallInfo *)userData;
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
			addHardLinkInfo (&tbInfo->hlInfoHead, statbuf->st_dev,
					statbuf->st_ino, statbuf->st_nlink, fileName);
	}

	/* It is against the rules to archive a socket */
	if (S_ISSOCK(statbuf->st_mode)) {
		error_msg("%s: socket ignored", fileName);
		return( TRUE);
	}

	/* It is a bad idea to store the archive we are in the process of creating,
	 * so check the device and inode to be sure that this particular file isn't
	 * the new tarball */
	if (tbInfo->statBuf.st_dev == statbuf->st_dev &&
			tbInfo->statBuf.st_ino == statbuf->st_ino) {
		error_msg("%s: file is the archive; skipping", fileName);
		return( TRUE);
	}

	header_name = fileName;
	while (header_name[0] == '/') {
		static int alreadyWarned=FALSE;
		if (alreadyWarned==FALSE) {
			error_msg("Removing leading '/' from member names");
			alreadyWarned=TRUE;
		}
		header_name++;
	}

	if (strlen(fileName) >= NAME_SIZE) {
		error_msg(name_longer_than_foo, NAME_SIZE);
		return ( TRUE);
	}

	if (header_name[0] == '\0')
		return TRUE;

# if defined CONFIG_FEATURE_TAR_EXCLUDE
	if (exclude_file(tbInfo->excludeList, header_name)) {
		return SKIP;
	}
# endif //CONFIG_FEATURE_TAR_EXCLUDE

	if (writeTarHeader(tbInfo, header_name, fileName, statbuf)==FALSE) {
		return( FALSE);
	} 

	/* Now, if the file is a regular file, copy it out to the tarball */
	if ((tbInfo->hlInfo == NULL)
	&&  (S_ISREG(statbuf->st_mode))) {
		int  inputFileFd;
		char buffer[BUFSIZ];
		ssize_t size=0, readSize=0;

		/* open the file we want to archive, and make sure all is well */
		if ((inputFileFd = open(fileName, O_RDONLY)) < 0) {
			error_msg("%s: Cannot open: %s", fileName, strerror(errno));
			return( FALSE);
		}
		
		/* write the file to the archive */
		while ( (size = full_read(inputFileFd, buffer, sizeof(buffer))) > 0 ) {
			if (full_write(tbInfo->tarFd, buffer, size) != size ) {
				/* Output file seems to have a problem */
				error_msg(io_error, fileName, strerror(errno)); 
				return( FALSE);
			}
			readSize+=size;
		}
		if (size == -1) {
			error_msg(io_error, fileName, strerror(errno)); 
			return( FALSE);
		}
		/* Pad the file up to the tar block size */
		for (; (readSize%TAR_BLOCK_SIZE) != 0; readSize++) {
			write(tbInfo->tarFd, "\0", 1);
		}
		close( inputFileFd);
	}

	return( TRUE);
}

static int writeTarFile(const char* tarName, int verboseFlag, char **argv,
		char** excludeList)
{
	int tarFd=-1;
	int errorFlag=FALSE;
	ssize_t size;
	struct TarBallInfo tbInfo;
	tbInfo.verboseFlag = verboseFlag;
	tbInfo.hlInfoHead = NULL;

	/* Make sure there is at least one file to tar up.  */
	if (*argv == NULL)
		error_msg_and_die("Cowardly refusing to create an empty archive");

	/* Open the tar file for writing.  */
	if (tarName == NULL)
		tbInfo.tarFd = fileno(stdout);
	else
		tbInfo.tarFd = open (tarName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (tbInfo.tarFd < 0) {
		perror_msg( "Error opening '%s'", tarName);
		freeHardLinkInfo(&tbInfo.hlInfoHead);
		return ( FALSE);
	}
	tbInfo.excludeList=excludeList;
	/* Store the stat info for the tarball's file, so
	 * can avoid including the tarball into itself....  */
	if (fstat(tbInfo.tarFd, &tbInfo.statBuf) < 0)
		error_msg_and_die(io_error, tarName, strerror(errno)); 

	/* Read the directory/files and iterate over them one at a time */
	while (*argv != NULL) {
		if (! recursive_action(*argv++, TRUE, FALSE, FALSE,
					writeFileToTarball, writeFileToTarball, 
					(void*) &tbInfo)) {
			errorFlag = TRUE;
		}
	}
	/* Write two empty blocks to the end of the archive */
	for (size=0; size<(2*TAR_BLOCK_SIZE); size++) {
		write(tbInfo.tarFd, "\0", 1);
	}

	/* To be pedantically correct, we would check if the tarball
	 * is smaller than 20 tar blocks, and pad it if it was smaller,
	 * but that isn't necessary for GNU tar interoperability, and
	 * so is considered a waste of space */

	/* Hang up the tools, close up shop, head home */
	close(tarFd);
	if (errorFlag) {
		error_msg("Error exit delayed from previous errors");
		freeHardLinkInfo(&tbInfo.hlInfoHead);
		return(FALSE);
	}
	freeHardLinkInfo(&tbInfo.hlInfoHead);
	return( TRUE);
}
#endif //tar_create

void append_file_to_list(const char *new_name, char ***list, int *list_count)
{
	*list = realloc(*list, sizeof(char *) * (*list_count + 2));
	(*list)[*list_count] = xstrdup(new_name);
	(*list_count)++;
	(*list)[*list_count] = NULL;
}

void append_file_list_to_list(char *filename, char ***name_list, int *num_of_entries)
{
	FILE *src_stream;
	char *line;
	
	src_stream = xfopen(filename, "r");
	while ((line = get_line_from_file(src_stream)) != NULL) {
		chomp (line);
		append_file_to_list(line, name_list, num_of_entries);
		free(line);
	}
	fclose(src_stream);
}

#ifdef CONFIG_FEATURE_TAR_EXCLUDE
/*
 * Create a list of names that are in the include list AND NOT in the exclude lists
 */
char **list_and_not_list(char **include_list, char **exclude_list)
{
	char **new_include_list = NULL;
	int new_include_count = 0;
	int include_count = 0;
	int exclude_count;

	if (include_list == NULL) {
		return(NULL);
	}
	
	while (include_list[include_count] != NULL) {
		int found = FALSE;
		exclude_count = 0;
		while (exclude_list[exclude_count] != NULL) {
			if (strcmp(include_list[include_count], exclude_list[exclude_count]) == 0) {
				found = TRUE;
				break;
			}
			exclude_count++;
		}

		if (! found) {
			new_include_list = realloc(new_include_list, sizeof(char *) * (include_count + 2));
			new_include_list[new_include_count] = include_list[include_count];
			new_include_count++;
		} else {
			free(include_list[include_count]);
		}
		include_count++;
	}
	new_include_list[new_include_count] = NULL;
	return(new_include_list);
}
#endif

int tar_main(int argc, char **argv)
{
	enum untar_funct_e {
		/* This is optional */
		untar_unzip = 1,
		/* Require one and only one of these */
		untar_list = 2,
		untar_create = 4,
		untar_extract = 8
	};

	FILE *src_stream = NULL;
	FILE *uncompressed_stream = NULL;
	char **include_list = NULL;
	char **exclude_list = NULL;
	char *src_filename = NULL;
	char *dst_prefix = NULL;
	int opt;
	unsigned short untar_funct = 0;
	unsigned short untar_funct_required = 0;
	unsigned short extract_function = 0;
	int include_list_count = 0;
#ifdef CONFIG_FEATURE_TAR_EXCLUDE
	int exclude_list_count = 0;
#endif
#ifdef CONFIG_FEATURE_TAR_GZIP
	int gunzip_pid;
	int gz_fd = 0;
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

	while ((opt = getopt(argc, argv, "ctxT:X:C:f:Opvz")) != -1) {
		switch (opt) {

		/* One and only one of these is required */
		case 'c':
			untar_funct_required |= untar_create;
			break;
		case 't':
			untar_funct_required |= untar_list;
			extract_function |= extract_list |extract_unconditional;
			break;
		case 'x':
			untar_funct_required |= untar_extract;
			extract_function |= (extract_all_to_fs | extract_unconditional | extract_create_leading_dirs);
			break;

		/* These are optional */
		/* Exclude or Include files listed in <filename>*/
#ifdef CONFIG_FEATURE_TAR_EXCLUDE
		case 'X':
			append_file_list_to_list(optarg, &exclude_list, &exclude_list_count);
			break;
#endif
		case 'T':
			// by default a list is an include list
			append_file_list_to_list(optarg, &include_list, &include_list_count);
			break;

		case 'C':	// Change to dir <optarg>
			/* Make sure dst_prefix ends in a '/' */
			dst_prefix = concat_path_file(optarg, "/");
			break;
		case 'f':	// archive filename
			if (strcmp(optarg, "-") == 0) {
				src_filename = NULL;
			} else {
				src_filename = xstrdup(optarg);
			}
			break;
		case 'O':
			extract_function |= extract_to_stdout;
			break;
		case 'p':
			break;
		case 'v':
			if (extract_function & extract_list) {
				extract_function |= extract_verbose_list;
			}
			extract_function |= extract_list;
			break;
#ifdef CONFIG_FEATURE_TAR_GZIP
		case 'z':
			untar_funct |= untar_unzip;
			break;
#endif
		default:
			show_usage();
		}
	}

	/* Make sure the valid arguments were passed */
	if (untar_funct_required == 0) {
		error_msg_and_die("You must specify one of the `-ctx' options");
	}
	if ((untar_funct_required != untar_create) && 
			(untar_funct_required != untar_extract) &&
			(untar_funct_required != untar_list)) {
		error_msg_and_die("You may not specify more than one `ctx' option.");
	}
	untar_funct |= untar_funct_required;

	/* Setup an array of filenames to work with */
	while (optind < argc) {
		append_file_to_list(argv[optind], &include_list, &include_list_count);
		optind++;
	}
	if (extract_function & (extract_list | extract_all_to_fs)) {
		if (dst_prefix == NULL) {
			dst_prefix = xstrdup("./");
		}

		/* Setup the source of the tar data */
		if (src_filename != NULL) {
			src_stream = xfopen(src_filename, "r");
		} else {
			src_stream = stdin;
		}
#ifdef CONFIG_FEATURE_TAR_GZIP
		/* Get a binary tree of all the tar file headers */
		if (untar_funct & untar_unzip) {
			uncompressed_stream = gz_open(src_stream, &gunzip_pid);
		} else
#endif // CONFIG_FEATURE_TAR_GZIP
			uncompressed_stream = src_stream;
		
		/* extract or list archive */
		unarchive(uncompressed_stream, stdout, &get_header_tar, extract_function, dst_prefix, include_list, exclude_list);
		fclose(uncompressed_stream);
	}
#ifdef CONFIG_FEATURE_TAR_CREATE
	/* create an archive */
	else if (untar_funct & untar_create) {
		int verboseFlag = FALSE;

#ifdef CONFIG_FEATURE_TAR_GZIP
		if (untar_funct & untar_unzip) {
			error_msg_and_die("Creation of compressed tarfile not internally support by tar, pipe to busybox gunzip");
		}
#endif // CONFIG_FEATURE_TAR_GZIP
		if (extract_function & extract_verbose_list) {
			verboseFlag = TRUE;
		}
		writeTarFile(src_filename, verboseFlag, include_list, exclude_list);
	}
#endif // CONFIG_FEATURE_TAR_CREATE

	/* Cleanups */
#ifdef CONFIG_FEATURE_TAR_GZIP
	if (untar_funct & untar_unzip) {
		fclose(src_stream);
		close(gz_fd);
		gz_close(gunzip_pid);
	}
#endif // CONFIG_FEATURE_TAR_GZIP
#ifdef CONFIG_FEATURE_CLEAN_UP
	if (src_filename) {
		free(src_filename);
	}
#endif
	return(EXIT_SUCCESS);
}
