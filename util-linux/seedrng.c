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
//usage:	"Seed the kernel RNG from seed files."
//usage:	"\n"
//usage:	"\n	-d, --seed-dir DIR	Use seed files from specified directory (default: /var/lib/seedrng)"
//usage:	"\n	-n, --skip-credit	Skip crediting seeds, even if creditable"

#include "libbb.h"

#include <linux/random.h>
#include <sys/random.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <endian.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef GRND_INSECURE
#define GRND_INSECURE 0x0004 /* Apparently some headers don't ship with this yet. */
#endif

#define DEFAULT_SEED_DIR "/var/lib/seedrng"
#define CREDITABLE_SEED_NAME "seed.credit"
#define NON_CREDITABLE_SEED_NAME "seed.no-credit"

enum seedrng_lengths {
	MIN_SEED_LEN = SHA256_OUTSIZE,
	MAX_SEED_LEN = 512
};

static size_t determine_optimal_seed_len(void)
{
	char poolsize_str[11] = { 0 };
	unsigned long poolsize;

	if (open_read_close("/proc/sys/kernel/random/poolsize", poolsize_str, sizeof(poolsize_str) - 1) < 0) {
		bb_perror_msg("unable to determine pool size, assuming %u bits", MIN_SEED_LEN * 8);
		return MIN_SEED_LEN;
	}
	poolsize = (bb_strtoul(poolsize_str, NULL, 10) + 7) / 8;
	return MAX(MIN(poolsize, MAX_SEED_LEN), MIN_SEED_LEN);
}

static int read_new_seed(uint8_t *seed, size_t len, bool *is_creditable)
{
	ssize_t ret;

	*is_creditable = false;
	ret = getrandom(seed, len, GRND_NONBLOCK);
	if (ret == (ssize_t)len) {
		*is_creditable = true;
		return 0;
	} else if (ret < 0 && errno == ENOSYS) {
		struct pollfd random_fd = {
			.fd = open("/dev/random", O_RDONLY),
			.events = POLLIN
		};
		if (random_fd.fd < 0)
			return -1;
		*is_creditable = safe_poll(&random_fd, 1, 0) == 1;
		close(random_fd.fd);
	} else if (getrandom(seed, len, GRND_INSECURE) == (ssize_t)len)
		return 0;
	if (open_read_close("/dev/urandom", seed, len) == (ssize_t)len)
		return 0;
	return -1;
}

static int seed_rng(uint8_t *seed, size_t len, bool credit)
{
	struct {
		int entropy_count;
		int buf_size;
		uint8_t buffer[MAX_SEED_LEN];
	} req = {
		.entropy_count = credit ? len * 8 : 0,
		.buf_size = len
	};
	int random_fd, ret;

	if (len > sizeof(req.buffer)) {
		errno = EFBIG;
		return -1;
	}
	memcpy(req.buffer, seed, len);

	random_fd = open("/dev/urandom", O_RDONLY);
	if (random_fd < 0)
		return -1;
	ret = ioctl(random_fd, RNDADDENTROPY, &req);
	if (ret)
		ret = -errno ? -errno : -EIO;
	if (ENABLE_FEATURE_CLEAN_UP)
		close(random_fd);
	errno = -ret;
	return ret ? -1 : 0;
}

static int seed_from_file_if_exists(const char *filename, int dfd, bool credit, sha256_ctx_t *hash)
{
	uint8_t seed[MAX_SEED_LEN];
	ssize_t seed_len;

	seed_len = open_read_close(filename, seed, sizeof(seed));
	if (seed_len < 0) {
		if (errno == ENOENT)
			return 0;
		bb_perror_msg("unable to%s seed", " read");
		return -1;
	}
	if ((unlink(filename) < 0 || fsync(dfd) < 0) && seed_len) {
		bb_perror_msg("unable to%s seed", " remove");
		return -1;
	} else if (!seed_len)
		return 0;

	sha256_hash(hash, &seed_len, sizeof(seed_len));
	sha256_hash(hash, seed, seed_len);

	printf("Seeding %zd bits %s crediting\n", seed_len * 8, credit ? "and" : "without");
	if (seed_rng(seed, seed_len, credit) < 0) {
		bb_perror_msg("unable to%s seed", "");
		return -1;
	}
	return 0;
}

