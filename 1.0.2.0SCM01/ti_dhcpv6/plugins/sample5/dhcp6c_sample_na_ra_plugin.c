/**************************************************************************
 * FILE PURPOSE	:  	dhcp6c sample na ra plugin
 **************************************************************************
 * FILE NAME	:   dhcp6c_sample_na_ra_plugin.c
 *
 * DESCRIPTION	:
 * This plugin adds option "Reconfigure Accept".
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

#define MAX_KEY_NUM 2

static DHCP6C_ERROR cmd_option_parser(int *argc, char *argv[]);
static void event_logger(dhcp6c_client_state curr_state,dhcp6c_client_state new_state, char *event );
static DHCP6C_ERROR report_config(dhcp6c_client_state state,TI_DHCP6C_PARAMS* dhcp6c_params, DHCP6C_ERROR error);
static DHCP6C_ERROR init_options(dhcp6c_client_state state,int msg_type,struct dhcp6opt* startbuf,struct dhcp6opt* endbuf, int* totallen);
static DHCP6C_ERROR add_options(dhcp6c_client_state state,int msg_type,TI_DHCP6C_PARAMS* dhcp6c_params, struct dhcp6opt* startbuf,struct dhcp6opt* endbuf, int* totallen);
static DHCP6C_ERROR parse_option(dhcp6c_client_state state,int msg_type,int opt_code, int opt_len, char* optionptr);
static struct keyinfo* find_key_here(char *realm, size_t realmlen, u_int32_t id);

DHCP6C_PLUGIN_MCB   sample_na_plugin;
struct keyinfo* keys[MAX_KEY_NUM];

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
*/
static DHCP6C_ERROR init_options(dhcp6c_client_state state,int msg_type,struct dhcp6opt* startbuf,struct dhcp6opt* endbuf, int* totallen)
{
    struct dhcp6_statefuladdr ia_addr;
    struct dhcp6_ia ia;
    struct in6_addr addr;
    int ia_len = 0, ia_addr_num = 0;
    u_int16_t oro_options[2] = {23, 24};
    struct auth_basic_params au;

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
                dprintf(LOG_ERR,FNAME, "Sample NA RA Plugin: Couldnt add OPTION_IA_NA");
                return TI_DHCP6C_ERROR;
            }
            /* Add all the IAADDR Options under this IA one after the other here */
            if( ti_dhcp6c_copy_option(DH6OPT_IAADDR, sizeof(struct dhcp6_statefuladdr),&ia_addr, &startbuf, 
                endbuf, totallen, state) != 0) {
                    dprintf(LOG_ERR,FNAME, "Sample NA RA Plugin: Couldnt Add OPTION_IAADDR");
                    return TI_DHCP6C_ERROR;
            }


            /* AUTH options */
            /* Init the auth params */
            memset(&au, 0, sizeof(struct auth_basic_params));
            au.authproto = DH6OPT_AUTH_PROTO_DELAYED;
            au.authalgorithm = DH6OPT_AUTH_ALG_HMACMD5;
            au.authrdm = DHCP6_AUTHRDM_MONOCOUNTER;
            au.authrd = 0;
            /* Add AUTH Option to Solicit message */
            if( ti_dhcp6c_copy_option(DH6OPT_AUTH, 11,&au, &startbuf, endbuf, 
                totallen, state) != 0) {
                dprintf(LOG_ERR,FNAME, "Sample NA RA Plugin: Couldnt add OPTION_AUTH");
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
    u_int16_t oro_options[3] = {22, 23, 24};
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

            /* Add Reconfigure Accept Option */
            #if 0
            if( ti_dhcp6c_copy_option(DH6OPT_RECONF_ACCEPT, 0,NULL, &startbuf, endbuf, totallen, state) != 0) {
                dprintf(LOG_ERR,FNAME, "Sample NA Reconfigure Accept Plugin: Couldnt Add OPTION_RECONF_ACCEPT");
                return TI_DHCP6C_ERROR;
            }
            #endif
            break;
        case DH6_INFORM_REQ:
            /* Add Option Request Options */
            if( ti_dhcp6c_copy_option(DH6OPT_ORO, sizeof(u_int16_t) * 2,oro_options, &startbuf, endbuf, 
                totallen, state) != 0) {
                dprintf(LOG_ERR,FNAME, "Sample NA Reconfigure Accept Plugin: Couldnt Add OPTION_ORO");
                return TI_DHCP6C_ERROR;
            }

            /* Add Reconfigure Accept Option */
            #if 0
            if( ti_dhcp6c_copy_option(DH6OPT_RECONF_ACCEPT, 0,NULL, &startbuf, endbuf, totallen, state) != 0) {
                dprintf(LOG_ERR,FNAME, "Sample NA Reconfigure Accept Plugin: Couldnt Add OPTION_RECONF_ACCEPT");
                return TI_DHCP6C_ERROR;
            }
            #endif
            break;
        default:
            break;
   }
   return TI_DHCP6C_SUCCESS;
}

