/*
 * bitops.h --- Bitmap frobbing code.  The byte swapping routines are
 * 	also included here.
 * 
 * Copyright (C) 1993, 1994, 1995, 1996 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 * 
 * i386 bitops operations taken from <asm/bitops.h>, Copyright 1992,
 * Linus Torvalds.
 */

#include <strings.h>

extern int ext2fs_set_bit(int nr,void * addr);
extern int ext2fs_clear_bit(int nr, void * addr);
extern int ext2fs_test_bit(int nr, const void * addr);
extern __u16 ext2fs_swab16(__u16 val);
extern __u32 ext2fs_swab32(__u32 val);

#ifdef WORDS_BIGENDIAN
#define ext2fs_cpu_to_le32(x) ext2fs_swab32((x))
#define ext2fs_le32_to_cpu(x) ext2fs_swab32((x))
#define ext2fs_cpu_to_le16(x) ext2fs_swab16((x))
#define ext2fs_le16_to_cpu(x) ext2fs_swab16((x))
#define ext2fs_cpu_to_be32(x) ((__u32)(x))
#define ext2fs_be32_to_cpu(x) ((__u32)(x))
#define ext2fs_cpu_to_be16(x) ((__u16)(x))
#define ext2fs_be16_to_cpu(x) ((__u16)(x))
#else
#define ext2fs_cpu_to_le32(x) ((__u32)(x))
#define ext2fs_le32_to_cpu(x) ((__u32)(x))
#define ext2fs_cpu_to_le16(x) ((__u16)(x))
#define ext2fs_le16_to_cpu(x) ((__u16)(x))
#define ext2fs_cpu_to_be32(x) ext2fs_swab32((x))
#define ext2fs_be32_to_cpu(x) ext2fs_swab32((x))
#define ext2fs_cpu_to_be16(x) ext2fs_swab16((x))
#define ext2fs_be16_to_cpu(x) ext2fs_swab16((x))
#endif

/*
 * EXT2FS bitmap manipulation routines.
 */

/* Support for sending warning messages from the inline subroutines */
extern const char *ext2fs_block_string;
extern const char *ext2fs_inode_string;
extern const char *ext2fs_mark_string;
extern const char *ext2fs_unmark_string;
extern const char *ext2fs_test_string;
extern void ext2fs_warn_bitmap(errcode_t errcode, unsigned long arg,
			       const char *description);
extern void ext2fs_warn_bitmap2(ext2fs_generic_bitmap bitmap,
				int code, unsigned long arg);

extern int ext2fs_mark_block_bitmap(ext2fs_block_bitmap bitmap, blk_t block);
extern int ext2fs_unmark_block_bitmap(ext2fs_block_bitmap bitmap,
				       blk_t block);
extern int ext2fs_test_block_bitmap(ext2fs_block_bitmap bitmap, blk_t block);

extern int ext2fs_mark_inode_bitmap(ext2fs_inode_bitmap bitmap, ext2_ino_t inode);
extern int ext2fs_unmark_inode_bitmap(ext2fs_inode_bitmap bitmap,
				       ext2_ino_t inode);
extern int ext2fs_test_inode_bitmap(ext2fs_inode_bitmap bitmap, ext2_ino_t inode);

extern void ext2fs_fast_mark_block_bitmap(ext2fs_block_bitmap bitmap,
					  blk_t block);
extern void ext2fs_fast_unmark_block_bitmap(ext2fs_block_bitmap bitmap,
					    blk_t block);
extern int ext2fs_fast_test_block_bitmap(ext2fs_block_bitmap bitmap,
					 blk_t block);

extern void ext2fs_fast_mark_inode_bitmap(ext2fs_inode_bitmap bitmap,
					  ext2_ino_t inode);
extern void ext2fs_fast_unmark_inode_bitmap(ext2fs_inode_bitmap bitmap,
					    ext2_ino_t inode);
extern int ext2fs_fast_test_inode_bitmap(ext2fs_inode_bitmap bitmap,
					 ext2_ino_t inode);
extern blk_t ext2fs_get_block_bitmap_start(ext2fs_block_bitmap bitmap);
extern ext2_ino_t ext2fs_get_inode_bitmap_start(ext2fs_inode_bitmap bitmap);
extern blk_t ext2fs_get_block_bitmap_end(ext2fs_block_bitmap bitmap);
extern ext2_ino_t ext2fs_get_inode_bitmap_end(ext2fs_inode_bitmap bitmap);

