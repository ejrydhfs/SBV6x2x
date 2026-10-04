/*
 * Dropbear - a SSH2 server
 *
 * Copyright (c) 2002,2003 Matt Johnston
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */
/*-------------------------------------------------------------------------------------
// Copyright 2006, Texas Instruments Incorporated
//
// This program has been modified from its original operation by Texas Instruments
// to do the following:
//  1. Modified the code to add support for binding the application to a
//     specified interface.
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

/***********************************************************************
 Portions Copyright (C) 2008 Motorola Incorporated - http://www.motorola.com
*********************************************************************/

#ifndef _RUNOPTS_H_
#define _RUNOPTS_H_

#include "includes.h"
#include "signkey.h"
#include "buffer.h"
#include "auth.h"
#include "tcpfwd.h"

typedef struct runopts {

#if defined(ENABLE_SVR_REMOTETCPFWD) || defined(ENABLE_CLI_LOCALTCPFWD)
	int listen_fwd_all;
#endif
	unsigned int recv_window;
	time_t keepalive_secs;

} runopts;

extern runopts opts;

int readhostkey(const char * filename, sign_key * hostkey, int *type);

#define	MAX_INTERFACE_LEN (10) 
typedef struct svr_runopts {

	char * rsakeyfile;
	char * dsskeyfile;
	char * bannerfile;

	int forkbg;
	int usingsyslog;

	/* ports is an array of the portcount listening ports */
	char *ports[DROPBEAR_MAX_PORTS];
	unsigned int portcount;
	char *addresses[DROPBEAR_MAX_PORTS];

	int inetdmode;

	/* Flags indicating whether to use ipv4 and ipv6 */
	/* not used yet
	int ipv4;
	int ipv6;
	*/

#ifdef DO_MOTD
	/* whether to print the MOTD */
	int domotd;
#endif

	int norootlogin;

	int noauthpass;
	int norootpass;

#ifdef ENABLE_SVR_REMOTETCPFWD
	int noremotetcp;
#endif
#ifdef ENABLE_SVR_LOCALTCPFWD
	int nolocaltcp;
#endif

	sign_key *hostkey;
	buffer * banner;
	char * pidfile;

    /* TI extension - the name of the interface to use to send/receive packets */
	char *interface;
	char acActiveInterfaceString[MAX_INTERFACE_LEN]; 

} svr_runopts;

extern svr_runopts svr_opts;

void svr_getopts(int argc, char ** argv);
void loadhostkeys();

typedef struct cli_runopts {

	char *progname;
	char *remotehost;
	char *remoteport;

	char *username;

	char *cmd;
	int wantpty;
	int always_accept_key;
	int no_cmd;
	int backgrounded;
#ifdef ENABLE_CLI_PUBKEY_AUTH
	struct SignKeyList *privkeys; /* Keys to use for public-key auth */
#endif
#ifdef ENABLE_CLI_REMOTETCPFWD
	struct TCPFwdList * remotefwds;
#endif
#ifdef ENABLE_CLI_LOCALTCPFWD
	struct TCPFwdList * localfwds;
#endif

    /* TI extension - the name of the interface to use to send/receive packets */
	char *interface;

} cli_runopts;

extern cli_runopts cli_opts;
void cli_getopts(int argc, char ** argv);

#endif /* _RUNOPTS_H_ */
