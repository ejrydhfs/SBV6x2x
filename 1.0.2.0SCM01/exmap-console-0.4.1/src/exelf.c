/* -*- mode:C; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (C) 2006 OpenedHand Ltd.
 *
 * Author: Tomas Frydrych <tf@o-hand.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Functions for retrieving information for Elf files.
 *
 * Based loosely on Elf.cpp from the exmap package,
 * (c) 2005 John Berthels <jjberthels@gmail.com>
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <errno.h>
#include <glib.h>
#include <sys/stat.h>

#include "exelf.h"

/* At the moment we do not need the segment stuff for anything
 * TODO -- see if it could not be completely removed
 */
#define DISABLE_SEGMENTS

/* convenience macros for accessing elf fields */
#define EHDR_GET(e,f) ((e)->is64 ? (e)->hdr.h64->f : (e)->hdr.h32->f)
#define ESYM_GET(e,f) ((e)->is64 ? (e)->sym.s64.f : (e)->sym.s32.f)

struct _ElfSymbol
{
  gboolean is64;

  union
  {
    Elf32_Sym s32;
    Elf64_Sym s64;
  } sym;

  const char * name;

  const elf_t * elf;
};


struct _ElfData
{
  gboolean is64; /* TRUE if 64bit header */

  union
  {
    Elf32_Ehdr * h32;
    Elf64_Ehdr * h64;
  } hdr;

  FILE  * f;
  GList * sects;
  GList * segs;

  const esec_t * symsect;
};

struct _ElfSection
{
  gboolean is64;
  union
  {
    Elf32_Shdr * h32;
    Elf64_Shdr * h64;
  }hdr;

  GList * syms;

  unsigned char * str;
  unsigned long   strsize;

  const elf_t * elf;
};

static void
elf_free_sect (esec_t *s)
{
  if (!s)
    return;

  GList * l = s->syms;
  while (l)
    {
      g_free (l->data);
      l = l->next;
    }

  g_free (s->str);
  g_free (s->hdr.h32);
  g_free (s);
}

struct _ElfSegm
{
  gboolean is64;
  union
  {
    Elf32_Phdr * h32;
    Elf64_Phdr * h64;
  }hdr;

  const elf_t * elf;
};

#ifndef DISABLE_SEGMENTS
static void
elf_free_segm (eseg_t *s)
{
  if (!s)
    return;

  g_free (s->hdr.h32);
  g_free (s);
}
#endif

void
elf_free (elf_t * e)
{
  if (!e)
    return;

  g_free (e->hdr.h32);

  GList * l = e->sects;
  while (l)
    {
      elf_free_sect ((esec_t*)l->data);
      l = l->next;
    }

#ifndef DISABLE_SEGMENTS
  l = e->segs;
  while (l)
    {
      elf_free_segm (l->data);
      l = l->next;
    }
#endif
  
  g_free (e);
}

static GList *
elf_read_table (FILE * f, unsigned long offset,
                unsigned short items, unsigned short size)
{
  GList *l = NULL;
  unsigned int i;

  if (offset == 0 || items < 1 || fseek (f, offset, SEEK_SET))
    goto error;

  char *buf = (char *) alloca(items * size);

  if (items > fread (buf, size, items, f))
    goto error;

    for (i = 0; i < items; ++i)
      {
        void * v = g_malloc0(size);
        memcpy (v, buf + i * size, size);
        l = g_list_prepend (l, v);
      }

    if (l)
      l = g_list_reverse (l);
    return l;

 error:

    if (l)
      {
        GList * l2 = l;
        while (l2)
          {
            g_free (l2->data);
            l2 = l2->next;
          }

        g_list_free (l);
      }

    return NULL;
}

static gboolean
elf_get_sections (elf_t *elf)
{
  if (!elf->f)
    return FALSE;

  GList * l = elf_read_table(elf->f,
                             EHDR_GET (elf, e_shoff),
                             EHDR_GET (elf, e_shnum),
                             EHDR_GET (elf, e_shentsize));

  if (!l)
    return FALSE;

  elf->sects = l;
  
  while (l)
    {
      /* data contains the section struct; we want to replace that with
       * elfs_t struct
       */
      esec_t * s = g_malloc0(sizeof(esec_t));
      s->hdr.h32 = l->data;
      s->is64 = elf->is64;
      s->elf = elf;
      l->data = s;

      if (!elf->symsect && EHDR_GET (s, sh_type) == SHT_SYMTAB)
        elf->symsect = s;
      else if (EHDR_GET (s, sh_type) == SHT_SYMTAB)
        g_warning ("Elf file contains more than one symtable");
      
      l = l->next;
    }

  return TRUE;
}

