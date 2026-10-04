/*
 * dhcp6c_plugin.h
 *
 * The file contains the definitions of all the PLUGIN API that is 
 * exposed by the core udhcpv6 client .
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

#ifndef _DHCP6C_PLUGIN_H
#define _DHCP6C_PLUGIN_H

#include "dhcp6c_common.h"

/* max len of plugin name including NULL should be 30 */
#define DHCP6C_MAX_PLUGIN_NAME 30

/* Maximum number of IA Addresses / IA Prefixes options that can be requested within a IA */
#define TI_DHCP6C_MAXNUM_ADDR_PREFIX 10

/* Maxmimum number of IA's one can request */
#define TI_DHCP6C_MAXNUM_IA 10

#define OPT_TYPE 0
#define OPT_LEN  2
#define OPT_VAL  4
 
struct dhcp6_option {
	char name[32];
	unsigned char code;
};

/* client status code */
typedef enum dhcp6c_client_state 
{
    DHCP6C_INIT, 
    DHCP6C_SOLICIT, 
    DHCP6C_INFOREQ, 
    DHCP6C_REQUEST,
    DHCP6C_RENEW, 
    DHCP6C_REBIND, 
    DHCP6C_RELEASE, 
    DHCP6C_DECLINE,
    DHCP6C_CONFIRM,
    DHCP6C_IDLE
} dhcp6c_client_state;

typedef union ia_prefix_addr_t{
    struct dhcp6_prefix uv_prefix;
    struct dhcp6_statefuladdr uv_statefuladdr;
} ia_prefix_addr_t;

typedef struct ti_dhcp6c_ia_t
{
    struct dhcp6_ia ia;
    u_int16_t ia_type;
    int ia_prefix_addr_cnt;
    ia_prefix_addr_t ia_prefix_addr[TI_DHCP6C_MAXNUM_ADDR_PREFIX];
} ti_dhcp6c_ia_t;

typedef struct TI_DHCP6C_PARAMS
{
    char* interface;                                /* Interface name */
    struct duid clientID;                           /* Our client ID */
    struct duid serverID;                           /* The Server ID from where the REPLY/ADVERTISE was received */
    int pref;                                       /* The PREFERENCE value of the server recd in REPLY/ADVERTISE */
    ti_dhcp6c_ia_t ia_array[TI_DHCP6C_MAXNUM_IA];     /* List of IA_PD / IA_NAs received in the REPLY/ADVERTISE */
    int ia_cnt;                                     /* Number of IA_NA / IA_PDs received */
	char msg_type;                                  /* Type of the DHCPv6 Message */
	char *options;                                 /* Rest of the options which the plugin can parse */
    int optlen;                                     /* Length of the options buffer */
}TI_DHCP6C_PARAMS;

/***********************************************************************************
 * Structure name : DHCP6C_PLUGIN_MCB
 ***********************************************************************************/

typedef struct DHCP6C_PLUGIN_MCB 
{
	/* version information about plugin */
	unsigned short major_version;
	unsigned short minor_version;

	/* name of the plugin */
	char plugin_name[DHCP6C_MAX_PLUGIN_NAME]; 

	/* List of callback function to be populated by the plugin */

	/* parse the command line option specific to a plugin. update *argc by the number
       arguments it consumes + 1 (for the option itself)
	*/  
	DHCP6C_ERROR  	(*cmd_option_parser)(int *argc, char *argv[]);

	/* add plugin specific options like IA_NA, IA_PD, ORO, Rapid Comit etc. 
	*/
	DHCP6C_ERROR	(*init_options)(dhcp6c_client_state state,int msg_type,struct dhcp6opt* startbuf, 
                          struct dhcp6opt* endbuf, int* totallen); 

	/* add vendor specific options. 
	*/
	DHCP6C_ERROR	(*add_options)(dhcp6c_client_state state,int msg_type, TI_DHCP6C_PARAMS* dhcp6c_params, 
                           struct dhcp6opt* startbuf, struct dhcp6opt* endbuf, int* totallen); 
    
	/* parse option from server, return status  */
	DHCP6C_ERROR	(*parse_option)(dhcp6c_client_state state,int msg_type, int opt_code, int opt_len, char* option);	

	/* Core pass DHCP configuration received from server to plugin using this API.
	   Return 0 - Success, the error code otherwise
	*/
	DHCP6C_ERROR (*report_config)(dhcp6c_client_state state, TI_DHCP6C_PARAMS *dhcp6c_params, DHCP6C_ERROR error );  

	/* DHCP Event logger. Handle General DHCP events (discovery, ack, nack, or error events)*/
	void 	(*event_logger)(dhcp6c_client_state curr_state, dhcp6c_client_state new_state, char* event);

	/* Finds a shared key from the plugin database */
    struct keyinfo* (*find_key)(char *realm, size_t realmlen, u_int32_t id);

	/* handle exit */
	void 	(*exit)(void);

}DHCP6C_PLUGIN_MCB;

/* Registration & Unregistration of Plugins with the core TI DHCP6C. */
extern int dhcp6c_register_plugin (DHCP6C_PLUGIN_MCB *ptr_plugin);
//extern int dhcp6c_unregister_plugin (char* ptr_name);

/* Returns the IPv6 DHCP server address */
extern char *ti_dhcp6c_get_dhcpserveraddr(void);

#endif /* _DHCP6C_PLUGIN_H */
