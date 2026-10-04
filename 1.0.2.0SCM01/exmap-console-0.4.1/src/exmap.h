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
 *
 * Simple cmd line frontend to John Berthels <jjberthels@gmail.com>
 * exmap kernel module.
 */

#ifndef EXMAP_H
#define EXMAP_H

#include <sys/types.h>
#include <stdio.h>
#include <glib.h>

#include "exelf.h"

#define SORT_BY_MASK   0x000000ff
#define SORT_BY_RESET  ~SORT_BY_MASK
#define SORT_BY_NONE   0
#define SORT_BY_VM     1
#define SORT_BY_SOLE   2
#define SORT_BY_HEAP   3
#define SORT_BY_STACK  4
#define SORT_BY_MAPPED 5
#define SORT_BY_EMAPPED 6

#define SORT_BY_GET(x)    (x & SORT_BY_MASK)
#define SORT_BY_SET(x, v) do{x &= SORT_BY_RESET; x |= v;}while(0)

/* sort-type: decending or ascending */
#define SORT_TYPE_MASK       (1 << 8)
#define SORT_TYPE_RESET      ~SORT_TYPE_MASK
#define SORT_TYPE_DESC       (0 << 8)
#define SORT_TYPE_ASC        (1 << 8)

#define SORT_TYPE_GET(x)     (x & SORT_TYPE_MASK)
#define SORT_TYPE_SET(x, v)  do{x &= SORT_TYPE_RESET; x |= v;}while(0)

#define DETAIL_FILE_INFO (1 << 0)
#define DETAIL_ELF_INFO  (1 << 1)
#define DETAIL_SET(x,d) x |= d
#define DETAIL_UNSET(x,d) x &= ~d

#define EXMFILE "/proc/exmap"

typedef enum {
  FORMAT_HUMAN = 0,
  FORMAT_CSV,
  FORMAT_CSV_SUMS
} format_t;

typedef struct
{
  gint      id;
  address_t start;
  guint     size;
  gboolean  resident : 1;
  gboolean  writable : 1;
  gboolean  mapped   : 1;
} page_t;

typedef struct
{
  address_t  start;
  address_t  end;
  address_t  offset;
  char       perms[5];
  GList    * pages;
  gchar    * file;
  
  gboolean   heap  : 1;
  gboolean   stack : 1;
  gboolean   vdso  : 1;
  gboolean   anon  : 1;
} vma_t;

typedef struct
{
  pid_t        pid;
  guint        vm;
  guint        evm;
  guint        heap;
  guint        stack;
  guint        vdso;
  guint        mapped;
  guint        emapped;
  guint        sole;
  guint        anon;
  GHashTable * pg_count;
  GHashTable * files;
  GHashTable * map;
  gchar      * cmd;
  gboolean     load_symbols;
} memory_t;

typedef struct
{
  memory_t       m;
  const gchar  * name;
  const esym_t * esym;
}symbol_t;

typedef struct
{
  memory_t      m;
  const gchar * name;

  GList * syms;
}section_t;

typedef struct
{
  memory_t   m;
  elf_t    * elf;
  GList    * vma;   /* of vma_t */
  GList    * sects; /* of section_t */
}file_t;

typedef struct
{
  pid_t       * pids_arg;
  int           pids_count;
  char       ** pids_names;
  guint         detail;
  gboolean      all_pids;
  gboolean      interactive;
  gboolean      remote;
  guint         sort;
  guint         threshold;
  format_t      format;
  const char  * log;
  char        * fs_root;
} args_t;

typedef struct
{
  GHashTable * pg_count;
  GList      * pids_sys;
  FILE *       of;
  GList      * mem_list;
  args_t       args;
  gboolean     global_lines;
  int          srv_sckt;
}data_t;


gint     print_paged (data_t * d, char * text);
gint     get_unit (const char * p);

void     main_load (data_t * d);
void     main_reset (data_t * d);
void     main_sort (data_t * d);
void     main_print (data_t * d);

void     interactive_init (data_t * d);
void     interactive_loop (data_t * d);
gboolean interactive_parse_line (data_t *d, char * line);
void     interactive_save_history (data_t *d);

#endif /* ifndef EXMAP_H */
