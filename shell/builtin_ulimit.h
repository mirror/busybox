/* vi: set sw=4 ts=4: */
/*
 * Adapted from ash applet code
 *
 * Copyright (c) 2010 Tobias Klauser
 * Split from ash.c and slightly adapted.
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */
#ifndef SHELL_BUILTIN_ULIMIT_H
#define SHELL_BUILTIN_ULIMIT_H 1

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

int FAST_FUNC shell_builtin_ulimit(char **argv);

POP_SAVED_FUNCTION_VISIBILITY

#endif
