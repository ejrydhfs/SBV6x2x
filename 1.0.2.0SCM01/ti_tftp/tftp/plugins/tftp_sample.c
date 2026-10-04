/*****************************************************************************
#**+-----------------------------------------------------------------------+**
#**|                ****                                                   |**
#**|                ****                                                   |**
#**|                ******o***                                             |**
#**|          ********_///_****                                            |**
#**|          ***** /_//_/ ****                                            |**
#**|           ** ** (__/ ****                                             |**
#**|               *********                                               |**
#**|                ****                                                   |**
#**|                ***                                                    |**
#**|                                                                       |**
#**|       Copyright (c) 1998-2007 Texas Instruments Incorporated          |**
#**|                      ALL RIGHTS RESERVED                              |**
#**|                                                                       |**
#**| Permission is hereby granted to licensees of Texas Instruments        |**
#**| Incorporated (TI) products to use this computer program for the sole  |**
#**| purpose of implementing a licensee product based on TI products.      |**
#**| No other rights to reproduce, use, or disseminate this computer       |**
#**| program, whether in part or in whole, are granted.                    |**
#**|                                                                       |**
#**| TI makes no representation or warranties with respect to the          |**
#**| performance of this computer program, and specifically disclaims      |**
#**| any responsibility for any damages, special or consequential,         |**
#**| connected with the use of this program.                               |**
#**|                                                                       |**
#**+-----------------------------------------------------------------------+**
#****************************************************************************/

#include <unistd.h>		/* exit */
#include <stdio.h>		/* printf */
#include <getopt.h>		/* getopt_long */
#include <string.h>		/* strdup */
#include <stdlib.h>		/* atoi */
#include "TI_tftp.h"
#include <syslog.h>

extern char *TI_interface;      /* -i option for binding to interface */
extern int TI_ipv6;		/* IPv6 mode */
extern int TI_reqsize;		/* requested blocksize; default 512 byte*/
extern int TI_error;            /* error codes */

extern int verbose;		/* tftp var used in options */

#define MAJOR_VERSION 1;
#define MINOR_VERSION 0;

struct Plugin myplug;

/* local prototypes */
void my_timeout_cb(void);
int my_report_config(int, char *);
int my_option_help(char *);
int my_option_parse(int, char **);
int my_plugin_exit(void);

/**************************************************************************
 * FUNCTION NAME : tftp_plugin_init
 **************************************************************************
 * DESCRIPTION   :
 *  Initialization of this plugin and self registration. function name
 *  must be tftp_plugin_init
 **************************************************************************/
void tftp_plugin_init(void) {

    
    myplug.ver.major = MAJOR_VERSION;
    myplug.ver.minor = MINOR_VERSION;

    myplug.tftp_plugin_exit = my_plugin_exit;
    myplug.tftp_option_help = my_option_help;
    myplug.tftp_option_parse = my_option_parse;
    myplug.tftp_timeout_cb = my_timeout_cb;
    myplug.tftp_report_config = my_report_config;

    tftp_register_plugin(myplug);

} /* end tftp_plugin_init */


/**************************************************************************
 * FUNCTION NAME : my_timeout_cb
 **************************************************************************
 * DESCRIPTION   :
 * callback for timeout reporting. Instantiated when maxtimeout or -t
 * val is retries are reached
 **************************************************************************/
void my_timeout_cb(void) {

    return;

} /* end TI_timeout_cb */


/**************************************************************************
 * FUNCTION NAME : my_report_config
 **************************************************************************
 * DESCRIPTION   :
 * 
 * used to set or get the current status depending on flag 
 * if you want to send a message pass a pointer to a string
 * if flag is REP_GET TI_error is returned;
 * if flag > REP_SET then TI_error is set to a value flag - REP_SET
 **************************************************************************/
int my_report_config(int flag, char *msg) {

    if (flag >= REP_SET) {
        TI_error = flag - REP_SET;  /* remap flag since we dont want
                                     * to stomp on some other value of
                                     * TI_error */
        if (TI_error == EX_DATAERR) {
            
        }
    }

    return(TI_error);

} /* end TI_report_config */


/**************************************************************************
 * FUNCTION NAME : my_option_help
 **************************************************************************
 * DESCRIPTION   :
 * hook called when option help is required from option_parse
 * 
 **************************************************************************/
int my_option_help(char *progname) {

    return(0);

} /* end my_option_help */

/**************************************************************************
 * FUNCTION NAME : my_option_parse
 **************************************************************************
 * DESCRIPTION   :
 * parse command line args; called within TI_option_parse
 * 
 **************************************************************************/
int my_option_parse(int ac, char **av) {


    return(0);

} /* end my_option_parse */

/**************************************************************************
 * FUNCTION NAME : my_plugin_exit
 **************************************************************************
 * DESCRIPTION   :
 * hook called during exit
 * 
 **************************************************************************/
int my_plugin_exit(void) {

    return(0);

} /* end my_plugin_exit */

