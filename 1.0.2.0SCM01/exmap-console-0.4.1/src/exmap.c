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

/*-----------------------------------------------------------------------------------------------------
 * Copyright 2008, Texas Instruments Incorporated
 *
 * This program has been modified from its original operation by Texas Instruments
 * to do the following:
 *       Support for non-glibc system
 *
 *   THIS MODIFIED SOFTWARE AND DOCUMENTATION ARE PROVIDED
 *   "AS IS," AND TEXAS INSTRUMENTS MAKES NO REPRESENTATIONS 
 *   OR WARRENTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED 
 *   TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY 
 *   PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR 
 *   DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS, 
 *   COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. 
 *
 *---------------------------------------------------------------------------------------------------*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

#include <glib/gprintf.h>

#include "exmap.h"

#ifdef INTERACTIVE
#include <readline/readline.h>
#endif

#define EXIT_KERNEL_ERROR  -1
#define EXIT_LOGFILE_ERROR -2
#define EXIT_SYSDATA_ERROR -3
#define EXIT_INVALID_ARGS  -4

#define BUF_SIZE 256

#define CSV_COLUMNS 18

#define DISP_KB_ONLY
#ifdef DISP_KB_ONLY
#define SCALE_V(x) \
    x >> 10
#define SCALE_U(x) \
    "KB"
#else /* DISP_KB_ONLY */
#if 1
#define SCALE_V(x) \
    x > 104857600 ? x >> 20 : \
    x > 102400 ? x >> 10 : x

#define SCALE_U(x) \
    x > 104857600 ? "MB" : \
    x > 102400 ? "KB" : " B"
#else
#define SCALE_V(x) x
#define SCALE_U(x) "B"
#endif
#endif

#ifdef INTERACTIVE
extern int rows;
extern int columns;
#endif

static data_t data;

static gboolean
myprintf (data_t *d, const char *fmt, ...)
{
  va_list args;

  if (! (d && d->of && fmt))
    return FALSE;

#ifdef INTERACTIVE
  if (d->args.interactive && d->global_lines >= rows - 1)
    {
      char * line;
      gboolean done = FALSE;
      
      d->global_lines = 0;

      /* read single character from kbd; reset back to reading
       * entire line afterwards
       */
      rl_num_chars_to_read = 1;
      line = readline ("  --- Press Return for more or q to quit ---");
      rl_num_chars_to_read = 0;
      
      if (*line == 'q' || *line == 'Q')
        done = TRUE;

      free (line);

      if (done)
        return FALSE;
    }
#endif
  
  va_start (args, fmt);
  g_vfprintf (d->of, fmt, args);
  va_end (args);

  d->global_lines++;

  return TRUE;
}

gint
print_paged (data_t * d, char * text)
{
  if (!d || !text)
    return 0;

  d->global_lines = 0;
  
  char * t = text;
  char * nl = strchr (t, '\n');
  guint count = 0;
  
  while (nl)
    {
      ++count;
      *nl = 0;

      if (!myprintf (d, "%s\n", t))
        return -1;
      
      t = nl + 1;
      
      nl = strchr (t, '\n');
    }
  

  if (*t)
    {
      if (!myprintf (d, "%s\n", t))
        return -1;
      
      ++count;
    }

  return count;
}


address_t
page_start (address_t addr)
{
  return addr & ~(getpagesize()-1);
}

static void
vma_free (gpointer p)
{
  vma_t * v  = (vma_t*)p;
  
  if (!v)
    return;
  
  if (v->pages)
    {
      GList * n = v->pages;
      while (n)
	{
          g_free (n->data);
          n = n->next;
	}
	
      g_list_free (v->pages);
    }

  g_free (v->file);
  g_free (v);
}

/* frees contents of the memory struct, but not the struct
 * itself
 */
static void
memory_destroy (memory_t * m)
{
  if (!m)
    return;
  
  if (m->map)
    {
      g_hash_table_destroy (m->map);
      m->map = NULL;
    }
  
  if (m->files)
    {
      g_hash_table_destroy (m->files);
      m->files = NULL;
    }
  
  
  g_free (m->cmd);
}

static void
memory_free (memory_t * m)
{
  if (!m)
    return;

  memory_destroy (m);
  g_free (m);
}

static void
symbol_free (symbol_t * s)
{
  if (!s)
    return;

  memory_destroy (&(s->m));

  g_free (s);
}

static void
section_free (section_t * s)
{
  if (!s)
    return;

  memory_destroy (&(s->m));

  if (s->syms)
    {
      GList * l = s->syms;
      while (l)
        {
          symbol_t * sym = l->data;
          symbol_free (sym);
      
          l = l->next;
        }

      g_list_free (s->syms);
    }
  
  g_free (s);
}

static void
file_free (file_t * f)
{
  if (!f)
    return;

  memory_destroy (&(f->m));
  elf_free (f->elf);

  if (f->vma)
    g_list_free (f->vma); /* do not free the contents though */

  GList * l = f->sects;
  while (l)
    {
      section_t * s = (section_t*) l->data;
      section_free (s);
      l = l->next;
    }

  if (f->sects)
    g_list_free (f->sects);
  
  g_free (f);
}

/* Goes through all the pages within this vma area and adds up memory
 * amounts; increases the page counter for any mapped pages
 *
 * Called for each entry in one pid's vma hash; data contains pointer to
 * a memory struct for this particular pid.
 */
