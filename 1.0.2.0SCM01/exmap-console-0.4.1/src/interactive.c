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
 * Readline interface code for interactive mode
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "exmap.h"

int rows = 24;
int columns = 80;

enum
  {
    EMSG_UNKNOWN_COMMAND = 0,
    EMSG_INVALID_PARAMETER,
    EMSG_PID_NOT_SELECTED,
    EMSG_NO_PROCESS_SELECTED,
    EMSG_FOPEN_ERROR,
    EMSG_UNKNOWN_ERROR,

    /* MUST BE LAST, HAVE NO CORRESPONDING STRING */
    EMSG_ERROR_NO_MESSAGE, /* for errors for which msg was already printed */
    EMSG_SUCCESS
  };

static char * emsg [] =
  {
    "  Unknown command '%s'\n",
    "  Invalid parameter '%s'\n",
    "  Process %d is not selected\n",
    "  No process selected\n",
    "  Could not open file [%s]\n",
    "  Unknown error occured\n"
  };

static gchar *
get_hist_file ()
{
  struct passwd *pws = getpwuid(getuid());
  if (pws && pws->pw_name)
    {
      gchar * s = g_strconcat ("/home/", pws->pw_name,
                               "/.exmap-console-history", NULL);

      return s;
    }

  return NULL;
}

void
interactive_init (data_t * d)
{
  rl_initialize();
  rl_get_screen_size (&rows, &columns);

  using_history ();

  gchar * s = get_hist_file ();
  read_history (s);
  g_free (s);

  DETAIL_UNSET (d->args.detail, DETAIL_FILE_INFO);
  DETAIL_UNSET (d->args.detail, DETAIL_ELF_INFO);

  g_print ("\nWelcome to exmap-console " VERSION "\n");
}

static GList *
cmd_to_pid (data_t * d, const char * cmd)
{
  GList * result = NULL;
  GList * l = d->mem_list;
  while (l)
    {
      memory_t * m = l->data;
      if (m->cmd && *m->cmd)
        {
          gchar * c = m->cmd;
          gchar * name = m->cmd;

          while ((c = strchr (c, '/')))
            {
              c++;
              name = c;
            }

          if (!strncmp (name, cmd, strlen (cmd)))
            {
              result = g_list_prepend (result, GINT_TO_POINTER (m->pid));
            }
        }

      l = l->next;
    }

  return result;
}

/* converts a GList of pid_t entries to an array; if the old_pids is not NULL
 * the GList entries get appended to the end of the old arrary
 * count is the number of entries in the array when called / on return
 */
static pid_t *
pid_list_to_array (GList * l, pid_t * old_pids, int * count)
{
  int i;
  guint len = g_list_length (l);

  pid_t * pids;

  if (old_pids)
    pids = g_realloc (old_pids, (*count + len + 1) * sizeof(pid_t));
  else
    {
      pids = g_malloc ((len + 1) * sizeof (pid_t));
      *count = 0;
    }

  for (i = 0; i < len; ++i)
    {
      pid_t pid = GPOINTER_TO_INT (l->data);
      pids[*count + i] = pid;
      l = l->next;
    }

  *count += len;
  pids[*count] = 0;

  g_list_free (l);

  return pids;
}

static guint
interactive_print (data_t * d, const char * p)
{
  if (p && *p)
    {
      GList  * l;
      pid_t    pid;

      if (!strcmp (p, "all"))
        {
          d->args.all_pids = TRUE;
          main_print (d);
          d->args.all_pids = FALSE;
          return EMSG_SUCCESS;
        }
      else
        {
          pid_t  * old_pids  = d->args.pids_arg;
          guint    old_count = d->args.pids_count;

          if ((pid = atoi (p)))
            {
              d->args.pids_arg = g_malloc0(2 * sizeof(pid_t));
              d->args.pids_arg[0] = pid;
              d->args.pids_arg[1] = 0;
              d->args.pids_count = 1;
            }
          else if ((l = cmd_to_pid (d, p)))
            {
              d->args.pids_count = 0;
              d->args.pids_arg = pid_list_to_array (l, NULL,
                                                    &d->args.pids_count);
            }
          else
            {
              return EMSG_INVALID_PARAMETER;
            }

          main_print (d);

          g_free (d->args.pids_arg);
          d->args.pids_arg = old_pids;
          d->args.pids_count = old_count;

          return EMSG_SUCCESS;
        }
    }
  else
    main_print (d);

  return EMSG_SUCCESS;
}