int seedrng_main(int argc, char *argv[]) MAIN_EXTERNALLY_VISIBLE;
int seedrng_main(int argc UNUSED_PARAM, char *argv[])
{
	const char *seed_dir = DEFAULT_SEED_DIR, *creditable_seed, *non_creditable_seed;
	int ret, fd = -1, dfd = -1, program_ret = 0;
	uint8_t new_seed[MAX_SEED_LEN];
	size_t new_seed_len;
	bool new_seed_creditable;
	bool skip_credit = false;
	struct timespec timestamp = { 0 };
	sha256_ctx_t hash;

	int opt;
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

	opt = getopt32long(argv, "d:n", longopts, &seed_dir);
	skip_credit = opt & OPT_n;
	creditable_seed = concat_path_file(seed_dir, CREDITABLE_SEED_NAME);
	non_creditable_seed = concat_path_file(seed_dir, NON_CREDITABLE_SEED_NAME);

	umask(0077);
	if (getuid())
		bb_simple_error_msg_and_die(bb_msg_you_must_be_root);

	if (mkdir(seed_dir, 0700) < 0 && errno != EEXIST)
		bb_perror_msg_and_die("unable to %s seed directory", "create");

	dfd = open(seed_dir, O_DIRECTORY | O_RDONLY);
	if (dfd < 0 || flock(dfd, LOCK_EX) < 0) {
		bb_perror_msg("unable to %s seed directory", "lock");
		program_ret = 1;
		goto out;
	}

	sha256_begin(&hash);
	sha256_hash(&hash, "SeedRNG v1 Old+New Prefix", 25);
	clock_gettime(CLOCK_REALTIME, &timestamp);
	sha256_hash(&hash, &timestamp, sizeof(timestamp));
	clock_gettime(CLOCK_BOOTTIME, &timestamp);
	sha256_hash(&hash, &timestamp, sizeof(timestamp));

	ret = seed_from_file_if_exists(non_creditable_seed, dfd, false, &hash);
	if (ret < 0)
		program_ret |= 1 << 1;
	ret = seed_from_file_if_exists(creditable_seed, dfd, !skip_credit, &hash);
	if (ret < 0)
		program_ret |= 1 << 2;

	new_seed_len = determine_optimal_seed_len();
	ret = read_new_seed(new_seed, new_seed_len, &new_seed_creditable);
	if (ret < 0) {
		bb_perror_msg("unable to%s seed", " read new");
		new_seed_len = SHA256_OUTSIZE;
		memset(new_seed, 0, SHA256_OUTSIZE);
		program_ret |= 1 << 3;
	}
	sha256_hash(&hash, &new_seed_len, sizeof(new_seed_len));
	sha256_hash(&hash, new_seed, new_seed_len);
	sha256_end(&hash, new_seed + new_seed_len - SHA256_OUTSIZE);

	printf("Saving %zu bits of %screditable seed for next boot\n", new_seed_len * 8, new_seed_creditable ? "" : "non-");
	fd = open(non_creditable_seed, O_WRONLY | O_CREAT | O_TRUNC, 0400);
	if (fd < 0 || full_write(fd, new_seed, new_seed_len) != (ssize_t)new_seed_len || fsync(fd) < 0) {
		bb_perror_msg("unable to%s seed", " write");
		program_ret |= 1 << 4;
		goto out;
	}
	if (new_seed_creditable && rename(non_creditable_seed, creditable_seed) < 0) {
		bb_simple_perror_msg("unable to make new seed creditable");
		program_ret |= 1 << 5;
	}
out:
	if (ENABLE_FEATURE_CLEAN_UP && fd >= 0)
		close(fd);
	if (ENABLE_FEATURE_CLEAN_UP && dfd >= 0)
		close(dfd);
	return program_ret;
}
