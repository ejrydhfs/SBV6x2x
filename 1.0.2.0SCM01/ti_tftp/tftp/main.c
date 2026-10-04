/* $USAGI: main.c,v 1.3 2001/01/12 21:36:33 sekiya Exp $ */

/*
 * Copyright (c) 1983 Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*----------------------------------------------------------------------------
// Copyright 2007, Texas Instruments Incorporated
//
// This program has been modified from its original operation by Texas Instruments
// to do the following:
//
// 1. Disabled interactive mode and exposed all configuration parameters
//    at the command line.
// 2. Add socket option for reading ICMP errors
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
//-----------------------------------------------------------------------------*/
/***********************************************************************
 Portions Copyright (C) 2008 Motorola Incorporated - http://www.motorola.com
*********************************************************************/

char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n"
"All rights reserved.\n";

/*
 * From: @(#)main.c	5.10 (Berkeley) 3/1/91
 */
char main_rcsid[] = 
"$Id: main.c,v 1.15 2000/07/22 19:06:29 dholland Exp $";

/* Many bug fixes are from Jim Guyton <guyton@rand-unix> */

/*
 * TFTP User Program -- Command Interface.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>

#include <netinet/in.h>
/* #include <netinet/ip.h> <--- unused? */
#include <arpa/inet.h>

#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <unistd.h>
#include "TI_tftp.h"
#include <net/if.h>

#include "tftpsubs.h"  /* for mysignal() */

#define	TIMEOUT		5		/* secs between rexmt's */

struct sockaddr_storage peeraddr;
socklen_t peer_len;
int f;
int trace;
int verbose;
int rexmtval = 1;
int maxtimeout = 16;
int ignoreicmp = 0;

sigjmp_buf toplevel;
void sendfile(int fd, char *name, char *modestr);
void recvfile(int fd, char *name, char *modestr);


static int connected;
static char mode[32];
static char line[200];
static int margc;
static char *margv[20];
static const char *prompt = "tftp";

static void setpeer0(char *, char *);
static char* hostcolon(char *, char **);
static void intr(int);

void makeargv(void);
void command(int top);
void quit(int argc, char *argv[]);
void help(int argc, char *argv[]);
void setverbose(int argc, char *argv[]);
void settrace(int argc, char *argv[]);
void status(int argc, char *argv[]);
void get(int argc, char *argv[]);
void put(int argc, char *argv[]);
void putusage(const char *s);
void setpeer(int argc, char *argv[]);
void modecmd(int argc, char *argv[]);
void setrexmt(int argc, char *argv[]);
void settimeout(int argc, char *argv[]);
void setbinary(int argc, char *argv[]);
void setascii(int argc, char *argv[]);
void setmode(const char *newmode);
void getusage(const char *s);
int mot_GetPortNumber(void);
int mot_StoreCmTftpPortInfo(unsigned int uiPortNum);
#define TFTP_PORT_PROC                 "/proc/net/tftp_port"
#define OK 0
#define NOK -1
#define HELPINDENT ((int) sizeof("connect"))

struct cmd {
    const char *name;
    const char *help;
    void (*handler)(int, char *[]);
};

char	vhelp[] = "toggle verbose mode";
char	thelp[] = "toggle packet tracing";
char	chelp[] = "connect to remote tftp";
char	qhelp[] = "exit tftp";
char	hhelp[] = "print help information";
char	shelp[] = "send file";
char	rhelp[] = "receive file";
char	mhelp[] = "set file transfer mode";
char	sthelp[] = "show current status";
char	xhelp[] = "set per-packet retransmission timeout";
char	ihelp[] = "set total retransmission timeout";
char    ashelp[] = "set mode to netascii";
char    bnhelp[] = "set mode to octet";

