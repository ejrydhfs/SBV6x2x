/*	$KAME: dhcp6c.c,v 1.164 2006/01/10 02:46:09 jinmei Exp $	*/
/*
 * Copyright (C) 1998 and 1999 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*-------------------------------------------------------------------------------------
// Copyright 2007, Texas Instruments Incorporated
//
// This program has been modified from its original operation by Texas Instruments
// to do the following:
// Made changes to the code at various places to add plugin hooks
//
// THIS MODIFIED SOFTWARE AND DOCUMENTATION ARE PROVIDED
// "AS IS," AND TEXAS INSTRUMENTS MAKES NO REPRESENTATIONS
// OR WARRENTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
// TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
// PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR
// DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS,
// COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
//
// These changes are covered as per original license.
//-------------------------------------------------------------------------------------*/
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/queue.h>
#include <errno.h>
#include <limits.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <net/if.h>
#ifdef __FreeBSD__
#include <net/if_var.h>
#endif
#include <netinet/in.h>
#ifdef __KAME__
#include <net/if_dl.h>
#include <netinet6/in6_var.h>
#endif

#include <arpa/inet.h>
#include <netdb.h>

#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include <dlfcn.h>
#include "ifaddrs.h"

#include "dhcp6c.h"
#include "dhcp6.h"
#include "config.h"
#include "common.h"
#include "timer.h"
#include "control.h"
#include "dhcp6_ctl.h"
#include "dhcp6c_ia.h"
#include "prefixconf.h"
#include "addrconf.h"
#include "auth.h"

static int debug = 0;
static int exit_ok = 0;
static u_long sig_flags = 0;
#define SIGF_TERM 0x1
#define SIGF_HUP 0x2
#define SIGF_USR1 0x4
#define SIGF_USR2 0x8
#define IPV6_V6ONLY     26

const dhcp6_mode_t dhcp6_mode = DHCP6_MODE_CLIENT;

int sock;	/* inbound/outbound udp port */
int rtsock;	/* routing socket */
int ctlsock = -1;		/* control TCP port */
char *ctladdr = DEFAULT_CLIENT_CONTROL_ADDR;
char *ctlport = DEFAULT_CLIENT_CONTROL_PORT;

//#define DEFAULT_KEYFILE SYSCONFDIR "/dhcp6cctlkey"
#define DEFAULT_KEYFILE "/var/tmp/dhcp6cctlkey"
#define CTLSKEW 300

#define MAX_ELAPSED_TIME 0xffff
/* Added this Boolean global variable. This will indicate 
   whether the DHCP6C needs to intialize itself using the
   conf file (Original Opensource approach) / the TI
   modular plugin architecture approach 
*/
static int ti_use_conf_file = 0;
static char *conffile = DHCP6C_CONF;

static const struct sockaddr_in6 *sa6_allagent;
static struct duid client_duid;
static char *pid_file = DHCP6C_PIDFILE;

static char *ctlkeyfile = DEFAULT_KEYFILE;
static struct keyinfo *ctlkey = NULL;
static int ctldigestlen;

static int infreq_mode = 0;
extern int foreground;

static inline int get_val32 __P((char **, int *, u_int32_t *));
static inline int get_ifname __P((char **, int *, char *, int));

static int client6_init __P((void));
static int client6_startall __P((int));
static void free_resources __P((struct dhcp6_if *));
static void client6_mainloop __P((void));
static int client6_do_ctlcommand __P((char *, ssize_t));
static void client6_reload __P((void));
static int client6_ifctl __P((char *ifname, u_int16_t));
static void check_exit __P((void));
static void process_signals __P((void));
static struct dhcp6_serverinfo *find_server __P((struct dhcp6_event *,
						 struct duid *));
static struct dhcp6_serverinfo *select_server __P((struct dhcp6_event *));
static int client6_recv __P((int *, int *));
static int client6_recvreconfig __P((struct dhcp6_if *, struct dhcp6 *,
				   ssize_t, struct dhcp6_optinfo *));
static int client6_recvadvert __P((struct dhcp6_if *, struct dhcp6 *,
				   ssize_t, struct dhcp6_optinfo *));
static int client6_recvreply __P((struct dhcp6_if *, struct dhcp6 *,
				  ssize_t, struct dhcp6_optinfo *));
static void client6_signal __P((int));
static struct dhcp6_event *find_event_withid __P((struct dhcp6_if *,
						  u_int32_t));
static int construct_confdata __P((struct dhcp6_if *, struct dhcp6_event *));
static int construct_reqdata __P((struct dhcp6_if *, struct dhcp6_optinfo *,
    struct dhcp6_event *));
static void destruct_iadata __P((struct dhcp6_eventdata *));
static void tv_sub __P((struct timeval *, struct timeval *, struct timeval *));
static struct dhcp6_timer *client6_expire_refreshtime __P((void *));
static int process_auth __P((struct authparam *, struct dhcp6 *dh6, ssize_t,
    struct dhcp6_optinfo *, struct dhcp6_if *));
static int set_auth __P((struct dhcp6_event *, struct dhcp6_optinfo *));

struct dhcp6_timer *client6_timo __P((void *));
int client6_start __P((struct dhcp6_if *));
int client6_send __P((struct dhcp6_event* ev));
static void info_printf __P((const char *, ...));

extern int client6_script __P((char *, struct dhcp6_optinfo *));

/* Functions added by TI to enable modular architecture */
static int ti_dhcp6c_save_options_ifp __P((struct dhcp6_if* ifp,struct dhcp6_optinfo* optinfo, struct dhcp6_event*));
static void ti_dhcp6c_confirm_requested __P((struct dhcp6_if *));
static void ti_dhcp6c_renew_requested __P((struct dhcp6_if *));
extern void default_plugin_init(void);
static void ti_exit_client(int);
static int ti_handle_plugin_options(int state, int msg_type, struct dhcp6opt *options, int len);
static int fill_ti_dhcp6c_params(struct dhcp6_optinfo* optinfo, TI_DHCP6C_PARAMS* dhcp6c_params, char* ifname);
static int ti_handle_dhcp6c_info(int state, char msg_type,DHCP6C_ERROR error, struct dhcp6opt *message,
            int len, struct dhcp6_optinfo* optionptr, char* ifname);
static int ti_dhcp6c_option_parse(int argc, char* argv[]);
extern void decline_ia __P((iatype_t, struct dhcp6_list*, struct dhcp6_if*, struct duid*, struct authparam*));

DHCP6C_MCB dhcp6c_mcb;

/* Store the interface name in this variable. This will be used in
   binding the application to the corresponding device. 
*/
char interface[IFNAMSIZ];

/* Global varaible that indicates if the client has been started to 
   send confirm message first 
*/
int confirm_requested = 0;

/* Global variable to store the DHCPv6Server/Relay agent on our link who 
   responded to our DHCP messages.
*/
static const struct sockaddr_in6 sa6_dhcpServerAddr;

//extern struct dhcp6_option options_supported[];
/* supported options are easily added here */
struct dhcp6_option options_supported[] = {
	/* name[32]						code */
	{"clientid",		            0x01},
	{"serverid",	                0x02},
	{"ia_na",	                    0x03},
	{"ia_ta",	                    0x04},
	{"ia_addr",	                	0x05},
#ifdef TI_CONFIG_DHCP6C_ORO
	{"oro",	                        0x06},
#endif
	{"pref",	                    0x07},
	{"elapstim",	                0x08},
	{"auth",	    	            0x0b},
	{"unicast",	    	            0x0c},
	{"stcode",	    	            0x0d},
	{"rapidcomm",	                0x0e},
#ifdef TI_CONFIG_DHCP6C_USERCLASS
	{"userclass",	                0x0f},
#endif
#ifdef TI_CONFIG_DHCP6C_VENDORCLASS
	{"vendclass",	                0x10},
#endif
#ifdef TI_CONFIG_DHCP6C_VENDOROPTIONS
	{"vendoptns",	                0x11},
#endif
	{"intf_id",	                    0x12},
	{"reconfmsg",	                0x13},
	{"reconfacc",	                0x14},
	{"",		                    0x00}
};

char *dhcp6c_error_texts[] = { 
		  "Success",
		  "RFC violation",
		  "Error reading/generating DUID",
		  "DHCP client Initialization error",
		  "Max config attempts reached",
		  "Plugin rejected option",
		  "Plugin error",
		  "short packet received",
		  "truncated packet received",
		  "bugus packet received",
		  "socket read error",
		  "internal error",
		  "XID Mismatch",
		  "Invalid ServerID in packet",
		  "Invalid/Mismatch in ClientID",
		  "Auth Failure",
		  "Status has error in it",
		  "Error allocating memory",
		  "Max config attempts reached (MRC exceeded)",
		  "Max config attempt reached (MRD exceeded)",
		  "Status code - No Prefixes/Addresses Available received",
		  "Status code - Not on link received",
          "Reconfigure message received has no/invalid msg_type option",
		  "General error",
          };

/**************************************************************************
 * FUNCTION NAME : ti_dhcp6c_error2str
 **************************************************************************
 * DESCRIPTION   :
 * A utility function that converts given an error number to a string.
 *************************************************************************/
char *ti_dhcp6c_error2str(DHCP6C_ERROR error) 
{ 
   if(error < TI_DHCP6C_ERROR_LAST) 
    	return dhcp6c_error_texts[error]; 
   else
        return "Unknown error";
}

/**************************************************************************
 * FUNCTION NAME : ti_dhcp6c_get_dhcpserveraddr
 **************************************************************************
 * DESCRIPTION   :
 * Util function that returns the DHCPv6 Server Address of the message
 * last received from a server. 
 *
 * RETURNS       :
 *      char *   ptr to interface name 
 *************************************************************************/
char* ti_dhcp6c_get_dhcpserveraddr(void)
{
	return (addr2str((struct sockaddr *)&sa6_dhcpServerAddr));
}

/**************************************************************************
 * FUNCTION NAME : main
 **************************************************************************
 * DESCRIPTION   :
 *  Entry point for the DHCPCv6 client. 
 *************************************************************************/
int main(int argc,char** argv)
{
	int pid;
	char *progname;
	FILE *pidfp;
    int error = TI_DHCP6C_SUCCESS;

#ifndef HAVE_ARC4RANDOM
	srandom(time(NULL) & getpid());
#endif

	if ((progname = strrchr(*argv, '/')) == NULL)
		progname = *argv;
	else
		progname++;

    default_plugin_init();

    memset(interface, 0, sizeof(interface));

    /* Parse and Process the command line options */
	if ((error = ti_dhcp6c_option_parse(argc,argv)) != TI_DHCP6C_SUCCESS)
	{ 
		printf("\n ti_dhcpv6 client exit due to parser error: %s\n", ti_dhcp6c_error2str(error));
		ti_exit_client(1);
	}

	printf("TI dhcpv6 client (v%s) started \n", VERSION);
    dhcp6c_mcb.plugin.event_logger(0, 0, "startup");

	if (foreground == 0)
		openlog(progname, LOG_NDELAY|LOG_PID, LOG_DAEMON);

    /* Set the default log level */
	setloglevel(debug);

    /* Added an additional check to see if we are indeed supposed to read the 
       configuration from .conf file. The default approach is to NOT use the
       .conf file but to use the TI modular plugin architecture to intialize
       and add options.
    */
	if (ti_use_conf_file == 1 && infreq_mode == 0 && (cfparse(conffile)) != 0) {
		dprintf(LOG_ERR, FNAME, "failed to parse configuration file");
		exit(1);
	}

    /* Check if we need to daemonize ourselves or run in foreground */
    /* for TAHI */
	//if (foreground == 0 && infreq_mode == 0) {
	if (foreground == 0 ) {
		if (daemon(0, 0) < 0)
			err(1, "daemon");
	}

	/* dump current PID */
	pid = getpid();
	if ((pidfp = fopen(pid_file, "w")) != NULL) {
		fprintf(pidfp, "%d\n", pid);
		fclose(pidfp);
	}

    /* Starts the client on all the specified interfaces */
	if((error = client6_startall(0)) != TI_DHCP6C_SUCCESS) {
        dprintf(LOG_ERR, FNAME, "Couldnt start the client on the specified interface. Error: %s", ti_dhcp6c_error2str(error));
        ti_exit_client(1);
    }

    /* Starts the client main loop where the client processes all events like timeouts, incoming packets */
	client6_mainloop();

	ti_exit_client(0);

return 0;
}

/**************************************************************************
 * FUNCTION NAME : client6_init
 **************************************************************************
 * DESCRIPTION   :
 * Called during the startup from main. Does the following:
 *      - Retrieves / generates a client DUID.
 *      - Creates a client socket, binds and initializes it.
 *      - Sets up signal handlers.
 *************************************************************************/
int client6_init()
{
	struct addrinfo hints, *res;
	static struct sockaddr_in6 sa6_allagent_storage;
	int error, on = 1;

	/* get client DUID */
	if (get_duid(DUID_FILE, &client_duid, interface)) {
		dprintf(LOG_ERR, FNAME, "failed to get a DUID for interface %s ", interface);
		printf("failed to get a DUID for interface %s ", interface);
        return TI_DHCP6C_DUID_ERROR;
	}

	if (dhcp6_ctl_authinit(ctlkeyfile, &ctlkey, &ctldigestlen) != 0) {
		dprintf(LOG_NOTICE, FNAME,
		    "failed initialize control message authentication");
		/* run the server anyway */
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = AI_PASSIVE;
	error = getaddrinfo(NULL, DH6PORT_DOWNSTREAM, &hints, &res);
	if (error) {
		dprintf(LOG_ERR, FNAME, "getaddrinfo: %s", gai_strerror(error));
        return TI_DHCP6C_SOCKET_ERROR;
	}

    /* Create client socket to send and receive packets */
	sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sock < 0) {
		dprintf(LOG_ERR, FNAME, "socket");
        return TI_DHCP6C_SOCKET_ERROR;
	}
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)) < 0) {
		dprintf(LOG_ERR, FNAME, "setsockopt(SO_REUSEPORT): %s", strerror(errno));
        if(sock) {
            close(sock);
            sock = -1;
        }
        return TI_DHCP6C_SOCKET_ERROR;
	}
#ifdef IPV6_RECVPKTINFO
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on)) < 0) {
		dprintf(LOG_ERR, FNAME,"setsockopt(IPV6_RECVPKTINFO): %s",strerror(errno));
        if(sock) {
            close(sock);
            sock = -1;
        }
        return TI_DHCP6C_SOCKET_ERROR;
	}
#else
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_PKTINFO, &on, sizeof(on)) < 0) {
		dprintf(LOG_ERR, FNAME,"setsockopt(IPV6_PKTINFO): %s", strerror(errno));
        if(sock) {
            close(sock);
            sock = -1;
        }
        return TI_DHCP6C_SOCKET_ERROR;
	}
#endif
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &on, sizeof(on)) < 0) {
		dprintf(LOG_ERR, FNAME,"setsockopt(sock, IPV6_MULTICAST_LOOP): %s", strerror(errno));
        if(sock) {
            close(sock);
            sock = -1;
        }
        return TI_DHCP6C_SOCKET_ERROR;
	}
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) < 0) {
		dprintf(LOG_ERR, FNAME, "setsockopt(IPV6_V6ONLY): %s", strerror(errno));
        if(sock) {
            close(sock);
            sock = -1;
        }
        return TI_DHCP6C_SOCKET_ERROR;
	}

    /* Bind the socket to the interface specified in the command line.
       Required for the application to work fine with multiple 
       logical stacks on the device.
    */
    if(strlen(interface) > 0) {
        if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE,interface, strlen(interface) + 1) < 0) {
            dprintf(LOG_ERR,FNAME, "ti_dhcpcv6: setsockopt BINDTODEVICE");
            if(sock) {
                close(sock);
                sock = -1;
            }
            return TI_DHCP6C_SOCKET_ERROR;
        }
    }

	/*
	 * According RFC3315 2.2, only the incoming port should be bound to UDP
	 * port 546.  However, to have an interoperability with some servers,
	 * the outgoing port is also bound to the DH6PORT_DOWNSTREAM.
	 */
	if (bind(sock, res->ai_addr, res->ai_addrlen) < 0) {
		dprintf(LOG_ERR, FNAME, "bind: %s", strerror(errno));
        if(sock) {
            close(sock);
            sock = -1;
        }
        return TI_DHCP6C_SOCKET_ERROR;
	}
	freeaddrinfo(res);

	/* open a routing socket to watch the routing table */
	if ((rtsock = socket(PF_ROUTE, SOCK_RAW, 0)) < 0) {
		dprintf(LOG_ERR, FNAME, "open a routing socket: %s", strerror(errno));
        if(sock) {
            close(sock);
            sock = -1;
        }
        return TI_DHCP6C_SOCKET_ERROR;
	}

    /* Get the socket details to reach "All DHCP servers and Relay Agents" multicast address ff02::1:2 */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	error = getaddrinfo(DH6ADDR_ALLAGENT, DH6PORT_UPSTREAM, &hints, &res);
	if (error) {
		dprintf(LOG_ERR, FNAME, "getaddrinfo: %s", gai_strerror(error));
        if(sock) {
            close(sock);
            sock = -1;
        }
        return TI_DHCP6C_SOCKET_ERROR;
	}
	memcpy(&sa6_allagent_storage, res->ai_addr, res->ai_addrlen);
	sa6_allagent = (const struct sockaddr_in6 *)&sa6_allagent_storage;
	freeaddrinfo(res);

	/* set up control socket */
	if (ctlkey == NULL)
		dprintf(LOG_NOTICE, FNAME, "skip opening control port");
	else if (dhcp6_ctl_init(ctladdr, ctlport,
	    DHCP6CTL_DEF_COMMANDQUEUELEN, &ctlsock)) {
		dprintf(LOG_ERR, FNAME, "failed to initialize control channel");
        if(sock) {
            close(sock);
            sock = -1;
        }
        return TI_DHCP6C_SOCKET_ERROR;
	}

    /* Register the required signal handlers */
	if (signal(SIGHUP, client6_signal) == SIG_ERR) {
		dprintf(LOG_WARNING, FNAME, "failed to set signal: %s", strerror(errno));
        if(sock) {
            close(sock);
            sock = -1;
        }
        return TI_DHCP6C_SOCKET_ERROR;
	}
	if (signal(SIGINT, client6_signal) == SIG_ERR) {
		dprintf(LOG_WARNING, FNAME, "failed to set signal: %s", strerror(errno));
        if(sock) {
            close(sock);
            sock = -1;
        }
        return TI_DHCP6C_SOCKET_ERROR;
	}
	if (signal(SIGTERM, client6_signal) == SIG_ERR) {
		printf(LOG_WARNING, FNAME, "failed to set signal: %s", strerror(errno));
        if(sock) {
            close(sock);
            sock = -1;
        }
        return TI_DHCP6C_SOCKET_ERROR;
	}
	if (signal(SIGUSR1, client6_signal) == SIG_ERR) {
		dprintf(LOG_WARNING, FNAME, "failed to set signal: %s", strerror(errno));
        if(sock) {
            close(sock);
            sock = -1;
        }
        return TI_DHCP6C_SOCKET_ERROR;
	}
	if (signal(SIGUSR2, client6_signal) == SIG_ERR) {
		dprintf(LOG_WARNING, FNAME, "failed to set signal: %s", strerror(errno));
        if(sock) {
            close(sock);
            sock = -1;
        }
        return TI_DHCP6C_SOCKET_ERROR;
	}
    return TI_DHCP6C_SUCCESS;
}

