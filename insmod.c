/* vi: set sw=4 ts=4: */
/*
 * Mini insmod implementation for busybox
 *
 * Copyright (C) 1999,2000,2001 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>
 * and Ron Alder <alder@lineo.com>
 *
 * Modified by Bryan Rittmeyer <bryan@ixiacom.com> to support SH4
 * and (theoretically) SH3. I have only tested SH4 in little endian mode.
 *
 * Modified by Alcove, Julien Gaulmin <julien.gaulmin@alcove.fr> and
 * Nicolas Ferre <nicolas.ferre@alcove.fr> to support ARM7TDMI.  Only
 * very minor changes required to also work with StrongArm and presumably
 * all ARM based systems.
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

#include "busybox.h"
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
#include <sys/utsname.h>
#include <sys/syscall.h>
#include <linux/unistd.h>

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

#ident "$Id: insmod.c,v 1.44 2001/01/27 09:33:38 andersen Exp $"

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
};

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

#ident "$Id: insmod.c,v 1.44 2001/01/27 09:33:38 andersen Exp $"

/* The relocatable object is manipulated using elfin types.  */

#include <stdio.h>
#include <elf.h>


/* Machine-specific elf macros for i386 et al.  */

/* the SH changes have only been tested on the SH4 in =little endian= mode */
/* I'm not sure about big endian, so let's warn: */

#if (defined(__SH4__) || defined(__SH3__)) && defined(__BIG_ENDIAN__)
#error insmod.c may require changes for use on big endian SH4/SH3
#endif

/* it may or may not work on the SH1/SH2... So let's error on those
   also */
#if (defined(__sh__) && (!(defined(__SH3__) || defined(__SH4__))))
#error insmod.c may require changes for non-SH3/SH4 use
#endif

#define ELFCLASSM	ELFCLASS32
#define ELFDATAM	ELFDATA2LSB



#if defined(__sh__)

#define MATCH_MACHINE(x) (x == EM_SH)
#define SHT_RELM	SHT_RELA
#define Elf32_RelM	Elf32_Rela

#elif defined(__arm__)

#define MATCH_MACHINE(x) (x == EM_ARM)
#define SHT_RELM	SHT_REL
#define Elf32_RelM	Elf32_Rel

#elif defined(__i386__)

/* presumably we can use these for anything but the SH and ARM*/
/* this is the previous behavior, but it does result in
   insmod.c being broken on anything except i386 */
#ifndef EM_486
#define MATCH_MACHINE(x)  (x == EM_386)
#else
#define MATCH_MACHINE(x)  (x == EM_386 || x == EM_486)
#endif

#define SHT_RELM	SHT_REL
#define Elf32_RelM	Elf32_Rel

#else
#error Sorry, but insmod.c does not yet support this architecture...
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

/* For some reason this is missing from libc5.  */
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

unsigned long obj_elf_hash(const char *);

unsigned long obj_elf_hash_n(const char *, unsigned long len);

struct obj_symbol *obj_add_symbol (struct obj_file *f, const char *name,
				   unsigned long symidx, int info, int secidx,
				   ElfW(Addr) value, unsigned long size);

struct obj_symbol *obj_find_symbol (struct obj_file *f,
					 const char *name);

ElfW(Addr) obj_symbol_final_value(struct obj_file *f,
				  struct obj_symbol *sym);

void obj_set_symbol_compare(struct obj_file *f,
			    int (*cmp)(const char *, const char *),
			    unsigned long (*hash)(const char *));

struct obj_section *obj_find_section (struct obj_file *f,
					   const char *name);

void obj_insert_section_load_order (struct obj_file *f,
				    struct obj_section *sec);

struct obj_section *obj_create_alloced_section (struct obj_file *f,
						const char *name,
						unsigned long align,
						unsigned long size);

struct obj_section *obj_create_alloced_section_first (struct obj_file *f,
						      const char *name,
						      unsigned long align,
						      unsigned long size);

void *obj_extend_section (struct obj_section *sec, unsigned long more);

int obj_string_patch(struct obj_file *f, int secidx, ElfW(Addr) offset,
		     const char *string);

int obj_symbol_patch(struct obj_file *f, int secidx, ElfW(Addr) offset,
		     struct obj_symbol *sym);

int obj_check_undefineds(struct obj_file *f);

void obj_allocate_commons(struct obj_file *f);

unsigned long obj_load_size (struct obj_file *f);

int obj_relocate (struct obj_file *f, ElfW(Addr) base);

struct obj_file *obj_load(FILE *f);

int obj_create_image (struct obj_file *f, char *image);

/* Architecture specific manipulation routines.  */

struct obj_file *arch_new_file (void);

struct obj_section *arch_new_section (void);

struct obj_symbol *arch_new_symbol (void);

