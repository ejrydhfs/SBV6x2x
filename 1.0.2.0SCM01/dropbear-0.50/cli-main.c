/*
 * Dropbear - a SSH2 server
 * SSH client implementation
 * 
 * Copyright (c) 2002,2003 Matt Johnston
 * Copyright (c) 2004 by Mihnea Stoenescu
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

#include "includes.h"
#include "dbutil.h"
#include "runopts.h"
#include "session.h"

static void cli_dropbear_exit(int exitcode, const char* format, va_list param);
static void cli_dropbear_log(int priority, const char* format, va_list param);

#if defined(DBMULTI_dbclient) || !defined(DROPBEAR_MULTI)
#if defined(DBMULTI_dbclient) && defined(DROPBEAR_MULTI)
int cli_main(int argc, char ** argv) {
#else
int main(int argc, char ** argv) {
#endif

	int sock;
	char* error = NULL;
	char* hostandport;
	int len;

	_dropbear_exit = cli_dropbear_exit;
	_dropbear_log = cli_dropbear_log;

	disallow_core();

	cli_getopts(argc, argv);

	TRACE(("user='%s' host='%s' port='%s'", cli_opts.username,
				cli_opts.remotehost, cli_opts.remoteport))

	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		dropbear_exit("signal() error");
	}

	sock = connect_remote(cli_opts.interface, cli_opts.remotehost, cli_opts.remoteport, 
			0, &error);

	if (sock < 0) {
		dropbear_exit("%s", error);
	}

	/* Set up the host:port log */
	len = strlen(cli_opts.remotehost);
	len += 10; /* 16 bit port and leeway*/
	hostandport = (char*)m_malloc(len);
	snprintf(hostandport, len, "%s:%s", 
			cli_opts.remotehost, cli_opts.remoteport);

	cli_session(sock, hostandport);

	/* not reached */
	return -1;
}
#endif /* DBMULTI stuff */

static void cli_dropbear_exit(int exitcode, const char* format, va_list param) {

	char fmtbuf[300];

	if (!sessinitdone) {
		snprintf(fmtbuf, sizeof(fmtbuf), "exited: %s",
				format);
	} else {
		snprintf(fmtbuf, sizeof(fmtbuf), 
				"connection to %s@%s:%s exited: %s", 
				cli_opts.username, cli_opts.remotehost, 
				cli_opts.remoteport, format);
	}

	/* Do the cleanup first, since then the terminal will be reset */
	cli_session_cleanup();
	common_session_cleanup();

	_dropbear_log(LOG_INFO, fmtbuf, param);

	exit(exitcode);
}

static void cli_dropbear_log(int UNUSED(priority), 
		const char* format, va_list param) {

	char printbuf[1024];

	vsnprintf(printbuf, sizeof(printbuf), format, param);

	fprintf(stderr, "%s: %s\n", cli_opts.progname, printbuf);

}
