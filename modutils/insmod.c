/* vi: set sw=4 ts=4: */
/*
 * Mini insmod implementation for busybox
 *
 * This version of insmod supports x86, ARM, SH3/4/5, powerpc, m68k,
 * MIPS, and v850e.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * and Ron Alder <alder@lineo.com>
 *
 * Miles Bader <miles@gnu.org> added NEC V850E support.
 *
 * Modified by Bryan Rittmeyer <bryan@ixiacom.com> to support SH4
 * and (theoretically) SH3. I have only tested SH4 in little endian mode.
 *
 * Modified by Alcove, Julien Gaulmin <julien.gaulmin@alcove.fr> and
 * Nicolas Ferre <nicolas.ferre@alcove.fr> to support ARM7TDMI.  Only
 * very minor changes required to also work with StrongArm and presumably
 * all ARM based systems.
 *
 * Paul Mundt <lethal@linux-sh.org> 08-Aug-2003.
 *   Integrated support for sh64 (SH-5), from preliminary modutils
 *   patches from Benedict Gaster <benedict.gaster@superh.com>.
 *   Currently limited to support for 32bit ABI.
 *
 * Magnus Damm <damm@opensource.se> 22-May-2002.
 *   The plt and got code are now using the same structs.
 *   Added generic linked list code to fully support PowerPC.
 *   Replaced the mess in arch_apply_relocation() with architecture blocks.
 *   The arch_create_got() function got cleaned up with architecture blocks.
 *   These blocks should be easy maintain and sync with obj_xxx.c in modutils.
 *
 * Magnus Damm <damm@opensource.se> added PowerPC support 20-Feb-2001.
 *   PowerPC specific code stolen from modutils-2.3.16,
 *   written by Paul Mackerras, Copyright 1996, 1997 Linux International.
 *   I've only tested the code on mpc8xx platforms in big-endian mode.
 *   Did some cleanup and added CONFIG_USE_xxx_ENTRIES...
 *
 * Quinn Jensen <jensenq@lineo.com> added MIPS support 23-Feb-2001.
 *   based on modutils-2.4.2
 *   MIPS specific support for Elf loading and relocation.
 *   Copyright 1996, 1997 Linux International.
 *   Contributed by Ralf Baechle <ralf@gnu.ai.mit.edu>
 *
 * Based almost entirely on the Linux modutils-2.3.11 implementation.
 *   Copyright 1996, 1997 Linux International.
 *   New implementation contributed by Richard Henderson <rth@tamu.edu>
 *   Based on original work by Bjorn Ekwall <bj0rn@blox.se>
 *   Restructured (and partly rewritten) by:
 *   Björn Ekwall <bj0rn@blox.se> February 1999
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include "busybox.h"

#if !defined(CONFIG_FEATURE_2_4_MODULES) && \
	!defined(CONFIG_FEATURE_2_2_MODULES) && \
	!defined(CONFIG_FEATURE_2_6_MODULES)
#define CONFIG_FEATURE_2_4_MODULES
#endif

#if !defined(CONFIG_FEATURE_2_4_MODULES) && !defined(CONFIG_FEATURE_2_2_MODULES)
#define insmod_ng_main insmod_main
#endif

#if defined(CONFIG_FEATURE_2_4_MODULES) || defined(CONFIG_FEATURE_2_2_MODULES)

#if defined(CONFIG_FEATURE_2_6_MODULES)
extern int insmod_ng_main( int argc, char **argv);
#endif

#ifdef CONFIG_FEATURE_2_4_MODULES
# undef CONFIG_FEATURE_2_2_MODULES
# define new_sys_init_module	init_module
#else
# define old_sys_init_module	init_module
#endif

#ifdef CONFIG_FEATURE_INSMOD_LOADINKMEM
#define LOADBITS 0
#else
#define LOADBITS 1
#endif


#if defined(__arm__)
#define CONFIG_USE_PLT_ENTRIES
#define CONFIG_PLT_ENTRY_SIZE 8
#define CONFIG_USE_GOT_ENTRIES
#define CONFIG_GOT_ENTRY_SIZE 8
#define CONFIG_USE_SINGLE

#define MATCH_MACHINE(x) (x == EM_ARM)
#define SHT_RELM	SHT_REL
#define Elf32_RelM	Elf32_Rel
#define ELFCLASSM	ELFCLASS32
#endif

#if defined(__s390__)
#define CONFIG_USE_PLT_ENTRIES
#define CONFIG_PLT_ENTRY_SIZE 8
#define CONFIG_USE_GOT_ENTRIES
#define CONFIG_GOT_ENTRY_SIZE 8
#define CONFIG_USE_SINGLE

#define MATCH_MACHINE(x) (x == EM_S390)
#define SHT_RELM	SHT_RELA
#define Elf32_RelM	Elf32_Rela
#define ELFCLASSM	ELFCLASS32
#endif

#if defined(__i386__)
#define CONFIG_USE_GOT_ENTRIES
#define CONFIG_GOT_ENTRY_SIZE 4
#define CONFIG_USE_SINGLE

#ifndef EM_486
#define MATCH_MACHINE(x) (x == EM_386)
#else
#define MATCH_MACHINE(x) (x == EM_386 || x == EM_486)
#endif

#define SHT_RELM	SHT_REL
#define Elf32_RelM	Elf32_Rel
#define ELFCLASSM	ELFCLASS32
#endif

#if defined(__mc68000__)
#define CONFIG_USE_GOT_ENTRIES
#define CONFIG_GOT_ENTRY_SIZE 4
#define CONFIG_USE_SINGLE

#define MATCH_MACHINE(x) (x == EM_68K)
#define SHT_RELM	SHT_RELA
#define Elf32_RelM	Elf32_Rela
#define ELFCLASSM	ELFCLASS32
#endif

#if defined(__mips__)
/* Account for ELF spec changes.  */
#ifndef EM_MIPS_RS3_LE
#ifdef EM_MIPS_RS4_BE
#define EM_MIPS_RS3_LE	EM_MIPS_RS4_BE
#else
#define EM_MIPS_RS3_LE	10
#endif
#endif /* !EM_MIPS_RS3_LE */

#define MATCH_MACHINE(x) (x == EM_MIPS || x == EM_MIPS_RS3_LE)
#define SHT_RELM	SHT_REL
#define Elf32_RelM	Elf32_Rel
#define ELFCLASSM	ELFCLASS32
#define ARCHDATAM       "__dbe_table"
#endif

#if defined(__powerpc__)
#define CONFIG_USE_PLT_ENTRIES
#define CONFIG_PLT_ENTRY_SIZE 16
#define CONFIG_USE_PLT_LIST
#define CONFIG_LIST_ARCHTYPE ElfW(Addr)
#define CONFIG_USE_LIST

#define MATCH_MACHINE(x) (x == EM_PPC)
#define SHT_RELM	SHT_RELA
#define Elf32_RelM	Elf32_Rela
#define ELFCLASSM	ELFCLASS32
#define ARCHDATAM       "__ftr_fixup"
#endif

#if defined(__sh__)
#define CONFIG_USE_GOT_ENTRIES
#define CONFIG_GOT_ENTRY_SIZE 4
#define CONFIG_USE_SINGLE

#define MATCH_MACHINE(x) (x == EM_SH)
#define SHT_RELM	SHT_RELA
#define Elf32_RelM	Elf32_Rela
#define ELFCLASSM	ELFCLASS32

/* the SH changes have only been tested in =little endian= mode */
/* I'm not sure about big endian, so let's warn: */

#if defined(__sh__) && defined(__BIG_ENDIAN__)
#error insmod.c may require changes for use on big endian SH
#endif

/* it may or may not work on the SH1/SH2... So let's error on those
   also */
#if ((!(defined(__SH3__) || defined(__SH4__) || defined(__SH5__)))) && \
	(defined(__sh__))
#error insmod.c may require changes for SH1 or SH2 use
#endif
#endif

#if defined (__v850e__)
#define CONFIG_USE_PLT_ENTRIES
#define CONFIG_PLT_ENTRY_SIZE 8
#define CONFIG_USE_SINGLE

#ifndef EM_CYGNUS_V850	/* grumble */
#define EM_CYGNUS_V850 	0x9080
#endif

#define MATCH_MACHINE(x) ((x) == EM_V850 || (x) == EM_CYGNUS_V850)
#define SHT_RELM	SHT_RELA
#define Elf32_RelM	Elf32_Rela
#define ELFCLASSM	ELFCLASS32

#define SYMBOL_PREFIX	"_"
#endif

#if defined(__cris__)
#ifndef EM_CRIS
#define EM_CRIS 76
#define R_CRIS_NONE 0
#define R_CRIS_32   3
#endif

#define MATCH_MACHINE(x) (x == EM_CRIS)
#define SHT_RELM	SHT_RELA
#define Elf32_RelM	Elf32_Rela
#define ELFCLASSM	ELFCLASS32
#endif

#ifndef SHT_RELM
#error Sorry, but insmod.c does not yet support this architecture...
#endif


//----------------------------------------------------------------------------
//--------modutils module.h, lines 45-242
//----------------------------------------------------------------------------

/* Definitions for the Linux module syscall interface.
   Copyright 1996, 1997 Linux International.

   Contributed by Richard Henderson <rth@tamu.edu>

   This file is part of the Linux modutils.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2 of the License, or (at your
   option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


#ifndef MODUTILS_MODULE_H
static const int MODUTILS_MODULE_H = 1;

#ident "$Id: insmod.c,v 1.116 2004/04/06 11:56:26 andersen Exp $"

/* This file contains the structures used by the 2.0 and 2.1 kernels.
   We do not use the kernel headers directly because we do not wish
   to be dependant on a particular kernel version to compile insmod.  */


/*======================================================================*/
/* The structures used by Linux 2.0.  */

/* The symbol format used by get_kernel_syms(2).  */
struct old_kernel_sym
{
	unsigned long value;
	char name[60];
};

struct old_module_ref
{
	unsigned long module;		/* kernel addresses */
	unsigned long next;
};

struct old_module_symbol
{
	unsigned long addr;
	unsigned long name;
};

struct old_symbol_table
{
	int size;			/* total, including string table!!! */
	int n_symbols;
	int n_refs;
	struct old_module_symbol symbol[0]; /* actual size defined by n_symbols */
	struct old_module_ref ref[0];	/* actual size defined by n_refs */
};

struct old_mod_routines
{
	unsigned long init;
	unsigned long cleanup;
};

struct old_module
{
	unsigned long next;
	unsigned long ref;		/* the list of modules that refer to me */
	unsigned long symtab;
	unsigned long name;
	int size;			/* size of module in pages */
	unsigned long addr;		/* address of module */
	int state;
	unsigned long cleanup;	/* cleanup routine */
};

/* Sent to init_module(2) or'ed into the code size parameter.  */
static const int OLD_MOD_AUTOCLEAN = 0x40000000; /* big enough, but no sign problems... */

int get_kernel_syms(struct old_kernel_sym *);
int old_sys_init_module(const char *name, char *code, unsigned codesize,
			struct old_mod_routines *, struct old_symbol_table *);

/*======================================================================*/
/* For sizeof() which are related to the module platform and not to the
   environment isnmod is running in, use sizeof_xx instead of sizeof(xx).  */

#define tgt_sizeof_char		sizeof(char)
#define tgt_sizeof_short	sizeof(short)
#define tgt_sizeof_int		sizeof(int)
#define tgt_sizeof_long		sizeof(long)
#define tgt_sizeof_char_p	sizeof(char *)
#define tgt_sizeof_void_p	sizeof(void *)
#define tgt_long		long

#if defined(__sparc__) && !defined(__sparc_v9__) && defined(ARCH_sparc64)
#undef tgt_sizeof_long
#undef tgt_sizeof_char_p
#undef tgt_sizeof_void_p
#undef tgt_long
static const int tgt_sizeof_long = 8;
static const int tgt_sizeof_char_p = 8;
static const int tgt_sizeof_void_p = 8;
#define tgt_long		long long
#endif

/*======================================================================*/
/* The structures used in Linux 2.1.  */

/* Note: new_module_symbol does not use tgt_long intentionally */
struct new_module_symbol
{
	unsigned long value;
	unsigned long name;
};

struct new_module_persist;

struct new_module_ref
{
	unsigned tgt_long dep;		/* kernel addresses */
	unsigned tgt_long ref;
	unsigned tgt_long next_ref;
};

struct new_module
{
	unsigned tgt_long size_of_struct;	/* == sizeof(module) */
	unsigned tgt_long next;
	unsigned tgt_long name;
	unsigned tgt_long size;

	tgt_long usecount;
	unsigned tgt_long flags;		/* AUTOCLEAN et al */

	unsigned nsyms;
	unsigned ndeps;

	unsigned tgt_long syms;
	unsigned tgt_long deps;
	unsigned tgt_long refs;
	unsigned tgt_long init;
	unsigned tgt_long cleanup;
	unsigned tgt_long ex_table_start;
	unsigned tgt_long ex_table_end;
#ifdef __alpha__
	unsigned tgt_long gp;
#endif
	/* Everything after here is extension.  */
	unsigned tgt_long persist_start;
	unsigned tgt_long persist_end;
	unsigned tgt_long can_unload;
	unsigned tgt_long runsize;
#ifdef CONFIG_FEATURE_2_4_MODULES
	const char *kallsyms_start;     /* All symbols for kernel debugging */
	const char *kallsyms_end;
	const char *archdata_start;     /* arch specific data for module */
	const char *archdata_end;
	const char *kernel_data;        /* Reserved for kernel internal use */
#endif
};

#ifdef ARCHDATAM
#define ARCHDATA_SEC_NAME ARCHDATAM
#else
#define ARCHDATA_SEC_NAME "__archdata"
#endif
#define KALLSYMS_SEC_NAME "__kallsyms"


struct new_module_info
{
	unsigned long addr;
	unsigned long size;
	unsigned long flags;
	long usecount;
};

/* Bits of module.flags.  */
static const int NEW_MOD_RUNNING = 1;
static const int NEW_MOD_DELETED = 2;
static const int NEW_MOD_AUTOCLEAN = 4;
static const int NEW_MOD_VISITED = 8;
static const int NEW_MOD_USED_ONCE = 16;

int new_sys_init_module(const char *name, const struct new_module *);
int query_module(const char *name, int which, void *buf, size_t bufsize,
		 size_t *ret);

/* Values for query_module's which.  */

static const int QM_MODULES = 1;
static const int QM_DEPS = 2;
static const int QM_REFS = 3;
static const int QM_SYMBOLS = 4;
static const int QM_INFO = 5;

/*======================================================================*/
/* The system calls unchanged between 2.0 and 2.1.  */

unsigned long create_module(const char *, size_t);
int delete_module(const char *);


#endif /* module.h */

//----------------------------------------------------------------------------
//--------end of modutils module.h
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//--------modutils obj.h, lines 253-462
//----------------------------------------------------------------------------

/* Elf object file loading and relocation routines.
   Copyright 1996, 1997 Linux International.

   Contributed by Richard Henderson <rth@tamu.edu>

   This file is part of the Linux modutils.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2 of the License, or (at your
   option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


#ifndef MODUTILS_OBJ_H
static const int MODUTILS_OBJ_H = 1;

#ident "$Id: insmod.c,v 1.116 2004/04/06 11:56:26 andersen Exp $"

/* The relocatable object is manipulated using elfin types.  */

#include <stdio.h>
#include <elf.h>
#include <endian.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ELFDATAM	ELFDATA2LSB
#elif __BYTE_ORDER == __BIG_ENDIAN
#define ELFDATAM	ELFDATA2MSB
#endif

#ifndef ElfW
# if ELFCLASSM == ELFCLASS32
#  define ElfW(x)  Elf32_ ## x
#  define ELFW(x)  ELF32_ ## x
# else
#  define ElfW(x)  Elf64_ ## x
#  define ELFW(x)  ELF64_ ## x
# endif
#endif

/* For some reason this is missing from some ancient C libraries....  */
#ifndef ELF32_ST_INFO
# define ELF32_ST_INFO(bind, type)       (((bind) << 4) + ((type) & 0xf))
#endif

#ifndef ELF64_ST_INFO
# define ELF64_ST_INFO(bind, type)       (((bind) << 4) + ((type) & 0xf))
#endif

struct obj_string_patch;
struct obj_symbol_patch;

struct obj_section
{
	ElfW(Shdr) header;
	const char *name;
	char *contents;
	struct obj_section *load_next;
	int idx;
};