enum obj_reloc arch_apply_relocation (struct obj_file *f,
				      struct obj_section *targsec,
				      struct obj_section *symsec,
				      struct obj_symbol *sym,
				      ElfW(RelM) *rel, ElfW(Addr) value);

int arch_create_got (struct obj_file *f);

struct new_module;
int arch_init_module (struct obj_file *f, struct new_module *);

#endif /* obj.h */
//----------------------------------------------------------------------------
//--------end of modutils obj.h
//----------------------------------------------------------------------------





#define _PATH_MODULES	"/lib/modules"
static const int STRVERSIONLEN = 32;

/*======================================================================*/

int flag_force_load = 0;
int flag_autoclean = 0;
int flag_verbose = 0;
int flag_export = 1;


/*======================================================================*/

/* previously, these were named i386_* but since we could be
   compiling for the sh, I've renamed them to the more general
   arch_* These structures are the same between the x86 and SH, 
   and we can't support anything else right now anyway. In the
   future maybe they should be #if defined'd */

/* Done ;-) */

#if defined(__arm__)
struct arm_plt_entry
{
  int offset;
  int allocated:1;
  int inited:1;                /* has been set up */
};
#endif

struct arch_got_entry {
	int offset;
	unsigned offset_done:1;
	unsigned reloc_done:1;
};

struct arch_file {
	struct obj_file root;
#if defined(__arm__)
    struct obj_section *plt;
#endif
	struct obj_section *got;
};

struct arch_symbol {
	struct obj_symbol root;
#if defined(__arm__)
    struct arm_plt_entry pltent;
#endif
	struct arch_got_entry gotent;
};


struct external_module {
	const char *name;
	ElfW(Addr) addr;
	int used;
	size_t nsyms;
	struct new_module_symbol *syms;
};

struct new_module_symbol *ksyms;
size_t nksyms;

struct external_module *ext_modules;
int n_ext_modules;
int n_ext_modules_used;



/* Some firendly syscalls to cheer everyone's day...  */
#define __NR_new_sys_init_module  __NR_init_module
_syscall2(int, new_sys_init_module, const char *, name,
		  const struct new_module *, info)
#define __NR_old_sys_init_module  __NR_init_module
_syscall5(int, old_sys_init_module, const char *, name, char *, code,
		  unsigned, codesize, struct old_mod_routines *, routines,
		  struct old_symbol_table *, symtab)
#ifndef BB_RMMOD
_syscall1(int, delete_module, const char *, name)
#else
extern int delete_module(const char *);
#endif

/* This is kind of troublesome. See, we don't actually support
   the m68k or the arm the same way we support i386 and (now)
   sh. In doing my SH patch, I just assumed that whatever works
   for i386 also works for m68k and arm since currently insmod.c
   does nothing special for them. If this isn't true, the below
   line is rather misleading IMHO, and someone should either
   change it or add more proper architecture-dependent support
   for these boys.

   -- Bryan Rittmeyer <bryan@ixiacom.com>                    */

#ifdef BB_FEATURE_OLD_MODULE_INTERFACE
_syscall1(int, get_kernel_syms, struct old_kernel_sym *, ks)
#endif

#if defined(__i386__) || defined(__m68k__) || defined(__arm__)
/* Jump through hoops to fixup error return codes */
#define __NR__create_module  __NR_create_module
static inline _syscall2(long, _create_module, const char *, name, size_t,
						size)
unsigned long create_module(const char *name, size_t size)
{
	long ret = _create_module(name, size);

	if (ret == -1 && errno > 125) {
		ret = -errno;
		errno = 0;
	}
	return ret;
}
#else
_syscall2(unsigned long, create_module, const char *, name, size_t, size)
#endif
static char m_filename[BUFSIZ + 1] = "\0";
static char m_fullName[BUFSIZ + 1] = "\0";

/*======================================================================*/


static int findNamedModule(const char *fileName, struct stat *statbuf,
						   void *userDate)
{
	char *fullName = (char *) userDate;


	if (fullName[0] == '\0')
		return (FALSE);
	else {
		char *tmp = strrchr((char *) fileName, '/');

		if (tmp == NULL)
			tmp = (char *) fileName;
		else
			tmp++;
		if (check_wildcard_match(tmp, fullName) == TRUE) {
			/* Stop searching if we find a match */
			memcpy(m_filename, fileName, strlen(fileName)+1);
			return (FALSE);
		}
	}
	return (TRUE);
}


/*======================================================================*/

struct obj_file *arch_new_file(void)
{
	struct arch_file *f;
	f = xmalloc(sizeof(*f));
	f->got = NULL;
	return &f->root;
}

struct obj_section *arch_new_section(void)
{
	return xmalloc(sizeof(struct obj_section));
}

struct obj_symbol *arch_new_symbol(void)
{
	struct arch_symbol *sym;
	sym = xmalloc(sizeof(*sym));
	memset(&sym->gotent, 0, sizeof(sym->gotent));
	return &sym->root;
}

