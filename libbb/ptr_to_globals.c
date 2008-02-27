/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 2008 by Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

/* We cheat here. It is declared as const ptr in libbb.h,
 * but here we make it live in R/W memory */
struct globals;
struct globals *ptr_to_globals;