struct cmd cmdtab[] = {
    { "connect",	chelp,		setpeer },
    { "mode",       mhelp,          modecmd },
    { "put",	shelp,		put },
    { "get",	rhelp,		get },
    { "quit",	qhelp,		quit },
    { "verbose",	vhelp,		setverbose },
    { "trace",	thelp,		settrace },
    { "status",	sthelp,		status },
    { "binary",     bnhelp,         setbinary },
    { "ascii",      ashelp,         setascii },
    { "rexmt",	xhelp,		setrexmt },
    { "timeout",	ihelp,		settimeout },
    { "?",		hhelp,		help },
    { 0,0,0 }
};

static struct cmd *getcmd(const char *name);
static char *tail(char *filename);


int
main(int argc, char *argv[])
{
    #if 0        
    int top;
    #endif
    unsigned int uiPortNum = 0;
    TI_core_init();
    mot_StoreCmTftpPortInfo(uiPortNum); 

    f = -1;
    connected = 0;
    strcpy(mode, "octet");
    mysignal(SIGINT, intr);
    if(argc <= 1) {
		perror("Invalid command line arguments");
        TI_option_help(argv[0]);
    }
    else {
	    char *server[2];
	    if (sigsetjmp(toplevel, 1) != 0)
	        exit(0);
        server[1] = strdup(argv[1]); /* preserve server parameter for
                                      * setpeer since TI_option_parse
                                      * might mangle argv */
	    if (argc > 2) {
	        TI_option_parse(argc, argv);
	    }
	    setpeer(2, server);  
    }
    
    if(TI_mode[0] != NULL)
        modecmd(2, TI_mode);
    if (TI_getfile[0] != NULL)  /* automatic mode, get the file */
	    get(2, TI_getfile);
    else if(TI_putfile[0] != NULL)  /* put the file */
	    put(2, TI_putfile);
    else
    {
		perror("Invalid command line arguments");
        TI_option_help(argv[0]);
    }
    #if 0        
    else {
	    top = sigsetjmp(toplevel, 1) == 0;
	    for (;;)
	        command(top);
    }
    #endif
    /* TFTP session completed : reset tftp port value in the proc file to 0 */
    mot_StoreCmTftpPortInfo(uiPortNum);
    return(0);
}


static char hostname[100];

static void
setpeer0(char *host, char *port)
{
    struct addrinfo hints, *res0, *res;
    int error;
    int on = 1;
    int optname, proto;
    struct sockaddr_storage ss;
    char *cause = NULL;
    unsigned int uiPortNum = 0; 
    if (connected) {
	close(f);
	f = -1;
    }
    connected = 0;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_CANONNAME;
    if (!port)
	port = "tftp";
    error = getaddrinfo(host, port, &hints, &res0);
    if (error) {
	fprintf(stderr, "tftp: getaddrinfo: %s\n", gai_strerror(error));
        TIplug.tftp_report_config(REP_SET + EX_NOHOST, "getaddrinfo failed");
	return;
    }

    for (res = res0; res; res = res->ai_next) {
	if (res->ai_family != AF_INET
#ifdef INET6
	    && res->ai_family != AF_INET6
#endif
	    )
	    continue;


	f = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    if(res->ai_family == AF_INET) {
      optname = IP_RECVERR;
      proto = SOL_IP;
    }
#ifdef INET6
	else {
        optname = IPV6_RECVERR;
        proto = SOL_IPV6;
        TI_ipv6 = 1;
    }
#endif
    
	if (f < 0) {
	    cause = "socket";
	    continue;
	}
	if (TI_interface) {


        if ( setsockopt(f, SOL_SOCKET, SO_BINDTODEVICE,(char *)TI_interface,
                 strlen(TI_interface)+1) < 0) {
            perror("setsockopt: BINDTODEVICE");
            TIplug.tftp_report_config(REP_SET, "setsockopt BINDTODEVICE");
            exit(SYSERR);
        }
    }
 
	memset(&ss, 0, sizeof(ss));
	((struct sockaddr *)&ss)->sa_family = res->ai_family;
        /*get random choice of port number */
        uiPortNum = mot_GetPortNumber();
        ((struct sockaddr_in *)&ss)->sin_port = uiPortNum; 
	if (bind(f, (struct sockaddr *)&ss, res->ai_addrlen) < 0) {
	    cause = "bind";
	    close(f);
	    f = -1;
	    continue;
	}
        /* Bind is successull store the port value in proc file */
        mot_StoreCmTftpPortInfo(uiPortNum); 
    if ( -1 == setsockopt(f, proto, optname,(char *)&on, sizeof(on)) ) {
        TIplug.tftp_report_config(REP_SET, "setsockopt IP_RECVERR");
        exit(SYSERR);
    }
    break;
    } /* for */

    if (f < 0) {
	if (cause) {
	    fprintf(stderr, "tftp: %s", cause);
            TIplug.tftp_report_config(REP_GET, cause);
	    perror("");
	} else {
	    fprintf(stderr, "tftp: unsupported address family\n");
            TIplug.tftp_report_config(REP_GET, "tftp: unsupported address family\n");
        }
    }
    else {
        if (res) {
            memcpy(&peeraddr, res->ai_addr, res->ai_addrlen);
            peer_len = res->ai_addrlen;
            if (res->ai_canonname)
                strncpy(hostname, res->ai_canonname, sizeof(hostname));
            else
                strncpy(hostname, host, sizeof(hostname));
            hostname[sizeof(hostname)-1] = '\0';
            connected = 1;
        }
    }

    freeaddrinfo(res0);
}

