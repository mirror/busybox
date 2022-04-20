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
//config:	bool "seedrng (2.4 kb)"
//config:	default y
//config:	help
//config:	Seed the kernel RNG from seed files, meant to be called
//config:	once during startup, once during shutdown, and optionally
//config:	at some periodic interval in between.

//applet:IF_SEEDRNG(APPLET(seedrng, BB_DIR_USR_SBIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_SEEDRNG) += seedrng.o

//usage:#define seedrng_trivial_usage
//usage:	"[-d SEED_DIRECTORY] [-l LOCK_FILE] [-n]"
//usage:#define seedrng_full_usage "\n\n"
//usage:	"Seed the kernel RNG from seed files."
//usage:	"\n"
//usage:	"\n	-d, --seed-dir DIR	Use seed files from specified directory (default: /var/lib/seedrng)"
//usage:	"\n	-l, --lock-file FILE	Use file as exclusive lock (default: /var/run/seedrng.lock)"
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

#if ENABLE_PID_FILE_PATH
#define PID_FILE_PATH CONFIG_PID_FILE_PATH
#else
#define PID_FILE_PATH "/var/run"
#endif

#define DEFAULT_SEED_DIR "/var/lib/seedrng"
#define DEFAULT_LOCK_FILE PID_FILE_PATH "/seedrng.lock"
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
		bb_perror_msg("unable to determine pool size, falling back to %u bits", MIN_SEED_LEN * 8);
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
	if (!errno)
		errno = EIO;
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

	random_fd = open("/dev/random", O_RDWR);
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
	int ret = 0;

	seed_len = open_read_close(filename, seed, sizeof(seed));
	if (seed_len < 0) {
		if (errno != ENOENT) {
			ret = -errno;
			bb_simple_perror_msg("unable to read seed file");
		}
		goto out;
	}
	if ((unlink(filename) < 0 || fsync(dfd) < 0) && seed_len) {
		ret = -errno;
		bb_simple_perror_msg("unable to remove seed after reading, so not seeding");
		goto out;
	}
	if (!seed_len)
		goto out;

	sha256_hash(hash, &seed_len, sizeof(seed_len));
	sha256_hash(hash, seed, seed_len);

	printf("Seeding %zd bits %s crediting\n", seed_len * 8, credit ? "and" : "without");
	ret = seed_rng(seed, seed_len, credit);
	if (ret < 0)
		bb_simple_perror_msg("unable to seed");
out:
	errno = -ret;
	return ret ? -1 : 0;
}

int seedrng_main(int argc, char *argv[]) MAIN_EXTERNALLY_VISIBLE;
int seedrng_main(int argc UNUSED_PARAM, char *argv[])
{
	static const char seedrng_prefix[] = "SeedRNG v1 Old+New Prefix";
	static const char seedrng_failure[] = "SeedRNG v1 No New Seed Failure";
	char *seed_dir, *lock_file, *creditable_seed, *non_creditable_seed;
	int ret, fd = -1, dfd = -1, lock, program_ret = 0;
	uint8_t new_seed[MAX_SEED_LEN];
	size_t new_seed_len;
	bool new_seed_creditable;
	bool skip_credit = false;
	struct timespec timestamp = { 0 };
	sha256_ctx_t hash;

	int opt;
	enum {
		OPT_d = (1 << 0),
		OPT_l = (1 << 1),
		OPT_n = (1 << 2)
	};
#if ENABLE_LONG_OPTS
	static const char longopts[] ALIGN1 =
		"seed-dir\0"	Required_argument	"d"
		"lock-file\0"	Required_argument	"l"
		"skip-credit\0"	No_argument		"n"
		;
#endif

	opt = getopt32long(argv, "d:l:n", longopts, &seed_dir, &lock_file);
	if (!(opt & OPT_d) || !seed_dir)
		seed_dir = xstrdup(DEFAULT_SEED_DIR);
	if (!(opt & OPT_l) || !lock_file)
		lock_file = xstrdup(DEFAULT_LOCK_FILE);
	skip_credit = opt & OPT_n;
	creditable_seed = concat_path_file(seed_dir, CREDITABLE_SEED_NAME);
	non_creditable_seed = concat_path_file(seed_dir, NON_CREDITABLE_SEED_NAME);

	umask(0077);
	if (getuid())
		bb_simple_error_msg_and_die("this program requires root");

	if (mkdir(seed_dir, 0700) < 0 && errno != EEXIST)
		bb_simple_perror_msg_and_die("unable to create seed directory");

	lock = open(lock_file, O_WRONLY | O_CREAT, 0000);
	if (lock < 0 || flock(lock, LOCK_EX) < 0) {
		bb_simple_perror_msg("unable to open lock file");
		program_ret = 1;
		goto out;
	}

	dfd = open(seed_dir, O_DIRECTORY | O_RDONLY);
	if (dfd < 0) {
		bb_simple_perror_msg("unable to open seed directory");
		program_ret = 1;
		goto out;
	}

	sha256_begin(&hash);
	sha256_hash(&hash, seedrng_prefix, strlen(seedrng_prefix));
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
		bb_simple_perror_msg("unable to read new seed");
		new_seed_len = SHA256_OUTSIZE;
		strncpy((char *)new_seed, seedrng_failure, new_seed_len);
		program_ret |= 1 << 3;
	}
	sha256_hash(&hash, &new_seed_len, sizeof(new_seed_len));
	sha256_hash(&hash, new_seed, new_seed_len);
	sha256_end(&hash, new_seed + new_seed_len - SHA256_OUTSIZE);

	printf("Saving %zu bits of %s seed for next boot\n", new_seed_len * 8, new_seed_creditable ? "creditable" : "non-creditable");
	fd = open(non_creditable_seed, O_WRONLY | O_CREAT | O_TRUNC, 0400);
	if (fd < 0) {
		bb_simple_perror_msg("unable to open seed file for writing");
		program_ret |= 1 << 4;
		goto out;
	}
	if (write(fd, new_seed, new_seed_len) != (ssize_t)new_seed_len || fsync(fd) < 0) {
		bb_simple_perror_msg("unable to write seed file");
		program_ret |= 1 << 5;
		goto out;
	}
	if (new_seed_creditable && rename(non_creditable_seed, creditable_seed) < 0) {
		bb_simple_perror_msg("unable to make new seed creditable");
		program_ret |= 1 << 6;
	}
out:
	if (ENABLE_FEATURE_CLEAN_UP && fd >= 0)
		close(fd);
	if (ENABLE_FEATURE_CLEAN_UP && dfd >= 0)
		close(dfd);
	if (ENABLE_FEATURE_CLEAN_UP && lock >= 0)
		close(lock);
	return program_ret;
}