#ifndef DISABLE_SEGMENTS
static gboolean
elf_get_segments (elf_t *elf)
{
  if (!elf->f)
    return FALSE;

  elf->segs = elf_read_table(elf->f,
                             EHDR_GET (elf, e_phoff),
                             EHDR_GET (elf, e_phnum),
                             EHDR_GET (elf, e_phentsize));

  if (!elf->segs)
    return FALSE;

  GList * l = elf->segs;
  while (l)
    {
      /* data contains the section struct; we want to replace that with
       * elfs_t struct
       */
      eseg_t * s = g_malloc0(sizeof(eseg_t));
      s->hdr.h32 = l->data;
      s->elf = elf;
      l->data = s;

      l = l->next;
    }

  return TRUE;
}
#endif

static void
elf_section_mem_range (const esec_t * s, address_t * start, address_t * end)
{
  if (!s)
    return;

  *start = (address_t)(EHDR_GET(s,sh_addr));
  *end   = (address_t)(EHDR_GET(s,sh_size) + *start);
}

/* calculates the intersection between the VM memory range
 * <*start, *end) and the segment seg for VM base at address
 * vma_base
 *
 * Return TRUE if the ranges intersect, false otherwise
 */
gboolean
elf_section_intersect (const esec_t *sec, address_t * start, address_t * end,
                       address_t vma_offset)
{
  address_t sec_start = 0, sec_end = UINT_MAX;
  elf_section_mem_range (sec, &sec_start, &sec_end);
  
  address_t vma_base = *start - vma_offset;
  address_t sect_base = sec_start - EHDR_GET (sec,sh_offset);
  address_t adjust = vma_base - sect_base;
  
  sec_start += adjust;
  sec_end   += adjust;
  
  if (*start >= sec_end || *end   <  sec_start)
    return FALSE;

  *start = MAX (*start, sec_start);
  *end   = MIN (*end,   sec_end);

  return (*start < *end) ? TRUE : FALSE;
}

/* calculates the intersection between the VM memory range
 * <*start, *end) and the segment seg for VM base at address
 * vma_base
 *
 * Return TRUE if the ranges intersect, false otherwise
 */
gboolean
elf_symbol_intersect (const esym_t *sym,
                      address_t * start, address_t * end, address_t vma_offset)
{
  if (!sym || !sym->elf || !sym->elf->sects)
    return FALSE;

  address_t sym_start = ESYM_GET (sym, st_value);
  address_t sym_end   = sym_start + ESYM_GET (sym, st_size);

  if (*start >= sym_end || *end   <  sym_start)
    return FALSE;
  
  *start = MAX (*start, sym_start);
  *end   = MIN (*end,   sym_end);

  return (*start < *end) ? TRUE : FALSE;
}

static const esec_t *
elf_get_section (const elf_t * e, int indx)
{
  return g_list_nth_data (e->sects, indx);
}

/* loads symbols into the sections sym GList*/
static void
elf_symsection_load_symbols (esec_t *s)
{
  GList * l;
  
  /* sanity checks */
  if (!s                                                   ||
      EHDR_GET (s, sh_type) != SHT_SYMTAB                  ||
      !EHDR_GET (s, sh_size)                               ||
      EHDR_GET (s, sh_size) % EHDR_GET (s, sh_entsize) != 0)
    {
      g_warning ("Invalid symbol section");
      return;
    }

  if (fseek (s->elf->f, EHDR_GET (s, sh_offset), SEEK_SET))
    {
      g_warning ("Seek in symtable failed");
      return;
    }

  unsigned long long i;
  for (i = 0; i <  EHDR_GET (s, sh_size); i += EHDR_GET (s, sh_entsize))
    {
      esym_t * sym = g_malloc0 (sizeof (esym_t));
      sym->is64 = s->is64;
      sym->elf = s->elf;
      
      void * d = sym->is64 ? (void*)&(sym->sym.s64) : (void*)&(sym->sym.s32);
      
      if (1 > fread (d, EHDR_GET (s, sh_entsize), 1, s->elf->f))
        {
          g_free (sym);
          g_warning ("Failed to read symtable");
          goto error;
        }

      const esec_t * strtbl = elf_get_section (s->elf, EHDR_GET (s, sh_link));

      if (strtbl)
        {
          if (strtbl->str && ESYM_GET (sym, st_name) < strtbl->strsize)
            {
              sym->name = (char*)(strtbl->str + ESYM_GET (sym, st_name));
#if 0
              int type = sym->is64 ?
                ELF64_ST_TYPE (sym->sym.s64.st_info) :
                ELF32_ST_TYPE (sym->sym.s32.st_info);

                g_debug ("loaded symbol [%s] type %d", sym->name, type);
#endif
            }
          else
            {
              g_warning ("could not load symbol; tbl %ld, offset %d",
                         strtbl->strsize, ESYM_GET (sym, st_name));
            }
        }
      
      s->syms = g_list_prepend (s->syms, sym);
    }

  if (s->syms)
    s->syms = g_list_reverse (s->syms);
  return;
  
 error:
  l = s->syms;
  while (l)
    {
      g_free (l->data);
      l = l->next;
    }

  g_list_free (s->syms);
  s->syms = NULL;
}

