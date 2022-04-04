// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Copyright (C) 2022 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 *
 * This is based on code from <https://git.zx2c4.com/seedrng/about/>.
 */

//config:config SEEDRNG
//config:	bool "seedrng (3.8 kb)"
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

#ifndef LOCALSTATEDIR
#define LOCALSTATEDIR "/var/lib"
#endif
#ifndef RUNSTATEDIR
#define RUNSTATEDIR "/var/run"
#endif

#define DEFAULT_SEED_DIR LOCALSTATEDIR "/seedrng"
#define DEFAULT_LOCK_FILE RUNSTATEDIR "/seedrng.lock"
#define CREDITABLE_SEED_NAME "seed.credit"
#define NON_CREDITABLE_SEED_NAME "seed.no-credit"

static char *seed_dir, *lock_file, *creditable_seed, *non_creditable_seed;

enum seedrng_lengths {
	MAX_SEED_LEN = 512,
	MIN_SEED_LEN = SHA256_OUTSIZE
};

#ifndef DIV_ROUND_UP
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#endif

static size_t determine_optimal_seed_len(void)
{
	size_t ret = 0;
	char poolsize_str[11] = { 0 };
	int fd = open("/proc/sys/kernel/random/poolsize", O_RDONLY);

	if (fd < 0 || read(fd, poolsize_str, sizeof(poolsize_str) - 1) < 0) {
		fprintf(stderr, "WARNING: Unable to determine pool size, falling back to %u bits: %s\n", MIN_SEED_LEN * 8, strerror(errno));
		ret = MIN_SEED_LEN;
	} else
		ret = DIV_ROUND_UP(strtoul(poolsize_str, NULL, 10), 8);
	if (fd >= 0)
		close(fd);
	if (ret < MIN_SEED_LEN)
		ret = MIN_SEED_LEN;
	else if (ret > MAX_SEED_LEN)
		ret = MAX_SEED_LEN;
	return ret;
}

static int read_new_seed(uint8_t *seed, size_t len, bool *is_creditable)
{
	ssize_t ret;
	int urandom_fd;

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
			return -errno;
		*is_creditable = poll(&random_fd, 1, 0) == 1;
		close(random_fd.fd);
	} else if (getrandom(seed, len, GRND_INSECURE) == (ssize_t)len)
		return 0;
	urandom_fd = open("/dev/urandom", O_RDONLY);
	if (urandom_fd < 0)
		return -errno;
	ret = read(urandom_fd, seed, len);
	if (ret == (ssize_t)len)
		ret = 0;
	else
		ret = -errno ? -errno : -EIO;
	close(urandom_fd);
	return ret;
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

	if (len > sizeof(req.buffer))
		return -EFBIG;
	memcpy(req.buffer, seed, len);

	random_fd = open("/dev/random", O_RDWR);
	if (random_fd < 0)
		return -errno;
	ret = ioctl(random_fd, RNDADDENTROPY, &req);
	if (ret)
		ret = -errno ? -errno : -EIO;
	close(random_fd);
	return ret;
}

static int seed_from_file_if_exists(const char *filename, bool credit, sha256_ctx_t *hash)
{
	uint8_t seed[MAX_SEED_LEN];
	ssize_t seed_len;
	int fd, dfd, ret = 0;

	fd = open(filename, O_RDONLY);
	if (fd < 0 && errno == ENOENT)
		return 0;
	else if (fd < 0) {
		ret = -errno;
		fprintf(stderr, "ERROR: Unable to open seed file: %s\n", strerror(errno));
		return ret;
	}
	dfd = open(seed_dir, O_DIRECTORY | O_RDONLY);
	if (dfd < 0) {
		ret = -errno;
		close(fd);
		fprintf(stderr, "ERROR: Unable to open seed directory: %s\n", strerror(errno));
		return ret;
	}
	seed_len = read(fd, seed, sizeof(seed));
	if (seed_len < 0) {
		ret = -errno;
		fprintf(stderr, "ERROR: Unable to read seed file: %s\n", strerror(errno));
	}
	close(fd);
	if (ret) {
		close(dfd);
		return ret;
	}
	if ((unlink(filename) < 0 || fsync(dfd) < 0) && seed_len) {
		ret = -errno;
		fprintf(stderr, "ERROR: Unable to remove seed after reading, so not seeding: %s\n", strerror(errno));
	}
	close(dfd);
	if (ret)
		return ret;
	if (!seed_len)
		return 0;

	sha256_hash(hash, &seed_len, sizeof(seed_len));
	sha256_hash(hash, seed, seed_len);

	fprintf(stdout, "Seeding %zd bits %s crediting\n", seed_len * 8, credit ? "and" : "without");
	ret = seed_rng(seed, seed_len, credit);
	if (ret < 0)
		fprintf(stderr, "ERROR: Unable to seed: %s\n", strerror(-ret));
	return ret;
}

