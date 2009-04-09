/* vi: set sw=4 ts=4: */
#ifndef UNARCHIVE_H
#define UNARCHIVE_H 1

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

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
	/* Define if the header and data component should be processed */
	char FAST_FUNC (*filter)(struct archive_handle_t *);
	llist_t *accept;
	/* List of files that have been rejected */
	llist_t *reject;
	/* List of files that have successfully been worked on */
	llist_t *passed;

	/* Contains the processed header entry */
	file_header_t *file_header;

	/* Process the header component, e.g. tar -t */
	void FAST_FUNC (*action_header)(const file_header_t *);

	/* Process the data component, e.g. extract to filesystem */
	void FAST_FUNC (*action_data)(struct archive_handle_t *);

#if ENABLE_DPKG || ENABLE_DPKG_DEB
	/* "subarchive" is used only by dpkg[-deb] applets */
	/* How to process any sub archive, e.g. get_header_tar_gz */
	char FAST_FUNC (*action_data_subarchive)(struct archive_handle_t *);
	/* Contains the handle to a sub archive */
	struct archive_handle_t *sub_archive;
#endif

	/* The raw stream as read from disk or stdin */
	int src_fd;

	/* Count the number of bytes processed */
	off_t offset;

	/* Function that skips data: read_by_char or read_by_skip */
	void FAST_FUNC (*seek)(const struct archive_handle_t *archive_handle, const unsigned amount);

	/* Temporary storage */
	char *buffer;

	/* Flags and misc. stuff */
	unsigned char ah_flags;

	/* "Private" storage for archivers */
//	unsigned char ah_priv_inited;
	void *ah_priv[8];

} archive_handle_t;


/* Info struct unpackers can fill out to inform users of thing like
 * timestamps of unpacked files */
typedef struct unpack_info_t {
	time_t mtime;
} unpack_info_t;

extern archive_handle_t *init_handle(void) FAST_FUNC;

extern char filter_accept_all(archive_handle_t *archive_handle) FAST_FUNC;
extern char filter_accept_list(archive_handle_t *archive_handle) FAST_FUNC;
extern char filter_accept_list_reassign(archive_handle_t *archive_handle) FAST_FUNC;
extern char filter_accept_reject_list(archive_handle_t *archive_handle) FAST_FUNC;

extern void unpack_ar_archive(archive_handle_t *ar_archive) FAST_FUNC;

extern void data_skip(archive_handle_t *archive_handle) FAST_FUNC;
extern void data_extract_all(archive_handle_t *archive_handle) FAST_FUNC;
extern void data_extract_to_stdout(archive_handle_t *archive_handle) FAST_FUNC;
extern void data_extract_to_buffer(archive_handle_t *archive_handle) FAST_FUNC;

extern void header_skip(const file_header_t *file_header) FAST_FUNC;
extern void header_list(const file_header_t *file_header) FAST_FUNC;
extern void header_verbose_list(const file_header_t *file_header) FAST_FUNC;

extern char get_header_ar(archive_handle_t *archive_handle) FAST_FUNC;
extern char get_header_cpio(archive_handle_t *archive_handle) FAST_FUNC;
extern char get_header_tar(archive_handle_t *archive_handle) FAST_FUNC;
extern char get_header_tar_gz(archive_handle_t *archive_handle) FAST_FUNC;
extern char get_header_tar_bz2(archive_handle_t *archive_handle) FAST_FUNC;
extern char get_header_tar_lzma(archive_handle_t *archive_handle) FAST_FUNC;

extern void seek_by_jump(const archive_handle_t *archive_handle, unsigned amount) FAST_FUNC;
extern void seek_by_read(const archive_handle_t *archive_handle, unsigned amount) FAST_FUNC;

extern void data_align(archive_handle_t *archive_handle, unsigned boundary) FAST_FUNC;
extern const llist_t *find_list_entry(const llist_t *list, const char *filename) FAST_FUNC;
extern const llist_t *find_list_entry2(const llist_t *list, const char *filename) FAST_FUNC;

/* A bit of bunzip2 internals are exposed for compressed help support: */
typedef struct bunzip_data bunzip_data;
int start_bunzip(bunzip_data **bdp, int in_fd, const unsigned char *inbuf, int len) FAST_FUNC;
int read_bunzip(bunzip_data *bd, char *outbuf, int len) FAST_FUNC;
void dealloc_bunzip(bunzip_data *bd) FAST_FUNC;

typedef struct inflate_unzip_result {
	off_t bytes_out;
	uint32_t crc;
} inflate_unzip_result;

USE_DESKTOP(long long) int inflate_unzip(inflate_unzip_result *res, off_t compr_size, int src_fd, int dst_fd) FAST_FUNC;
/* lzma unpacker takes .lzma stream from offset 0 */
USE_DESKTOP(long long) int unpack_lzma_stream(int src_fd, int dst_fd) FAST_FUNC;
/* the rest wants 2 first bytes already skipped by the caller */
USE_DESKTOP(long long) int unpack_bz2_stream(int src_fd, int dst_fd) FAST_FUNC;
USE_DESKTOP(long long) int unpack_gz_stream(int src_fd, int dst_fd) FAST_FUNC;
USE_DESKTOP(long long) int unpack_gz_stream_with_info(int src_fd, int dst_fd, unpack_info_t *info) FAST_FUNC;
USE_DESKTOP(long long) int unpack_Z_stream(int fd_in, int fd_out) FAST_FUNC;
/* wrapper which checks first two bytes to be "BZ" */
USE_DESKTOP(long long) int unpack_bz2_stream_prime(int src_fd, int dst_fd) FAST_FUNC;

int bbunpack(char **argv,
	     char* (*make_new_name)(char *filename),
	     USE_DESKTOP(long long) int (*unpacker)(unpack_info_t *info)) FAST_FUNC;

#if BB_MMU
void open_transformer(int fd,
	USE_DESKTOP(long long) int FAST_FUNC (*transformer)(int src_fd, int dst_fd)) FAST_FUNC;
#define open_transformer(fd, transformer, transform_prog) open_transformer(fd, transformer)
#else
void open_transformer(int src_fd, const char *transform_prog) FAST_FUNC;
#define open_transformer(fd, transformer, transform_prog) open_transformer(fd, transform_prog)
#endif

POP_SAVED_FUNCTION_VISIBILITY

#endif