static void
elf_section_get_strings (esec_t * s)
{
  if (EHDR_GET (s, sh_type) == SHT_STRTAB)
    {
      if (fseek (s->elf->f, EHDR_GET (s, sh_offset), SEEK_SET))
        {
          g_warning ("Seek in strtable faile");
        }
      else
        {
              
          s->strsize = EHDR_GET(s,sh_size);
          s->str = g_malloc0 (s->strsize);

          if (1 > fread (s->str, s->strsize, 1, s->elf->f))
            {
              g_free (s->str);
              s->str = NULL;
              s->strsize = 0;
              g_warning ("Falied to read string table");
            }
        }
    }
}

/*
 * Returns a GList of symbols in the given section that are in memory
 * range <start, end).
 *
 * The caller must free the GList when no longer needed, but *must not*
 * free the list contents.
 */
static GList *
elf_symsection_symbols_in_range (const esec_t *s,
                                 address_t start, address_t end)
{
  GList * l = s->syms;
  GList * r = NULL;

  while (l)
    {
      esym_t * sym = l->data;

      address_t sym_start = ESYM_GET (sym, st_value);
      address_t sym_end   = sym_start +  ESYM_GET (sym, st_size);

      if (sym_start >= start && sym_end <= end)
        r = g_list_prepend (r, sym);
      
      l = l->next;
    }

  if (r)
    r = g_list_reverse (r);

  return r;
}

/*
 * Returns a GList of symbols that belong to this section.
 *
 * The caller must free the GList when no longer needed, but *must not*
 * free the list contents.
 */
static GList *
elf_section_get_symbols (esec_t * s)
{
  if (!s || !s->elf->symsect)
    return NULL;
  
  address_t start = EHDR_GET (s, sh_addr);
  address_t end   = start + EHDR_GET (s, sh_size);
  return elf_symsection_symbols_in_range (s->elf->symsect, start, end);
}

GList *
elf_section_symbols (esec_t *s)
{
  if (!s)
    return NULL;

  return s->syms;
}

elf_t *
elf_load (const char * file, gboolean load_symbols)
{
  elf_t * elf = g_malloc0 (sizeof(elf_t));

  struct stat stb;
  if (0 > stat (file, &stb) ||
      !(S_ISREG(stb.st_mode) || S_ISLNK(stb.st_mode)))
    return NULL;

  elf->f = fopen (file, "rb");

  if (!elf->f)
    {
      g_warning ("Failed to open [%s]", file);
      return NULL;
    }

  char buf[EI_NIDENT + 1];
  memset(buf, 0, sizeof(buf));

  if (1 > fread (buf, EI_NIDENT, 1, elf->f))
    {
      g_warning ("Could not read magic; error no %d [%s]", errno, file);
      goto error;
    }

  if (strncmp (buf, ELFMAG, 4))
    {
      /* not an elf file -- do not want to warn, this is OK */
      goto error;
    }

  if (fseek (elf->f, 0, SEEK_SET))
    {
      g_warning ("Seek failed\n");
      goto error;
    }

  switch (buf[EI_CLASS])
    {
    case ELFCLASS32:
      elf->hdr.h32 = g_malloc0(sizeof(Elf32_Ehdr));
      if (1 > fread (elf->hdr.h32, sizeof(Elf32_Ehdr), 1, elf->f))
        {
          g_warning ("Header read failed");
          goto error;
        }

      break;

    case ELFCLASS64:
      elf->hdr.h64 = g_malloc0(sizeof(Elf64_Ehdr));
      elf->is64 = TRUE;
      if (1 > fread (elf->hdr.h64, sizeof(Elf64_Ehdr), 1, elf->f))
        {
          g_warning ("Header read failed (64)");
          goto error;
        }
      break;

    default:
      goto error;
    }

  if (!elf_get_sections (elf)
#ifndef DISABLE_SEGMENTS
      || !elf_get_segments (elf)
#endif
      )
    {
      g_warning ("Failed reading sections and segments\n");
      goto error;
    }

  /* GList * l = g_list_nth (elf->sects, EHDR_GET (elf,e_shstrndx));*/
  GList * l = elf->sects;
  
  while (l)
    {
      esec_t * s = (esec_t*)l->data;
      elf_section_get_strings (s);
      l = l->next;
    }

  if (load_symbols)
    {
      l = elf->sects;
      while (l)
        {
          esec_t * s = l->data;

          if (EHDR_GET (s, sh_type) == SHT_SYMTAB)
            {
              elf_symsection_load_symbols (s);
            }

          l = l->next;
        }
    }
  
  fclose (elf->f);
  elf->f  = NULL;
  return elf;

 error:
  if (elf->f)
    {
      fclose (elf->f);
      elf->f  = NULL;
    }

  if (elf)
    elf_free (elf);

  return NULL;
}

