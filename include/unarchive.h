/* vi: set sw=4 ts=4: */
#ifndef	__UNARCHIVE_H__
#define	__UNARCHIVE_H__

#if __GNUC_PREREQ(4,1)
# pragma GCC visibility push(hidden)
#endif

#define ARCHIVE_PRESERVE_DATE           1
#define ARCHIVE_CREATE_LEADING_DIRS     2
#define ARCHIVE_EXTRACT_UNCONDITIONAL   4
#define ARCHIVE_EXTRACT_QUIET           8
#define ARCHIVE_EXTRACT_NEWER           16
#define ARCHIVE_NOPRESERVE_OWN          32
#define ARCHIVE_NOPRESERVE_PERM         64

typedef struct file_header_t {
	char *name;
	char *link_target;
#if ENABLE_FEATURE_TAR_UNAME_GNAME
	char *uname;
	char *gname;
#endif
	off_t size;
	uid_t uid;
	gid_t gid;
	mode_t mode;
	time_t mtime;
	dev_t device;
} file_header_t;

typedef struct archive_handle_t {
	/* define if the header and data component should be processed */
	char (*filter)(struct archive_handle_t *);
	llist_t *accept;
	/* List of files that have been rejected */
	llist_t *reject;
	/* List of files that have successfully been worked on */
	llist_t *passed;

	/* Contains the processed header entry */
	file_header_t *file_header;

	/* process the header component, e.g. tar -t */
	void (*action_header)(const file_header_t *);

	/* process the data component, e.g. extract to filesystem */
	void (*action_data)(struct archive_handle_t *);

	/* How to process any sub archive, e.g. get_header_tar_gz */
	char (*action_data_subarchive)(struct archive_handle_t *);

	/* Contains the handle to a sub archive */
	struct archive_handle_t *sub_archive;

	/* The raw stream as read from disk or stdin */
	int src_fd;

	/* Count the number of bytes processed */
	off_t offset;

	/* Function that skips data: read_by_char or read_by_skip */
	void (*seek)(const struct archive_handle_t *archive_handle, const unsigned amount);

	/* Temporary storage */
	char *buffer;

	/* Flags and misc. stuff */
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

extern char get_header_ar(archive_handle_t *archive_handle);
extern char get_header_cpio(archive_handle_t *archive_handle);
extern char get_header_tar(archive_handle_t *archive_handle);
extern char get_header_tar_bz2(archive_handle_t *archive_handle);
extern char get_header_tar_lzma(archive_handle_t *archive_handle);
extern char get_header_tar_gz(archive_handle_t *archive_handle);

extern void seek_by_jump(const archive_handle_t *archive_handle, unsigned amount);
extern void seek_by_read(const archive_handle_t *archive_handle, unsigned amount);

extern ssize_t archive_xread_all_eof(archive_handle_t *archive_handle, unsigned char *buf, size_t count);

extern void data_align(archive_handle_t *archive_handle, unsigned boundary);
extern const llist_t *find_list_entry(const llist_t *list, const char *filename);
extern const llist_t *find_list_entry2(const llist_t *list, const char *filename);

/* A bit of bunzip2 internals are exposed for compressed help support: */
typedef struct bunzip_data bunzip_data;
int start_bunzip(bunzip_data **bdp, int in_fd, const unsigned char *inbuf, int len);
int read_bunzip(bunzip_data *bd, char *outbuf, int len);
void dealloc_bunzip(bunzip_data *bd);

typedef struct inflate_unzip_result {
	off_t bytes_out;
	uint32_t crc;
} inflate_unzip_result;

extern USE_DESKTOP(long long) int unpack_bz2_stream(int src_fd, int dst_fd);
extern USE_DESKTOP(long long) int inflate_unzip(inflate_unzip_result *res, off_t compr_size, int src_fd, int dst_fd);
extern USE_DESKTOP(long long) int unpack_gz_stream(int src_fd, int dst_fd);
extern USE_DESKTOP(long long) int unpack_lzma_stream(int src_fd, int dst_fd);

#if BB_MMU
extern int open_transformer(int src_fd,
	USE_DESKTOP(long long) int (*transformer)(int src_fd, int dst_fd));
#define open_transformer(src_fd, transformer, transform_prog) open_transformer(src_fd, transformer)
#else
extern int open_transformer(int src_fd, const char *transform_prog);
#define open_transformer(src_fd, transformer, transform_prog) open_transformer(src_fd, transform_prog)
#endif

#if __GNUC_PREREQ(4,1)
# pragma GCC visibility pop
#endif

#endif
