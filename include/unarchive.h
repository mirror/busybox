#ifndef	__UNARCHIVE_H__
#define	__UNARCHIVE_H__

#define ARCHIVE_PRESERVE_DATE	1
#define ARCHIVE_CREATE_LEADING_DIRS		2
#define ARCHIVE_EXTRACT_UNCONDITIONAL	4
#define ARCHIVE_EXTRACT_QUIET	8
#define ARCHIVE_EXTRACT_NEWER	16

#include <sys/types.h>
#include <stdio.h>
#include "libbb.h"

typedef struct file_headers_s {
	char *name;
	char *link_name;
	off_t size;
	uid_t uid;
	gid_t gid;
	mode_t mode;
	time_t mtime;
	dev_t device;
} file_header_t;

typedef struct archive_handle_s {
	/* define if the header and data compenent should processed */
	char (*filter)(struct archive_handle_s *);
	llist_t *accept;
	llist_t *reject;
	llist_t *passed;	/* List of files that have successfully been worked on */

	/* Contains the processed header entry */
	file_header_t *file_header;

	/* process the header component, e.g. tar -t */
	void (*action_header)(const file_header_t *);

	/* process the data component, e.g. extract to filesystem */
	void (*action_data)(struct archive_handle_s *);
	
	/* How to process any sub archive, e.g. get_header_tar_gz */
	char (*action_data_subarchive)(struct archive_handle_s *);

	/* Contains the handle to a sub archive */
	struct archive_handle_s *sub_archive;

	/* The raw stream as read from disk or stdin */
	int src_fd;

	/* Count the number of bytes processed */
	off_t offset;

	/* Function that skips data: read_by_char or read_by_skip */
	void (*seek)(const struct archive_handle_s *archive_handle, const unsigned int amount);

	/* Temporary storage */
	char *buffer;

	/* Misc. stuff */
	unsigned char flags;

} archive_handle_t;

extern archive_handle_t *init_handle(void);

extern char filter_accept_all(archive_handle_t *archive_handle);
extern char filter_accept_list(archive_handle_t *archive_handle);
extern char filter_accept_list_reassign(archive_handle_t *archive_handle);
extern char filter_accept_reject_list(archive_handle_t *archive_handle);

extern void unpack_ar_archive(archive_handle_t *ar_archive);

extern void data_skip(archive_handle_t *archive_handle);
extern void data_extract_all(archive_handle_t *archive_handle);
extern void data_extract_to_stdout(archive_handle_t *archive_handle);
extern void data_extract_to_buffer(archive_handle_t *archive_handle);

extern void header_skip(const file_header_t *file_header);
extern void header_list(const file_header_t *file_header);
extern void header_verbose_list(const file_header_t *file_header);

extern void check_header_gzip(int src_fd);

extern char get_header_ar(archive_handle_t *archive_handle);
extern char get_header_cpio(archive_handle_t *archive_handle);
extern char get_header_tar(archive_handle_t *archive_handle);
extern char get_header_tar_bz2(archive_handle_t *archive_handle);
extern char get_header_tar_gz(archive_handle_t *archive_handle);

extern void seek_by_jump(const archive_handle_t *archive_handle, const unsigned int amount);
extern void seek_by_char(const archive_handle_t *archive_handle, const unsigned int amount);

extern void archive_xread_all(const archive_handle_t *archive_handle, void *buf, const size_t count);
extern ssize_t archive_xread_all_eof(archive_handle_t *archive_handle, unsigned char *buf, size_t count);

extern void data_align(archive_handle_t *archive_handle, const unsigned short boundary);
extern const llist_t *find_list_entry(const llist_t *list, const char *filename);

extern int uncompressStream(int src_fd, int dst_fd);
extern void inflate_init(unsigned int bufsize);
extern int inflate_unzip(int in, int out);
extern int inflate_gunzip(int in, int out);

extern int open_transformer(int src_fd, int (*transformer)(int src_fd, int dst_fd));


#endif
