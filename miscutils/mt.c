/* vi: set sw=4 ts=4: */
#include "internal.h"
#include <stdio.h>
#include <sys/mtio.h>
#include <sys/fcntl.h>

static const char mt_usage[] = "mt [-f device] opcode value\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
			"\nControl magnetic tape drive operation\n"
#endif
			;

struct mt_opcodes {
	char *name;
	short value;
};

/* missing: eod/seod, stoptions, stwrthreshold, densities */
static const struct mt_opcodes opcodes[] = {
	{"bsf", MTBSF},
	{"bsfm", MTBSFM},
	{"bsr", MTBSR},
	{"bss", MTBSS},
	{"datacompression", MTCOMPRESSION},
	{"eom", MTEOM},
	{"erase", MTERASE},
	{"fsf", MTFSF},
	{"fsfm", MTFSFM},
	{"fsr", MTFSR},
	{"fss", MTFSS},
	{"load", MTLOAD},
	{"lock", MTLOCK},
	{"mkpart", MTMKPART},
	{"nop", MTNOP},
	{"offline", MTOFFL},
	{"rewoffline", MTOFFL},
	{"ras1", MTRAS1},
	{"ras2", MTRAS2},
	{"ras3", MTRAS3},
	{"reset", MTRESET},
	{"retension", MTRETEN},
	{"rew", MTREW},
	{"seek", MTSEEK},
	{"setblk", MTSETBLK},
	{"setdensity", MTSETDENSITY},
	{"drvbuffer", MTSETDRVBUFFER},
	{"setpart", MTSETPART},
	{"tell", MTTELL},
	{"wset", MTWSM},
	{"unload", MTUNLOAD},
	{"unlock", MTUNLOCK},
	{"eof", MTWEOF},
	{"weof", MTWEOF},
	{0, 0}
};

extern int mt_main(int argc, char **argv)
{
	const char *file = "/dev/tape";
	const struct mt_opcodes *code = opcodes;
	struct mtop op;
	int fd;
	
	if ((argc != 2 && argc != 3) && **(argv + 1) != '-') {
		usage(mt_usage);
	}

	if (strcmp(argv[1], "-f") == 0) {
		if (argc < 4) {
			usage(mt_usage);
		}
		file = argv[2];
		argv += 2;
		argc -= 2;
	}

	while (code->name != 0) {
		if (strcmp(code->name, argv[1]) == 0)
			break;
		code++;
	}

	if (code->name == 0) {
		fprintf(stderr, "mt: unrecognized opcode %s.\n", argv[1]);
		exit (FALSE);
	}

	op.mt_op = code->value;
	if (argc >= 3)
		op.mt_count = atoi(argv[2]);
	else
		op.mt_count = 1;		/* One, not zero, right? */

	if ((fd = open(file, O_RDONLY, 0)) < 0) {
		perror(file);
		exit (FALSE);
	}

	if (ioctl(fd, MTIOCTOP, &op) != 0) {
		perror(file);
		exit (FALSE);
	}

	exit (TRUE);
}
