/*
 * Copyright (C) 2007 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * This file uses bzip2 library code which is written
 * by Julian Seward <jseward@bzip.org>.
 * See README and LICENSE files in bz/ directory for more information
 * about bzip2 library code.
 */

#include "libbb.h"

#define CONFIG_BZIP2_FEATURE_SPEED 1

/* Speed test:
 * Compiled with gcc 4.2.1, run on Athlon 64 1800 MHz (512K L2 cache).
 * Stock bzip2 is 26.4% slower than bbox bzip2 at SPEED 1
 * (time to compress gcc-4.2.1.tar is 126.4% compared to bbox).
 * At SPEED 5 difference is 32.7%.
 *
 * Test run of all CONFIG_BZIP2_FEATURE_SPEED values on a 11Mb text file:
 *     Size   Time (3 runs)
 * 0:  10828  4.145 4.146 4.148
 * 1:  11097  3.845 3.860 3.861
 * 2:  11392  3.763 3.767 3.768
 * 3:  11892  3.722 3.724 3.727
 * 4:  12740  3.637 3.640 3.644
 * 5:  17273  3.497 3.509 3.509
 */


#define BZ_DEBUG 0
/* Takes ~300 bytes, detects corruption caused by bad RAM etc */
#define BZ_LIGHT_DEBUG 0

#include "bz/bzlib.h"

#include "bz/bzlib_private.h"

#include "bz/blocksort.c"
#include "bz/bzlib.c"
#include "bz/compress.c"
#include "bz/huffman.c"

/* No point in being shy and having very small buffer here.
 * bzip2 internal buffers are much bigger anyway, hundreds of kbytes.
 * If iobuf is several pages long, malloc() may use mmap,
 * making iobuf is page aligned and thus (maybe) have one memcpy less
 * if kernel is clever enough.
 */
enum {
	IOBUF_SIZE = 8 * 1024
};

/* Returns:
 * <0 on write errors (examine errno),
 * >0 on short writes (errno == 0)
 * 0  no error (entire input consumed, gimme more)
 * on "impossible" errors (internal bzip2 compressor bug) dies
 */
static
ssize_t bz_write(bz_stream *strm, void* rbuf, ssize_t rlen, void *wbuf)
{
	int n, n2, ret;

	strm->avail_in = rlen;
	strm->next_in  = rbuf;
	while (1) {
		strm->avail_out = IOBUF_SIZE;
		strm->next_out = wbuf;

		ret = BZ2_bzCompress(strm, BZ_RUN);
		if (ret != BZ_RUN_OK)
			bb_error_msg_and_die("internal error %d", ret);

		n = IOBUF_SIZE - strm->avail_out;
		if (n) {
			/* short reads must have errno == 0 */
			errno = 0;
			n2 = full_write(STDOUT_FILENO, wbuf, n);
			if (n2 != n)
				return n2 ? n2 : 1;
		}

		if (strm->avail_in == 0)
			return 0;
	}
}


/*---------------------------------------------------*/
static
USE_DESKTOP(long long) int bz_write_tail(bz_stream *strm, void *wbuf)
{
	int n, n2, ret;
	USE_DESKTOP(long long) int total;

	total = -1;
	while (1) {
		strm->avail_out = IOBUF_SIZE;
		strm->next_out = wbuf;

		ret = BZ2_bzCompress(strm, BZ_FINISH);
		if (ret != BZ_FINISH_OK && ret != BZ_STREAM_END)
			bb_error_msg_and_die("internal error %d", ret);

		n = IOBUF_SIZE - strm->avail_out;
		if (n) {
			n2 = full_write(STDOUT_FILENO, wbuf, n);
			if (n2 != n)
				goto err;
		}

		if (ret == BZ_STREAM_END)
			break;
	}

	total = 0 USE_DESKTOP( + strm->total_out );
 err:
	BZ2_bzCompressEnd(strm);
	return total;
}


static
USE_DESKTOP(long long) int compressStream(void)
{
	USE_DESKTOP(long long) int total;
	ssize_t count;
	bz_stream bzs; /* it's small */
#define strm (&bzs)
	char *iobuf;
#define rbuf iobuf
#define wbuf (iobuf + IOBUF_SIZE)

	iobuf = xmalloc(2 * IOBUF_SIZE);

	BZ2_bzCompressInit(strm, 9 /*blockSize100k*/);

	while (1) {
		count = full_read(STDIN_FILENO, rbuf, IOBUF_SIZE);
		if (count < 0)
			bb_perror_msg("read error");
		if (count <= 0)
			break;
		count = bz_write(strm, rbuf, count, wbuf);
		if (count) {
			bb_perror_msg(count < 0 ? "write error" : "short write");
			break;
		}
	}

	total = bz_write_tail(strm, wbuf);
	free(iobuf);
	/* we had no error _only_ if count == 0 */
	return count == 0 ? total : -1;
}

static
char* make_new_name_bzip2(char *filename)
{
	return xasprintf("%s.bz2", filename);
}

int bzip2_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int bzip2_main(int argc, char **argv)
{
	unsigned opt;

	/* Must match bbunzip's constants OPT_STDOUT, OPT_FORCE! */
	opt = getopt32(argv, "cfv" USE_BUNZIP2("d") "q123456789" );
#if ENABLE_BUNZIP2 /* bunzip2_main may not be visible... */
	if (opt & 0x8) // -d
		return bunzip2_main(argc, argv);
#endif
	option_mask32 &= 0x7; /* ignore -q, -0..9 */
	//if (opt & 0x1) // -c
	//if (opt & 0x2) // -f
	//if (opt & 0x4) // -v
	argv += optind;

	return bbunpack(argv, make_new_name_bzip2, compressStream);
}