struct obj_symbol
{
	struct obj_symbol *next;	/* hash table link */
	const char *name;
	unsigned long value;
	unsigned long size;
	int secidx;			/* the defining section index/module */
	int info;
	int ksymidx;			/* for export to the kernel symtab */
	int referenced;		/* actually used in the link */
};

/* Hardcode the hash table size.  We shouldn't be needing so many
   symbols that we begin to degrade performance, and we get a big win
   by giving the compiler a constant divisor.  */

#define HASH_BUCKETS  521

struct obj_file
{
	ElfW(Ehdr) header;
	ElfW(Addr) baseaddr;
	struct obj_section **sections;
	struct obj_section *load_order;
	struct obj_section **load_order_search_start;
	struct obj_string_patch *string_patches;
	struct obj_symbol_patch *symbol_patches;
	int (*symbol_cmp)(const char *, const char *);
	unsigned long (*symbol_hash)(const char *);
	unsigned long local_symtab_size;
	struct obj_symbol **local_symtab;
	struct obj_symbol *symtab[HASH_BUCKETS];
};

enum obj_reloc
{
	obj_reloc_ok,
	obj_reloc_overflow,
	obj_reloc_dangerous,
	obj_reloc_unhandled
};

struct obj_string_patch
{
	struct obj_string_patch *next;
	int reloc_secidx;
	ElfW(Addr) reloc_offset;
	ElfW(Addr) string_offset;
};

struct obj_symbol_patch
{
	struct obj_symbol_patch *next;
	int reloc_secidx;
	ElfW(Addr) reloc_offset;
	struct obj_symbol *sym;
};


/* Generic object manipulation routines.  */

static unsigned long obj_elf_hash(const char *);

static unsigned long obj_elf_hash_n(const char *, unsigned long len);

static struct obj_symbol *obj_find_symbol (struct obj_file *f,
					 const char *name);

static ElfW(Addr) obj_symbol_final_value(struct obj_file *f,
				  struct obj_symbol *sym);

#ifdef CONFIG_FEATURE_INSMOD_VERSION_CHECKING
static void obj_set_symbol_compare(struct obj_file *f,
			    int (*cmp)(const char *, const char *),
			    unsigned long (*hash)(const char *));
#endif

static struct obj_section *obj_find_section (struct obj_file *f,
					   const char *name);

static void obj_insert_section_load_order (struct obj_file *f,
				    struct obj_section *sec);

static struct obj_section *obj_create_alloced_section (struct obj_file *f,
						const char *name,
						unsigned long align,
						unsigned long size);

static struct obj_section *obj_create_alloced_section_first (struct obj_file *f,
						      const char *name,
						      unsigned long align,
						      unsigned long size);

static void *obj_extend_section (struct obj_section *sec, unsigned long more);

static int obj_string_patch(struct obj_file *f, int secidx, ElfW(Addr) offset,
		     const char *string);

#ifdef CONFIG_FEATURE_2_4_MODULES
static int obj_symbol_patch(struct obj_file *f, int secidx, ElfW(Addr) offset,
		     struct obj_symbol *sym);
#endif

static int obj_check_undefineds(struct obj_file *f);

static void obj_allocate_commons(struct obj_file *f);

static unsigned long obj_load_size (struct obj_file *f);

static int obj_relocate (struct obj_file *f, ElfW(Addr) base);

static struct obj_file *obj_load(FILE *f, int loadprogbits);

static int obj_create_image (struct obj_file *f, char *image);

/* Architecture specific manipulation routines.  */

static struct obj_file *arch_new_file (void);

static struct obj_section *arch_new_section (void);

static struct obj_symbol *arch_new_symbol (void);

static enum obj_reloc arch_apply_relocation (struct obj_file *f,
				      struct obj_section *targsec,
				      struct obj_section *symsec,
				      struct obj_symbol *sym,
				      ElfW(RelM) *rel, ElfW(Addr) value);

static void arch_create_got (struct obj_file *f);

static int obj_gpl_license(struct obj_file *f, const char **license);

#ifdef CONFIG_FEATURE_2_4_MODULES
static int arch_init_module (struct obj_file *f, struct new_module *);
#endif

#endif /* obj.h */
//----------------------------------------------------------------------------
//--------end of modutils obj.h
//----------------------------------------------------------------------------


/* SPFX is always a string, so it can be concatenated to string constants.  */
#ifdef SYMBOL_PREFIX
#define SPFX	SYMBOL_PREFIX
#else
#define SPFX 	""
#endif


#define _PATH_MODULES	"/lib/modules"
static const int STRVERSIONLEN = 32;

/*======================================================================*/

static int flag_force_load = 0;
static int flag_autoclean = 0;
static int flag_verbose = 0;
static int flag_quiet = 0;
static int flag_export = 1;


/*======================================================================*/

#if defined(CONFIG_USE_LIST)

struct arch_list_entry
{
	struct arch_list_entry *next;
	CONFIG_LIST_ARCHTYPE addend;
	int offset;
	int inited : 1;
};

#endif

#if defined(CONFIG_USE_SINGLE)

struct arch_single_entry
{
	int offset;
	int inited : 1;
	int allocated : 1;
};

#endif

#if defined(__mips__)
struct mips_hi16
{
	struct mips_hi16 *next;
	Elf32_Addr *addr;
	Elf32_Addr value;
};
#endif

struct arch_file {
	struct obj_file root;
#if defined(CONFIG_USE_PLT_ENTRIES)
	struct obj_section *plt;
#endif
#if defined(CONFIG_USE_GOT_ENTRIES)
	struct obj_section *got;
#endif
#if defined(__mips__)
	struct mips_hi16 *mips_hi16_list;
#endif
};

struct arch_symbol {
	struct obj_symbol root;
#if defined(CONFIG_USE_PLT_ENTRIES)
#if defined(CONFIG_USE_PLT_LIST)
	struct arch_list_entry *pltent;
#else
	struct arch_single_entry pltent;
#endif
#endif
#if defined(CONFIG_USE_GOT_ENTRIES)
	struct arch_single_entry gotent;
#endif
};


struct external_module {
	const char *name;
	ElfW(Addr) addr;
	int used;
	size_t nsyms;
	struct new_module_symbol *syms;
};

static struct new_module_symbol *ksyms;
static size_t nksyms;

static struct external_module *ext_modules;
static int n_ext_modules;
static int n_ext_modules_used;
extern int delete_module(const char *);

static char *m_filename;
static char *m_fullName;



/*======================================================================*/


static int check_module_name_match(const char *filename, struct stat *statbuf,
						   void *userdata)
{
	char *fullname = (char *) userdata;

	if (fullname[0] == '\0')
		return (FALSE);
	else {
		char *tmp, *tmp1 = bb_xstrdup(filename);
		tmp = bb_get_last_path_component(tmp1);
		if (strcmp(tmp, fullname) == 0) {
			free(tmp1);
			/* Stop searching if we find a match */
			m_filename = bb_xstrdup(filename);
			return (FALSE);
		}
		free(tmp1);
	}
	return (TRUE);
}


/*======================================================================*/

static struct obj_file *arch_new_file(void)
{
	struct arch_file *f;
	f = xmalloc(sizeof(*f));

	memset(f, 0, sizeof(*f));

	return &f->root;
}

static struct obj_section *arch_new_section(void)
{
	return xmalloc(sizeof(struct obj_section));
}

static struct obj_symbol *arch_new_symbol(void)
{
	struct arch_symbol *sym;
	sym = xmalloc(sizeof(*sym));

	memset(sym, 0, sizeof(*sym));

	return &sym->root;
}

static enum obj_reloc
arch_apply_relocation(struct obj_file *f,
					  struct obj_section *targsec,
					  struct obj_section *symsec,
					  struct obj_symbol *sym,
				      ElfW(RelM) *rel, ElfW(Addr) v)
{
	struct arch_file *ifile = (struct arch_file *) f;
	enum obj_reloc ret = obj_reloc_ok;
	ElfW(Addr) *loc = (ElfW(Addr) *) (targsec->contents + rel->r_offset);
	ElfW(Addr) dot = targsec->header.sh_addr + rel->r_offset;
#if defined(CONFIG_USE_GOT_ENTRIES) || defined(CONFIG_USE_PLT_ENTRIES)
	struct arch_symbol *isym = (struct arch_symbol *) sym;
#endif
#if defined(CONFIG_USE_GOT_ENTRIES)
	ElfW(Addr) got = ifile->got ? ifile->got->header.sh_addr : 0;
#endif
#if defined(CONFIG_USE_PLT_ENTRIES)
	ElfW(Addr) plt = ifile->plt ? ifile->plt->header.sh_addr : 0;
	unsigned long *ip;
#if defined(CONFIG_USE_PLT_LIST)
	struct arch_list_entry *pe;
#else
	struct arch_single_entry *pe;
#endif
#endif

	switch (ELF32_R_TYPE(rel->r_info)) {


#if defined(__arm__)
		case R_ARM_NONE:
			break;

		case R_ARM_ABS32:
			*loc += v;
			break;

		case R_ARM_GOT32:
			goto bb_use_got;

		case R_ARM_GOTPC:
			/* relative reloc, always to _GLOBAL_OFFSET_TABLE_
			 * (which is .got) similar to branch,
			 * but is full 32 bits relative */

			assert(got);
			*loc += got - dot;
			break;

		case R_ARM_PC24:
		case R_ARM_PLT32:
			goto bb_use_plt;

		case R_ARM_GOTOFF: /* address relative to the got */
			assert(got);
			*loc += v - got;
			break;

#elif defined(__s390__)
		case R_390_32:
			*(unsigned int *) loc += v;
			break;
		case R_390_16:
			*(unsigned short *) loc += v;
			break;
		case R_390_8:
			*(unsigned char *) loc += v;
			break;

		case R_390_PC32:
			*(unsigned int *) loc += v - dot;
			break;
		case R_390_PC16DBL:
			*(unsigned short *) loc += (v - dot) >> 1;
			break;
		case R_390_PC16:
			*(unsigned short *) loc += v - dot;
			break;

		case R_390_PLT32:
		case R_390_PLT16DBL:
			/* find the plt entry and initialize it.  */
			assert(isym != NULL);
			pe = (struct arch_single_entry *) &isym->pltent;
			assert(pe->allocated);
			if (pe->inited == 0) {
				ip = (unsigned long *)(ifile->plt->contents + pe->offset);
				ip[0] = 0x0d105810; /* basr 1,0; lg 1,10(1); br 1 */
				ip[1] = 0x100607f1;
				if (ELF32_R_TYPE(rel->r_info) == R_390_PLT16DBL)
					ip[2] = v - 2;
				else
					ip[2] = v;
				pe->inited = 1;
			}

			/* Insert relative distance to target.  */
			v = plt + pe->offset - dot;
			if (ELF32_R_TYPE(rel->r_info) == R_390_PLT32)
				*(unsigned int *) loc = (unsigned int) v;
			else if (ELF32_R_TYPE(rel->r_info) == R_390_PLT16DBL)
				*(unsigned short *) loc = (unsigned short) ((v + 2) >> 1);
			break;

		case R_390_GLOB_DAT:
		case R_390_JMP_SLOT:
			*loc = v;
			break;

		case R_390_RELATIVE:
			*loc += f->baseaddr;
			break;

		case R_390_GOTPC:
			assert(got != 0);
			*(unsigned long *) loc += got - dot;
			break;

		case R_390_GOT12:
		case R_390_GOT16:
		case R_390_GOT32:
			assert(isym != NULL);
			assert(got != 0);
			if (!isym->gotent.inited)
			{
				isym->gotent.inited = 1;
				*(Elf32_Addr *)(ifile->got->contents + isym->gotent.offset) = v;
			}
			if (ELF32_R_TYPE(rel->r_info) == R_390_GOT12)
				*(unsigned short *) loc |= (*(unsigned short *) loc + isym->gotent.offset) & 0xfff;
			else if (ELF32_R_TYPE(rel->r_info) == R_390_GOT16)
				*(unsigned short *) loc += isym->gotent.offset;
			else if (ELF32_R_TYPE(rel->r_info) == R_390_GOT32)
				*(unsigned int *) loc += isym->gotent.offset;
			break;

#ifndef R_390_GOTOFF32
#define R_390_GOTOFF32 R_390_GOTOFF
#endif
		case R_390_GOTOFF32:
			assert(got != 0);
			*loc += v - got;
			break;

#elif defined(__i386__)

		case R_386_NONE:
			break;

		case R_386_32:
			*loc += v;
			break;

		case R_386_PLT32:
		case R_386_PC32:
			*loc += v - dot;
			break;

		case R_386_GLOB_DAT:
		case R_386_JMP_SLOT:
			*loc = v;
			break;

		case R_386_RELATIVE:
			*loc += f->baseaddr;
			break;

		case R_386_GOTPC:
			assert(got != 0);
			*loc += got - dot;
			break;

		case R_386_GOT32:
			goto bb_use_got;

		case R_386_GOTOFF:
			assert(got != 0);
			*loc += v - got;
			break;

#elif defined(__mc68000__)

		case R_68K_NONE:
			break;

		case R_68K_32:
			*loc += v;
			break;

		case R_68K_8:
			if (v > 0xff) {
				ret = obj_reloc_overflow;
			}
			*(char *)loc = v;
			break;

		case R_68K_16:
			if (v > 0xffff) {
				ret = obj_reloc_overflow;
			}
			*(short *)loc = v;
			break;

		case R_68K_PC8:
			v -= dot;
			if ((Elf32_Sword)v > 0x7f ||
					(Elf32_Sword)v < -(Elf32_Sword)0x80) {
				ret = obj_reloc_overflow;
			}
			*(char *)loc = v;
			break;

		case R_68K_PC16:
			v -= dot;
			if ((Elf32_Sword)v > 0x7fff ||
					(Elf32_Sword)v < -(Elf32_Sword)0x8000) {
				ret = obj_reloc_overflow;
			}
			*(short *)loc = v;
			break;

		case R_68K_PC32:
			*(int *)loc = v - dot;
			break;

		case R_68K_GLOB_DAT:
		case R_68K_JMP_SLOT:
			*loc = v;
			break;

		case R_68K_RELATIVE:
			*(int *)loc += f->baseaddr;
			break;

		case R_68K_GOT32:
			goto bb_use_got;

#ifdef R_68K_GOTOFF
		case R_68K_GOTOFF:
			assert(got != 0);
			*loc += v - got;
			break;
#endif

#elif defined(__mips__)

		case R_MIPS_NONE:
			break;

		case R_MIPS_32:
			*loc += v;
			break;

		case R_MIPS_26:
			if (v % 4)
				ret = obj_reloc_dangerous;
			if ((v & 0xf0000000) != ((dot + 4) & 0xf0000000))
				ret = obj_reloc_overflow;
			*loc =
				(*loc & ~0x03ffffff) | ((*loc + (v >> 2)) &
										0x03ffffff);
			break;

		case R_MIPS_HI16:
			{
				struct mips_hi16 *n;

				/* We cannot relocate this one now because we don't know the value
				   of the carry we need to add.  Save the information, and let LO16
				   do the actual relocation.  */
				n = (struct mips_hi16 *) xmalloc(sizeof *n);
				n->addr = loc;
				n->value = v;
				n->next = ifile->mips_hi16_list;
				ifile->mips_hi16_list = n;
				break;
			}

		case R_MIPS_LO16:
			{
				unsigned long insnlo = *loc;
				Elf32_Addr val, vallo;

				/* Sign extend the addend we extract from the lo insn.  */
				vallo = ((insnlo & 0xffff) ^ 0x8000) - 0x8000;

				if (ifile->mips_hi16_list != NULL) {
					struct mips_hi16 *l;

					l = ifile->mips_hi16_list;
					while (l != NULL) {
						struct mips_hi16 *next;
						unsigned long insn;

						/* The value for the HI16 had best be the same. */
						assert(v == l->value);

						/* Do the HI16 relocation.  Note that we actually don't
						   need to know anything about the LO16 itself, except where
						   to find the low 16 bits of the addend needed by the LO16.  */
						insn = *l->addr;
						val =
							((insn & 0xffff) << 16) +
							vallo;
						val += v;

						/* Account for the sign extension that will happen in the
						   low bits.  */
						val =
							((val >> 16) +
							 ((val & 0x8000) !=
							  0)) & 0xffff;

						insn = (insn & ~0xffff) | val;
						*l->addr = insn;

						next = l->next;
						free(l);
						l = next;
					}

					ifile->mips_hi16_list = NULL;
				}

				/* Ok, we're done with the HI16 relocs.  Now deal with the LO16.  */
				val = v + vallo;
				insnlo = (insnlo & ~0xffff) | (val & 0xffff);
				*loc = insnlo;
				break;
			}

#elif defined(__powerpc__)

		case R_PPC_ADDR16_HA:
			*(unsigned short *)loc = (v + 0x8000) >> 16;
			break;

		case R_PPC_ADDR16_HI:
			*(unsigned short *)loc = v >> 16;
			break;

		case R_PPC_ADDR16_LO:
			*(unsigned short *)loc = v;
			break;

		case R_PPC_REL24:
			goto bb_use_plt;

		case R_PPC_REL32:
			*loc = v - dot;
			break;

		case R_PPC_ADDR32:
			*loc = v;
			break;

#elif defined(__sh__)

		case R_SH_NONE:
			break;

		case R_SH_DIR32:
			*loc += v;
			break;

		case R_SH_REL32:
			*loc += v - dot;
			break;

		case R_SH_PLT32:
			*loc = v - dot;
			break;

		case R_SH_GLOB_DAT:
		case R_SH_JMP_SLOT:
			*loc = v;
			break;

		case R_SH_RELATIVE:
			*loc = f->baseaddr + rel->r_addend;
			break;

		case R_SH_GOTPC:
			assert(got != 0);
			*loc = got - dot + rel->r_addend;
			break;

		case R_SH_GOT32:
			goto bb_use_got;

		case R_SH_GOTOFF:
			assert(got != 0);
			*loc = v - got;
			break;

#if defined(__SH5__)
		case R_SH_IMM_MEDLOW16:
		case R_SH_IMM_LOW16:
			{
				Elf32_Addr word;

				if (ELF32_R_TYPE(rel->r_info) == R_SH_IMM_MEDLOW16)
					v >>= 16;

				/*
				 *  movi and shori have the format:
				 *
				 *  |  op  | imm  | reg | reserved |
				 *   31..26 25..10 9.. 4 3   ..   0
				 *
				 * so we simply mask and or in imm.
				 */
				word = *loc & ~0x3fffc00;
				word |= (v & 0xffff) << 10;

				*loc = word;

				break;
			}

		case R_SH_IMM_MEDLOW16_PCREL:
		case R_SH_IMM_LOW16_PCREL:
			{
				Elf32_Addr word;

				word = *loc & ~0x3fffc00;

				v -= dot;

				if (ELF32_R_TYPE(rel->r_info) == R_SH_IMM_MEDLOW16_PCREL)
					v >>= 16;

				word |= (v & 0xffff) << 10;

				*loc = word;

				break;
			}
#endif /* __SH5__ */
#endif /* __sh__ */

		default:
			printf("Warning: unhandled reloc %d\n",(int)ELF32_R_TYPE(rel->r_info));
			ret = obj_reloc_unhandled;
			break;

#if defined (__v850e__)
		case R_V850_NONE:
			break;

		case R_V850_32:
			/* We write two shorts instead of a long because even
			   32-bit insns only need half-word alignment, but
			   32-bit data needs to be long-word aligned.  */
			v += ((unsigned short *)loc)[0];
			v += ((unsigned short *)loc)[1] << 16;
			((unsigned short *)loc)[0] = v & 0xffff;
			((unsigned short *)loc)[1] = (v >> 16) & 0xffff;
			break;

		case R_V850_22_PCREL:
			goto bb_use_plt;
#endif

#if defined (__cris__)
		case R_CRIS_NONE:
			break;

		case R_CRIS_32:
			/* CRIS keeps the relocation value in the r_addend field and
			 * should not use whats in *loc at all
			 */
			*loc = v;
			break;
#endif

#if defined(CONFIG_USE_PLT_ENTRIES)

bb_use_plt:

			/* find the plt entry and initialize it if necessary */
			assert(isym != NULL);

#if defined(CONFIG_USE_PLT_LIST)
			for (pe = isym->pltent; pe != NULL && pe->addend != rel->r_addend;)
				pe = pe->next;
			assert(pe != NULL);
#else
			pe = &isym->pltent;
#endif

			if (! pe->inited) {
				ip = (unsigned long *) (ifile->plt->contents + pe->offset);

				/* generate some machine code */

#if defined(__arm__)
				ip[0] = 0xe51ff004;			/* ldr pc,[pc,#-4] */
				ip[1] = v;				/* sym@ */
#endif
#if defined(__powerpc__)
				ip[0] = 0x3d600000 + ((v + 0x8000) >> 16);  /* lis r11,sym@ha */
				ip[1] = 0x396b0000 + (v & 0xffff);	      /* addi r11,r11,sym@l */
				ip[2] = 0x7d6903a6;			      /* mtctr r11 */
				ip[3] = 0x4e800420;			      /* bctr */
#endif
#if defined (__v850e__)
				/* We have to trash a register, so we assume that any control
				   transfer more than 21-bits away must be a function call
				   (so we can use a call-clobbered register).  */
				ip[0] = 0x0621 + ((v & 0xffff) << 16);   /* mov sym, r1 ... */
				ip[1] = ((v >> 16) & 0xffff) + 0x610000; /* ...; jmp r1 */
#endif
				pe->inited = 1;
			}

			/* relative distance to target */
			v -= dot;
			/* if the target is too far away.... */
#if defined (__arm__) || defined (__powerpc__)
			if ((int)v < -0x02000000 || (int)v >= 0x02000000)
#elif defined (__v850e__)
				if ((Elf32_Sword)v > 0x1fffff || (Elf32_Sword)v < (Elf32_Sword)-0x200000)
#endif
					/* go via the plt */
					v = plt + pe->offset - dot;

#if defined (__v850e__)
			if (v & 1)
#else
				if (v & 3)
#endif
					ret = obj_reloc_dangerous;

			/* merge the offset into the instruction. */
#if defined(__arm__)
			/* Convert to words. */
			v >>= 2;

			*loc = (*loc & ~0x00ffffff) | ((v + *loc) & 0x00ffffff);
#endif
#if defined(__powerpc__)
			*loc = (*loc & ~0x03fffffc) | (v & 0x03fffffc);
#endif
#if defined (__v850e__)
			/* We write two shorts instead of a long because even 32-bit insns
			   only need half-word alignment, but the 32-bit data write needs
			   to be long-word aligned.  */
			((unsigned short *)loc)[0] =
				(*(unsigned short *)loc & 0xffc0) /* opcode + reg */
				| ((v >> 16) & 0x3f);             /* offs high part */
			((unsigned short *)loc)[1] =
				(v & 0xffff);                    /* offs low part */
#endif
			break;
#endif /* CONFIG_USE_PLT_ENTRIES */

#if defined(CONFIG_USE_GOT_ENTRIES)
bb_use_got:

			assert(isym != NULL);
			/* needs an entry in the .got: set it, once */
			if (!isym->gotent.inited) {
				isym->gotent.inited = 1;
				*(ElfW(Addr) *) (ifile->got->contents + isym->gotent.offset) = v;
			}
			/* make the reloc with_respect_to_.got */
#if defined(__sh__)
			*loc += isym->gotent.offset + rel->r_addend;
#elif defined(__i386__) || defined(__arm__) || defined(__mc68000__)
			*loc += isym->gotent.offset;
#endif
			break;

#endif /* CONFIG_USE_GOT_ENTRIES */
	}

	return ret;
}


