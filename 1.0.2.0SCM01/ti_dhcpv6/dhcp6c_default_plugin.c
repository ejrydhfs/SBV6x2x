/*
 * dhcp6c_default_plugin.c
 *
 * The file contains the default plugin implementation that can be 
 * overridden by custom plugins functions
 *
 * Copyright (C) 2008 Texas Instruments Incorporated - http://www.ti.com/ 
 * 
 * 
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions 
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the   
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/

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

#include "dhcp6c_plugin.h"


static DHCP6C_ERROR cmd_option_parser_default(int *argc, char *argv[]);
static void event_logger_default(dhcp6c_client_state curr_state,dhcp6c_client_state new_state, char *event );
static DHCP6C_ERROR report_config_default(dhcp6c_client_state state,TI_DHCP6C_PARAMS* dhcp6c_params, DHCP6C_ERROR error);
static DHCP6C_ERROR init_options_default(dhcp6c_client_state state,int msg_type,struct dhcp6opt* startbuf,struct dhcp6opt* endbuf, int* totallen);
static DHCP6C_ERROR add_options_default(dhcp6c_client_state state,int msg_type,TI_DHCP6C_PARAMS* dhcp6c_params, 
struct dhcp6opt* startbuf,struct dhcp6opt* endbuf, int* totallen);
static DHCP6C_ERROR parse_option_default(dhcp6c_client_state state,int msg_type,int opt_code, int opt_len, char* optionptr);
static struct keyinfo* find_key_here(char *realm, size_t realmlen, u_int32_t id);

DHCP6C_PLUGIN_MCB   default_dhcp6c_plugin;

static DHCP6C_ERROR cmd_option_parser_default(int *argc, char *argv[])
{
    if(*argc > 0)
	    dprintf(LOG_DEBUG,FNAME, "cmd_option_parser_default:argc:%d,argv:%p",*argc,argv);
    else
        return TI_DHCP6C_ERROR;

	return TI_DHCP6C_ERROR;
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
    /* Realm should be a valid char pointer, and realmlen and id have to be positive */
    if( !realm || realmlen <= 0 || id <=0 ) {
        dprintf(LOG_DEBUG, FNAME, "Invalid arguments to find_key");
        return NULL;
    }
	return NULL;
}

/************************************************************************
 * FUNCTION NAME : parse_option_default
 ************************************************************************
 * DESCRIPTION   :
 * The core passes an option which it is not registered to handle to
 * the plugin to handle through this function. The plugin should 
 * inspect this option and check if it is a valid option. If not it should
 * return an error upon which the core discards the message and proceeds
 * with its state machine.
 ************************************************************************/
