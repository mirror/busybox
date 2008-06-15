/* vi: set sw=4 ts=4: */
/*
 * Mini tar implementation for busybox
 *
 * Modified to use common extraction code used by ar, cpio, dpkg-deb, dpkg
 *  by Glenn McGrath
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
#include "libbb.h"
#include "unarchive.h"

/* FIXME: Stop using this non-standard feature */
#ifndef FNM_LEADING_DIR
#define FNM_LEADING_DIR 0
#endif


#define block_buf bb_common_bufsiz1


#if !ENABLE_FEATURE_TAR_GZIP && !ENABLE_FEATURE_TAR_BZIP2
/* Do not pass gzip flag to writeTarFile() */
#define writeTarFile(tar_fd, verboseFlag, dereferenceFlag, include, exclude, gzip) \
	writeTarFile(tar_fd, verboseFlag, dereferenceFlag, include, exclude)
#endif


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
	/* POSIX:   "ustar" NUL "00" */
	/* GNU tar: "ustar  " NUL */
	/* Normally it's defined as magic[6] followed by
	 * version[2], but we put them together to save code.
	 */
	char magic[8];            /* 257-264 */
	char uname[32];           /* 265-296 */
	char gname[32];           /* 297-328 */
	char devmajor[8];         /* 329-336 */
	char devminor[8];         /* 337-344 */
	char prefix[155];         /* 345-499 */
	char padding[12];         /* 500-512 (pad to exactly TAR_BLOCK_SIZE) */
};

/*
** writeTarFile(), writeFileToTarball(), and writeTarHeader() are
** the only functions that deal with the HardLinkInfo structure.
** Even these functions use the xxxHardLinkInfo() functions.
*/
typedef struct HardLinkInfo HardLinkInfo;
struct HardLinkInfo {
	HardLinkInfo *next;     /* Next entry in list */
	dev_t dev;              /* Device number */
	ino_t ino;              /* Inode number */
	short linkCount;        /* (Hard) Link Count */
	char name[1];           /* Start of filename (must be last) */
};

/* Some info to be carried along when creating a new tarball */
typedef struct TarBallInfo TarBallInfo;
struct TarBallInfo {
	int tarFd;                      /* Open-for-write file descriptor
	                                 * for the tarball */
	struct stat statBuf;            /* Stat info for the tarball, letting
	                                 * us know the inode and device that the
	                                 * tarball lives, so we can avoid trying
	                                 * to include the tarball into itself */
	int verboseFlag;                /* Whether to print extra stuff or not */
	const llist_t *excludeList;     /* List of files to not include */
	HardLinkInfo *hlInfoHead;       /* Hard Link Tracking Information */
	HardLinkInfo *hlInfo;           /* Hard Link Info for the current file */
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
static void addHardLinkInfo(HardLinkInfo **hlInfoHeadPtr,
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

static void freeHardLinkInfo(HardLinkInfo **hlInfoHeadPtr)
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
}

/* Might be faster (and bigger) if the dev/ino were stored in numeric order;) */
static HardLinkInfo *findHardLinkInfo(HardLinkInfo *hlInfo, struct stat *statbuf)
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
	/* POSIX says that checksum is done on unsigned bytes
	 * (Sun and HP-UX gets it wrong... more details in
	 * GNU tar source) */
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
		char *lpath = xmalloc_readlink_or_warn(fileName);
		if (!lpath)
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

	/* Strip leading '/' (must be before memorizing hardlink's name) */
	header_name = fileName;
	while (header_name[0] == '/') {
		static smallint warned;

		if (!warned) {
			bb_error_msg("removing leading '/' from member names");
			warned = 1;
		}
		header_name++;
	}

	if (header_name[0] == '\0')
		return TRUE;

	/* It is against the rules to archive a socket */
	if (S_ISSOCK(statbuf->st_mode)) {
		bb_error_msg("%s: socket ignored", fileName);
		return TRUE;
	}

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
			addHardLinkInfo(&tbInfo->hlInfoHead, statbuf, header_name);
	}

	/* It is a bad idea to store the archive we are in the process of creating,
	 * so check the device and inode to be sure that this particular file isn't
	 * the new tarball */
	if (tbInfo->statBuf.st_dev == statbuf->st_dev
	 && tbInfo->statBuf.st_ino == statbuf->st_ino
	) {
		bb_error_msg("%s: file is the archive; skipping", fileName);
		return TRUE;
	}

	if (exclude_file(tbInfo->excludeList, header_name))
		return SKIP;

#if !ENABLE_FEATURE_TAR_GNU_EXTENSIONS
	if (strlen(header_name) >= NAME_SIZE) {
		bb_error_msg("names longer than "NAME_SIZE_STR" chars not supported");
		return TRUE;
	}