extern void ext2fs_mark_block_bitmap_range(ext2fs_block_bitmap bitmap,
					   blk_t block, int num);
extern void ext2fs_unmark_block_bitmap_range(ext2fs_block_bitmap bitmap,
					     blk_t block, int num);
extern int ext2fs_test_block_bitmap_range(ext2fs_block_bitmap bitmap,
					  blk_t block, int num);
extern void ext2fs_fast_mark_block_bitmap_range(ext2fs_block_bitmap bitmap,
						blk_t block, int num);
extern void ext2fs_fast_unmark_block_bitmap_range(ext2fs_block_bitmap bitmap,
						  blk_t block, int num);
extern int ext2fs_fast_test_block_bitmap_range(ext2fs_block_bitmap bitmap,
					       blk_t block, int num);
extern void ext2fs_set_bitmap_padding(ext2fs_generic_bitmap map);

/* These two routines moved to gen_bitmap.c */
extern int ext2fs_mark_generic_bitmap(ext2fs_generic_bitmap bitmap,
					 __u32 bitno);
extern int ext2fs_unmark_generic_bitmap(ext2fs_generic_bitmap bitmap,
					   blk_t bitno);
/*
 * The inline routines themselves...
 * 
 * If NO_INLINE_FUNCS is defined, then we won't try to do inline
 * functions at all; they will be included as normal functions in
 * inline.c
 */
#ifdef NO_INLINE_FUNCS
#if (defined(__GNUC__) && (defined(__i386__) || defined(__i486__) || \
			   defined(__i586__) || defined(__mc68000__) || \
			   defined(__sparc__)))
	/* This prevents bitops.c from trying to include the C */
	/* function version of these functions */
#define _EXT2_HAVE_ASM_BITOPS_
#endif
#endif /* NO_INLINE_FUNCS */

#if (defined(INCLUDE_INLINE_FUNCS) || !defined(NO_INLINE_FUNCS))
#ifdef INCLUDE_INLINE_FUNCS
#define _INLINE_ extern
#else
#ifdef __GNUC__
#define _INLINE_ extern __inline__
#else				/* For Watcom C */
#define _INLINE_ extern inline
#endif
#endif

#if ((defined __GNUC__) && !defined(_EXT2_USE_C_VERSIONS_) && \
     (defined(__i386__) || defined(__i486__) || defined(__i586__)))

#define _EXT2_HAVE_ASM_BITOPS_
#define _EXT2_HAVE_ASM_SWAB_
#define _EXT2_HAVE_ASM_FINDBIT_

/*
 * These are done by inline assembly for speed reasons.....
 *
 * All bitoperations return 0 if the bit was cleared before the
 * operation and != 0 if it was not.  Bit 0 is the LSB of addr; bit 32
 * is the LSB of (addr+1).
 */

/*
 * Some hacks to defeat gcc over-optimizations..
 */
struct __dummy_h { unsigned long a[100]; };
#define EXT2FS_ADDR (*(struct __dummy_h *) addr)
#define EXT2FS_CONST_ADDR (*(const struct __dummy_h *) addr)	

_INLINE_ int ext2fs_set_bit(int nr, void * addr)
{
	int oldbit;

	__asm__ __volatile__("btsl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"=m" (EXT2FS_ADDR)
		:"r" (nr));
	return oldbit;
}

_INLINE_ int ext2fs_clear_bit(int nr, void * addr)
{
	int oldbit;

	__asm__ __volatile__("btrl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"=m" (EXT2FS_ADDR)
		:"r" (nr));
	return oldbit;
}

_INLINE_ int ext2fs_test_bit(int nr, const void * addr)
{
	int oldbit;

	__asm__ __volatile__("btl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit)
		:"m" (EXT2FS_CONST_ADDR),"r" (nr));
	return oldbit;
}

#if 0
_INLINE_ int ext2fs_find_first_bit_set(void * addr, unsigned size)
{
	int d0, d1, d2;
	int res;

	if (!size)
		return 0;
	/* This looks at memory. Mark it volatile to tell gcc not to move it around */
	__asm__ __volatile__(
		"cld\n\t"			     
		"xorl %%eax,%%eax\n\t"
		"xorl %%edx,%%edx\n\t"
		"repe; scasl\n\t"
		"je 1f\n\t"
		"movl -4(%%edi),%%eax\n\t"
		"subl $4,%%edi\n\t"
		"bsfl %%eax,%%edx\n"
		"1:\tsubl %%esi,%%edi\n\t"
		"shll $3,%%edi\n\t"
		"addl %%edi,%%edx"
		:"=d" (res), "=&c" (d0), "=&D" (d1), "=&a" (d2)
		:"1" ((size + 31) >> 5), "2" (addr), "S" (addr));
	return res;
}