/**************************************************************************
 * FUNCTION NAME : client6_start
 **************************************************************************
 * DESCRIPTION   :
 * Does the following
 *      - Sets up the timers for initial delays.
 *      - Creates and initializes the event on the interface to start 
 *        the state machine on it.
 *************************************************************************/
int client6_start(ifp)
	struct dhcp6_if *ifp;
{
	struct dhcp6_event *ev;

	/* make sure that the interface does not have a timer */
	if (ifp->timer != NULL) {
		dprintf(LOG_DEBUG, FNAME,"removed existing timer on %s", ifp->ifname);
		dhcp6_remove_timer(&ifp->timer);
	}

	/* create an event for the initial delay */
    /* Event is the data structure which contains information
       relevant to the state machine that needs to be run 
       on the specific interface 
    */
	if ((ev = dhcp6_create_event(ifp, DHCP6S_INIT)) == NULL) {
		dprintf(LOG_NOTICE, FNAME, "failed to create an event");
		return TI_DHCP6C_INITLZN_ERROR;
	}
	TAILQ_INSERT_TAIL(&ifp->event_list, ev, link);

    /* Init the authentication params too */
    if(ifp->authproto != DHCP6_AUTHPROTO_RECONFIG) {
	    if ((ev->authparam = new_authparam(ifp->authproto,
	        ifp->authalgorithm, ifp->authrdm)) == NULL) {
		    dprintf(LOG_WARNING, FNAME, "failed to allocate "
		        "authentication parameters");
		    dhcp6_remove_event(ev);
		    return TI_DHCP6C_INITLZN_ERROR;
	    }
    }
    else {
	    if ((ev->authparam = new_authparam(DHCP6_AUTHPROTO_UNDEF,
	        DHCP6_AUTHALG_UNDEF, DHCP6_AUTHRDM_UNDEF)) == NULL) {
		    dprintf(LOG_WARNING, FNAME, "failed to allocate "
		        "authentication parameters");
		    dhcp6_remove_event(ev);
		    return TI_DHCP6C_INITLZN_ERROR;
	    }
    }

    /* Add the timeout call back function and init the timer */
	if ((ev->timer = dhcp6_add_timer(client6_timo, ev)) == NULL) {
		dprintf(LOG_NOTICE, FNAME, "failed to add a timer for %s",
		    ifp->ifname);
		dhcp6_remove_event(ev);
		return TI_DHCP6C_INITLZN_ERROR;
	}
	dhcp6_reset_timer(ev);

	return TI_DHCP6C_SUCCESS;
}

/**************************************************************************
 * FUNCTION NAME : client6_startall 
 **************************************************************************
 * DESCRIPTION   :
 * Starts the dhcpcv6 client on all the specified interfaces.
 *************************************************************************/
static int client6_startall(isrestart)
	int isrestart;
{
	struct dhcp6_if *ifp;
    int error = TI_DHCP6C_SUCCESS;

	for (ifp = dhcp6_if; ifp; ifp = ifp->next) {
		if (isrestart &&ifreset(ifp)) {
			dprintf(LOG_NOTICE, FNAME, "failed to reset %s", ifp->ifname);
			continue; /* XXX: try to recover? */
		}
        /* init the state machine on the interface */
		if ((error = client6_start(ifp)) != TI_DHCP6C_SUCCESS)
            return TI_DHCP6C_INITLZN_ERROR;
	}
    return TI_DHCP6C_SUCCESS;
}

/**************************************************************************
 * FUNCTION NAME : free_resources
 **************************************************************************
 * DESCRIPTION   :
 * Free all the resources held by an interface.
 *************************************************************************/
static void
free_resources(freeifp)
	struct dhcp6_if *freeifp;
{
	struct dhcp6_if *ifp;

	for (ifp = dhcp6_if; ifp; ifp = ifp->next) {
		struct dhcp6_event *ev, *ev_next;

		if (freeifp != NULL && freeifp != ifp)
			continue;

		/* release all IAs as well as send RELEASE message(s) */
		release_all_ia(ifp);

		/*
		 * Cancel all outstanding events for each interface except
		 * ones being released.
		 */
		for (ev = TAILQ_FIRST(&ifp->event_list); ev; ev = ev_next) {
			ev_next = TAILQ_NEXT(ev, link);

			if (ev->state == DHCP6S_RELEASE)
				continue; /* keep it for now */

			dhcp6_remove_event(ev);
		}
	}
}

/**************************************************************************
 * FUNCTION NAME : check_exit
 **************************************************************************
 * DESCRIPTION   :
 * Check if all the resources are freed up before exiting.
 *************************************************************************/
static void
check_exit()
{
	struct dhcp6_if *ifp;

	if (!exit_ok)
		return;

	for (ifp = dhcp6_if; ifp; ifp = ifp->next) {
		/*
		 * Check if we have an outstanding event.  If we do, we cannot
		 * exit for now.
		 */
		if (!TAILQ_EMPTY(&ifp->event_list))
			return;
	}

	/* We have no existing event.  Do exit. */
	dprintf(LOG_DEBUG, FNAME, "exiting");

	exit(0);
}

/**************************************************************************
 * FUNCTION NAME : ti_dhcp6c_confirm_requested
 **************************************************************************
 * DESCRIPTION   :
 * Issues out confirm message on all the IAs related to an interface. If no
 * interface is specified, all IAs on all interfaces are renewed.
 *************************************************************************/
static void
ti_dhcp6c_confirm_requested(freeifp)
	struct dhcp6_if *freeifp;
{
	struct dhcp6_if *ifp;

	for (ifp = dhcp6_if; ifp; ifp = ifp->next) {

		if (freeifp != NULL && freeifp != ifp)
			continue;

		/* confirm all IAs */
		confirm_all_ia(ifp);
	}
}


/**************************************************************************
 * FUNCTION NAME : ti_dhcp6c_renew_requested
 **************************************************************************
 * DESCRIPTION   :
 * Issues out renew commmand on all the IAs related to an interface. If no
 * interface is specified, all IAs on all interfaces are renewed.
 *************************************************************************/
static void
ti_dhcp6c_renew_requested(freeifp)
	struct dhcp6_if *freeifp;
{
	struct dhcp6_if *ifp;

	for (ifp = dhcp6_if; ifp; ifp = ifp->next) {

		if (freeifp != NULL && freeifp != ifp)
			continue;

		/* renews all IAs */
		renew_all_ia(ifp);
	}
}

/**************************************************************************
 * FUNCTION NAME : process_signals
 **************************************************************************
 * DESCRIPTION   :
 * Checks if a signal was received, if so performs the necessary action. 
 *************************************************************************/
static void process_signals()
{
    /* Handle SIGTERM */
	if ((sig_flags & SIGF_TERM)) {
		dprintf(LOG_DEBUG, FNAME, "exiting");
		exit_ok = 1;
		free_resources(NULL);
		unlink(pid_file);
		check_exit();
	}

    /* Handle SIGHUP - Sends out Confirm message */
	if ((sig_flags & SIGF_HUP)) {
		dprintf(LOG_DEBUG, FNAME, "confirm requested");
        ti_dhcp6c_confirm_requested(NULL);
        dhcp6c_mcb.plugin.event_logger(0, 0, "Confirmed all IAs");
	}
#if 0
	if ((sig_flags & SIGF_HUP)) {
		dprintf(LOG_DEBUG, FNAME, "restarting");
		free_resources(NULL);
		if((error = client6_startall(1)) < 0) {
            dprintf(LOG_ERR, FNAME, "Couldnt start the client on the specified interface. Error: %s \n", ti_dhcp6c_error2str(error));
            dhcp6c_mcb.plugin.event_logger(0, 0, ti_dhcp6c_error2str(error));
            ti_exit_client(1);
        }
	}
#endif

    /* Handle SIGUSR1 - Renew Request */
	if ((sig_flags & SIGF_USR1)) {
		dprintf(LOG_DEBUG, FNAME, "renew requested");
        ti_dhcp6c_renew_requested(NULL);
        dhcp6c_mcb.plugin.event_logger(0, 0, "Renewed all IAs");
	}
    /* Handle SIGUSR2 - Release Request */
	if ((sig_flags & SIGF_USR2)) {
		dprintf(LOG_DEBUG, FNAME, "release requested");
        free_resources(NULL);
        dhcp6c_mcb.plugin.event_logger(0, 0, "Released all IAs");
	}

	sig_flags = 0;
}

/**************************************************************************
 * FUNCTION NAME : client6_mainloop
 **************************************************************************
 * DESCRIPTION   :
 * Client's infinite loop wherein it calls respective functions to:
 *      - Process signals.
 *      - Process timeouts.
 *      - Process incoming packets on the socket.
 *************************************************************************/
static void client6_mainloop()
{
	struct timeval *w;
	int ret, maxsock;
	fd_set r;
    int error = TI_DHCP6C_SUCCESS;
    int msg_type, state;

	while(1) {

        /* If a signal was delivered to the process meanwhile, the sig_flags 
           would have been set with the appropriate signal flag by the 
           client6_signal() function. If so, call process_signals() to 
           act upon it 
        */
		if (sig_flags)
			process_signals();

        /* Check for expiration of timer, process it and returns the next
           timer val for the select call
        */
		w = dhcp6_check_timer();

		FD_ZERO(&r);
		FD_SET(sock, &r);
		maxsock = sock;
		if (ctlsock >= 0) {
			FD_SET(ctlsock, &r);
			maxsock = (sock > ctlsock) ? sock : ctlsock;
			(void)dhcp6_ctl_setreadfds(&r, &maxsock);
		}

        /* Check if there is any data on the sockets */
		ret = select(maxsock + 1, &r, NULL, NULL, w);

		switch (ret) {
		case -1:
			if (errno != EINTR) {
				dprintf(LOG_ERR, FNAME, "select: %s", strerror(errno));
				ti_exit_client(1);
			}
			continue;
		case 0:	/* timeout */
			break;	/* dhcp6_check_timer() will treat the case */
		default:
			break;
		}

        /* Incoming packet on client socket, handle it */
		if (FD_ISSET(sock, &r))
			if((error = client6_recv(&msg_type, &state)) != TI_DHCP6C_SUCCESS) {
                dprintf(LOG_ERR, FNAME, "Error in packet receive. Error: %s msg_type: %d state: %d", ti_dhcp6c_error2str(error), msg_type, state);
                dhcp6c_mcb.plugin.event_logger(state, state,ti_dhcp6c_error2str(error)); 
                ti_handle_dhcp6c_info(state, msg_type, error, NULL, 0, NULL, NULL);
            }
        /* Data on control socket, handle it */
		if (ctlsock >= 0) {
			if (FD_ISSET(ctlsock, &r)) {
				(void)dhcp6_ctl_acceptcommand(ctlsock,
				    client6_do_ctlcommand);
			}
			(void)dhcp6_ctl_readcommand(&r);
		}
	}
}

/**************************************************************************
 * FUNCTION NAME : get_val32
 **************************************************************************
 * DESCRIPTION   :
 * Converts the data contained in the buffer to 32bit int.
 *************************************************************************/
static inline int
get_val32(bpp, lenp, valp)
	char **bpp;
	int *lenp;
	u_int32_t *valp;
{
	char *bp = *bpp;
	unsigned int len = *lenp;
	u_int32_t i32;

	if (len < sizeof(*valp))
		return (-1);

	memcpy(&i32, bp, sizeof(i32));
	*valp = ntohl(i32);

	*bpp = bp + sizeof(*valp);
	*lenp = len - sizeof(*valp);

	return (0);
}

/**************************************************************************
 * FUNCTION NAME : get_ifname
 **************************************************************************
 * DESCRIPTION   :
 * Retrieves the interface name.
 *************************************************************************/
static inline int
get_ifname(bpp, lenp, ifbuf, ifbuflen)
	char **bpp;
	int *lenp;
	char *ifbuf;
	int ifbuflen;
{
	char *bp = *bpp;
	int len = *lenp, ifnamelen;
	u_int32_t i32;

	if (get_val32(bpp, lenp, &i32))
		return (-1);
	ifnamelen = (int)i32;

	if (*lenp < ifnamelen || ifnamelen > ifbuflen)
		return (-1);

	memset(ifbuf, 0, sizeof(ifbuf));
	memcpy(ifbuf, *bpp, ifnamelen);
	if (ifbuf[ifbuflen - 1] != '\0')
		return (-1);	/* not null terminated */

	*bpp = bp + sizeof(i32) + ifnamelen;
	*lenp = len - (sizeof(i32) + ifnamelen);

	return (0);
}

/**************************************************************************
 * FUNCTION NAME : client6_do_ctlcommand
 **************************************************************************
 * DESCRIPTION   :
 * Handles commands sent on the control socket.
 *************************************************************************/
static int
client6_do_ctlcommand(buf, len)
	char *buf;
	ssize_t len;
{
	struct dhcp6ctl *ctlhead;
	u_int16_t command, version;
	u_int32_t p32, ts, ts0;
	unsigned int commandlen;
	char *bp;
	char ifname[IFNAMSIZ];
	time_t now;

	memset(ifname, 0, sizeof(ifname));

	ctlhead = (struct dhcp6ctl *)buf;

	command = ntohs(ctlhead->command);
	commandlen = (int)(ntohs(ctlhead->len));
	version = ntohs(ctlhead->version);
	if ((unsigned long)len != sizeof(struct dhcp6ctl) + commandlen) {
		dprintf(LOG_ERR, FNAME,"assumption failure: command length mismatch");
		return (DHCP6CTL_R_FAILURE);
	}

	/* replay protection and message authentication */
	if ((now = time(NULL)) < 0) {
		dprintf(LOG_ERR, FNAME, "failed to get current time: %s",
		    strerror(errno));
		return (DHCP6CTL_R_FAILURE);
	}
	ts0 = (u_int32_t)now;
	ts = ntohl(ctlhead->timestamp);
	if (ts + CTLSKEW < ts0 || (ts - CTLSKEW) > ts0) {
		dprintf(LOG_DEBUG, FNAME, "timestamp is out of range");
		return (DHCP6CTL_R_FAILURE);
	}

	if (ctlkey == NULL) {	/* should not happen!! */
		dprintf(LOG_ERR, FNAME, "no secret key for control channel");
		return (DHCP6CTL_R_FAILURE);
	}
	if (dhcp6_verify_mac(buf, len, 
	    DHCP6CTL_AUTHALG_HMACMD5, sizeof(*ctlhead), ctlkey) != 0) {
		dprintf(LOG_DEBUG, FNAME, "authentication failure");
		return (DHCP6CTL_R_FAILURE);
	}

	bp = buf + sizeof(*ctlhead) + ctldigestlen;
	commandlen -= ctldigestlen;

	if (version > DHCP6CTL_VERSION) {
		dprintf(LOG_DEBUG, FNAME, "unsupported version: %d", version);
		return (DHCP6CTL_R_FAILURE);
	}

	switch (command) {
	case DHCP6CTL_COMMAND_RELOAD:
		if (commandlen != 0) {
			dprintf(LOG_DEBUG, FNAME, "invalid command length "
			    "for reload: %d", commandlen);
			return (DHCP6CTL_R_DONE);
		}
		client6_reload();
		break;
	case DHCP6CTL_COMMAND_START:
		if (get_val32(&bp, &commandlen, &p32))
			return (DHCP6CTL_R_FAILURE);
		switch (p32) {
		case DHCP6CTL_INTERFACE:
			if (get_ifname(&bp, &commandlen, ifname,
			    sizeof(ifname))) {
				return (DHCP6CTL_R_FAILURE);
			}
			if (client6_ifctl(ifname, DHCP6CTL_COMMAND_START))
				return (DHCP6CTL_R_FAILURE);
			break;
		default:
			dprintf(LOG_DEBUG, FNAME,
			    "unknown start target: %ul", p32);
			return (DHCP6CTL_R_FAILURE);
		}
		break;
	case DHCP6CTL_COMMAND_STOP:
		if (commandlen == 0) {
			exit_ok = 1;
			free_resources(NULL);
			unlink(pid_file);
			check_exit();
		} else {
			if (get_val32(&bp, &commandlen, &p32))
				return (DHCP6CTL_R_FAILURE);

			switch (p32) {
			case DHCP6CTL_INTERFACE:
				if (get_ifname(&bp, &commandlen, ifname,
				    sizeof(ifname))) {
					return (DHCP6CTL_R_FAILURE);
				}
				if (client6_ifctl(ifname,
				    DHCP6CTL_COMMAND_STOP)) {
					return (DHCP6CTL_R_FAILURE);
				}
				break;
			default:
				dprintf(LOG_DEBUG, FNAME,
				    "unknown start target: %ul", p32);
				return (DHCP6CTL_R_FAILURE);
			}
		}
		break;
	default:
		dprintf(LOG_DEBUG, FNAME,
		    "unknown control command: %d (len=%d)",
		    (int)command, commandlen);
		return (DHCP6CTL_R_FAILURE);
	}

  	return (DHCP6CTL_R_DONE);
}

/**************************************************************************
 * FUNCTION NAME : client6_reload
 **************************************************************************
 * DESCRIPTION   :
 * Reads the configuration file again and loads it to the client.
 *************************************************************************/
static void
client6_reload()
{
	/* reload the configuration file */
	if (ti_use_conf_file && cfparse(conffile) != 0) {
		dprintf(LOG_WARNING, FNAME,
		    "failed to reload configuration file");
		return;
	}

	dprintf(LOG_NOTICE, FNAME, "client reloaded");

	return;
}

/**************************************************************************
 * FUNCTION NAME : client6_ifctl
 **************************************************************************
 * DESCRIPTION   :
 * Handles commands sent to the interface through the control socket.
 *************************************************************************/
static int
client6_ifctl(ifname, command)
	char *ifname;
	u_int16_t command;
{
	struct dhcp6_if *ifp;

	if ((ifp = find_ifconfbyname(ifname)) == NULL) {
		dprintf(LOG_DEBUG, FNAME,
		    "failed to find interface configuration for %s",
		    ifname);
		return (-1);
	}

	dprintf(LOG_DEBUG, FNAME, "%s interface %s",
	    command == DHCP6CTL_COMMAND_START ? "start" : "stop", ifname);

	switch(command) {
	case DHCP6CTL_COMMAND_START:
		free_resources(ifp);
		if (client6_start(ifp)) {
			dprintf(LOG_NOTICE, FNAME, "failed to restart %s",
			    ifname);
			return (-1);
		}
		break;
	case DHCP6CTL_COMMAND_STOP:
		free_resources(ifp);
		if (ifp->timer != NULL) {
			dprintf(LOG_DEBUG, FNAME,
			    "removed existing timer on %s", ifp->ifname);
			dhcp6_remove_timer(&ifp->timer);
		}
		break;
	default:		/* impossible case, should be a bug */
		dprintf(LOG_ERR, FNAME, "unknown command: %d", (int)command);
		break;
	}

	return (0);
}

/**************************************************************************
 * FUNCTION NAME : client6_expire_refreshtime
 **************************************************************************
 * DESCRIPTION   :
 * This function called when the refresh_time expires on the configuration
 * retrieved in the last INFO_REQ. It restarts the state machine to fetch
 * the configuration once again.
 *************************************************************************/