#if defined(CONFIG_USE_LIST)

static int arch_list_add(ElfW(RelM) *rel, struct arch_list_entry **list,
			  int offset, int size)
{
	struct arch_list_entry *pe;

	for (pe = *list; pe != NULL; pe = pe->next) {
		if (pe->addend == rel->r_addend) {
			break;
		}
	}

	if (pe == NULL) {
		pe = xmalloc(sizeof(struct arch_list_entry));
		pe->next = *list;
		pe->addend = rel->r_addend;
		pe->offset = offset;
		pe->inited = 0;
		*list = pe;
		return size;
	}
	return 0;
}

#endif

#if defined(CONFIG_USE_SINGLE)

static int arch_single_init(ElfW(RelM) *rel, struct arch_single_entry *single,
			     int offset, int size)
{
	if (single->allocated == 0) {
		single->allocated = 1;
		single->offset = offset;
		single->inited = 0;
		return size;
	}
	return 0;
}

#endif

#if defined(CONFIG_USE_GOT_ENTRIES) || defined(CONFIG_USE_PLT_ENTRIES)

static struct obj_section *arch_xsect_init(struct obj_file *f, char *name,
					   int offset, int size)
{
	struct obj_section *myrelsec = obj_find_section(f, name);

	if (offset == 0) {
		offset += size;
	}

	if (myrelsec) {
		obj_extend_section(myrelsec, offset);
	} else {
		myrelsec = obj_create_alloced_section(f, name,
				size, offset);
		assert(myrelsec);
	}

	return myrelsec;
}

#endif

static void arch_create_got(struct obj_file *f)
{
#if defined(CONFIG_USE_GOT_ENTRIES) || defined(CONFIG_USE_PLT_ENTRIES)
	struct arch_file *ifile = (struct arch_file *) f;
	int i;
#if defined(CONFIG_USE_GOT_ENTRIES)
	int got_offset = 0, got_needed = 0, got_allocate;
#endif
#if defined(CONFIG_USE_PLT_ENTRIES)
	int plt_offset = 0, plt_needed = 0, plt_allocate;
#endif
	struct obj_section *relsec, *symsec, *strsec;
	ElfW(RelM) *rel, *relend;
	ElfW(Sym) *symtab, *extsym;
	const char *strtab, *name;
	struct arch_symbol *intsym;

	for (i = 0; i < f->header.e_shnum; ++i) {
		relsec = f->sections[i];
		if (relsec->header.sh_type != SHT_RELM)
			continue;

		symsec = f->sections[relsec->header.sh_link];
		strsec = f->sections[symsec->header.sh_link];

		rel = (ElfW(RelM) *) relsec->contents;
		relend = rel + (relsec->header.sh_size / sizeof(ElfW(RelM)));
		symtab = (ElfW(Sym) *) symsec->contents;
		strtab = (const char *) strsec->contents;

		for (; rel < relend; ++rel) {
			extsym = &symtab[ELF32_R_SYM(rel->r_info)];

#if defined(CONFIG_USE_GOT_ENTRIES)
			got_allocate = 0;
#endif
#if defined(CONFIG_USE_PLT_ENTRIES)
			plt_allocate = 0;
#endif

			switch (ELF32_R_TYPE(rel->r_info)) {
#if defined(__arm__)
				case R_ARM_PC24:
				case R_ARM_PLT32:
					plt_allocate = 1;
					break;

				case R_ARM_GOTOFF:
				case R_ARM_GOTPC:
					got_needed = 1;
					continue;

				case R_ARM_GOT32:
					got_allocate = 1;
					break;

#elif defined(__i386__)
				case R_386_GOTPC:
				case R_386_GOTOFF:
					got_needed = 1;
					continue;

				case R_386_GOT32:
					got_allocate = 1;
					break;

#elif defined(__powerpc__)
				case R_PPC_REL24:
					plt_allocate = 1;
					break;

#elif defined(__mc68000__)
				case R_68K_GOT32:
					got_allocate = 1;
					break;

#ifdef R_68K_GOTOFF
				case R_68K_GOTOFF:
					got_needed = 1;
					continue;
#endif

#elif defined(__sh__)
				case R_SH_GOT32:
					got_allocate = 1;
					break;

				case R_SH_GOTPC:
				case R_SH_GOTOFF:
					got_needed = 1;
					continue;

#elif defined (__v850e__)
				case R_V850_22_PCREL:
					plt_needed = 1;
					break;

#endif
				default:
					continue;
			}

			if (extsym->st_name != 0) {
				name = strtab + extsym->st_name;
			} else {
				name = f->sections[extsym->st_shndx]->name;
			}
			intsym = (struct arch_symbol *) obj_find_symbol(f, name);
#if defined(CONFIG_USE_GOT_ENTRIES)
			if (got_allocate) {
				got_offset += arch_single_init(
						rel, &intsym->gotent,
						got_offset, CONFIG_GOT_ENTRY_SIZE);

				got_needed = 1;
			}
#endif
#if defined(CONFIG_USE_PLT_ENTRIES)
			if (plt_allocate) {
#if defined(CONFIG_USE_PLT_LIST)
				plt_offset += arch_list_add(
						rel, &intsym->pltent,
						plt_offset, CONFIG_PLT_ENTRY_SIZE);
#else
				plt_offset += arch_single_init(
						rel, &intsym->pltent,
						plt_offset, CONFIG_PLT_ENTRY_SIZE);
#endif
				plt_needed = 1;
			}
#endif
		}
	}

#if defined(CONFIG_USE_GOT_ENTRIES)
	if (got_needed) {
		ifile->got = arch_xsect_init(f, ".got", got_offset,
				CONFIG_GOT_ENTRY_SIZE);
	}
#endif

#if defined(CONFIG_USE_PLT_ENTRIES)
	if (plt_needed) {
		ifile->plt = arch_xsect_init(f, ".plt", plt_offset,
				CONFIG_PLT_ENTRY_SIZE);
	}
#endif

#endif /* defined(CONFIG_USE_GOT_ENTRIES) || defined(CONFIG_USE_PLT_ENTRIES) */
}

#ifdef CONFIG_FEATURE_2_4_MODULES
static int arch_init_module(struct obj_file *f, struct new_module *mod)
{
	return 1;
}
#endif

/*======================================================================*/

/* Standard ELF hash function.  */
static inline unsigned long obj_elf_hash_n(const char *name, unsigned long n)
{
	unsigned long h = 0;
	unsigned long g;
	unsigned char ch;

	while (n > 0) {
		ch = *name++;
		h = (h << 4) + ch;
		if ((g = (h & 0xf0000000)) != 0) {
			h ^= g >> 24;
			h &= ~g;
		}
		n--;
	}
	return h;
}

static unsigned long obj_elf_hash(const char *name)
{
	return obj_elf_hash_n(name, strlen(name));
}

#ifdef CONFIG_FEATURE_INSMOD_VERSION_CHECKING
/* String comparison for non-co-versioned kernel and module.  */

static int ncv_strcmp(const char *a, const char *b)
{
	size_t alen = strlen(a), blen = strlen(b);

	if (blen == alen + 10 && b[alen] == '_' && b[alen + 1] == 'R')
		return strncmp(a, b, alen);
	else if (alen == blen + 10 && a[blen] == '_' && a[blen + 1] == 'R')
		return strncmp(a, b, blen);
	else
		return strcmp(a, b);
}

/* String hashing for non-co-versioned kernel and module.  Here
   we are simply forced to drop the crc from the hash.  */

static unsigned long ncv_symbol_hash(const char *str)
{
	size_t len = strlen(str);
	if (len > 10 && str[len - 10] == '_' && str[len - 9] == 'R')
		len -= 10;
	return obj_elf_hash_n(str, len);
}

static void
obj_set_symbol_compare(struct obj_file *f,
					   int (*cmp) (const char *, const char *),
					   unsigned long (*hash) (const char *))
{
	if (cmp)
		f->symbol_cmp = cmp;
	if (hash) {
		struct obj_symbol *tmptab[HASH_BUCKETS], *sym, *next;
		int i;

		f->symbol_hash = hash;

		memcpy(tmptab, f->symtab, sizeof(tmptab));
		memset(f->symtab, 0, sizeof(f->symtab));

		for (i = 0; i < HASH_BUCKETS; ++i)
			for (sym = tmptab[i]; sym; sym = next) {
				unsigned long h = hash(sym->name) % HASH_BUCKETS;
				next = sym->next;
				sym->next = f->symtab[h];
				f->symtab[h] = sym;
			}
	}
}

#endif							/* CONFIG_FEATURE_INSMOD_VERSION_CHECKING */