_INLINE_ int ext2fs_find_next_bit_set (void * addr, int size, int offset)
{
	unsigned long * p = ((unsigned long *) addr) + (offset >> 5);
	int set = 0, bit = offset & 31, res;
	
	if (bit) {
		/*
		 * Look for zero in first byte
		 */
		__asm__("bsfl %1,%0\n\t"
			"jne 1f\n\t"
			"movl $32, %0\n"
			"1:"
			: "=r" (set)
			: "r" (*p >> bit));
		if (set < (32 - bit))
			return set + offset;
		set = 32 - bit;
		p++;
	}
	/*
	 * No bit found yet, search remaining full bytes for a bit
	 */
	res = ext2fs_find_first_bit_set(p, size - 32 * (p - (unsigned long *) addr));
	return (offset + set + res);
}
#endif

#ifdef EXT2FS_ENABLE_SWAPFS
_INLINE_ __u32 ext2fs_swab32(__u32 val)
{
#ifdef EXT2FS_REQUIRE_486
	__asm__("bswap %0" : "=r" (val) : "0" (val));
#else
	__asm__("xchgb %b0,%h0\n\t"	/* swap lower bytes	*/
		"rorl $16,%0\n\t"	/* swap words		*/
		"xchgb %b0,%h0"		/* swap higher bytes	*/
		:"=q" (val)
		: "0" (val));
#endif
	return val;
}

_INLINE_ __u16 ext2fs_swab16(__u16 val)
{
	__asm__("xchgb %b0,%h0"		/* swap bytes		*/ \
		: "=q" (val) \
		:  "0" (val)); \
		return val;
}
#endif

#undef EXT2FS_ADDR

#endif	/* i386 */

#ifdef __mc68000__

#define _EXT2_HAVE_ASM_BITOPS_

_INLINE_ int ext2fs_set_bit(int nr,void * addr)
{
	char retval;

	__asm__ __volatile__ ("bfset %2@{%1:#1}; sne %0"
	     : "=d" (retval) : "d" (nr^7), "a" (addr));

	return retval;
}

_INLINE_ int ext2fs_clear_bit(int nr, void * addr)
{
	char retval;

	__asm__ __volatile__ ("bfclr %2@{%1:#1}; sne %0"
	     : "=d" (retval) : "d" (nr^7), "a" (addr));

	return retval;
}

_INLINE_ int ext2fs_test_bit(int nr, const void * addr)
{
	char retval;

	__asm__ __volatile__ ("bftst %2@{%1:#1}; sne %0"
	     : "=d" (retval) : "d" (nr^7), "a" (addr));

	return retval;
}

#endif /* __mc68000__ */


#if !defined(_EXT2_HAVE_ASM_SWAB_) && defined(EXT2FS_ENABLE_SWAPFS)

_INLINE_ __u16 ext2fs_swab16(__u16 val)
{
	return (val >> 8) | (val << 8);
}

_INLINE_ __u32 ext2fs_swab32(__u32 val)
{
	return ((val>>24) | ((val>>8)&0xFF00) |
		((val<<8)&0xFF0000) | (val<<24));
}

#endif /* !_EXT2_HAVE_ASM_SWAB */

#if !defined(_EXT2_HAVE_ASM_FINDBIT_)
_INLINE_ int ext2fs_find_first_bit_set(void * addr, unsigned size)
{
	char	*cp = (unsigned char *) addr;
	int 	res = 0, d0;

	if (!size)
		return 0;

	while ((size > res) && (*cp == 0)) {
		cp++;
		res += 8;
	}
	d0 = ffs(*cp);
	if (d0 == 0)
		return size;
	
	return res + d0 - 1;
}

_INLINE_ int ext2fs_find_next_bit_set (void * addr, int size, int offset)
{
	unsigned char * p;
	int set = 0, bit = offset & 7, res = 0, d0;
	
	res = offset >> 3;
	p = ((unsigned char *) addr) + res;
	
	if (bit) {
		set = ffs(*p & ~((1 << bit) - 1));
		if (set)
			return (offset & ~7) + set - 1;
		p++;
		res += 8;
	}
	while ((size > res) && (*p == 0)) {
		p++;
		res += 8;
	}
	d0 = ffs(*p);
	if (d0 == 0)
		return size;

	return (res + d0 - 1);
}
#endif	