static void
process_vma (gpointer key, gpointer value, gpointer user_data)
{
  vma_t    * v  = (vma_t*) value;
  memory_t * m  = (memory_t*)user_data;
  GList    * l  = v->pages;
  file_t * fm = NULL;
  
  if (v->file)
    {
      if (!(fm = g_hash_table_lookup (m->files, v->file)))
        {
          fm = g_malloc0(sizeof(file_t));

          /* store the file name in the cmd field, it comes handy later */
          fm->m.cmd = g_strdup (v->file);

          /* Evil global variable ... */
          if (data.srv_sckt < 0 || data.args.fs_root)
            fm->elf = elf_load (v->file, m->load_symbols);
          
          fm->m.pg_count = m->pg_count;
          
          g_hash_table_insert (m->files, g_strdup (v->file), fm);
        }

      fm->vma = g_list_prepend (fm->vma, v);
    }
  
  while (l)
    {
      page_t * p = (page_t*)l->data;
      int      count = 1;
      if (p->id)
	{
          gpointer ptr = g_hash_table_lookup (m->pg_count,
                                              GINT_TO_POINTER (p->id));
          count = GPOINTER_TO_INT (ptr);

          if (count == 1)
            {
              m->sole += p->size;
              
              if (fm)
                fm->m.sole += p->size;
            }
        }
      
      m->vm += p->size;
      m->evm += p->size / count;
      
      if (fm)
        {
          fm->m.vm += p->size;
          fm->m.evm += p->size / count;
        }
      
      if (p->mapped)
        {
          m->mapped += p->size;
          m->emapped += (p->size / count);
          
          if (fm)
            {
              fm->m.mapped += p->size;
              fm->m.emapped += (p->size / count);
            }
        }
      
      if (v->vdso)
        m->vdso += p->size;
      
      if (v->heap)
        m->heap += p->size;

      if (v->stack)
        m->stack += p->size;
      
      if (v->anon)
        m->anon += p->size;
      
      l = l->next;
    }
}

static guint
page_in_range (page_t * p, address_t start, address_t end)
{
  if (!p->size)
    return 0;

  if (p->start > end || p->start + p->size <= start)
    return 0;
  
  address_t r_start = MAX (p->start, start);
  address_t r_end   = MIN (p->start + p->size, end);

  return (r_end - r_start);
}

static void
process_pages_in_range (memory_t * m, GList * pages,
                        address_t start, address_t end)
{
  GList * l = pages;

  while (l)
    {
      page_t   * p = (page_t*)l->data;
      guint range_size = 0;
      
      if (0 != (range_size = page_in_range (p, start, end)))
        {
          if (p->id)
            {
              gpointer ptr = g_hash_table_lookup (m->pg_count,
                                                  GINT_TO_POINTER (p->id));
              int count = GPOINTER_TO_INT (ptr);

              if (count == 1)
                m->sole += range_size;
            }
      
          m->vm += range_size;
      
          if (p->mapped)
            m->mapped += range_size;
        }
      
      l = l->next;
    }
}

static void
process_files (gpointer key, gpointer value, gpointer data)
{
  if (! value)
    return;
  
  file_t * f = (file_t *) value;

  if (!f->elf)
    return;
  
  const GList *l, * sects = elf_sections (f->elf);
  
  l = sects;

  while (l)
    {
      esec_t * s = (esec_t*) l->data;
      section_t * sec = g_malloc0 (sizeof (section_t));

      sec->name = elf_section_name (s);
      sec->m.pg_count = f->m.pg_count;

      /* create any symbol data structures */
      GList * k = elf_section_symbols (s);
      while (k)
        {
          esym_t    * esbl = k->data;
          symbol_t  * sbl = g_malloc0 (sizeof (symbol_t));
          
          sbl->esym = esbl;
          sbl->name = elf_symbol_name (esbl);
          sbl->m.pg_count = f->m.pg_count;
          
          sec->syms = g_list_prepend (sec->syms, sbl);
          k = k->next;
        }

      if (sec->syms)
        sec->syms = g_list_reverse (sec->syms);
      
      if (!sec->m.pg_count)
        g_warning ("Missing pg_count hash");
      
      f->sects = g_list_prepend (f->sects, sec);

      k = f->vma;
      while (k)
        {
          vma_t * v = (vma_t*)k->data;
          address_t start = v->start;
          address_t end   = v->end;

          if (start != end)
            {
              /* NB: the order here matters, as elf_section_interset()
               * modifies the start and end values
               */
              if (sec->syms)
                {
                  /* This is a symtable; process the symbols
                   *
                   * TODO -- is there a more efficient way of doing this ?
                   */
                  GList * syms = sec->syms;
                  while (syms)
                    {
                      symbol_t  * sbl = syms->data;
                      address_t   start2 = start;
                      address_t   end2 = end;
                      
                      if (elf_symbol_intersect (sbl->esym,
                                                &start2, &end2, v->offset))
                        {
                          process_pages_in_range (&(sbl->m),
                                                  v->pages, start2, end2);
                        }
                      
                      syms = syms->next;
                    }
                }
              else if (elf_section_intersect (s, &start, &end, v->offset))
                {
                  process_pages_in_range (&(sec->m), v->pages, start, end);
                }
            }
          
          k = k->next;
        }

      l = l->next;
    }
}

static gint
print_symbol_info (symbol_t * s, data_t * d)
{
  if (!s || !s->m.vm)
    return 0;

  guint lines = 0;
  
  if (d->args.format == FORMAT_HUMAN || d->args.interactive)
    {
      if (!myprintf (d, "      %-26s: VM%8u %s, M%8u %s, S%8u %s\n",
                     s->name,
                     SCALE_V(s->m.vm),     SCALE_U(s->m.vm),
                     SCALE_V(s->m.mapped), SCALE_U(s->m.mapped),
                     SCALE_V(s->m.sole),   SCALE_U(s->m.sole)))
        return -1;
      
      lines++;
    }
  else if (d->args.format == FORMAT_CSV || d->args.format == FORMAT_CSV_SUMS)
    {
      if (!myprintf (d, ",,,,,,,,,,,,\"%s\",%u,%u,%u\n",
                     s->name, 
                     s->m.vm,
                     s->m.mapped,
                     s->m.sole))
        return -1;
      
      lines++;
    }

  return lines;
}