static struct obj_symbol *
obj_add_symbol(struct obj_file *f, const char *name,
								  unsigned long symidx, int info,
								  int secidx, ElfW(Addr) value,
								  unsigned long size)
{
	struct obj_symbol *sym;
	unsigned long hash = f->symbol_hash(name) % HASH_BUCKETS;
	int n_type = ELFW(ST_TYPE) (info);
	int n_binding = ELFW(ST_BIND) (info);

	for (sym = f->symtab[hash]; sym; sym = sym->next)
		if (f->symbol_cmp(sym->name, name) == 0) {
			int o_secidx = sym->secidx;
			int o_info = sym->info;
			int o_type = ELFW(ST_TYPE) (o_info);
			int o_binding = ELFW(ST_BIND) (o_info);

			/* A redefinition!  Is it legal?  */

			if (secidx == SHN_UNDEF)
				return sym;
			else if (o_secidx == SHN_UNDEF)
				goto found;
			else if (n_binding == STB_GLOBAL && o_binding == STB_LOCAL) {
				/* Cope with local and global symbols of the same name
				   in the same object file, as might have been created
				   by ld -r.  The only reason locals are now seen at this
				   level at all is so that we can do semi-sensible things
				   with parameters.  */

				struct obj_symbol *nsym, **p;

				nsym = arch_new_symbol();
				nsym->next = sym->next;
				nsym->ksymidx = -1;

				/* Excise the old (local) symbol from the hash chain.  */
				for (p = &f->symtab[hash]; *p != sym; p = &(*p)->next)
					continue;
				*p = sym = nsym;
				goto found;
			} else if (n_binding == STB_LOCAL) {
				/* Another symbol of the same name has already been defined.
				   Just add this to the local table.  */
				sym = arch_new_symbol();
				sym->next = NULL;
				sym->ksymidx = -1;
				f->local_symtab[symidx] = sym;
				goto found;
			} else if (n_binding == STB_WEAK)
				return sym;
			else if (o_binding == STB_WEAK)
				goto found;
			/* Don't unify COMMON symbols with object types the programmer
			   doesn't expect.  */
			else if (secidx == SHN_COMMON
					&& (o_type == STT_NOTYPE || o_type == STT_OBJECT))
				return sym;
			else if (o_secidx == SHN_COMMON
					&& (n_type == STT_NOTYPE || n_type == STT_OBJECT))
				goto found;
			else {
				/* Don't report an error if the symbol is coming from
				   the kernel or some external module.  */
				if (secidx <= SHN_HIRESERVE)
					bb_error_msg("%s multiply defined", name);
				return sym;
			}
		}

	/* Completely new symbol.  */
	sym = arch_new_symbol();
	sym->next = f->symtab[hash];
	f->symtab[hash] = sym;
	sym->ksymidx = -1;

	if (ELFW(ST_BIND)(info) == STB_LOCAL && symidx != -1) {
		if (symidx >= f->local_symtab_size)
			bb_error_msg("local symbol %s with index %ld exceeds local_symtab_size %ld",
					name, (long) symidx, (long) f->local_symtab_size);
		else
			f->local_symtab[symidx] = sym;
	}

found:
	sym->name = name;
	sym->value = value;
	sym->size = size;
	sym->secidx = secidx;
	sym->info = info;

	return sym;
}

static struct obj_symbol *
obj_find_symbol(struct obj_file *f, const char *name)
{
	struct obj_symbol *sym;
	unsigned long hash = f->symbol_hash(name) % HASH_BUCKETS;

	for (sym = f->symtab[hash]; sym; sym = sym->next)
		if (f->symbol_cmp(sym->name, name) == 0)
			return sym;

	return NULL;
}

static ElfW(Addr)
	obj_symbol_final_value(struct obj_file * f, struct obj_symbol * sym)
{
	if (sym) {
		if (sym->secidx >= SHN_LORESERVE)
			return sym->value;

		return sym->value + f->sections[sym->secidx]->header.sh_addr;
	} else {
		/* As a special case, a NULL sym has value zero.  */
		return 0;
	}
}

static struct obj_section *obj_find_section(struct obj_file *f, const char *name)
{
	int i, n = f->header.e_shnum;

	for (i = 0; i < n; ++i)
		if (strcmp(f->sections[i]->name, name) == 0)
			return f->sections[i];

	return NULL;
}

static int obj_load_order_prio(struct obj_section *a)
{
	unsigned long af, ac;

	af = a->header.sh_flags;

	ac = 0;
	if (a->name[0] != '.' || strlen(a->name) != 10 ||
			strcmp(a->name + 5, ".init"))
		ac |= 32;
	if (af & SHF_ALLOC)
		ac |= 16;
	if (!(af & SHF_WRITE))
		ac |= 8;
	if (af & SHF_EXECINSTR)
		ac |= 4;
	if (a->header.sh_type != SHT_NOBITS)
		ac |= 2;

	return ac;
}

static void
obj_insert_section_load_order(struct obj_file *f, struct obj_section *sec)
{
	struct obj_section **p;
	int prio = obj_load_order_prio(sec);
	for (p = f->load_order_search_start; *p; p = &(*p)->load_next)
		if (obj_load_order_prio(*p) < prio)
			break;
	sec->load_next = *p;
	*p = sec;
}

static struct obj_section *obj_create_alloced_section(struct obj_file *f,
											   const char *name,
											   unsigned long align,
											   unsigned long size)
{
	int newidx = f->header.e_shnum++;
	struct obj_section *sec;

	f->sections = xrealloc(f->sections, (newidx + 1) * sizeof(sec));
	f->sections[newidx] = sec = arch_new_section();

	memset(sec, 0, sizeof(*sec));
	sec->header.sh_type = SHT_PROGBITS;
	sec->header.sh_flags = SHF_WRITE | SHF_ALLOC;
	sec->header.sh_size = size;
	sec->header.sh_addralign = align;
	sec->name = name;
	sec->idx = newidx;
	if (size)
		sec->contents = xmalloc(size);

	obj_insert_section_load_order(f, sec);

	return sec;
}

static struct obj_section *obj_create_alloced_section_first(struct obj_file *f,
													 const char *name,
													 unsigned long align,
													 unsigned long size)
{
	int newidx = f->header.e_shnum++;
	struct obj_section *sec;

	f->sections = xrealloc(f->sections, (newidx + 1) * sizeof(sec));
	f->sections[newidx] = sec = arch_new_section();

	memset(sec, 0, sizeof(*sec));
	sec->header.sh_type = SHT_PROGBITS;
	sec->header.sh_flags = SHF_WRITE | SHF_ALLOC;
	sec->header.sh_size = size;
	sec->header.sh_addralign = align;
	sec->name = name;
	sec->idx = newidx;
	if (size)
		sec->contents = xmalloc(size);

	sec->load_next = f->load_order;
	f->load_order = sec;
	if (f->load_order_search_start == &f->load_order)
		f->load_order_search_start = &sec->load_next;

	return sec;
}

static void *obj_extend_section(struct obj_section *sec, unsigned long more)
{
	unsigned long oldsize = sec->header.sh_size;
	if (more) {
		sec->contents = xrealloc(sec->contents, sec->header.sh_size += more);
	}
	return sec->contents + oldsize;
}


/* Conditionally add the symbols from the given symbol set to the
   new module.  */

static int
add_symbols_from(
				 struct obj_file *f,
				 int idx, struct new_module_symbol *syms, size_t nsyms)
{
	struct new_module_symbol *s;
	size_t i;
	int used = 0;
#ifdef SYMBOL_PREFIX
	char *name_buf = 0;
	size_t name_alloced_size = 0;
#endif
#ifdef CONFIG_FEATURE_CHECK_TAINTED_MODULE
	int gpl;

	gpl = obj_gpl_license(f, NULL) == 0;
#endif
	for (i = 0, s = syms; i < nsyms; ++i, ++s) {
		/* Only add symbols that are already marked external.
		   If we override locals we may cause problems for
		   argument initialization.  We will also create a false
		   dependency on the module.  */
		struct obj_symbol *sym;
		char *name;

		/* GPL licensed modules can use symbols exported with
		 * EXPORT_SYMBOL_GPL, so ignore any GPLONLY_ prefix on the
		 * exported names.  Non-GPL modules never see any GPLONLY_
		 * symbols so they cannot fudge it by adding the prefix on
		 * their references.
		 */
		if (strncmp((char *)s->name, "GPLONLY_", 8) == 0) {
#ifdef CONFIG_FEATURE_CHECK_TAINTED_MODULE
			if (gpl)
				((char *)s->name) += 8;
			else
#endif
				continue;
		}
		name = (char *)s->name;

#ifdef SYMBOL_PREFIX
		/* Prepend SYMBOL_PREFIX to the symbol's name (the
		   kernel exports `C names', but module object files
		   reference `linker names').  */
		size_t extra = sizeof SYMBOL_PREFIX;
		size_t name_size = strlen (name) + extra;
		if (name_size > name_alloced_size) {
			name_alloced_size = name_size * 2;
			name_buf = alloca (name_alloced_size);
		}
		strcpy (name_buf, SYMBOL_PREFIX);
		strcpy (name_buf + extra - 1, name);
		name = name_buf;
#endif /* SYMBOL_PREFIX */

		sym = obj_find_symbol(f, name);
		if (sym && !(ELFW(ST_BIND) (sym->info) == STB_LOCAL)) {
#ifdef SYMBOL_PREFIX
			/* Put NAME_BUF into more permanent storage.  */
			name = xmalloc (name_size);
			strcpy (name, name_buf);
#endif
			sym = obj_add_symbol(f, name, -1,
					ELFW(ST_INFO) (STB_GLOBAL,
						STT_NOTYPE),
					idx, s->value, 0);
			/* Did our symbol just get installed?  If so, mark the
			   module as "used".  */
			if (sym->secidx == idx)
				used = 1;
		}
	}

	return used;
}

static void add_kernel_symbols(struct obj_file *f)
{
	struct external_module *m;
	int i, nused = 0;

	/* Add module symbols first.  */

	for (i = 0, m = ext_modules; i < n_ext_modules; ++i, ++m)
		if (m->nsyms
				&& add_symbols_from(f, SHN_HIRESERVE + 2 + i, m->syms,
					m->nsyms)) m->used = 1, ++nused;

	n_ext_modules_used = nused;

	/* And finally the symbols from the kernel proper.  */

	if (nksyms)
		add_symbols_from(f, SHN_HIRESERVE + 1, ksyms, nksyms);
}

static char *get_modinfo_value(struct obj_file *f, const char *key)
{
	struct obj_section *sec;
	char *p, *v, *n, *ep;
	size_t klen = strlen(key);

	sec = obj_find_section(f, ".modinfo");
	if (sec == NULL)
		return NULL;
	p = sec->contents;
	ep = p + sec->header.sh_size;
	while (p < ep) {
		v = strchr(p, '=');
		n = strchr(p, '\0');
		if (v) {
			if (p + klen == v && strncmp(p, key, klen) == 0)
				return v + 1;
		} else {
			if (p + klen == n && strcmp(p, key) == 0)
				return n;
		}
		p = n + 1;
	}

	return NULL;
}


/*======================================================================*/
/* Functions relating to module loading in pre 2.1 kernels.  */

static int
old_process_module_arguments(struct obj_file *f, int argc, char **argv)
{
	while (argc > 0) {
		char *p, *q;
		struct obj_symbol *sym;
		int *loc;

		p = *argv;
		if ((q = strchr(p, '=')) == NULL) {
			argc--;
			continue;
		}
		*q++ = '\0';

		sym = obj_find_symbol(f, p);

		/* Also check that the parameter was not resolved from the kernel.  */
		if (sym == NULL || sym->secidx > SHN_HIRESERVE) {
			bb_error_msg("symbol for parameter %s not found", p);
			return 0;
		}

		loc = (int *) (f->sections[sym->secidx]->contents + sym->value);

		/* Do C quoting if we begin with a ".  */
		if (*q == '"') {
			char *r, *str;

			str = alloca(strlen(q));
			for (r = str, q++; *q != '"'; ++q, ++r) {
				if (*q == '\0') {
					bb_error_msg("improperly terminated string argument for %s", p);
					return 0;
				} else if (*q == '\\')
					switch (*++q) {
						case 'a':
							*r = '\a';
							break;
						case 'b':
							*r = '\b';
							break;
						case 'e':
							*r = '\033';
							break;
						case 'f':
							*r = '\f';
							break;
						case 'n':
							*r = '\n';
							break;
						case 'r':
							*r = '\r';
							break;
						case 't':
							*r = '\t';
							break;

						case '0':
						case '1':
						case '2':
						case '3':
						case '4':
						case '5':
						case '6':
						case '7':
							{
								int c = *q - '0';
								if (q[1] >= '0' && q[1] <= '7') {
									c = (c * 8) + *++q - '0';
									if (q[1] >= '0' && q[1] <= '7')
										c = (c * 8) + *++q - '0';
								}
								*r = c;
							}
							break;

						default:
							*r = *q;
							break;
					} else
						*r = *q;
			}
			*r = '\0';
			obj_string_patch(f, sym->secidx, sym->value, str);
		} else if (*q >= '0' && *q <= '9') {
			do
				*loc++ = strtoul(q, &q, 0);
			while (*q++ == ',');
		} else {
			char *contents = f->sections[sym->secidx]->contents;
			char *myloc = contents + sym->value;
			char *r;			/* To search for commas */

			/* Break the string with comas */
			while ((r = strchr(q, ',')) != (char *) NULL) {
				*r++ = '\0';
				obj_string_patch(f, sym->secidx, myloc - contents, q);
				myloc += sizeof(char *);
				q = r;
			}

			/* last part */
			obj_string_patch(f, sym->secidx, myloc - contents, q);
		}

		argc--, argv++;
	}

	return 1;
}

#ifdef CONFIG_FEATURE_INSMOD_VERSION_CHECKING
static int old_is_module_checksummed(struct obj_file *f)
{
	return obj_find_symbol(f, "Using_Versions") != NULL;
}
/* Get the module's kernel version in the canonical integer form.  */

static int
old_get_module_version(struct obj_file *f, char str[STRVERSIONLEN])
{
	struct obj_symbol *sym;
	char *p, *q;
	int a, b, c;

	sym = obj_find_symbol(f, "kernel_version");
	if (sym == NULL)
		return -1;

	p = f->sections[sym->secidx]->contents + sym->value;
	safe_strncpy(str, p, STRVERSIONLEN);

	a = strtoul(p, &p, 10);
	if (*p != '.')
		return -1;
	b = strtoul(p + 1, &p, 10);
	if (*p != '.')
		return -1;
	c = strtoul(p + 1, &q, 10);
	if (p + 1 == q)
		return -1;

	return a << 16 | b << 8 | c;
}

#endif   /* CONFIG_FEATURE_INSMOD_VERSION_CHECKING */

#ifdef CONFIG_FEATURE_2_2_MODULES

/* Fetch all the symbols and divvy them up as appropriate for the modules.  */

static int old_get_kernel_symbols(const char *m_name)
{
	struct old_kernel_sym *ks, *k;
	struct new_module_symbol *s;
	struct external_module *mod;
	int nks, nms, nmod, i;

	nks = get_kernel_syms(NULL);
	if (nks <= 0) {
		if (nks)
			bb_perror_msg("get_kernel_syms: %s", m_name);
		else
			bb_error_msg("No kernel symbols");
		return 0;
	}

	ks = k = xmalloc(nks * sizeof(*ks));

	if (get_kernel_syms(ks) != nks) {
		perror("inconsistency with get_kernel_syms -- is someone else "
				"playing with modules?");
		free(ks);
		return 0;
	}

	/* Collect the module information.  */

	mod = NULL;
	nmod = -1;

	while (k->name[0] == '#' && k->name[1]) {
		struct old_kernel_sym *k2;

		/* Find out how many symbols this module has.  */
		for (k2 = k + 1; k2->name[0] != '#'; ++k2)
			continue;
		nms = k2 - k - 1;

		mod = xrealloc(mod, (++nmod + 1) * sizeof(*mod));
		mod[nmod].name = k->name + 1;
		mod[nmod].addr = k->value;
		mod[nmod].used = 0;
		mod[nmod].nsyms = nms;
		mod[nmod].syms = s = (nms ? xmalloc(nms * sizeof(*s)) : NULL);

		for (i = 0, ++k; i < nms; ++i, ++s, ++k) {
			s->name = (unsigned long) k->name;
			s->value = k->value;
		}

		k = k2;
	}

	ext_modules = mod;
	n_ext_modules = nmod + 1;

	/* Now collect the symbols for the kernel proper.  */

	if (k->name[0] == '#')
		++k;

	nksyms = nms = nks - (k - ks);
	ksyms = s = (nms ? xmalloc(nms * sizeof(*s)) : NULL);

	for (i = 0; i < nms; ++i, ++s, ++k) {
		s->name = (unsigned long) k->name;
		s->value = k->value;
	}

	return 1;
}