static struct dhcp6_timer *
client6_expire_refreshtime(arg)
	void *arg;
{
	struct dhcp6_if *ifp = arg;

	dprintf(LOG_DEBUG, FNAME,
	    "information refresh time on %s expired", ifp->ifname);

	dhcp6_remove_timer(&ifp->timer);
	client6_start(ifp);

	return (NULL);
}

/**************************************************************************
 * FUNCTION NAME : client6_timo
 **************************************************************************
 * DESCRIPTION   :
 * This function handles all the timeouts.
 *************************************************************************/
struct dhcp6_timer *
client6_timo(arg)
	void *arg;
{
	struct dhcp6_event *ev = (struct dhcp6_event *)arg;
	struct dhcp6_if *ifp;
	int state = ev->state;
	char plugin_buf[BUFSIZ];
	struct dhcp6_optinfo optinfo;
	ssize_t pluginbuf_len = 0;
    int err_code = 0;
    struct timeval now;

	ifp = ev->ifp;
	ev->timeouts++;

	/*
	 * Unless MRC is zero, the message exchange fails once the client has
	 * transmitted the message MRC times.
	 * [RFC3315 14.]
	 */
	if (ev->max_retrans_cnt && ev->timeouts > ev->max_retrans_cnt) {
		dprintf(LOG_DEBUG, FNAME, "no responses were received");
        dhcp6c_mcb.plugin.event_logger(ev->state, 0, "No response received so far. MRC exceeded");

        if((ev->state == DHCP6S_RENEW || ev->state == DHCP6S_INFOREQ) && ifp->reconf_in_progress &&
            !TAILQ_EMPTY(&ev->reconf_reqopt_list)) {
            ifp->reconf_in_progress = 0;
        }
        else
            ti_handle_dhcp6c_info(ev->state, 0, TI_DHCP6C_MRC_EXCEEDED, NULL, 0, NULL, NULL);

		dhcp6_remove_event(ev);

		if (state == DHCP6S_RELEASE)
			check_exit();

        return NULL;
	}
	/*
	 * Unless MRD is zero, the message exchange fails once the client has
	 * transmitted for MRD seconds the same message.
	 * [RFC3315 14.]
	 */
	get_sysUpTime(&now);
	if (ev->max_retrans_dur && ((now.tv_sec - ev->first_tx_time) > (ev->max_retrans_dur / 1000))) {
		dprintf(LOG_DEBUG, FNAME, "no responses were received");
        dhcp6c_mcb.plugin.event_logger(ev->state, 0, "No response received so far. MRD exceeded");

        ti_handle_dhcp6c_info(ev->state, 0, TI_DHCP6C_MRD_EXCEEDED, NULL, 0, NULL, NULL);

		dhcp6_remove_event(ev);

        confirm_requested = 0;

		if (state == DHCP6S_RELEASE)
			check_exit();

        return NULL;
	}

	switch(ev->state) {
	case DHCP6S_INIT:
		ev->timeouts = 0; /* indicate to generate a new XID. */
		if ((ifp->send_flags & DHCIFF_INFO_ONLY) || infreq_mode) {
			ev->state = DHCP6S_INFOREQ;
            dhcp6c_mcb.plugin.event_logger(ev->state, 0, "Starting DHCP client in INFORMATION_REQUEST mode");
        }
        else if(confirm_requested == 1) {
	        ev->state = DHCP6S_CONFIRM;
            dhcp6c_mcb.plugin.event_logger(ev->state, ev->state, "Starting DHCP client in Confirm-Reply mode");

            /* If this is the first time we are getting IA_NA/IA_PD options from plugin
               we'll save them in ifp for later use 
            */
            dhcp6_init_options(&optinfo);

    	    dprintf(LOG_DEBUG, FNAME, "Going to call plugin init options");
            err_code = dhcp6c_mcb.plugin.init_options(ev->state, DH6_CONFIRM,
                    (struct dhcp6opt *)(plugin_buf), (struct dhcp6opt *)(plugin_buf + sizeof(plugin_buf)), &pluginbuf_len);
            if(err_code != TI_DHCP6C_SUCCESS) {
                dprintf(LOG_DEBUG, FNAME, "Plugin rejected init options");
                ti_exit_client(1);
            }

	        dprintf(LOG_DEBUG, FNAME, "After init options: pluginbuf_start: %p len: %d", plugin_buf, pluginbuf_len);
    	    if (dhcp6_xfer_options((struct dhcp6opt *)plugin_buf, 
                                    (struct dhcp6opt *)(plugin_buf + pluginbuf_len), &optinfo) < 0) {
    	    	dprintf(LOG_DEBUG, FNAME, "failed to copy plugin options into optinfo");
                ti_exit_client(1);
    	    }

            if(!ti_use_conf_file) {
	    	    dprintf(LOG_ERR, FNAME, "Going to call save_options");
        	    if (ti_dhcp6c_save_options_ifp(ifp, &optinfo, ev)) {
	        	    dprintf(LOG_ERR, FNAME, "can't copy to event data");
                    ti_exit_client(1);
        	    }
            }
        	dhcp6_clear_options(&optinfo);

            if(TAILQ_EMPTY(&ifp->iaconf_list)) {
                dprintf(LOG_ERR, FNAME, "No IAs to send confirm");
                dhcp6_remove_event(ev);
                ti_exit_client(1);
            }
        }
		else {
	        ev->state = DHCP6S_SOLICIT;
            dhcp6c_mcb.plugin.event_logger(ev->state, 0, "Starting DHCP client in Solicit-Adv-Req-Reply mode");

            /* If this is the first time we are getting IA_NA/IA_PD options from plugin
               we'll save them in ifp for later use 
            */
            dhcp6_init_options(&optinfo);

    	    dprintf(LOG_DEBUG, FNAME, "Going to call plugin init options");
            err_code = dhcp6c_mcb.plugin.init_options(ev->state, DH6_SOLICIT,
                    (struct dhcp6opt *)(plugin_buf), (struct dhcp6opt *)(plugin_buf + sizeof(plugin_buf)), &pluginbuf_len);
            if(err_code != TI_DHCP6C_SUCCESS) {
                dprintf(LOG_DEBUG, FNAME, "Plugin rejected init options");
                ti_exit_client(1);
            }

	        dprintf(LOG_DEBUG, FNAME, "After init options: pluginbuf_start: %p len: %d", plugin_buf, pluginbuf_len);
    	    if (dhcp6_xfer_options((struct dhcp6opt *)plugin_buf, 
                                    (struct dhcp6opt *)(plugin_buf + pluginbuf_len), &optinfo) < 0) {
    	    	dprintf(LOG_DEBUG, FNAME, "failed to copy plugin options into optinfo");
                ti_exit_client(1);
    	    }

            if(!ti_use_conf_file) {
	    	    dprintf(LOG_ERR, FNAME, "Going to call save_options");
        	    if (ti_dhcp6c_save_options_ifp(ifp, &optinfo, ev)) {
	        	    dprintf(LOG_ERR, FNAME, "can't copy to event data");
                    ti_exit_client(1);
        	    }
            }
        	dhcp6_clear_options(&optinfo);
        }
       	if ((err_code = construct_confdata(ifp, ev)) != TI_DHCP6C_SUCCESS) {
        	dprintf(LOG_ERR, FNAME, "can't send message, error in construct_confdata");
            return NULL;
   		}
		dhcp6_set_timeoparam(ev); /* XXX */
		/* fall through */
	case DHCP6S_REQUEST:
	case DHCP6S_RELEASE:
	case DHCP6S_INFOREQ:
    case DHCP6S_DECLINE:
		client6_send(ev);
		break;
	case DHCP6S_RENEW:
	case DHCP6S_REBIND:
    case DHCP6S_CONFIRM:
		if (!TAILQ_EMPTY(&ev->data_list))
			client6_send(ev);
		else {
			dprintf(LOG_DEBUG, FNAME,
			    "all information to be updated was canceled");
			dhcp6_remove_event(ev);
			return (NULL);
		}
		break;
	case DHCP6S_SOLICIT:
		if (ev->servers) {
			/*
			 * Send a Request to the best server.
			 * Note that when we set Rapid-commit in Solicit,
			 * but a direct Reply has been delayed (very much),
			 * the transition to DHCP6S_REQUEST (and the change of
			 * transaction ID) will invalidate the reply even if it
			 * ever arrives.
			 */
			ev->current_server = select_server(ev);
			if (ev->current_server == NULL) {
				/* this should not happen! */
				dprintf(LOG_NOTICE, FNAME,
				    "can't find a server");
				exit(1); /* XXX */
			}
			if (duidcpy(&ev->serverid,
			    &ev->current_server->optinfo.serverID)) {
				dprintf(LOG_NOTICE, FNAME,
				    "can't copy server ID");
				return (NULL); /* XXX: better recovery? */
			}
			ev->timeouts = 0;
			ev->state = DHCP6S_REQUEST;
			dhcp6_set_timeoparam(ev);

			if (ev->authparam != NULL)
				free(ev->authparam);
			ev->authparam = ev->current_server->authparam;
			ev->current_server->authparam = NULL;

		    if ((err_code = construct_reqdata(ifp,
		        &ev->current_server->optinfo, ev)) != TI_DHCP6C_SUCCESS) {
    			dprintf(LOG_NOTICE, FNAME,
	    		    "failed to construct request data");
                return NULL;
		    }
		}
		client6_send(ev);
		break;
	}

	dhcp6_reset_timer(ev);

	return (ev->timer);
}

/**************************************************************************
 * FUNCTION NAME : ti_dhcp6c_save_options_ifp
 **************************************************************************
 * DESCRIPTION   :
 * Build the ifp with the IA_PD/IA_NA lists obtained from our
 * optinfo data structure built from plugin options.
 *************************************************************************/
static int
ti_dhcp6c_save_options_ifp(ifp, optinfo, ev)
	struct dhcp6_if *ifp;
    struct dhcp6_optinfo* optinfo;
    struct dhcp6_event* ev;
{
	struct ia_conf *iac = NULL;
	size_t confsize = 0;
    struct dhcp6_listval *op;

    if(!optinfo)
        return TI_DHCP6C_ERROR;

	TAILQ_INIT(&ifp->iaconf_list);

    /* Copy the IA_NAs/IA_PDs from the options to ifp in the conf
       format. This is used later when a reply is received from the
       server in matching IAIDs and forming response message 
    */
    /* Copy IA_PDs */
    for (op = TAILQ_FIRST(&optinfo->iapd_list); op; op = TAILQ_NEXT(op, link)) {
    	switch(op->type) {
	        case DHCP6_LISTVAL_IAPD:
		        confsize = sizeof(struct iapd_conf);
        		break;
        	case DHCP6_LISTVAL_IANA:
	    	    dprintf(LOG_ERR, FNAME, "internal error");
        		break;
    	    default:
	    	    dprintf(LOG_ERR, FNAME, "internal error");
         		goto bad;
    	}

		if ((iac = malloc(confsize)) == NULL) {
			dprintf(LOG_ERR, FNAME,"memory allocation for IA %d failed",op->val_ia.iaid);
			goto bad;
		}
		memset(iac, 0, confsize);

		/* common initialization */
		iac->type = IATYPE_PD;
		iac->iaid = op->val_ia.iaid;
		TAILQ_INIT(&iac->iadata);
		TAILQ_INSERT_TAIL(&ifp->iaconf_list, iac, link);

		/* IA-type specific initialization */
		switch(op->type) {
            case DHCP6_LISTVAL_IAPD:
			    TAILQ_INIT(&((struct iapd_conf *)iac)->iapd_prefix_list);
    			TAILQ_INIT(&((struct iapd_conf *)iac)->iapd_pif_list);
	    		break;
            case DHCP6_LISTVAL_IANA:
    			dprintf(LOG_ERR, FNAME,"Invalid confrgn");
                goto bad;
            default: 
	    		dprintf(LOG_ERR, FNAME,"Invalid confrgn");
                goto bad;
		}

		/* set up parameters for the IA */
		struct iapd_conf *pdp = (struct iapd_conf *) iac;

		switch (op->type) {
            case DHCP6_LISTVAL_IAPD:
                if(dhcp6_copy_list(&pdp->iapd_prefix_list, &op->sublist)){
						dprintf(LOG_NOTICE, FNAME, "failed to configure prefix");
						goto bad;
				}
				break;
            case DHCP6_LISTVAL_IANA:
    			dprintf(LOG_ERR, FNAME,"Invalid confrgn");
                goto bad;
            default: 
		    	dprintf(LOG_ERR, FNAME, "Invalid confrgn");
                goto bad;
		}
	}

    /* Copy all the IA_NAs */
    for (op = TAILQ_FIRST(&optinfo->iana_list); op; op = TAILQ_NEXT(op, link)) {
    	switch(op->type) {
	        case DHCP6_LISTVAL_IAPD:
	    	    dprintf(LOG_ERR, FNAME, "internal error");
         		goto bad;
        	case DHCP6_LISTVAL_IANA:
	        	confsize = sizeof(struct iana_conf);
    		    break;
    	    default:
	    	    dprintf(LOG_ERR, FNAME, "internal error");
         		goto bad;
    	}

		if ((iac = malloc(confsize)) == NULL) {
			dprintf(LOG_ERR, FNAME, "memory allocation for IA %s failed", op->val_ia.iaid);
			goto bad;
		}
		memset(iac, 0, confsize);

		/* common initialization */
		iac->type = IATYPE_NA;
		iac->iaid = op->val_ia.iaid;
		TAILQ_INIT(&iac->iadata);
		TAILQ_INSERT_TAIL(&ifp->iaconf_list, iac, link);

		/* IA-type specific initialization */
		switch(op->type) {
            case DHCP6_LISTVAL_IAPD:
	    		dprintf(LOG_ERR, FNAME, "Invalid confrgn");
                goto bad;
            case DHCP6_LISTVAL_IANA:
	    		TAILQ_INIT(&((struct iana_conf *)iac)->iana_address_list);
		    	break;
            default: 
	    		dprintf(LOG_ERR, FNAME, "Invalid confrgn");
                goto bad;
		}

		/* set up parameters for the IA */
		struct iana_conf *nap = (struct iana_conf *) iac;

		switch (op->type) {
            case DHCP6_LISTVAL_IAPD:
			    dprintf(LOG_ERR, FNAME,"Invalid confrgn");
                goto bad;
            case DHCP6_LISTVAL_IANA:
                if(dhcp6_copy_list(&nap->iana_address_list, &op->sublist)){
						dprintf(LOG_NOTICE, FNAME, "failed to configure prefix");
						goto bad;
				}
				break;
            default: 
		    	dprintf(LOG_ERR, FNAME,"Invalid confrgn");
                goto bad;
		}
	}

    /* Reconfigure Authentication Protocol can be initiated
       only by the server, not the client.
    */
    if(optinfo->authproto == DHCP6_AUTHPROTO_RECONFIG) {
        goto bad;
    }

    /* Save the Auth related Params too */
    if(optinfo->authproto != DHCP6_AUTHPROTO_UNDEF)
        ifp->authproto = optinfo->authproto;

    if(optinfo->authalgorithm != DHCP6_AUTHALG_UNDEF)
        ifp->authalgorithm = optinfo->authalgorithm;

    if(optinfo->authrdm != DHCP6_AUTHRDM_UNDEF)
        ifp->authrdm = optinfo->authrdm;

    /* Init the authentication params too */
    if(ifp->authproto != DHCP6_AUTHPROTO_RECONFIG) {
	    if ((ev->authparam = new_authparam(ifp->authproto,
	        ifp->authalgorithm, ifp->authrdm)) == NULL) {
		    dprintf(LOG_WARNING, FNAME, "failed to allocate "
		        "authentication parameters");
		    dhcp6_remove_event(ev);
		    return TI_DHCP6C_INITLZN_ERROR;
	    }
    }
    else {
	    if ((ev->authparam = new_authparam(DHCP6_AUTHPROTO_UNDEF,
	        DHCP6_AUTHALG_UNDEF, DHCP6_AUTHRDM_UNDEF)) == NULL) {
		    dprintf(LOG_WARNING, FNAME, "failed to allocate "
		        "authentication parameters");
		    dhcp6_remove_event(ev);
		    return TI_DHCP6C_INITLZN_ERROR;
	    }
    }

    /* Copy any other option requests */
    if(dhcp6_copy_list(&ifp->reqopt_list, &optinfo->reqopt_list)) {
        dprintf(LOG_DEBUG, FNAME, "Error in saving optinfo->reqopt_list in ifp");
    }

    /* If Rapid commit turned on, check and set it */
    if(optinfo->rapidcommit)
        ifp->send_flags |= DHCIFF_RAPID_COMMIT;

   /* If the client id is configured, copy it */
   if(optinfo->clientID.duid_len > 0) {
        if (duidcpy(&client_duid, &optinfo->clientID)) {
	    	dprintf(LOG_ERR, FNAME, "failed to save DUID");
            goto bad;
        }
   }

	return TI_DHCP6C_SUCCESS;

  bad:
	return TI_DHCP6C_ERROR;
}

/**************************************************************************
 * FUNCTION NAME : construct_confdata
 **************************************************************************
 * DESCRIPTION   :
 * Build the ev->data with the IA_PD/IA_NA lists obtained from our
 * initial configuration read from .conf file.
 *************************************************************************/
static int
construct_confdata(ifp, ev)
	struct dhcp6_if *ifp;
	struct dhcp6_event *ev;
{
	struct ia_conf *iac;
	struct dhcp6_eventdata *evd = NULL;
	struct dhcp6_list *ial = NULL, pl;
	struct dhcp6_ia iaparam;

	TAILQ_INIT(&pl);	/* for safety */

    /* Traverse through the lsit of IA confs
       and build the ev->data 
    */
	for (iac = TAILQ_FIRST(&ifp->iaconf_list); iac;
	    iac = TAILQ_NEXT(iac, link)) {

        /* If we are sending a renew message in 
         * response to a reconfigure message, we 
         * would want to add all IAs even though 
         * they are valid.
         */
#if 0
		/* ignore IA config currently used */
		if (!TAILQ_EMPTY(&iac->iadata))
			continue;
#endif            

		evd = NULL;
		if ((evd = malloc(sizeof(*evd))) == NULL) {
			dprintf(LOG_NOTICE, FNAME,
			    "failed to create a new event data");
			goto fail;
		}
		memset(evd, 0, sizeof(evd));

		memset(&iaparam, 0, sizeof(iaparam));
		iaparam.iaid = iac->iaid;
		switch (iac->type) {
		case IATYPE_PD:
			ial = NULL;
			if ((ial = malloc(sizeof(*ial))) == NULL)
				goto fail;
			TAILQ_INIT(ial);

			TAILQ_INIT(&pl);
			if(dhcp6_copy_list(&pl,
			    &((struct iapd_conf *)iac)->iapd_prefix_list))
                    goto fail;
			if (dhcp6_add_listval(ial, DHCP6_LISTVAL_IAPD,
			    &iaparam, &pl) == NULL) {
				goto fail;
			}
			dhcp6_clear_list(&pl);

			evd->type = DHCP6_EVDATA_IAPD;
			evd->data = ial;
			evd->event = ev;
			evd->destructor = destruct_iadata;
			TAILQ_INSERT_TAIL(&ev->data_list, evd, link);
			break;
		case IATYPE_NA:
			ial = NULL;
			if ((ial = malloc(sizeof(*ial))) == NULL)
				goto fail;
			TAILQ_INIT(ial);

			TAILQ_INIT(&pl);
			if(dhcp6_copy_list(&pl,
			    &((struct iana_conf *)iac)->iana_address_list))
                    goto fail;
			if (dhcp6_add_listval(ial, DHCP6_LISTVAL_IANA,
			    &iaparam, &pl) == NULL) {
				goto fail;
			}
			dhcp6_clear_list(&pl);

			evd->type = DHCP6_EVDATA_IANA;
			evd->data = ial;
			evd->event = ev;
			evd->destructor = destruct_iadata;
			TAILQ_INSERT_TAIL(&ev->data_list, evd, link);
			break;
		default:
			dprintf(LOG_ERR, FNAME, "internal error");
			exit(1);
		}
	}