/* TODO refactor these print fuctions to share code */
static gint
print_section_info (section_t * s, data_t * d)
{
  if (!s)
    return 0;

  guint lines = 0;

  if (s->m.vm)
    {
      if (d->args.format == FORMAT_HUMAN || d->args.interactive)
        {
          if (!myprintf (d,"   %-29s: VM%8u %s, M%8u %s, S%8u %s\n",
                         s->name,
                         SCALE_V(s->m.vm),     SCALE_U(s->m.vm),
                         SCALE_V(s->m.mapped), SCALE_U(s->m.mapped),
                         SCALE_V(s->m.sole),   SCALE_U(s->m.sole)))
            return -1;
      
          lines++;
        }
      else if (d->args.format == FORMAT_CSV ||
               d->args.format == FORMAT_CSV_SUMS)
        {
          if (!myprintf (d,",,,,,,,,,,,,\"%s\",%u,%u,%u\n",
                         s->name, 
                         s->m.vm,
                         s->m.mapped,
                         s->m.sole))
            return -1;

          lines++;
        }
    }
  else if (s->syms)
    {
      if (d->args.format == FORMAT_HUMAN || d->args.interactive)
        {
          if (!myprintf (d,"   %-29s:\n", s->name))
            return -1;
          
          lines++;
        }
      else if (d->args.format == FORMAT_CSV ||
               d->args.format == FORMAT_CSV_SUMS)
        {
          if (!myprintf (d,",,,,,,,,,,,,\"%s\",0,0,0\n", s->name))
            return -1;
          
          lines++;
        }
    }
  
  GList * l = s->syms;
  while (l)
    {
      symbol_t * sbl = l->data;
      int i = print_symbol_info (sbl, d);

      if (i < 0)
        return -1;
      
      lines += i;
      l = l->next;
    }
  
  return lines;
}

static gint
print_file_info (file_t * f, data_t * d)
{
  if (!f)
    return 0;

  guint lines = 0;
  gchar * cmd = g_path_get_basename (f->m.cmd);
  
  if (d->args.format == FORMAT_HUMAN || d->args.interactive)
    {
      if (d->args.detail & DETAIL_ELF_INFO)
        {
          if (!myprintf (d,"  ----------------------------------"
                         "---------------------------------------\n"))
            return -1;
          
          lines++;
        }

      if (!myprintf (d,"  %-30s: VM%8u %s, M%8u %s, S%8u %s ",
                     cmd,
                     SCALE_V(f->m.vm),     SCALE_U(f->m.vm),
                     SCALE_V(f->m.mapped), SCALE_U(f->m.mapped),
                     SCALE_V(f->m.sole),   SCALE_U(f->m.sole)))
        return -1;
      
      d->global_lines--;
      
#if 0
      elf_print_dbg_info (f->elf);
#endif
      if (d->args.detail & DETAIL_ELF_INFO)
        {
          lines += 2;
          if (!myprintf (d,"\n  ----------------------------------"
                         "---------------------------------------\n"))
            return -1;
          
          d->global_lines++;
        }
      else
        {
          lines++;
          if (!myprintf (d,"\n"))
            return -1;
        }
    }
  else if (d->args.format == FORMAT_CSV || d->args.format == FORMAT_CSV_SUMS)
    {
      if (!myprintf (d,",,,,,,,,\"%s\",%u,%u,%u\n",
                     f->m.cmd, 
                     f->m.vm,
                     f->m.mapped,
                     f->m.sole))
        return -1;

      lines++;
    }

  g_free (cmd);

  if (d->args.detail & DETAIL_ELF_INFO)
    {
      GList * l = f->sects;
      while (l)
        {
          section_t * s= (section_t*)l->data;
          int i = print_section_info (s, d);

          if (i < 0)
            return -1;
          
          lines += i;
          l = l->next;
        }
    }

  return lines;
}

#ifdef INTERACTIVE
static gboolean
srv_cmd_done (data_t * d, const char * buf)
{
  if (d->srv_sckt < 0)
    return FALSE;
  
  if (!strncmp (buf, "EXMP_DONE", 9))
    return TRUE;

  if (!strncmp (buf, "EXMP_ERROR", 10))
    {
      printf("Server error: %s\n", buf + 11);
      return TRUE;
    }

  return FALSE;
}
#endif

/* loads a list of accessible pids from /proc
 */
static GList *
load_pids (data_t * d)
{
  GList       * pids = NULL;

#ifdef INTERACTIVE
  if (d->srv_sckt >= 0)
    {
      char buf[256];
      g_snprintf (buf, sizeof (buf), "EXMP_CMD SYSPIDS\n");
      write (d->srv_sckt, buf, strlen (buf));

      FILE * f = fdopen (dup (d->srv_sckt), "r");
      
      while (fgets (buf, sizeof (buf), f))
        {
#ifdef INTERACTIVE
          if (srv_cmd_done (d, buf))
            break;
#endif
          pid_t pid = atoi (buf);
          
          if (pid)
            pids = g_list_prepend (pids, GINT_TO_POINTER (pid));
        }

      fclose (f);
    }
  else
#endif
    {
      const gchar * n;
      GDir        * dir = g_dir_open ("/proc", 0, NULL);

      if (!dir)
        return NULL;

      while ((n = g_dir_read_name (dir)) != NULL)
        {
          if (isdigit(*n))
            pids = g_list_prepend (pids, GINT_TO_POINTER (atoi (n)));
        }

      g_dir_close (dir);
    }
    
  return pids;
}


/* loads the contents of /proc/pid/maps for given pid into a hastable
 */
