/* vi: set sw=4 ts=4: */
/*
 * makemime: create MIME-encoded message
 * reformime: parse MIME-encoded message
 *
 * Copyright (C) 2008 by Vladimir Dronnikov <dronnikov@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

//usage:#define makemime_trivial_usage
//usage:       "[OPTIONS] [FILE]..."
//usage:#define makemime_full_usage "\n\n"
//usage:       "Create multipart MIME-encoded message from FILEs\n"
/* //usage:    "Transfer encoding is base64, disposition is inline (not attachment)\n" */
//usage:     "\nOptions:"
//usage:     "\n	-o FILE	Output. Default: stdout"
//usage:     "\n	-a HDR	Add header. Examples:"
//usage:     "\n		\"From: user@host.org\", \"Date: `date -R`\""
//usage:     "\n	-c CT	Content type. Default: text/plain"
//usage:     "\n	-C CS   Charset. Default: " CONFIG_FEATURE_MIME_CHARSET
/* //usage:  "\n	-e ENC	Transfer encoding. Ignored. base64 is assumed" */
//usage:     "\n"
//usage:     "\nOther options are silently ignored"

//usage:#define reformime_trivial_usage
//usage:       "[OPTIONS] [FILE]..."
//usage:#define reformime_full_usage "\n\n"
//usage:       "Parse MIME-encoded message\n"
//usage:     "\nOptions:"
//usage:     "\n	-x PREFIX	Extract content of MIME sections to files"
//usage:     "\n	-X PROG ARGS	Filter content of MIME sections through PROG"
//usage:     "\n			Must be the last option"
//usage:     "\n"
//usage:     "\nOther options are silently ignored"

#include "libbb.h"
#include "mail.h"

/*
  makemime -c type [-o file] [-e encoding] [-C charset] [-N name] \
                   [-a "Header: Contents"] file
           -m [ type ] [-o file] [-e encoding] [-a "Header: Contents"] file
           -j [-o file] file1 file2
           @file

   file:  filename    - read or write from filename
          -           - read or write from stdin or stdout
          &n          - read or write from file descriptor n
          \( opts \)  - read from child process, that generates [ opts ]

Options:

  -c type         - create a new MIME section from "file" with this
                    Content-Type: (default is application/octet-stream).
  -C charset      - MIME charset of a new text/plain section.
  -N name         - MIME content name of the new mime section.
  -m [ type ]     - create a multipart mime section from "file" of this
                    Content-Type: (default is multipart/mixed).
  -e encoding     - use the given encoding (7bit, 8bit, quoted-printable,
                    or base64), instead of guessing.  Omit "-e" and use
                    -c auto to set Content-Type: to text/plain or
                    application/octet-stream based on picked encoding.
  -j file1 file2  - join mime section file2 to multipart section file1.
  -o file         - write the result to file, instead of stdout (not
                    allowed in child processes).
  -a header       - prepend an additional header to the output.

  @file - read all of the above options from file, one option or
          value on each line.
  {which version of makemime is this? What do we support?}
*/


/* In busybox 1.15.0.svn, makemime generates output like this
 * (empty lines are shown exactly!):
{headers added with -a HDR}
Mime-Version: 1.0
Content-Type: multipart/mixed; boundary="24269534-2145583448-1655890676"

--24269534-2145583448-1655890676
Content-Type: {set by -c, e.g. text/plain}; charset={set by -C, e.g. us-ascii}
Content-Disposition: inline; filename="A"
Content-Transfer-Encoding: base64

...file A contents...
--24269534-2145583448-1655890676
Content-Type: {set by -c, e.g. text/plain}; charset={set by -C, e.g. us-ascii}
Content-Disposition: inline; filename="B"
Content-Transfer-Encoding: base64

...file B contents...
--24269534-2145583448-1655890676--

*/