	return TI_DHCP6C_SUCCESS;

  fail:
	if (evd)
		free(evd);
	if (ial)
		free(ial);
	dhcp6_remove_event(ev);	/* XXX */
	
	return TI_DHCP6C_ERROR;
}

/**************************************************************************
 * FUNCTION NAME : construct_reqdata
 **************************************************************************
 * DESCRIPTION   :
 * Build the ev->data with the IA_PD/IA_NA based on the configuration 
 * from the server.
 *************************************************************************/
static int
construct_reqdata(ifp, optinfo, ev)
	struct dhcp6_if *ifp;
	struct dhcp6_optinfo *optinfo;
	struct dhcp6_event *ev;
{
	struct ia_conf *iac;
	struct dhcp6_eventdata *evd = NULL;
	struct dhcp6_list *ial = NULL;
	struct dhcp6_ia iaparam;

	/* discard previous event data */
	dhcp6_remove_evdata(ev);

	if (optinfo == NULL)
		return (0);

	for (iac = TAILQ_FIRST(&ifp->iaconf_list); iac;
	    iac = TAILQ_NEXT(iac, link)) {
		struct dhcp6_listval *v;

        /* If we are sending a request message in 
         * response to a advertise message, we 
         * would want to add all IAs even though 
         * they are valid and we are using them.
         */
#if 0
		/* ignore IA config currently used */
		if (!TAILQ_EMPTY(&iac->iadata))
			continue;
#endif            

		memset(&iaparam, 0, sizeof(iaparam));
		iaparam.iaid = iac->iaid;

		ial = NULL;
		evd = NULL;

		switch (iac->type) {
		case IATYPE_PD:
			if ((v = dhcp6_find_listval(&optinfo->iapd_list,
			    DHCP6_LISTVAL_IAPD, &iaparam, 0)) == NULL)
				continue;

			if ((ial = malloc(sizeof(*ial))) == NULL)
				goto fail;

			TAILQ_INIT(ial);
			if (dhcp6_add_listval(ial, DHCP6_LISTVAL_IAPD,
			    &iaparam, &v->sublist) == NULL) {
				goto fail;
			}

			if ((evd = malloc(sizeof(*evd))) == NULL)
				goto fail;
			memset(evd, 0, sizeof(*evd));
			evd->type = DHCP6_EVDATA_IAPD;
			evd->data = ial;
			evd->event = ev;
			evd->destructor = destruct_iadata;
			TAILQ_INSERT_TAIL(&ev->data_list, evd, link);
			break;
		case IATYPE_NA:
			if ((v = dhcp6_find_listval(&optinfo->iana_list,
			    DHCP6_LISTVAL_IANA, &iaparam, 0)) == NULL)
				continue;

			if ((ial = malloc(sizeof(*ial))) == NULL)
				goto fail;

			TAILQ_INIT(ial);
			if (dhcp6_add_listval(ial, DHCP6_LISTVAL_IANA,
			    &iaparam, &v->sublist) == NULL) {
				goto fail;
			}

			if ((evd = malloc(sizeof(*evd))) == NULL)
				goto fail;
			memset(evd, 0, sizeof(*evd));
			evd->type = DHCP6_EVDATA_IANA;
			evd->data = ial;
			evd->event = ev;
			evd->destructor = destruct_iadata;
			TAILQ_INSERT_TAIL(&ev->data_list, evd, link);
			break;
		default:
			dprintf(LOG_ERR, FNAME, "internal error");
			exit(1);
		}
	}

	return TI_DHCP6C_SUCCESS;

  fail:
	if (evd)
		free(evd);
	if (ial)
		free(ial);
	dhcp6_remove_event(ev);	/* XXX */
	
	return TI_DHCP6C_ERROR;
}

/**************************************************************************
 * FUNCTION NAME : destruct_iadata
 **************************************************************************
 * DESCRIPTION   :
 * Clean up the event data list.
 *************************************************************************/
static void
destruct_iadata(evd)
	struct dhcp6_eventdata *evd;
{
	struct dhcp6_list *ial;

	if (evd->type != DHCP6_EVDATA_IAPD && evd->type != DHCP6_EVDATA_IANA) {
		dprintf(LOG_ERR, FNAME, "assumption failure %d", evd->type);
		exit(1);
	}

	ial = (struct dhcp6_list *)evd->data;
	dhcp6_clear_list(ial);
	free(ial);
}

/**************************************************************************
 * FUNCTION NAME : select_server
 **************************************************************************
 * DESCRIPTION   :
 * Pick the best server, i.e, a server which is active and has the highest
 * preference value.
 *************************************************************************/
static struct dhcp6_serverinfo *
select_server(ev)
	struct dhcp6_event *ev;
{
	struct dhcp6_serverinfo *s;

	/*
	 * pick the best server according to RFC3315 Section 17.1.3.
	 * XXX: we currently just choose the one that is active and has the
	 * highest preference.
	 */
	for (s = ev->servers; s; s = s->next) {
		if (s->active) {
			dprintf(LOG_DEBUG, FNAME, "picked a server (ID: %s)",
			    duidstr(&s->optinfo.serverID));
			return (s);
		}
	}

	return (NULL);
}

/**************************************************************************
 * FUNCTION NAME : client6_signal
 **************************************************************************
 * DESCRIPTION   :
 * Registered signal handler. This function is called whenever a signal
 * is delivered to the ti_dhcpcv6 process. Based on the signal, sets the
 * sig_flags which is further processed by process_signals() function.
 *************************************************************************/
static void client6_signal(sig)
	int sig;
{

	dprintf(LOG_DEBUG, FNAME, "received a signal (%d)", sig);

	switch (sig) {
    case SIGINT:
	case SIGTERM:
		sig_flags |= SIGF_TERM;
		break;
	case SIGHUP:
		sig_flags |= SIGF_HUP;
		break;
	case SIGUSR1:
		sig_flags |= SIGF_USR1;
		break;
	case SIGUSR2:
		sig_flags |= SIGF_USR2;
		break;
    default:
       break;
	}
}

/**************************************************************************
 * FUNCTION NAME : client6_send
 **************************************************************************
 * DESCRIPTION   :
 * Registered Transmit handler for the ti_dhcpv6 client. It is responsible
 * for:
 *      - Building a packet buffer
 *      - Setting default options like CLIENTID, XID, Message Type, 
 *        ELAPSED_TIME, SERVERID.
 *      - Handing over the packet buffer to the plugin for it to add any 
 *        vendor specific options.
 *      - Sending the packet onto the network using the client socket.
 *************************************************************************/
int client6_send(ev)
	struct dhcp6_event *ev;
{
	struct dhcp6_if *ifp;
	char buf[BUFSIZ];
	char error_str[BUFSIZ];
	struct sockaddr_in6 dst;
	struct dhcp6 *dh6;
	struct dhcp6_optinfo optinfo;
	ssize_t optlen, len = 0;
	struct dhcp6_eventdata *evd;
    int err_code = TI_DHCP6C_SUCCESS;
	TI_DHCP6C_PARAMS dhcp6c_params;
    struct timeval now;

	ifp = ev->ifp;

	dh6 = (struct dhcp6 *)buf;
	memset(dh6, 0, sizeof(*dh6));

	switch(ev->state) {
	case DHCP6S_SOLICIT:
		dh6->dh6_msgtype = DH6_SOLICIT;
		break;
	case DHCP6S_REQUEST:
		dh6->dh6_msgtype = DH6_REQUEST;
		break;
	case DHCP6S_RENEW:
		dh6->dh6_msgtype = DH6_RENEW;
		break;
	case DHCP6S_REBIND:
		dh6->dh6_msgtype = DH6_REBIND;
		break;
	case DHCP6S_RELEASE:
		dh6->dh6_msgtype = DH6_RELEASE;
		break;
	case DHCP6S_INFOREQ:
		dh6->dh6_msgtype = DH6_INFORM_REQ;
		break;
	case DHCP6S_DECLINE:
		dh6->dh6_msgtype = DH6_DECLINE;
		break;
	case DHCP6S_CONFIRM:
		dh6->dh6_msgtype = DH6_CONFIRM;
		break;
	default:
		dprintf(LOG_ERR, FNAME, "unexpected state");
		exit(1);	/* XXX */
	}

	if (ev->timeouts == 0) {
		/*
		 * A client SHOULD generate a random number that cannot easily
		 * be guessed or predicted to use as the transaction ID for
		 * each new message it sends.
		 *
		 * A client MUST leave the transaction-ID unchanged in
		 * retransmissions of a message. [RFC3315 15.1]
		 */
#ifdef HAVE_ARC4RANDOM
		ev->xid = arc4random() & DH6_XIDMASK;
#else
		ev->xid = random() & DH6_XIDMASK;
#endif
		dprintf(LOG_DEBUG, FNAME, "a new XID (%x) is generated",
		    ev->xid);
	}
	dh6->dh6_xid &= ~ntohl(DH6_XIDMASK);
	dh6->dh6_xid |= htonl(ev->xid);
	len = sizeof(*dh6);

	/*
	 * construct options
	 */
	dhcp6_init_options(&optinfo);

	/* server ID */
	switch (ev->state) {
	case DHCP6S_REQUEST:
	case DHCP6S_RENEW:
	case DHCP6S_RELEASE:
    case DHCP6S_DECLINE:
		if (duidcpy(&optinfo.serverID, &ev->serverid)) {
			dprintf(LOG_ERR, FNAME, "failed to copy server ID");
			goto end;
		}
		break;
	}

    /* Accg to RFC 3315, 19.4.3 - When responding to a Reconfigure,
       the client creates and sends the Information-request message
       exactly in the same manner as outlined in section 18.1.5 
       with the exception that the client includes a Server 
       Identifier option from the Reconfigure message to which
       the client is responding 
    */
    if(ev->state == DHCP6S_INFOREQ && ifp->reconf_in_progress) {
		if (duidcpy(&optinfo.serverID, &ev->serverid)) {
			dprintf(LOG_ERR, FNAME, "failed to copy server ID");
			goto end;
		}
    }

	/* client ID */
    /* If the plugin has not already set the CLIENTID option, 
       we will set it here in core
    */
    if (duidcpy(&optinfo.clientID, &client_duid)) {
	    dprintf(LOG_ERR, FNAME, "failed to copy client ID");
   		goto end;
    }

	/* rapid commit (in Solicit only) */
	if (ev->state == DHCP6S_SOLICIT && (ifp->send_flags & DHCIFF_RAPID_COMMIT)) {
		optinfo.rapidcommit = 1;
	}

	/* elapsed time */
	if (ev->timeouts == 0) {
		get_sysUpTime(&ev->tv_start);
		optinfo.elapsed_time = 0;
	} else {
		struct timeval now, tv_diff;
		long et;

		get_sysUpTime(&now);
		tv_sub(&now, &ev->tv_start, &tv_diff);

		/*
		 * The client uses the value 0xffff to represent any elapsed
		 * time values greater than the largest time value that can be
		 * represented in the Elapsed Time option.
		 * [RFC3315 22.9.]
		 */
		if (tv_diff.tv_sec >= (MAX_ELAPSED_TIME / 100) + 1) {
			/*
			 * Perhaps we are nervous too much, but without this
			 * additional check, we would see an overflow in 248
			 * days (of no responses). 
			 */
			et = MAX_ELAPSED_TIME;
		} else {
			et = tv_diff.tv_sec * 100 + tv_diff.tv_usec / 10000;
			if (et >= MAX_ELAPSED_TIME)
				et = MAX_ELAPSED_TIME;
		}
		optinfo.elapsed_time = (int32_t)et;
	}

	/* option request options */
    /* Check if a Reconfigure is in progress, if so we are supposed to copy the OROs
       from the Reconfigure message and not build from our saved list - RFC3315, 19.4.2
       and 19.4.3
    */
    if((ev->state == DHCP6S_RENEW || ev->state == DHCP6S_INFOREQ) && ifp->reconf_in_progress) {
        if(!TAILQ_EMPTY(&ev->reconf_reqopt_list)) {
            if(dhcp6_copy_list(&optinfo.reqopt_list, &ev->reconf_reqopt_list))
                goto end;
        }
    }
	else if (ev->state != DHCP6S_RELEASE && ev->state != DHCP6S_DECLINE &&
	    dhcp6_copy_list(&optinfo.reqopt_list, &ifp->reqopt_list)) {
		dprintf(LOG_ERR, FNAME, "failed to copy requested options");
		goto end;
	} 

	/* configuration information specified as event data */
	for (evd = TAILQ_FIRST(&ev->data_list); evd;
	     evd = TAILQ_NEXT(evd, link)) {
		switch(evd->type) {
		case DHCP6_EVDATA_IAPD:
			if (dhcp6_copy_list(&optinfo.iapd_list,
			    (struct dhcp6_list *)evd->data)) {
				dprintf(LOG_NOTICE, FNAME,
				    "failed to add an IAPD");
				goto end;
			}
			break;
		case DHCP6_EVDATA_IANA:
			if (dhcp6_copy_list(&optinfo.iana_list,
			    (struct dhcp6_list *)evd->data)) {
				dprintf(LOG_NOTICE, FNAME,
				    "failed to add an IAPD");
				goto end;
			}
			break;
		default:
			dprintf(LOG_ERR, FNAME, "unexpected event data (%d)",
			    evd->type);
			exit(1);
		}
	}

	/* authentication information */
	if ((err_code = set_auth(ev, &optinfo)) != TI_DHCP6C_SUCCESS) {
		dprintf(LOG_DEBUG, FNAME,
		    "failed to set authentication option");
		goto end;
	}

	/* set options in the message */
	if ((optlen = dhcp6_set_options(dh6->dh6_msgtype,
	    (struct dhcp6opt *)(dh6 + 1),
	    (struct dhcp6opt *)(buf + sizeof(buf)), &optinfo)) < 0) {
		dprintf(LOG_DEBUG, FNAME, "failed to construct options");
		goto end;
	}
	len += optlen;

    /* Call plugin add_options. The plugin can add any OROs which it had not added before */
	//if (len > 0 && (ev->state != DHCP6S_DECLINE && ev->state != DHCP6S_RELEASE) && !ifp->reconf_in_progress)
	if (len > 0 && (ev->state != DHCP6S_DECLINE && ev->state != DHCP6S_RELEASE))
	{
        memset(&dhcp6c_params, 0, sizeof(dhcp6c_params));

		if((err_code = fill_ti_dhcp6c_params(&optinfo, &dhcp6c_params, ifp->ifname)) < 0) {
			dprintf(LOG_ERR,FNAME, "Error in copying options to TI_DHCP6C_PARAMS \n");
			return TI_DHCP6C_ERROR;
		}

		dhcp6c_params.msg_type = dh6->dh6_msgtype;
		dhcp6c_params.options = (void *)(dh6 + 1);
        dhcp6c_params.optlen = len;

       	dprintf(LOG_DEBUG, FNAME, "Going to call plugin add options");

        err_code = dhcp6c_mcb.plugin.add_options(ev->state, dh6->dh6_msgtype, &dhcp6c_params,
                (struct dhcp6opt *)(buf + len), (struct dhcp6opt *)(buf + sizeof(buf)), &len);

        dprintf(LOG_DEBUG, FNAME, "After plugin add options len: %d error: %d ", len, err_code);

        if(err_code != TI_DHCP6C_SUCCESS) {
            sprintf(error_str, "Cant send %s message since plugin rejected add_options", dhcp6msgstr(dh6->dh6_msgtype));

            dprintf(LOG_ERR, FNAME,"%s", error_str);
            dhcp6c_mcb.plugin.event_logger(ev->state, ev->state, error_str);
            return err_code; 
        }
    }

	/* calculate MAC if necessary, and put it to the message */
	if (ev->authparam != NULL) {
		switch (ev->authparam->authproto) {
		case DHCP6_AUTHPROTO_DELAYED:
			if (ev->authparam->key == NULL)
				break;

			if (dhcp6_calc_mac((char *)dh6, len,
			    optinfo.authalgorithm,
			    optinfo.delayedauth_offset + sizeof(*dh6),
			    ev->authparam->key)) {
				dprintf(LOG_WARNING, FNAME,
				    "failed to calculate MAC");
				goto end;
			}
			break;
		default:
			break;	/* do nothing */
		}
	}
    if((ev->state == DHCP6S_RENEW || ev->state == DHCP6S_INFOREQ) && ifp->reconf_in_progress && 
        !TAILQ_EMPTY(&ev->reconf_reqopt_list)) {
            sprintf(error_str, "Sending %s message triggered by Reconfigure message", dhcp6msgstr(dh6->dh6_msgtype));
            dprintf(LOG_ERR, FNAME,"%s", error_str);
            dhcp6c_mcb.plugin.event_logger(ev->state, ev->state, error_str);
    }

    /* Record the first time we are transmitting Confirm message. Used in 
      calculating the duration for retransmits
    */
    if(ev->state == DHCP6S_CONFIRM && ev->timeouts == 0) {
	    get_sysUpTime(&now);
        ev->first_tx_time = now.tv_sec;
        dprintf(LOG_DEBUG, FNAME, "now: %u first_tx: %u",now.tv_sec, ev->first_tx_time);
    }

	/*
	 * Unless otherwise specified in this document or in a document that
	 * describes how IPv6 is carried over a specific type of link (for link
	 * types that do not support multicast), a client sends DHCP messages
	 * to the All_DHCP_Relay_Agents_and_Servers.
	 * [RFC3315 Section 13.]
	 */
	dst = *sa6_allagent;
	dst.sin6_scope_id = ifp->linkid;

	if (sendto(sock, buf, len, 0, (struct sockaddr *)&dst,
	    sysdep_sa_len((struct sockaddr *)&dst)) == -1) {
		dprintf(LOG_ERR, FNAME,
		    "transmit failed: %s", strerror(errno));
		goto end;
	}

	dprintf(LOG_DEBUG, FNAME, "send %s to %s",
	    dhcp6msgstr(dh6->dh6_msgtype), addr2str((struct sockaddr *)&dst));
    return TI_DHCP6C_SUCCESS;

  end:
	dhcp6_clear_options(&optinfo);
	return TI_DHCP6C_ERROR;
}

/**************************************************************************
 * FUNCTION NAME : tv_sub
 **************************************************************************
 * DESCRIPTION   :
 * Time utlity function. Given two timeval structures, it calculates the
 * difference in time denoted by the two structures and populates the
 * result structure with the difference.
 *************************************************************************/