static GHashTable *
load_map (data_t * d, pid_t pid)
{
  GHashTable * map_hash = g_hash_table_new_full (NULL, NULL, NULL, vma_free);
  gboolean     error = FALSE;
  FILE       * fmaps = NULL;
  gchar        buf[BUF_SIZE];

#ifdef INTERACTIVE
  if (d->srv_sckt >= 0)
    {
      fmaps = fdopen (dup (d->srv_sckt), "r");
      g_snprintf (buf, sizeof (buf), "EXMP_CMD PIDMAPS %d\n", pid);
      write (d->srv_sckt, buf, strlen (buf));
    }
  else
#endif
    {
      gchar * map   = g_strdup_printf ("/proc/%d/maps", pid);
      fmaps = fopen (map, "r");

      if (!fmaps)
        {
          g_warning ("unable to open [%s]", map);
          error = TRUE;
          goto finish;
        }

      g_free (map);
    }
    
  while (fgets(buf, sizeof(buf), fmaps))
    {
#ifdef INTERACTIVE
      if (srv_cmd_done (d, buf))
        break;
#endif
      
      vma_t * v = g_malloc0 (sizeof(vma_t));
      gchar * b = buf;
      gchar * p = strchr (b, '-');
      
      if (!p)
	{
          g_warning ("%d: misformated maps line [%s]", __LINE__, buf);
          error = TRUE;
          goto finish;
	}
	
      *p = 0;
      p++;
	
      v->start = strtoul (b, NULL, 16);
      
      b = p;
      p = strchr (b, ' ');

      if (!p)
	{
          g_warning ("%d: misformated maps line [%s]", __LINE__, buf);
          error = TRUE;
          goto finish;
	}

      *p  = 0;
      p++;

      v->end   = strtoul (b, NULL, 16);

      b = p;
      p = strchr (b, ' ');

      if (!p)
	{
          g_warning ("%d: misformated maps line [%s]", __LINE__, buf);
          error = TRUE;
          goto finish;
	}

      *p  = 0;
      p++;

      strncpy (v->perms, b, 4);

      /* offset */
      b = p;
      p = strchr (b, ' ');

      if (!p)
	{
          g_warning ("%d: misformated maps line [%s]", __LINE__, buf);
          error = TRUE;
          goto finish;
	}

      *p = 0;
      p++;

      v->offset   = strtoul (b, NULL, 16);
	
      /* token 1 */
      b = p;
      p = strchr (b, ' ');

      if (!p)
	{
          g_warning ("%d: misformated maps line [%s]", __LINE__, buf);
          error = TRUE;
          goto finish;
	}

      p++;

      /* token 2 */
      b = p;
      p = strchr (b, ' ');

      if (!p)
	{
          g_warning ("%d: misformated maps line [%s]", __LINE__, buf);
          error = TRUE;
          goto finish;
	}

      p++;

      /* file */
      b = p;

      while (isspace(*b))
        b++;

      if (*b && *b != '\n')
	{
          p = strchr (b, '[');

          if (p)
	    {
              p++;
              if (!strncmp(p, "heap",4))
                v->heap = TRUE;
              else if (!strncmp(p, "vdso",4))
                v->vdso = TRUE;
              else if (!strncmp(p, "stack",5))
                v->stack = TRUE;
	    }
          else
            {
              char * n = strchr (b, '\n');

              if (n)
                *n = 0;

              /* If we are not connected to remote sever, or no FS root
               * has been specified, we simply copy the file name.
               *
               * If we both have FS root and are connected, we prefix the
               * file name with the root
               */
              if (!d->args.fs_root || d->srv_sckt < 0)
                v->file = g_strdup (b);
              else
                v->file = g_build_filename (d->args.fs_root, b, NULL);
	    }
	}
      else
        v->anon = TRUE;

      g_hash_table_insert (map_hash, GINT_TO_POINTER(v->start), v);
    }

 finish:
  if (fmaps)
    fclose (fmaps);

  if (error && map_hash)
    {
      g_hash_table_destroy (map_hash);
      map_hash = NULL;
    }
  
  return map_hash;
}

/* Loads the data for given pid from /proc/exmap
 *
 * Return: pointer to our memory struct representing the current pid
 *
 * NB: this function can only be called after load_map () has been called
 *     for all accessible pids.
 */
