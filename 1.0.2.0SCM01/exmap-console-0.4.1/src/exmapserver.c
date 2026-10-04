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
 * Exmap server
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
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>

//#include "exmap.h"

static int sckt_in;

#define EXMFILE "/proc/exmap"
#define EXMAP_PROTOCOL_VERSION "0.1"
#define VERSION "0.9"
#define EXMAP_VERSION "0.9"

static void
cmd_end ()
{
  write (sckt_in, "EXMP_DONE\n", 10);
}

static void
write_line (int s, const char * buf, size_t len)
{
  write (s, buf, len);
  write (s, "\n", 1);
}

static void
server_load_pids ()
{
	DIR *dir = opendir("/proc");
	struct dirent *dent;
	if (!dir) return;

	while ((dent = readdir(dir))) {
		if (isdigit(dent->d_name[0])) write_line(sckt_in, dent->d_name, strlen(dent->d_name));
	}

	cmd_end();
	closedir(dir);
	return;
}

static void
server_get_pid_maps (pid_t pid)
{
	char buf [256];
	char cmd_f[256];
	FILE *fcmd = NULL;

	if (!pid) {
		snprintf(buf, sizeof (buf), "EXMP_ERROR Invalid pid %d\n", pid);
		write(sckt_in, buf, strlen (buf));
		return;
	}
  
  snprintf (cmd_f, sizeof cmd_f, "/proc/%d/maps",pid);
  fcmd = fopen (cmd_f, "r");

  if (!fcmd)
    {
      snprintf (buf, sizeof (buf),
                  "EXMP_ERROR Could not read maps for %d\n",
                  pid);
      write (sckt_in, buf, strlen (buf));
      return;
    }

  while (fgets(buf, sizeof (buf), fcmd))
    {
      write (sckt_in, buf, strlen (buf));
    }

  cmd_end ();
}

static void
server_get_pid_cmdline (pid_t pid)
{
  char buf [256];
  char cmd_f[256];
  FILE     * fcmd = NULL;

  if (!pid)
    {
      snprintf (buf, sizeof (buf),
                  "EXMP_ERROR Invalid pid %d\n",
                  pid);
      write (sckt_in, buf, strlen (buf));
      return;
    }
  
  snprintf (cmd_f, sizeof cmd_f, "/proc/%d/cmdline",pid);
  fcmd = fopen (cmd_f, "r");
  
  if (fcmd)
    {
      /* cmd line can be empty, in which case we send a single newline */
      if (fgets(buf, sizeof (buf), fcmd))
        write_line (sckt_in, buf, strlen (buf));
      else
        write (sckt_in, "\n", 1);
      cmd_end ();
    }
  else
    {
      snprintf (buf, sizeof (buf),
                  "EXMP_ERROR Could not read cmdline for %d\n",
                  pid);
      write (sckt_in, buf, strlen (buf));
    }
}

static void
server_get_pid_data (pid_t pid)
{
  char buf [256];
  FILE * fexmp = NULL;

  if (!pid)
    {
      snprintf (buf, sizeof (buf),
                  "EXMP_ERROR Invalid pid %d\n",
                  pid);
      write (sckt_in, buf, strlen (buf));
      return;
    }
  
  fexmp = fopen (EXMFILE, "w+");

  if (fexmp)
    {
      if (EOF == fprintf (fexmp, "%d", pid))
        {
          snprintf (buf, sizeof (buf),
                      "EXMP_ERROR Could not read exmap data for %d\n",
                      pid);
          write (sckt_in, buf, strlen (buf));
        }
      else
        {
          while (fgets (buf, sizeof(buf), fexmp))
            {
              write (sckt_in, buf, strlen (buf));
            }

          cmd_end ();
        }
    }
  else
    {
      snprintf (buf, sizeof (buf),
                  "EXMP_ERROR Could not open /proc/exmap\n");
      write (sckt_in, buf, strlen (buf));
    }
}

/* This the work horse -- it parses incomming commands and acts
 * correspondingly.
 *
 * The cmd set is very limitted -- the server only providies access to
 * to /proc/pid/cmdline and /proc/exmap. Everything else is done by the
 * client.
 *
 * Each command line looks like this
 *
 * EXMP_CMD cmd [param1 param2 ...]
 *
 */
static void
server_parse_line (const char * line)
{
  if (line && !strncmp (line, "EXMP_CMD", 8))
    {
      const char * p = line + 8;

      while (isspace(*p))
        ++p;

      if (!strncmp (p, "SYSPIDS", 7))
        {
          server_load_pids ();
          return;
        }
      
      if (!strncmp (p, "PIDCMDLINE", 10))
        {
          p += 10;
          while (isspace(*p))
            ++p;

          pid_t pid = atoi (p);
          server_get_pid_cmdline (pid);
          return;
        }

      if (!strncmp (p, "PIDMAPS", 7))
        {
          p += 7;
          while (isspace(*p))
            ++p;

          pid_t pid = atoi (p);
          server_get_pid_maps (pid);
          return;
        }

      if (!strncmp (p, "PIDDATA", 7))
        {
          p += 7;
          while (isspace(*p))
            ++p;

          pid_t pid = atoi (p);
          server_get_pid_data (pid);
          return;
        }

      if (!strncmp (p, "SPAWN", 5))
        {
          p += 5;
          while (isspace(*p))
            ++p;
          return;
        }
    }
  else
    {
      char buf [256];

      snprintf (buf, sizeof (buf),
                  "EXMP_ERROR Invalid input '%s'\n",
                  line);
      write (sckt_in, buf, strlen (buf));
    }
}

