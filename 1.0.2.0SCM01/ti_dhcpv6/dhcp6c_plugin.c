/*
 * dhcp6c_plugin.c
 *
 * The file contains the implementation of all the PLUGIN API that is 
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
#include <dlfcn.h>
#include "dhcp6c.h"

/**************************************************************************
 * FUNCTION NAME : dhcp6c_register_plugin
 **************************************************************************
 * DESCRIPTION   :
 *  This function is the API that is invoked by the plugin to place itself
 *  into the dhcp6c Core application. It is assumed that the PLUGIN
 *  structure has been filled.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 *************************************************************************/
int dhcp6c_register_plugin (DHCP6C_PLUGIN_MCB* ptr_plugin)
{
	int len;
    DHCP6C_PLUGIN_MCB*  ptr_dhcp6c_plugin = &dhcp6c_mcb.plugin;

    /* Validate the version of the plugin. */
    if (ptr_plugin->major_version < dhcp6c_mcb.major_version)
    {
        dprintf(LOG_ERR, FNAME, "plugin register failed due to version mismatch\n");
        return -1;
    }
	ptr_dhcp6c_plugin->major_version = ptr_plugin->major_version;
	ptr_dhcp6c_plugin->minor_version = ptr_plugin->minor_version;
	len = strlen(ptr_plugin->plugin_name);
	if (len >= DHCP6C_MAX_PLUGIN_NAME)
	{
        dprintf(LOG_ERR, FNAME, "plugin register failed due to plugin name exceeding size\n");
        return -1;
    }

    dprintf(LOG_DEBUG, FNAME, "In register_plugin");

	strncpy(ptr_dhcp6c_plugin->plugin_name,ptr_plugin->plugin_name,len);
	ptr_dhcp6c_plugin->plugin_name[len] = (char)NULL;

	if (ptr_plugin->cmd_option_parser)
		ptr_dhcp6c_plugin->cmd_option_parser = ptr_plugin->cmd_option_parser;
	if (ptr_plugin->parse_option)
		ptr_dhcp6c_plugin->parse_option = ptr_plugin->parse_option;
	if (ptr_plugin->init_options)
		ptr_dhcp6c_plugin->init_options= ptr_plugin->init_options;
	if (ptr_plugin->add_options)
		ptr_dhcp6c_plugin->add_options= ptr_plugin->add_options;
	if (ptr_plugin->report_config)
		ptr_dhcp6c_plugin->report_config= ptr_plugin->report_config;
	if (ptr_plugin->event_logger)
		ptr_dhcp6c_plugin->event_logger= ptr_plugin->event_logger;
	if (ptr_plugin->find_key)
		ptr_dhcp6c_plugin->find_key = ptr_plugin->find_key;
	if (ptr_plugin->exit)
        ptr_dhcp6c_plugin->exit= ptr_plugin->exit;

    /* Return Success. */
    dprintf(LOG_DEBUG, FNAME, "Plugin %s registered successfully\n", ptr_plugin->plugin_name);
    return 0;
}


/**************************************************************************
 * FUNCTION NAME : dhcp6c_unregister_plugin
 **************************************************************************
 * DESCRIPTION   :
 *  This function is the API that is invoked by the plugin to unregister
 *  itself from the core dhcp6c application.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 *************************************************************************/
#if 0
int udhcp_unregister_plugin (char* ptr_name)
{
	DHCP6C_PLUGIN_MCB*   ptr_dhcp6c_plugin = &dhcp6c_mcb.plugin;

    /* Basic Validations: Validate the name of the plugin that is to be unregistered? */
    if (ptr_name == NULL)
    {
        dprintf(LOG_ERR, FNAME, "dhcp6c plugin Error: No name passed for unregistering.\n");
        return -1;
    }

	bzero((void *)ptr_dhcp6c_plugin, sizeof(DHCP6C_PLUGIN_MCB));

    /* The plugin has been unregistered. */
    dprintf(LOG_INFO, FNAME, "Plugin %s unregistered succesfully\n", ptr_name);
    return 0;
}
#endif
