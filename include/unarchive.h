#include <stdio.h>	// for FILE
#include <unistd.h>	// for off_t

enum extract_functions_e {
	extract_verbose_list = 1,
	extract_list = 2,
	extract_one_to_buffer = 4,
	extract_to_stdout = 8,
	extract_all_to_fs = 16,
	extract_preserve_date = 32,
	extract_data_tar_gz = 64,
	extract_control_tar_gz = 128,
	extract_unzip_only = 256,
	extract_unconditional = 512,
	extract_create_leading_dirs = 1024,
	extract_quiet = 2048,
	extract_exclude_list = 4096
};

typedef struct file_headers_s {
	char *name;
	char *link_name;
	off_t size;
	uid_t uid;
	gid_t gid;
	mode_t mode;
	time_t mtime;
	dev_t device;
	int (*extract_func)(FILE *, FILE *);
} file_header_t;

file_header_t *get_header_ar(FILE *in_file);
file_header_t *get_header_cpio(FILE *src_stream);
file_header_t *get_header_tar(FILE *tar_stream);
file_header_t *get_header_zip(FILE *zip_stream);

void seek_sub_file(FILE *src_stream, const int count);

extern off_t archive_offset;

char *unarchive(FILE *src_stream, FILE *out_stream, file_header_t *(*get_headers)(FILE *),
	const int extract_function, const char *prefix, char **include_name, char **exclude_name);

char *deb_extract(const char *package_filename, FILE *out_stream, const int extract_function,
	const char *prefix, const char *filename);
