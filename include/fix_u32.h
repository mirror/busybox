/* vi: set sw=4 ts=4: */
/*
 * This header makes it easier to include kernel headers
 * which use u32 and such.
 *
 * Licensed under the GPL version 2, see the file LICENSE in this tarball.
 */
#ifndef FIX_U32_H
#define FIX_U32_H 1

#undef u64
#undef u32
#undef u16
#undef u8
#undef s64
#undef s32
#undef s16
#undef s8

#define u64 bb_hack_u64
#define u32 bb_hack_u32
#define u16 bb_hack_u16
#define u8  bb_hack_u8
#define s64 bb_hack_s64
#define s32 bb_hack_s32
#define s16 bb_hack_s16
#define s8  bb_hack_s8

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;

#endif
