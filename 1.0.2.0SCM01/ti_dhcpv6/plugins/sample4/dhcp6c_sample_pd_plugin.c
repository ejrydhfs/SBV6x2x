/**************************************************************************
 * FILE PURPOSE	:  	dhcp6c sample pd plugin
 **************************************************************************
 * FILE NAME	:   dhcp6c_sample_pd_plugin.c
 *
 * DESCRIPTION	:
 * This plugin adds options required to request IPv6 prefixes from the 
 * DHCP server.
 *
 *	CALL-INs:
 *
 *	CALL-OUTs:
 *
 *	User-Configurable Items:
 *
 *	(C) Copyright 2007, Texas Instruments, Inc.
**+----------------------------------------------------------------------------------+**
**|                            ****                                                  |**
**|                            ****                                                  |**
**|                            ******o***                                            |**
**|                      ********_///_****                                           |**
**|                      ***** /_//_/ ****                                           |**
**|                       ** ** (__/ ****                                            |**
**|                           *********                                              |**
**|                            ****                                                  |**
**|                            ***                                                   |**
**|                                                                                  |**
**|         Copyright (c) 1998-2007 Texas Instruments Incorporated                   |**
**|                        ALL RIGHTS RESERVED                                       |**
**|                                                                                  |**
**| Permission is hereby granted to licensees of Texas Instruments                   |**
**| Incorporated (TI) products to use this computer program for the sole             |**
**| purpose of implementing a licensee product based on TI products.                 |**
**| No other rights to reproduce, use, or disseminate this computer                  |**
**| program, whether in part or in whole, are granted.                               |**
**|                                                                                  |**
**| TI makes no representation or warranties with respect to the                     |**
**| performance of this computer program, and specifically disclaims                 |**
**| any responsibility for any damages, special or consequential,                    |**
**| connected with the use of this program.                                          |**
**|                                                                                  |**
***************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <assert.h>
#include <sys/select.h>
#include <sys/param.h>
#include <errno.h>
#include <alloca.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>
#include "dhcp6c_plugin.h"


static DHCP6C_ERROR cmd_option_parser(int *argc, char *argv[]);
static void event_logger(dhcp6c_client_state curr_state,dhcp6c_client_state new_state, char *event );
static DHCP6C_ERROR report_config(dhcp6c_client_state state,TI_DHCP6C_PARAMS* dhcp6c_params, DHCP6C_ERROR error);
static DHCP6C_ERROR init_options(dhcp6c_client_state state,int msg_type,struct dhcp6opt* startbuf,struct dhcp6opt* endbuf, int* totallen);
static DHCP6C_ERROR add_options(dhcp6c_client_state state,int msg_type,TI_DHCP6C_PARAMS* dhcp6c_params, struct dhcp6opt* startbuf,struct dhcp6opt* endbuf, int* totallen);
static DHCP6C_ERROR parse_option(dhcp6c_client_state state,int msg_type,int opt_code, int opt_len, char* optionptr);

DHCP6C_PLUGIN_MCB   sample_na_plugin;

static DHCP6C_ERROR cmd_option_parser(int *argc, char *argv[])
{
	// return error since we don't have any real plugin and hence no
	dprintf(LOG_DEBUG,FNAME, "cmd_option_parser:argc:%d,argv:%p",*argc,argv);
	return TI_DHCP6C_ERROR;
}

/************************************************************************
 * FUNCTION NAME : parse_option
 ************************************************************************
 * DESCRIPTION   :
 * The core passes an option which it is not registered to handle to
 * the plugin to handle through this function. The plugin should 
 * inspect this option and check if it is a valid option. If not it should
 * return an error upon which the core discards the message and proceeds
 * with its state machine.
 ************************************************************************/
static DHCP6C_ERROR parse_option(dhcp6c_client_state state,int msg_type,int opt_code, int opt_len, char* optionptr)
{
    int i;

	dprintf(
		LOG_DEBUG,FNAME, "parse_option:state - %d,msg_type - %d,option ptr - %p",
		state,
	    msg_type, 
		optionptr
		);
    dprintf(LOG_DEBUG,FNAME, "Sample: parse_plugin_option state = %d,opt code = %d, len = %d, data = ",state, opt_code, opt_len);
    for (i=0; i < opt_len; i++)
        printf("%x,",*(optionptr+i));
    dprintf(LOG_DEBUG,FNAME,"\n");

    return TI_DHCP6C_SUCCESS;
} 

