/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"
#include "bb_archive.h"

int FAST_FUNC unsafe_symlink_target(const char *target)
{
	const char *dot;

	if (target[0] == '/') {
		const char *var;
 unsafe:
		var = getenv("EXTRACT_UNSAFE_SYMLINKS");
		if (var) {
			if (LONE_CHAR(var, '1'))
				return 0; /* pretend it's safe */
			return 1; /* "UNSAFE!" */
		}
		bb_error_msg("skipping unsafe symlink to '%s' in archive,"
			" set %s=1 to extract",
			target,
			"EXTRACT_UNSAFE_SYMLINKS"
		);
		/* Prevent further messages */
		setenv("EXTRACT_UNSAFE_SYMLINKS", "0", 0);
		return 1; /* "UNSAFE!" */
	}

	dot = target;
	for (;;) {
		dot = strchr(dot, '.');
		if (!dot)
			return 0; /* safe target */

		/* Is it a path component starting with ".."? */
		if ((dot[1] == '.')
		 && (dot == target || dot[-1] == '/')
		    /* Is it exactly ".."? */
		 && (dot[2] == '/' || dot[2] == '\0')
		) {
			goto unsafe;
		}
		/* NB: it can even be trailing ".", should only add 1 */
		dot += 1;
	}
}