/* For reference: here is an example email to LKML which has
 * 1st unnamed part (so it serves as an email body)
 * and one attached file:
...other headers...
Content-Type: multipart/mixed; boundary="=-tOfTf3byOS0vZgxEWcX+"
...other headers...
Mime-Version: 1.0
...other headers...


--=-tOfTf3byOS0vZgxEWcX+
Content-Type: text/plain
Content-Transfer-Encoding: 7bit

...email text...
...email text...


--=-tOfTf3byOS0vZgxEWcX+
Content-Disposition: attachment; filename="xyz"
Content-Type: text/plain; name="xyz"; charset="UTF-8"
Content-Transfer-Encoding: 7bit

...file contents...
...file contents...

--=-tOfTf3byOS0vZgxEWcX+--

...random junk added by mailing list robots and such...
*/

/* man makemime:

 * -c TYPE: create a (non-multipart) MIME section with Content-Type: TYPE
 * makemime -c TYPE [-e ENCODING] [-o OUTFILE] [-C CHARSET] [-N NAME] [-a HEADER...] FILE
 * The -C option sets the MIME charset attribute for text/plain content.
 * The -N option sets the name attribute for Content-Type:
 * Encoding must be one of the following: 7bit, 8bit, quoted-printable, or base64.

 * -m multipart/TYPE: create a multipart MIME collection with Content-Type: multipart/TYPE
 * makemime -m multipart/TYPE [-e ENCODING] [-o OUTFILE] [-a HEADER...] FILE
 * Type must be either "multipart/mixed", "multipart/alternative", or some other MIME multipart content type.
 * Additionally, encoding can only be "7bit" or "8bit", and will default to "8bit" if not specified.
 * Finally, filename must be a MIME-formatted section, NOT a regular file.
 * The -m option creates an initial multipart MIME collection, that contains only one MIME section, taken from filename.
 * The collection is written to standard output, or the pipe or to outputfile.

 * -j FILE1: add a section to a multipart MIME collection
 * makemime -j FILE1 [-o OUTFILE] FILE2
 * FILE1 must be a MIME collection that was previously created by the -m option.
 * FILE2 must be a MIME section that was previously created by the -c option.
 * The -j options adds the MIME section in FILE2 to the MIME collection in FILE1.
 */
int makemime_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int makemime_main(int argc UNUSED_PARAM, char **argv)
{
	llist_t *opt_headers = NULL, *l;
	const char *opt_output;
#define boundary opt_output

	enum {
		OPT_c = 1 << 0,         // create (non-multipart) section
		OPT_e = 1 << 1,         // Content-Transfer-Encoding. Ignored. Assumed base64
		OPT_o = 1 << 2,         // output to
		OPT_C = 1 << 3,         // charset
		OPT_N = 1 << 4,         // COMPAT
		OPT_a = 1 << 5,         // additional headers
		//OPT_m = 1 << 6,         // create mutipart section
		//OPT_j = 1 << 7,         // join section to multipart section
	};

	INIT_G();

	// parse options
	opt_complementary = "a::";
	opts = getopt32(argv,
		"c:e:o:C:N:a", //:m:j:",
		&G.content_type, NULL, &opt_output, &G.opt_charset, NULL, &opt_headers //, NULL, NULL
	);
	//argc -= optind;
	argv += optind;

	// respect -o output
	if (opts & OPT_o)
		freopen(opt_output, "w", stdout);

	// no files given on command line? -> use stdin
	if (!*argv)
		*--argv = (char *)"-";

	// put additional headers
	for (l = opt_headers; l; l = l->link)
		puts(l->data);

	// make a random string -- it will delimit message parts
	srand(monotonic_us());
	boundary = xasprintf("%u-%u-%u",
			(unsigned)rand(), (unsigned)rand(), (unsigned)rand());

	// put multipart header
	printf(
		"Mime-Version: 1.0\n"
		"Content-Type: multipart/mixed; boundary=\"%s\"\n"
		, boundary
	);

	// put attachments
	while (*argv) {
		printf(
			"\n--%s\n"
			"Content-Type: %s; charset=%s\n"
			"Content-Disposition: inline; filename=\"%s\"\n"
			"Content-Transfer-Encoding: base64\n"
			, boundary
			, G.content_type
			, G.opt_charset
			, bb_get_last_path_component_strip(*argv)
		);
		encode_base64(*argv++, (const char *)stdin, "");
	}

	// put multipart footer
	printf("\n--%s--\n" "\n", boundary);

	return EXIT_SUCCESS;
#undef boundary
}