/* Return the kernel symbol checksum version, or zero if not used.  */

static int old_is_kernel_checksummed(void)
{
	/* Using_Versions is the first symbol.  */
	if (nksyms > 0
			&& strcmp((char *) ksyms[0].name,
				"Using_Versions") == 0) return ksyms[0].value;
	else
		return 0;
}


static int old_create_mod_use_count(struct obj_file *f)
{
	struct obj_section *sec;

	sec = obj_create_alloced_section_first(f, ".moduse", sizeof(long),
			sizeof(long));

	obj_add_symbol(f, "mod_use_count_", -1,
			ELFW(ST_INFO) (STB_LOCAL, STT_OBJECT), sec->idx, 0,
			sizeof(long));

	return 1;
}

static int
old_init_module(const char *m_name, struct obj_file *f,
				unsigned long m_size)
{
	char *image;
	struct old_mod_routines routines;
	struct old_symbol_table *symtab;
	int ret;

	/* Create the symbol table */
	{
		int nsyms = 0, strsize = 0, total;

		/* Size things first... */
		if (flag_export) {
			int i;
			for (i = 0; i < HASH_BUCKETS; ++i) {
				struct obj_symbol *sym;
				for (sym = f->symtab[i]; sym; sym = sym->next)
					if (ELFW(ST_BIND) (sym->info) != STB_LOCAL
							&& sym->secidx <= SHN_HIRESERVE)
					{
						sym->ksymidx = nsyms++;
						strsize += strlen(sym->name) + 1;
					}
			}
		}

		total = (sizeof(struct old_symbol_table)
				+ nsyms * sizeof(struct old_module_symbol)
				+ n_ext_modules_used * sizeof(struct old_module_ref)
				+ strsize);
		symtab = xmalloc(total);
		symtab->size = total;
		symtab->n_symbols = nsyms;
		symtab->n_refs = n_ext_modules_used;

		if (flag_export && nsyms) {
			struct old_module_symbol *ksym;
			char *str;
			int i;

			ksym = symtab->symbol;
			str = ((char *) ksym + nsyms * sizeof(struct old_module_symbol)
					+ n_ext_modules_used * sizeof(struct old_module_ref));

			for (i = 0; i < HASH_BUCKETS; ++i) {
				struct obj_symbol *sym;
				for (sym = f->symtab[i]; sym; sym = sym->next)
					if (sym->ksymidx >= 0) {
						ksym->addr = obj_symbol_final_value(f, sym);
						ksym->name =
							(unsigned long) str - (unsigned long) symtab;

						strcpy(str, sym->name);
						str += strlen(sym->name) + 1;
						ksym++;
					}
			}
		}

		if (n_ext_modules_used) {
			struct old_module_ref *ref;
			int i;

			ref = (struct old_module_ref *)
				((char *) symtab->symbol + nsyms * sizeof(struct old_module_symbol));

			for (i = 0; i < n_ext_modules; ++i)
				if (ext_modules[i].used)
					ref++->module = ext_modules[i].addr;
		}
	}

	/* Fill in routines.  */

	routines.init =
		obj_symbol_final_value(f, obj_find_symbol(f, SPFX "init_module"));
	routines.cleanup =
		obj_symbol_final_value(f, obj_find_symbol(f, SPFX "cleanup_module"));

	/* Whew!  All of the initialization is complete.  Collect the final
	   module image and give it to the kernel.  */

	image = xmalloc(m_size);
	obj_create_image(f, image);

	/* image holds the complete relocated module, accounting correctly for
	   mod_use_count.  However the old module kernel support assume that
	   it is receiving something which does not contain mod_use_count.  */
	ret = old_sys_init_module(m_name, image + sizeof(long),
			m_size | (flag_autoclean ? OLD_MOD_AUTOCLEAN
				: 0), &routines, symtab);
	if (ret)
		bb_perror_msg("init_module: %s", m_name);

	free(image);
	free(symtab);

	return ret == 0;
}

#else

#define old_create_mod_use_count(x) TRUE
#define old_init_module(x, y, z) TRUE

#endif							/* CONFIG_FEATURE_2_2_MODULES */



/*======================================================================*/
/* Functions relating to module loading after 2.1.18.  */

static int
new_process_module_arguments(struct obj_file *f, int argc, char **argv)
{
	while (argc > 0) {
		char *p, *q, *key, *sym_name;
		struct obj_symbol *sym;
		char *contents, *loc;
		int min, max, n;

		p = *argv;
		if ((q = strchr(p, '=')) == NULL) {
			argc--;
			continue;
		}

		key = alloca(q - p + 6);
		memcpy(key, "parm_", 5);
		memcpy(key + 5, p, q - p);
		key[q - p + 5] = 0;

		p = get_modinfo_value(f, key);
		key += 5;
		if (p == NULL) {
			bb_error_msg("invalid parameter %s", key);
			return 0;
		}

#ifdef SYMBOL_PREFIX
		sym_name = alloca (strlen (key) + sizeof SYMBOL_PREFIX);
		strcpy (sym_name, SYMBOL_PREFIX);
		strcat (sym_name, key);
#else
		sym_name = key;
#endif
		sym = obj_find_symbol(f, sym_name);

		/* Also check that the parameter was not resolved from the kernel.  */
		if (sym == NULL || sym->secidx > SHN_HIRESERVE) {
			bb_error_msg("symbol for parameter %s not found", key);
			return 0;
		}

		if (isdigit(*p)) {
			min = strtoul(p, &p, 10);
			if (*p == '-')
				max = strtoul(p + 1, &p, 10);
			else
				max = min;
		} else
			min = max = 1;

		contents = f->sections[sym->secidx]->contents;
		loc = contents + sym->value;
		n = (*++q != '\0');

		while (1) {
			if ((*p == 's') || (*p == 'c')) {
				char *str;

				/* Do C quoting if we begin with a ", else slurp the lot.  */
				if (*q == '"') {
					char *r;

					str = alloca(strlen(q));
					for (r = str, q++; *q != '"'; ++q, ++r) {
						if (*q == '\0') {
							bb_error_msg("improperly terminated string argument for %s",
									key);
							return 0;
						} else if (*q == '\\')
							switch (*++q) {
								case 'a':
									*r = '\a';
									break;
								case 'b':
									*r = '\b';
									break;
								case 'e':
									*r = '\033';
									break;
								case 'f':
									*r = '\f';
									break;
								case 'n':
									*r = '\n';
									break;
								case 'r':
									*r = '\r';
									break;
								case 't':
									*r = '\t';
									break;

								case '0':
								case '1':
								case '2':
								case '3':
								case '4':
								case '5':
								case '6':
								case '7':
									{
										int c = *q - '0';
										if (q[1] >= '0' && q[1] <= '7') {
											c = (c * 8) + *++q - '0';
											if (q[1] >= '0' && q[1] <= '7')
												c = (c * 8) + *++q - '0';
										}
										*r = c;
									}
									break;

								default:
									*r = *q;
									break;
							} else
								*r = *q;
					}
					*r = '\0';
					++q;
				} else {
					char *r;

					/* In this case, the string is not quoted. We will break
					   it using the coma (like for ints). If the user wants to
					   include comas in a string, he just has to quote it */

					/* Search the next coma */
					r = strchr(q, ',');

					/* Found ? */
					if (r != (char *) NULL) {
						/* Recopy the current field */
						str = alloca(r - q + 1);
						memcpy(str, q, r - q);

						/* I don't know if it is usefull, as the previous case
						   doesn't null terminate the string ??? */
						str[r - q] = '\0';

						/* Keep next fields */
						q = r;
					} else {
						/* last string */
						str = q;
						q = "";
					}
				}

				if (*p == 's') {
					/* Normal string */
					obj_string_patch(f, sym->secidx, loc - contents, str);
					loc += tgt_sizeof_char_p;
				} else {
					/* Array of chars (in fact, matrix !) */
					unsigned long charssize;	/* size of each member */

					/* Get the size of each member */
					/* Probably we should do that outside the loop ? */
					if (!isdigit(*(p + 1))) {
						bb_error_msg("parameter type 'c' for %s must be followed by"
								" the maximum size", key);
						return 0;
					}
					charssize = strtoul(p + 1, (char **) NULL, 10);

					/* Check length */
					if (strlen(str) >= charssize) {
						bb_error_msg("string too long for %s (max %ld)", key,
								charssize - 1);
						return 0;
					}

					/* Copy to location */
					strcpy((char *) loc, str);
					loc += charssize;
				}
			} else {
				long v = strtoul(q, &q, 0);
				switch (*p) {
					case 'b':
						*loc++ = v;
						break;
					case 'h':
						*(short *) loc = v;
						loc += tgt_sizeof_short;
						break;
					case 'i':
						*(int *) loc = v;
						loc += tgt_sizeof_int;
						break;
					case 'l':
						*(long *) loc = v;
						loc += tgt_sizeof_long;
						break;

					default:
						bb_error_msg("unknown parameter type '%c' for %s", *p, key);
						return 0;
				}
			}

retry_end_of_value:
			switch (*q) {
				case '\0':
					goto end_of_arg;

				case ' ':
				case '\t':
				case '\n':
				case '\r':
					++q;
					goto retry_end_of_value;

				case ',':
					if (++n > max) {
						bb_error_msg("too many values for %s (max %d)", key, max);
						return 0;
					}
					++q;
					break;

				default:
					bb_error_msg("invalid argument syntax for %s", key);
					return 0;
			}
		}

end_of_arg:
		if (n < min) {
			bb_error_msg("too few values for %s (min %d)", key, min);
			return 0;
		}

		argc--, argv++;
	}

	return 1;
}

#ifdef CONFIG_FEATURE_INSMOD_VERSION_CHECKING
static int new_is_module_checksummed(struct obj_file *f)
{
	const char *p = get_modinfo_value(f, "using_checksums");
	if (p)
		return atoi(p);
	else
		return 0;
}

/* Get the module's kernel version in the canonical integer form.  */

static int
new_get_module_version(struct obj_file *f, char str[STRVERSIONLEN])
{
	char *p, *q;
	int a, b, c;

	p = get_modinfo_value(f, "kernel_version");
	if (p == NULL)
		return -1;
	safe_strncpy(str, p, STRVERSIONLEN);

	a = strtoul(p, &p, 10);
	if (*p != '.')
		return -1;
	b = strtoul(p + 1, &p, 10);
	if (*p != '.')
		return -1;
	c = strtoul(p + 1, &q, 10);
	if (p + 1 == q)
		return -1;

	return a << 16 | b << 8 | c;
}

#endif   /* CONFIG_FEATURE_INSMOD_VERSION_CHECKING */


#ifdef CONFIG_FEATURE_2_4_MODULES

/* Fetch the loaded modules, and all currently exported symbols.  */

static int new_get_kernel_symbols(void)
{
	char *module_names, *mn;
	struct external_module *modules, *m;
	struct new_module_symbol *syms, *s;
	size_t ret, bufsize, nmod, nsyms, i, j;

	/* Collect the loaded modules.  */

	module_names = xmalloc(bufsize = 256);
retry_modules_load:
	if (query_module(NULL, QM_MODULES, module_names, bufsize, &ret)) {
		if (errno == ENOSPC && bufsize < ret) {
			module_names = xrealloc(module_names, bufsize = ret);
			goto retry_modules_load;
		}
		bb_perror_msg("QM_MODULES");
		return 0;
	}

	n_ext_modules = nmod = ret;

	/* Collect the modules' symbols.  */

	if (nmod){
		ext_modules = modules = xmalloc(nmod * sizeof(*modules));
		memset(modules, 0, nmod * sizeof(*modules));
		for (i = 0, mn = module_names, m = modules;
				i < nmod; ++i, ++m, mn += strlen(mn) + 1) {
			struct new_module_info info;

			if (query_module(mn, QM_INFO, &info, sizeof(info), &ret)) {
				if (errno == ENOENT) {
					/* The module was removed out from underneath us.  */
					continue;
				}
				bb_perror_msg("query_module: QM_INFO: %s", mn);
				return 0;
			}

			syms = xmalloc(bufsize = 1024);
retry_mod_sym_load:
			if (query_module(mn, QM_SYMBOLS, syms, bufsize, &ret)) {
				switch (errno) {
					case ENOSPC:
						syms = xrealloc(syms, bufsize = ret);
						goto retry_mod_sym_load;
					case ENOENT:
						/* The module was removed out from underneath us.  */
						continue;
					default:
						bb_perror_msg("query_module: QM_SYMBOLS: %s", mn);
						return 0;
				}
			}
			nsyms = ret;

			m->name = mn;
			m->addr = info.addr;
			m->nsyms = nsyms;
			m->syms = syms;

			for (j = 0, s = syms; j < nsyms; ++j, ++s) {
				s->name += (unsigned long) syms;
			}
		}
	}

	/* Collect the kernel's symbols.  */

	syms = xmalloc(bufsize = 16 * 1024);
retry_kern_sym_load:
	if (query_module(NULL, QM_SYMBOLS, syms, bufsize, &ret)) {
		if (errno == ENOSPC && bufsize < ret) {
			syms = xrealloc(syms, bufsize = ret);
			goto retry_kern_sym_load;
		}
		bb_perror_msg("kernel: QM_SYMBOLS");
		return 0;
	}
	nksyms = nsyms = ret;
	ksyms = syms;

	for (j = 0, s = syms; j < nsyms; ++j, ++s) {
		s->name += (unsigned long) syms;
	}
	return 1;
}


/* Return the kernel symbol checksum version, or zero if not used.  */

static int new_is_kernel_checksummed(void)
{
	struct new_module_symbol *s;
	size_t i;

	/* Using_Versions is not the first symbol, but it should be in there.  */

	for (i = 0, s = ksyms; i < nksyms; ++i, ++s)
		if (strcmp((char *) s->name, "Using_Versions") == 0)
			return s->value;

	return 0;
}


static int new_create_this_module(struct obj_file *f, const char *m_name)
{
	struct obj_section *sec;

	sec = obj_create_alloced_section_first(f, ".this", tgt_sizeof_long,
			sizeof(struct new_module));
	memset(sec->contents, 0, sizeof(struct new_module));

	obj_add_symbol(f, SPFX "__this_module", -1,
			ELFW(ST_INFO) (STB_LOCAL, STT_OBJECT), sec->idx, 0,
			sizeof(struct new_module));

	obj_string_patch(f, sec->idx, offsetof(struct new_module, name),
			m_name);

	return 1;
}

#ifdef CONFIG_FEATURE_INSMOD_KSYMOOPS_SYMBOLS
/* add an entry to the __ksymtab section, creating it if necessary */
static void new_add_ksymtab(struct obj_file *f, struct obj_symbol *sym)
{
	struct obj_section *sec;
	ElfW(Addr) ofs;

	/* ensure __ksymtab is allocated, EXPORT_NOSYMBOLS creates a non-alloc section.
	 * If __ksymtab is defined but not marked alloc, x out the first character
	 * (no obj_delete routine) and create a new __ksymtab with the correct
	 * characteristics.
	 */
	sec = obj_find_section(f, "__ksymtab");
	if (sec && !(sec->header.sh_flags & SHF_ALLOC)) {
		*((char *)(sec->name)) = 'x';	/* override const */
		sec = NULL;
	}
	if (!sec)
		sec = obj_create_alloced_section(f, "__ksymtab",
				tgt_sizeof_void_p, 0);
	if (!sec)
		return;
	sec->header.sh_flags |= SHF_ALLOC;
	sec->header.sh_addralign = tgt_sizeof_void_p;	/* Empty section might
													   be byte-aligned */
	ofs = sec->header.sh_size;
	obj_symbol_patch(f, sec->idx, ofs, sym);
	obj_string_patch(f, sec->idx, ofs + tgt_sizeof_void_p, sym->name);
	obj_extend_section(sec, 2 * tgt_sizeof_char_p);
}
#endif /* CONFIG_FEATURE_INSMOD_KSYMOOPS_SYMBOLS */