static memory_t *
load_pid_data (data_t * d,
               pid_t pid,
               GHashTable * map_hash,
               GHashTable * pg_count)
{
  gchar      buf[BUF_SIZE];
  gchar    * cmd_f;
  vma_t    * cur_vma  = NULL;
  gint       cur_page = 0;
  gboolean   error = FALSE;
  memory_t * mem_data;
  FILE *     fexmp = NULL;
  FILE *     fcmd = NULL;
  
  g_return_val_if_fail (d && pid && map_hash && pg_count, NULL);
  
  mem_data = g_malloc0(sizeof(memory_t));
  mem_data->pid = pid;
  mem_data->pg_count = pg_count;
  mem_data->map = map_hash;
  mem_data->files = g_hash_table_new_full (g_str_hash,g_str_equal,
                                           g_free, (GDestroyNotify)file_free);

#ifdef INTERACTIVE
  if (d->srv_sckt >= 0)
    {
      /* remote server, we request the contents to be mirrored into
       * our socket
       */
      fcmd = fdopen (dup (d->srv_sckt), "r");
      g_snprintf (buf, sizeof (buf), "EXMP_CMD PIDCMDLINE %d\n", pid);
      write (d->srv_sckt, buf, strlen (buf));
    }
  else
#endif
    {
      cmd_f = g_strdup_printf ("/proc/%d/cmdline",pid);
      fcmd = fopen (cmd_f, "r");
      g_free (cmd_f);
    }
    
  if (fcmd)
    {
      while (fgets (buf, sizeof (buf), fcmd))
        {
#ifdef INTERACTIVE
          if (srv_cmd_done (d, buf))
            break;
#endif
          gchar * n = strchr (buf, '\n');
          if (n)
            *n = 0;

          mem_data->cmd = g_strdup (buf);

          /* see if this process is among named processes given to us on the
           * cmd line
           */
          if (mem_data->cmd && *mem_data->cmd &&
              d->args.pids_names && d->args.pids_names[0])
            {
              /* Unfortunately, g_path_basename() does not work if the
               * cmd line has any parameters, so we extract the process name
               * ourselves.
               */
              gchar * c = mem_data->cmd;
              gchar * name = mem_data->cmd;

              while ((c = strchr (c, '/')))
                {
                  c++;
                  name = c;
                }
      
              char ** p = d->args.pids_names;

              while (*p)
                {
                  if (!strncmp (name, *p, strlen (*p)))
                    {
                      /* add space for the new pid
                       * 2 == 1 for new pid 1 for NULL terminator
                       */
                      d->args.pids_arg = g_realloc (d->args.pids_arg,
                                                    d->args.pids_count + 2);
                      
                      d->args.pids_arg[d->args.pids_count] = pid;
                      d->args.pids_count++;
                      d->args.pids_arg[d->args.pids_count] = 0;
                    }
                  
                  p++;
                }
            }
          
        }
      
      fclose (fcmd);
    }

#ifdef INTERACTIVE
  if (d->srv_sckt >= 0)
    {
      /* remote server, we request the contents to be mirrored into
       * our socket
       */
      fexmp = fdopen (dup (d->srv_sckt), "r");
      g_snprintf (buf, sizeof (buf), "EXMP_CMD PIDDATA %d\n", pid);
      write (d->srv_sckt, buf, strlen (buf));
    }
  else
#endif
    {
      /* local data */
      fexmp = fopen (EXMFILE, "w+");
      if (!fexmp)
        {
          /* This is an irrecoverable error, so we just exit */
          g_warning ("Could not open [%s]. Is exmap.ko loaded ?\n", EXMFILE);
          exit (EXIT_KERNEL_ERROR);
        }

      if (EOF == fprintf (fexmp, "%d", pid))
        {
          g_warning ("unable to write pid %d to [%s]",
                     pid, EXMFILE);

          error = TRUE;
          goto finish;
        }
    }
    
  while (fgets (buf, sizeof(buf), fexmp))
    {
#ifdef INTERACTIVE
      if (srv_cmd_done (d, buf))
        break;
#endif
      if (!strncmp (buf, "VMA ", 4))
	{
          gint a = strtoul (buf + 4, NULL, 16);
          vma_t * v = (vma_t*) g_hash_table_lookup (map_hash,
                                                    GINT_TO_POINTER(a));

          if (v)
	    {
              cur_vma = v;
              cur_page = 0;
	    }
          else
	    {
              g_warning ("VMA not found in map_hash [%s]", buf);
              cur_vma = NULL;
	    }
	}
      else if (cur_vma)
	{
          page_t * pg = g_malloc0 (sizeof(page_t));
          
          cur_vma->pages = g_list_append (cur_vma->pages, pg);
          
          gchar * b = buf;
          gchar * p = strchr (b, ' ');

          if (!p)
            {
              g_warning ("Expected resident field missing");
              continue;
            }
          
          *p = 0;
          p++;
          pg->resident = strtol (b, NULL, 0);

          b = p;
          p = strchr (b, ' ');

          if (!p)
            {
              g_warning ("Expected writable field missing");
              continue;
            }
          
          pg->writable = strtol (b, NULL, 0);

          p++;

          pg->id = strtoul (p, NULL,16);
	    
          if (pg->id || pg->resident)
            pg->mapped = TRUE;

          if (pg->id)
	    {
              gpointer ptr = g_hash_table_lookup (mem_data->pg_count,
                                                  GINT_TO_POINTER (pg->id));

              gint count = GPOINTER_TO_INT (ptr) + 1;
              g_hash_table_replace (mem_data->pg_count,
                                    GINT_TO_POINTER (pg->id),
                                    GINT_TO_POINTER (count));
	    }

          address_t vma_start = page_start (cur_vma->start);
          address_t vma_last = page_start (cur_vma->end - 1);
	    
          if (!cur_page && vma_start != cur_vma->start)
	    {
              address_t vma_p1 = vma_start + getpagesize ();
              pg->size = vma_p1 - cur_vma->start;
              pg->start = cur_vma->start;
              
#if 0
              g_warning ("VMA %x-%x (page start %x), page %d, size %d",
                         cur_vma->start, cur_vma->end, vma_start,
                         cur_page, pg->size);
#endif
	    }
          else if (cur_page * getpagesize() ==  vma_last)
	    {
              pg->size = cur_vma->end - cur_page * getpagesize();
              pg->start = vma_start + cur_page * getpagesize ();
#if 0
              g_warning ("VMA %x-%x, page %d, size %d",
                         cur_vma->start, cur_vma->end, cur_page, pg->size);
#endif
	    }
          else
            {
              pg->size = getpagesize ();
              pg->start = vma_start + cur_page * getpagesize ();
            }
          
          
          cur_page++;
	}
    }

 finish:
  if (error && mem_data)
    {
      if (mem_data->pg_count)
        g_hash_table_destroy (mem_data->pg_count);

      if (mem_data->files)
        g_hash_table_destroy (mem_data->files);
      
      g_free (mem_data);
      mem_data = NULL;
    }
    
  if (fexmp)
    fclose (fexmp);

  return mem_data;
}

static void
hash_to_list (gpointer key, gpointer value, gpointer data)
{
  *((GList**)data) = g_list_prepend (*((GList**)data), value);
}

static gint
sort_pid_list (gconstpointer a, gconstpointer b, gpointer data)
{
  gint sort = GPOINTER_TO_INT (data);
  const memory_t * m1 = (const memory_t *) a;
  const memory_t * m2 = (const memory_t *) b;
  gint r = 0;

  switch (SORT_BY_GET (sort))
    {
      case SORT_BY_VM:     r = m2->vm - m1->vm;         break;
      case SORT_BY_SOLE:   r = m2->sole - m1->sole;     break;
      case SORT_BY_HEAP:   r = m2->heap - m1->heap;     break;
      case SORT_BY_STACK:  r = m2->stack - m1->stack;   break;
      case SORT_BY_MAPPED: r = m2->mapped - m1->mapped; break;
      case SORT_BY_EMAPPED: r = m2->emapped - m1->emapped; break;

      default:
      case SORT_BY_NONE: r = 0;                   break;
    }

  if (SORT_TYPE_GET (sort) == SORT_TYPE_ASC)
    r *= -1;

  return r;
}

/* Prints contents of a memory struct to stdout.
 * file_info indicates if per-file information is to be printed as well
 */