static const char *find_token(const char *const string_array[], const char *key, const char *defvalue)
{
	const char *r = NULL;
	int i;
	for (i = 0; string_array[i] != NULL; i++) {
		if (strcasecmp(string_array[i], key) == 0) {
			r = (char *)string_array[i+1];
			break;
		}
	}
	return (r) ? r : defvalue;
}

static const char *xfind_token(const char *const string_array[], const char *key)
{
	const char *r = find_token(string_array, key, NULL);
	if (r)
		return r;
	bb_error_msg_and_die("header: %s", key);
}

enum {
	OPT_x = 1 << 0,
	OPT_X = 1 << 1,
#if ENABLE_FEATURE_REFORMIME_COMPAT
	OPT_d = 1 << 2,
	OPT_e = 1 << 3,
	OPT_i = 1 << 4,
	OPT_s = 1 << 5,
	OPT_r = 1 << 6,
	OPT_c = 1 << 7,
	OPT_m = 1 << 8,
	OPT_h = 1 << 9,
	OPT_o = 1 << 10,
	OPT_O = 1 << 11,
#endif
};

static int parse(const char *boundary, char **argv)
{
	char *line, *s, *p;
	const char *type;
	int boundary_len = strlen(boundary);
	const char *delims = " ;\"\t\r\n";
	const char *uniq;
	int ntokens;
	const char *tokens[32]; // 32 is enough

	// prepare unique string pattern
	uniq = xasprintf("%%llu.%u.%s", (unsigned)getpid(), safe_gethostname());

//bb_info_msg("PARSE[%s]", uniq);

	while ((line = xmalloc_fgets_str(stdin, "\r\n\r\n")) != NULL) {

		// seek to start of MIME section
		// N.B. to avoid false positives let us seek to the _last_ occurance
		p = NULL;
		s = line;
		while ((s = strcasestr(s, "Content-Type:")) != NULL)
			p = s++;
		if (!p)
			goto next;
//bb_info_msg("L[%s]", p);

		// split to tokens
		// TODO: strip of comments which are of form: (comment-text)
		ntokens = 0;
		tokens[ntokens] = NULL;
		for (s = strtok(p, delims); s; s = strtok(NULL, delims)) {
			tokens[ntokens] = s;
			if (ntokens < ARRAY_SIZE(tokens) - 1)
				ntokens++;
//bb_info_msg("L[%d][%s]", ntokens, s);
		}
		tokens[ntokens] = NULL;
//bb_info_msg("N[%d]", ntokens);

		// analyse tokens
		type = find_token(tokens, "Content-Type:", "text/plain");
//bb_info_msg("T[%s]", type);
		if (0 == strncasecmp(type, "multipart/", 10)) {
			if (0 == strcasecmp(type+10, "mixed")) {
				parse(xfind_token(tokens, "boundary="), argv);
			} else
				bb_error_msg_and_die("no support of content type '%s'", type);
		} else {
			pid_t pid = pid;
			int rc;
			FILE *fp;
			// fetch charset
			const char *charset = find_token(tokens, "charset=", CONFIG_FEATURE_MIME_CHARSET);
			// fetch encoding
			const char *encoding = find_token(tokens, "Content-Transfer-Encoding:", "7bit");
			// compose target filename
			char *filename = (char *)find_token(tokens, "filename=", NULL);
			if (!filename)
				filename = xasprintf(uniq, monotonic_us());
			else
				filename = bb_get_last_path_component_strip(xstrdup(filename));

			// start external helper, if any
			if (opts & OPT_X) {
				int fd[2];
				xpipe(fd);
				pid = vfork();
				if (0 == pid) {
					// child reads from fd[0]
					close(fd[1]);
					xmove_fd(fd[0], STDIN_FILENO);
					xsetenv("CONTENT_TYPE", type);
					xsetenv("CHARSET", charset);
					xsetenv("ENCODING", encoding);
					xsetenv("FILENAME", filename);
					BB_EXECVP_or_die(argv);
				}
				// parent dumps to fd[1]
				close(fd[0]);
				fp = xfdopen_for_write(fd[1]);
				signal(SIGPIPE, SIG_IGN); // ignore EPIPE
			// or create a file for dump
			} else {
				char *fname = xasprintf("%s%s", *argv, filename);
				fp = xfopen_for_write(fname);
				free(fname);
			}

			// housekeeping
			free(filename);

			// dump to fp
			if (0 == strcasecmp(encoding, "base64")) {
				read_base64(stdin, fp, '-');
			} else if (0 != strcasecmp(encoding, "7bit")
				&& 0 != strcasecmp(encoding, "8bit")
			) {
				// quoted-printable, binary, user-defined are unsupported so far
				bb_error_msg_and_die("no support of encoding '%s'", encoding);
			} else {
				// N.B. we have written redundant \n. so truncate the file
				// The following weird 2-tacts reading technique is due to
				// we have to not write extra \n at the end of the file
				// In case of -x option we could truncate the resulting file as
				// fseek(fp, -1, SEEK_END);
				// if (ftruncate(fileno(fp), ftell(fp)))
				//	bb_perror_msg("ftruncate");
				// But in case of -X we have to be much more careful. There is
				// no means to truncate what we already have sent to the helper.
				p = xmalloc_fgets_str(stdin, "\r\n");
				while (p) {
					s = xmalloc_fgets_str(stdin, "\r\n");
					if (s == NULL)
						break;
					if ('-' == s[0]
					 && '-' == s[1]
					 && 0 == strncmp(s+2, boundary, boundary_len)
					) {
						break;
					}
					fputs(p, fp);
					p = s;
				}

/*
				while ((s = xmalloc_fgetline_str(stdin, "\r\n")) != NULL) {
					if ('-' == s[0] && '-' == s[1]
						&& 0 == strncmp(s+2, boundary, boundary_len))
						break;
					fprintf(fp, "%s\n", s);
				}
				// N.B. we have written redundant \n. so truncate the file
				fseek(fp, -1, SEEK_END);
				if (ftruncate(fileno(fp), ftell(fp)))
					bb_perror_msg("ftruncate");
*/
			}
			fclose(fp);

			// finalize helper
			if (opts & OPT_X) {
				signal(SIGPIPE, SIG_DFL);
				// exit if helper exited >0
				rc = (wait4pid(pid) & 0xff);
				if (rc)
					return rc+20;
			}

			// check multipart finalized
			if (s && '-' == s[2+boundary_len] && '-' == s[2+boundary_len+1]) {
				free(line);
				break;
			}
		}
 next:
		free(line);
	}

//bb_info_msg("ENDPARSE[%s]", boundary);

	return EXIT_SUCCESS;
}