#endif

	/* Is this a regular file? */
	if (tbInfo->hlInfo == NULL && S_ISREG(statbuf->st_mode)) {
		/* open the file we want to archive, and make sure all is well */
		inputFileFd = open_or_warn(fileName, O_RDONLY);
		if (inputFileFd < 0) {
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
		/* Write the file to the archive. */
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
		memset(block_buf, 0, readSize);
		xwrite(tbInfo->tarFd, block_buf, readSize);
	}

	return TRUE;
}

#if ENABLE_FEATURE_TAR_GZIP || ENABLE_FEATURE_TAR_BZIP2
#if !(ENABLE_FEATURE_TAR_GZIP && ENABLE_FEATURE_TAR_BZIP2)
#define vfork_compressor(tar_fd, gzip) vfork_compressor(tar_fd)
#endif
/* Don't inline: vfork scares gcc and pessimizes code */
static void NOINLINE vfork_compressor(int tar_fd, int gzip)
{
	pid_t gzipPid;
#if ENABLE_FEATURE_TAR_GZIP && ENABLE_FEATURE_TAR_BZIP2
	const char *zip_exec = (gzip == 1) ? "gzip" : "bzip2";
#elif ENABLE_FEATURE_TAR_GZIP
	const char *zip_exec = "gzip";
#else /* only ENABLE_FEATURE_TAR_BZIP2 */
	const char *zip_exec = "bzip2";
#endif
	// On Linux, vfork never unpauses parent early, although standard
	// allows for that. Do we want to waste bytes checking for it?
#define WAIT_FOR_CHILD 0
	volatile int vfork_exec_errno = 0;
	struct fd_pair gzipDataPipe;
#if WAIT_FOR_CHILD
	struct fd_pair gzipStatusPipe;
	xpiped_pair(gzipStatusPipe);
#endif
	xpiped_pair(gzipDataPipe);

	signal(SIGPIPE, SIG_IGN); /* we only want EPIPE on errors */

#if defined(__GNUC__) && __GNUC__
	/* Avoid vfork clobbering */
	(void) &zip_exec;
#endif

	gzipPid = vfork();
	if (gzipPid < 0)
		bb_perror_msg_and_die("can't vfork");

	if (gzipPid == 0) {
		/* child */
		/* NB: close _first_, then move fds! */
		close(gzipDataPipe.wr);
#if WAIT_FOR_CHILD
		close(gzipStatusPipe.rd);
		/* gzipStatusPipe.wr will close only on exec -
		 * parent waits for this close to happen */
		fcntl(gzipStatusPipe.wr, F_SETFD, FD_CLOEXEC);
#endif
		xmove_fd(gzipDataPipe.rd, 0);
		xmove_fd(tar_fd, 1);
		/* exec gzip/bzip2 program/applet */
		BB_EXECLP(zip_exec, zip_exec, "-f", NULL);
		vfork_exec_errno = errno;
		_exit(EXIT_FAILURE);
	}

	/* parent */
	xmove_fd(gzipDataPipe.wr, tar_fd);
	close(gzipDataPipe.rd);
#if WAIT_FOR_CHILD
	close(gzipStatusPipe.wr);
	while (1) {
		char buf;
		int n;

		/* Wait until child execs (or fails to) */
		n = full_read(gzipStatusPipe.rd, &buf, 1);
		if (n < 0 /* && errno == EAGAIN */)
			continue;	/* try it again */
	}
	close(gzipStatusPipe.rd);
#endif
	if (vfork_exec_errno) {
		errno = vfork_exec_errno;
		bb_perror_msg_and_die("cannot exec %s", zip_exec);
	}
}
#endif /* ENABLE_FEATURE_TAR_GZIP || ENABLE_FEATURE_TAR_BZIP2 */


/* gcc 4.2.1 inlines it, making code bigger */
static NOINLINE int writeTarFile(int tar_fd, int verboseFlag,
	int dereferenceFlag, const llist_t *include,
	const llist_t *exclude, int gzip)
{
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

#if ENABLE_FEATURE_TAR_GZIP || ENABLE_FEATURE_TAR_BZIP2
	if (gzip)
		vfork_compressor(tbInfo.tarFd, gzip);
#endif

	tbInfo.excludeList = exclude;

	/* Read the directory/files and iterate over them one at a time */
	while (include) {
		if (!recursive_action(include->data, ACTION_RECURSE |
				(dereferenceFlag ? ACTION_FOLLOWLINKS : 0),
				writeFileToTarball, writeFileToTarball, &tbInfo, 0))
		{
			errorFlag = TRUE;
		}
		include = include->link;
	}
	/* Write two empty blocks to the end of the archive */
	memset(block_buf, 0, 2*TAR_BLOCK_SIZE);
	xwrite(tbInfo.tarFd, block_buf, 2*TAR_BLOCK_SIZE);

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

#if ENABLE_FEATURE_TAR_GZIP || ENABLE_FEATURE_TAR_BZIP2
	if (gzip) {
		int status;
		if (safe_waitpid(-1, &status, 0) == -1)
			bb_perror_msg("waitpid");
		else if (!WIFEXITED(status) || WEXITSTATUS(status))
			/* gzip was killed or has exited with nonzero! */
			errorFlag = TRUE;
	}
#endif
	return errorFlag;
}
#else
int writeTarFile(int tar_fd, int verboseFlag,
	int dereferenceFlag, const llist_t *include,
	const llist_t *exclude, int gzip);