_INLINE_ int ext2fs_test_generic_bitmap(ext2fs_generic_bitmap bitmap,
					blk_t bitno);

_INLINE_ int ext2fs_test_generic_bitmap(ext2fs_generic_bitmap bitmap,
					blk_t bitno)
{
	if ((bitno < bitmap->start) || (bitno > bitmap->end)) {
		ext2fs_warn_bitmap2(bitmap, EXT2FS_TEST_ERROR, bitno);
		return 0;
	}
	return ext2fs_test_bit(bitno - bitmap->start, bitmap->bitmap);
}

_INLINE_ int ext2fs_mark_block_bitmap(ext2fs_block_bitmap bitmap,
				       blk_t block)
{
	return ext2fs_mark_generic_bitmap((ext2fs_generic_bitmap)
				       bitmap,
					  block);
}

_INLINE_ int ext2fs_unmark_block_bitmap(ext2fs_block_bitmap bitmap,
					 blk_t block)
{
	return ext2fs_unmark_generic_bitmap((ext2fs_generic_bitmap) bitmap, 
					    block);
}

_INLINE_ int ext2fs_test_block_bitmap(ext2fs_block_bitmap bitmap,
				       blk_t block)
{
	return ext2fs_test_generic_bitmap((ext2fs_generic_bitmap) bitmap, 
					  block);
}

_INLINE_ int ext2fs_mark_inode_bitmap(ext2fs_inode_bitmap bitmap,
				       ext2_ino_t inode)
{
	return ext2fs_mark_generic_bitmap((ext2fs_generic_bitmap) bitmap, 
					  inode);
}

_INLINE_ int ext2fs_unmark_inode_bitmap(ext2fs_inode_bitmap bitmap,
					 ext2_ino_t inode)
{
	return ext2fs_unmark_generic_bitmap((ext2fs_generic_bitmap) bitmap, 
				     inode);
}

_INLINE_ int ext2fs_test_inode_bitmap(ext2fs_inode_bitmap bitmap,
				       ext2_ino_t inode)
{
	return ext2fs_test_generic_bitmap((ext2fs_generic_bitmap) bitmap, 
					  inode);
}

_INLINE_ void ext2fs_fast_mark_block_bitmap(ext2fs_block_bitmap bitmap,
					    blk_t block)
{
#ifdef EXT2FS_DEBUG_FAST_OPS
	if ((block < bitmap->start) || (block > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_BLOCK_MARK, block,
				   bitmap->description);
		return;
	}
#endif	
	ext2fs_set_bit(block - bitmap->start, bitmap->bitmap);
}

_INLINE_ void ext2fs_fast_unmark_block_bitmap(ext2fs_block_bitmap bitmap,
					      blk_t block)
{
#ifdef EXT2FS_DEBUG_FAST_OPS
	if ((block < bitmap->start) || (block > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_BLOCK_UNMARK,
				   block, bitmap->description);
		return;
	}
#endif
	ext2fs_clear_bit(block - bitmap->start, bitmap->bitmap);
}

_INLINE_ int ext2fs_fast_test_block_bitmap(ext2fs_block_bitmap bitmap,
					    blk_t block)
{
#ifdef EXT2FS_DEBUG_FAST_OPS
	if ((block < bitmap->start) || (block > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_BLOCK_TEST,
				   block, bitmap->description);
		return 0;
	}
#endif
	return ext2fs_test_bit(block - bitmap->start, bitmap->bitmap);
}

_INLINE_ void ext2fs_fast_mark_inode_bitmap(ext2fs_inode_bitmap bitmap,
					    ext2_ino_t inode)
{
#ifdef EXT2FS_DEBUG_FAST_OPS
	if ((inode < bitmap->start) || (inode > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_INODE_MARK,
				   inode, bitmap->description);
		return;
	}
#endif
	ext2fs_set_bit(inode - bitmap->start, bitmap->bitmap);
}

