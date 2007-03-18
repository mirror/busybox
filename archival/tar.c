/* vi: set sw=4 ts=4: */
/*
 * Mini tar implementation for busybox
 *
 * Modified to use common extraction code used by ar, cpio, dpkg-deb, dpkg
 *  Glenn McGrath <bug1@iinet.net.au>
 *
 * Note, that as of BusyBox-0.43, tar has been completely rewritten from the
 * ground up.  It still has remnants of the old code lying about, but it is
 * very different now (i.e., cleaner, less global variables, etc.)
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Based in part in the tar implementation in sash
 *  Copyright (c) 1999 by David I. Bell
 *  Permission is granted to use, distribute, or modify this source,
 *  provided that this copyright notice remains intact.
 *  Permission to distribute sash derived code under the GPL has been granted.
 *
 * Based in part on the tar implementation from busybox-0.28
 *  Copyright (C) 1995 Bruce Perens
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <fnmatch.h>
#include <getopt.h>
#include "busybox.h"
#include "unarchive.h"

#if ENABLE_FEATURE_TAR_CREATE

/* Tar file constants  */

#define TAR_BLOCK_SIZE		512

/* POSIX tar Header Block, from POSIX 1003.1-1990  */
#define NAME_SIZE      100
#define NAME_SIZE_STR "100"
typedef struct TarHeader TarHeader;
struct TarHeader {		  /* byte offset */
	char name[NAME_SIZE];     /*   0-99 */
	char mode[8];             /* 100-107 */
	char uid[8];              /* 108-115 */
	char gid[8];              /* 116-123 */
	char size[12];            /* 124-135 */
	char mtime[12];           /* 136-147 */
	char chksum[8];           /* 148-155 */
	char typeflag;            /* 156-156 */
	char linkname[NAME_SIZE]; /* 157-256 */
	char magic[6];            /* 257-262 */
	char version[2];          /* 263-264 */
	char uname[32];           /* 265-296 */
	char gname[32];           /* 297-328 */
	char devmajor[8];         /* 329-336 */
	char devminor[8];         /* 337-344 */
	char prefix[155];         /* 345-499 */
	char padding[12];         /* 500-512 (pad to exactly the TAR_BLOCK_SIZE) */
};

/*
** writeTarFile(), writeFileToTarball(), and writeTarHeader() are
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
typedef struct TarBallInfo TarBallInfo;
struct TarBallInfo {
	int tarFd;				/* Open-for-write file descriptor
							   for the tarball */
	struct stat statBuf;	/* Stat info for the tarball, letting
							   us know the inode and device that the
							   tarball lives, so we can avoid trying
							   to include the tarball into itself */
	int verboseFlag;		/* Whether to print extra stuff or not */
	const llist_t *excludeList;	/* List of files to not include */
	HardLinkInfo *hlInfoHead;	/* Hard Link Tracking Information */
	HardLinkInfo *hlInfo;	/* Hard Link Info for the current file */
};

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
static void addHardLinkInfo(HardLinkInfo ** hlInfoHeadPtr,
					struct stat *statbuf,
					const char *fileName)
{
	/* Note: hlInfoHeadPtr can never be NULL! */
	HardLinkInfo *hlInfo;

	hlInfo = xmalloc(sizeof(HardLinkInfo) + strlen(fileName));
	hlInfo->next = *hlInfoHeadPtr;
	*hlInfoHeadPtr = hlInfo;
	hlInfo->dev = statbuf->st_dev;
	hlInfo->ino = statbuf->st_ino;
	hlInfo->linkCount = statbuf->st_nlink;
	strcpy(hlInfo->name, fileName);
}