void
setpeer(int argc, char *argv[])
{
#if 0
    size_t len;

    if (argc < 2) {
	strcpy(line, "Connect ");
	printf("(to) ");
	len = strlen(line);
	fgets(line+len, sizeof(line)-len, stdin);
	makeargv();
	argc = margc;
	argv = margv;
    }
#endif  
  
    if (argc < 2 || argc > 3) {
	    printf("usage: %s host-name [port]\n", argv[0]);
	    return;
    }
    if (argc == 2)
	    setpeer0(argv[1], NULL);
    else
	    setpeer0(argv[1], argv[2]);
}

struct	modes {
    const char *m_name;
    const char *m_mode;
} modes[] = {
    { "ascii",	"netascii" },
    { "netascii",   "netascii" },
    { "binary",     "octet" },
    { "image",      "octet" },
    { "octet",      "octet" },
/*      { "mail",       "mail" },       */
    { 0,		0 }
};

void
modecmd(int argc, char *argv[])
{
    register struct modes *p;
    const char *sep;

#if 0
    if (argc < 2) {
	    printf("Using %s mode to transfer files.\n", mode);
	    return;
    }
#endif    
    if (argc == 2) {
	    for (p = modes; p->m_name; p++)
	        if (strcmp(argv[1], p->m_name) == 0)
		        break;
	    if (p->m_name) {
	        setmode(p->m_mode);
	        return;
	    }
	    printf("%s: unknown mode\n", argv[1]);
	    /* drop through and print usage message */
    }

    printf("usage: %s [", argv[0]);
    sep = " ";
    for (p = modes; p->m_name; p++) {
	    printf("%s%s", sep, p->m_name);
	    if (*sep == ' ')
	        sep = " | ";
    }
    printf(" ]\n");
    printf("defaulting to octet mode\n");
    return;
}

void
setbinary(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    setmode("octet");
}

void
setascii(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    setmode("netascii");
}

void
setmode(const char *newmode)
{
    strcpy(mode, newmode);
    if (verbose)
	printf("mode set to %s\n", mode);
}

/*
 * Return: hostname; Retrieve filename.
 *   For checking, filename pointer specifies NULL.
 */
