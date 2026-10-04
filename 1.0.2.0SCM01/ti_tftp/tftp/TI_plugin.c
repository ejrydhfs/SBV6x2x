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
/***********************************************************************
 Portions Copyright (C) 2008 Motorola Incorporated - http://www.motorola.com
*********************************************************************/

#include <unistd.h>		/* exit */
#include <stdio.h>		/* printf */
#include <getopt.h>		/* getopt_long */
#include <string.h>		/* strdup */
#include <stdlib.h>		/* atoi */
#include "TI_tftp.h"

char *TI_getfile[2];		/* noninteractive get mode */
char *TI_putfile[2];		/* noninteractive put mode */
char *TI_interface;		/* -i option for binding to interface */
char *TI_mode[2];		/* -m option for mode of transfer */
int TI_ipv6 = 0;		/* IPv6 mode */
int TI_reqsize = 512;		/* requested blocksize; default 512 byte*/
int TI_error = -1;              /* error codes */
int TI_blocksize = 0;           /* user blocksize option */

#define MAJOR_VERSION 1;        /* version of core */
#define MINOR_VERSION 0;

struct Plugin TIplug;

extern int maxtimeout;		/* tftp var used in options */
extern int rexmtval;		/* tftp var used in options */
extern int verbose;		/* tftp var used in options */
extern int trace;		/* tftp var used in options */
extern int ignoreicmp;          /* tftp var used in options */

void (*pluginit)(void);         /* prototype used in option_parse */


/**************************************************************************
 * FUNCTION NAME : TI_register_plugin
 **************************************************************************
 * DESCRIPTION   :
 *  Initialization
 **************************************************************************/
int tftp_register_plugin(struct Plugin pi) {

    if (TIplug.ver.major != pi.ver.major) {
        return(-1);
    }

    if (pi.tftp_option_parse)
        TIplug.tftp_option_parse = pi.tftp_option_parse;

    if (pi.tftp_timeout_cb)
        TIplug.tftp_timeout_cb = pi.tftp_timeout_cb;

    if (pi.tftp_report_config)
        TIplug.tftp_report_config = pi.tftp_report_config;

    if (pi.tftp_plugin_exit)
        TIplug.tftp_plugin_exit = pi.tftp_plugin_exit;

    if (pi.tftp_option_help)
        TIplug.tftp_option_help = pi.tftp_option_help;

    return(0);

} /* TI_register_plugin */


/**************************************************************************
 * FUNCTION NAME : TI_option_help
 **************************************************************************
 * DESCRIPTION   :
 *  Print usage to user
 **************************************************************************/
void TI_option_help(char *progname) {
    printf("Usage: %s server [-v] [-f file_to_get] [-s file_to_put] [-t timeout] [-i interface] [-w wrokdir] [-p plugin] "
            "[-b blocksize ] [-m transfer_mode] [-r rexmt_timeout] [-d] [-c]\n", progname);
    printf("   -v                 : verbose\n"
           "   -f filename        : filename for transfer (get)\n"
           "   -s filename        : filename for sending (put)\n"
           "   -t timeout         : retry attempts; default 16\n"
           "   -i interface       : interface name\n"
           "   -w workdir         : working directory\n"
           "   -p plugin	      : plugin shared object; default /lib/tftp_sample_plugin.so\n"
           "   -b blocksize       : optional size for blocksize in read request, max 1448\n"
           "   -m mode            : data transfer mode(octet|ascii)\n"
           "   -r rexmtval        : per packet retransmission timeout\n"
           "   -c                 : ignore icmp receive\n"
           "   -d                 : turn debug trace on/off\n");

    if (TIplug.tftp_option_help)
        TIplug.tftp_option_help(progname);

    exit(0);

} /* TI_option_help */

/**************************************************************************
 * FUNCTION NAME : TI_option_parse
 **************************************************************************
 * DESCRIPTION   :
 * parse command line args using getopt_long; 
 * assumes first argument is host and any flags follow
 **************************************************************************/