enum obj_reloc
arch_apply_relocation(struct obj_file *f,
					  struct obj_section *targsec,
					  struct obj_section *symsec,
					  struct obj_symbol *sym,
				      ElfW(RelM) *rel, ElfW(Addr) v)
{
	struct arch_file *ifile = (struct arch_file *) f;
	struct arch_symbol *isym = (struct arch_symbol *) sym;

	ElfW(Addr) *loc = (ElfW(Addr) *) (targsec->contents + rel->r_offset);
	ElfW(Addr) dot = targsec->header.sh_addr + rel->r_offset;
	ElfW(Addr) got = ifile->got ? ifile->got->header.sh_addr : 0;
#if defined(__arm__)
	ElfW(Addr) plt = ifile->plt ? ifile->plt->header.sh_addr : 0;

	struct arm_plt_entry *pe;
	unsigned long *ip;
#endif

	enum obj_reloc ret = obj_reloc_ok;

	switch (ELF32_R_TYPE(rel->r_info)) {

/* even though these constants seem to be the same for
   the i386 and the sh, we "#if define" them for clarity
   and in case that ever changes */
#if defined(__sh__)
	case R_SH_NONE:
#elif defined(__arm__)
	case R_ARM_NONE:
#elif defined(__i386__)
	case R_386_NONE:
#endif
		break;

#if defined(__sh__)
	case R_SH_DIR32:
#elif defined(__arm__)
	case R_ARM_ABS32:
#elif defined(__i386__)
	case R_386_32:
#endif
		*loc += v;
		break;

#if defined(__arm__)
#elif defined(__sh__)
        case R_SH_REL32:
		*loc += v - dot;
		break;
#elif defined(__i386__)
	case R_386_PLT32:
	case R_386_PC32:
		*loc += v - dot;
		break;
#endif

#if defined(__sh__)
        case R_SH_PLT32:
                *loc = v - dot;
                break;
#elif defined(__arm__)
    case R_ARM_PC24:
    case R_ARM_PLT32:
      /* find the plt entry and initialize it if necessary */
      assert(isym != NULL);
      pe = (struct arm_plt_entry*) &isym->pltent;
      if (! pe->inited) {
	  	ip = (unsigned long *) (ifile->plt->contents + pe->offset);
	  	ip[0] = 0xe51ff004;			/* ldr pc,[pc,#-4] */
	  	ip[1] = v;				/* sym@ */
	  	pe->inited = 1;
	  }

      /* relative distance to target */
      v -= dot;
      /* if the target is too far away.... */
      if ((int)v < -0x02000000 || (int)v >= 0x02000000) {
	    /* go via the plt */
	    v = plt + pe->offset - dot;
	  }
      if (v & 3)
	    ret = obj_reloc_dangerous;

      /* Convert to words. */
      v >>= 2;

      /* merge the offset into the instruction. */
      *loc = (*loc & ~0x00ffffff) | ((v + *loc) & 0x00ffffff);
      break;
#elif defined(__i386__)
#endif


#if defined(__arm__)
#elif defined(__sh__)
        case R_SH_GLOB_DAT:
        case R_SH_JMP_SLOT:
               	*loc = v;
                break;
#elif defined(__i386__)
	case R_386_GLOB_DAT:
	case R_386_JMP_SLOT:
		*loc = v;
		break;
#endif

#if defined(__arm__)
#elif defined(__sh__)
        case R_SH_RELATIVE:
	        *loc += f->baseaddr + rel->r_addend;
                break;
#elif defined(__i386__)
        case R_386_RELATIVE:
		*loc += f->baseaddr;
		break;
#endif

#if defined(__sh__)
        case R_SH_GOTPC:
#elif defined(__arm__)
    case R_ARM_GOTPC:
#elif defined(__i386__)
	case R_386_GOTPC:
#endif
		assert(got != 0);
#if defined(__sh__)
		*loc += got - dot + rel->r_addend;;
#elif defined(__i386__) || defined(__arm__)
		*loc += got - dot;
#endif
		break;

#if defined(__sh__)
	case R_SH_GOT32:
#elif defined(__arm__)
    case R_ARM_GOT32:
#elif defined(__i386__)
	case R_386_GOT32:
#endif
		assert(isym != NULL);
        /* needs an entry in the .got: set it, once */
		if (!isym->gotent.reloc_done) {
			isym->gotent.reloc_done = 1;
			*(ElfW(Addr) *) (ifile->got->contents + isym->gotent.offset) = v;
		}
        /* make the reloc with_respect_to_.got */
#if defined(__sh__)
		*loc += isym->gotent.offset + rel->r_addend;
#elif defined(__i386__) || defined(__arm__)
		*loc += isym->gotent.offset;
#endif
		break;

    /* address relative to the got */
#if defined(__sh__)
	case R_SH_GOTOFF:
#elif defined(__arm__)
    case R_ARM_GOTOFF:
#elif defined(__i386__)
	case R_386_GOTOFF:
#endif
		assert(got != 0);
		*loc += v - got;
		break;

	default:
        printf("Warning: unhandled reloc %d\n",(int)ELF32_R_TYPE(rel->r_info));
		ret = obj_reloc_unhandled;
		break;
	}

	return ret;
}