static char*
hostcolon(char *str, char **pfile)
{
    char *cp;

#ifdef INET6
    if (*str == '[') {
	cp = strstr(str + 1, "]:");
	if (cp) {
	    if (pfile) {
		*cp = '\0';
		*pfile = cp + 2;
	    }
	    return str + 1;
	}
    }
#endif
    cp = strchr(str + 1, ':');
    if (cp) {
	if (pfile) {
	    *cp = '\0';
	    *pfile = cp + 1;
	}
	return str;
    }
    if (pfile)
	*pfile = str;
    return NULL;
}

/*
 * Send file(s).
 */
void
put(int argc, char *argv[])
{
    int fd;
    register int n;
    char *ccp, *targ;
#if 0
    size_t len;

    if (argc < 2) {
	strcpy(line, "send ");
	printf("(file) ");
	len = strlen(line);
	fgets(line+len, sizeof(line)-len, stdin);
	makeargv();
	argc = margc;
	argv = margv;
    }
#endif    
    if (argc < 2) {
	    putusage(argv[0]);
	    return;
    }
    targ = argv[argc - 1];
    if (hostcolon(argv[argc - 1], NULL)) {
	    char *cp = NULL;
	    for (n = 1; n < argc - 1; n++)
	        if (hostcolon(argv[n], NULL)) {
		        putusage(argv[0]);
		        return;
	        }
	    cp = hostcolon(argv[argc - 1], &targ);
        if(!cp)
            return;
	    setpeer0(cp, NULL);
    }
    if (!connected) {
	    printf("No target machine specified.\n");
	    return;
    }
    if (argc < 4) {
	    ccp = argc == 2 ? tail(targ) : argv[1];
	    fd = open(ccp, O_RDONLY);
	    if (fd < 0) {
	        fprintf(stderr, "tftp: "); 
	        perror(ccp);
	        return;
	    }
	    if (verbose)
	        printf("putting %s to %s:%s [%s]\n",
		            ccp, hostname, targ, mode);
	    sendfile(fd, targ, mode);
	    return;
    }
    /* this assumes the target is a directory */
    /* on a remote unix system.  hmmmm.  */
    ccp = targ+strlen(targ);
    *ccp++ = '/';
    for (n = 1; n < argc - 1; n++) {
	strcpy(ccp, tail(argv[n]));
	fd = open(argv[n], O_RDONLY);
	if (fd < 0) {
	    fprintf(stderr, "tftp: "); perror(argv[n]);
	    continue;
	}
	if (verbose)
	    printf("putting %s to %s:%s [%s]\n",
		   argv[n], hostname, targ, mode);
	sendfile(fd, targ, mode);
    }
}

void
putusage(const char *s)
{
    printf("usage: %s file ... host:target, or\n", s);
    printf("       %s file ... target (when already connected)\n", s);
}

/*
 * Receive file(s).
 */
void
get(int argc, char *argv[])
{
    int fd;
    register int n;
    register char *cp;
    char *src;
#if 0
    size_t len;

    if (argc < 2) {
	strcpy(line, "get ");
	printf("(files) ");
	len = strlen(line);
	fgets(line+len, sizeof(line)-len, stdin);
	makeargv();
	argc = margc;
	argv = margv;
    }
#endif    
    if (argc < 2) {
	    getusage(argv[0]);
	    return;
    }
    if (!connected) {
	    for (n = 1; n < argc ; n++)
	        if (hostcolon(argv[n], NULL) == NULL) {
		        getusage(argv[0]);
		        return;
	        }
    }
    for (n = 1; n < argc ; n++) {
	    cp = hostcolon(argv[n], &src);
	    if (cp) {
	        setpeer0(cp, NULL);
	        if (!connected) {
		        if (argc < 4) {
		            fprintf(stderr,
			                "Invalid host %s\n", cp);
		            break;
		        } else
		            continue;
	        }
	    }
	    if (argc < 4) {
	        cp = argc == 3 ? argv[2] : tail(src);
	        fd = creat(cp, 0644);
	        if (fd < 0) {
		        fprintf(stderr, "tftp: "); perror(cp);
                TIplug.tftp_report_config(REP_SET + errno, "Can't create file");
		    return;
	    }
	    if (verbose)
		    printf("getting from %s:%s to %s [%s]\n",
		        hostname, src, cp, mode);

	    recvfile(fd, src, mode);

        if (TI_error != 0)  /* delete file on error */
            unlink(cp);
	    break;
	}
	cp = tail(src);         /* new .. jdg */
	fd = creat(cp, 0644);
	if (fd < 0) {
	    fprintf(stderr, "tftp: "); perror(cp);
        TIplug.tftp_report_config(REP_SET + errno, "Can't create file");
	    continue;
	}
	if (verbose)
	    printf("getting from %s:%s to %s [%s]\n",
		   hostname, src, cp, mode);

	recvfile(fd, src, mode);


    if (TI_error != 0)      /* delete file on error */
        unlink(cp);
    }
}

