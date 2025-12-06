/*
 * pmap implementation for busybox
 *
 * Copyright (C) 2010 Nokia Corporation. All rights reserved.
 * Written by Alexander Shishkin <virtuoso@slind.org>
 *
 * Licensed under GPLv2 or later, see the LICENSE file in this source tree
 * for details.
 */
//config:config PMAP
//config:	bool "pmap (6.2 kb)"
//config:	default y
//config:	help
//config:	Display processes' memory mappings.

//applet:IF_PMAP(APPLET(pmap, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_PMAP) += pmap.o

//usage:#define pmap_trivial_usage
//usage:       "[-xq] PID..."
//usage:#define pmap_full_usage "\n\n"
//usage:       "Display process memory usage"
//usage:     "\n"
//usage:     "\n	-x	Show details"
//usage:     "\n	-q	Quiet"

#include "libbb.h"

#if ULLONG_MAX == 0xffffffff
# define TABS "\t"
# define SIZEWIDTHx "7"
# define SIZEWIDTH  "9"
# define AFMTLL "8"
# define DASHES ""
#else
# define TABS "\t\t"
# define SIZEWIDTHx "15"
# define SIZEWIDTH  "17"
# define AFMTLL "16"
# define DASHES "--------"
#endif

enum {
	OPT_x = 1 << 0,
	OPT_q = 1 << 1,
};

struct smaprec {
	// For mixed 32/64 userspace, 32-bit pmap still needs
	// 64-bit field here to correctly show 64-bit processes:
	unsigned long long smap_start;
	// Make size wider too:
	// I've seen 1203765248 kb large "---p" mapping in a browser,
	// this cuts close to 4 terabytes.
	unsigned long long smap_size;
	// (strictly speaking, other fields need to be wider too,
	// but they are in kbytes, not bytes, and they hold sizes,
	// not start addresses, sizes tend to be less than 4 terabytes)
	unsigned long private_dirty;
	unsigned long smap_pss, smap_swap;
	char smap_mode[5];
	char *smap_name;
};

// How long the filenames and command lines we want to handle?
#define PMAP_BUFSZ 4096

static void print_smaprec(struct smaprec *currec)
{
	printf("%0" AFMTLL "llx ", currec->smap_start);

	if (option_mask32 & OPT_x)
		printf("%7llu %7lu %7lu %7lu ",
			currec->smap_size,
			currec->smap_pss,
			currec->private_dirty,
			currec->smap_swap);
	else
		printf("%7lluK", currec->smap_size);

	printf(" %.4s  %s\n", currec->smap_mode, currec->smap_name ? : "  [ anon ]");
}

/* libbb's procps_read_smaps() looks somewhat similar,
 * but the collected information is sufficiently different
 * that merging them into one function is not a good idea
 * (unless you feel masochistic today).
 */
static int read_smaps(pid_t pid, char buf[PMAP_BUFSZ], struct smaprec *total)
{
	FILE *file;
	struct smaprec currec;
	char filename[sizeof("/proc/%u/smaps") + sizeof(int)*3];

	sprintf(filename, "/proc/%u/smaps", (int)pid);

	file = fopen_for_read(filename);
	if (!file)
		return 1;

	memset(&currec, 0, sizeof(currec));
	while (fgets(buf, PMAP_BUFSZ, file)) {
		// Each mapping datum has this form:
		// f7d29000-f7d39000 rw-s FILEOFS M:m INODE FILENAME
		// Size:                nnn kB
		// Rss:                 nnn kB
		// .....

		char *tp, *p;

#define bytes4 *(uint32_t*)buf
#define Pss_   PACK32_BYTES('P','s','s',':')
#define Swap   PACK32_BYTES('S','w','a','p')
#define Priv   PACK32_BYTES('P','r','i','v')
#define FETCH(X, N) \
	tp = skip_whitespace(buf+4 + (N)); \
	total->X += currec.X = fast_strtoul_10(&tp); \
	continue
#define SCAN(S, X) \
if (memcmp(buf+4, S, sizeof(S)-1) == 0) { \
	FETCH(X, sizeof(S)-1); \
}
		if (bytes4 == Pss_) {
			FETCH(smap_pss, 0);
		}
		if (bytes4 == Swap && buf[4] == ':') {
			FETCH(smap_swap, 1);
		}
		if (bytes4 == Priv) {
			SCAN("ate_Dirty:", private_dirty);
		}
#undef bytes4
#undef Pss_
#undef Swap
#undef Priv
#undef FETCH
#undef SCAN
		tp = strchr(buf, '-');
		if (tp) {
			// We reached next mapping - the line of this form:
			// f7d29000-f7d39000 rw-s FILEOFS M:m INODE FILENAME

			// If we have a previous record, there's nothing more
			// for it, print and clear currec
			if (currec.smap_size)
				print_smaprec(&currec);
			free(currec.smap_name);
			memset(&currec, 0, sizeof(currec));

			*tp = ' ';
			tp = buf;
			currec.smap_start = fast_strtoull_16(&tp);
			currec.smap_size = (fast_strtoull_16(&tp) - currec.smap_start) >> 10;
			strncpy(currec.smap_mode, tp, sizeof(currec.smap_mode)-1);

			// skipping "rw-s FILEOFS M:m INODE "
			tp = skip_fields(tp, 4);
			tp = skip_whitespace(tp); // there may be many spaces, can't just "tp++"
			p = strchrnul(tp, '\n');
			if (p != tp) {
				currec.smap_name = xstrndup(tp, p - tp);
			}
			total->smap_size += currec.smap_size;
		}
	} // while (got line)
	fclose(file);

	if (currec.smap_size)
		print_smaprec(&currec);
	free(currec.smap_name);

	return 0;
}

static int procps_get_maps(pid_t pid, unsigned opt)
{
	struct smaprec total;
	int ret;
	char buf[PMAP_BUFSZ] ALIGN4;

	ret = read_cmdline(buf, sizeof(buf), pid, NULL);
	if (ret < 0)
		return ret;

	printf("%u: %s\n", (int)pid, buf);

	if (!(opt & OPT_q) && (opt & OPT_x))
		puts("Address" TABS "  Kbytes     PSS   Dirty    Swap  Mode  Mapping");

	memset(&total, 0, sizeof(total));
	ret = read_smaps(pid, buf, &total);
	if (ret)
		return ret;

	if (!(opt & OPT_q)) {
		if (opt & OPT_x)
			printf("--------" DASHES "  ------  ------  ------  ------\n"
				"total kB %"SIZEWIDTHx"llu %7lu %7lu %7lu\n",
				total.smap_size, total.smap_pss, total.private_dirty, total.smap_swap);
		else
			printf(" total %"SIZEWIDTH"lluK\n", total.smap_size);
	}

	return 0;
}

int pmap_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int pmap_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned opts;
	int ret;

	opts = getopt32(argv, "^" "xq" "\0" "-1"); /* min one arg */
	argv += optind;

	ret = 0;
	while (*argv) {
		pid_t pid = xatoi_positive(*argv++);
		/* GNU pmap returns 42 if any of the pids failed */
		if (procps_get_maps(pid, opts) != 0)
			ret = 42;
	}

	return ret;
}
