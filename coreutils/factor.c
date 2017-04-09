/*
 * Copyright (C) 2017 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config FACTOR
//config:	bool "factor"
//config:	default y
//config:	help
//config:	  factor factorizes integers

//applet:IF_FACTOR(APPLET(factor, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_FACTOR) += factor.o

//usage:#define factor_trivial_usage
//usage:       "NUMBER..."
//usage:#define factor_full_usage "\n\n"
//usage:       "Print prime factors"

#include "libbb.h"

#if ULLONG_MAX == (UINT_MAX * UINT_MAX + 2 * UINT_MAX)
/* "unsigned" is half as wide as ullong */
typedef unsigned half_t;
#define HALF_FMT ""
#elif ULLONG_MAX == (ULONG_MAX * ULONG_MAX + 2 * ULONG_MAX)
/* long is half as wide as ullong */
typedef unsigned long half_t;
#define HALF_FMT "l"
#else
#error Cant find an integer type which is half as wide as ullong
#endif

static void factorize(const char *numstr)
{
	unsigned long long N, factor2;
	half_t factor;
	unsigned count3;

	/* Coreutils compat */
	numstr = skip_whitespace(numstr);
	if (*numstr == '+')
		numstr++;

	N = bb_strtoull(numstr, NULL, 10);
	if (errno)
		bb_show_usage();

	printf("%llu:", N);

	if (N < 4)
		goto end;
	while (!(N & 1)) {
		printf(" 2");
		N >>= 1;
	}

	count3 = 3;
	factor = 3;
	factor2 = 3 * 3;
	for (;;) {
		unsigned long long nfactor2;

		while ((N % factor) == 0) {
			N = N / factor;
			printf(" %u"HALF_FMT"", factor);
		}
 next_factor:
		/* (f + 2)^2 = f^2 + 4*f + 4 = f^2 + 4*(f+1) */
		nfactor2 = factor2 + 4 * (factor + 1);
		if (nfactor2 < factor2) /* overflow? */
			break;
		factor2 = nfactor2;
		if (factor2 > N)
			break;
		factor += 2;
		/* Rudimentary wheel sieving: skip multiples of 3:
		 * Every third odd number is divisible by three and thus isn't a prime:
		 * 5  7  9  11  13  15  17 19 21 23 25 27 29 31 33 35 37...
		 * ^  ^     ^   ^       ^  ^     ^  _     ^  ^     _  ^ (^primes)
		 */
		--count3;
		if (count3 == 0) {
			count3 = 3;
			goto next_factor;
		}
	}
 end:
	if (N > 1)
		printf(" %llu", N);
	bb_putchar('\n');
}

int factor_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int factor_main(int argc UNUSED_PARAM, char **argv)
{
	//// coreutils has undocumented option ---debug (three dashes)
	//getopt32(argv, "");
	//argv += optind;
	argv++;

	if (!*argv)
		//TODO: read from stdin
		bb_show_usage();

	do {
		factorize(*argv);
	} while (*++argv);

	fflush_stdout_and_exit(EXIT_SUCCESS);
}