int TI_option_parse(int ac, char **av) {


    void *dlhandle;
    char *pluginfile = NULL;


    while (1) {
        int option_index = 1;	/* skip first argument, should be host */
	int c;
        static const struct option long_options[] = {
            {"file", 1, 0, 'f'},
            {"put", 1, 0, 's'},
            {"timeout", 1, 0, 't'},
            {"interface", 1, 0, 'i'},
            {"verbose", 1, 0, 'v'},
            {"plugin", 1, 0, 'p'},
            {"blocksize", 1, 0, 'b'},
            {"mode", 1, 0, 'm'},
            {"rexmt", 1, 0, 'r'},
            {"workdir", 1, 0, 'w'},
            {"debug", 1, 0, 'd'},
            {"icmp", 1, 0, 'c'},
            {0, 0, 0, 0}
        };

        c = getopt_long (ac, av, "?vdcf:t:i:p:b:s:r:m:w:",
                         long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case '?':
            TI_option_help(av[0]);
            break;
        case 'f':
	        TI_getfile[0] = strdup("get");
            if ((TI_getfile[1] = strdup(optarg)) == NULL)
		        perror("TI_option_parse: strdup f");
            break;
        case 'w':
            chdir(optarg);
            break;
        case 's':
	        TI_putfile[0] = strdup("put");
            if ((TI_putfile[1] = strdup(optarg)) == NULL)
		        perror("TI_option_parse: strdup s");
            break;
	    case 't':
	        maxtimeout = atoi(optarg);
	        break;
	    case 'i':
            if ((TI_interface = strdup(optarg)) == NULL)
		        perror("TI_option_parse: strdup i");
	        break;
	    case 'm':
	        TI_mode[0] = strdup("mode");
            if ((TI_mode[1] = strdup(optarg)) == NULL)
		        perror("TI_option_parse: strdup m");
	        break;
	    case 'r':
	        rexmtval = atoi(optarg);
	        break;
        case 'v':
            verbose = 1;
            break;
        case 'd':
            trace = 1;
            break;
        case 'c':
            ignoreicmp = 1;
            break;
        case 'p':
	        if (pluginfile)
	            free(pluginfile);
            pluginfile = strdup(optarg);
            break;
        case 'b':
            if ((TI_blocksize = atoi(optarg)) > MAXBLKSZV6) {
                                /* can't be larger than largest
                                 * allowable size, MAXBLKSZV6 1448 */
                fprintf(stderr,"Bad option -b: max blocksize %d\n", MAXBLKSZV6);
                TIplug.tftp_report_config(REP_SET + 253, "Invalid block option");
                exit(0);
            }
            break;
        default:
            printf("TI_option_parse: possibly unhandled option: %c\n", c);
        }
    }

    if (pluginfile) {
        if (!(dlhandle = dlopen(pluginfile, RTLD_GLOBAL | RTLD_LAZY))) {
            printf("dlopen: %s\n", dlerror());
            exit(SYSERR);
        }
    
    
        if ((pluginit = dlsym(dlhandle, "tftp_plugin_init")) == NULL) {
            printf("dlopen: %s\n", dlerror());
            exit(SYSERR);
        }       

        pluginit();
    }

    if (TIplug.tftp_option_parse)
        TIplug.tftp_option_parse(ac , av);

    if (optind < ac)
        return(optind);

    return(0);

} /* end TI_option_parse */

/**************************************************************************
 * FUNCTION NAME : TI_plugin_exit
 **************************************************************************
 * DESCRIPTION   :
 *  exit handling
 **************************************************************************/
int TI_plugin_exit(void) {

    if (TIplug.tftp_plugin_exit)
        TIplug.tftp_plugin_exit();
    
    _exit(TI_error);

} /* TI_plugin_exit */

/**************************************************************************
 * FUNCTION NAME : TI_report_config
 **************************************************************************
 * DESCRIPTION   :
 *  error/state handling
 **************************************************************************/
int TI_report_config(int flag, char *msg) {
    
    
    if (flag >= REP_SET) {
        TI_error = flag - REP_SET;  /* remap flag since we dont want
                                     * to stomp on some other value of
                                     * TI_error */
        if (TI_error == EX_DATAERR) {
            
        }
    }
    return(TI_error);

} /* TI_report_config */

/**************************************************************************
 * FUNCTION NAME : TI_core_init
 **************************************************************************
 * DESCRIPTION   :
 *  initialize core functionality
 **************************************************************************/
void TI_core_init(void) {


    memset((void *)&TIplug, 0, sizeof(TIplug));

    TIplug.ver.major = MAJOR_VERSION;
    TIplug.ver.minor = MINOR_VERSION;

    TIplug.tftp_report_config = TI_report_config;

    atexit(TI_plugin_exit);

} /* end TI_core_init */