static void
server_loop (int port)
{
  int sckt;
  struct sockaddr addr_in;

  union 
  {
    struct sockaddr s;
    struct sockaddr_in in;
  } addr_lstn;

  addr_lstn.in.sin_family = AF_INET;
  addr_lstn.in.sin_port = port;
  addr_lstn.in.sin_addr.s_addr = INADDR_ANY;


  if ((sckt = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      fprintf(stderr, "Unable to open socket");
      return;
    }
    
  if (bind (sckt, &addr_lstn.s, sizeof (addr_lstn)) < 0)
    {
      fprintf(stderr, "Unable to bind socket");
      return;
    }

  while (1)
    {
      listen (sckt, 1);
      char buf[100];
      int len;
        
      do
	{
          socklen_t size = sizeof (addr_in);
          sckt_in = accept (sckt, &addr_in, &size);

          if (sckt_in >= 0) {
              int version_check = 0;
              int prot_check = 0;

              /* Handshake -- check that it is a exmap client on the
               * other hand and that its version matches ours.
               */
              if (0 < (len = read (sckt_in, buf, sizeof (buf))))
                {
                  char * p;

                  if ((p = strstr(buf, "EXMP_HANDSHAKE")))
                    {
                      prot_check = 1;
                      p = p + 14;

                      while (*p && isspace (*p))
                        ++p;

                      if (0 == strncmp (p, EXMAP_PROTOCOL_VERSION,
                                        strlen (EXMAP_PROTOCOL_VERSION)))
                        {
                          version_check = 1;
                        }
                    }
                }

              if (prot_check)
                {
                  if (!version_check)
                    {
                      snprintf (buf, sizeof (buf),
                                  "EXMP_ERROR Version mismatch -- "
                                  "server %s\n",
                                  VERSION "-" EXMAP_VERSION);

                      write (sckt_in, buf, strlen (buf));
                      close (sckt_in);
                      sckt_in = -1;
                    }
                  else
                    {
                      cmd_end();
                    }
                }
                    
              if (version_check)
                {
                  /* All preliminaries successfully completed -- wait for
                   * instructions
                   */
                  while (0 < (len = read (sckt_in, buf, sizeof (buf))))
                    {
                      char * line = buf;

                      char * p = strstr (buf, "\n");

                      while (p)
                        {
                          len -= (p - line + 1);
                          *p = 0;
                    
                          server_parse_line (line);
                    
                          line = p + 1;

                          p = strstr (line, "\n");
                        }
                    }

                  close (sckt_in);
                  sckt_in = -1;
                }
            }
	}
      while (1);
    }
    
  close (sckt);
}

static void
signal_handler (int sig)
{
  if (sig == SIGPIPE)
    {
      sckt_in = -1;
      fprintf(stderr, "Caught SIGPIPE\n");
    }
}

static void
usage(const char * name)
{
	fprintf(stderr, "\nThis is Exmap-server for remote exmap sessions.\n");
	fprintf(stderr, "\nUsage: %s [--no-fork] port\n\n", name);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  --no-fork             : Do not fork daemon on startup\n\n");
}

int
main(int argc, char **argv)
{
  int        i;
  int   no_fork = 0;
  pid_t      pid;
  
  if (argc < 2)
    {
      usage (argv[0]);
      return -1;
    }

  for (i = 1; i < argc; ++i)
    {
      if (strncmp ("--", argv[i], 2))
        break;
      
      if (!strcmp (argv[i], "--no-fork"))
        {
          no_fork = 1;
          continue;
        }

      if (!strcmp (argv[i], "--help"))
        {
          usage (argv[0]);
          return 0;
        }

      if (!strcmp (argv[i], "--version"))
        {
          printf(VERSION "\n");
          return 0;
        }

      if (!strcmp (argv[i], "--module-version"))
        {
          printf(EXMAP_VERSION "\n");
          return 0;
        }
    }

  if (i >= argc)
    {
      usage (argv[0]);
      return -1;
    }
  
  int port = atoi (argv[i]);

  if (!port)
    {
      usage (argv[0]);
      return -1;
    }

  /* The daemon fork */
  if (!no_fork)
    {
      printf("Exmap server: forking; run with --no-fork to prevent fork\n");
      
      pid = fork();
      
      switch (pid) 
	{
	case -1:
	  perror("Fork failed.\n");
	  break;
          
	case 0:
	  /* Child - live and let live */
	  break;
          
	default:
          /* Original process -- time to die and let the younsters to
	   * to take over the world.
	   */
	  exit(0);
	}
  }

  /* handle SIGPIPE */
  struct sigaction sa;
  sigfillset (&sa.sa_mask);
  sa.sa_handler = signal_handler;
  sigaction (SIGPIPE, &sa, NULL);
  
  server_loop (port);

  return 0;
}

