/**************************************************************************
 * FILE PURPOSE	:  	dhcp6c sample1 plugin
 **************************************************************************
 * FILE NAME	:   dhcp6c_sample_na_plugin.c
 *
 * DESCRIPTION	:
 *  The file contains the plugin implementation that requests basic IA_NA
 *  options and some OROs.
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
        dprintf(LOG_DEBUG, FNAME, "%x,",*(optionptr+i));
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
    int i = 0, j = 0;
    char* optionptr = NULL;
    char *bp, *cp;
	struct dhcp6opt *np, opth, *p, *ep;
	unsigned int option_code, option_len; 
    ti_dhcp6c_ia_t* ia_t_ptr = NULL;
    char cmd[1024];

	dprintf(
	  LOG_DEBUG, FNAME,
	  "report_config: state - %d, error - %d, params: %p",
	  state, 
	  error,
      dhcp6c_params
      );

    if(dhcp6c_params) {
        dprintf(LOG_DEBUG, FNAME, "Message Type: %s ClientID: %s ServerID: %s ServerID Pref: %d IA_cnt: %d",
            dhcp6msgstr(dhcp6c_params->msg_type), duidstr(&dhcp6c_params->clientID), duidstr(&dhcp6c_params->clientID),
            dhcp6c_params->pref, dhcp6c_params->ia_cnt);

        dprintf(LOG_DEBUG, FNAME, "IA details: \n");
        for(i = 0; i < dhcp6c_params->ia_cnt; i++) {
            ia_t_ptr = &dhcp6c_params->ia_array[i];
            dprintf(LOG_DEBUG, FNAME, "IA_addrcnt: %d IA_type: %d IAID: %d t1: %d t2: %d \n", 
                    ia_t_ptr->ia_prefix_addr_cnt, 
                    ia_t_ptr->ia_type,ia_t_ptr->ia.iaid, 
                    ia_t_ptr->ia.t1, ia_t_ptr->ia.t2 );
            for(j =0; j < ia_t_ptr->ia_prefix_addr_cnt; j ++) {
                if(ia_t_ptr->ia_type == DH6OPT_IA_NA) {
                    dprintf(LOG_DEBUG, FNAME, "Addr: %s Pltime: %d Vltime: %d \n", 
                        in6addr2str(&(ia_t_ptr->ia_prefix_addr[j].uv_statefuladdr.addr), 0), 
                        ia_t_ptr->ia_prefix_addr[j].uv_statefuladdr.pltime,
                        ia_t_ptr->ia_prefix_addr[j].uv_statefuladdr.vltime);
                    if(dhcp6c_params->msg_type == DH6_REPLY) {  
                        if((state == DHCP6C_SOLICIT || state == DHCP6C_REQUEST || 
                           state == DHCP6C_RENEW || state == DHCP6C_REBIND || state == DHCP6C_CONFIRM) 
                           && ia_t_ptr->ia_prefix_addr[j].uv_statefuladdr.pltime != 0) 
                            sprintf(cmd, "ifconfig %s add %s/128", dhcp6c_params->interface, 
                                    in6addr2str(&(ia_t_ptr->ia_prefix_addr[j].uv_statefuladdr.addr), 0));
                        else
                            sprintf(cmd, "ifconfig %s del %s/128", dhcp6c_params->interface, 
                                    in6addr2str(&(ia_t_ptr->ia_prefix_addr[j].uv_statefuladdr.addr), 0));
                        dprintf(LOG_DEBUG, FNAME, "Executing Command %s", cmd);
                        system(cmd);
                    }
                }
                else if(ia_t_ptr->ia_type == DH6OPT_IA_PD) {
                    dprintf(LOG_DEBUG, FNAME, "Prefix: %s Pltime: %d Vltime: %d \n", 
                        in6addr2str(&(ia_t_ptr->ia_prefix_addr[j].uv_prefix.addr), 0), 
                        ia_t_ptr->ia_prefix_addr[j].uv_prefix.pltime,
                        ia_t_ptr->ia_prefix_addr[j].uv_prefix.vltime);
                    if(dhcp6c_params->msg_type == DH6_REPLY) {  
                        if(state == DHCP6C_SOLICIT || state == DHCP6C_REQUEST || 
                            state == DHCP6C_RENEW || state == DHCP6C_REBIND || state == DHCP6C_CONFIRM) 
                            sprintf(cmd, "ifconfig %s add %s/128", dhcp6c_params->interface, 
                                    in6addr2str(&(ia_t_ptr->ia_prefix_addr[j].uv_prefix.addr), 0));
                        else
                            sprintf(cmd, "ifconfig %s del %s/128", dhcp6c_params->interface, 
                                    in6addr2str(&(ia_t_ptr->ia_prefix_addr[j].uv_prefix.addr), 0));
                        dprintf(LOG_DEBUG, FNAME, "Executing Command %s", cmd);
                        system(cmd);
                    }
            }
        }

        optionptr = (char *)dhcp6c_params->options;    
        dprintf(LOG_DEBUG, FNAME, "Dump options buffer: optionsptr: %p len: %d", optionptr, dhcp6c_params->optlen);
        if(optionptr) {
	        bp = (char *)optionptr;
	        p = (struct dhcp6opt *)(optionptr);
	        ep = (struct dhcp6opt *)((char *)optionptr + dhcp6c_params->optlen - 4); 
	        for (; p + 1 <= ep; p = np) {
		        memcpy(&opth, p, sizeof(opth));
		        option_len = ntohs(opth.dh6opt_len);
		        option_code = ntohs(opth.dh6opt_type);

		        cp = (char *)(p + 1);
		        np = (struct dhcp6opt *)(cp + option_len);

                dprintf(LOG_DEBUG, FNAME, "option: [%d]%s option_len: %d \n", option_code, dhcp6optstr(option_code), option_len);
            }
        }
      }
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
 * Options like IA_NA/IA_PD/IA_ADDRESS/IA_PREFIX/AUTH related 
 * options only should be added here. The rest of the options should be 
 * added in add_options function. Any other options added here are 
 * ignored.
*/
static DHCP6C_ERROR init_options(dhcp6c_client_state state,int msg_type,struct dhcp6opt* startbuf,struct dhcp6opt* endbuf, int* totallen)
{
    struct dhcp6_statefuladdr ia_addr;
    struct dhcp6_ia ia;
    struct in6_addr addr;
    int ia_len = 0, ia_addr_num = 0;
    
    /* Uncomment to override CLIENTID from the core */ 
    #if 0
	struct clid_option_type2  {
        u_int16_t duidtype;
        u_int32_t enterprise_number;
	    char eid[16];
    } __attribute__ ((__packed__));
    struct clid_option_type2 clid_1; 
    int i = 0;
    char eid[16] = {0x0, 0x0, 0xC, 0xC, 0x0, 0x8, 0x4, 0xD, 0x3, 0x0, 0x3, 0xD, 0xF, 0xF, 0xE, 0x4};
    #endif
    #if 0
	struct clid_option_type3  {
        u_int16_t duidtype;
        u_int16_t hwtype;
	    char lladdress[12];
    } __attribute__ ((__packed__));
    struct clid_option_type3 clid_2; 
    int i = 0;
    char lladdress[12] = {0x0, 0x0, 0x1, 0x1, 0x2, 0x2, 0x3, 0x3, 0x4, 0x4, 0x5, 0x5};
    #endif

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
        case DH6_CONFIRM:
            /* Uncomment to override CLIENTID from the core */ 
            /* Type 2 - DUID-EN */
            #if 0
            memset(&clid_1, 0, sizeof(struct clid_option_type2));
            clid_1.duidtype = 2;
            clid_1.enterprise_number = 9;
            for(i =0; i < 16; i++)
              memcpy((void *)&clid_1.eid[i], (void *)&eid[i], sizeof(char));
            if( ti_dhcp6c_copy_option(DH6OPT_CLIENTID, sizeof(struct clid_option_type2),&clid_1, &startbuf, endbuf, totallen, state) != 0) {
                dprintf(LOG_ERR,FNAME, "Sample NA Plugin: Couldnt Add OPTION_CLIENT_ID");
                return TI_DHCP6C_PLUGIN_ERROR;
            }
            #endif
            /* Type 3 - DUID-LL */
            #if 0
            memset(&clid_2, 0, sizeof(struct clid_option_type3));
            clid_2.duidtype = 3;
            clid_2.hwtype = 1;
            for(i =0; i < 12; i++)
              memcpy((void *)&clid_2.lladdress[i], (void *)&lladdress[i], sizeof(char));
            if( ti_dhcp6c_copy_option(DH6OPT_CLIENTID, sizeof(struct clid_option_type3),&clid_2, &startbuf, endbuf, totallen, state) != 0) {
                dprintf(LOG_ERR,FNAME, "Sample NA Plugin: Couldnt Add OPTION_CLIENT_ID");
                return TI_DHCP6C_PLUGIN_ERROR;
            }
            #endif

            /* Init OPTION_IA_NA */
            memset(&ia, 0, sizeof(struct dhcp6_ia));
        	ia.iaid = 9;
        	ia.t1 = 10;
        	ia.t2 = 20;

            /* Init OPTION_IAADDR */
            memset(&ia_addr, 0, sizeof(struct dhcp6_statefuladdr));
            ia_addr.pltime = 100;
            ia_addr.vltime = 200;
            memset(&addr, 0, sizeof(struct in6_addr));
            if (inet_pton(AF_INET6, "2001:db8:1:2::1001", &addr) <= 0) {
                dprintf(LOG_ERR,FNAME, "Sample NA Plugin: Unable to get the IPv6 address");
                return TI_DHCP6C_ERROR;
            }
            memcpy(&ia_addr.addr, &addr, sizeof(struct in6_addr));
            ia_addr_num += 1;

            ia_len = sizeof(struct dhcp6_ia) + ia_addr_num * (sizeof(struct dhcp6_statefuladdr) + 4);

            /* Add IA_NA Option */
            if( ti_dhcp6c_copy_option(DH6OPT_IA_NA, ia_len,&ia, &startbuf, endbuf, totallen, state) != 0) {
                dprintf(LOG_ERR,FNAME, "Sample NA Plugin: Couldnt add OPTION_IA_NA");
                return TI_DHCP6C_ERROR;
            }
            /* Add all the IAADDR Options under this IA one after the other here */
            if( ti_dhcp6c_copy_option(DH6OPT_IAADDR, sizeof(struct dhcp6_statefuladdr),&ia_addr, &startbuf, 
                endbuf, totallen, state) != 0) {
                    dprintf(LOG_ERR,FNAME, "Sample NA Plugin: Couldnt Add OPTION_IAADDR");
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
 * All options other than auth, IA_NA/IA_PD related can be requested here.
*/
static DHCP6C_ERROR add_options(dhcp6c_client_state state,int msg_type,TI_DHCP6C_PARAMS* dhcp6c_params, struct dhcp6opt* startbuf,struct dhcp6opt* endbuf, int* totallen)
{
    u_int16_t oro_options[2] = {23, 24};
    /* Uncomment to request vendor class */
    #if 0
    /* Add Vendor class option */
	struct vendor_class_option  {
        u_int32_t enterprise_number;
	    char hw_confgrn[128];
    } vendor_class_option_1 = { 1234 , "Cable Test Dummy Hw Configrn"};
    #endif

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
        case DH6_RENEW:
        case DH6_REBIND:
        case DH6_INFORM_REQ:
            /* Uncomment to request vendor class */
            #if 0
            if( ti_dhcp6c_copy_option(DH6OPT_VENDOR_CLASS, sizeof(struct vendor_class_option),&vendor_class_option_1, &startbuf, endbuf, totallen, state) != 0) {
                dprintf(LOG_ERR,FNAME, "Sample NA Plugin: Couldnt Add OPTION_VENDOR_CLASS");
                return -1;
            }
            #endif

            /* Add Option Request Options */
            if( ti_dhcp6c_copy_option(DH6OPT_ORO, sizeof(u_int16_t) * 2,oro_options, &startbuf, endbuf, 
                totallen, state) != 0) {
                dprintf(LOG_ERR,FNAME, "Sample NA Plugin: Couldnt Add OPTION_ORO");
                return TI_DHCP6C_ERROR;
            }
            if( ti_dhcp6c_copy_option(DH6OPT_RECONF_ACCEPT, 0,NULL, &startbuf, endbuf, totallen, state) != 0) {
                dprintf(LOG_ERR,FNAME, "Sample NA Reconfigure Accept Plugin: Couldnt Add OPTION_RECONF_ACCEPT");
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