#endif /* FEATURE_TAR_CREATE */

#if ENABLE_FEATURE_TAR_FROM
static llist_t *append_file_list_to_list(llist_t *list)
{
	FILE *src_stream;
	char *line;
	llist_t *newlist = NULL;

	while (list) {
		src_stream = xfopen(llist_pop(&list), "r");
		while ((line = xmalloc_fgetline(src_stream)) != NULL) {
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

	archive_handle->src_fd = open_transformer(archive_handle->src_fd, uncompress, "uncompress");
	archive_handle->offset = 0;
	while (get_header_tar(archive_handle) == EXIT_SUCCESS)
		continue;

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
	if (wait_any_nohang(&status) < 0)
		/* wait failed?! I'm confused... */
		return;

	if (WIFEXITED(status) && WEXITSTATUS(status)==0)
		/* child exited with 0 */
		return;
	/* Cannot happen?
	if (!WIFSIGNALED(status) && !WIFEXITED(status)) return; */
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
static const char tar_longopts[] ALIGN1 =
	"list\0"                No_argument       "t"
	"extract\0"             No_argument       "x"
	"directory\0"           Required_argument "C"
	"file\0"                Required_argument "f"
	"to-stdout\0"           No_argument       "O"
	"same-permissions\0"    No_argument       "p"
	"verbose\0"             No_argument       "v"
	"keep-old\0"            No_argument       "k"
# if ENABLE_FEATURE_TAR_CREATE
	"create\0"              No_argument       "c"
	"dereference\0"         No_argument       "h"
# endif
# if ENABLE_FEATURE_TAR_BZIP2
	"bzip2\0"               No_argument       "j"
# endif
# if ENABLE_FEATURE_TAR_LZMA
	"lzma\0"                No_argument       "a"
# endif
# if ENABLE_FEATURE_TAR_FROM
	"files-from\0"          Required_argument "T"
	"exclude-from\0"        Required_argument "X"
# endif
# if ENABLE_FEATURE_TAR_GZIP
	"gzip\0"                No_argument       "z"
# endif
# if ENABLE_FEATURE_TAR_COMPRESS
	"compress\0"            No_argument       "Z"
# endif
	"no-same-owner\0"       No_argument       "\xfd"
	"no-same-permissions\0" No_argument       "\xfe"
	/* --exclude takes next bit position in option mask, */
	/* therefore we have to either put it _after_ --no-same-perm */
	/* or add OPT[BIT]_EXCLUDE before OPT[BIT]_NOPRESERVE_OWN */
# if ENABLE_FEATURE_TAR_FROM
	"exclude\0"             Required_argument "\xff"
# endif
	;
#endif

int tar_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int tar_main(int argc ATTRIBUTE_UNUSED, char **argv)
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
	applet_long_options = tar_longopts;
#endif
	opt = getopt32(argv,
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
	argv += optind;

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

	/* Setup an array of filenames to work with */
	/* TODO: This is the same as in ar, separate function ? */
	while (*argv) {
		/* kill trailing '/' unless the string is just "/" */
		char *cp = last_char_is(*argv, '/');
		if (cp > *argv)
			*cp = '\0';
		llist_add_to_end(&tar_handle->accept, *argv);
		argv++;
	}

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
#if ENABLE_FEATURE_TAR_GZIP || ENABLE_FEATURE_TAR_BZIP2
		int zipMode = 0;
		if (ENABLE_FEATURE_TAR_GZIP && (opt & OPT_GZIP))
			zipMode = 1;
		if (ENABLE_FEATURE_TAR_BZIP2 && (opt & OPT_BZIP2))
			zipMode = 2;
#endif
		/* NB: writeTarFile() closes tar_handle->src_fd */
		return writeTarFile(tar_handle->src_fd, verboseFlag, opt & OPT_DEREFERENCE,
				tar_handle->accept,
				tar_handle->reject, zipMode);
	}

	while (get_header_ptr(tar_handle) == EXIT_SUCCESS)
		continue;

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