static int new_create_module_ksymtab(struct obj_file *f)
{
	struct obj_section *sec;
	int i;

	/* We must always add the module references.  */

	if (n_ext_modules_used) {
		struct new_module_ref *dep;
		struct obj_symbol *tm;

		sec = obj_create_alloced_section(f, ".kmodtab", tgt_sizeof_void_p,
				(sizeof(struct new_module_ref)
				 * n_ext_modules_used));
		if (!sec)
			return 0;

		tm = obj_find_symbol(f, SPFX "__this_module");
		dep = (struct new_module_ref *) sec->contents;
		for (i = 0; i < n_ext_modules; ++i)
			if (ext_modules[i].used) {
				dep->dep = ext_modules[i].addr;
				obj_symbol_patch(f, sec->idx,
						(char *) &dep->ref - sec->contents, tm);
				dep->next_ref = 0;
				++dep;
			}
	}

	if (flag_export && !obj_find_section(f, "__ksymtab")) {
		size_t nsyms;
		int *loaded;

		sec =
			obj_create_alloced_section(f, "__ksymtab", tgt_sizeof_void_p,
					0);

		/* We don't want to export symbols residing in sections that
		   aren't loaded.  There are a number of these created so that
		   we make sure certain module options don't appear twice.  */

		loaded = alloca(sizeof(int) * (i = f->header.e_shnum));
		while (--i >= 0)
			loaded[i] = (f->sections[i]->header.sh_flags & SHF_ALLOC) != 0;

		for (nsyms = i = 0; i < HASH_BUCKETS; ++i) {
			struct obj_symbol *sym;
			for (sym = f->symtab[i]; sym; sym = sym->next)
				if (ELFW(ST_BIND) (sym->info) != STB_LOCAL
						&& sym->secidx <= SHN_HIRESERVE
						&& (sym->secidx >= SHN_LORESERVE
							|| loaded[sym->secidx])) {
					ElfW(Addr) ofs = nsyms * 2 * tgt_sizeof_void_p;

					obj_symbol_patch(f, sec->idx, ofs, sym);
					obj_string_patch(f, sec->idx, ofs + tgt_sizeof_void_p,
							sym->name);

					nsyms++;
				}
		}

		obj_extend_section(sec, nsyms * 2 * tgt_sizeof_char_p);
	}

	return 1;
}


static int
new_init_module(const char *m_name, struct obj_file *f,
				unsigned long m_size)
{
	struct new_module *module;
	struct obj_section *sec;
	void *image;
	int ret;
	tgt_long m_addr;

	sec = obj_find_section(f, ".this");
	if (!sec || !sec->contents) {
		bb_perror_msg_and_die("corrupt module %s?",m_name);
	}
	module = (struct new_module *) sec->contents;
	m_addr = sec->header.sh_addr;

	module->size_of_struct = sizeof(*module);
	module->size = m_size;
	module->flags = flag_autoclean ? NEW_MOD_AUTOCLEAN : 0;

	sec = obj_find_section(f, "__ksymtab");
	if (sec && sec->header.sh_size) {
		module->syms = sec->header.sh_addr;
		module->nsyms = sec->header.sh_size / (2 * tgt_sizeof_char_p);
	}

	if (n_ext_modules_used) {
		sec = obj_find_section(f, ".kmodtab");
		module->deps = sec->header.sh_addr;
		module->ndeps = n_ext_modules_used;
	}

	module->init =
		obj_symbol_final_value(f, obj_find_symbol(f, SPFX "init_module"));
	module->cleanup =
		obj_symbol_final_value(f, obj_find_symbol(f, SPFX "cleanup_module"));

	sec = obj_find_section(f, "__ex_table");
	if (sec) {
		module->ex_table_start = sec->header.sh_addr;
		module->ex_table_end = sec->header.sh_addr + sec->header.sh_size;
	}

	sec = obj_find_section(f, ".text.init");
	if (sec) {
		module->runsize = sec->header.sh_addr - m_addr;
	}
	sec = obj_find_section(f, ".data.init");
	if (sec) {
		if (!module->runsize ||
				module->runsize > sec->header.sh_addr - m_addr)
			module->runsize = sec->header.sh_addr - m_addr;
	}
	sec = obj_find_section(f, ARCHDATA_SEC_NAME);
	if (sec && sec->header.sh_size) {
		module->archdata_start = (void*)sec->header.sh_addr;
		module->archdata_end = module->archdata_start + sec->header.sh_size;
	}
	sec = obj_find_section(f, KALLSYMS_SEC_NAME);
	if (sec && sec->header.sh_size) {
		module->kallsyms_start = (void*)sec->header.sh_addr;
		module->kallsyms_end = module->kallsyms_start + sec->header.sh_size;
	}

	if (!arch_init_module(f, module))
		return 0;

	/* Whew!  All of the initialization is complete.  Collect the final
	   module image and give it to the kernel.  */

	image = xmalloc(m_size);
	obj_create_image(f, image);

	ret = new_sys_init_module(m_name, (struct new_module *) image);
	if (ret)
		bb_perror_msg("init_module: %s", m_name);

	free(image);

	return ret == 0;
}

#else

#define new_init_module(x, y, z) TRUE
#define new_create_this_module(x, y) 0
#define new_add_ksymtab(x, y) -1
#define new_create_module_ksymtab(x)
#define query_module(v, w, x, y, z) -1

#endif							/* CONFIG_FEATURE_2_4_MODULES */


/*======================================================================*/

static int
obj_string_patch(struct obj_file *f, int secidx, ElfW(Addr) offset,
				 const char *string)
{
	struct obj_string_patch *p;
	struct obj_section *strsec;
	size_t len = strlen(string) + 1;
	char *loc;

	p = xmalloc(sizeof(*p));
	p->next = f->string_patches;
	p->reloc_secidx = secidx;
	p->reloc_offset = offset;
	f->string_patches = p;

	strsec = obj_find_section(f, ".kstrtab");
	if (strsec == NULL) {
		strsec = obj_create_alloced_section(f, ".kstrtab", 1, len);
		p->string_offset = 0;
		loc = strsec->contents;
	} else {
		p->string_offset = strsec->header.sh_size;
		loc = obj_extend_section(strsec, len);
	}
	memcpy(loc, string, len);

	return 1;
}

#ifdef CONFIG_FEATURE_2_4_MODULES
static int
obj_symbol_patch(struct obj_file *f, int secidx, ElfW(Addr) offset,
				 struct obj_symbol *sym)
{
	struct obj_symbol_patch *p;

	p = xmalloc(sizeof(*p));
	p->next = f->symbol_patches;
	p->reloc_secidx = secidx;
	p->reloc_offset = offset;
	p->sym = sym;
	f->symbol_patches = p;

	return 1;
}
#endif

static int obj_check_undefineds(struct obj_file *f)
{
	unsigned long i;
	int ret = 1;

	for (i = 0; i < HASH_BUCKETS; ++i) {
		struct obj_symbol *sym;
		for (sym = f->symtab[i]; sym; sym = sym->next)
			if (sym->secidx == SHN_UNDEF) {
				if (ELFW(ST_BIND) (sym->info) == STB_WEAK) {
					sym->secidx = SHN_ABS;
					sym->value = 0;
				} else {
					if (!flag_quiet) {
						bb_error_msg("unresolved symbol %s", sym->name);
					}
					ret = 0;
				}
			}
	}

	return ret;
}

static void obj_allocate_commons(struct obj_file *f)
{
	struct common_entry {
		struct common_entry *next;
		struct obj_symbol *sym;
	} *common_head = NULL;

	unsigned long i;

	for (i = 0; i < HASH_BUCKETS; ++i) {
		struct obj_symbol *sym;
		for (sym = f->symtab[i]; sym; sym = sym->next)
			if (sym->secidx == SHN_COMMON) {
				/* Collect all COMMON symbols and sort them by size so as to
				   minimize space wasted by alignment requirements.  */
				{
					struct common_entry **p, *n;
					for (p = &common_head; *p; p = &(*p)->next)
						if (sym->size <= (*p)->sym->size)
							break;

					n = alloca(sizeof(*n));
					n->next = *p;
					n->sym = sym;
					*p = n;
				}
			}
	}

	for (i = 1; i < f->local_symtab_size; ++i) {
		struct obj_symbol *sym = f->local_symtab[i];
		if (sym && sym->secidx == SHN_COMMON) {
			struct common_entry **p, *n;
			for (p = &common_head; *p; p = &(*p)->next)
				if (sym == (*p)->sym)
					break;
				else if (sym->size < (*p)->sym->size) {
					n = alloca(sizeof(*n));
					n->next = *p;
					n->sym = sym;
					*p = n;
					break;
				}
		}
	}

	if (common_head) {
		/* Find the bss section.  */
		for (i = 0; i < f->header.e_shnum; ++i)
			if (f->sections[i]->header.sh_type == SHT_NOBITS)
				break;

		/* If for some reason there hadn't been one, create one.  */
		if (i == f->header.e_shnum) {
			struct obj_section *sec;

			f->sections = xrealloc(f->sections, (i + 1) * sizeof(sec));
			f->sections[i] = sec = arch_new_section();
			f->header.e_shnum = i + 1;

			memset(sec, 0, sizeof(*sec));
			sec->header.sh_type = SHT_PROGBITS;
			sec->header.sh_flags = SHF_WRITE | SHF_ALLOC;
			sec->name = ".bss";
			sec->idx = i;
		}

		/* Allocate the COMMONS.  */
		{
			ElfW(Addr) bss_size = f->sections[i]->header.sh_size;
			ElfW(Addr) max_align = f->sections[i]->header.sh_addralign;
			struct common_entry *c;

			for (c = common_head; c; c = c->next) {
				ElfW(Addr) align = c->sym->value;

				if (align > max_align)
					max_align = align;
				if (bss_size & (align - 1))
					bss_size = (bss_size | (align - 1)) + 1;

				c->sym->secidx = i;
				c->sym->value = bss_size;

				bss_size += c->sym->size;
			}

			f->sections[i]->header.sh_size = bss_size;
			f->sections[i]->header.sh_addralign = max_align;
		}
	}

	/* For the sake of patch relocation and parameter initialization,
	   allocate zeroed data for NOBITS sections now.  Note that after
	   this we cannot assume NOBITS are really empty.  */
	for (i = 0; i < f->header.e_shnum; ++i) {
		struct obj_section *s = f->sections[i];
		if (s->header.sh_type == SHT_NOBITS) {
			if (s->header.sh_size != 0)
				s->contents = memset(xmalloc(s->header.sh_size),
						0, s->header.sh_size);
			else
				s->contents = NULL;

			s->header.sh_type = SHT_PROGBITS;
		}
	}
}

static unsigned long obj_load_size(struct obj_file *f)
{
	unsigned long dot = 0;
	struct obj_section *sec;

	/* Finalize the positions of the sections relative to one another.  */

	for (sec = f->load_order; sec; sec = sec->load_next) {
		ElfW(Addr) align;

		align = sec->header.sh_addralign;
		if (align && (dot & (align - 1)))
			dot = (dot | (align - 1)) + 1;

		sec->header.sh_addr = dot;
		dot += sec->header.sh_size;
	}

	return dot;
}

static int obj_relocate(struct obj_file *f, ElfW(Addr) base)
{
	int i, n = f->header.e_shnum;
	int ret = 1;

	/* Finalize the addresses of the sections.  */

	f->baseaddr = base;
	for (i = 0; i < n; ++i)
		f->sections[i]->header.sh_addr += base;

	/* And iterate over all of the relocations.  */

	for (i = 0; i < n; ++i) {
		struct obj_section *relsec, *symsec, *targsec, *strsec;
		ElfW(RelM) * rel, *relend;
		ElfW(Sym) * symtab;
		const char *strtab;

		relsec = f->sections[i];
		if (relsec->header.sh_type != SHT_RELM)
			continue;

		symsec = f->sections[relsec->header.sh_link];
		targsec = f->sections[relsec->header.sh_info];
		strsec = f->sections[symsec->header.sh_link];

		rel = (ElfW(RelM) *) relsec->contents;
		relend = rel + (relsec->header.sh_size / sizeof(ElfW(RelM)));
		symtab = (ElfW(Sym) *) symsec->contents;
		strtab = (const char *) strsec->contents;

		for (; rel < relend; ++rel) {
			ElfW(Addr) value = 0;
			struct obj_symbol *intsym = NULL;
			unsigned long symndx;
			ElfW(Sym) * extsym = 0;
			const char *errmsg;

			/* Attempt to find a value to use for this relocation.  */

			symndx = ELFW(R_SYM) (rel->r_info);
			if (symndx) {
				/* Note we've already checked for undefined symbols.  */

				extsym = &symtab[symndx];
				if (ELFW(ST_BIND) (extsym->st_info) == STB_LOCAL) {
					/* Local symbols we look up in the local table to be sure
					   we get the one that is really intended.  */
					intsym = f->local_symtab[symndx];
				} else {
					/* Others we look up in the hash table.  */
					const char *name;
					if (extsym->st_name)
						name = strtab + extsym->st_name;
					else
						name = f->sections[extsym->st_shndx]->name;
					intsym = obj_find_symbol(f, name);
				}

				value = obj_symbol_final_value(f, intsym);
				intsym->referenced = 1;
			}
#if SHT_RELM == SHT_RELA
#if defined(__alpha__) && defined(AXP_BROKEN_GAS)
			/* Work around a nasty GAS bug, that is fixed as of 2.7.0.9.  */
			if (!extsym || !extsym->st_name ||
					ELFW(ST_BIND) (extsym->st_info) != STB_LOCAL)
#endif
				value += rel->r_addend;
#endif

			/* Do it! */
			switch (arch_apply_relocation
					(f, targsec, symsec, intsym, rel, value)) {
				case obj_reloc_ok:
					break;

				case obj_reloc_overflow:
					errmsg = "Relocation overflow";
					goto bad_reloc;
				case obj_reloc_dangerous:
					errmsg = "Dangerous relocation";
					goto bad_reloc;
				case obj_reloc_unhandled:
					errmsg = "Unhandled relocation";
bad_reloc:
					if (extsym) {
						bb_error_msg("%s of type %ld for %s", errmsg,
								(long) ELFW(R_TYPE) (rel->r_info),
								strtab + extsym->st_name);
					} else {
						bb_error_msg("%s of type %ld", errmsg,
								(long) ELFW(R_TYPE) (rel->r_info));
					}
					ret = 0;
					break;
			}
		}
	}

	/* Finally, take care of the patches.  */

	if (f->string_patches) {
		struct obj_string_patch *p;
		struct obj_section *strsec;
		ElfW(Addr) strsec_base;
		strsec = obj_find_section(f, ".kstrtab");
		strsec_base = strsec->header.sh_addr;

		for (p = f->string_patches; p; p = p->next) {
			struct obj_section *targsec = f->sections[p->reloc_secidx];
			*(ElfW(Addr) *) (targsec->contents + p->reloc_offset)
				= strsec_base + p->string_offset;
		}
	}

	if (f->symbol_patches) {
		struct obj_symbol_patch *p;

		for (p = f->symbol_patches; p; p = p->next) {
			struct obj_section *targsec = f->sections[p->reloc_secidx];
			*(ElfW(Addr) *) (targsec->contents + p->reloc_offset)
				= obj_symbol_final_value(f, p->sym);
		}
	}

	return ret;
}

static int obj_create_image(struct obj_file *f, char *image)
{
	struct obj_section *sec;
	ElfW(Addr) base = f->baseaddr;

	for (sec = f->load_order; sec; sec = sec->load_next) {
		char *secimg;

		if (sec->contents == 0 || sec->header.sh_size == 0)
			continue;

		secimg = image + (sec->header.sh_addr - base);

		/* Note that we allocated data for NOBITS sections earlier.  */
		memcpy(secimg, sec->contents, sec->header.sh_size);
	}

	return 1;
}

/*======================================================================*/

static struct obj_file *obj_load(FILE * fp, int loadprogbits)
{
	struct obj_file *f;
	ElfW(Shdr) * section_headers;
	int shnum, i;
	char *shstrtab;

	/* Read the file header.  */

	f = arch_new_file();
	memset(f, 0, sizeof(*f));
	f->symbol_cmp = strcmp;
	f->symbol_hash = obj_elf_hash;
	f->load_order_search_start = &f->load_order;

	fseek(fp, 0, SEEK_SET);
	if (fread(&f->header, sizeof(f->header), 1, fp) != 1) {
		bb_perror_msg("error reading ELF header");
		return NULL;
	}

