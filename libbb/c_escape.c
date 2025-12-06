/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 2025 by Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//kbuild:lib-y += c_escape.o

#include "libbb.h"

const char c_escape_conv_str00[] ALIGN1 =
	"\\""0""\0" // [0]:00
	"\\""a""\0" // [1]:07
	"\\""b""\0" // [2]:08
	"\\""t""\0" // [3]:09
	"\\""n""\0" // [4]:0a
	"\\""v""\0" // [5]:0b
	"\\""f""\0" // [6]:0c
	"\\""r"     // [7]:0d
	;