int arch_create_got(struct obj_file *f)
{
	struct arch_file *ifile = (struct arch_file *) f;
	int i, got_offset = 0, gotneeded = 0;
#if defined(__arm__)
	int plt_offset = 0, pltneeded = 0;
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

			switch (ELF32_R_TYPE(rel->r_info)) {
#if defined(__arm__)
			case R_ARM_GOT32:
#elif defined(__sh__)
			case R_SH_GOT32:
#elif defined(__i386__)
			case R_386_GOT32:
#endif
				break;

#if defined(__arm__)
			case R_ARM_PC24:
			case R_ARM_PLT32:
				pltneeded = 1;
				break;

			case R_ARM_GOTPC:
			case R_ARM_GOTOFF:
				gotneeded = 1;
				if (got_offset == 0)
					got_offset = 4;
#elif defined(__sh__)
			case R_SH_GOTPC:
			case R_SH_GOTOFF:
				gotneeded = 1;
#elif defined(__i386__)
			case R_386_GOTPC:
			case R_386_GOTOFF:
				gotneeded = 1;
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

			if (!intsym->gotent.offset_done) {
				intsym->gotent.offset_done = 1;
				intsym->gotent.offset = got_offset;
				got_offset += 4;
			}
#if defined(__arm__)
			if (pltneeded && intsym->pltent.allocated == 0) {
				intsym->pltent.allocated = 1;
				intsym->pltent.offset = plt_offset;
				plt_offset += 8;
				intsym->pltent.inited = 0;
				pltneeded = 0;
			}
#endif
			}
		}

#if defined(__arm__)
	if (got_offset) {
		struct obj_section* relsec = obj_find_section(f, ".got");

		if (relsec) {
			obj_extend_section(relsec, got_offset);
		} else {
			relsec = obj_create_alloced_section(f, ".got", 8, got_offset);
			assert(relsec);
		}

		ifile->got = relsec;
	}

	if (plt_offset)
		ifile->plt = obj_create_alloced_section(f, ".plt", 8, plt_offset);
#else
	if (got_offset > 0 || gotneeded)
		ifile->got = obj_create_alloced_section(f, ".got", 4, got_offset);
#endif

	return 1;
}

int arch_init_module(struct obj_file *f, struct new_module *mod)
{
	return 1;
}


/*======================================================================*/

/* Standard ELF hash function.  */
inline unsigned long obj_elf_hash_n(const char *name, unsigned long n)
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

unsigned long obj_elf_hash(const char *name)
{
	return obj_elf_hash_n(name, strlen(name));
}

#ifdef BB_FEATURE_INSMOD_VERSION_CHECKING
/* Get the kernel version in the canonical integer form.  */

static int get_kernel_version(char str[STRVERSIONLEN])
{
	struct utsname uts_info;
	char *p, *q;
	int a, b, c;

	if (uname(&uts_info) < 0)
		return -1;
	strncpy(str, uts_info.release, STRVERSIONLEN);
	p = uts_info.release;

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

void
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

#endif							/* BB_FEATURE_INSMOD_VERSION_CHECKING */


struct obj_symbol *obj_add_symbol(struct obj_file *f, const char *name,
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
					error_msg("%s multiply defined\n", name);
				return sym;
			}
		}

	/* Completely new symbol.  */
	sym = arch_new_symbol();
	sym->next = f->symtab[hash];
	f->symtab[hash] = sym;
	sym->ksymidx = -1;

	if (ELFW(ST_BIND) (info) == STB_LOCAL)
		f->local_symtab[symidx] = sym;

  found:
	sym->name = name;
	sym->value = value;
	sym->size = size;
	sym->secidx = secidx;
	sym->info = info;

	return sym;
}

struct obj_symbol *obj_find_symbol(struct obj_file *f, const char *name)
{
	struct obj_symbol *sym;
	unsigned long hash = f->symbol_hash(name) % HASH_BUCKETS;

	for (sym = f->symtab[hash]; sym; sym = sym->next)
		if (f->symbol_cmp(sym->name, name) == 0)
			return sym;

	return NULL;
}

ElfW(Addr)
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

struct obj_section *obj_find_section(struct obj_file *f, const char *name)
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

void
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

