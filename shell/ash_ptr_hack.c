/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 2008 by Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

/* We cheat here. They are declared as const ptr in ash.c,
 * but here we make them live in R/W memory */
struct globals_misc;
struct globals_memstack;
struct globals_var;

struct globals_misc     *ash_ptr_to_globals_misc;
struct globals_memstack *ash_ptr_to_globals_memstack;
struct globals_var      *ash_ptr_to_globals_var;