/************************************************************************
 * FUNCTION NAME : report_config
 ************************************************************************
 * DESCRIPTION   :
 * The core passes an option which it is not registered to handle to
 * the plugin to handle through this function. The plugin should 
 * inspect this option and check if it is a valid option. If not it should
 * return an error upon which the core discards the message and proceeds
 * with its state machine.
*/
static DHCP6C_ERROR report_config(dhcp6c_client_state state, TI_DHCP6C_PARAMS* dhcp6c_params, DHCP6C_ERROR error)
{
	dprintf(
	  LOG_DEBUG, FNAME,
	  "report_config: state - %d, error - %d, params: %p",
	  state, 
	  error,
      dhcp6c_params
      );
    if(dhcp6c_params) {
    dprintf(LOG_DEBUG, FNAME, "Message Type: %s ClientID: %s ServerID: %s ServerID Pref: %d",
            dhcp6msgstr(dhcp6c_params->msg_type), duidstr(&dhcp6c_params->clientID), duidstr(&dhcp6c_params->clientID),
            dhcp6c_params->pref);
    dprintf(LOG_DEBUG, FNAME, "IA cnt: %d IA_addr:cnt: %d IA_type: %d t1: %d t2: %d", dhcp6c_params->ia_cnt, dhcp6c_params->ia_array[0].ia_prefix_addr_cnt, dhcp6c_params->ia_array[0].ia_type, dhcp6c_params->ia_array[0].ia.t1, dhcp6c_params->ia_array[0].ia.t2 );
    }

	return TI_DHCP6C_SUCCESS;	
}

/************************************************************************
 * FUNCTION NAME : event_logger
 ************************************************************************
 * DESCRIPTION   :
 * Plugin event logger.
*/
static void event_logger(dhcp6c_client_state curr_state,dhcp6c_client_state new_state, char *event )
{
	char *temp = (char *)malloc(strlen(event)+50);
	if (temp)
	{
		sprintf(
			temp,
			"EVENT:%s:current state= %d,new state = %d\n",
			event,
			(unsigned int)curr_state,
			(unsigned int)new_state
			);
		dprintf(LOG_INFO,FNAME, temp);
		free(temp);
	}
	return;
}

/************************************************************************
 * FUNCTION NAME : init_options
 ************************************************************************
 * DESCRIPTION   :
 * Adds any options specific to the plugin in the DHCP SOLICIT message.
*/
static DHCP6C_ERROR init_options(dhcp6c_client_state state,int msg_type,struct dhcp6opt* startbuf,struct dhcp6opt* endbuf, int* totallen)
{
    struct dhcp6_ia ia;
    struct dhcp6_prefix ia_prefix;;
    struct in6_addr addr;
    int ia_len = 0, ia_prefix_num = 0;
    u_int16_t oro_options[2] = {23, 24};

    dprintf(
	  LOG_DEBUG,
      FNAME,
	  "init_options: state - %d,msg_type - %d, len - %d",
	  state,
	  msg_type,
      *totallen
	  );

    switch(msg_type) {
        case DH6_SOLICIT:
            /* Init OPTION_IA_PD */
            memset(&ia, 0, sizeof(struct dhcp6_ia));
        	ia.iaid = 27;
        	ia.t1 = 10;
        	ia.t2 = 20;

            /* Init OPTION_IA_PD_PREFIX */
            memset(&ia_prefix, 0, sizeof(struct dhcp6_prefix));
            ia_prefix.pltime = 100;
            ia_prefix.vltime = 200;
            memset(&addr, 0, sizeof(struct in6_addr));
            if (inet_pton(AF_INET6, "2001:0db8:0100:f101::", &addr) <= 0) {
                dprintf(LOG_ERR,FNAME, "Sample PD Plugin: Unable to get the IPv6 address");
                return TI_DHCP6C_ERROR;
            }
            memcpy(&ia_prefix.addr, &addr, sizeof(struct in6_addr));
            ia_prefix.plen = 64;
            ia_prefix_num += 1;

            ia_len = sizeof(struct dhcp6_ia) + ia_prefix_num * (sizeof(struct dhcp6_prefix) + 4);

            /* Add IA_NA Option */
            if( ti_dhcp6c_copy_option(DH6OPT_IA_PD, ia_len,&ia, &startbuf, endbuf, totallen, state) != 0) {
                dprintf(LOG_ERR,FNAME, "Sample NA Plugin: Couldnt add OPTION_IA_NA");
                return TI_DHCP6C_ERROR;
            }
            /* Add all the IA_PD Options under this IA one after the other here */
            if( ti_dhcp6c_copy_option(DH6OPT_IA_PD_PREFIX, sizeof(struct dhcp6_prefix),&ia_prefix, &startbuf, 
                endbuf, totallen, state) != 0) {
                    dprintf(LOG_ERR,FNAME, "Sample NA Plugin: Couldnt Add OPTION_IAADDR");
                    return TI_DHCP6C_ERROR;
            }
            break;
        case DH6_REQUEST:
            /* Add Option Request Options */
            if( ti_dhcp6c_copy_option(DH6OPT_ORO, sizeof(u_int16_t) * 2,oro_options, &startbuf, endbuf, 
                totallen, state) != 0) {
                dprintf(LOG_ERR,FNAME, "Sample NA Plugin: Couldnt Add OPTION_ORO");
                return TI_DHCP6C_ERROR;
            }
            break;
        case DH6_INFORM_REQ:
            /* Add Option Request Options */
            if( ti_dhcp6c_copy_option(DH6OPT_ORO, sizeof(u_int16_t) * 2,oro_options, &startbuf, endbuf, 
                totallen, state) != 0) {
                dprintf(LOG_ERR,FNAME, "Sample Na Plugin: Couldnt Add OPTION_ORO");
                return TI_DHCP6C_ERROR;
            }
            
            break;
        default:
            break;
   }
   return TI_DHCP6C_SUCCESS;
}