static guint
interactive_add (data_t *d, const char * p)
{
  GList * l;
  pid_t   pid;

  if (!strcmp (p, "all"))
    {
      d->args.all_pids = TRUE;
    }
  else if (*p && (pid = atoi (p)))
    {
      d->args.pids_arg = g_realloc(d->args.pids_arg,
                                   (d->args.pids_count + 2)*sizeof(pid_t));

      d->args.pids_arg[d->args.pids_count] = pid;
      d->args.pids_count++;
      d->args.pids_arg[d->args.pids_count] = 0;
    }
  else if (*p && (l = cmd_to_pid (d, p)))
    {
      d->args.pids_arg = pid_list_to_array (l, d->args.pids_arg,
                                            &d->args.pids_count);
    }
  else
    return EMSG_INVALID_PARAMETER;

  return EMSG_SUCCESS;
}

static guint
interactive_rm (data_t * d, const char * p)
{
  pid_t pid;

  if (!strcmp (p, "all"))
    d->args.all_pids = FALSE;
  else if (*p && (pid = atoi (p)))
    {
      pid_t * pp = d->args.pids_arg;
      gboolean found = FALSE;

      while (*pp)
        {
          if (*pp == pid)
            {
              found = TRUE;
              break;
            }

          pp++;
        }

      if (found)
        {
          while (*pp)
            {
              *pp = *(pp + 1);
              pp++;
            }

          d->args.pids_count--;
        }
      else
        {
          g_print (emsg[EMSG_PID_NOT_SELECTED], pid);
          return EMSG_ERROR_NO_MESSAGE;
        }
    }
  else
    return EMSG_INVALID_PARAMETER;

  return EMSG_SUCCESS;
}

static guint
interactive_ls (data_t *d)
{
  if (d->args.all_pids)
    g_print ("'all'\n");
  else
    {
      pid_t * p = d->args.pids_arg;
      if (p && *p)
        {
          while (*p)
            {
              g_print ("%d ", *p);
              p++;
            }

          g_print ("\n");
        }
      else
        return EMSG_NO_PROCESS_SELECTED;
    }

  return EMSG_SUCCESS;
}

static guint
interactive_save (data_t * d, const char * file)
{
  FILE  * old_of = d->of;
  FILE  * of = fopen (file, "w");

  if (!of)
    {
      g_print (emsg[EMSG_FOPEN_ERROR], file);
      return EMSG_ERROR_NO_MESSAGE;
    }

  /* step out of interactive mode ... */
  d->of = of;
  d->args.interactive = FALSE;

  main_print (d);

  d->of = old_of;
  d->args.interactive = TRUE;

  fclose (of);

  return EMSG_SUCCESS;
}