static DHCP6C_ERROR parse_option_default(dhcp6c_client_state state,int msg_type,int opt_code, int opt_len, char* optionptr)
{
    int i;

	dprintf(
		LOG_DEBUG,FNAME, "parse_option_default:state - %d,msg_type - %d,option ptr - %p",
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
 * FUNCTION NAME : report_config_default
 ************************************************************************
 * DESCRIPTION   :
 * The core passes an option which it is not registered to handle to
 * the plugin to handle through this function. The plugin should 
 * inspect this option and check if it is a valid option. If not it should
 * return an error upon which the core discards the message and proceeds
 * with its state machine.
*/
static DHCP6C_ERROR report_config_default(dhcp6c_client_state state, TI_DHCP6C_PARAMS* dhcp6c_params, DHCP6C_ERROR error)
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
 * FUNCTION NAME : event_logger_default
 ************************************************************************
 * DESCRIPTION   :
 * Plugin event logger.
*/
static void event_logger_default(dhcp6c_client_state curr_state,dhcp6c_client_state new_state, char *event )
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
 * FUNCTION NAME : init_options_default
 ************************************************************************
 * DESCRIPTION   :
 * Adds any options specific to the plugin in the DHCP SOLICIT message.
*/
static DHCP6C_ERROR init_options_default(dhcp6c_client_state state,int msg_type,struct dhcp6opt* startbuf,struct dhcp6opt* endbuf, int* totallen)
{
    struct dhcp6_statefuladdr ia_addr;
    struct dhcp6_ia ia;
    struct in6_addr addr;
    int ia_len = 0, ia_addr_num = 0;
    u_int16_t oro_options[2] = {23, 24};

    dprintf(
	  LOG_DEBUG,
      FNAME,
	  "init_options_default: state - %d,msg_type - %d, len - %d",
	  state,
	  msg_type,
      *totallen
	  );

    switch(msg_type) {
        case DH6_SOLICIT:
        case DH6_CONFIRM:
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
                dprintf(LOG_ERR,FNAME, "Default Plugin: Unable to get the IPv6 address");
                return TI_DHCP6C_ERROR;
            }
            memcpy(&ia_addr.addr, &addr, sizeof(struct in6_addr));
            ia_addr_num += 1;

            ia_len = sizeof(struct dhcp6_ia) + ia_addr_num * (sizeof(struct dhcp6_statefuladdr) + 4);

            /* Add IA_NA Option */
            if( ti_dhcp6c_copy_option(DH6OPT_IA_NA, ia_len,&ia, &startbuf, endbuf, totallen, state) != 0) {
                dprintf(LOG_ERR,FNAME, "Default Plugin: Couldnt add OPTION_IA_NA");
                return TI_DHCP6C_ERROR;
            }
            /* Add all the IAADDR Options under this IA one after the other here */
            if( ti_dhcp6c_copy_option(DH6OPT_IAADDR, sizeof(struct dhcp6_statefuladdr),&ia_addr, &startbuf, 
                endbuf, totallen, state) != 0) {
                    dprintf(LOG_ERR,FNAME, "Default Plugin: Couldnt Add OPTION_IAADDR");
                    return TI_DHCP6C_ERROR;
            }

            break;
        case DH6_REQUEST:
            /* Add Option Request Options */
            if( ti_dhcp6c_copy_option(DH6OPT_ORO, sizeof(u_int16_t) * 2,oro_options, &startbuf, endbuf, 
                totallen, state) != 0) {
                dprintf(LOG_ERR,FNAME, "Default Plugin: Couldnt Add OPTION_ORO");
                return TI_DHCP6C_ERROR;
            }
            break;
        case DH6_INFORM_REQ:
            /* Add Option Request Options */
            if( ti_dhcp6c_copy_option(DH6OPT_ORO, sizeof(u_int16_t) * 2,oro_options, &startbuf, endbuf, 
                totallen, state) != 0) {
                dprintf(LOG_ERR,FNAME, "Default Plugin: Couldnt Add OPTION_ORO");
                return TI_DHCP6C_ERROR;
            }
            
            break;
        default:
            break;
   }
   return TI_DHCP6C_SUCCESS;
}

/************************************************************************
 * FUNCTION NAME : add_options_default
 ************************************************************************
 * DESCRIPTION   :
 * Adds any options specific to the plugin in the DHCP SOLICIT message.
*/
static DHCP6C_ERROR add_options_default(dhcp6c_client_state state,int msg_type, TI_DHCP6C_PARAMS* dhcp6c_params, struct dhcp6opt* startbuf,struct dhcp6opt* endbuf, int* totallen)
{
    u_int16_t oro_options[2] = {23, 24};
    /* Add Vendor class option */
    #if 0
	struct vendor_class_option  {
        u_int32_t enterprise_number;
	    char hw_confgrn[128];
    } vendor_class_option_1 = { 1234 , "Cable Test Dummy Hw Configrn"};
    #endif

    dprintf(
	  LOG_DEBUG,
      FNAME,
	  "add_options_default: state - %d,msg_type - %d, len - %d dhcp6c_params - %p",
	  state,
	  msg_type,
      *totallen,
      dhcp6c_params
	  );

    switch(msg_type) {
        case DH6_SOLICIT:
        case DH6_REQUEST:
            #if 0
            if( ti_dhcp6c_copy_option(DH6OPT_VENDOR_CLASS, sizeof(struct vendor_class_option),&vendor_class_option_1, &startbuf, endbuf, totallen, state) != 0) {
                dprintf(LOG_ERR,FNAME, "Default Plugin: Couldnt Add OPTION_VENDOR_CLASS");
                return TI_DHCP6C_ERROR;
            }
            #endif

            /* Add Option Request Options */
            if( ti_dhcp6c_copy_option(DH6OPT_ORO, sizeof(u_int16_t) * 2,oro_options, &startbuf, endbuf, 
                totallen, state) != 0) {
                dprintf(LOG_ERR,FNAME, "Default Plugin: Couldnt Add OPTION_ORO");
                return TI_DHCP6C_ERROR;
            }
            break;
        case DH6_INFORM_REQ:
            /* Add Option Request Options */
            if( ti_dhcp6c_copy_option(DH6OPT_ORO, sizeof(u_int16_t) * 2,oro_options, &startbuf, endbuf, 
                totallen, state) != 0) {
                dprintf(LOG_ERR,FNAME, "Default Plugin: Couldnt Add OPTION_ORO");
                return TI_DHCP6C_ERROR;
            }
            
            break;
        default:
            break;
   }
   return TI_DHCP6C_SUCCESS;
}

/************************************************************************
 * FUNCTION NAME : exit_default
 ************************************************************************
 * DESCRIPTION   :
 * Cleansup plugin and exits.
*/
static void exit_default(void)
{
	return;
}

/************************************************************************
 * FUNCTION NAME : default_plugin_init
 ************************************************************************
 * DESCRIPTION   :
 * Initialises the plugin MCB with appropriate function callbacks, and 
 * registers itself with the core.
*/
void default_plugin_init(void)
{
	dprintf(LOG_DEBUG,FNAME, "default_plugin_init");
	    /* Extract the version information */
    sscanf(
		VERSION, 
		"%hu.%hu", (unsigned short *)&default_dhcp6c_plugin.major_version, 
		(unsigned short *)&default_dhcp6c_plugin.minor_version
		);

	bzero((void *)&default_dhcp6c_plugin, sizeof(DHCP6C_PLUGIN_MCB));
	default_dhcp6c_plugin.major_version=1;
	default_dhcp6c_plugin.minor_version=0;
	strcpy(default_dhcp6c_plugin.plugin_name,"DHCP6C Default");
	default_dhcp6c_plugin.find_key=find_key_here;
	default_dhcp6c_plugin.cmd_option_parser=cmd_option_parser_default;
	default_dhcp6c_plugin.init_options=init_options_default;
	default_dhcp6c_plugin.add_options=add_options_default;
	default_dhcp6c_plugin.parse_option=parse_option_default;
	default_dhcp6c_plugin.report_config=report_config_default;
	default_dhcp6c_plugin.event_logger=event_logger_default;
	default_dhcp6c_plugin.exit=exit_default;
	dhcp6c_register_plugin(&default_dhcp6c_plugin);
	dprintf(LOG_DEBUG,FNAME, "default_plugin_init");
	return;
}
