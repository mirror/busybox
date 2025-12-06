/*
 * The compilation unit for yescrypt-related code.
 *
 * Copyright (C) 2025 by Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//kbuild:lib-$(CONFIG_USE_BB_CRYPT_YES) += y.o

#include "libbb.h"

#define YESCRYPT_INTERNAL
#include "alg-yescrypt.h"
#include "alg-sha256.c"
#include "alg-yescrypt-kdf.c"
#include "alg-yescrypt-common.c"