struct obj_section *obj_create_alloced_section(struct obj_file *f,
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

struct obj_section *obj_create_alloced_section_first(struct obj_file *f,
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

void *obj_extend_section(struct obj_section *sec, unsigned long more)
{
	unsigned long oldsize = sec->header.sh_size;
	sec->contents = xrealloc(sec->contents, sec->header.sh_size += more);
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

	for (i = 0, s = syms; i < nsyms; ++i, ++s) {

		/* Only add symbols that are already marked external.  If we
		   override locals we may cause problems for argument initialization.
		   We will also create a false dependency on the module.  */
		struct obj_symbol *sym;

		sym = obj_find_symbol(f, (char *) s->name);
		if (sym && !ELFW(ST_BIND) (sym->info) == STB_LOCAL) {
			sym = obj_add_symbol(f, (char *) s->name, -1,
								 ELFW(ST_INFO) (STB_GLOBAL, STT_NOTYPE),
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
			error_msg("symbol for parameter %s not found\n", p);
			return 0;
		}

		loc = (int *) (f->sections[sym->secidx]->contents + sym->value);

		/* Do C quoting if we begin with a ".  */
		if (*q == '"') {
			char *r, *str;

			str = alloca(strlen(q));
			for (r = str, q++; *q != '"'; ++q, ++r) {
				if (*q == '\0') {
					error_msg("improperly terminated string argument for %s\n", p);
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
			char *loc = contents + sym->value;
			char *r;			/* To search for commas */

			/* Break the string with comas */
			while ((r = strchr(q, ',')) != (char *) NULL) {
				*r++ = '\0';
				obj_string_patch(f, sym->secidx, loc - contents, q);
				loc += sizeof(char *);
				q = r;
			}

			/* last part */
			obj_string_patch(f, sym->secidx, loc - contents, q);
		}

		argc--, argv++;
	}

	return 1;
}

#ifdef BB_FEATURE_INSMOD_VERSION_CHECKING
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
	strncpy(str, p, STRVERSIONLEN);

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

#endif   /* BB_FEATURE_INSMOD_VERSION_CHECKING */

#ifdef BB_FEATURE_OLD_MODULE_INTERFACE

/* Fetch all the symbols and divvy them up as appropriate for the modules.  */

static int old_get_kernel_symbols(const char *m_name)
{
	struct old_kernel_sym *ks, *k;
	struct new_module_symbol *s;
	struct external_module *mod;
	int nks, nms, nmod, i;

	nks = get_kernel_syms(NULL);
	if (nks < 0) {
		perror_msg("get_kernel_syms: %s", m_name);
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
		struct new_module_symbol *s;

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
		obj_symbol_final_value(f, obj_find_symbol(f, "init_module"));
	routines.cleanup =
		obj_symbol_final_value(f, obj_find_symbol(f, "cleanup_module"));

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
		perror_msg("init_module: %s", m_name);

	free(image);
	free(symtab);

	return ret == 0;
}

#else

#define old_create_mod_use_count(x) TRUE
#define old_init_module(x, y, z) TRUE

#endif							/* BB_FEATURE_OLD_MODULE_INTERFACE */



/*======================================================================*/
/* Functions relating to module loading after 2.1.18.  */

static int
new_process_module_arguments(struct obj_file *f, int argc, char **argv)
{
	while (argc > 0) {
		char *p, *q, *key;
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
			error_msg("invalid parameter %s\n", key);
			return 0;
		}

		sym = obj_find_symbol(f, key);

		/* Also check that the parameter was not resolved from the kernel.  */
		if (sym == NULL || sym->secidx > SHN_HIRESERVE) {
			error_msg("symbol for parameter %s not found\n", key);
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
							error_msg("improperly terminated string argument for %s\n",
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
						error_msg("parameter type 'c' for %s must be followed by"
								" the maximum size\n", key);
						return 0;
					}
					charssize = strtoul(p + 1, (char **) NULL, 10);

					/* Check length */
					if (strlen(str) >= charssize) {
						error_msg("string too long for %s (max %ld)\n", key,
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
					error_msg("unknown parameter type '%c' for %s\n", *p, key);
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
					error_msg("too many values for %s (max %d)\n", key, max);
					return 0;
				}
				++q;
				break;

			default:
				error_msg("invalid argument syntax for %s\n", key);
				return 0;
			}
		}

	  end_of_arg:
		if (n < min) {
			error_msg("too few values for %s (min %d)\n", key, min);
			return 0;
		}

		argc--, argv++;
	}

	return 1;
}

#ifdef BB_FEATURE_INSMOD_VERSION_CHECKING
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
	strncpy(str, p, STRVERSIONLEN);

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

#endif   /* BB_FEATURE_INSMOD_VERSION_CHECKING */


#ifdef BB_FEATURE_NEW_MODULE_INTERFACE

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
		if (errno == ENOSPC) {
			module_names = xrealloc(module_names, bufsize = ret);
			goto retry_modules_load;
		}
		perror_msg("QM_MODULES");
		return 0;
	}

	n_ext_modules = nmod = ret;
	ext_modules = modules = xmalloc(nmod * sizeof(*modules));
	memset(modules, 0, nmod * sizeof(*modules));

	/* Collect the modules' symbols.  */

	for (i = 0, mn = module_names, m = modules;
		 i < nmod; ++i, ++m, mn += strlen(mn) + 1) {
		struct new_module_info info;

		if (query_module(mn, QM_INFO, &info, sizeof(info), &ret)) {
			if (errno == ENOENT) {
				/* The module was removed out from underneath us.  */
				continue;
			}
			perror_msg("query_module: QM_INFO: %s", mn);
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
				perror_msg("query_module: QM_SYMBOLS: %s", mn);
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

	/* Collect the kernel's symbols.  */

	syms = xmalloc(bufsize = 16 * 1024);
  retry_kern_sym_load:
	if (query_module(NULL, QM_SYMBOLS, syms, bufsize, &ret)) {
		if (errno == ENOSPC) {
			syms = xrealloc(syms, bufsize = ret);
			goto retry_kern_sym_load;
		}
		perror_msg("kernel: QM_SYMBOLS");
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

	obj_add_symbol(f, "__this_module", -1,
				   ELFW(ST_INFO) (STB_LOCAL, STT_OBJECT), sec->idx, 0,
				   sizeof(struct new_module));

	obj_string_patch(f, sec->idx, offsetof(struct new_module, name),
					 m_name);

	return 1;
}


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

		tm = obj_find_symbol(f, "__this_module");
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
		obj_symbol_final_value(f, obj_find_symbol(f, "init_module"));
	module->cleanup =
		obj_symbol_final_value(f, obj_find_symbol(f, "cleanup_module"));

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

	if (!arch_init_module(f, module))
		return 0;

	/* Whew!  All of the initialization is complete.  Collect the final
	   module image and give it to the kernel.  */

	image = xmalloc(m_size);
	obj_create_image(f, image);

	ret = new_sys_init_module(m_name, (struct new_module *) image);
	if (ret)
		perror_msg("init_module: %s", m_name);

	free(image);

	return ret == 0;
}

#else

#define new_init_module(x, y, z) TRUE
#define new_create_this_module(x, y) 0
#define new_create_module_ksymtab(x)
#define query_module(v, w, x, y, z) -1

#endif							/* BB_FEATURE_NEW_MODULE_INTERFACE */


/*======================================================================*/

int
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

int
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

int obj_check_undefineds(struct obj_file *f)
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
					error_msg("unresolved symbol %s\n", sym->name);
					ret = 0;
				}
			}
	}

	return ret;
}

void obj_allocate_commons(struct obj_file *f)
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

unsigned long obj_load_size(struct obj_file *f)
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

int obj_relocate(struct obj_file *f, ElfW(Addr) base)
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
					error_msg("%s of type %ld for %s\n", errmsg,
							(long) ELFW(R_TYPE) (rel->r_info),
							strtab + extsym->st_name);
				} else {
					error_msg("%s of type %ld\n", errmsg,
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

int obj_create_image(struct obj_file *f, char *image)
{
	struct obj_section *sec;
	ElfW(Addr) base = f->baseaddr;

	for (sec = f->load_order; sec; sec = sec->load_next) {
		char *secimg;

		if (sec->header.sh_size == 0)
			continue;

		secimg = image + (sec->header.sh_addr - base);

		/* Note that we allocated data for NOBITS sections earlier.  */
		memcpy(secimg, sec->contents, sec->header.sh_size);
	}

	return 1;
}

/*======================================================================*/

struct obj_file *obj_load(FILE * fp)
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
		perror_msg("error reading ELF header");
		return NULL;
	}

	if (f->header.e_ident[EI_MAG0] != ELFMAG0
		|| f->header.e_ident[EI_MAG1] != ELFMAG1
		|| f->header.e_ident[EI_MAG2] != ELFMAG2
		|| f->header.e_ident[EI_MAG3] != ELFMAG3) {
		error_msg("not an ELF file\n");
		return NULL;
	}
	if (f->header.e_ident[EI_CLASS] != ELFCLASSM
		|| f->header.e_ident[EI_DATA] != ELFDATAM
		|| f->header.e_ident[EI_VERSION] != EV_CURRENT
		|| !MATCH_MACHINE(f->header.e_machine)) {
		error_msg("ELF file not for this architecture\n");
		return NULL;
	}
	if (f->header.e_type != ET_REL) {
		error_msg("ELF file not a relocatable object\n");
		return NULL;
	}

	/* Read the section headers.  */

	if (f->header.e_shentsize != sizeof(ElfW(Shdr))) {
		error_msg("section header size mismatch: %lu != %lu\n",
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
		perror_msg("error reading ELF section headers");
		return NULL;
	}

	/* Read the section data.  */

	for (i = 0; i < shnum; ++i) {
		struct obj_section *sec;

		f->sections[i] = sec = arch_new_section();
		memset(sec, 0, sizeof(*sec));

		sec->header = section_headers[i];
		sec->idx = i;

		switch (sec->header.sh_type) {
		case SHT_NULL:
		case SHT_NOTE:
		case SHT_NOBITS:
			/* ignore */
			break;

		case SHT_PROGBITS:
		case SHT_SYMTAB:
		case SHT_STRTAB:
		case SHT_RELM:
			if (sec->header.sh_size > 0) {
				sec->contents = xmalloc(sec->header.sh_size);
				fseek(fp, sec->header.sh_offset, SEEK_SET);
				if (fread(sec->contents, sec->header.sh_size, 1, fp) != 1) {
					perror_msg("error reading ELF section data");
					return NULL;
				}
			} else {
				sec->contents = NULL;
			}
			break;

#if SHT_RELM == SHT_REL
		case SHT_RELA:
			error_msg("RELA relocations not supported on this architecture\n");
			return NULL;
#else
		case SHT_REL:
			error_msg("REL relocations not supported on this architecture\n");
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

			error_msg("can't handle sections of type %ld\n",
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

		if (sec->header.sh_flags & SHF_ALLOC)
			obj_insert_section_load_order(f, sec);

		switch (sec->header.sh_type) {
		case SHT_SYMTAB:
			{
				unsigned long nsym, j;
				char *strtab;
				ElfW(Sym) * sym;

				if (sec->header.sh_entsize != sizeof(ElfW(Sym))) {
					error_msg("symbol size mismatch: %lu != %lu\n",
							(unsigned long) sec->header.sh_entsize,
							(unsigned long) sizeof(ElfW(Sym)));
					return NULL;
				}

				nsym = sec->header.sh_size / sizeof(ElfW(Sym));
				strtab = f->sections[sec->header.sh_link]->contents;
				sym = (ElfW(Sym) *) sec->contents;

				/* Allocate space for a table of local symbols.  */
				j = f->local_symtab_size = sec->header.sh_info;
				f->local_symtab = xmalloc(j *=
										  sizeof(struct obj_symbol *));
				memset(f->local_symtab, 0, j);

				/* Insert all symbols into the hash table.  */
				for (j = 1, ++sym; j < nsym; ++j, ++sym) {
					const char *name;
					if (sym->st_name)
						name = strtab + sym->st_name;
		else
						name = f->sections[sym->st_shndx]->name;

					obj_add_symbol(f, name, j, sym->st_info, sym->st_shndx,
								   sym->st_value, sym->st_size);
		}
	}
			break;

		case SHT_RELM:
			if (sec->header.sh_entsize != sizeof(ElfW(RelM))) {
				error_msg("relocation entry size mismatch: %lu != %lu\n",
						(unsigned long) sec->header.sh_entsize,
						(unsigned long) sizeof(ElfW(RelM)));
				return NULL;
			}
			break;
		}
	}

	return f;
}

static void hide_special_symbols(struct obj_file *f)
{
	static const char *const specials[] = {
		"cleanup_module",
		"init_module",
		"kernel_version",
		NULL
	};

	struct obj_symbol *sym;
	const char *const *p;

	for (p = specials; *p; ++p)
		if ((sym = obj_find_symbol(f, *p)) != NULL)
			sym->info =
				ELFW(ST_INFO) (STB_LOCAL, ELFW(ST_TYPE) (sym->info));
}



extern int insmod_main( int argc, char **argv)
{
	int opt;
	int k_crcs;
	int k_new_syscalls;
	int len;
	char *tmp;
	unsigned long m_size;
	ElfW(Addr) m_addr;
	FILE *fp;
	struct obj_file *f;
	char m_name[BUFSIZ + 1] = "\0";
	int exit_status = EXIT_FAILURE;
	int m_has_modinfo;
#ifdef BB_FEATURE_INSMOD_VERSION_CHECKING
	int k_version;
	char k_strversion[STRVERSIONLEN];
	char m_strversion[STRVERSIONLEN];
	int m_version;
	int m_crcs;
#endif

	/* Parse any options */
	while ((opt = getopt(argc, argv, "fkvxLo:")) > 0) {
		switch (opt) {
			case 'f':			/* force loading */
				flag_force_load = 1;
				break;
			case 'k':			/* module loaded by kerneld, auto-cleanable */
				flag_autoclean = 1;
				break;
			case 'v':			/* verbose output */
				flag_verbose = 1;
				break;
			case 'x':			/* do not export externs */
				flag_export = 0;
				break;
			case 'o':			/* name the output module */
				strncpy(m_name, optarg, BUFSIZ);
				break;
			case 'L':			/* Stub warning */
				/* This is needed for compatibility with modprobe.
				 * In theory, this does locking, but we don't do
				 * that.  So be careful and plan your life around not
				 * loading the same module 50 times concurrently. */
				break;
			default:
				usage(insmod_usage);
		}
	}
	
	if (argv[optind] == NULL) {
		usage(insmod_usage);
	}

	/* Grab the module name */
	if ((tmp = strrchr(argv[optind], '/')) != NULL) {
		tmp++;
	} else {
		tmp = argv[optind];
	}
	len = strlen(tmp);

	if (len > 2 && tmp[len - 2] == '.' && tmp[len - 1] == 'o')
		len -= 2;
	strncpy(m_fullName, tmp, len);
	if (*m_name == '\0') {
		strcpy(m_name, m_fullName);
	}
	strcat(m_fullName, ".o");

	/* Get a filedesc for the module */
	if ((fp = fopen(argv[optind], "r")) == NULL) {
		/* Hmpf.  Could not open it. Search through _PATH_MODULES to find a module named m_name */
		if (recursive_action(_PATH_MODULES, TRUE, FALSE, FALSE,
							findNamedModule, 0, m_fullName) == FALSE) 
		{
			if (m_filename[0] == '\0'
				|| ((fp = fopen(m_filename, "r")) == NULL)) 
			{
				error_msg("No module named '%s' found in '%s'\n", m_fullName, _PATH_MODULES);
				return EXIT_FAILURE;
			}
		} else
			error_msg_and_die("No module named '%s' found in '%s'\n", m_fullName, _PATH_MODULES);
	} else
		memcpy(m_filename, argv[optind], strlen(argv[optind]));


	if ((f = obj_load(fp)) == NULL)
		perror_msg_and_die("Could not load the module");

	if (get_modinfo_value(f, "kernel_version") == NULL)
		m_has_modinfo = 0;
	else
		m_has_modinfo = 1;

#ifdef BB_FEATURE_INSMOD_VERSION_CHECKING
	/* Version correspondence?  */

	k_version = get_kernel_version(k_strversion);
	if (m_has_modinfo) {
		m_version = new_get_module_version(f, m_strversion);
	} else {
		m_version = old_get_module_version(f, m_strversion);
		if (m_version == -1) {
			error_msg("couldn't find the kernel version the module was "
					"compiled for\n");
			goto out;
		}
	}

	if (strncmp(k_strversion, m_strversion, STRVERSIONLEN) != 0) {
		if (flag_force_load) {
			error_msg("Warning: kernel-module version mismatch\n"
					"\t%s was compiled for kernel version %s\n"
					"\twhile this kernel is version %s\n",
					m_filename, m_strversion, k_strversion);
		} else {
			error_msg("kernel-module version mismatch\n"
					"\t%s was compiled for kernel version %s\n"
					"\twhile this kernel is version %s.\n",
					m_filename, m_strversion, k_strversion);
			goto out;
		}
	}
	k_crcs = 0;
#endif							/* BB_FEATURE_INSMOD_VERSION_CHECKING */

	k_new_syscalls = !query_module(NULL, 0, NULL, 0, NULL);

	if (k_new_syscalls) {
#ifdef BB_FEATURE_NEW_MODULE_INTERFACE
		if (!new_get_kernel_symbols())
			goto out;
		k_crcs = new_is_kernel_checksummed();
#else
		error_msg("Not configured to support new kernels\n");
		goto out;
#endif
	} else {
#ifdef BB_FEATURE_OLD_MODULE_INTERFACE
		if (!old_get_kernel_symbols(m_name))
			goto out;
		k_crcs = old_is_kernel_checksummed();
#else
		error_msg("Not configured to support old kernels\n");
		goto out;
#endif
	}

#ifdef BB_FEATURE_INSMOD_VERSION_CHECKING
	if (m_has_modinfo)
		m_crcs = new_is_module_checksummed(f);
	else
		m_crcs = old_is_module_checksummed(f);

	if (m_crcs != k_crcs)
		obj_set_symbol_compare(f, ncv_strcmp, ncv_symbol_hash);
#endif							/* BB_FEATURE_INSMOD_VERSION_CHECKING */

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

	if (k_new_syscalls)
		new_create_module_ksymtab(f);

	/* Find current size of the module */
	m_size = obj_load_size(f);


	m_addr = create_module(m_name, m_size);
	if (m_addr==-1) switch (errno) {
	case EEXIST:
		error_msg("A module named %s already exists\n", m_name);
		goto out;
	case ENOMEM:
		error_msg("Can't allocate kernel memory for module; needed %lu bytes\n",
				m_size);
		goto out;
	default:
		perror_msg("create_module: %s", m_name);
		goto out;
	}

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

	exit_status = EXIT_SUCCESS;

out:
	fclose(fp);
	return(exit_status);
}