static void tv_sub(a, b, result)
	struct timeval *a, *b, *result;
{
	if (a->tv_sec < b->tv_sec ||
	    (a->tv_sec == b->tv_sec && a->tv_usec < b->tv_usec)) {
		result->tv_sec = 0;
		result->tv_usec = 0;

		return;
	}

	result->tv_sec = a->tv_sec - b->tv_sec;
	if (a->tv_usec < b->tv_usec) {
		result->tv_usec = a->tv_usec + 1000000 - b->tv_usec;
		result->tv_sec -= 1;
	} else
		result->tv_usec = a->tv_usec - b->tv_usec;

	return;
}

/**************************************************************************
 * FUNCTION NAME : client6_recv
 **************************************************************************
 * DESCRIPTION   :
 * Registered Receive handler for the ti_dhcpv6 client. It is responsible
 * for:
 *      - Receiving the packet from the socket and copying to a buffer.
 *      - Performing sanity check and validating the packet size and 
 *        headers against RFC requirements 
 *      - Based on the message type of the received packet and the current
 *        state of the client on that interface, sends the packet for
 *        further processing to client6_recvadvert / client6_recvreply /
 *        client6_recvreconfig.
 *************************************************************************/
static int client6_recv(int* msg_type, int* state)
{
	char rbuf[BUFSIZ], cmsgbuf[BUFSIZ];
	struct msghdr mhdr;
	struct iovec iov;
	struct sockaddr_storage from;
	struct dhcp6_if *ifp;
	struct dhcp6opt *p, *ep;
	struct dhcp6_optinfo optinfo;
	ssize_t len;
	struct dhcp6 *dh6;
	struct cmsghdr *cm;
	struct in6_pktinfo *pi = NULL;
    int error = TI_DHCP6C_SUCCESS;
    struct dhcp6_event *ev = NULL;

	memset(&iov, 0, sizeof(iov));
	memset(&mhdr, 0, sizeof(mhdr));

	iov.iov_base = (caddr_t)rbuf;
	iov.iov_len = sizeof(rbuf);
	mhdr.msg_name = (caddr_t)&from;
	mhdr.msg_namelen = sizeof(from);
	mhdr.msg_iov = &iov;
	mhdr.msg_iovlen = 1;
	mhdr.msg_control = (caddr_t)cmsgbuf;
	mhdr.msg_controllen = sizeof(cmsgbuf);
    /* Receive the DHCP message from the socket. */
	if ((len = recvmsg(sock, &mhdr, 0)) < 0) {
		dprintf(LOG_ERR, FNAME, "recvmsg: %s", strerror(errno));
		return TI_DHCP6C_SOCKET_ERROR;
	}

	/* detect receiving interface */
	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(&mhdr); cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(&mhdr, cm)) {
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_PKTINFO &&
		    cm->cmsg_len == CMSG_LEN(sizeof(struct in6_pktinfo))) {
			pi = (struct in6_pktinfo *)(CMSG_DATA(cm));
		}
	}
	if (pi == NULL) {
		dprintf(LOG_NOTICE, FNAME, "failed to get packet info");
		return TI_DHCP6C_ERROR;
	}

    /* Retrieve the interface configuration we saved for this interface index. */
	if ((ifp = find_ifconfbyid((unsigned int)pi->ipi6_ifindex)) == NULL) {
		dprintf(LOG_DEBUG, FNAME, "unexpected interface (%d)", (unsigned int)pi->ipi6_ifindex);
		return TI_DHCP6C_ERROR;
	}

    /* Check if the packet size is valid (for minimum length) */
	if ((unsigned long)len < sizeof(*dh6)) {
		dprintf(LOG_DEBUG, FNAME, "short packet (%d bytes)", len);
		return TI_DHCP6C_SHORT_PKT_RECVD;
	}

	dh6 = (struct dhcp6 *)rbuf;

	dprintf(LOG_DEBUG, FNAME, "receive %s from %s on %s",
	    dhcp6msgstr(dh6->dh6_msgtype),
	    addr2str((struct sockaddr *)&from), ifp->ifname);

    /* The message type should be sent back to client6_mainloop */
    *msg_type = dh6->dh6_msgtype;

    /* Initialize the state to be returned to client6_mainloop to
       IDLE.
    */
    *state = DHCP6C_IDLE; 

	/* Init the options struct to which we'll be shortly 
       copying the options received in the message.
    */
	dhcp6_init_options(&optinfo);
	p = (struct dhcp6opt *)(dh6 + 1);
	ep = (struct dhcp6opt *)((char *)dh6 + len);

    /* Parse the options from the message and copy them into 
       dhcp6_optinfo data structure for ease of processing 
    */
	if (dhcp6_get_options(p, ep, &optinfo) < 0) {
		dprintf(LOG_DEBUG, FNAME, "failed to parse options");
		return TI_DHCP6C_BOGUS_PKT_RECVD;
	}

	/* Store the DHCPv6 Server /DHCPv6 Relay Agent's IPv6 Address */
	memcpy((void *)&sa6_dhcpServerAddr, (void *)&from, sizeof(struct sockaddr_in6));
	dprintf(LOG_DEBUG, FNAME, "DHCPv6 Server Addr of msg %s: (%s)", dhcp6msgstr(dh6->dh6_msgtype), addr2str((struct sockaddr *)&sa6_dhcpServerAddr));
	  
	/* find the corresponding event based on the received xid */
	ev = find_event_withid(ifp, ntohl(dh6->dh6_xid) & DH6_XIDMASK);
	if (ev != NULL) 
        *state = ev->state;

    /* Based on the message type of the packet received
       call the appropriate function handler to do
       the further processing and state machine 
       transitions 
    */
	switch(dh6->dh6_msgtype) {
	case DH6_ADVERTISE:
		error = client6_recvadvert(ifp, dh6, len, &optinfo);
		break;
	case DH6_REPLY:
		error = client6_recvreply(ifp, dh6, len, &optinfo);
		break;
    case DH6_RECONFIGURE:
        if(ifp->reconf_in_progress == 0)
    		error = client6_recvreconfig(ifp, dh6, len, &optinfo);
        else {
            dprintf(LOG_DEBUG, FNAME, "Already doing a reconfig on this interface, ignore this message");
            error = TI_DHCP6C_SUCCESS;
        }
		break;
	default:
		dprintf(LOG_DEBUG, FNAME, "received an unexpected message (%s) "
		    "from %s", dhcp6msgstr(dh6->dh6_msgtype),
		    addr2str((struct sockaddr *)&from));
		break;
	}

    /* We are done processing the received message, 
       clean up the options local copy we had created
       for processing.
    */
	dhcp6_clear_options(&optinfo);
	return error;
}

/**************************************************************************
 * FUNCTION NAME : client6_recvreconfig
 **************************************************************************
 * DESCRIPTION   :
 * Receive handler for all the RECONFIGURE messages. 
 * It is responsible for:
 *      - Performing sanity check and validating the RECONFIGURE message 
 *        against RFC requirements 
 *      - Handing over the packet to registered plugin for further 
 *        validation and acting based on the result from the plugin.
 *      - Set the state machine, timeout related params accordingly.
 *************************************************************************/
static int client6_recvreconfig(ifp, dh6, len, optinfo)
	struct dhcp6_if *ifp;
	struct dhcp6 *dh6;
	ssize_t len;
	struct dhcp6_optinfo *optinfo;
{
	struct dhcp6_event *ev = NULL;
	struct dhcp6_eventdata *evd;
	struct authparam* authparam;
    int state;
    struct dhcp6_list *ial = NULL;
    struct dhcp6_listval *op = NULL;
    int error = TI_DHCP6C_SUCCESS;

	/* packet validation based on Section 15.11 of RFC3315. */
	if (optinfo->serverID.duid_len == 0) {
		dprintf(LOG_DEBUG, FNAME, "no server ID option");
		return TI_DHCP6C_INVALID_SERVERID;
	} else {
		dprintf(LOG_DEBUG, FNAME, "server ID: %s, pref=%d",
		    duidstr(&optinfo->serverID),
		    optinfo->pref);
	}
	if (optinfo->clientID.duid_len == 0) {
		dprintf(LOG_DEBUG, FNAME, "no client ID option");
		return TI_DHCP6C_INVALID_CLIENTID;
	}
	if (duidcmp(&optinfo->clientID, &client_duid)) {
		dprintf(LOG_DEBUG, FNAME, "client DUID mismatch");
		return TI_DHCP6C_INVALID_CLIENTID;
	}

    if (optinfo->reconfig_msg_type == DH6OPT_RECONF_MSG_TYPE_UNDEF) {
        dprintf(LOG_DEBUG, FNAME, "Message Type not set in Reconfigure message");
        return TI_DHCP6C_BAD_MSGTYPE_RECONF;
    }

    switch(optinfo->reconfig_msg_type) {
        case DH6_RENEW:
            state = DHCP6S_RENEW;
            break;
        case DH6_INFORM_REQ:
            state = DHCP6S_INFOREQ;
            if(!TAILQ_EMPTY(&optinfo->iapd_list) || !TAILQ_EMPTY(&optinfo->iana_list)) {
                dprintf(LOG_ERR, FNAME, "IA options present in INFORM_REQ Reconfigure message");
                return TI_DHCP6C_BAD_MSGTYPE_RECONF;
            }
            break;
        default:
            dprintf(LOG_ERR, FNAME, "Invalid msg_type option in reconfigure message");
            return TI_DHCP6C_BAD_MSGTYPE_RECONF;
    }

    /* Done with basic RFC message validation. Call the plugin handle_options */
    if(!(error = ti_handle_plugin_options(state, dh6->dh6_msgtype, (struct dhcp6opt*)(dh6 + 1), len))) {
        if(!ti_handle_dhcp6c_info(state, dh6->dh6_msgtype, TI_DHCP6C_SUCCESS, (struct dhcp6opt* )(dh6 + 1), len, optinfo, ifp->ifname)) {
            dprintf(LOG_DEBUG, FNAME, "Plugin accepted the options");
            dhcp6c_mcb.plugin.event_logger(state, state, "Got a Valid Reconfigure Message from Server");
        }
        else {
	    	dprintf(LOG_ERR,FNAME, "Plugin rejected Reconfigure message");
            return TI_DHCP6C_PLUGIN_REJECT;
    	}
    }
    else {
        dprintf(LOG_ERR, FNAME, "Plugin rejected the Config in Reconfigure message");
        return TI_DHCP6C_PLUGIN_REJECT;
    }

	/* allocate new authentication parameter with 
       algorithm as Reconfigure, Proto - HMACMD5
       and RDM as monocounter
    */
	if ((authparam = new_authparam(ifp->authproto,
	    ifp->authalgorithm, ifp->authrdm)) == NULL) {
		dprintf(LOG_WARNING, FNAME, "memory allocation failed "
		    "for authentication parameters");
		return TI_DHCP6C_MALLOC_FAILURE;
	}
	if (process_auth(authparam, dh6, len, optinfo, ifp)) {
		dprintf(LOG_DEBUG, FNAME, "failed to process authentication");
        free(authparam);
		return TI_DHCP6C_AUTH_FAILED;
	}

    ifp->reconf_in_progress = 1;

    /* Reset the Auth params to UNDEF. If using reconfigure key auth 
       protocol, the reconfigure key is used only to authenticate
       the reconfigure message, otherwise the other communication
       with the server is as normal (unauthenticated)
    */
    if(authparam && (ifp->authproto == DHCP6_AUTHPROTO_RECONFIG)) {
        memset(authparam, 0, sizeof(*authparam));
        authparam->authproto = DHCP6_AUTHPROTO_UNDEF;
        authparam->authalgorithm = DHCP6_AUTHALG_UNDEF;
        authparam->authrdm = DHCP6_AUTHRDM_UNDEF;
    }

    /* Create a renew/inform-req message */
	if ((ev = dhcp6_create_event(ifp, state)) == NULL) {
		dprintf(LOG_NOTICE, FNAME, "failed to create a new event");
		goto fail;
	}
	TAILQ_INSERT_TAIL(&ifp->event_list, ev, link);

	if ((ev->timer = dhcp6_add_timer(client6_timo, ev)) == NULL) {
		dprintf(LOG_NOTICE, FNAME,
		    "failed to create a new event timer");
		goto fail;
	}

	if (duidcpy(&ev->serverid, &optinfo->serverID)) {
		dprintf(LOG_NOTICE, FNAME, "failed to copy server ID");
		goto fail;
	}

    /* Copy the OROs from the Reconfigure message
       See the previous comment for RFC reference.
    */
    TAILQ_INIT(&ev->reconf_reqopt_list);
    if(!TAILQ_EMPTY(&optinfo->reqopt_list))
    {
        if(dhcp6_copy_list(&ev->reconf_reqopt_list, &optinfo->reqopt_list)) 
            goto fail;
    }
    /* No ORO present in Reconfigure Message. Also no IAs present. So copy
       the IAs from our saved configuration.
     */
    else if(TAILQ_EMPTY(&optinfo->iapd_list) && TAILQ_EMPTY(&optinfo->iana_list))
    {
       	if ((error = construct_confdata(ifp, ev)) != TI_DHCP6C_SUCCESS) {
        	dprintf(LOG_ERR, FNAME, "can't send message, error in construct_confdata");
            goto fail;
   		}
    }

    /* Accg to RFC 3315 - 19.4.2 - When responding to reconfigure, 
       client creates and sends the Renew message exactly in the 
       same manner as 18.1.3, with the exception that the client
       copies the ORO and any IA options from the Reconfigure
       message into the Renew message. 
       This is applicable only if the Reconfigure message sent
       by the server has ORO and IAs set.
    */
    if((state == DHCP6S_RENEW) && (!TAILQ_EMPTY(&optinfo->iapd_list) || !TAILQ_EMPTY(&optinfo->iana_list))) {
        for (op = TAILQ_FIRST(&optinfo->iapd_list); op; op = TAILQ_NEXT(op, link)) {

			ial = NULL;
			if ((ial = malloc(sizeof(*ial))) == NULL)
				goto fail;
			TAILQ_INIT(ial);

		    if (dhcp6_add_listval(ial, op->type, &op->uv, &op->sublist) == NULL)
			    goto fail;

            if ((evd = malloc(sizeof(*evd))) == NULL) {
    	    	dprintf(LOG_NOTICE, FNAME,
	    	        "failed to create a new event data");
    	    	goto fail;
    	    }
        	memset(evd, 0, sizeof(*evd));
		    evd->type = DHCP6_EVDATA_IAPD;
    		evd->data = ial;
	    	evd->event = ev;
		    evd->destructor = destruct_iadata;
    		TAILQ_INSERT_TAIL(&ev->data_list, evd, link);
       }

       for (op = TAILQ_FIRST(&optinfo->iana_list); op; op = TAILQ_NEXT(op, link)) {

			ial = NULL;
			if ((ial = malloc(sizeof(*ial))) == NULL)
				goto fail;
			TAILQ_INIT(ial);

		    if (dhcp6_add_listval(ial, op->type, &op->uv, &op->sublist) == NULL)
			    goto fail;

         	if ((evd = malloc(sizeof(*evd))) == NULL) {
	        	dprintf(LOG_NOTICE, FNAME,
		            "failed to create a new event data");
        		goto fail;
	        }
    	    memset(evd, 0, sizeof(*evd));
    		evd->type = DHCP6_EVDATA_IANA;
	    	evd->data = ial;
    		evd->event = ev;
	    	evd->destructor = destruct_iadata;
    		TAILQ_INSERT_TAIL(&ev->data_list, evd, link);
       }
    }
   
    ev->timeouts = 0;
	dhcp6_set_timeoparam(ev);
	dhcp6_reset_timer(ev);

	if ((ev->authparam = copy_authparam(authparam)) == NULL) {
		dprintf(LOG_WARNING, FNAME,
		    "failed to copy authparam");
		goto fail;
	}

    /* Send the INFORM-REQ/RENEW message out */
	client6_send(ev);

    if(authparam) {
        free(authparam);
        authparam = NULL;
    }
	return TI_DHCP6C_SUCCESS;

    fail:
        if(ial)
            dhcp6_clear_list(ial);
        if(ev)
            dhcp6_remove_event(ev);
        ifp->reconf_in_progress = 0;

        if(authparam) {
            free(authparam);
            authparam = NULL;
        }
    return TI_DHCP6C_ERROR;
}

/**************************************************************************
 * FUNCTION NAME : client6_recvadvert
 **************************************************************************
 * DESCRIPTION   :
 * Receive handler for all the ADVERTISE messages. 
 * It is responsible for:
 *      - Performing sanity check and validating the ADVERTISE message 
 *        against RFC requirements 
 *      - Handing over the packet to registered plugin for further 
 *        validation and acting based on the result from the plugin.
 *      - Set the state machine, timeout related params accordingly.
 *************************************************************************/
static int client6_recvadvert(ifp, dh6, len, optinfo)
	struct dhcp6_if *ifp;
	struct dhcp6 *dh6;
	ssize_t len;
	struct dhcp6_optinfo *optinfo;
{
	struct dhcp6_serverinfo *newserver, **sp;
	struct dhcp6_event *ev;
	struct dhcp6_eventdata *evd;
	struct authparam *authparam = NULL, authparam0;
    char status[BUFSIZ];
    int error = TI_DHCP6C_SUCCESS;
    struct dhcp6_listval *lv = NULL, *iav = NULL;
    struct dhcp6_list *ialist = NULL;
	u_int16_t stcode;
	char *stcodestr;
    int valid_ia = 0;
    int iatype;

	/* find the corresponding event based on the received xid */
	ev = find_event_withid(ifp, ntohl(dh6->dh6_xid) & DH6_XIDMASK);
	if (ev == NULL) {
		dprintf(LOG_DEBUG, FNAME, "XID mismatch");
		return TI_DHCP6C_XID_MISMATCH;
	}

	/* packet validation based on Section 15.3 of RFC3315. */
	if (optinfo->serverID.duid_len == 0) {
		dprintf(LOG_DEBUG, FNAME, "no server ID option");
		return TI_DHCP6C_INVALID_SERVERID;
	} else {
		dprintf(LOG_DEBUG, FNAME, "server ID: %s, pref=%d",
		    duidstr(&optinfo->serverID),
		    optinfo->pref);
	}
	if (optinfo->clientID.duid_len == 0) {
		dprintf(LOG_DEBUG, FNAME, "no client ID option");
		return TI_DHCP6C_INVALID_CLIENTID;
	}
	if (duidcmp(&optinfo->clientID, &client_duid)) {
		dprintf(LOG_DEBUG, FNAME, "client DUID mismatch");
		return TI_DHCP6C_INVALID_CLIENTID;
	}

	/* validate authentication */
	authparam0 = *ev->authparam;
	if (process_auth(&authparam0, dh6, len, optinfo, ifp)) {
		dprintf(LOG_DEBUG, FNAME, "failed to process authentication");
		return TI_DHCP6C_AUTH_FAILED;
	}