/* setup a connection to remote server */
static guint
interactive_connect (data_t * d, const char * server)
{
  if (!d || !server)
    return EMSG_ERROR_NO_MESSAGE;
  
  if (d->srv_sckt >= 0)
    {
      close (d->srv_sckt);
      d->srv_sckt = -1;
    }
  
  gchar * srvr = g_strdup (server);
  gchar * p = strchr (srvr, ':');

  if (!p)
    {
      g_print ("String [%s] is not valid server specification\n",
               server);
      
      return EMSG_ERROR_NO_MESSAGE;      
    }

  *p++ = 0;
  guint port = atoi (p);

  if (!port)
    {
      g_print ("String [%s] is not valid port\n",
               p);
      
      return EMSG_ERROR_NO_MESSAGE;      
    }
  
  
  if ((d->srv_sckt = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      g_print ("Unable to open socket\n");
      return EMSG_ERROR_NO_MESSAGE;      
    }

  union
  {
    struct sockaddr s;
    struct sockaddr_in i;
  } sck;
    
  sck.i.sin_family = AF_INET;
  sck.i.sin_port   = htons(port);

  struct in_addr   addr;
  struct hostent * he = gethostbyname(srvr);

  memcpy(&addr, *(he->h_addr_list),sizeof(struct in_addr));
  memcpy(&(sck.i.sin_addr),*(he->h_addr_list),sizeof(struct in_addr));

  if (connect(d->srv_sckt, &sck.s, sizeof (struct sockaddr)) < 0)
    {
      g_print ("Unable to open connection to server %s (%d)\n",
               server, errno);

      close (d->srv_sckt);
      d->srv_sckt = -1;
      
      return EMSG_ERROR_NO_MESSAGE;      
    }

  g_free (srvr);

  int ioarg = 0;
  ioctl (d->srv_sckt, FIONBIO, &ioarg);

  /* Handshake */
  char buf[100];
  g_snprintf (buf, sizeof (buf), "EXMP_HANDSHAKE %s\n",
              EXMAP_PROTOCOL_VERSION);
  
  write (d->srv_sckt, buf, strlen (buf));

  /* Check result */
  FILE * f = fdopen (dup (d->srv_sckt), "r");
  gboolean success = FALSE;
  
  if (f)
    {
      while (fgets (buf, sizeof (buf), f))
        {
          if (!strncmp (buf, "EXMP_DONE", 9))
            {
              success = TRUE;
              break;
            }

          if (!strncmp (buf, "EXMP_ERROR", 10))
            {
              g_print ("Server error: %s\n", buf);
              break;
            }
        }
      
      fclose (f);      
    }
  else
    {
      g_print ("Unknown error opening socket to remote server\n");
    }
  
  if (!success)
    return EMSG_ERROR_NO_MESSAGE;      
  
  main_reset (d);
  main_load (d);

  return EMSG_SUCCESS;
}

static guint
interactive_root (data_t * d, const char * r)
{
  if (d->args.fs_root)
    g_free (d->args.fs_root);

  if (r && *r)
    d->args.fs_root = g_strdup (r);
  else
    d->args.fs_root = NULL;

  main_reset (d);

  if (d->srv_sckt >= 0)
    main_load (d);

  return EMSG_SUCCESS;
}

static guint
interactive_help ()
{
  g_print (
      "Available commands:\n"
      "  add 'all'|pid|command       : select process for examination\n"
      "  clear                       : deselect all selected processes\n"
      "  connect host:port           : connect to remote exmap server\n"
      "  debug                       : some debug info\n"
      "  detail 'none'|'file'|'elf'  : set level of detail to show\n"
      "  disconnect                  : disconnect from remote server\n"
      "  format 'human'|'csv'        : format for saving data\n"
      "  ls                          : list selected processes\n"
      "  print ['all'|pid|command]   : print data for selected/specified processes\n"
      "  ps [options]                : run system ps command\n"
      "  reload                      : recreate system snapshot\n"
      "  rm pid|'all'                : deselect process with pid, or clear 'all' flag\n"
      "  root fsroot                 : change file system root for remote session\n"
      "  save file                   : save data for selected process to file\n"
      "  shell commands              : run shell commands\n"
      "  sort asc|desc|vm|sole|heap|stack|mapped|emapped\n"
      "                              : sort results by\n"
      "  threshold size [k|M|G]      : set minimum VM for processes printed\n"
      "  ?, help                     : help\n"
      "  quit                        : quit\n");

  return EMSG_SUCCESS;
}

static guint
interactive_format (data_t * d, const char *p)
{
  if (!p)
    return EMSG_INVALID_PARAMETER;

  if (!strncmp (p, "human", 5))
    d->args.format = FORMAT_HUMAN;
  else if (!strncmp (p, "csv", 3))
    d->args.format = FORMAT_CSV;
  else if (!strncmp (p, "csv+sums", 8))
    d->args.format = FORMAT_CSV_SUMS;
  else
    return EMSG_INVALID_PARAMETER;

  return EMSG_SUCCESS;
}

static guint
interactive_debug (data_t * d)
{
  g_print ("Socket %d\n", d->srv_sckt);
  return EMSG_SUCCESS;
}

static guint
interactive_threshold (data_t * d, const char *p)
{
  int unit = get_unit (p);
  
  if (*p)
    d->args.threshold = unit * atoi (p);
  else
    return EMSG_INVALID_PARAMETER;

  return EMSG_SUCCESS;
}

static guint
interactive_spawn (data_t *d, const char * cmd, const char * params)
{
  GError * error = NULL;
  gchar  * out = NULL;
  gchar  * err = NULL;
  gint     status;
  guint    retval = EMSG_SUCCESS;
  gchar  * cmdline = NULL;

  cmdline = g_strconcat (cmd, " ", params, NULL);

  d->global_lines = 0;

  if (d->srv_sckt < 0)
    {
      if (g_spawn_command_line_sync (cmdline, &out, &err, &status, &error))
        {
          if (out)
            {
              print_paged (d, out);
              g_free (out);
            }

          if (WEXITSTATUS (status))
            {
              if (err)
                {
                  print_paged (d, err);
                  g_free (err);
                }

              retval = EMSG_UNKNOWN_ERROR;
            }
        }
      else
        {
          print_paged (d, error->message);
          g_error_free (error);
          retval = EMSG_UNKNOWN_ERROR;
        }
    }
  else
    {
      char buf[256];
      g_snprintf (buf, sizeof (buf), "EXMP_CMD SPAWN %s\n", cmdline);
  
      write (d->srv_sckt, buf, strlen (buf));

      FILE * f = fdopen (dup (d->srv_sckt), "r");
  
      if (f)
        {
          while (fgets (buf, sizeof (buf), f))
            {
              if (!strncmp (buf, "EXMP_DONE", 9))
                {
                  break;
                }

              if (!strncmp (buf, "EXMP_ERROR", 10))
                {
                  g_print ("Server error: %s\n", buf);
                  break;
                }
              
              print_paged (d, buf);
            }
      
          fclose (f);      
        }
      else
        {
          g_print ("Unknown error opening socket to remote server\n");
        }
    }
  
  g_free (cmdline);

  return EMSG_SUCCESS;
}

gboolean
interactive_parse_line (data_t *d, char * line)
{
  char       c;
  char     * p;
  gboolean   history = TRUE;
  guint      retval = EMSG_SUCCESS;
  gchar    * cmd;
  gchar    * param;
  size_t     cmd_len;

  if (!line)
    return FALSE;

  cmd = g_strdup (line);

  while (isspace (*cmd))
    cmd++;

  c = *cmd;
  p = cmd;

  while (*p && !isspace (*p))
    p++;

  while (*p && isspace (*p))
    {
      *p = 0;
      p++;
    }

  param = p;

  while (*p && !isspace (*p))
    p++;

  if (*p)
    *p = 0;
  
  cmd_len = strlen (cmd);

  switch (c)
    {
    case 'a':
      if (!strncmp (cmd, "add", cmd_len))
        retval = interactive_add (d, param);
      else
        retval = EMSG_UNKNOWN_COMMAND;
      break;

    case 'c':
      if (!strncmp (cmd, "clear", cmd_len))
        {
          d->args.all_pids = FALSE;
          g_free (d->args.pids_arg);
          d->args.pids_arg = NULL;
          d->args.pids_count = 0;
        }
      else if (!strncmp (cmd, "connect", cmd_len))
        {
          retval = interactive_connect (d, param);
        }
      else
        retval = EMSG_UNKNOWN_COMMAND;
      break;

    case 'd':
      if (!strncmp (cmd, "detail", cmd_len))
        {
          if (!strcmp (param, "none"))
            d->args.detail = 0;
          else if (!strcmp (param, "file"))
            {
              d->args.detail = 0;
              DETAIL_SET (d->args.detail, DETAIL_FILE_INFO);
            }
          else if (!strcmp (param, "elf"))
            {
              DETAIL_SET (d->args.detail, DETAIL_FILE_INFO);

              if (!d->args.fs_root && d->srv_sckt >= 0)
                g_print ("  You must specify root for local filesystem to "
                         "access elf internals\n");
              else
                DETAIL_SET (d->args.detail, DETAIL_ELF_INFO);
            }
          else
            {
              g_print (emsg[EMSG_INVALID_PARAMETER], param);
              retval = EMSG_ERROR_NO_MESSAGE;
            }
        }
      else if (!strncmp (cmd, "debug", cmd_len))
        {
          retval = interactive_debug (d);
        }
      else if (!strncmp (cmd, "disconnect", cmd_len))
        {
          if (d->srv_sckt >= 0)
            {
              close (d->srv_sckt);
              d->srv_sckt = -1;
              main_reset (d);
              g_print ("Disconnected from remote server\n");
            }
          else
            {
              g_print ("Not connected\n");
            }
          
          retval = EMSG_SUCCESS;
        }
      else
        retval = EMSG_UNKNOWN_COMMAND;
      break;

    case 'f':
      if (!strncmp (cmd, "format", cmd_len))
        retval = interactive_format (d, param);
      else
        retval = EMSG_UNKNOWN_COMMAND;
      break;
      
    case 'h':
      if (!strncmp (cmd, "help", cmd_len))
        retval = interactive_help ();
      else
        retval = interactive_help ();
      break;

    case '?':
      retval = interactive_help ();
      break;

    case 'l':
      if (!strncmp (cmd, "ls", cmd_len))
        retval = interactive_ls (d);
      else
        retval = EMSG_UNKNOWN_COMMAND;
      break;

    case 'p':
      if (!strcmp (cmd, "ps"))
        retval = interactive_spawn (d, "/bin/ps", param);
      else if (!strncmp (cmd, "print", cmd_len))
        retval = interactive_print (d, param);
      else
        retval = EMSG_UNKNOWN_COMMAND;
      break;

    case 'q':
      if (!strncmp (cmd, "quit", cmd_len))
        {
          if (d->srv_sckt >= 0)
            {
              close (d->srv_sckt);
            }
          return TRUE;
        }
      else
        retval = EMSG_UNKNOWN_COMMAND;

    case 'r':
      if (!strcmp (cmd, "rm"))
        retval = interactive_rm (d, param);
      else if (!strncmp (cmd, "reload", cmd_len))
        {
          main_reset (d);
          main_load (d);
          retval = EMSG_SUCCESS;
        }
      else if (!strncmp (cmd, "root", cmd_len))
        {
          retval = interactive_root (d, param);
        }
      else
        retval = EMSG_UNKNOWN_COMMAND;
      break;

    case 's':
      if (!strncmp (cmd, "sort", cmd_len))
        {
          if (!strcmp (param, "asc"))
            SORT_TYPE_SET (d->args.sort, SORT_TYPE_ASC);
          else if (!strcmp (param, "desc"))
            SORT_TYPE_SET (d->args.sort, SORT_TYPE_DESC);
          else if (!strcmp (param, "vm"))
            SORT_BY_SET (d->args.sort, SORT_BY_VM);
          else if (!strcmp (param, "sole"))
            SORT_BY_SET (d->args.sort, SORT_BY_SOLE);
          else if (!strcmp (param, "heap"))
            SORT_BY_SET (d->args.sort, SORT_BY_HEAP);
          else if (!strcmp (param, "stack"))
            SORT_BY_SET (d->args.sort, SORT_BY_STACK);
          else if (!strcmp (param, "mapped"))
            SORT_BY_SET (d->args.sort, SORT_BY_MAPPED);
          else if (!strcmp (param, "emapped"))
            SORT_BY_SET (d->args.sort, SORT_BY_EMAPPED);

          main_sort (d);
        }
      else if (!strncmp (cmd, "shell", cmd_len))
        {
          gchar * s = g_strconcat ("\"", param, "\"", NULL);
          retval = interactive_spawn (d, "/bin/sh -r -c", s);
          g_free (s);
        }
      else if (!strncmp (cmd, "save", cmd_len))
        retval = interactive_save (d, param);
      else
        retval = EMSG_UNKNOWN_COMMAND;
      break;

    case 't':
      if (!strncmp (cmd, "threshold", cmd_len))
        {
          retval = interactive_threshold (d, param);
        }
    }

  if (retval != EMSG_SUCCESS)
    {
      switch (retval)
        {
        case EMSG_INVALID_PARAMETER:
          g_print (emsg[retval], param);
          break;

        case EMSG_NO_PROCESS_SELECTED:
          g_print (emsg[retval]);
          break;

        case EMSG_UNKNOWN_COMMAND:
          g_print (emsg[retval], cmd);
          break;

        case EMSG_UNKNOWN_ERROR:
        case EMSG_ERROR_NO_MESSAGE:
          break;

        default:
          g_warning ("Unhandled error value %d", retval);

        }

      return FALSE;
    }

  if (history)
    {
      /* if the line already exists in the history, we remove it
       * first, so that we have no duplicates and the most recent
       * one is always on the top
       */
      if (-1 < history_search_prefix (line, -1))
        {
          int indx = where_history ();
          HIST_ENTRY * h = remove_history (indx);
          if (h)
            {
#if RL_VERSION_MAJOR < 5
              free (h->line);
              free (h);
#else
              free_history_entry (h);
#endif
            }
        }

      add_history (line);
    }

  return FALSE;
}

void
interactive_save_history (data_t *d)
{
  gchar * s = get_hist_file ();
  write_history (s);
  g_free (s);
}

void
interactive_loop (data_t * d)
{
  char * line;

  if (d->args.remote)
    {
      g_print ("  Use 'connect' command to connect to remote server or "
               "'?' for help.\n");
    }
  
  while ((line = readline (": ")))
    {
      if (interactive_parse_line (d, line))
        break;

      free (line);
    }

  interactive_save_history (d);
}