static gint
print_results (memory_t * mem_data, data_t * d)
{
  guint lines = 0;

  /* do not print if the vm is below the threshold, and or 0 */
  guint threshold = d->args.threshold ? d->args.threshold : 1;
  
  if (threshold && mem_data->vm < threshold)
    return lines;

  if (d->args.format == FORMAT_HUMAN || d->args.interactive)
    {
      if (!myprintf (d,"Process %d", mem_data->pid))
        return -1;
      
      d->global_lines--;
      
      if (mem_data->cmd)
        {
          if (!myprintf (d," [%s]\n", mem_data->cmd))
            return -1;
        }
      else
        {
          if (!myprintf (d,"\n"))
            return -1;
        }
      
      if (!myprintf (d," Virtual memory  : %9u %s\n",
                     SCALE_V(mem_data->vm), SCALE_U(mem_data->vm)))
        return -1;
      
      if (!myprintf (d," Effective VM    : %9u %s\n",
                     SCALE_V(mem_data->evm), SCALE_U(mem_data->evm)))
        return -1;
      
      if (!myprintf (d," Heap            : %9u %s\n",
                     SCALE_V(mem_data->heap), SCALE_U(mem_data->heap)))
        return -1;
      
      if (!myprintf (d," Stack           : %9u %s\n",
                     SCALE_V(mem_data->stack), SCALE_U(mem_data->stack)))
        return -1;
      
      if (!myprintf (d," vdso            : %9u %s\n",
                     SCALE_V(mem_data->vdso), SCALE_U(mem_data->vdso)))
        return -1;
      
      if (!myprintf (d," Mapped          : %9u %s\n",
                     SCALE_V(mem_data->mapped), SCALE_U(mem_data->mapped)))
          return -1;
          
      if (!myprintf (d," Effective mapped: %9u %s\n",
                     SCALE_V(mem_data->emapped), SCALE_U(mem_data->emapped)))
        return -1;
      
      if (!myprintf (d," Sole use        : %9u %s\n",
                     SCALE_V(mem_data->sole), SCALE_U(mem_data->sole)))
        return -1;

      lines += 9;
      
      if (d->args.detail & DETAIL_FILE_INFO)
        {
          lines += 2;
          d->global_lines++;
          if (!myprintf (d,"\n Per file memory use\n"))
            return -1;
        }
    }
  else if (d->args.format == FORMAT_CSV || d->args.format == FORMAT_CSV_SUMS)
    {
      lines++;
      
      if (!myprintf (d,"%u,\"%s\",%u,%u,%u,%u,%u,%u,%u,%u\n",
                     mem_data->pid,
                     mem_data->cmd ? mem_data->cmd : "???",
                     mem_data->vm,
                     mem_data->evm,
                     mem_data->heap,
                     mem_data->stack,
                     mem_data->vdso,
                     mem_data->mapped,
                     mem_data->emapped,
                     mem_data->sole))
        return -1;
    }

  if (d->args.detail & DETAIL_FILE_INFO)
    {
      GList * l = NULL;
      g_hash_table_foreach (mem_data->files, hash_to_list, &l);

      if (l)
        {
          GList * list = g_list_sort_with_data (l, sort_pid_list,
                                              GINT_TO_POINTER (d->args.sort));
          l = list;
          
          while (l)
            {
              int i = print_file_info ((file_t*)l->data, d);

              if (i < 0)
                {
                  g_list_free (list);
                  return -1;
                }
              
              lines += i;
              l = l->next;
            }

          g_list_free (list);
        }
    }

  if (d->args.format == FORMAT_HUMAN || d->args.interactive)
    {
      if (!d->args.interactive)
        {
          if (!myprintf (d,"\n========================================"
                         "=======================================\n\n"))
            return -1;
          
          d->global_lines += 2;
        }
      else
        if (!myprintf (d, "\n"))
          return -1;
    }

  return lines;
}

static gint
find_pid (gconstpointer a, gconstpointer b)
{
  const memory_t * A   = (const memory_t *)a;
  pid_t            pid = GPOINTER_TO_INT(b);

  if (A->pid == pid)
    return 0;

  return 1;
}

static void
usage (const char * option)
{
  FILE * f = stdout;
  
  if (option)
    {
      f = stderr;
      fprintf (f, "\nUnknown option %s.\n", option);
    }
  else
    fprintf (f, "\nThis is the command-line front-end to Exmap.\n");
  
  fprintf (f, "\nUsage: exmap [options] [pid|name ...]\n\n");
  fprintf (f, "Options:\n");
  fprintf (f, "  --no-file-info        : Only summary for each process (implies --no-elf-info).\n");
  fprintf (f, "  --no-elf-info         : No breakdown per elf sections in each file.\n");
  fprintf (f, "  --all-pids            : Information for all accessible pids; if not used,\n"
              "                          at least one process must be specified.\n");
  fprintf (f, "  --vm-threshold=size   : Information only for processes above VM threshold\n");
  fprintf (f, "  --sort-by=what        : vm|sole|heap|stack|mapped|none (default: none)\n");
  fprintf (f, "  --sort-type=which     : desc|asc (default: desc)\n");
  fprintf (f, "  --format=which        : human|csv|csv+sums (default: human)\n");
  fprintf (f, "  --log=file            : Print results to file, not stdout\n");
  fprintf (f, "  --interactive         : Run in interactive mode\n");
  fprintf (f, "  --remote              : Run in interactive mode for remote connection\n");
  fprintf (f, "  --root                : Root of the file system to use (remote session only)\n");
  fprintf (f, "  --version             : Program version\n");
  fprintf (f, "  --module-version      : Version of required kernel module\n");
  fprintf (f, "  --help\n\n");
  fprintf (f, "If --all-pids is not given, at least one process has to be specified,\n");
  fprintf (f, "either by a pid or command name. Command names must not include path. \n");
  fprintf (f, "Specifying a process by name is equivalent to giving pids for all running \n");
  fprintf (f, "processes of that name.\n\n");

  fprintf (f, "If neither --no-file-info nor --no-elf-info is given then for any \n");
  fprintf (f, "processes specified explicitely symbol information is printed where available.\n\n");
}

gint
get_unit (const char * p)
{
  gint multiplier = 1;
  
  while (*p && (isdigit (*p)|| isspace(*p)))
    ++p;

  switch (*p)
    {
    case 'g':
    case 'G':
      multiplier *= 1024;
      /* fall through */
    case 'm':
    case 'M':
      multiplier *= 1024;
      /* fall through */
    case 'k':
    case 'K':
      multiplier *= 1024;
      break;

    default:
      ;
    }

  return multiplier;
}