	/*
	 * The requesting router MUST ignore any Advertise message that
	 * includes a Status Code option containing the value NoPrefixAvail
	 * [RFC3633 Section 11.1].
	 * Likewise, the client MUST ignore any Advertise message that includes
	 * a Status Code option containing the value NoAddrsAvail. 
	 * [RFC3315 Section 17.1.3].
	 * We only apply this when we are going to request an address or
	 * a prefix.
	 */
	for (evd = TAILQ_FIRST(&ev->data_list); evd;
	    evd = TAILQ_NEXT(evd, link)) {

		switch (evd->type) {
		case DHCP6_EVDATA_IAPD:
			stcode = DH6OPT_STCODE_NOPREFIXAVAIL;
			stcodestr = "NoPrefixAvail";
            ialist = &optinfo->iapd_list;
            iatype = DHCP6_LISTVAL_IAPD;
			break;
		case DHCP6_EVDATA_IANA:
			stcode = DH6OPT_STCODE_NOADDRSAVAIL;
			stcodestr = "NoAddrsAvail";
            ialist = &optinfo->iana_list;
            iatype = DHCP6_LISTVAL_IANA;
			break;
		default:
			continue;
		}

        iav = TAILQ_FIRST((struct dhcp6_list*)evd->data);

        if(iav && (lv = dhcp6_find_listval(ialist, iatype, &iav->val_ia, 0))) {

            dprintf(LOG_DEBUG, FNAME, "calling findlistval with stcode: %d stcode %s", stcode, dhcp6_stcodestr(stcode));
		    if (dhcp6_find_listval(&lv->sublist,
		        DHCP6_LISTVAL_STCODE, &stcode, 0)) {
			    dprintf(LOG_DEBUG, FNAME,
			        "advertise contains %s status", stcodestr);

                sprintf(status, "Advertise Received but contains %s status!", stcodestr);

                /* Report the fact that the Advertise had an error to the plugin */
                dhcp6c_mcb.plugin.event_logger(ev->state, ev->state, status); 
		    }
            else
                valid_ia ++;
        }
	}

    if(valid_ia == 0) {
        dprintf(LOG_DEBUG, FNAME, "Advertise message had NoAddrAvail/NoPrefixAvail"
            " for all requested IAs, Discard the Adv message");
        return TI_DHCP6C_STATUS_NOADDRPREFIXAVAIL;
    }

	if (ev->state != DHCP6S_SOLICIT ||
	    (ifp->send_flags & DHCIFF_RAPID_COMMIT) || infreq_mode) {
		/*
		 * We expected a reply message, but do actually receive an
		 * Advertise message.  The server should be configured not to
		 * allow the Rapid Commit option.
		 * We process the message as if we expected the Advertise.
		 * [RFC3315 Section 17.1.4]
		 */
		dprintf(LOG_DEBUG, FNAME, "unexpected advertise");
		/* proceed anyway */
	}

    /* Done with basic RFC message validation. Call the plugin handle_options */
    if(!(error = ti_handle_plugin_options(ev->state, dh6->dh6_msgtype, (struct dhcp6opt*)(dh6 + 1), len))) {
        if(!ti_handle_dhcp6c_info(ev->state, dh6->dh6_msgtype, TI_DHCP6C_SUCCESS, 
                                    (struct dhcp6opt* )(dh6 + 1), len, optinfo, ev->ifp->ifname)) {
            dprintf(LOG_DEBUG, FNAME, "Plugin accepted the options");
            dhcp6c_mcb.plugin.event_logger(ev->state, DHCP6S_REQUEST, "Got a Valid Advertise from Server");
        }
        else {
	    	dprintf(LOG_ERR,FNAME, "Plugin rejected Advertise message");
            return TI_DHCP6C_PLUGIN_REJECT;
    	}
    }
    else {
        dprintf(LOG_ERR, FNAME, "Plugin rejected the Config");
        return TI_DHCP6C_PLUGIN_REJECT;
    }

	/* ignore the server if it is known */
	if (find_server(ev, &optinfo->serverID)) {
		dprintf(LOG_DEBUG, FNAME, "duplicated server (ID: %s)", duidstr(&optinfo->serverID));
		return TI_DHCP6C_SUCCESS;
	}

	/* keep the server */
	if ((newserver = malloc(sizeof(*newserver))) == NULL) {
		dprintf(LOG_WARNING, FNAME, "memory allocation failed for server");
		return TI_DHCP6C_MALLOC_FAILURE;
	}
	memset(newserver, 0, sizeof(*newserver));

	/* remember authentication parameters */
	newserver->authparam = ev->authparam;
    if(newserver->authparam) {
	    newserver->authparam->flags = authparam0.flags;
	    newserver->authparam->prevrd = authparam0.prevrd;
	    newserver->authparam->key = authparam0.key;
    }

	/* allocate new authentication parameter for the soliciting event */
	if ((authparam = new_authparam(ev->authparam->authproto,
	    ev->authparam->authalgorithm, ev->authparam->authrdm)) == NULL) {
		dprintf(LOG_WARNING, FNAME, "memory allocation failed "
		    "for authentication parameters");
		free(newserver);
		return TI_DHCP6C_MALLOC_FAILURE;
	}
	ev->authparam = authparam;

	/* copy options */
	dhcp6_init_options(&newserver->optinfo);
	if (dhcp6_copy_options(&newserver->optinfo, optinfo)) {
		dprintf(LOG_ERR, FNAME, "failed to copy options");
		if (newserver->authparam != NULL)
			free(newserver->authparam);
		free(newserver);
		return TI_DHCP6C_ERROR;
	}
	if (optinfo->pref != DH6OPT_PREF_UNDEF)
		newserver->pref = optinfo->pref;
	newserver->active = 1;

    /* The servers are stored ordered in the decreasing order of Preference option.
       Traverse through the list of known servers and find the right place to 
       insert the new server.
    */
	for (sp = &ev->servers; *sp; sp = &(*sp)->next) {
		if ((*sp)->pref != DH6OPT_PREF_MAX &&
		    (*sp)->pref < newserver->pref) {
			break;
		}
	}
	newserver->next = *sp;
	*sp = newserver;

	if (newserver->pref == DH6OPT_PREF_MAX) {
		/*
		 * If the client receives an Advertise message that includes a
		 * Preference option with a preference value of 255, the client
		 * immediately begins a client-initiated message exchange.
		 * [RFC3315 Section 17.1.2]
		 */
		ev->current_server = newserver;
		if (duidcpy(&ev->serverid,
		    &ev->current_server->optinfo.serverID)) {
			dprintf(LOG_NOTICE, FNAME, "can't copy server ID");
			return TI_DHCP6C_INVALID_SERVERID; /* XXX: better recovery? */
		}
		if ((error = construct_reqdata(ifp, &ev->current_server->optinfo, ev)) != TI_DHCP6C_SUCCESS) {
			dprintf(LOG_NOTICE, FNAME,
			    "failed to construct request data");
			return TI_DHCP6C_ERROR; /* XXX */
		}

		ev->timeouts = 0;
        dhcp6c_mcb.plugin.event_logger(ev->state, DHCP6S_REQUEST, "Got Advertise from Server with Preference 255."
                                        "Moving to Request State");
		ev->state = DHCP6S_REQUEST;

		free(ev->authparam);
		ev->authparam = newserver->authparam;
		newserver->authparam = NULL;

		client6_send(ev);

		dhcp6_set_timeoparam(ev);
		dhcp6_reset_timer(ev);
	} else if (ev->servers->next == NULL) {
		struct timeval *rest, elapsed, tv_rt, tv_irt, timo;

		/*
		 * If this is the first advertise, adjust the timer so that
		 * the client can collect other servers until IRT elapses.
		 * XXX: we did not want to do such "low level" timer
		 *      calculation here.
		 */
		rest = dhcp6_timer_rest(ev->timer);
		tv_rt.tv_sec = (ev->retrans * 1000) / 1000000;
		tv_rt.tv_usec = (ev->retrans * 1000) % 1000000;
        
        /* The timers in the kernel can only go upto a 10ms 
         * resolution. So, there's no point we configuring 
         * timers to microsecond resolution. Besides that,
         * the uptime retrieved is also rounded off to 
         * the second decimal of milliseconds. Lets 
         * configure our timers similarly, to avoid any
         * time related issues.
         */
        /* first convert to milliseconds from microseconds. */
        tv_rt.tv_usec = tv_rt.tv_usec / 1000;

        /* next, round up milliseconds to closest higher 
         * number of 2 digits. Thats how much resolution
         * our uptime clocks offer.
         */
        if(tv_rt.tv_usec % 10 >= 4)
            tv_rt.tv_usec = tv_rt.tv_usec / 10 + 1;
        else
            tv_rt.tv_usec = tv_rt.tv_usec / 10;

        /* Finally convert it back to microseconds */
        tv_rt.tv_usec *= 1000;

		tv_irt.tv_sec = (ev->init_retrans * 1000) / 1000000;
		tv_irt.tv_usec = (ev->init_retrans * 1000) % 1000000;

        /* The timers in the kernel can only go upto a 10ms 
         * resolution. So, there's no point we configuring 
         * timers to microsecond resolution. Besides that,
         * the uptime retrieved is also rounded off to 
         * the second decimal of milliseconds. Lets 
         * configure our timers similarly, to avoid any
         * time related issues.
        */
        /* first convert to milliseconds from microseconds. */
        tv_irt.tv_usec = tv_irt.tv_usec / 1000;

        /* next, round up milliseconds to closest higher 
         * number of 2 digits. Thats how much resolution
         * our uptime clocks offer.
         */
        if(tv_irt.tv_usec % 10 >= 4)
            tv_irt.tv_usec = (tv_irt.tv_usec / 10 + 1) * 10;
        else
            tv_irt.tv_usec = (int)(tv_irt.tv_usec / 10) * 10;

        /* Finally convert it back to microseconds */
        tv_irt.tv_usec *= 1000;

		timeval_sub(&tv_rt, rest, &elapsed);
		if (TIMEVAL_LEQ(elapsed, tv_irt))
			timeval_sub(&tv_irt, &elapsed, &timo);
		else
			timo.tv_sec = timo.tv_usec = 0;

		dprintf(LOG_DEBUG, FNAME, "reset timer for %s to %d.%06d",
		    ifp->ifname, (int)timo.tv_sec, (int)timo.tv_usec);
                
		dhcp6_set_timer(&timo, ev->timer);
	}

	return (0);
}

/**************************************************************************
 * FUNCTION NAME : find_server
 **************************************************************************
 * DESCRIPTION   :
 * Given a ServerDUID, this function finds the server info in the servers
 * list.
 *************************************************************************/
static struct dhcp6_serverinfo *
find_server(ev, duid)
	struct dhcp6_event *ev;
	struct duid *duid;
{
	struct dhcp6_serverinfo *s;

	for (s = ev->servers; s; s = s->next) {
		if (duidcmp(&s->optinfo.serverID, duid) == 0)
			return (s);
	}

	return (NULL);
}

/**************************************************************************
 * FUNCTION NAME : client6_recvreply
 **************************************************************************
 * DESCRIPTION   :
 * Receive handler for all the REPLY messages. 
 * It is responsible for:
 *      - Performing sanity check and validating the REPLY message 
 *        against RFC requirements 
 *      - Handing over the packet to registered plugin for further 
 *        validation and acting based on the result from the plugin.
 *      - Set the state machine, timeout related params accordingly.
 *************************************************************************/
static int
client6_recvreply(ifp, dh6, len, optinfo)
	struct dhcp6_if *ifp;
	struct dhcp6 *dh6;
	ssize_t len;
	struct dhcp6_optinfo *optinfo;
{
	struct dhcp6_listval *lv, *iav = NULL;
	struct dhcp6_event *ev;
	int state;
    int error = TI_DHCP6C_SUCCESS;
    char status[BUFSIZ];
	u_int16_t stcode;
	char *stcodestr = NULL;
	struct dhcp6_eventdata *evd;
    struct dhcp6_list *ialist = NULL;
    int ia_found = 0;
    int iatype;

	/* find the corresponding event based on the received xid */
	ev = find_event_withid(ifp, ntohl(dh6->dh6_xid) & DH6_XIDMASK);
	if (ev == NULL) {
		dprintf(LOG_DEBUG, FNAME, "XID mismatch");
		return TI_DHCP6C_XID_MISMATCH;
	}

	state = ev->state;
	if (state != DHCP6S_INFOREQ &&
	    state != DHCP6S_REQUEST &&
	    state != DHCP6S_RENEW &&
	    state != DHCP6S_REBIND &&
	    state != DHCP6S_RELEASE &&
        state != DHCP6S_DECLINE &&
        state != DHCP6S_CONFIRM &&
	    (state != DHCP6S_SOLICIT ||
	     !(ifp->send_flags & DHCIFF_RAPID_COMMIT))) {
		dprintf(LOG_DEBUG, FNAME, "unexpected reply");
		return TI_DHCP6C_ERROR;
	}

	/* A Reply message must contain a Server ID option */
	if (optinfo->serverID.duid_len == 0) {
		dprintf(LOG_DEBUG, FNAME, "no server ID option");
		return TI_DHCP6C_INVALID_SERVERID;
	}

	/*
	 * DUID in the Client ID option (which must be contained for our
	 * client implementation) must match ours.
	 */
	if (optinfo->clientID.duid_len == 0) {
		dprintf(LOG_DEBUG, FNAME, "no client ID option");
		return TI_DHCP6C_INVALID_CLIENTID;
	}
	if (duidcmp(&optinfo->clientID, &client_duid)) {
		dprintf(LOG_DEBUG, FNAME, "client DUID mismatch");
		return TI_DHCP6C_INVALID_CLIENTID;
	}

	/* validate authentication */
	if (process_auth(ev->authparam, dh6, len, optinfo, ifp)) {
		dprintf(LOG_DEBUG, FNAME, "failed to process authentication");

        /* RFC 3315 - 21.4.4.5 
         * If the Reply fails to pass the validation test, the client
         * MUST restart the DHCP configuration process by sending a 
         * SOLICIT message 
         */
        error = TI_DHCP6C_AUTH_FAILED;

        /* Report the fact that the Reply  had an Auth error to the plugin */
        dhcp6c_mcb.plugin.event_logger(ev->state, DHCP6S_INIT, "Reply did not pass Auth Validation Test, so restarting!"); 
        ti_handle_dhcp6c_info(ev->state, dh6->dh6_msgtype, error, 0, 0, 0, NULL);

        /* Restart the client so as to renegotiate IAs */
        free_resources(ifp);
        if (client6_start(ifp)) {
	        dprintf(LOG_NOTICE, FNAME, "failed to restart %s", ifp->ifname);
            ti_exit_client(1);
      	}
    	return TI_DHCP6C_SUCCESS;
	}

	/*
	 * If the client included a Rapid Commit option in the Solicit message,
	 * the client discards any Reply messages it receives that do not
	 * include a Rapid Commit option.
	 * (should we keep the server otherwise?)
	 * [RFC3315 Section 17.1.4]
	 */
	if (state == DHCP6S_SOLICIT &&
	    (ifp->send_flags & DHCIFF_RAPID_COMMIT) &&
	    !optinfo->rapidcommit) {
		dprintf(LOG_DEBUG, FNAME, "no rapid commit");

        /* Report the fact that the Reply message doesnt have Rapid Commit option */
        sprintf(status, "Sent Solicit with Rapid Commit option. Received Reply which doesnt have this option");
        dhcp6c_mcb.plugin.event_logger(ev->state, ev->state, status); 
        return TI_DHCP6C_ERROR;
	}

    if((state == DHCP6S_RENEW || state == DHCP6S_REBIND) && !TAILQ_EMPTY(&ev->data_list)) {
        ia_found = 0;

	    for (evd = TAILQ_FIRST(&ev->data_list); evd; evd = TAILQ_NEXT(evd, link)) {

		    switch (evd->type) {
		    case DHCP6_EVDATA_IAPD:
                ialist = &optinfo->iapd_list;
                iatype = DHCP6_LISTVAL_IAPD;
			    break;
		    case DHCP6_EVDATA_IANA:
                ialist = &optinfo->iana_list;
                iatype = DHCP6_LISTVAL_IANA;
			    break;
		    default:
			    continue;
		    }   

            iav = TAILQ_FIRST((struct dhcp6_list*)evd->data);

            if(iav && (lv = dhcp6_find_listval(ialist, iatype, &iav->val_ia, 0))) {
                ia_found ++;
            }
	    }

        if(ia_found <= 0) {
            dprintf(LOG_DEBUG, FNAME, "We sent a RENEW/REBIND packet, got a REPLY which doesnt have our IA");
            return TI_DHCP6C_ERROR;
        }
    }
        
	/*
	 * The client MAY choose to report any status code or message from the
	 * status code option in the Reply message.
	 * [RFC3315 Section 18.1.8]
	 */
	for (lv = TAILQ_FIRST(&optinfo->stcode_list); lv; lv = TAILQ_NEXT(lv, link)) {
		dprintf(LOG_DEBUG, FNAME, "status code: %s", dhcp6_stcodestr(lv->val_num16));
        dhcp6c_mcb.plugin.event_logger(ev->state, ev->state, dhcp6_stcodestr(lv->val_num16));

        switch(lv->val_num16) {
            case DH6OPT_STCODE_SUCCESS:
                /* Nothing to do, accept this reply */
                break;
            case DH6OPT_STCODE_UNSPECFAIL:
                /* Ignore this reply packet since there seems to be
                   some unspecified failure at the servers end 
                */
                return TI_DHCP6C_ERROR;
            case DH6OPT_STCODE_NOADDRSAVAIL:
                break;
            case DH6OPT_STCODE_NOBINDING:
                /* This error is taken care in dhcp6c_ia.c when we 
                   call update_ia 
                */
                break;
            case DH6OPT_STCODE_NOTONLINK:
	            /*
                 * If we sent a CONFIRM and got a REPLY with
                 * NotOnLink status, we reset our state m/c
                 * and go back to Solicit and renegotiate 
                 * IP addresses - RFC 18.1.8
	             */
                /* RFC 3315 - 18.1.8, When the client receives a
                 * Not on Link status from the server in response
                 * to a Request, the client can either reissue
                 * the Request without specifying any addresses or
                 * restart the DHCP discovery process
                 */
	            stcode = DH6OPT_STCODE_NOTONLINK;
                stcodestr = "NotOnLink";
	            if (ev->state == DHCP6S_CONFIRM || ev->state == DHCP6S_REQUEST) {
                    dprintf(LOG_DEBUG, FNAME, "reply contains %s status", stcodestr);
                    sprintf(status, "Reply Received but contains %s status!", stcodestr);
                    error = TI_DHCP6C_STATUS_NOTONLINK;

                    /* Report the fact that the Reply  had an error to the plugin */
                    dhcp6c_mcb.plugin.event_logger(ev->state, DHCP6S_INIT, status); 
                    ti_handle_dhcp6c_info(ev->state, dh6->dh6_msgtype, error, 0, 0, 0, NULL);

                    /* Restart the client so as to renegotiate IAs */
                    confirm_requested = 0;
                    free_resources(ifp);
                    if (client6_start(ifp)) {
	                    dprintf(LOG_NOTICE, FNAME, "failed to restart %s", ifp->ifname);
                        ti_exit_client(1);
      	            }
    	            return TI_DHCP6C_SUCCESS;
	            }
                break;
            case DH6OPT_STCODE_USEMULTICAST:
                /* Retransmit the previous message using multicast 
                */
                return TI_DHCP6C_ERROR;
            case DH6OPT_STCODE_NOPREFIXAVAIL:
                break;
            default:
                return TI_DHCP6C_ERROR;
        }
	}