void
getusage(const char *s)
{
    printf("usage: %s host:file host:file ... file, or\n", s);
    printf("       %s file file ... file if connected\n", s);
}


void
setrexmt(int argc, char *argv[])
{
    int t;
#if 0
    size_t len;

    if (argc < 2) {
	strcpy(line, "Rexmt-timeout ");
	printf("(value) ");
	len = strlen(line);
	fgets(line+len, sizeof(line)-len, stdin);
	makeargv();
	argc = margc;
	argv = margv;
    }
#endif

    if (argc != 2) {
	    printf("usage: %s value\n", argv[0]);
	    return;
    }
    t = atoi(argv[1]);
    if (t < 0)
	    printf("%s: bad value\n", argv[1]);
    else
	    rexmtval = t;
}

void
settimeout(int argc, char *argv[])
{
    int t;
#if 0
    size_t len;

    if (argc < 2) {
	strcpy(line, "Maximum-timeout ");
	printf("(value) ");
	len = strlen(line);
	fgets(line+len, sizeof(line)-len, stdin);
	makeargv();
	argc = margc;
	argv = margv;
    }
#endif

    if (argc != 2) {
	    printf("usage: %s value\n", argv[0]);
	    return;
    }
    t = atoi(argv[1]);
    if (t < 0)
	    printf("%s: bad value\n", argv[1]);
    else
	    maxtimeout = t;
}

void
status(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (connected)
	printf("Connected to %s.\n", hostname);
    else
	printf("Not connected.\n");
    printf("Mode: %s Verbose: %s Tracing: %s\n", mode,
	   verbose ? "on" : "off", trace ? "on" : "off");
    printf("Rexmt-interval: %d seconds, Max-timeout: %d seconds\n",
	   rexmtval, maxtimeout);
}

static
void
intr(int ignore)
{
    (void)ignore;

    mysignal(SIGALRM, SIG_IGN);
    alarm(0);
    siglongjmp(toplevel, -1);
}

static
char *
tail(char *filename)
{
    register char *s;
	
    while (*filename) {
	s = strrchr(filename, '/');
	if (s == NULL)
	    break;
	if (s[1])
	    return (s + 1);
	*s = '\0';
    }
    return filename;
}

/*
 * Command parser.
 */
void
command(int top)
{
    register struct cmd *c;

    if (!top)
	putchar('\n');
    for (;;) {
	printf("%s> ", prompt);
	if (fgets(line, sizeof(line), stdin) == NULL) {
	    if (feof(stdin)) {
		quit(0, NULL);
	    } else {
		continue;
	    }
	}
	if (line[0] == 0)
	    continue;
	makeargv();
	if (margc<1) {
	    /* empty line */
	    continue;
	}
	c = getcmd(margv[0]);
	if (c == (struct cmd *)-1) {
	    printf("?Ambiguous command\n");
	    continue;
	}
	if (c == 0) {
	    printf("?Invalid command\n");
	    continue;
	}
	(*c->handler)(margc, margv);
    }
}