	if (f->header.e_ident[EI_MAG0] != ELFMAG0
			|| f->header.e_ident[EI_MAG1] != ELFMAG1
			|| f->header.e_ident[EI_MAG2] != ELFMAG2
			|| f->header.e_ident[EI_MAG3] != ELFMAG3) {
		bb_error_msg("not an ELF file");
		return NULL;
	}
	if (f->header.e_ident[EI_CLASS] != ELFCLASSM
			|| f->header.e_ident[EI_DATA] != ELFDATAM
			|| f->header.e_ident[EI_VERSION] != EV_CURRENT
			|| !MATCH_MACHINE(f->header.e_machine)) {
		bb_error_msg("ELF file not for this architecture");
		return NULL;
	}
	if (f->header.e_type != ET_REL) {
		bb_error_msg("ELF file not a relocatable object");
		return NULL;
	}

	/* Read the section headers.  */

	if (f->header.e_shentsize != sizeof(ElfW(Shdr))) {
		bb_error_msg("section header size mismatch: %lu != %lu",
				(unsigned long) f->header.e_shentsize,
				(unsigned long) sizeof(ElfW(Shdr)));
		return NULL;
	}

	shnum = f->header.e_shnum;
	f->sections = xmalloc(sizeof(struct obj_section *) * shnum);
	memset(f->sections, 0, sizeof(struct obj_section *) * shnum);

	section_headers = alloca(sizeof(ElfW(Shdr)) * shnum);
	fseek(fp, f->header.e_shoff, SEEK_SET);
	if (fread(section_headers, sizeof(ElfW(Shdr)), shnum, fp) != shnum) {
		bb_perror_msg("error reading ELF section headers");
		return NULL;
	}

	/* Read the section data.  */

	for (i = 0; i < shnum; ++i) {
		struct obj_section *sec;

		f->sections[i] = sec = arch_new_section();
		memset(sec, 0, sizeof(*sec));

		sec->header = section_headers[i];
		sec->idx = i;

		if(sec->header.sh_size) switch (sec->header.sh_type) {
			case SHT_NULL:
			case SHT_NOTE:
			case SHT_NOBITS:
				/* ignore */
				break;

			case SHT_PROGBITS:
#if LOADBITS
				if (!loadprogbits) {
					sec->contents = NULL;
					break;
				}
#endif
			case SHT_SYMTAB:
			case SHT_STRTAB:
			case SHT_RELM:
				if (sec->header.sh_size > 0) {
					sec->contents = xmalloc(sec->header.sh_size);
					fseek(fp, sec->header.sh_offset, SEEK_SET);
					if (fread(sec->contents, sec->header.sh_size, 1, fp) != 1) {
						bb_perror_msg("error reading ELF section data");
						return NULL;
					}
				} else {
					sec->contents = NULL;
				}
				break;

#if SHT_RELM == SHT_REL
			case SHT_RELA:
				bb_error_msg("RELA relocations not supported on this architecture");
				return NULL;
#else
			case SHT_REL:
				bb_error_msg("REL relocations not supported on this architecture");
				return NULL;
#endif

			default:
				if (sec->header.sh_type >= SHT_LOPROC) {
					/* Assume processor specific section types are debug
					   info and can safely be ignored.  If this is ever not
					   the case (Hello MIPS?), don't put ifdefs here but
					   create an arch_load_proc_section().  */
					break;
				}

				bb_error_msg("can't handle sections of type %ld",
						(long) sec->header.sh_type);
				return NULL;
		}
	}

	/* Do what sort of interpretation as needed by each section.  */

	shstrtab = f->sections[f->header.e_shstrndx]->contents;

	for (i = 0; i < shnum; ++i) {
		struct obj_section *sec = f->sections[i];
		sec->name = shstrtab + sec->header.sh_name;
	}

	for (i = 0; i < shnum; ++i) {
		struct obj_section *sec = f->sections[i];

		/* .modinfo should be contents only but gcc has no attribute for that.
		 * The kernel may have marked .modinfo as ALLOC, ignore this bit.
		 */
		if (strcmp(sec->name, ".modinfo") == 0)
			sec->header.sh_flags &= ~SHF_ALLOC;

		if (sec->header.sh_flags & SHF_ALLOC)
			obj_insert_section_load_order(f, sec);

		switch (sec->header.sh_type) {
			case SHT_SYMTAB:
				{
					unsigned long nsym, j;
					char *strtab;
					ElfW(Sym) * sym;

					if (sec->header.sh_entsize != sizeof(ElfW(Sym))) {
						bb_error_msg("symbol size mismatch: %lu != %lu",
								(unsigned long) sec->header.sh_entsize,
								(unsigned long) sizeof(ElfW(Sym)));
						return NULL;
					}

					nsym = sec->header.sh_size / sizeof(ElfW(Sym));
					strtab = f->sections[sec->header.sh_link]->contents;
					sym = (ElfW(Sym) *) sec->contents;

					/* Allocate space for a table of local symbols.  */
					j = f->local_symtab_size = sec->header.sh_info;
					f->local_symtab = xcalloc(j, sizeof(struct obj_symbol *));

					/* Insert all symbols into the hash table.  */
					for (j = 1, ++sym; j < nsym; ++j, ++sym) {
						ElfW(Addr) val = sym->st_value;
						const char *name;
						if (sym->st_name)
							name = strtab + sym->st_name;
						else if (sym->st_shndx < shnum)
							name = f->sections[sym->st_shndx]->name;
						else
							continue;

#if defined(__SH5__)
						/*
						 * For sh64 it is possible that the target of a branch
						 * requires a mode switch (32 to 16 and back again).
						 *
						 * This is implied by the lsb being set in the target
						 * address for SHmedia mode and clear for SHcompact.
						 */
						val |= sym->st_other & 4;
#endif

						obj_add_symbol(f, name, j, sym->st_info, sym->st_shndx,
								val, sym->st_size);
					}
				}
				break;

			case SHT_RELM:
				if (sec->header.sh_entsize != sizeof(ElfW(RelM))) {
					bb_error_msg("relocation entry size mismatch: %lu != %lu",
							(unsigned long) sec->header.sh_entsize,
							(unsigned long) sizeof(ElfW(RelM)));
					return NULL;
				}
				break;
				/* XXX  Relocation code from modutils-2.3.19 is not here.
				 * Why?  That's about 20 lines of code from obj/obj_load.c,
				 * which gets done in a second pass through the sections.
				 * This BusyBox insmod does similar work in obj_relocate(). */
		}
	}

	return f;
}

#ifdef CONFIG_FEATURE_INSMOD_LOADINKMEM
/*
 * load the unloaded sections directly into the memory allocated by
 * kernel for the module
 */

static int obj_load_progbits(FILE * fp, struct obj_file* f, char* imagebase)
{
	ElfW(Addr) base = f->baseaddr;
	struct obj_section* sec;

	for (sec = f->load_order; sec; sec = sec->load_next) {

		/* section already loaded? */
		if (sec->contents != NULL)
			continue;

		if (sec->header.sh_size == 0)
			continue;

		sec->contents = imagebase + (sec->header.sh_addr - base);
		fseek(fp, sec->header.sh_offset, SEEK_SET);
		if (fread(sec->contents, sec->header.sh_size, 1, fp) != 1) {
			bb_error_msg("error reading ELF section data: %s\n", strerror(errno));
			return 0;
		}

	}
	return 1;
}
#endif

static void hide_special_symbols(struct obj_file *f)
{
	static const char *const specials[] = {
		SPFX "cleanup_module",
		SPFX "init_module",
		SPFX "kernel_version",
		NULL
	};

	struct obj_symbol *sym;
	const char *const *p;

	for (p = specials; *p; ++p)
		if ((sym = obj_find_symbol(f, *p)) != NULL)
			sym->info =
				ELFW(ST_INFO) (STB_LOCAL, ELFW(ST_TYPE) (sym->info));
}


#ifdef CONFIG_FEATURE_CHECK_TAINTED_MODULE
static int obj_gpl_license(struct obj_file *f, const char **license)
{
	struct obj_section *sec;
	/* This list must match *exactly* the list of allowable licenses in
	 * linux/include/linux/module.h.  Checking for leading "GPL" will not
	 * work, somebody will use "GPL sucks, this is proprietary".
	 */
	static const char *gpl_licenses[] = {
		"GPL",
		"GPL v2",
		"GPL and additional rights",
		"Dual BSD/GPL",
		"Dual MPL/GPL",
	};

	if ((sec = obj_find_section(f, ".modinfo"))) {
		const char *value, *ptr, *endptr;
		ptr = sec->contents;
		endptr = ptr + sec->header.sh_size;
		while (ptr < endptr) {
			if ((value = strchr(ptr, '=')) && strncmp(ptr, "license", value-ptr) == 0) {
				int i;
				if (license)
					*license = value+1;
				for (i = 0; i < sizeof(gpl_licenses)/sizeof(gpl_licenses[0]); ++i) {
					if (strcmp(value+1, gpl_licenses[i]) == 0)
						return(0);
				}
				return(2);
			}
			if (strchr(ptr, '\0'))
				ptr = strchr(ptr, '\0') + 1;
			else
				ptr = endptr;
		}
	}
	return(1);
}

#define TAINT_FILENAME                  "/proc/sys/kernel/tainted"
#define TAINT_PROPRIETORY_MODULE        (1<<0)
#define TAINT_FORCED_MODULE             (1<<1)
#define TAINT_UNSAFE_SMP                (1<<2)
#define TAINT_URL						"http://www.tux.org/lkml/#export-tainted"

static void set_tainted(struct obj_file *f, int fd, char *m_name,
		int kernel_has_tainted, int taint, const char *text1, const char *text2)
{
	char buf[80];
	int oldval;
	static int first = 1;
	if (fd < 0 && !kernel_has_tainted)
		return;		/* New modutils on old kernel */
	printf("Warning: loading %s will taint the kernel: %s%s\n",
			m_name, text1, text2);
	if (first) {
		printf("  See %s for information about tainted modules\n", TAINT_URL);
		first = 0;
	}
	if (fd >= 0) {
		read(fd, buf, sizeof(buf)-1);
		buf[sizeof(buf)-1] = '\0';
		oldval = strtoul(buf, NULL, 10);
		sprintf(buf, "%d\n", oldval | taint);
		write(fd, buf, strlen(buf));
	}
}

/* Check if loading this module will taint the kernel. */
static void check_tainted_module(struct obj_file *f, char *m_name)
{
	static const char tainted_file[] = TAINT_FILENAME;
	int fd, kernel_has_tainted;
	const char *ptr;

	kernel_has_tainted = 1;
	if ((fd = open(tainted_file, O_RDWR)) < 0) {
		if (errno == ENOENT)
			kernel_has_tainted = 0;
		else if (errno == EACCES)
			kernel_has_tainted = 1;
		else {
			perror(tainted_file);
			kernel_has_tainted = 0;
		}
	}

	switch (obj_gpl_license(f, &ptr)) {
		case 0:
			break;
		case 1:
			set_tainted(f, fd, m_name, kernel_has_tainted, TAINT_PROPRIETORY_MODULE, "no license", "");
			break;
		case 2:
			/* The module has a non-GPL license so we pretend that the
			 * kernel always has a taint flag to get a warning even on
			 * kernels without the proc flag.
			 */
			set_tainted(f, fd, m_name, 1, TAINT_PROPRIETORY_MODULE, "non-GPL license - ", ptr);
			break;
		default:
			set_tainted(f, fd, m_name, 1, TAINT_PROPRIETORY_MODULE, "Unexpected return from obj_gpl_license", "");
			break;
	}

	if (flag_force_load)
		set_tainted(f, fd, m_name, 1, TAINT_FORCED_MODULE, "forced load", "");

	if (fd >= 0)
		close(fd);
}
#else /* CONFIG_FEATURE_CHECK_TAINTED_MODULE */
#define check_tainted_module(x, y) do { } while(0);
#endif /* CONFIG_FEATURE_CHECK_TAINTED_MODULE */

#ifdef CONFIG_FEATURE_INSMOD_KSYMOOPS_SYMBOLS
/* add module source, timestamp, kernel version and a symbol for the
 * start of some sections.  this info is used by ksymoops to do better
 * debugging.
 */
static int
get_module_version(struct obj_file *f, char str[STRVERSIONLEN])
{
#ifdef CONFIG_FEATURE_INSMOD_VERSION_CHECKING
	if (get_modinfo_value(f, "kernel_version") == NULL)
		return old_get_module_version(f, str);
	else
		return new_get_module_version(f, str);
#else  /* CONFIG_FEATURE_INSMOD_VERSION_CHECKING */
	strncpy(str, "???", sizeof(str));
	return -1;
#endif /* CONFIG_FEATURE_INSMOD_VERSION_CHECKING */
}

/* add module source, timestamp, kernel version and a symbol for the
 * start of some sections.  this info is used by ksymoops to do better
 * debugging.
 */
static void
add_ksymoops_symbols(struct obj_file *f, const char *filename,
				 const char *m_name)
{
	static const char symprefix[] = "__insmod_";
	struct obj_section *sec;
	struct obj_symbol *sym;
	char *name, *absolute_filename;
	char str[STRVERSIONLEN], real[PATH_MAX];
	int i, l, lm_name, lfilename, use_ksymtab, version;
	struct stat statbuf;

	static const char *section_names[] = {
		".text",
		".rodata",
		".data",
		".bss"
			".sbss"
	};

	if (realpath(filename, real)) {
		absolute_filename = bb_xstrdup(real);
	}
	else {
		int save_errno = errno;
		bb_error_msg("cannot get realpath for %s", filename);
		errno = save_errno;
		perror("");
		absolute_filename = bb_xstrdup(filename);
	}

	lm_name = strlen(m_name);
	lfilename = strlen(absolute_filename);

	/* add to ksymtab if it already exists or there is no ksymtab and other symbols
	 * are not to be exported.  otherwise leave ksymtab alone for now, the
	 * "export all symbols" compatibility code will export these symbols later.
	 */
	use_ksymtab =  obj_find_section(f, "__ksymtab") || !flag_export;

	if ((sec = obj_find_section(f, ".this"))) {
		/* tag the module header with the object name, last modified
		 * timestamp and module version.  worst case for module version
		 * is 0xffffff, decimal 16777215.  putting all three fields in
		 * one symbol is less readable but saves kernel space.
		 */
		l = sizeof(symprefix)+			/* "__insmod_" */
			lm_name+				/* module name */
			2+					/* "_O" */
			lfilename+				/* object filename */
			2+					/* "_M" */
			2*sizeof(statbuf.st_mtime)+		/* mtime in hex */
			2+					/* "_V" */
			8+					/* version in dec */
			1;					/* nul */
		name = xmalloc(l);
		if (stat(absolute_filename, &statbuf) != 0)
			statbuf.st_mtime = 0;
		version = get_module_version(f, str);	/* -1 if not found */
		snprintf(name, l, "%s%s_O%s_M%0*lX_V%d",
				symprefix, m_name, absolute_filename,
				(int)(2*sizeof(statbuf.st_mtime)), statbuf.st_mtime,
				version);
		sym = obj_add_symbol(f, name, -1,
				ELFW(ST_INFO) (STB_GLOBAL, STT_NOTYPE),
				sec->idx, sec->header.sh_addr, 0);
		if (use_ksymtab)
			new_add_ksymtab(f, sym);
	}
	free(absolute_filename);
#ifdef _NOT_SUPPORTED_
	/* record where the persistent data is going, same address as previous symbol */

	if (f->persist) {
		l = sizeof(symprefix)+		/* "__insmod_" */
			lm_name+		/* module name */
			2+			/* "_P" */
			strlen(f->persist)+	/* data store */
			1;			/* nul */
		name = xmalloc(l);
		snprintf(name, l, "%s%s_P%s",
				symprefix, m_name, f->persist);
		sym = obj_add_symbol(f, name, -1, ELFW(ST_INFO) (STB_GLOBAL, STT_NOTYPE),
				sec->idx, sec->header.sh_addr, 0);
		if (use_ksymtab)
			new_add_ksymtab(f, sym);
	}
#endif /* _NOT_SUPPORTED_ */
	/* tag the desired sections if size is non-zero */

	for (i = 0; i < sizeof(section_names)/sizeof(section_names[0]); ++i) {
		if ((sec = obj_find_section(f, section_names[i])) &&
				sec->header.sh_size) {
			l = sizeof(symprefix)+		/* "__insmod_" */
				lm_name+		/* module name */
				2+			/* "_S" */
				strlen(sec->name)+	/* section name */
				2+			/* "_L" */
				8+			/* length in dec */
				1;			/* nul */
			name = xmalloc(l);
			snprintf(name, l, "%s%s_S%s_L%ld",
					symprefix, m_name, sec->name,
					(long)sec->header.sh_size);
			sym = obj_add_symbol(f, name, -1, ELFW(ST_INFO) (STB_GLOBAL, STT_NOTYPE),
					sec->idx, sec->header.sh_addr, 0);
			if (use_ksymtab)
				new_add_ksymtab(f, sym);
		}
	}
}
#endif /* CONFIG_FEATURE_INSMOD_KSYMOOPS_SYMBOLS */