/*
Usage: reformime [options]
    -d - parse a delivery status notification.
    -e - extract contents of MIME section.
    -x - extract MIME section to a file.
    -X - pipe MIME section to a program.
    -i - show MIME info.
    -s n.n.n.n - specify MIME section.
    -r - rewrite message, filling in missing MIME headers.
    -r7 - also convert 8bit/raw encoding to quoted-printable, if possible.
    -r8 - also convert quoted-printable encoding to 8bit, if possible.
    -c charset - default charset for rewriting, -o, and -O.
    -m [file] [file]... - create a MIME message digest.
    -h "header" - decode RFC 2047-encoded header.
    -o "header" - encode unstructured header using RFC 2047.
    -O "header" - encode address list header using RFC 2047.
*/

int reformime_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int reformime_main(int argc UNUSED_PARAM, char **argv)
{
	const char *opt_prefix = "";

	INIT_G();

	// parse options
	// N.B. only -x and -X are supported so far
	opt_complementary = "x--X:X--x" IF_FEATURE_REFORMIME_COMPAT(":m::");
	opts = getopt32(argv,
		"x:X" IF_FEATURE_REFORMIME_COMPAT("deis:r:c:m:h:o:O:"),
		&opt_prefix
		IF_FEATURE_REFORMIME_COMPAT(, NULL, NULL, &G.opt_charset, NULL, NULL, NULL, NULL)
	);
	//argc -= optind;
	argv += optind;

	return parse("", (opts & OPT_X) ? argv : (char **)&opt_prefix);
}
