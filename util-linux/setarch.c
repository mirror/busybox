/* vi: set sw=4 ts=4: */
/*
 * Linux32/linux64 allows for changing uname emulation.
 *
 * Copyright 2002 Andi Kleen, SuSE Labs.
 *
 * Licensed under GPL v2 or later, see file License for details.
*/

#include <sys/personality.h>

#include "libbb.h"

int setarch_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int setarch_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	int pers = -1;

	/* Figure out what personality we are supposed to switch to ...
	 * we can be invoked as either:
	 * argv[0],argv[1] -> "setarch","personality"
	 * argv[0]         -> "personality"
	 */
retry:
	if (argv[0][5] == '6') /* linux64 */
		pers = PER_LINUX;
	else if (argv[0][5] == '3') /* linux32 */
		pers = PER_LINUX32;
	else if (pers == -1 && argv[1] != NULL) {
		pers = PER_LINUX32;
		++argv;
		goto retry;
	}

	/* make user actually gave us something to do */
	++argv;
	if (argv[0] == NULL)
		bb_show_usage();

	/* Try to set personality */
	if (personality(pers) >= 0) {

		/* Try to execute the program */
		BB_EXECVP(argv[0], argv);
	}

	bb_simple_perror_msg_and_die(argv[0]);
}