/************************************************************************
 * FUNCTION NAME : find_key
 ************************************************************************
 * DESCRIPTION   :
 * Looks up the database of shared keys and returns a pointer to the key
 * based on the realm, len and the key id. Returns NULL if key not found.
*/
static struct keyinfo* find_key_here(char *realm, size_t realmlen, u_int32_t id)
{
    int i = 0;

    printf("RA pluginfind_key_here keyid: %d name: %s realm: %s relamlen: %d secret: %s secretlen: %d expire: %d \n",
           keys[0]->keyid, keys[0]->name, keys[0]->realm, keys[0]->realmlen, keys[0]->secret, keys[0]->secretlen, 
           keys[0]->expire);

    printf("realm: %s realmlen: %d id: %d \n", realm, realmlen, id);

    for(i = 0; i < MAX_KEY_NUM && keys[i]; i++) {
        if (keys[i]->realmlen == realmlen && memcmp(keys[i]->realm, realm, realmlen) == 0 
            && keys[i]->keyid == id) {
                return (keys[i]);
        }
    }

	return NULL;
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
    struct keyinfo* keyinfo = NULL;
    char* secretstr = NULL;
    char secret[1024];
    int secretlen = 0;

	dprintf(LOG_DEBUG,FNAME, "sample_na_ra_plugin_init");

	bzero((void *)&sample_na_plugin, sizeof(DHCP6C_PLUGIN_MCB));
	sample_na_plugin.major_version=1;
	sample_na_plugin.minor_version=0;
	strcpy(sample_na_plugin.plugin_name,"DHCP6C Sample NA RA plugin");
	sample_na_plugin.cmd_option_parser=cmd_option_parser;
	sample_na_plugin.init_options=init_options;
	sample_na_plugin.add_options=add_options;
	sample_na_plugin.parse_option=parse_option;
	sample_na_plugin.report_config=report_config;
	sample_na_plugin.event_logger=event_logger;
	sample_na_plugin.find_key=find_key_here;
	sample_na_plugin.exit=dhcp6c_plugin_exit;
	dhcp6c_register_plugin(&sample_na_plugin);

    /* TAHI Test Shared Key */
    keyinfo = (struct keyinfo *)malloc(sizeof(struct keyinfo));
    keyinfo->name = strdup("SharedKey1");
    keyinfo->realm = strdup("DHCPv6.TEST.EXAMPLE.COM");
    keyinfo->realmlen = strlen(keyinfo->realm);
    keyinfo->keyid = 1;
    secretstr = strdup("VEFISVRFU1RfVkFMSUQxMg==");
    /* convert base64 string to binary secret */
    memset(secret, 0, sizeof(secret));
    secretlen = base64_decodestring(secretstr,secret, sizeof(secret));
    printf("secretlen: %d secret: %s \n", secretlen, secret);

    if (secretlen < 0) {
        dprintf(LOG_ERR, FNAME,"failed to parse base64 key %s",secretstr);
        printf("failed to parse base64 key %s",secretstr);
        free(secretstr);
        free(keyinfo);
        return;
    }
    free(secretstr);
    keyinfo->secret = malloc(secretlen);
    if(keyinfo->secret == NULL) {
        dprintf(LOG_ERR, FNAME, "Cant allocate memory for secret");
        printf("Cant allocate memory for secret");
        free(keyinfo);
        return;
    }
    memcpy(keyinfo->secret, secret, secretlen);
    keyinfo->secretlen = secretlen;
    keyinfo->expire = 0;

    keys[0] = keyinfo;

    printf("RA plugin keyid: %d name: %s realm: %s relamlen: %d secret: %s secretlen: %d expire: %d \n",
           keys[0]->keyid, keys[0]->name, keys[0]->realm, keys[0]->realmlen, keys[0]->secret, keys[0]->secretlen, 
           keys[0]->expire);

	dprintf(LOG_DEBUG,FNAME, "sample_na_ra_plugin_init");
	return;
}