static gint
parse_args (int argc, char ** argv, args_t * args)
{
  gint i, j;
  
  if (argc < 2)
    {
      usage (NULL);
      return EXIT_INVALID_ARGS;
    }

  for (i = 1; i < argc; ++i)
    {
      if (*argv[i] != '-')
        break;

      if (!strcmp (argv[i], "--no-file-info"))
        {
          DETAIL_UNSET (args->detail, DETAIL_FILE_INFO);
          DETAIL_UNSET (args->detail, DETAIL_ELF_INFO);
          continue;
        }

      if (!strcmp (argv[i], "--no-elf-info"))
        {
          DETAIL_UNSET (args->detail, DETAIL_ELF_INFO);
          continue;
        }
      
      if (!strcmp (argv[i], "--all-pids"))
        {
          args->all_pids = TRUE;
          continue;
        }

      if (!strncmp (argv[i], "--sort-by",9))
        {
          if (!strcmp (argv[i] + 10, "vm"))
            SORT_BY_SET (args->sort, SORT_BY_VM);
          else if (!strcmp (argv[i] + 10, "sole"))
            SORT_BY_SET (args->sort, SORT_BY_SOLE);
          else if (!strcmp (argv[i] + 10, "heap"))
            SORT_BY_SET (args->sort, SORT_BY_HEAP);
          else if (!strcmp (argv[i] + 10, "stack"))
            SORT_BY_SET (args->sort, SORT_BY_STACK);
          else if (!strcmp (argv[i] + 10, "mapped"))
            SORT_BY_SET (args->sort, SORT_BY_MAPPED);
          else if (!strcmp (argv[i] + 10, "emapped"))
            SORT_BY_SET (args->sort, SORT_BY_EMAPPED);
          else
            SORT_BY_SET (args->sort, SORT_BY_NONE);

          continue;
        }

      
      if (!strncmp (argv[i], "--sort-type",11))
        {
          if (!strcmp (argv[i] + 12, "asc"))
            SORT_TYPE_SET (args->sort, SORT_TYPE_ASC);
          else
            SORT_TYPE_SET (args->sort, SORT_TYPE_DESC);

          continue;
        }

      if (!strncmp (argv[i], "--vm-threshold",14))
        {
          gint unit = get_unit (argv[i] + 15);
          args->threshold = unit * atoi (argv[i] + 15);
          continue;
        }

      if (!strncmp (argv[i], "--format",8))
        {
          if(!strcmp (argv[i] + 9, "csv"))
            args->format = FORMAT_CSV;
          else if(!strcmp (argv[i] + 9, "csv+sums"))
            args->format = FORMAT_CSV_SUMS;
          else
            goto args_error;
          continue;
        }

      if (!strncmp (argv[i], "--log",5))
        {
          args->log = argv[i] + 6;
          continue;
        }

      if (!strcmp (argv[i], "--interactive"))
        {
#ifdef INTERACTIVE
          args->interactive = TRUE;
          continue;
#else
          g_print ("--interactive not supported "
                   "(compiled with --without-readline)\n");
          return EXIT_INVALID_ARGS;
#endif
        }

      if (!strcmp (argv[i], "--remote"))
        {
#ifdef INTERACTIVE
          args->interactive = TRUE;
          args->remote = TRUE;
          continue;
#else
          g_print ("--remote not supported "
                   "(compiled with --without-readline)\n");
          return EXIT_INVALID_ARGS;
#endif
        }

      if (!strncmp (argv[i], "--root", 6))
        {
#ifdef INTERACTIVE
          args->fs_root = g_strdup (argv[i] + 7);
          continue;
#else
          g_print ("--root not supported "
                   "(compiled with --without-readline)\n");
          return EXIT_INVALID_ARGS;
#endif
        }
      
      if (!strcmp (argv[i], "--help"))
        {
          usage (NULL);
          exit (0);
        }

      if (!strcmp (argv[i], "--version"))
        {
          g_print (VERSION "\n");
          exit (0);
        }

      if (!strcmp (argv[i], "--module-version"))
        {
          g_print (EXMAP_VERSION "\n");
          exit (0);
        }
      
    args_error:
      /* unknown option */
      usage (argv[i]);
      return EXIT_INVALID_ARGS;
    }

  if (args->interactive &&
      (args->log))
    {
      g_print ("Cannot use --log with --interactive.\n");
      return EXIT_INVALID_ARGS;
    }

  args->pids_arg   = g_malloc0((argc - i + 1) * sizeof(pid_t));
  args->pids_names = g_malloc0((argc - i + 1) * sizeof(const char*));
  args->pids_count = 0;
  j = 0;

  for (; i < argc; ++i)
    {
      args->pids_arg[args->pids_count] = atoi (argv[i]);

      if(args->pids_arg[args->pids_count])
        args->pids_count++;
      else
        {
          /* assume this is a process name */
          args->pids_names[j] = argv[i];
          j++;
        }
    }

  if (!j && !args->all_pids && !args->pids_count && !args->interactive)
    {
      g_warning ("Either use --all-pids, or specify at least one pid");
      return EXIT_INVALID_ARGS;
    }
  

  return 0;
}

static void
data_free (data_t * d)
{
  main_reset (d);

  if (d->args.pids_arg)
    g_free (d->args.pids_arg);

  if (d->args.pids_names)
    g_free (d->args.pids_names);
  
  fclose (d->of);
  
}

static gboolean
is_pid_present (pid_t * pids, pid_t pid)
{
  while (pids && *pids)
    {
      if (*pids == pid)
        return TRUE;

      pids++;
    }

  return FALSE;
}

typedef struct
{
  int    sckt;
  args_t args;
} connection_t;

void
main_load (data_t * d)
{
  GList      * l;

  if (d->args.interactive)
    {
      g_print ("  Creating system snapshot ...");
    }
  
  if (!(d->pids_sys = load_pids (d)))
    {
      g_warning ("There was a problem loading system data");
      exit (EXIT_SYSDATA_ERROR);
    }

  d->pg_count = g_hash_table_new (NULL, NULL);
  
  /* load data for each accessible pid */
  l = d->pids_sys;
  while (l)
    {
      memory_t * m;
      pid_t      p = GPOINTER_TO_INT (l->data);
      
      GHashTable * h = load_map (d, p);

      if (!h)
        continue;

      if ((m = load_pid_data (d, p, h, d->pg_count)))
        {
          m->load_symbols = is_pid_present (d->args.pids_arg, p);
          d->mem_list = g_list_prepend (d->mem_list, m);
        }
      
      l = l->next;
    }

  /* work out how much memory is shared between processes
   * and how much between files.
   */
  
  l = d->mem_list;
  while (l)
    {
      memory_t * m = (memory_t *) l->data;

      g_hash_table_foreach (m->map, process_vma, m);
      g_hash_table_foreach (m->files, process_files, m);

      l = l->next;
    }

#ifdef INTERACTIVE
  if (d->args.interactive)
    {
      g_print (" %d processes running.\n", g_list_length (d->mem_list));
      g_print ("  Enter a command or '?' for help.\n");
    }
#endif
}