static void freeHardLinkInfo(HardLinkInfo ** hlInfoHeadPtr)
{
	HardLinkInfo *hlInfo;
	HardLinkInfo *hlInfoNext;

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
static HardLinkInfo *findHardLinkInfo(HardLinkInfo * hlInfo, struct stat *statbuf)
{
	while (hlInfo) {
		if ((statbuf->st_ino == hlInfo->ino) && (statbuf->st_dev == hlInfo->dev))
			break;
		hlInfo = hlInfo->next;
	}
	return hlInfo;
}

/* Put an octal string into the specified buffer.
 * The number is zero padded and possibly null terminated.
 * Stores low-order bits only if whole value does not fit. */
static void putOctal(char *cp, int len, off_t value)
{
	char tempBuffer[sizeof(off_t)*3+1];
	char *tempString = tempBuffer;
	int width;

	width = sprintf(tempBuffer, "%0*"OFF_FMT"o", len, value);
	tempString += (width - len);

	/* If string has leading zeroes, we can drop one */
	/* and field will have trailing '\0' */
	/* (increases chances of compat with other tars) */
	if (tempString[0] == '0')
		tempString++;

	/* Copy the string to the field */
	memcpy(cp, tempString, len);
}
#define PUT_OCTAL(a, b) putOctal((a), sizeof(a), (b))

static void chksum_and_xwrite(int fd, struct TarHeader* hp)
{
	const unsigned char *cp;
	int chksum, size;

	strcpy(hp->magic, "ustar  ");

	/* Calculate and store the checksum (i.e., the sum of all of the bytes of
	 * the header).  The checksum field must be filled with blanks for the
	 * calculation.  The checksum field is formatted differently from the
	 * other fields: it has 6 digits, a null, then a space -- rather than
	 * digits, followed by a null like the other fields... */
	memset(hp->chksum, ' ', sizeof(hp->chksum));
	cp = (const unsigned char *) hp;
	chksum = 0;
	size = sizeof(*hp);
	do { chksum += *cp++; } while (--size);
	putOctal(hp->chksum, sizeof(hp->chksum)-1, chksum);

	/* Now write the header out to disk */
	xwrite(fd, hp, sizeof(*hp));
}

#if ENABLE_FEATURE_TAR_GNU_EXTENSIONS
static void writeLongname(int fd, int type, const char *name, int dir)
{
	static const struct {
		char mode[8];             /* 100-107 */
		char uid[8];              /* 108-115 */
		char gid[8];              /* 116-123 */
		char size[12];            /* 124-135 */
		char mtime[12];           /* 136-147 */
	} prefilled = {
		"0000000",
		"0000000",
		"0000000",
		"00000000000",
		"00000000000",
	};
	struct TarHeader header;
	int size;

	dir = !!dir; /* normalize: 0/1 */
	size = strlen(name) + 1 + dir; /* GNU tar uses strlen+1 */
	/* + dir: account for possible '/' */

	memset(&header, 0, sizeof(header));
	strcpy(header.name, "././@LongLink");
	memcpy(header.mode, prefilled.mode, sizeof(prefilled));
	PUT_OCTAL(header.size, size);
	header.typeflag = type;
	chksum_and_xwrite(fd, &header);

	/* Write filename[/] and pad the block. */
	/* dir=0: writes 'name<NUL>', pads */
	/* dir=1: writes 'name', writes '/<NUL>', pads */
	dir *= 2;
	xwrite(fd, name, size - dir);
	xwrite(fd, "/", dir);
	size = (-size) & (TAR_BLOCK_SIZE-1);
	memset(&header, 0, size);
	xwrite(fd, &header, size);
}
#endif

/* Write out a tar header for the specified file/directory/whatever */
void BUG_tar_header_size(void);
static int writeTarHeader(struct TarBallInfo *tbInfo,
		const char *header_name, const char *fileName, struct stat *statbuf)
{
	struct TarHeader header;

	if (sizeof(header) != 512)
		BUG_tar_header_size();

	memset(&header, 0, sizeof(struct TarHeader));

	strncpy(header.name, header_name, sizeof(header.name));

	/* POSIX says to mask mode with 07777. */
	PUT_OCTAL(header.mode, statbuf->st_mode & 07777);
	PUT_OCTAL(header.uid, statbuf->st_uid);
	PUT_OCTAL(header.gid, statbuf->st_gid);
	memset(header.size, '0', sizeof(header.size)-1); /* Regular file size is handled later */
	PUT_OCTAL(header.mtime, statbuf->st_mtime);

	/* Enter the user and group names */
	safe_strncpy(header.uname, get_cached_username(statbuf->st_uid), sizeof(header.uname));
	safe_strncpy(header.gname, get_cached_groupname(statbuf->st_gid), sizeof(header.gname));

	if (tbInfo->hlInfo) {
		/* This is a hard link */
		header.typeflag = LNKTYPE;
		strncpy(header.linkname, tbInfo->hlInfo->name,
				sizeof(header.linkname));
#if ENABLE_FEATURE_TAR_GNU_EXTENSIONS
		/* Write out long linkname if needed */
		if (header.linkname[sizeof(header.linkname)-1])
			writeLongname(tbInfo->tarFd, GNULONGLINK,
					tbInfo->hlInfo->name, 0);
#endif
	} else if (S_ISLNK(statbuf->st_mode)) {
		char *lpath = xreadlink(fileName);
		if (!lpath)		/* Already printed err msg inside xreadlink() */
			return FALSE;
		header.typeflag = SYMTYPE;
		strncpy(header.linkname, lpath, sizeof(header.linkname));
#if ENABLE_FEATURE_TAR_GNU_EXTENSIONS
		/* Write out long linkname if needed */
		if (header.linkname[sizeof(header.linkname)-1])
			writeLongname(tbInfo->tarFd, GNULONGLINK, lpath, 0);
#else
		/* If it is larger than 100 bytes, bail out */
		if (header.linkname[sizeof(header.linkname)-1]) {
			free(lpath);
			bb_error_msg("names longer than "NAME_SIZE_STR" chars not supported");
			return FALSE;
		}
#endif
		free(lpath);
	} else if (S_ISDIR(statbuf->st_mode)) {
		header.typeflag = DIRTYPE;
		/* Append '/' only if there is a space for it */
		if (!header.name[sizeof(header.name)-1])
			header.name[strlen(header.name)] = '/';
	} else if (S_ISCHR(statbuf->st_mode)) {
		header.typeflag = CHRTYPE;
		PUT_OCTAL(header.devmajor, major(statbuf->st_rdev));
		PUT_OCTAL(header.devminor, minor(statbuf->st_rdev));
	} else if (S_ISBLK(statbuf->st_mode)) {
		header.typeflag = BLKTYPE;
		PUT_OCTAL(header.devmajor, major(statbuf->st_rdev));
		PUT_OCTAL(header.devminor, minor(statbuf->st_rdev));
	} else if (S_ISFIFO(statbuf->st_mode)) {
		header.typeflag = FIFOTYPE;
	} else if (S_ISREG(statbuf->st_mode)) {
		if (sizeof(statbuf->st_size) > 4
		 && statbuf->st_size > (off_t)0777777777777LL
		) {
			bb_error_msg_and_die("cannot store file '%s' "
				"of size %"OFF_FMT"d, aborting",
				fileName, statbuf->st_size);
		}
		header.typeflag = REGTYPE;
		PUT_OCTAL(header.size, statbuf->st_size);
	} else {
		bb_error_msg("%s: unknown file type", fileName);
		return FALSE;
	}

#if ENABLE_FEATURE_TAR_GNU_EXTENSIONS
	/* Write out long name if needed */
	/* (we, like GNU tar, output long linkname *before* long name) */
	if (header.name[sizeof(header.name)-1])
		writeLongname(tbInfo->tarFd, GNULONGNAME,
				header_name, S_ISDIR(statbuf->st_mode));
#endif

	/* Now write the header out to disk */
	chksum_and_xwrite(tbInfo->tarFd, &header);

	/* Now do the verbose thing (or not) */
	if (tbInfo->verboseFlag) {
		FILE *vbFd = stdout;

		if (tbInfo->tarFd == STDOUT_FILENO)	/* If the archive goes to stdout, verbose to stderr */
			vbFd = stderr;
		/* GNU "tar cvvf" prints "extended" listing a-la "ls -l" */
		/* We don't have such excesses here: for us "v" == "vv" */
		/* '/' is probably a GNUism */
		fprintf(vbFd, "%s%s\n", header_name,
				S_ISDIR(statbuf->st_mode) ? "/" : "");
	}

	return TRUE;
}

#if ENABLE_FEATURE_TAR_FROM
static int exclude_file(const llist_t *excluded_files, const char *file)
{
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
#else
#define exclude_file(excluded_files, file) 0
#endif

static int writeFileToTarball(const char *fileName, struct stat *statbuf,
			void *userData, int depth ATTRIBUTE_UNUSED)
{
	struct TarBallInfo *tbInfo = (struct TarBallInfo *) userData;
	const char *header_name;
	int inputFileFd = -1;

	/*
	 * Check to see if we are dealing with a hard link.
	 * If so -
	 * Treat the first occurance of a given dev/inode as a file while
	 * treating any additional occurances as hard links.  This is done
	 * by adding the file information to the HardLinkInfo linked list.
	 */
	tbInfo->hlInfo = NULL;
	if (statbuf->st_nlink > 1) {
		tbInfo->hlInfo = findHardLinkInfo(tbInfo->hlInfoHead, statbuf);
		if (tbInfo->hlInfo == NULL)
			addHardLinkInfo(&tbInfo->hlInfoHead, statbuf, fileName);
	}

	/* It is against the rules to archive a socket */
	if (S_ISSOCK(statbuf->st_mode)) {
		bb_error_msg("%s: socket ignored", fileName);
		return TRUE;
	}

	/* It is a bad idea to store the archive we are in the process of creating,
	 * so check the device and inode to be sure that this particular file isn't
	 * the new tarball */
	if (tbInfo->statBuf.st_dev == statbuf->st_dev &&
		tbInfo->statBuf.st_ino == statbuf->st_ino) {
		bb_error_msg("%s: file is the archive; skipping", fileName);
		return TRUE;
	}

	header_name = fileName;
	while (header_name[0] == '/') {
		static int alreadyWarned = FALSE;

		if (alreadyWarned == FALSE) {
			bb_error_msg("removing leading '/' from member names");
			alreadyWarned = TRUE;
		}
		header_name++;
	}

#if !ENABLE_FEATURE_TAR_GNU_EXTENSIONS
	if (strlen(fileName) >= NAME_SIZE) {
		bb_error_msg("names longer than "NAME_SIZE_STR" chars not supported");
		return TRUE;
	}
#endif

	if (header_name[0] == '\0')
		return TRUE;

	if (exclude_file(tbInfo->excludeList, header_name))
		return SKIP;

	/* Is this a regular file? */
	if (tbInfo->hlInfo == NULL && S_ISREG(statbuf->st_mode)) {
		/* open the file we want to archive, and make sure all is well */
		inputFileFd = open(fileName, O_RDONLY);
		if (inputFileFd < 0) {
			bb_perror_msg("%s: cannot open", fileName);
			return FALSE;
		}
	}

	/* Add an entry to the tarball */
	if (writeTarHeader(tbInfo, header_name, fileName, statbuf) == FALSE) {
		return FALSE;
	}

	/* If it was a regular file, write out the body */
	if (inputFileFd >= 0) {
		size_t readSize;
		/* Wwrite the file to the archive. */
		/* We record size into header first, */
		/* and then write out file. If file shrinks in between, */
		/* tar will be corrupted. So we don't allow for that. */
		/* NB: GNU tar 1.16 warns and pads with zeroes */
		/* or even seeks back and updates header */
		bb_copyfd_exact_size(inputFileFd, tbInfo->tarFd, statbuf->st_size);
		////off_t readSize;
		////readSize = bb_copyfd_size(inputFileFd, tbInfo->tarFd, statbuf->st_size);
		////if (readSize != statbuf->st_size && readSize >= 0) {
		////	bb_error_msg_and_die("short read from %s, aborting", fileName);
		////}

		/* Check that file did not grow in between? */
		/* if (safe_read(inputFileFd, 1) == 1) warn but continue? */

		close(inputFileFd);

		/* Pad the file up to the tar block size */
		/* (a few tricks here in the name of code size) */
		readSize = (-(int)statbuf->st_size) & (TAR_BLOCK_SIZE-1);
		memset(bb_common_bufsiz1, 0, readSize);
		xwrite(tbInfo->tarFd, bb_common_bufsiz1, readSize);
	}

	return TRUE;
}

static int writeTarFile(const int tar_fd, const int verboseFlag,
	const unsigned long dereferenceFlag, const llist_t *include,
	const llist_t *exclude, const int gzip)
{
	pid_t gzipPid = 0;
	int errorFlag = FALSE;
	struct TarBallInfo tbInfo;

	tbInfo.hlInfoHead = NULL;

	fchmod(tar_fd, 0644);
	tbInfo.tarFd = tar_fd;
	tbInfo.verboseFlag = verboseFlag;

	/* Store the stat info for the tarball's file, so
	 * can avoid including the tarball into itself....  */
	if (fstat(tbInfo.tarFd, &tbInfo.statBuf) < 0)
		bb_perror_msg_and_die("cannot stat tar file");

	if ((ENABLE_FEATURE_TAR_GZIP || ENABLE_FEATURE_TAR_BZIP2) && gzip) {
		int gzipDataPipe[2] = { -1, -1 };
		int gzipStatusPipe[2] = { -1, -1 };
		volatile int vfork_exec_errno = 0;
		char *zip_exec = (gzip == 1) ? "gzip" : "bzip2";

		if (pipe(gzipDataPipe) < 0 || pipe(gzipStatusPipe) < 0)
			bb_perror_msg_and_die("pipe");

		signal(SIGPIPE, SIG_IGN); /* we only want EPIPE on errors */

#if defined(__GNUC__) && __GNUC__
		/* Avoid vfork clobbering */
		(void) &include;
		(void) &errorFlag;
		(void) &zip_exec;
#endif

		gzipPid = vfork();

		if (gzipPid == 0) {
			dup2(gzipDataPipe[0], 0);
			close(gzipDataPipe[1]);

			dup2(tbInfo.tarFd, 1);

			close(gzipStatusPipe[0]);
			fcntl(gzipStatusPipe[1], F_SETFD, FD_CLOEXEC);	/* close on exec shows success */

			execlp(zip_exec, zip_exec, "-f", NULL);
			vfork_exec_errno = errno;

			close(gzipStatusPipe[1]);
			exit(-1);
		} else if (gzipPid > 0) {
			close(gzipDataPipe[0]);
			close(gzipStatusPipe[1]);

			while (1) {
				char buf;

				int n = full_read(gzipStatusPipe[0], &buf, 1);

				if (n == 0 && vfork_exec_errno != 0) {
					errno = vfork_exec_errno;
					bb_perror_msg_and_die("cannot exec %s", zip_exec);
				} else if ((n < 0) && (errno == EAGAIN || errno == EINTR))
					continue;	/* try it again */
				break;
			}
			close(gzipStatusPipe[0]);

			tbInfo.tarFd = gzipDataPipe[1];
		} else bb_perror_msg_and_die("vfork gzip");
	}

	tbInfo.excludeList = exclude;

	/* Read the directory/files and iterate over them one at a time */
	while (include) {
		if (!recursive_action(include->data, TRUE, dereferenceFlag,
				FALSE, writeFileToTarball, writeFileToTarball, &tbInfo, 0))
		{
			errorFlag = TRUE;
		}
		include = include->link;
	}
	/* Write two empty blocks to the end of the archive */
	memset(bb_common_bufsiz1, 0, 2*TAR_BLOCK_SIZE);
	xwrite(tbInfo.tarFd, bb_common_bufsiz1, 2*TAR_BLOCK_SIZE);

	/* To be pedantically correct, we would check if the tarball
	 * is smaller than 20 tar blocks, and pad it if it was smaller,
	 * but that isn't necessary for GNU tar interoperability, and
	 * so is considered a waste of space */

	/* Close so the child process (if any) will exit */
	close(tbInfo.tarFd);

	/* Hang up the tools, close up shop, head home */
	if (ENABLE_FEATURE_CLEAN_UP)
		freeHardLinkInfo(&tbInfo.hlInfoHead);

	if (errorFlag)
		bb_error_msg("error exit delayed from previous errors");

	if (gzipPid) {
		int status;
		if (waitpid(gzipPid, &status, 0) == -1)
			bb_perror_msg("waitpid");
		else if (!WIFEXITED(status) || WEXITSTATUS(status))
			/* gzip was killed or has exited with nonzero! */
			errorFlag = TRUE;
	}
	return errorFlag;
}
#else
int writeTarFile(const int tar_fd, const int verboseFlag,
	const unsigned long dereferenceFlag, const llist_t *include,
	const llist_t *exclude, const int gzip);
#endif /* FEATURE_TAR_CREATE */

#if ENABLE_FEATURE_TAR_FROM
static llist_t *append_file_list_to_list(llist_t *list)
{
	FILE *src_stream;
	llist_t *cur = list;
	llist_t *tmp;
	char *line;
	llist_t *newlist = NULL;

	while (cur) {
		src_stream = xfopen(cur->data, "r");
		tmp = cur;
		cur = cur->link;
		free(tmp);
		while ((line = xmalloc_getline(src_stream)) != NULL) {
			/* kill trailing '/' unless the string is just "/" */
			char *cp = last_char_is(line, '/');
			if (cp > line)
				*cp = '\0';
			llist_add_to(&newlist, line);
		}
		fclose(src_stream);
	}
	return newlist;
}
#else
#define append_file_list_to_list(x) 0
#endif

#if ENABLE_FEATURE_TAR_COMPRESS
static char get_header_tar_Z(archive_handle_t *archive_handle)
{
	/* Can't lseek over pipes */
	archive_handle->seek = seek_by_read;

	/* do the decompression, and cleanup */
	if (xread_char(archive_handle->src_fd) != 0x1f
	 || xread_char(archive_handle->src_fd) != 0x9d
	) {
		bb_error_msg_and_die("invalid magic");
	}

	archive_handle->src_fd = open_transformer(archive_handle->src_fd, uncompress);
	archive_handle->offset = 0;
	while (get_header_tar(archive_handle) == EXIT_SUCCESS)
		/* nothing */;

	/* Can only do one file at a time */
	return EXIT_FAILURE;
}
#else
#define get_header_tar_Z NULL
#endif

#ifdef CHECK_FOR_CHILD_EXITCODE
/* Looks like it isn't needed - tar detects malformed (truncated)
 * archive if e.g. bunzip2 fails */
static int child_error;

static void handle_SIGCHLD(int status)
{
	/* Actually, 'status' is a signo. We reuse it for other needs */

	/* Wait for any child without blocking */
	if (waitpid(-1, &status, WNOHANG) < 0)
		/* wait failed?! I'm confused... */
		return;

	if (WIFEXITED(status) && WEXITSTATUS(status)==0)
		/* child exited with 0 */
		return;
	/* Cannot happen?
	if(!WIFSIGNALED(status) && !WIFEXITED(status)) return; */
	child_error = 1;
}
#endif

enum {
	OPTBIT_KEEP_OLD = 7,
	USE_FEATURE_TAR_CREATE(  OPTBIT_CREATE      ,)
	USE_FEATURE_TAR_CREATE(  OPTBIT_DEREFERENCE ,)
	USE_FEATURE_TAR_BZIP2(   OPTBIT_BZIP2       ,)
	USE_FEATURE_TAR_LZMA(    OPTBIT_LZMA        ,)
	USE_FEATURE_TAR_FROM(    OPTBIT_INCLUDE_FROM,)
	USE_FEATURE_TAR_FROM(    OPTBIT_EXCLUDE_FROM,)
	USE_FEATURE_TAR_GZIP(    OPTBIT_GZIP        ,)
	USE_FEATURE_TAR_COMPRESS(OPTBIT_COMPRESS    ,)
	OPTBIT_NOPRESERVE_OWN,
	OPTBIT_NOPRESERVE_PERM,
	OPT_TEST         = 1 << 0, // t
	OPT_EXTRACT      = 1 << 1, // x
	OPT_BASEDIR      = 1 << 2, // C
	OPT_TARNAME      = 1 << 3, // f
	OPT_2STDOUT      = 1 << 4, // O
	OPT_P            = 1 << 5, // p
	OPT_VERBOSE      = 1 << 6, // v
	OPT_KEEP_OLD     = 1 << 7, // k
	OPT_CREATE       = USE_FEATURE_TAR_CREATE(  (1<<OPTBIT_CREATE      )) + 0, // c
	OPT_DEREFERENCE  = USE_FEATURE_TAR_CREATE(  (1<<OPTBIT_DEREFERENCE )) + 0, // h
	OPT_BZIP2        = USE_FEATURE_TAR_BZIP2(   (1<<OPTBIT_BZIP2       )) + 0, // j
	OPT_LZMA         = USE_FEATURE_TAR_LZMA(    (1<<OPTBIT_LZMA        )) + 0, // a
	OPT_INCLUDE_FROM = USE_FEATURE_TAR_FROM(    (1<<OPTBIT_INCLUDE_FROM)) + 0, // T
	OPT_EXCLUDE_FROM = USE_FEATURE_TAR_FROM(    (1<<OPTBIT_EXCLUDE_FROM)) + 0, // X
	OPT_GZIP         = USE_FEATURE_TAR_GZIP(    (1<<OPTBIT_GZIP        )) + 0, // z
	OPT_COMPRESS     = USE_FEATURE_TAR_COMPRESS((1<<OPTBIT_COMPRESS    )) + 0, // Z
	OPT_NOPRESERVE_OWN  = 1 << OPTBIT_NOPRESERVE_OWN , // no-same-owner
	OPT_NOPRESERVE_PERM = 1 << OPTBIT_NOPRESERVE_PERM, // no-same-permissions
};
#if ENABLE_FEATURE_TAR_LONG_OPTIONS
static const struct option tar_long_options[] = {
	{ "list",               0,  NULL,   't' },
	{ "extract",            0,  NULL,   'x' },
	{ "directory",          1,  NULL,   'C' },
	{ "file",               1,  NULL,   'f' },
	{ "to-stdout",          0,  NULL,   'O' },
	{ "same-permissions",   0,  NULL,   'p' },
	{ "verbose",            0,  NULL,   'v' },
	{ "keep-old",           0,  NULL,   'k' },
# if ENABLE_FEATURE_TAR_CREATE
	{ "create",             0,  NULL,   'c' },
	{ "dereference",        0,  NULL,   'h' },
# endif
# if ENABLE_FEATURE_TAR_BZIP2
	{ "bzip2",              0,  NULL,   'j' },
# endif
# if ENABLE_FEATURE_TAR_LZMA
	{ "lzma",               0,  NULL,   'a' },
# endif
# if ENABLE_FEATURE_TAR_FROM
	{ "files-from",         1,  NULL,   'T' },
	{ "exclude-from",       1,  NULL,   'X' },
# endif
# if ENABLE_FEATURE_TAR_GZIP
	{ "gzip",               0,  NULL,   'z' },
# endif
# if ENABLE_FEATURE_TAR_COMPRESS
	{ "compress",           0,  NULL,   'Z' },
# endif
	{ "no-same-owner",      0,  NULL,   0xfd },
	{ "no-same-permissions",0,  NULL,   0xfe },
	/* --exclude takes next bit position in option mask, */
	/* therefore we have to either put it _after_ --no-same-perm */
	/* or add OPT[BIT]_EXCLUDE before OPT[BIT]_NOPRESERVE_OWN */
# if ENABLE_FEATURE_TAR_FROM
	{ "exclude",            1,  NULL,   0xff },
# endif
	{ 0,                    0, 0, 0 }
};
#endif

int tar_main(int argc, char **argv)
{
	char (*get_header_ptr)(archive_handle_t *) = get_header_tar;
	archive_handle_t *tar_handle;
	char *base_dir = NULL;
	const char *tar_filename = "-";
	unsigned opt;
	int verboseFlag = 0;
#if ENABLE_FEATURE_TAR_LONG_OPTIONS && ENABLE_FEATURE_TAR_FROM
	llist_t *excludes = NULL;
#endif

	/* Initialise default values */
	tar_handle = init_handle();
	tar_handle->flags = ARCHIVE_CREATE_LEADING_DIRS
	                  | ARCHIVE_PRESERVE_DATE
	                  | ARCHIVE_EXTRACT_UNCONDITIONAL;

	/* Prepend '-' to the first argument if required */
	opt_complementary = "--:" // first arg is options
		"tt:vv:" // count -t,-v
		"?:" // bail out with usage instead of error return
		"X::T::" // cumulative lists
#if ENABLE_FEATURE_TAR_LONG_OPTIONS && ENABLE_FEATURE_TAR_FROM
		"\xff::" // cumulative lists for --exclude
#endif
		USE_FEATURE_TAR_CREATE("c:") "t:x:" // at least one of these is reqd
		USE_FEATURE_TAR_CREATE("c--tx:t--cx:x--ct") // mutually exclusive
		SKIP_FEATURE_TAR_CREATE("t--x:x--t"); // mutually exclusive
#if ENABLE_FEATURE_TAR_LONG_OPTIONS
	applet_long_options = tar_long_options;
#endif
	opt = getopt32(argc, argv,
		"txC:f:Opvk"
		USE_FEATURE_TAR_CREATE(  "ch"  )
		USE_FEATURE_TAR_BZIP2(   "j"   )
		USE_FEATURE_TAR_LZMA(    "a"   )
		USE_FEATURE_TAR_FROM(    "T:X:")
		USE_FEATURE_TAR_GZIP(    "z"   )
		USE_FEATURE_TAR_COMPRESS("Z"   )
		, &base_dir // -C dir
		, &tar_filename // -f filename
		USE_FEATURE_TAR_FROM(, &(tar_handle->accept)) // T
		USE_FEATURE_TAR_FROM(, &(tar_handle->reject)) // X
#if ENABLE_FEATURE_TAR_LONG_OPTIONS && ENABLE_FEATURE_TAR_FROM
		, &excludes // --exclude
#endif
		, &verboseFlag // combined count for -t and -v
		, &verboseFlag // combined count for -t and -v
		);

	if (verboseFlag) tar_handle->action_header = header_verbose_list;
	if (verboseFlag == 1) tar_handle->action_header = header_list;

	if (opt & OPT_EXTRACT)
		tar_handle->action_data = data_extract_all;

	if (opt & OPT_2STDOUT)
		tar_handle->action_data = data_extract_to_stdout;

	if (opt & OPT_KEEP_OLD)
		tar_handle->flags &= ~ARCHIVE_EXTRACT_UNCONDITIONAL;

	if (opt & OPT_NOPRESERVE_OWN)
		tar_handle->flags |= ARCHIVE_NOPRESERVE_OWN;

	if (opt & OPT_NOPRESERVE_PERM)
		tar_handle->flags |= ARCHIVE_NOPRESERVE_PERM;

	if (opt & OPT_GZIP)
		get_header_ptr = get_header_tar_gz;

	if (opt & OPT_BZIP2)
		get_header_ptr = get_header_tar_bz2;

	if (opt & OPT_LZMA)
		get_header_ptr = get_header_tar_lzma;

	if (opt & OPT_COMPRESS)
		get_header_ptr = get_header_tar_Z;

#if ENABLE_FEATURE_TAR_FROM
	tar_handle->reject = append_file_list_to_list(tar_handle->reject);
#if ENABLE_FEATURE_TAR_LONG_OPTIONS
	/* Append excludes to reject */
	while (excludes) {
		llist_t *next = excludes->link;
		excludes->link = tar_handle->reject;
		tar_handle->reject = excludes;
		excludes = next;
	}
#endif
	tar_handle->accept = append_file_list_to_list(tar_handle->accept);
#endif

	/* Check if we are reading from stdin */
	if (argv[optind] && *argv[optind] == '-') {
		/* Default is to read from stdin, so just skip to next arg */
		optind++;
	}

	/* Setup an array of filenames to work with */
	/* TODO: This is the same as in ar, separate function ? */
	while (optind < argc) {
		/* kill trailing '/' unless the string is just "/" */
		char *cp = last_char_is(argv[optind], '/');
		if (cp > argv[optind])
			*cp = '\0';
		llist_add_to(&tar_handle->accept, argv[optind]);
		optind++;
	}
	tar_handle->accept = rev_llist(tar_handle->accept);

	if (tar_handle->accept || tar_handle->reject)
		tar_handle->filter = filter_accept_reject_list;

	/* Open the tar file */
	{
		FILE *tar_stream;
		int flags;

		if (opt & OPT_CREATE) {
			/* Make sure there is at least one file to tar up.  */
			if (tar_handle->accept == NULL)
				bb_error_msg_and_die("empty archive");

			tar_stream = stdout;
			/* Mimicking GNU tar 1.15.1: */
			flags = O_WRONLY|O_CREAT|O_TRUNC;
		/* was doing unlink; open(O_WRONLY|O_CREAT|O_EXCL); why? */
		} else {
			tar_stream = stdin;
			flags = O_RDONLY;
		}

		if (LONE_DASH(tar_filename)) {
			tar_handle->src_fd = fileno(tar_stream);
			tar_handle->seek = seek_by_read;
		} else {
			tar_handle->src_fd = xopen(tar_filename, flags);
		}
	}

	if (base_dir)
		xchdir(base_dir);

#ifdef CHECK_FOR_CHILD_EXITCODE
	/* We need to know whether child (gzip/bzip/etc) exits abnormally */
	signal(SIGCHLD, handle_SIGCHLD);
#endif

	/* create an archive */
	if (opt & OPT_CREATE) {
		int zipMode = 0;
		if (ENABLE_FEATURE_TAR_GZIP && get_header_ptr == get_header_tar_gz)
			zipMode = 1;
		if (ENABLE_FEATURE_TAR_BZIP2 && get_header_ptr == get_header_tar_bz2)
			zipMode = 2;
		/* NB: writeTarFile() closes tar_handle->src_fd */
		return writeTarFile(tar_handle->src_fd, verboseFlag, opt & OPT_DEREFERENCE,
				tar_handle->accept,
				tar_handle->reject, zipMode);
	}

	while (get_header_ptr(tar_handle) == EXIT_SUCCESS)
		/* nothing */;

	/* Check that every file that should have been extracted was */
	while (tar_handle->accept) {
		if (!find_list_entry(tar_handle->reject, tar_handle->accept->data)
		 && !find_list_entry(tar_handle->passed, tar_handle->accept->data)
		) {
			bb_error_msg_and_die("%s: not found in archive",
				tar_handle->accept->data);
		}
		tar_handle->accept = tar_handle->accept->link;
	}
	if (ENABLE_FEATURE_CLEAN_UP /* && tar_handle->src_fd != STDIN_FILENO */)
		close(tar_handle->src_fd);

	return EXIT_SUCCESS;
}