int seedrng_main(int argc, char *argv[]) MAIN_EXTERNALLY_VISIBLE;
int seedrng_main(int argc UNUSED_PARAM, char *argv[])
{
	static const char seedrng_prefix[] = "SeedRNG v1 Old+New Prefix";
	static const char seedrng_failure[] = "SeedRNG v1 No New Seed Failure";
	int ret, fd = -1, lock, program_ret = 0;
	uint8_t new_seed[MAX_SEED_LEN];
	size_t new_seed_len;
	bool new_seed_creditable;
	bool skip_credit = false;
	struct timespec realtime = { 0 }, boottime = { 0 };
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
	creditable_seed = xasprintf("%s/%s", seed_dir, CREDITABLE_SEED_NAME);
	non_creditable_seed = xasprintf("%s/%s", seed_dir, NON_CREDITABLE_SEED_NAME);

	umask(0077);
	if (getuid()) {
		fprintf(stderr, "ERROR: This program requires root\n");
		return 1;
	}

	sha256_begin(&hash);
	sha256_hash(&hash, seedrng_prefix, strlen(seedrng_prefix));
	clock_gettime(CLOCK_REALTIME, &realtime);
	clock_gettime(CLOCK_BOOTTIME, &boottime);
	sha256_hash(&hash, &realtime, sizeof(realtime));
	sha256_hash(&hash, &boottime, sizeof(boottime));

	if (mkdir(seed_dir, 0700) < 0 && errno != EEXIST) {
		fprintf(stderr, "ERROR: Unable to create \"%s\" directory: %s\n", seed_dir, strerror(errno));
		return 1;
	}

	lock = open(lock_file, O_WRONLY | O_CREAT, 0000);
	if (lock < 0 || flock(lock, LOCK_EX) < 0) {
		fprintf(stderr, "ERROR: Unable to open lock file: %s\n", strerror(errno));
		program_ret = 1;
		goto out;
	}

	ret = seed_from_file_if_exists(non_creditable_seed, false, &hash);
	if (ret < 0)
		program_ret |= 1 << 1;
	ret = seed_from_file_if_exists(creditable_seed, !skip_credit, &hash);
	if (ret < 0)
		program_ret |= 1 << 2;

	new_seed_len = determine_optimal_seed_len();
	ret = read_new_seed(new_seed, new_seed_len, &new_seed_creditable);
	if (ret < 0) {
		fprintf(stderr, "ERROR: Unable to read new seed: %s\n", strerror(-ret));
		new_seed_len = SHA256_OUTSIZE;
		strncpy((char *)new_seed, seedrng_failure, new_seed_len);
		program_ret |= 1 << 3;
	}
	sha256_hash(&hash, &new_seed_len, sizeof(new_seed_len));
	sha256_hash(&hash, new_seed, new_seed_len);
	sha256_end(&hash, new_seed + new_seed_len - SHA256_OUTSIZE);

	fprintf(stdout, "Saving %zu bits of %s seed for next boot\n", new_seed_len * 8, new_seed_creditable ? "creditable" : "non-creditable");
	fd = open(non_creditable_seed, O_WRONLY | O_CREAT | O_TRUNC, 0400);
	if (fd < 0) {
		fprintf(stderr, "ERROR: Unable to open seed file for writing: %s\n", strerror(errno));
		program_ret |= 1 << 4;
		goto out;
	}
	if (write(fd, new_seed, new_seed_len) != (ssize_t)new_seed_len || fsync(fd) < 0) {
		fprintf(stderr, "ERROR: Unable to write seed file: %s\n", strerror(errno));
		program_ret |= 1 << 5;
		goto out;
	}
	if (new_seed_creditable && rename(non_creditable_seed, creditable_seed) < 0) {
		fprintf(stderr, "WARNING: Unable to make new seed creditable: %s\n", strerror(errno));
		program_ret |= 1 << 6;
	}
out:
	if (fd >= 0)
		close(fd);
	if (lock >= 0)
		close(lock);
	return program_ret;
}