void
main_sort (data_t * d)
{
  /* sort results as appropriate */
  if (SORT_BY_GET (d->args.sort) != SORT_BY_NONE)
    {
      d->mem_list = g_list_sort_with_data (d->mem_list, sort_pid_list,
                                           GINT_TO_POINTER (d->args.sort));
    }
}

void
main_print (data_t * d)
{
  guint        lines = 0;
  GList      * l;
  pid_t      * pid;

  d->global_lines = 0;
  
  if (!d->args.interactive)
    {
      time_t       now;
      struct tm    t;
      char         date_stamp[30];
  
      /* Print a time stamp into the log */
      now = time (NULL);
      localtime_r (&now, &t);
      strftime (date_stamp, sizeof (date_stamp),
                "%a %Y %m %d %H:%M:%S", &t);
                      
      fprintf (d->of,"--- Exmap log from %s ---\n\n", date_stamp);
    }
  
  /* output header */
  if (d->args.format == FORMAT_CSV || d->args.format == FORMAT_CSV_SUMS)
    {
      if (!myprintf (d,
                     "pid,command,VM,effective VM,heap,stack,vdso,mapped,"
                     "emapped,sole"))
        return;
      
      d->global_lines--;
      
      if (d->args.detail & DETAIL_FILE_INFO)
        {
          if (!myprintf (d,",file,file-VM,file-mapped,file-solefile"))
            return;

          if (d->args.detail & DETAIL_ELF_INFO)
            {
              if (!myprintf (d,
                          ",section,section-VM,section-mapped,section-sole\n"))
                return;
            }
          else
            {
              if (!myprintf (d,"\n"))
                return;
            }
          
          d->global_lines--;
        }
      else
        if (!myprintf (d,"\n"))
          return;
    }

  /* output info for individula pids */
  if (d->args.all_pids || !d->args.pids_arg || !*d->args.pids_arg)
    {
      l = d->mem_list;
      while (l)
        {
          int i = print_results ((memory_t*)l->data, d);

          if (i < 0)
            return;
          
          lines += i;
          l = l->next;
        }
    }
  else
    {
      pid = d->args.pids_arg;
      if (pid && *pid)
        {
          while (*pid)
            {
              l = g_list_find_custom (d->mem_list,
                                      GINT_TO_POINTER (*pid), find_pid);

              if (l)
                {
                  int i = print_results ((memory_t*)l->data, d);

                  if (i < 0)
                    return;
                  
                  lines += i;
                }
              
              pid++;
            }
        }
      else
        {
          fprintf (d->of, "  No process selected.\n");
        }
    }

  if (!d->args.interactive)
    {
      if (d->args.format == FORMAT_CSV_SUMS)
        {
          /* output sum line -- keep in sync with the header line */
          int i;
          char c = 'A' + 2;

          fprintf (d->of,"Totals,\"(F9 to recalculate)\"");
          
          for (i = 0; i < CSV_COLUMNS - 2; ++i, ++c)
            fprintf (d->of,",\"=sum(%c2:%c%d)\"", c, c, lines+1);

          fprintf (d->of,"\n");

          lines++;

          if (lines > 2 << 16)
            fprintf (stderr,
                     "\n"
                     "   WARNING: Generated CSV file contain %u lines -- spreadsheet\n"
                     "            application may not be able to load this file in its\n"
                     "            entirety.\n\n",
                     lines);
        }

      /* put a mark in, so we know that the process finished normally */
      fprintf (d->of,"--- Exmap log ends here ---\n");
    }
}

void
main_reset (data_t * d)
{
  GList * l;
  
  if (d->pg_count)
    {
      g_hash_table_destroy (d->pg_count);
      d->pg_count = NULL;
    }
  
  if (d->mem_list)
    {
      l = d->mem_list;
      while (l)
        {
          memory_t * m = (memory_t *) l->data;
          memory_free (m);
          l = l->next;
        }

      g_list_free (d->mem_list);
      d->mem_list = NULL;
    }

  if (d->pids_sys)
    {
      g_list_free (d->pids_sys);
      d->pids_sys = NULL;
    }
}

static void
signal_handler (int sig)
{
#ifdef INTERACTIVE
  if (sig == SIGPIPE)
    {
      g_print ("Connection to remote server lost.\n");
      main_reset (&data);

      if (data.srv_sckt >= 0)
        {
          close (data.srv_sckt);
          data.srv_sckt = -1;
        }
    }
#endif
  
  /* If we have an open socket to a remote server, close it */
  if (sig == SIGTERM || sig == SIGSEGV || sig == SIGABRT)
    {
#ifdef INTERACTIVE
      if (data.srv_sckt >= 0)
        close (data.srv_sckt);

      interactive_save_history (&data);
#endif
      g_message ("caught signal %d, exiting", sig);
      exit (sig);
    }
}

int
main (int argc, char ** argv)
{
  gint         retval = 0;

  memset (&data, 0, sizeof (data));

  data.of          = stdout;
  data.args.detail = (DETAIL_FILE_INFO | DETAIL_ELF_INFO);
  data.args.format = FORMAT_HUMAN;
  data.srv_sckt    = -1;
  
  if ((retval = parse_args (argc, argv, &data.args)))
    {
      goto finish;
    }

  if (data.args.log && (data.of = fopen (data.args.log, "w")) == NULL)
    {
      g_warning ("Could not open putput file [%s]", data.args.log);
      retval = EXIT_LOGFILE_ERROR;
      goto finish;
    }

  /* handle signals */
  struct sigaction sa;
  sigfillset(&sa.sa_mask);
  sa.sa_handler = signal_handler;
#ifdef INTERACTIVE
  sigaction(SIGPIPE, &sa, NULL);
#endif
  sigaction(SIGTERM, &sa, NULL);
  //sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGABRT, &sa, NULL);
  
#ifdef INTERACTIVE
  if (data.args.interactive)
    {
      interactive_init (&data);
    }
#endif

  if (!data.args.remote)
    {
      main_load (&data);
      main_sort (&data);
    }
  
#ifdef INTERACTIVE
  if (data.args.interactive)
    {
      interactive_loop (&data);
    }
  else
#endif
    main_print (&data);
  
 finish:
  data_free (&data);

  if (data.srv_sckt >= 0)
    close (data.srv_sckt);
  
  return retval;
}
