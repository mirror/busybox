#ifndef	__UNARCHIVE_H__
#define	__UNARCHIVE_H__

#define ARCHIVE_PRESERVE_DATE	1
#define ARCHIVE_CREATE_LEADING_DIRS		2
#define ARCHIVE_EXTRACT_UNCONDITIONAL	4
#define ARCHIVE_EXTRACT_QUIET	8

#include <sys/types.h>
#include <stdio.h>

typedef struct gunzip_s {
	unsigned short buffer_count;
	unsigned char *buffer;
	unsigned int crc;
	unsigned int count;
} gunzip_t;

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

typedef struct llist_s {
	const char *data;
	const struct llist_s *link;
} llist_t;

typedef struct archive_handle_s {
	/* define if the header and data compenent should processed */
	char (*filter)(const llist_t *, const llist_t *, const char *);
	const llist_t *accept;
	const llist_t *reject;
	const llist_t *passed;	/* List of files that have successfully been worked on */

	/* Contains the processed header entry */
	file_header_t *file_header;

	/* process the header component, e.g. tar -t */
	void (*action_header)(const file_header_t *);

	/* process the data componenet, e.g. extract to filesystem */
	void (*action_data)(struct archive_handle_s *);
	char (*action_data_subarchive)(struct archive_handle_s *);

	/* Contains the handle to a sub archive */
	struct archive_handle_s *sub_archive;

	/* The raw stream as read from disk or stdin */
	int src_fd;

	/* Count the number of bytes processed */
	off_t offset;

	/* Temperary storage */
	char *buffer;

	/* Misc. stuff */
	unsigned char flags;

} archive_handle_t;

extern archive_handle_t *init_handle(void);

extern char filter_accept_all(const llist_t *accept_list, const llist_t *reject_list, const char *key);
extern char filter_accept_list(const llist_t *accept_list, const llist_t *reject_list, const char *key);
extern char filter_accept_reject_list(const llist_t *accept_list, const llist_t *reject_list, const char *key);

extern void unpack_ar_archive(archive_handle_t *ar_archive);

extern void data_skip(archive_handle_t *archive_handle);
extern void data_extract_all(archive_handle_t *archive_handle);
extern void data_extract_to_stdout(archive_handle_t *archive_handle);
extern void data_extract_to_buffer(archive_handle_t *archive_handle);

extern void header_skip(const file_header_t *file_header);
extern void header_list(const file_header_t *file_header);
extern void header_verbose_list(const file_header_t *file_header);

extern void check_header_gzip(int src_fd);
extern void check_trailer_gzip(int src_fd);

extern char get_header_ar(archive_handle_t *archive_handle);
extern char get_header_tar(archive_handle_t *archive_handle);
extern char get_header_tar_gz(archive_handle_t *archive_handle);

extern unsigned char uncompressStream(FILE *zStream, FILE *stream);

extern void seek_sub_file(int src_fd, unsigned int amount);
extern const unsigned short data_align(const int src_fd, const unsigned int offset, const unsigned short align_to);
extern const llist_t *add_to_list(const llist_t *old_head, const char *new_item);
extern int copy_file_chunk_fd(int src_fd, int dst_fd, unsigned long long chunksize);
extern const llist_t *find_list_entry(const llist_t *list, const char *filename);
#endif