#ifndef DISABLE_SEGMENTS
const eseg_t *
elf_find_segment (elf_t * elf, address_t address)
{
  if (!elf)
    return NULL;

  GList * l = elf->segs;
  while (l)
    {
      eseg_t * s = (eseg_t *)l->data;

      if (EHDR_GET(s, p_vaddr) <= address &&
          EHDR_GET(s, p_vaddr)+ EHDR_GET(s, p_memsz) > address)
        return s;

      l = l->next;
    }

  return NULL;
}
#endif

const char *
elf_section_name (const esec_t *s)
{
  if (!s)
    return NULL;

  unsigned int nindx  = EHDR_GET (s, sh_name);
  const esec_t * strtbl = (const esec_t*) g_list_nth_data (s->elf->sects,
                                                EHDR_GET (s->elf, e_shstrndx));

  g_return_val_if_fail (strtbl, NULL);
  
  const char * name = (strtbl->str && nindx < strtbl->strsize) ?
    (const char*)(strtbl->str + nindx) : "";

  return name;
}

const char *
elf_symbol_name (const esym_t *s)
{
  if (!s)
    return NULL;

  return s->name;
}


#ifdef DEBUG
static void
elf_symbol_print (esym_t *s)
{
  address_t start = ESYM_GET (s, st_value);
  address_t end   = start + ESYM_GET (s, st_size);
  
  g_print ("Symbol %s [0x%x:0x%x]\n", s->name, start, end);
}

static void
elf_section_print (esec_t *s)
{
  if (!s || !s->elf)
    return;

  unsigned int nindx  = EHDR_GET (s, sh_name);
  unsigned int type   = EHDR_GET (s, sh_type);
  unsigned int offset = EHDR_GET (s, sh_offset);
  unsigned int size   = EHDR_GET (s, sh_size);
  unsigned int esize  = EHDR_GET (s, sh_entsize);
  unsigned int addr   = EHDR_GET (s, sh_addr);
  
  const char * name = elf_section_name (s);

  g_print ("Section name [%s](%u), type 0x%x, address 0x%x, size %d (%d), offset 0x%x\n",
           name, nindx, type, addr, size, esize, offset);


  GList * syms = elf_section_get_symbols (s);
  GList * l = syms;

  while (l)
    {
      esym_t * sym = l->data;
      elf_symbol_print (sym);
      l = l->next;
    }
}

#ifndef DISABLE_SEGMENTS
static void
elf_segment_print (eseg_t *s)
{
  if (!s || !s->elf)
    return;

  unsigned int type   = EHDR_GET (s, p_type);
  unsigned int offset = EHDR_GET (s, p_offset);
  unsigned int vaddr  = EHDR_GET (s, p_vaddr);
  unsigned int paddr  = EHDR_GET (s, p_paddr);
  unsigned int filesz = EHDR_GET (s, p_filesz);
  unsigned int memsz  = EHDR_GET (s, p_memsz);
  unsigned int flags  = EHDR_GET (s, p_flags);
  unsigned int align  = EHDR_GET (s, p_align);

  g_print ("Segment: type 0x%x, offset 0x%x, vaddr 0x%x, paddr 0x%x, "
           "filesz %u, memsz %u, flags 0x%x, align 0x%x\n",
           type, offset, vaddr, paddr, filesz, memsz, flags, align);
}
#endif

void
elf_print_dbg_info (elf_t * e)
{
  if (!e)
    return;

  g_print ("Elf: str table index %d, sections %d, segments %d\n",
           EHDR_GET (e,e_shstrndx),
           e->sects ? g_list_length (e->sects) : 0,
           e->segs  ? g_list_length (e->segs)  : 0);

  GList * l = e->sects;
  int i = 0;
  while (l)
    {
      g_print ("section %d\n", i);
      i++;
      
      esec_t * s = (esec_t*)l->data;
      elf_section_print (s);
      l = l->next;
    }

#ifndef DISABLE_SEGMENTS
  l = e->segs;
  while (l)
    {
      eseg_t * s = (eseg_t*)l->data;
      elf_segment_print (s);
      l = l->next;
    }
#endif
}
#endif

const GList *
elf_sections (elf_t * e)
{
  if (!e)
    return NULL;

  if (!e->sects)
    g_warning ("No sections in this ELF");

  /* typically the first section is of type null, in which case, we
   * ingnore it
   */
  esec_t * s0 = (esec_t*) e->sects->data;
  if (EHDR_GET (s0, sh_type) == SHT_NULL)
    return e->sects->next;

  return e->sects;
}


