// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Copyright (C) 2022 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 *
 * SeedRNG is a simple program made for seeding the Linux kernel random number
 * generator from seed files. It is is useful in light of the fact that the
 * Linux kernel RNG cannot be initialized from shell scripts, and new seeds
 * cannot be safely generated from boot time shell scripts either. It should
 * be run once at init time and once at shutdown time. It can be run at other
 * times on a timer as well. Whenever it is run, it writes existing seed files
 * into the RNG pool, and then creates a new seed file. If the RNG is
 * initialized at the time of creating a new seed file, then that new seed file
 * is marked as "creditable", which means it can be used to initialize the RNG.
 * Otherwise, it is marked as "non-creditable", in which case it is still used
 * to seed the RNG's pool, but will not initialize the RNG. In order to ensure
 * that entropy only ever stays the same or increases from one seed file to the
 * next, old seed values are hashed together with new seed values when writing
 * new seed files.
 *
 * This is based on code from <https://git.zx2c4.com/seedrng/about/>.
 */

//config:config SEEDRNG
//config:	bool "seedrng (2 kb)"
//config:	default y
//config:	help
//config:	Seed the kernel RNG from seed files, meant to be called
//config:	once during startup, once during shutdown, and optionally
//config:	at some periodic interval in between.

//applet:IF_SEEDRNG(APPLET(seedrng, BB_DIR_USR_SBIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_SEEDRNG) += seedrng.o

//usage:#define seedrng_trivial_usage
//usage:	"[-d SEED_DIRECTORY] [-n]"
//usage:#define seedrng_full_usage "\n\n"
//usage:	"Seed the kernel RNG from seed files"
//usage:	"\n"
//usage:	"\n	-d DIR	Use seed files from DIR (default: /var/lib/seedrng)"
//usage:	"\n	-n	Skip crediting seeds, even if creditable"

#include "libbb.h"

#include <linux/random.h>
#include <sys/random.h>
#include <sys/file.h>

#ifndef GRND_INSECURE
#define GRND_INSECURE 0x0004 /* Apparently some headers don't ship with this yet. */
#endif

#define DEFAULT_SEED_DIR "/var/lib/seedrng"
#define CREDITABLE_SEED_NAME "seed.credit"
#define NON_CREDITABLE_SEED_NAME "seed.no-credit"

enum {
	MIN_SEED_LEN = SHA256_OUTSIZE,
	MAX_SEED_LEN = 512
};

static size_t determine_optimal_seed_len(void)
{
	char poolsize_str[12];
	unsigned poolsize;
	int n;

	n = open_read_close("/proc/sys/kernel/random/poolsize", poolsize_str, sizeof(poolsize_str) - 1);
	if (n < 0) {
		bb_perror_msg("can't determine pool size, assuming %u bits", MIN_SEED_LEN * 8);
		return MIN_SEED_LEN;
	}
	poolsize_str[n] = '\0';
	poolsize = (bb_strtoul(poolsize_str, NULL, 10) + 7) / 8;
	return MAX(MIN(poolsize, MAX_SEED_LEN), MIN_SEED_LEN);
}

static bool read_new_seed(uint8_t *seed, size_t len)
{
	bool is_creditable;
	ssize_t ret;

	ret = getrandom(seed, len, GRND_NONBLOCK);
	if (ret == (ssize_t)len) {
		return true;
	}
	if (ret < 0 && errno == ENOSYS) {
		struct pollfd random_fd = {
			.fd = xopen("/dev/random", O_RDONLY),
			.events = POLLIN
		};
		is_creditable = poll(&random_fd, 1, 0) == 1;
//This is racy. is_creditable can be set to true here, but other process
//can consume "good" random data from /dev/urandom before we do it below.
		close(random_fd.fd);
	} else {
		if (getrandom(seed, len, GRND_INSECURE) == (ssize_t)len)
			return false;
		is_creditable = false;
	}

	/* Either getrandom() is not implemented, or
	 * getrandom(GRND_INSECURE) did not give us LEN bytes.
	 * Fallback to reading /dev/urandom.
	 */
	errno = 0;
	if (open_read_close("/dev/urandom", seed, len) != (ssize_t)len)
		bb_perror_msg_and_die("can't read '%s'", "/dev/urandom");
	return is_creditable;
}

