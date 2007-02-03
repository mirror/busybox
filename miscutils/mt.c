/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"
#include <sys/mtio.h>

struct mt_opcodes {
	const char *name;
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
	{"rewind", MTREW},
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

int mt_main(int argc, char **argv);
int mt_main(int argc, char **argv)
{
	const char *file = "/dev/tape";
	const struct mt_opcodes *code = opcodes;
	struct mtop op;
	struct mtpos position;
	int fd, mode;

	if (argc < 2) {
		bb_show_usage();
	}

	if (strcmp(argv[1], "-f") == 0) {
		if (argc < 4) {
			bb_show_usage();
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
		bb_error_msg("unrecognized opcode %s", argv[1]);
		return EXIT_FAILURE;
	}

	op.mt_op = code->value;
	if (argc >= 3)
		op.mt_count = xatoi_u(argv[2]);
	else
		op.mt_count = 1;		/* One, not zero, right? */

	switch (code->value) {
		case MTWEOF:
		case MTERASE:
		case MTWSM:
		case MTSETDRVBUFFER:
			mode = O_WRONLY;
			break;

		default:
			mode = O_RDONLY;
			break;
	}

	fd = xopen(file, mode);

	switch (code->value) {
		case MTTELL:
			if (ioctl(fd, MTIOCPOS, &position) < 0)
				bb_perror_msg_and_die("%s", file);
			printf("At block %d.\n", (int) position.mt_blkno);
			break;

		default:
			if (ioctl(fd, MTIOCTOP, &op) != 0)
				bb_perror_msg_and_die("%s", file);
			break;
	}

	return EXIT_SUCCESS;
}