    /* Done with basic RFC message validation. Call the plugin handle_options */
    if(!(error = ti_handle_plugin_options(ev->state, dh6->dh6_msgtype, (struct dhcp6opt*)(dh6 + 1), len))) {
        if(!ti_handle_dhcp6c_info(ev->state, dh6->dh6_msgtype, TI_DHCP6C_SUCCESS, (struct dhcp6opt* )(dh6 + 1), len, optinfo, ev->ifp->ifname)) {
            dprintf(LOG_DEBUG, FNAME, "Plugin accepted the options");
            if(ev->state == DHCP6S_SOLICIT)
                dhcp6c_mcb.plugin.event_logger(ev->state, DHCP6S_REQUEST, "Got a Valid Reply from Server");
            else
                dhcp6c_mcb.plugin.event_logger(ev->state, ev->state, "Got a Valid Reply from Server");
        }
        else {
	    	dprintf(LOG_ERR,FNAME, "Plugin rejected Reply message");

            /* Since the plugin rejected the Reply packet, we 
               better send Decline to the server, otherwise 
               the server would be under the impression that we 
               are using the IAs 
            */
            if(!TAILQ_EMPTY(&optinfo->iapd_list)) {
                decline_ia(IATYPE_PD, &optinfo->iapd_list, ifp, &optinfo->serverID, ev->authparam);
            }
            if(!TAILQ_EMPTY(&optinfo->iana_list)) {
                decline_ia(IATYPE_NA, &optinfo->iapd_list, ifp, &optinfo->serverID, ev->authparam);
            }
            return TI_DHCP6C_PLUGIN_REJECT;
    	}
    }
    else {
        dprintf(LOG_ERR, FNAME, "Plugin rejected the Config");
        return TI_DHCP6C_PLUGIN_REJECT;
    }

    if(ev->state == DHCP6S_CONFIRM) {
        dprintf(LOG_DEBUG, FNAME, "Confirm-Reply exchange done, we can continue as before");
        dhcp6_remove_event(ev);
        confirm_requested = 0;
        return TI_DHCP6C_SUCCESS;
    }

	if (!TAILQ_EMPTY(&optinfo->dns_list)) {
		struct dhcp6_listval *d;
		int i = 0;

		for (d = TAILQ_FIRST(&optinfo->dns_list); d;
		     d = TAILQ_NEXT(d, link), i++) {
			info_printf("nameserver[%d] %s",
			    i, in6addr2str(&d->val_addr6, 0));
		}
	}

	if (!TAILQ_EMPTY(&optinfo->dnsname_list)) {
		struct dhcp6_listval *d;
		int i = 0;

		for (d = TAILQ_FIRST(&optinfo->dnsname_list); d;
		     d = TAILQ_NEXT(d, link), i++) {
			info_printf("Domain search list[%d] %s",
			    i, d->val_vbuf.dv_buf);
		}
	}

	if (!TAILQ_EMPTY(&optinfo->ntp_list)) {
		struct dhcp6_listval *d;
		int i = 0;

		for (d = TAILQ_FIRST(&optinfo->ntp_list); d;
		     d = TAILQ_NEXT(d, link), i++) {
			info_printf("NTP server[%d] %s",
			    i, in6addr2str(&d->val_addr6, 0));
		}
	}

	if (!TAILQ_EMPTY(&optinfo->sip_list)) {
		struct dhcp6_listval *d;
		int i = 0;

		for (d = TAILQ_FIRST(&optinfo->sip_list); d;
		     d = TAILQ_NEXT(d, link), i++) {
			info_printf("SIP server address[%d] %s",
			    i, in6addr2str(&d->val_addr6, 0));
		}
	}

	if (!TAILQ_EMPTY(&optinfo->sipname_list)) {
		struct dhcp6_listval *d;
		int i = 0;

		for (d = TAILQ_FIRST(&optinfo->sipname_list); d;
		     d = TAILQ_NEXT(d, link), i++) {
			info_printf("SIP domain name[%d] %s",
			    i, d->val_vbuf.dv_buf);
		}
	}

	/*
	 * Call the configuration script, if specified, to handle various
	 * configuration parameters.
	 */
	if (ifp->scriptpath != NULL && strlen(ifp->scriptpath) != 0) {
		dprintf(LOG_DEBUG, FNAME, "executes %s", ifp->scriptpath);
		client6_script(ifp->scriptpath, optinfo);
	}

	/*
	 * Set refresh timer for configuration information specified in
	 * information-request.  If the timer value is specified by the server
	 * in an information refresh time option, use it; use the protocol
	 * default otherwise.
	 */
	if (state == DHCP6S_INFOREQ) {
		int64_t refreshtime = DHCP6_IRT_DEFAULT;

		if (optinfo->refreshtime != DH6OPT_REFRESHTIME_UNDEF)
			refreshtime = optinfo->refreshtime;

		ifp->timer = dhcp6_add_timer(client6_expire_refreshtime, ifp);
		if (ifp->timer == NULL) {
			dprintf(LOG_WARNING, FNAME,
			    "failed to add timer for refresh time");
		} else {
			struct timeval tv;

			tv.tv_sec = (long)refreshtime;
			tv.tv_usec = 0;

			if (tv.tv_sec < 0) {
				/*
				 * XXX: tv_sec can overflow for an
				 * unsigned 32bit value.
				 */
				dprintf(LOG_WARNING, FNAME,
				    "refresh time is too large: %lu",
				    (u_int32_t)refreshtime);
				tv.tv_sec = 0x7fffffff;	/* XXX */
			}

			dhcp6_set_timer(&tv, ifp->timer);
		}
	} else if (optinfo->refreshtime != DH6OPT_REFRESHTIME_UNDEF) {
		/*
		 * draft-ietf-dhc-lifetime-02 clarifies that refresh time
		 * is only used for information-request and reply exchanges.
		 */
		dprintf(LOG_DEBUG, FNAME,
		    "unexpected information refresh time option (ignored)");
	}

	/* update stateful configuration information */
	if (state != DHCP6S_RELEASE && state != DHCP6S_DECLINE) {
		update_ia(IATYPE_PD, &optinfo->iapd_list, ifp,
		    &optinfo->serverID, ev->authparam);
		update_ia(IATYPE_NA, &optinfo->iana_list, ifp,
		    &optinfo->serverID, ev->authparam);
	}

    /* Reset the Reconfigure flag so that the next reconfig message can be processed */
    if(ifp->reconf_in_progress != 0) {
        ifp->reconf_in_progress = 0;
    }

	dhcp6_remove_event(ev);

	if (state == DHCP6S_RELEASE || state == DHCP6S_DECLINE) {
		/*
		 * When the client receives a valid Reply message in response
		 * to a Release message, the client considers the Release event
		 * completed, regardless of the Status Code option(s) returned
		 * by the server.
		 * [RFC3315 Section 18.1.8]
		 */
		check_exit();
	}

	dprintf(LOG_DEBUG, FNAME, "got an expected reply, sleeping.");

    /* Lets continue running the dhcp6c even if it was 
       started in Information-Request mode.
    */
#if 0
	if (infreq_mode) {
		exit_ok = 1;
		free_resources(NULL);
		unlink(pid_file);
		check_exit();
	}
#endif

	return error;
}

/**************************************************************************
 * FUNCTION NAME : find_event_with_xid
 **************************************************************************
 * DESCRIPTION   :
 * For a given interface and a transaction id (xid), this function finds
 * the event.
 *************************************************************************/
static struct dhcp6_event *
find_event_withid(ifp, xid)
	struct dhcp6_if *ifp;
	u_int32_t xid;
{
	struct dhcp6_event *ev;

	for (ev = TAILQ_FIRST(&ifp->event_list); ev;
	     ev = TAILQ_NEXT(ev, link)) {
		if (ev->xid == xid)
			return (ev);
	}

	return (NULL);
}

/**************************************************************************
 * FUNCTION NAME : process_auth
 **************************************************************************
 * DESCRIPTION   :
 * This function validates the authentication parameters received in the
 * message from server against what it had stored.
 *************************************************************************/
static int
process_auth(authparam, dh6, len, optinfo, ifp)
	struct authparam *authparam;
	struct dhcp6 *dh6;
	ssize_t len;
	struct dhcp6_optinfo *optinfo;
    struct dhcp6_if* ifp;
{
	struct keyinfo *key = NULL;
	struct keyinfo *plugin_key = NULL;
	int authenticated = 0;

	switch (optinfo->authproto) {
	case DHCP6_AUTHPROTO_UNDEF:
		/* server did not provide authentication option */
		break;
	case DHCP6_AUTHPROTO_DELAYED:
		if ((optinfo->authflags & DHCP6OPT_AUTHFLAG_NOINFO)) {
			dprintf(LOG_DEBUG, FNAME, "server did not include "
			    "authentication information");
			break;
		}

		if (optinfo->authalgorithm != DHCP6_AUTHALG_HMACMD5) {
			dprintf(LOG_DEBUG, FNAME, "unknown authentication "
			    "algorithm (%d)", optinfo->authalgorithm);
			break;
		}

		if (optinfo->authrdm != DHCP6_AUTHRDM_MONOCOUNTER) {
			dprintf(LOG_DEBUG, FNAME,"unknown RDM (%d)",
			    optinfo->authrdm);
			break;
		}

		/*
		 * Replay protection.  If we do not know the previous RD value,
		 * we accept the message anyway (XXX).
		 */
		if ((authparam->flags & AUTHPARAM_FLAGS_NOPREVRD)) {
			dprintf(LOG_WARNING, FNAME, "previous RD value is "
			    "unknown (accept it)");
		} else {
			if (dhcp6_auth_replaycheck(optinfo->authrdm,
			    authparam->prevrd, optinfo->authrd)) {
				dprintf(LOG_DEBUG, FNAME,
				    "possible replay attack detected");
				break;
			}
		}

		/* identify the secret key */
		if ((key = authparam->key) != NULL) {
			/*
			 * If we already know a key, its identification should
			 * match that contained in the received option.
			 * (from Section 21.4.5.1 of RFC3315)
			 */
			if (optinfo->delayedauth_keyid != key->keyid ||
			    optinfo->delayedauth_realmlen != key->realmlen ||
			    memcmp(optinfo->delayedauth_realmval, key->realm,
			    key->realmlen) != 0) {
				dprintf(LOG_DEBUG, FNAME,
				    "authentication key mismatch");
				break;
			}
		} else {
			key = find_key(optinfo->delayedauth_realmval,
			    optinfo->delayedauth_realmlen,
			    optinfo->delayedauth_keyid);

            plugin_key = dhcp6c_mcb.plugin.find_key(optinfo->delayedauth_realmval,
                  optinfo->delayedauth_realmlen, optinfo->delayedauth_keyid);

            if(plugin_key != NULL)
                key = plugin_key;

			if (key == NULL) {
				dprintf(LOG_DEBUG, FNAME, "failed to find key "
				    "provided by the server (ID: %x)",
				    optinfo->delayedauth_keyid);
				break;
			} else {
				dprintf(LOG_DEBUG, FNAME, "found key for "
				    "authentication: %s", key->name);
			}
			authparam->key = key;
		}

		/* check for the key lifetime */
		if (dhcp6_validate_key(key)) {
			dprintf(LOG_DEBUG, FNAME, "key %s has expired",
			    key->name);
			break;
		}

		/* validate MAC */
		if (dhcp6_verify_mac((char *)dh6, len, 
		    optinfo->authalgorithm,
		    optinfo->delayedauth_offset + sizeof(*dh6), key) == 0) {
			dprintf(LOG_DEBUG, FNAME, "message authentication "
			    "validated");
			authenticated = 1;
		} else {
			dprintf(LOG_DEBUG, FNAME, "invalid message "
			    "authentication");
		}

		break;
	case DHCP6_AUTHPROTO_RECONFIG:
		if ((optinfo->authflags & DHCP6OPT_AUTHFLAG_NOINFO)) {
			dprintf(LOG_DEBUG, FNAME, "server did not include "
			    "authentication information");
			break;
		}

		if (optinfo->authalgorithm != DHCP6_AUTHALG_HMACMD5) {
			dprintf(LOG_DEBUG, FNAME, "unknown authentication "
			    "algorithm (%d)", optinfo->authalgorithm);
			break;
		}

		if (optinfo->authrdm != DHCP6_AUTHRDM_MONOCOUNTER) {
			dprintf(LOG_DEBUG, FNAME,"unknown RDM (%d)",
			    optinfo->authrdm);
			break;
		}

		/*
		 * Replay protection.  If we do not know the previous RD value,
		 * we accept the message anyway (XXX).
		 */
		if ((authparam->flags & AUTHPARAM_FLAGS_NOPREVRD)) {
			dprintf(LOG_WARNING, FNAME, "previous RD value is "
			    "unknown (accept it)");
		} else {
			if (dhcp6_auth_replaycheck(optinfo->authrdm,
			    authparam->prevrd, optinfo->authrd)) {
				dprintf(LOG_DEBUG, FNAME,
				    "possible replay attack detected");
				break;
			}
		}

        /* If this is a Reply message containing a Reconfigure Key,
         * then, we need to set our interface authentication 
         * algorithm, protocol, and rdm accordingly to reflect
         * RECONFIGURE KEY AUTHENTICATION protocol. Also, we need
         * to record the key for further use
         */
        if(optinfo->reconfigauth_type == DH6_AUTH_RECONF_TYPE_KEY && dh6->dh6_msgtype == DH6_REPLY) {  
            ifp->authproto = optinfo->authproto;
            ifp->authalgorithm = optinfo->authalgorithm;
            ifp->authrdm = optinfo->authrdm;

            memcpy(ifp->reconfigauth_key, optinfo->reconfigauth_val, sizeof(optinfo->reconfigauth_val));

            return (0);
        } else if(optinfo->reconfigauth_type == DH6_AUTH_RECONF_TYPE_HMACMD5 && dh6->dh6_msgtype == DH6_RECONFIGURE) {
            /* Check if we have a key recorded, if so then validate the incoming
             * message with the recorded key
             */

            if(ifp->reconfigauth_key == NULL) {
                dprintf(LOG_ERR, FNAME, "No reconfigure key found to authenticate the message");
                return -1;
            }

            if((key = (struct keyinfo *)malloc(sizeof(struct keyinfo))) == NULL)
                return -1;
            memset(key, 0, sizeof(struct keyinfo));
            if((key->secret = (char *)malloc(sizeof(char) * sizeof(ifp->reconfigauth_key))) == NULL) {
                free(key);
                key = NULL;
                return -1;
            }
            memcpy(key->secret, ifp->reconfigauth_key, sizeof(ifp->reconfigauth_key));
            key->secretlen = strlen(key->secret);
            key->expire = 0;

    		/* check for the key lifetime */
	    	if (dhcp6_validate_key(key)) {
		    	dprintf(LOG_DEBUG, FNAME, "key %s has expired",
			        key->name);
                free(key->secret);
                free(key);
	    		break;
		    }

		    /* validate MAC */
		    if (dhcp6_verify_mac((char *)dh6, len, 
		        optinfo->authalgorithm,
		        optinfo->reconfigauth_offset + sizeof(*dh6), key) == 0) {
			    dprintf(LOG_DEBUG, FNAME, "message authentication "
			        "validated");
			    authenticated = 1;
		    } else {
			    dprintf(LOG_DEBUG, FNAME, "invalid message "
			    "authentication");
		    }
            
            free(key->secret);
            free(key);
       }
       else {
            dprintf(LOG_ERR, FNAME, "Only a Reply/Reconfigure message can contain Reconfigure Key Auth Params");
            return -1;
       }
	    break;
	default:
		dprintf(LOG_DEBUG, FNAME, "server sent unsupported "
		    "authentication protocol (%d)", optinfo->authproto);
		break;
	}

	if (authenticated == 0) {
		if (authparam->authproto != DHCP6_AUTHPROTO_UNDEF) {
			dprintf(LOG_DEBUG, FNAME, "message not authenticated "
			    "while authentication required");

			/*
			 * Right now, we simply discard unauthenticated
			 * messages.
			 */
			return (-1);
		}
	} else {
		/* if authenticated, update the "previous" RD value */
		authparam->prevrd = optinfo->authrd;
		authparam->flags &= ~AUTHPARAM_FLAGS_NOPREVRD;
	}

	return (0);
}

/**************************************************************************
 * FUNCTION NAME : set_auth
 **************************************************************************
 * DESCRIPTION   :
 * This function sets the AUTH related parameters in the options to be
 * sent to the server.
 *************************************************************************/
static int
set_auth(ev, optinfo)
	struct dhcp6_event *ev;
	struct dhcp6_optinfo *optinfo;
{
	struct authparam *authparam = ev->authparam;

	if (authparam == NULL)
		return TI_DHCP6C_SUCCESS;

	optinfo->authproto = authparam->authproto;
	optinfo->authalgorithm = authparam->authalgorithm;
	optinfo->authrdm = authparam->authrdm;

	switch (authparam->authproto) {
	case DHCP6_AUTHPROTO_UNDEF: /* we simply do not need authentication */
		return TI_DHCP6C_SUCCESS;
	case DHCP6_AUTHPROTO_DELAYED:
        #if 0
		if (ev->state == DHCP6S_INFOREQ) {
			/*
			 * In the current implementation, delayed
			 * authentication for Information-request and Reply
			 * exchanges doesn't work.  Specification is also
			 * unclear on this usage.
			 */
			dprintf(LOG_WARNING, FNAME, "delayed authentication "
			    "cannot be used for Information-request yet");
			return TI_DHCP6C_ERROR;
		}
        #endif

        /*
         * In Solicit message, the client fills in the protocol,
         * algorithm, and RDM fields in the authentication option
         * with client's preferences. The client sets the replay
         * detection to zero and omits the authentication information
         * field. It sets the option length to 11. - RFC 3315, 21.4.1
         */
		if (ev->state == DHCP6S_SOLICIT) {
			optinfo->authflags |= DHCP6OPT_AUTHFLAG_NOINFO;
			return TI_DHCP6C_SUCCESS; /* no auth information is needed */
		}

		if (authparam->key == NULL) {
			dprintf(LOG_DEBUG, FNAME,
			    "no authentication key for %s",
			    dhcp6_event_statestr(ev));
			return TI_DHCP6C_ERROR;
		}

		if (dhcp6_validate_key(authparam->key)) {
			dprintf(LOG_DEBUG, FNAME, "key %s is invalid",
			    authparam->key->name);
			return TI_DHCP6C_ERROR;
		}

		if (get_rdvalue(optinfo->authrdm, &optinfo->authrd,
		    sizeof(optinfo->authrd))) {
			dprintf(LOG_ERR, FNAME, "failed to get a replay "
			    "detection value");
			return TI_DHCP6C_ERROR;
		}

		optinfo->delayedauth_keyid = authparam->key->keyid;
		optinfo->delayedauth_realmlen = authparam->key->realmlen;
		optinfo->delayedauth_realmval =
		    malloc(optinfo->delayedauth_realmlen);
		if (optinfo->delayedauth_realmval == NULL) {
			dprintf(LOG_ERR, FNAME, "failed to allocate memory "
			    "for authentication realm");
			return TI_DHCP6C_ERROR;
		}
		memcpy(optinfo->delayedauth_realmval, authparam->key->realm,
		    optinfo->delayedauth_realmlen);

		break;
	case DHCP6_AUTHPROTO_RECONFIG: 
		return TI_DHCP6C_SUCCESS;
	default:
		dprintf(LOG_ERR, FNAME, "unsupported authentication protocol "
		    "%d", authparam->authproto);
		return TI_DHCP6C_ERROR;
	}