static void seed_rng(uint8_t *seed, size_t len, bool credit)
{
	struct {
		int entropy_count;
		int buf_size;
		uint8_t buffer[MAX_SEED_LEN];
	} req;
	int random_fd;

	req.entropy_count = credit ? len * 8 : 0;
	req.buf_size = len;
	memcpy(req.buffer, seed, len);

	random_fd = xopen("/dev/urandom", O_RDONLY);
	xioctl(random_fd, RNDADDENTROPY, &req);
	if (ENABLE_FEATURE_CLEAN_UP)
		close(random_fd);
}

static void seed_from_file_if_exists(const char *filename, bool credit, sha256_ctx_t *hash)
{
	uint8_t seed[MAX_SEED_LEN];
	ssize_t seed_len;

	seed_len = open_read_close(filename, seed, sizeof(seed));
	if (seed_len < 0) {
		if (errno != ENOENT)
			bb_perror_msg_and_die("can't%s seed", " read");
		return;
	}
	xunlink(filename);
	if (seed_len != 0) {
		sha256_hash(hash, &seed_len, sizeof(seed_len));
		sha256_hash(hash, seed, seed_len);
		printf("Seeding %u bits %s crediting\n", (unsigned)seed_len * 8, credit ? "and" : "without");
		seed_rng(seed, seed_len, credit);
	}
}

int seedrng_main(int argc, char *argv[]) MAIN_EXTERNALLY_VISIBLE;
int seedrng_main(int argc UNUSED_PARAM, char *argv[])
{
	const char *seed_dir;
	int fd, dfd, program_ret = 0;
	uint8_t new_seed[MAX_SEED_LEN];
	size_t new_seed_len;
	bool new_seed_creditable, skip_credit = false;
	struct timespec timestamp;
	sha256_ctx_t hash;

	enum {
		OPT_d = (1 << 0),
		OPT_n = (1 << 1)
	};
#if ENABLE_LONG_OPTS
	static const char longopts[] ALIGN1 =
		"seed-dir\0"	Required_argument	"d"
		"skip-credit\0"	No_argument		"n"
		;
#endif

	seed_dir = DEFAULT_SEED_DIR;
	skip_credit = getopt32long(argv, "d:n", longopts, &seed_dir) & OPT_n;
	umask(0077);
	if (getuid() != 0)
		bb_simple_error_msg_and_die(bb_msg_you_must_be_root);

	if (mkdir(seed_dir, 0700) < 0 && errno != EEXIST)
		bb_perror_msg_and_die("can't %s seed directory", "create");
	dfd = open(seed_dir, O_DIRECTORY | O_RDONLY);
	if (dfd < 0 || flock(dfd, LOCK_EX) < 0)
		bb_perror_msg_and_die("can't %s seed directory", "lock");
	xfchdir(dfd);

	sha256_begin(&hash);
	sha256_hash(&hash, "SeedRNG v1 Old+New Prefix", 25);
	clock_gettime(CLOCK_REALTIME, &timestamp);
	sha256_hash(&hash, &timestamp, sizeof(timestamp));
	clock_gettime(CLOCK_BOOTTIME, &timestamp);
	sha256_hash(&hash, &timestamp, sizeof(timestamp));

	for (int i = 1; i < 3; ++i) {
		seed_from_file_if_exists(i == 1 ? NON_CREDITABLE_SEED_NAME : CREDITABLE_SEED_NAME,
					i == 1 ? false : !skip_credit,
					&hash);
	}

	new_seed_len = determine_optimal_seed_len();
	new_seed_creditable = read_new_seed(new_seed, new_seed_len);
	sha256_hash(&hash, &new_seed_len, sizeof(new_seed_len));
	sha256_hash(&hash, new_seed, new_seed_len);
	sha256_end(&hash, new_seed + new_seed_len - SHA256_OUTSIZE);

	printf("Saving %u bits of %screditable seed for next boot\n",
			(unsigned)new_seed_len * 8, new_seed_creditable ? "" : "non-");
	fd = open(NON_CREDITABLE_SEED_NAME, O_WRONLY | O_CREAT | O_TRUNC, 0400);
	if (fd < 0 || full_write(fd, new_seed, new_seed_len) != (ssize_t)new_seed_len || fsync(fd) < 0) {
		bb_perror_msg("can't%s seed", " write");
		return program_ret | (1 << 4);
	}
	if (new_seed_creditable && rename(NON_CREDITABLE_SEED_NAME, CREDITABLE_SEED_NAME) < 0) {
		bb_simple_perror_msg("can't make new seed creditable");
		return program_ret | (1 << 5);
	}
	return program_ret;
}
