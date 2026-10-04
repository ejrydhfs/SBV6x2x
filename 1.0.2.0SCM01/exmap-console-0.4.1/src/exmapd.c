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
 * This is a simple deamon that fires up exmap at specified intervals.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>


#include <glib.h>


static void
usage ()
{
  FILE * f = stdout;
  
  fprintf (f, "Exmapd is a deamon that spawns exmap-console at specified intervals.\n");
  
  fprintf (f, "\nUsage: exmapd [options] [exmap options]\n\n");
  fprintf (f, "Options:\nExmapd options:\n");
  fprintf (f, "  --interval=time       : How often to spawn exmap, in minutes; default: 3.\n");
  fprintf (f, "  --log-dir=dir         : Where to put exmap output; default: /var/log/exmapd/.\n");
  fprintf (f, "  --no-fork             : Do not fork.\n");
  fprintf (f, "  --version             : Program version\n");
  fprintf (f, "  --help\n\n");
  fprintf (f, "Exmap options:\n");
  fprintf (f, " For exmap options see exmap.1 man page. In addtion to the supplied options exmapd invokes exmap with --all-pids and --no-file-info.\n\n");
}

typedef struct
{
  guint          timeout;
  const char  *  log_dir;
  const char  ** exmap_argv;
  gint           exmap_argc;
  gboolean       no_fork;
} args_t;

static gboolean
parse_args (int argc, char ** argv, args_t * args)
{
  gint i;
  
  for (i = 1; i < argc; ++i)
    {
      if (*argv[i] != '-')
        break;

      if (!strncmp (argv[i], "--interval", 10))
        {
          int interval = atoi (argv[i] + 11);
          args->timeout = interval * 60 * 1000;
          continue;
        }

      if (!strncmp (argv[i], "--log-dir", 9))
        {
          args->log_dir = argv[i] + 10;
          continue;
        }

      if (!strcmp (argv[i], "--no-fork"))
        {
          args->no_fork = TRUE;
          continue;
        }
      
      if (!strcmp (argv[i], "--help"))
        {
          usage ();
          exit (0);
        }

      if (!strcmp (argv[i], "--version"))
        {
          g_print (VERSION "\n");
          exit (0);
        }

      /* unknown argument -- we are in the exmap args section */
      break;
    }

  /* what's left over we will pass to exmap */
  if (i < argc)
    {
      args->exmap_argv = (const char **) argv + i;
      args->exmap_argc = argc - i;
    }
  else
    {
      args->exmap_argv = NULL;
      args->exmap_argc = 0;
    }
  
  return TRUE;
}

typedef struct
{
  gchar        ** argv;
  const gchar  *  log_dir;
} data_t;

void
data_destroy (data_t * d)
{
  if (d->argv)
    {
      char ** p = d->argv;
      
      while (*p)
        {
          g_free (*p);
          p++;
        }

      g_free (d->argv);
      d->argv = NULL;
    }
}

/*
 * This is the work horse; when called it spawns exmap with appropriate
 * command line arguments, and waits for it to exit.
 */
static gboolean
timeout_cb (gpointer data)
{
  int          status;
  char         buf[25];
  time_t       now;
  struct tm    t;
  data_t     * d = data;
  gchar      * path;
  GError     * error = NULL;

  /* Construct a date+time-based name for the log file */
  now = time (NULL);
  localtime_r (&now, &t);
  strftime (buf, sizeof (buf), "/exmap-%Y%m%d_%H%M%S", &t);
  path = g_strconcat ("--log=", d->log_dir, buf, NULL);

  /* We keep the log file in argv[1]; this changes each call */
  if (d->argv[1])
    g_free (d->argv[1]);

  d->argv[1] = path;

  /* Spawn exmap, deal with errors, etc. */
  if (g_spawn_sync (NULL, d->argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL,
                    NULL, &status, &error))
    {
      if (status)
        g_warning ("Exmap exited with status %d", status);
    }
  else
    {
      g_warning ("Could not spawn exmap: %s", error ? error->message : "");
      if (error)
        {
          g_error_free (error);
          error = NULL;
        }
    }

  return TRUE;
}

int
main (int argc, char ** argv)
{
  int             i;
  struct stat     st;
  args_t          args;
  pid_t           pid;
  data_t          timeout_data;
  int             modp_status = 0;
  GMainLoop     * main_loop = g_main_loop_new (NULL, FALSE);
  char          * modp_args [3] = {"modprobe", "exmap", NULL};
  GError        * error = NULL;
  char         ** p;

  memset (& args, 0, sizeof (args));
  memset (& timeout_data, 0, sizeof (timeout_data));
  
  args.exmap_argv = NULL;
  args.timeout    = 3 * 60 * 1000; /* default to 3min -> ms */
  args.log_dir    = "/var/log/exmapd";

  if (! parse_args (argc, argv, &args))
    {
      exit (-1);
    }

  /* Have to be the superuser to run this (need to be able to call
   * modprobe), but we allow the argument parsing to be done by all, so that
   * exmapd --help and exmapd --version can be used by all.
   */
  if (getuid() != 0)
    g_error ("You must be root to start exmapd daemon");
  
  
  /* The daemon fork */
  if (!args.no_fork)
    {
      g_print("Exmap daemon: forking; run with --no-fork to prevent fork\n");
      
      pid = fork();
      
      switch (pid) 
	{
	case -1:
	  g_error("Fork failed.\n");
	  break;
          
	case 0:
	  /* Child - live and let live */
	  break;
          
	default:
          /* Original process -- time to die and let the younsters to
	   * to take over the world.
	   */
	  exit(0);
	  break;
	}
    }

  /* make sure that the exmap module is loaded */
  if (g_spawn_sync (NULL, (char**)&modp_args, NULL, G_SPAWN_SEARCH_PATH,
                    NULL, NULL, NULL, NULL, &modp_status, &error))
    {
      if (modp_status)
        g_error ("Error loading exmap module; modprobe returned %d",
                 modp_status);
    }
  else
    {
      g_error ("Could not spawn modprobe: %s", error ? error->message : "");
    }
  
  /* Initialise data for the timeout callback
   *
   * 4 == 3 + 1: 3 for the exec name + 2 params we feed exmap
   *             1 for NULL terminator
   */
  timeout_data.argv = g_malloc0 (sizeof (char*) * (4 + args.exmap_argc));
  p = timeout_data.argv;
  *p = g_strdup ("exmap");
  p++;

  /* We leave the second entry empty; this is where --log parameter (which
   * changes call from call) will go
   */
  p++;

  /* Monitor all accessible pids
   *
   * TODO: should it be possible to override this from cmd line ?
   */
  *p = g_strdup ("--all-pids");
  p++;

  /* Now duplicate the rest of the arguments passed to us */
  for (i = 0; i < args.exmap_argc; ++i)
    {
      *p = g_strdup (args.exmap_argv[i]);
      p++;
    }

  timeout_data.log_dir = args.log_dir;

  /* Make sure our logging directory exists */
  if (-1 == stat(args.log_dir, &st))
    {
      if (mkdir (args.log_dir, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH))
        g_error ("Could not create directory [%s], error %d", args.log_dir, i);
    }
  else if (!S_ISDIR (st.st_mode))
    {
      g_error ("[%s] exists and is not a directory !!!", args.log_dir);
    }
  
  /* get the ball rolling */
  g_timeout_add (args.timeout, timeout_cb, &timeout_data);
  g_main_loop_run (main_loop);

  /* cleanup and leave */
  data_destroy (&timeout_data);
  g_main_loop_unref (main_loop);

  return 0;
}