#ifdef CONFIG_FEATURE_INSMOD_LOAD_MAP
static void print_load_map(struct obj_file *f)
{
	struct obj_symbol *sym;
	struct obj_symbol **all, **p;
	struct obj_section *sec;
	int i, nsyms, *loaded;

	/* Report on the section layout.  */

	printf("Sections:       Size      %-*s  Align\n",
			(int) (2 * sizeof(void *)), "Address");

	for (sec = f->load_order; sec; sec = sec->load_next) {
		int a;
		unsigned long tmp;

		for (a = -1, tmp = sec->header.sh_addralign; tmp; ++a)
			tmp >>= 1;
		if (a == -1)
			a = 0;

		printf("%-15s %08lx  %0*lx  2**%d\n",
				sec->name,
				(long)sec->header.sh_size,
				(int) (2 * sizeof(void *)),
				(long)sec->header.sh_addr,
				a);
	}
#ifdef CONFIG_FEATURE_INSMOD_LOAD_MAP_FULL
	/* Quick reference which section indicies are loaded.  */

	loaded = alloca(sizeof(int) * (i = f->header.e_shnum));
	while (--i >= 0)
		loaded[i] = (f->sections[i]->header.sh_flags & SHF_ALLOC) != 0;

	/* Collect the symbols we'll be listing.  */

	for (nsyms = i = 0; i < HASH_BUCKETS; ++i)
		for (sym = f->symtab[i]; sym; sym = sym->next)
			if (sym->secidx <= SHN_HIRESERVE
					&& (sym->secidx >= SHN_LORESERVE || loaded[sym->secidx]))
				++nsyms;

	all = alloca(nsyms * sizeof(struct obj_symbol *));

	for (i = 0, p = all; i < HASH_BUCKETS; ++i)
		for (sym = f->symtab[i]; sym; sym = sym->next)
			if (sym->secidx <= SHN_HIRESERVE
					&& (sym->secidx >= SHN_LORESERVE || loaded[sym->secidx]))
				*p++ = sym;

	/* And list them.  */
	printf("\nSymbols:\n");
	for (p = all; p < all + nsyms; ++p) {
		char type = '?';
		unsigned long value;

		sym = *p;
		if (sym->secidx == SHN_ABS) {
			type = 'A';
			value = sym->value;
		} else if (sym->secidx == SHN_UNDEF) {
			type = 'U';
			value = 0;
		} else {
			sec = f->sections[sym->secidx];

			if (sec->header.sh_type == SHT_NOBITS)
				type = 'B';
			else if (sec->header.sh_flags & SHF_ALLOC) {
				if (sec->header.sh_flags & SHF_EXECINSTR)
					type = 'T';
				else if (sec->header.sh_flags & SHF_WRITE)
					type = 'D';
				else
					type = 'R';
			}
			value = sym->value + sec->header.sh_addr;
		}

		if (ELFW(ST_BIND) (sym->info) == STB_LOCAL)
			type = tolower(type);

		printf("%0*lx %c %s\n", (int) (2 * sizeof(void *)), value,
				type, sym->name);
	}
#endif
}

#endif

extern int insmod_main( int argc, char **argv)
{
	int opt;
	int k_crcs;
	int k_new_syscalls;
	int len;
	char *tmp, *tmp1;
	unsigned long m_size;
	ElfW(Addr) m_addr;
	struct obj_file *f;
	struct stat st;
	char *m_name = 0;
	int exit_status = EXIT_FAILURE;
	int m_has_modinfo;
#ifdef CONFIG_FEATURE_INSMOD_VERSION_CHECKING
	struct utsname uts_info;
	char m_strversion[STRVERSIONLEN];
	int m_version;
	int m_crcs;
#endif
#ifdef CONFIG_FEATURE_CLEAN_UP
	FILE *fp = 0;
#else
	FILE *fp;
#endif
#ifdef CONFIG_FEATURE_INSMOD_LOAD_MAP
	int flag_print_load_map = 0;
#endif
	int k_version = 0;
	struct utsname myuname;

	/* Parse any options */
#ifdef CONFIG_FEATURE_INSMOD_LOAD_MAP
	while ((opt = getopt(argc, argv, "fkqsvxmLo:")) > 0)
#else
	while ((opt = getopt(argc, argv, "fkqsvxLo:")) > 0)
#endif
		{
			switch (opt) {
				case 'f':			/* force loading */
					flag_force_load = 1;
					break;
				case 'k':			/* module loaded by kerneld, auto-cleanable */
					flag_autoclean = 1;
					break;
				case 's':			/* log to syslog */
					/* log to syslog -- not supported              */
					/* but kernel needs this for request_module(), */
					/* as this calls: modprobe -k -s -- <module>   */
					/* so silently ignore this flag                */
					break;
				case 'v':			/* verbose output */
					flag_verbose = 1;
					break;
				case 'q':			/* silent */
					flag_quiet = 1;
					break;
				case 'x':			/* do not export externs */
					flag_export = 0;
					break;
				case 'o':			/* name the output module */
					free(m_name);
					m_name = bb_xstrdup(optarg);
					break;
				case 'L':			/* Stub warning */
					/* This is needed for compatibility with modprobe.
					 * In theory, this does locking, but we don't do
					 * that.  So be careful and plan your life around not
					 * loading the same module 50 times concurrently. */
					break;
#ifdef CONFIG_FEATURE_INSMOD_LOAD_MAP
				case 'm':			/* print module load map */
					flag_print_load_map = 1;
					break;
#endif
				default:
					bb_show_usage();
			}
		}

	if (argv[optind] == NULL) {
		bb_show_usage();
	}

	/* Grab the module name */
	tmp1 = bb_xstrdup(argv[optind]);
	tmp = basename(tmp1);
	len = strlen(tmp);

	if (uname(&myuname) == 0) {
		if (myuname.release[0] == '2') {
			k_version = myuname.release[2] - '0';
		}
	}

#if defined(CONFIG_FEATURE_2_6_MODULES)
	if (k_version > 4 && len > 3 && tmp[len - 3] == '.' &&
			tmp[len - 2] == 'k' && tmp[len - 1] == 'o') {
		len-=3;
		tmp[len] = '\0';
	}
	else
#endif
		if (len > 2 && tmp[len - 2] == '.' && tmp[len - 1] == 'o') {
			len-=2;
			tmp[len] = '\0';
		}


#if defined(CONFIG_FEATURE_2_6_MODULES)
	if (k_version > 4)
		bb_xasprintf(&m_fullName, "%s.ko", tmp);
	else
#endif
		bb_xasprintf(&m_fullName, "%s.o", tmp);

	if (!m_name) {
		m_name = tmp;
	} else {
		free(tmp1);
		tmp1 = 0;       /* flag for free(m_name) before exit() */
	}

	/* Get a filedesc for the module.  Check we we have a complete path */
	if (stat(argv[optind], &st) < 0 || !S_ISREG(st.st_mode) ||
			(fp = fopen(argv[optind], "r")) == NULL) {
		/* Hmm.  Could not open it.  First search under /lib/modules/`uname -r`,
		 * but do not error out yet if we fail to find it... */
		if (k_version) {	/* uname succeedd */
			char *module_dir;
			char *tmdn;
			char real_module_dir[FILENAME_MAX];

			tmdn = concat_path_file(_PATH_MODULES, myuname.release);
			/* Jump through hoops in case /lib/modules/`uname -r`
			 * is a symlink.  We do not want recursive_action to
			 * follow symlinks, but we do want to follow the
			 * /lib/modules/`uname -r` dir, So resolve it ourselves
			 * if it is a link... */
			if (realpath (tmdn, real_module_dir) == NULL)
				module_dir = tmdn;
			else
				module_dir = real_module_dir;
			recursive_action(module_dir, TRUE, FALSE, FALSE,
					check_module_name_match, 0, m_fullName);
			free(tmdn);
		}

		/* Check if we have found anything yet */
		if (m_filename == 0 || ((fp = fopen(m_filename, "r")) == NULL))
		{
			char module_dir[FILENAME_MAX];

			free(m_filename);
			m_filename = 0;
			if (realpath (_PATH_MODULES, module_dir) == NULL)
				strcpy(module_dir, _PATH_MODULES);
			/* No module found under /lib/modules/`uname -r`, this
			 * time cast the net a bit wider.  Search /lib/modules/ */
			if (! recursive_action(module_dir, TRUE, FALSE, FALSE,
						check_module_name_match, 0, m_fullName))
			{
				if (m_filename == 0
						|| ((fp = fopen(m_filename, "r")) == NULL))
				{
					bb_error_msg("%s: no module by that name found", m_fullName);
					goto out;
				}
			} else
				bb_error_msg_and_die("%s: no module by that name found", m_fullName);
		}
	} else
		m_filename = bb_xstrdup(argv[optind]);

	if (!flag_quiet)
		printf("Using %s\n", m_filename);

#ifdef CONFIG_FEATURE_2_6_MODULES
	if (k_version > 4)
	{
		optind--;
		argv[optind + 1] = m_filename;
		return insmod_ng_main(argc - optind, argv + optind);
	}
#endif

	if ((f = obj_load(fp, LOADBITS)) == NULL)
		bb_perror_msg_and_die("Could not load the module");

	if (get_modinfo_value(f, "kernel_version") == NULL)
		m_has_modinfo = 0;
	else
		m_has_modinfo = 1;

#ifdef CONFIG_FEATURE_INSMOD_VERSION_CHECKING
	/* Version correspondence?  */
	if (!flag_quiet) {
		if (uname(&uts_info) < 0)
			uts_info.release[0] = '\0';
		if (m_has_modinfo) {
			m_version = new_get_module_version(f, m_strversion);
		} else {
			m_version = old_get_module_version(f, m_strversion);
			if (m_version == -1) {
				bb_error_msg("couldn't find the kernel version the module was "
						"compiled for");
				goto out;
			}
		}

		if (strncmp(uts_info.release, m_strversion, STRVERSIONLEN) != 0) {
			if (flag_force_load) {
				bb_error_msg("Warning: kernel-module version mismatch\n"
						"\t%s was compiled for kernel version %s\n"
						"\twhile this kernel is version %s",
						m_filename, m_strversion, uts_info.release);
			} else {
				bb_error_msg("kernel-module version mismatch\n"
						"\t%s was compiled for kernel version %s\n"
						"\twhile this kernel is version %s.",
						m_filename, m_strversion, uts_info.release);
				goto out;
			}
		}
	}
	k_crcs = 0;
#endif							/* CONFIG_FEATURE_INSMOD_VERSION_CHECKING */

	k_new_syscalls = !query_module(NULL, 0, NULL, 0, NULL);

	if (k_new_syscalls) {
#ifdef CONFIG_FEATURE_2_4_MODULES
		if (!new_get_kernel_symbols())
			goto out;
		k_crcs = new_is_kernel_checksummed();
#else
		bb_error_msg("Not configured to support new kernels");
		goto out;
#endif
	} else {
#ifdef CONFIG_FEATURE_2_2_MODULES
		if (!old_get_kernel_symbols(m_name))
			goto out;
		k_crcs = old_is_kernel_checksummed();
#else
		bb_error_msg("Not configured to support old kernels");
		goto out;
#endif
	}

#ifdef CONFIG_FEATURE_INSMOD_VERSION_CHECKING
	if (m_has_modinfo)
		m_crcs = new_is_module_checksummed(f);
	else
		m_crcs = old_is_module_checksummed(f);

	if (m_crcs != k_crcs)
		obj_set_symbol_compare(f, ncv_strcmp, ncv_symbol_hash);
#endif							/* CONFIG_FEATURE_INSMOD_VERSION_CHECKING */

	/* Let the module know about the kernel symbols.  */
	add_kernel_symbols(f);

	/* Allocate common symbols, symbol tables, and string tables.  */

	if (k_new_syscalls
			? !new_create_this_module(f, m_name)
			: !old_create_mod_use_count(f))
	{
		goto out;
	}

	if (!obj_check_undefineds(f)) {
		goto out;
	}
	obj_allocate_commons(f);
	check_tainted_module(f, m_name);

	/* done with the module name, on to the optional var=value arguments */
	++optind;

	if (optind < argc) {
		if (m_has_modinfo
				? !new_process_module_arguments(f, argc - optind, argv + optind)
				: !old_process_module_arguments(f, argc - optind, argv + optind))
		{
			goto out;
		}
	}

	arch_create_got(f);
	hide_special_symbols(f);

#ifdef CONFIG_FEATURE_INSMOD_KSYMOOPS_SYMBOLS
	add_ksymoops_symbols(f, m_filename, m_name);
#endif /* CONFIG_FEATURE_INSMOD_KSYMOOPS_SYMBOLS */

	if (k_new_syscalls)
		new_create_module_ksymtab(f);

	/* Find current size of the module */
	m_size = obj_load_size(f);


	m_addr = create_module(m_name, m_size);
	if (m_addr == -1) switch (errno) {
		case EEXIST:
			bb_error_msg("A module named %s already exists", m_name);
			goto out;
		case ENOMEM:
			bb_error_msg("Can't allocate kernel memory for module; needed %lu bytes",
					m_size);
			goto out;
		default:
			bb_perror_msg("create_module: %s", m_name);
			goto out;
	}

#if  !LOADBITS
	/*
	 * the PROGBITS section was not loaded by the obj_load
	 * now we can load them directly into the kernel memory
	 */
	if (!obj_load_progbits(fp, f, (char*)m_addr)) {
		delete_module(m_name);
		goto out;
	}
#endif

	if (!obj_relocate(f, m_addr)) {
		delete_module(m_name);
		goto out;
	}

	if (k_new_syscalls
			? !new_init_module(m_name, f, m_size)
			: !old_init_module(m_name, f, m_size))
	{
		delete_module(m_name);
		goto out;
	}

#ifdef CONFIG_FEATURE_INSMOD_LOAD_MAP
	if(flag_print_load_map)
		print_load_map(f);
#endif

	exit_status = EXIT_SUCCESS;

out:
#ifdef CONFIG_FEATURE_CLEAN_UP
	if(fp)
		fclose(fp);
	if(tmp1) {
		free(tmp1);
	} else {
		free(m_name);
	}
	free(m_filename);
#endif
	return(exit_status);
}


#endif


#ifdef CONFIG_FEATURE_2_6_MODULES

#include <sys/mman.h>
#include <asm/unistd.h>
#include <sys/syscall.h>

/* We use error numbers in a loose translation... */
static const char *moderror(int err)
{
	switch (err) {
		case ENOEXEC:
			return "Invalid module format";
		case ENOENT:
			return "Unknown symbol in module";
		case ESRCH:
			return "Module has wrong symbol version";
		case EINVAL:
			return "Invalid parameters";
		default:
			return strerror(err);
	}
}

extern int insmod_ng_main( int argc, char **argv)
{
	int i;
	int fd;
	long int ret;
	struct stat st;
	unsigned long len;
	void *map;
	char *filename, *options = bb_xstrdup("");

	filename = argv[1];
	if (!filename) {
		bb_show_usage();
		return -1;
	}

	/* Rest is options */
	for (i = 2; i < argc; i++) {
		options = xrealloc(options, strlen(options) + 2 + strlen(argv[i]) + 2);
		/* Spaces handled by "" pairs, but no way of escaping quotes */
		if (strchr(argv[i], ' ')) {
			strcat(options, "\"");
			strcat(options, argv[i]);
			strcat(options, "\"");
		} else {
			strcat(options, argv[i]);
		}
		strcat(options, " ");
	}

	if ((fd = open(filename, O_RDONLY, 0)) < 0) {
		bb_perror_msg_and_die("cannot open module `%s'", filename);
	}

	fstat(fd, &st);
	len = st.st_size;
	map = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED) {
		bb_perror_msg_and_die("cannot mmap `%s'", filename);
	}

	ret = syscall(__NR_init_module, map, len, options);
	if (ret != 0) {
		bb_perror_msg_and_die("cannot insert `%s': %s (%li)",
				filename, moderror(errno), ret);
	}

	return 0;
}

#endif
