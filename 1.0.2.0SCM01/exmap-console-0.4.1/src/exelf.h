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
 */

#ifndef EXMAP_ELF_H
#define EXMAP_ELF_H

#include <elf.h>

typedef unsigned int         address_t;
typedef struct _ElfData      elf_t;
typedef struct _ElfSegm      eseg_t;
typedef struct _ElfSection   esec_t;
typedef struct _ElfSymbol    esym_t;

elf_t *        elf_load (const char * file, gboolean load_symbols);
void           elf_free (elf_t * e);
const GList *  elf_sections (elf_t * e);

const char *   elf_section_name (const esec_t *s);
GList *        elf_section_symbols (esec_t *s);
gboolean       elf_section_intersect (const esec_t *s,
                                      address_t * start, address_t * end,
                                      address_t vma_offset);

const char *   elf_symbol_name (const esym_t *s);
gboolean       elf_symbol_intersect (const esym_t *s,
                                     address_t * start, address_t * end,
                                     address_t vma_offset);

const eseg_t * elf_find_segment (elf_t * elf, address_t address);

#ifdef DEBUG
void           elf_print_dbg_info (elf_t * e);
#endif

#endif /* #ifndef EXMAP_ELF_H */
