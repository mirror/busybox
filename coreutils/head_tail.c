/*
 * Copyright (C) 2013 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#include "libbb.h"
#include "head_tail.h"

const struct suffix_mult head_tail_suffixes[] = {
	{ "b", 512 },
	{ "k", 1024 },
	{ "m", 1024*1024 },
	{ "", 0 }
};