/************************************************************************
 * FUNCTION NAME : add_options
 ************************************************************************
 * DESCRIPTION   :
 * Adds any options specific to the plugin in the DHCP SOLICIT message.
*/
static DHCP6C_ERROR add_options(dhcp6c_client_state state,int msg_type,TI_DHCP6C_PARAMS* dhcp6c_params, struct dhcp6opt* startbuf,struct dhcp6opt* endbuf, int* totallen)
{
    u_int16_t oro_options[2] = {23, 24};
    /* Add Vendor class option */
	struct vendor_class_option  {
        u_int32_t enterprise_number;
	    char hw_confgrn[128];
    } vendor_class_option_1 = { 1234 , "Cable Test Dummy Hw Configrn"};

    dprintf(
	  LOG_DEBUG,
      FNAME,
	  "add_options: state - %d,msg_type - %d, len - %d",
	  state,
	  msg_type,
      *totallen
	  );

    switch(msg_type) {
        case DH6_SOLICIT:
        case DH6_REQUEST:
            #if 0
            if( ti_dhcp6c_copy_option(DH6OPT_VENDOR_CLASS, sizeof(struct vendor_class_option),&vendor_class_option_1, &startbuf, endbuf, totallen, state) != 0) {
                dprintf(LOG_ERR,FNAME, "Sample NA Plugin: Couldnt Add OPTION_VENDOR_CLASS");
                return TI_DHCP6C_ERROR;
            }
            #endif

            /* Add Option Request Options */
            if( ti_dhcp6c_copy_option(DH6OPT_ORO, sizeof(u_int16_t) * 2,oro_options, &startbuf, endbuf, 
                totallen, state) != 0) {
                dprintf(LOG_ERR,FNAME, "Sample NA Plugin: Couldnt Add OPTION_ORO");
                return TI_DHCP6C_ERROR;
            }
            break;
        case DH6_INFORM_REQ:
            /* Add Option Request Options */
            if( ti_dhcp6c_copy_option(DH6OPT_ORO, sizeof(u_int16_t) * 2,oro_options, &startbuf, endbuf, 
                totallen, state) != 0) {
                dprintf(LOG_ERR,FNAME, "Sample NA Plugin: Couldnt Add OPTION_ORO");
                return TI_DHCP6C_ERROR;
            }
            
            break;
        default:
            break;
   }
   return TI_DHCP6C_SUCCESS;
}

/************************************************************************
 * FUNCTION NAME : exit_plugin
 ************************************************************************
 * DESCRIPTION   :
 * Cleansup plugin and exits.
*/
static void dhcp6c_plugin_exit(void)
{
	return;
}

/************************************************************************
 * FUNCTION NAME : plugin_init
 ************************************************************************
 * DESCRIPTION   :
 * Initialises the plugin MCB with appropriate function callbacks, and 
 * registers itself with the core.
*/
void dhcp6c_plugin_init(void)
{
	dprintf(LOG_DEBUG,FNAME, "sample_na_plugin_init");
	bzero((void *)&sample_na_plugin, sizeof(DHCP6C_PLUGIN_MCB));
	sample_na_plugin.major_version=1;
	sample_na_plugin.minor_version=0;
	strcpy(sample_na_plugin.plugin_name,"DHCP6C Sample NA plugin");
	sample_na_plugin.cmd_option_parser=cmd_option_parser;
	sample_na_plugin.init_options=init_options;
	sample_na_plugin.add_options=add_options;
	sample_na_plugin.parse_option=parse_option;
	sample_na_plugin.report_config=report_config;
	sample_na_plugin.event_logger=event_logger;
	sample_na_plugin.exit=dhcp6c_plugin_exit;
	dhcp6c_register_plugin(&sample_na_plugin);
	dprintf(LOG_DEBUG,FNAME, "sample_na_plugin_init");
	return;
}