struct cmd *
getcmd(const char *name)
{
    const char *p, *q;
    struct cmd *c, *found;
    int nmatches, longest;

    longest = 0;
    nmatches = 0;
    found = 0;
    for (c = cmdtab; (p = c->name)!=NULL; c++) {
	for (q = name; *q == *p++; q++)
	    if (*q == 0)		/* exact match? */
		return (c);
	if (!*q) {			/* the name was a prefix */
	    if (q - name > longest) {
		longest = q - name;
		nmatches = 1;
		found = c;
	    } else if (q - name == longest)
		nmatches++;
	}
    }
    if (nmatches > 1)
	return ((struct cmd *)-1);
    return (found);
}

/*
 * Slice a string up into argc/argv.
 */
void
makeargv(void)
{
    register char *cp;
    register char **argp = margv;

    margc = 0;
    for (cp = line; *cp;) {
	while (isspace(*cp))
	    cp++;
	if (*cp == '\0')
	    break;
	*argp++ = cp;
	margc += 1;
	while (*cp != '\0' && !isspace(*cp))
	    cp++;
	if (*cp == '\0')
	    break;
	*cp++ = '\0';
    }
    *argp++ = 0;
}

void
quit(int ign1, char *ign2[])
{
    (void)ign1;
    (void)ign2;
    exit(0);
}

/*
 * Help command.
 */
void
help(int argc, char *argv[])
{
    register struct cmd *c;

    if (argc == 1) {
	printf("Commands may be abbreviated.  Commands are:\n\n");
	for (c = cmdtab; c->name; c++)
	    printf("%-*s\t%s\n", HELPINDENT, c->name, c->help);
	return;
    }
    while (--argc > 0) {
	register char *arg;
	arg = *++argv;
	c = getcmd(arg);
	if (c == (struct cmd *)-1)
	    printf("?Ambiguous help command %s\n", arg);
	else if (c == (struct cmd *)0)
	    printf("?Invalid help command %s\n", arg);
	else
	    printf("%s\n", c->help);
    }
}

void
settrace(int ign1, char *ign2[])
{
    (void)ign1;
    (void)ign2;
    trace = !trace;
    printf("Packet tracing %s.\n", trace ? "on" : "off");
}

void
setverbose(int ign1, char *ign2[])
{
    (void)ign1;
    (void)ign2;
    verbose = !verbose;
    printf("Verbose mode %s.\n", verbose ? "on" : "off");
}

/*****************************************************************************************/
/*! \fn int mot_GetPortNumt(void)
 *******************************************************************************************
 *  \brief returns a random number between 3000 to 3500 to be used for tftp client binding
 *  0 to 1024 port numbers are reserved by the system , referred from 5x2x code to choose the
 *  port number between 3000 to 3500
 *  \returns portnumber
 ******************************************************************************************/

int mot_GetPortNumber()
{
  	 int iPortNum = 3000;  /* holds the port value to be returned */
        
	/*Initial random number generator. */
         srand(1);
        /* choose a random number for port */
         iPortNum = 3000 + (rand() % 500);
         return iPortNum;
}

/************************************************************************** 
 *! \fn int mot_StoreCmTftpPortInfo(unsigned int uiPortNum)
 **************************************************************************
 *  \brief Store the  CM's tftp port value in /proc/net/tftp_port
 *  \param[in] Port Number
 *  \return 0 or -1.
 *  \note: setting a value of 0 through this function means TFTP client is not yet done bind. 
 *************************************************************************/
int mot_StoreCmTftpPortInfo(unsigned int uiPortNum)
{
    FILE *fp = NULL;

    if ( (fp = fopen(TFTP_PORT_PROC,"w"))==NULL )
     {
	   return NOK;
     }
    fprintf(fp,"%d ",uiPortNum); /* mast add one sapc at the end*/
    fclose(fp);
    return OK;

}