	return TI_DHCP6C_SUCCESS;
}

/**************************************************************************
 * FUNCTION NAME : info_printf
 **************************************************************************
 * DESCRIPTION   :
 * Utility function to print debug info
 *************************************************************************/
static void
info_printf(const char *fmt, ...)
{
	va_list ap;
	char logbuf[LINE_MAX];

	va_start(ap, fmt);
	vsnprintf(logbuf, sizeof(logbuf), fmt, ap);

	dprintf(LOG_DEBUG, FNAME, "%s", logbuf);
	if (infreq_mode)
		printf("%s\n", logbuf);

	return;
}

/************************************************************************
 * FUNCTION NAME : ti_exit_client 
 ************************************************************************
 * DESCRIPTION   :
 * Exit and cleanup. 
 ************************************************************************/
static void ti_exit_client(int retval)
{
	dhcp6c_mcb.plugin.exit();
    if(sock) {
        close(sock);
        sock = -1;
    }
	exit_ok = 1;
	free_resources(NULL);
	unlink(pid_file);
	exit(retval);
}

/************************************************************************
 * FUNCTION NAME : ti_print_usage
 ************************************************************************
 * DESCRIPTION   :
 *  Prints the correct usage of the TI DHCPv6 client.
 ************************************************************************/
static void ti_print_usage(void)
{
	printf(
"Usage: ti_dhcp6c [OPTIONS]\n\n"
"  -conf, --CONF_FILE=DHCPC.CONF   Conf file to use\n"
"  -d, --DEBUG_LEVEL=1             Sets the debug level to 1(low)\n"
"  -D, --DEBUG_LEVEL=2             Sets the debug level to 2(high)\n"
"  -f, --FOREGROUND                Runs the client in foreground (default: Daemon)\n"
"  -k, --CTLKEYFILE=CTLKEYFILE     Control Key file to use\n"
"  -p, --PID_FILE=PID_FILE.PID     File to save the PID\n"
"  -r, --INFORMATION_REQUEST=1     Runs the client in INFORMATION REQUEST mode, i.e, No IP address will be requested.(default: Information Request mode turned off)\n"
"  -i, --interface=INTERFACE       Interface to use\n"
"  -c, --SEND_CONFIRM=1            The client at startup will send a CONFIRM message(default: CONFIRM is not sent, instead SOLICIT is sent)\n"
"  -v, --version                   Display version\n"
	);
    return;
}

/************************************************************************
 * FUNCTION NAME : ti_dhcp6c_option_parse
 ************************************************************************
 * DESCRIPTION   :
 * Parses the command line options and initializes the plugin if 
 * any specified. 
 ************************************************************************/
static int ti_dhcp6c_option_parse(int argc, char* argv[])
{
    int index = 0;
	struct dhcp6_if *ifp = NULL;
    DHCP6C_ERROR error = TI_DHCP6C_SUCCESS;

    /* Extract the version information */
    sscanf(
		VERSION, 
		"%hu.%hu", (unsigned short *)&dhcp6c_mcb.major_version, 
		(unsigned short *)&dhcp6c_mcb.minor_version
		);

    /* Verify if some options have been passed or not? */
    if (argc == 1)
    {
        /* No options passed cannot proceed. */
        ti_print_usage();
        return TI_DHCP6C_INITLZN_ERROR;
    }

    /* Skip the executable name and start with the arguments. */
    index = 1;

    /* Parse all the options. */
    while (index < argc)
    {
        if (strcmp(argv[index], "-conf") == 0)
        {
            ti_use_conf_file = 1;
            conffile =  argv[index + 1];
            index = index + 2;
        }
        else if (strcmp(argv[index], "-d") == 0)
        {
            debug = 1;
            index = index + 1;
        }
        else if (strcmp(argv[index], "-D") == 0)
        {
            debug = 2;
            index = index + 1;
        }
        else if (strcmp(argv[index], "-f") == 0)
        {
            foreground = 1;
            index = index + 1;
        }
        else if (strcmp(argv[index], "-k") == 0)
        {
			ctlkeyfile = argv[index + 1];
            index = index + 2;
        }
        else if (strcmp(argv[index], "-p") == 0)
        {
            pid_file = argv[index + 1];
            index = index + 2;
        }
        /* In the original implementation "-i" option was 
           used to indicate the INFORMATION REQUEST mode.
           Changed it to "-r" since TI NETDK code uses
           "-i" by standard to indicate an interface 
        */
        else if (strcmp(argv[index], "-r") == 0)
        {
			infreq_mode = 1;
            index = index + 1;
        }
        else if (strcmp(argv[index], "-h") == 0)
		{
			ti_print_usage();
            index = index + 1;
            ti_exit_client(0);
		}
        else if (strcmp(argv[index], "-c") == 0)
        {
            confirm_requested = 1;
            index = index + 1;
        }
        else if (strcmp(argv[index], "-i") == 0)
		{
            if(argv[index + 1] == NULL ) {
                ti_print_usage();
                return TI_DHCP6C_INITLZN_ERROR;
            }
            index = index + 1;
            while(argv[index] && index < argc && argv[index][0] != '-') {
                strcpy(interface, argv[index]);
                if((error = client6_init()) != TI_DHCP6C_SUCCESS) {
                    printf("failed to initialize client\n");
                    return error;
                }
                if ((ifp = ifinit(argv[index])) == NULL) {
			        printf("failed to initialize %s \n", argv[index]);
                    return TI_DHCP6C_INITLZN_ERROR;
                }
                index = index + 1;
           }
		}
        else if (strcmp(argv[index], "-v") == 0)
		{
			printf("ti_dhcpcv6, version %s\n\n", VERSION);
			ti_exit_client(0);
		}
        else if (strcmp(argv[index], "-plugin") == 0)
        {
            void* ptr_plugin;
            void (*ptr_plugin_init)(void); 

            /* Load the library. */
            ptr_plugin = dlopen(argv[index+1], RTLD_GLOBAL | RTLD_NOW);
            if (ptr_plugin == NULL)
            {
                printf("Unable to load plugin %s Error: %s.\n", argv[index+1], dlerror());
                return TI_DHCP6C_INITLZN_ERROR;
            }

            /* Initialize the plugin */
            ptr_plugin_init = dlsym (ptr_plugin, "dhcp6c_plugin_init");
            if (ptr_plugin_init == NULL)
            {
                printf("Unable to initialize plugin %s Error: %s.\n", argv[index+1], dlerror());
                return TI_DHCP6C_INITLZN_ERROR;
            }

            /* Initialize the Plugin. */
            ptr_plugin_init();

            index = index + 2;
        }
        else
        {
            /* This could be a plugin specific option. */
			DHCP6C_PLUGIN_MCB* plugin_ptr = &dhcp6c_mcb.plugin;
            int               option_processed = TI_DHCP6C_ERROR;

            /* Does this plugin expect options?  Call the option handler for the plugin! */
            option_processed = plugin_ptr->cmd_option_parser(&index, argv);

            /* Is this an invalid option for the plugin? */
            if (option_processed != TI_DHCP6C_SUCCESS)
            {
                printf("ti_dhcpcv6 Error: Plugin %s rejected the option \n", plugin_ptr->plugin_name);
				index++;
                return TI_DHCP6C_INITLZN_ERROR;
            }
		}
	}
	return TI_DHCP6C_SUCCESS;
}

/************************************************************************
 * FUNCTION NAME : ti_handle_plugin_options
 ************************************************************************
 * DESCRIPTION   :
 * Parses all core dhcp6c options and hands over the rest for plugin 
 * to handle.
 ************************************************************************/
static int ti_handle_plugin_options(int state, int msg_type, struct dhcp6opt *optionp, int len)
{
    int j = 0;
	char *optionptr = NULL;
	unsigned int option_code, option_len; 
	int found, res;
    char *bp, *cp;
	struct dhcp6opt *np, opth, *p, *ep;

    if(optionp && len > 0) {
        if((optionptr = (char *)malloc(sizeof(char) * len)) == NULL)
            return TI_DHCP6C_ERROR;
        memset(optionptr, 0, len);
        memcpy((void *)optionptr, (void *)optionp, len);
    }
    else
        return TI_DHCP6C_ERROR;

	bp = (char *)optionptr;
	p = (struct dhcp6opt *)(optionptr);
	ep = (struct dhcp6opt *)((char *)optionptr + len - 4); 
	for (; p + 1 <= ep; p = np) {
		/*
		 * get the option header.  XXX: since there is no guarantee
		 * about the header alignment, we need to make a local copy.
		 */
		memcpy(&opth, p, sizeof(opth));
		option_len = ntohs(opth.dh6opt_len);
		option_code = ntohs(opth.dh6opt_type);

		cp = (char *)(p + 1);
		np = (struct dhcp6opt *)(cp + option_len);

#if 0
		if (np > ep) {
			dprintf(LOG_INFO, FNAME, "malformed DHCP options");
			return TI_DHCP6C_BOGUS_PKT_RECVD;
		}
#endif
        dprintf(LOG_DEBUG, FNAME, "option code: %d option_len: %d", option_code, option_len);

		switch (option_code) {
		    default:
			{
				found=0;
                /* Check if the current option is in our
                   options array, i.e., we need to handle it
                */
	    		for (j = 0; options_supported[j].code; j++) {
					if (option_code == options_supported[j].code)
					{
						found=1;
						break;
					}
				}
                /* Looks like we have found an option which we are 
                   not registered to handle, i.e., its not defined in
                   the options[] array.
                */
				if (!found)
				{
						dprintf(LOG_DEBUG, FNAME,"found a plugin option\n");
						// pass the option to the plugin
						res = dhcp6c_mcb.plugin.parse_option(state, msg_type,option_code, option_len, cp);
						if (res) {
                            if(optionptr) {
                                free(optionptr);
                                optionptr = NULL;
                            }
							return res;
                        }
	            }
           }
      }
    }
    if(optionptr) {
        free(optionptr);
        optionptr = NULL;
    }
	return TI_DHCP6C_SUCCESS;
}

/************************************************************************
 * FUNCTION NAME : ti_handle_dhcp6c_info
 ************************************************************************
 * DESCRIPTION   :
 * Copies the configuration received in the REPLY/ADVERTISE message from 
 * the server to an easy to use data structure TI_DHCP6C_PARAMS which the
 * plugin can use to see if the configuration suits it and if so, set it.
 * If the plugin finds the configuration not suitable, it can return an 
 * error, and this packet is discarded by the plugin. Also this function
 * reports any errors / timeouts during the DHCP client-server message
 * process to plugin.
 ************************************************************************/
static int ti_handle_dhcp6c_info(
			int state, 
			char msg_type,DHCP6C_ERROR error, 
			struct dhcp6opt *message,
            int len,
            struct dhcp6_optinfo* optionptr,
            char* ifname
			)
{
	int ret;
	TI_DHCP6C_PARAMS* dhcp6c_params = NULL;
	char *options = NULL;

    /* If a valid configuration has been received from the server,
       allocate the Params structure and copy the configuration
       received into it.
    */
	if (optionptr && len > 0 && message)
	{
        if(ifname == NULL || !strcmp(ifname, "")) {
			dprintf(LOG_ERR,FNAME, "Interface name not valid \n");
            return TI_DHCP6C_ERROR;
        }

		dhcp6c_params = (TI_DHCP6C_PARAMS *)malloc(sizeof (TI_DHCP6C_PARAMS));

		if (!dhcp6c_params) {
			dprintf(LOG_ERR,FNAME, "Error in malloc for recvd_message \n");
			return TI_DHCP6C_MALLOC_FAILURE;
		}
		if((ret = fill_ti_dhcp6c_params(optionptr, dhcp6c_params, ifname)) < 0) {
			dprintf(LOG_ERR,FNAME, "Error in copying options to TI_DHCP6C_PARAMS \n");
            free(dhcp6c_params);
            dhcp6c_params = NULL;
			return TI_DHCP6C_ERROR;
		}

		dhcp6c_params->msg_type = msg_type;
        if((options = (char *)malloc(sizeof(char) * len)) == NULL) {
            if(dhcp6c_params)
                free(dhcp6c_params);
            return TI_DHCP6C_MALLOC_FAILURE;
        }
        memset(options, 0, len);
        memcpy((void *)options, (void *)message, len);

		dhcp6c_params->options = (char *)options;
        dhcp6c_params->optlen = len;
	}

    /* Report the configuration to the server. If an error was 
       encountered in the client-server message exchange process,
       the second parameter passed here is NULL and an 
       appropriate error number is set in the third parameter
    */
	ret=dhcp6c_mcb.plugin.report_config(state, dhcp6c_params, error);	

    if(dhcp6c_params)
        free(dhcp6c_params);

    if(options)
        free(options);

	return ret;
}


/************************************************************************
 * FUNCTION NAME : fill_ti_dhcp6c_params
 ************************************************************************
 * DESCRIPTION   :
 * Fills the params structure with the options receieved
 ************************************************************************/
static int fill_ti_dhcp6c_params(struct dhcp6_optinfo* optinfo, TI_DHCP6C_PARAMS* dhcp6c_params, char* ifname)
{
    struct dhcp6_listval* op;
    struct dhcp6_listval* subov;
    struct dhcp6_ia* ia = NULL;
    struct dhcp6_prefix* pd_prefix = NULL;
    struct dhcp6_statefuladdr* statefuladdr = NULL;
    ti_dhcp6c_ia_t* ia_t_ptr = NULL;

    if(!optinfo) {
        dprintf(LOG_ERR, FNAME, "Error, cannot access options!");
        return TI_DHCP6C_ERROR;
    }

    memset(dhcp6c_params, 0, sizeof(dhcp6c_params));

    /* Copy the interface name */
    if(ifname && strlen(ifname) > 0) {
        if((dhcp6c_params->interface = malloc(sizeof(char) * strlen(ifname))) == NULL) {
            dprintf(LOG_DEBUG, FNAME, "Error in allocating memory for the interface name");
            return TI_DHCP6C_MALLOC_FAILURE;
        }
        strcpy(dhcp6c_params->interface, ifname);
    }
    else 
        return TI_DHCP6C_ERROR;

    /* Copy the Client ID */
	if (duidcpy(&dhcp6c_params->clientID, &optinfo->clientID))
        return TI_DHCP6C_ERROR;
    /* Copy the Server ID if available */
	if ((optinfo->serverID.duid_len > 0) && 
        (duidcpy(&dhcp6c_params->serverID, &optinfo->serverID)))
        return TI_DHCP6C_ERROR;
    /* Copy the Server Preference value */
	dhcp6c_params->pref = optinfo->pref;

    dhcp6c_params->ia_cnt = 0;
    /* If any IA_PDs present in the optinfo copy them over */
    if(1) {
	    for (op = TAILQ_FIRST(&optinfo->iapd_list); op && dhcp6c_params->ia_cnt < TI_DHCP6C_MAXNUM_IA; 
             op = TAILQ_NEXT(op, link), dhcp6c_params->ia_cnt++) {
        
            ia_t_ptr = &dhcp6c_params->ia_array[dhcp6c_params->ia_cnt];
            
    	    ia_t_ptr->ia_type = DH6OPT_IA_PD;
            ia = &(ia_t_ptr->ia);
	    	memset(ia, 0, sizeof(struct dhcp6_ia));
		    ia->iaid = op->val_ia.iaid;
		    ia->t1 = op->val_ia.t1;
        	ia->t2 = op->val_ia.t2;
            dprintf(LOG_DEBUG, FNAME, "Adding a IA_PD id: %d t1: %d t2: %d cnt: %d", ia->iaid, ia->t1, ia->t2, dhcp6c_params->ia_cnt);

            ia_t_ptr->ia_prefix_addr_cnt = 0;

    	    /* then, calculate the length of and/or fill in the sub-options */
        	for (subov = TAILQ_FIRST(&op->sublist); subov && ia_t_ptr->ia_prefix_addr_cnt < TI_DHCP6C_MAXNUM_ADDR_PREFIX; 
                 subov = TAILQ_NEXT(subov, link), ia_t_ptr->ia_prefix_addr_cnt++) {
                pd_prefix = &(ia_t_ptr->ia_prefix_addr[ia_t_ptr->ia_prefix_addr_cnt].uv_prefix);
    		    memset(pd_prefix, 0, sizeof(struct dhcp6_prefix));
	    	    pd_prefix->pltime = subov->val_prefix6.pltime;
                pd_prefix->vltime = subov->val_prefix6.vltime;
		        memcpy(&pd_prefix->addr, &subov->val_prefix6.addr, sizeof(struct in6_addr));
		        pd_prefix->plen =  subov->val_prefix6.plen;
                dprintf(LOG_DEBUG, FNAME, "Adding a Prefix cnt: %d pltime: %d vltime: %d", ia_t_ptr->ia_prefix_addr_cnt, pd_prefix->pltime, pd_prefix->vltime);
    	    }
        }
    }

    /* If any IA_NAs present in the optinfo copy them over */
    if(1) {
	    for (op = TAILQ_FIRST(&optinfo->iana_list); op && dhcp6c_params->ia_cnt < TI_DHCP6C_MAXNUM_IA; 
             op = TAILQ_NEXT(op, link), dhcp6c_params->ia_cnt++) {

            ia_t_ptr = &dhcp6c_params->ia_array[dhcp6c_params->ia_cnt];

        	ia_t_ptr->ia_type = DH6OPT_IA_NA;
            ia = &ia_t_ptr->ia;
  	        memset(ia, 0, sizeof(struct dhcp6_ia));
            ia->iaid = op->val_ia.iaid;
            ia->t1 = op->val_ia.t1;
   		    ia->t2 = op->val_ia.t2;
            dprintf(LOG_DEBUG, FNAME, "Adding a IA_NA id: %d t1: %d t2: %d cnt: %d", ia->iaid, ia->t1, ia->t2, dhcp6c_params->ia_cnt);

            ia_t_ptr->ia_prefix_addr_cnt = 0;

    	    /* then, calculate the length of and/or fill in the sub-options */
        	for (subov = TAILQ_FIRST(&op->sublist); subov && ia_t_ptr->ia_prefix_addr_cnt < TI_DHCP6C_MAXNUM_ADDR_PREFIX; 
                 subov = TAILQ_NEXT(subov, link), ia_t_ptr->ia_prefix_addr_cnt++) {
                statefuladdr = &(ia_t_ptr->ia_prefix_addr[ia_t_ptr->ia_prefix_addr_cnt].uv_statefuladdr);
	            memset(statefuladdr, 0, sizeof(struct dhcp6_statefuladdr));
                statefuladdr->pltime = subov->val_statefuladdr6.pltime;
		        statefuladdr->vltime = subov->val_statefuladdr6.vltime;
	    	    memcpy(&statefuladdr->addr, &subov->val_statefuladdr6.addr, sizeof(struct in6_addr));
                dprintf(LOG_DEBUG, FNAME, "Adding an Address cnt: %d pltime: %d vltime: %d", ia_t_ptr->ia_prefix_addr_cnt, statefuladdr->pltime, statefuladdr->vltime);
            }
    	}
   }

   return TI_DHCP6C_SUCCESS;
}