_INLINE_ void ext2fs_fast_unmark_inode_bitmap(ext2fs_inode_bitmap bitmap,
					      ext2_ino_t inode)
{
#ifdef EXT2FS_DEBUG_FAST_OPS
	if ((inode < bitmap->start) || (inode > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_INODE_UNMARK,
				   inode, bitmap->description);
		return;
	}
#endif
	ext2fs_clear_bit(inode - bitmap->start, bitmap->bitmap);
}

_INLINE_ int ext2fs_fast_test_inode_bitmap(ext2fs_inode_bitmap bitmap,
					   ext2_ino_t inode)
{
#ifdef EXT2FS_DEBUG_FAST_OPS
	if ((inode < bitmap->start) || (inode > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_INODE_TEST,
				   inode, bitmap->description);
		return 0;
	}
#endif
	return ext2fs_test_bit(inode - bitmap->start, bitmap->bitmap);
}

_INLINE_ blk_t ext2fs_get_block_bitmap_start(ext2fs_block_bitmap bitmap)
{
	return bitmap->start;
}

_INLINE_ ext2_ino_t ext2fs_get_inode_bitmap_start(ext2fs_inode_bitmap bitmap)
{
	return bitmap->start;
}

_INLINE_ blk_t ext2fs_get_block_bitmap_end(ext2fs_block_bitmap bitmap)
{
	return bitmap->end;
}

_INLINE_ ext2_ino_t ext2fs_get_inode_bitmap_end(ext2fs_inode_bitmap bitmap)
{
	return bitmap->end;
}

_INLINE_ int ext2fs_test_block_bitmap_range(ext2fs_block_bitmap bitmap,
					    blk_t block, int num)
{
	int	i;

	if ((block < bitmap->start) || (block+num-1 > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_BLOCK_TEST,
				   block, bitmap->description);
		return 0;
	}
	for (i=0; i < num; i++) {
		if (ext2fs_fast_test_block_bitmap(bitmap, block+i))
			return 0;
	}
	return 1;
}

_INLINE_ int ext2fs_fast_test_block_bitmap_range(ext2fs_block_bitmap bitmap,
						 blk_t block, int num)
{
	int	i;

#ifdef EXT2FS_DEBUG_FAST_OPS
	if ((block < bitmap->start) || (block+num-1 > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_BLOCK_TEST,
				   block, bitmap->description);
		return 0;
	}
#endif
	for (i=0; i < num; i++) {
		if (ext2fs_fast_test_block_bitmap(bitmap, block+i))
			return 0;
	}
	return 1;
}

_INLINE_ void ext2fs_mark_block_bitmap_range(ext2fs_block_bitmap bitmap,
					     blk_t block, int num)
{
	int	i;
	
	if ((block < bitmap->start) || (block+num-1 > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_BLOCK_MARK, block,
				   bitmap->description);
		return;
	}
	for (i=0; i < num; i++)
		ext2fs_set_bit(block + i - bitmap->start, bitmap->bitmap);
}

_INLINE_ void ext2fs_fast_mark_block_bitmap_range(ext2fs_block_bitmap bitmap,
						  blk_t block, int num)
{
	int	i;
	
#ifdef EXT2FS_DEBUG_FAST_OPS
	if ((block < bitmap->start) || (block+num-1 > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_BLOCK_MARK, block,
				   bitmap->description);
		return;
	}
#endif	
	for (i=0; i < num; i++)
		ext2fs_set_bit(block + i - bitmap->start, bitmap->bitmap);
}

_INLINE_ void ext2fs_unmark_block_bitmap_range(ext2fs_block_bitmap bitmap,
					       blk_t block, int num)
{
	int	i;
	
	if ((block < bitmap->start) || (block+num-1 > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_BLOCK_UNMARK, block,
				   bitmap->description);
		return;
	}
	for (i=0; i < num; i++)
		ext2fs_clear_bit(block + i - bitmap->start, bitmap->bitmap);
}

_INLINE_ void ext2fs_fast_unmark_block_bitmap_range(ext2fs_block_bitmap bitmap,
						    blk_t block, int num)
{
	int	i;
	
#ifdef EXT2FS_DEBUG_FAST_OPS
	if ((block < bitmap->start) || (block+num-1 > bitmap->end)) {
		ext2fs_warn_bitmap(EXT2_ET_BAD_BLOCK_UNMARK, block,
				   bitmap->description);
		return;
	}
#endif	
	for (i=0; i < num; i++)
		ext2fs_clear_bit(block + i - bitmap->start, bitmap->bitmap);
}
#undef _INLINE_
#endif

