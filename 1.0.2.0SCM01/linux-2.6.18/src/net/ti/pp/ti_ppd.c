/*
 * ti_ppd.c
 *
 *  The file contains the Packet Processor Driver. The driver communicates
 *  with the Session Router PDSP.
 *
 * Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "ti_ppd.h"

/* Initialize/Deinitialize API. */
extern int ti_pp_initialize(unsigned int num_of_fw, TI_PP_FIRMWARE *firmware, TI_PPD_CONFIG *cfg);
extern int ti_pp_deinitialize (void);

/* PID/VPID Management API. */
extern int ti_pp_create_pid (TI_PP_PID *params);
extern int ti_pp_delete_pid (unsigned char pid);
extern int ti_pp_set_pid_flags (TI_PP_PID* ptr_pid, unsigned int new_flags);

extern int ti_pp_create_vpid (TI_PP_VPID *params);
extern int ti_pp_delete_vpid (unsigned char vpid);
extern int ti_pp_set_vpid_flags (TI_PP_VPID* ptr_vpid, unsigned int new_flags);

/* Session Management API. */
extern int ti_pp_create_session (TI_PP_SESSION *session_cfg, TI_PPD_IF *ingress_if, TI_PPD_IF *egress_if);
extern int ti_pp_delete_session (unsigned char ses_id);
extern int ti_pp_modify_session (TI_PP_SESSION *session_cfg, TI_PPD_IF *ingress_if, TI_PPD_IF *egress_if);

/* Statistics API */
extern int ti_pp_get_session_stats (unsigned char ses_id, TI_PP_SESSION_STATS *stats);
extern int ti_pp_clear_session_stats (unsigned char ses_id);
extern int ti_pp_get_vpid_stats (unsigned char vpid, TI_PP_VPID_STATS *stats);
extern int ti_pp_get_n_clear_vpid_stats (unsigned char vpid, TI_PP_VPID_STATS *stats);
extern int ti_pp_get_srl_pkt_stats (TI_PP_GLOBAL_STATS *stats);
extern int ti_pp_get_n_clear_srl_pkt_stats (TI_PP_GLOBAL_STATS *stats);

/* Generic API. */
extern int ti_pp_health_check (void);

/* Global Event Handler. */
void (*ppd_event_handler)(unsigned int event_id, unsigned int param1, unsigned int param2);

/**************************************************************************
 * FUNCTION NAME : ti_ppd_create_pid
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to create a PID in the PDSP.
 *
 * RETURNS       :
 *      0   -   Success
 *      <0  -   Error
 **************************************************************************/
int ti_ppd_create_pid(TI_PP_PID *ptr_pid_params)
{
    return ti_pp_create_pid(ptr_pid_params);
}

/**************************************************************************
 * FUNCTION NAME : ti_ppd_delete_pid
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to delete a PID from the PDSP.
 *
 * RETURNS       :
 *      0   -   Success
 *      <0  -   Error
 **************************************************************************/
int ti_ppd_delete_pid(unsigned char pid)
{
    return ti_pp_delete_pid(pid);
}

/**************************************************************************
 * FUNCTION NAME : ti_ppd_config_pid_range
 **************************************************************************
 * DESCRIPTION   :
 *  The function uses the information passed to config PID range in the PDSP.
 *
 * RETURNS       :
 *  Handle to the created VPID - Success
 *  <0                         - Error
 **************************************************************************/
int ti_ppd_config_pid_range(TI_PP_PID_RANGE *pid_range)
{
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_ppd_remove_pid_range
 **************************************************************************
 * DESCRIPTION   :
 *  The function uses the information passed to remove PID range in the PDSP.
 *
 * RETURNS       :
 *  Handle to the created VPID - Success
 *  <0                         - Error
 **************************************************************************/
int ti_ppd_remove_pid_range(unsigned int port_num)
{
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_ppd_set_pid_flags
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to modify PID flags in the PDSP.
 *
 * RETURNS       :
 *      0   -   Success
 *      <0  -   Error
 **************************************************************************/
int ti_ppd_set_pid_flags(TI_PP_PID *ptr_pid_params, unsigned int new_flags)
{
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_ppd_create_vpid
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to create a VPID in the PDSP.
 *
 * RETURNS       :
 *      Handle to the PID   -   Success
 *      <0                  -   Error
 **************************************************************************/
int ti_ppd_create_vpid(TI_PP_VPID *params)
{
    return ti_pp_create_vpid(params);
}

/**************************************************************************
 * FUNCTION NAME : ti_ppd_delete_vpid
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to delete a VPID from the PDSP.
 *
 * RETURNS       :
 *      0   -   Success
 *      <0  -   Error
 **************************************************************************/
int ti_ppd_delete_vpid (unsigned char vpid)
{
    return ti_pp_delete_vpid (vpid);
}

/**************************************************************************
 * FUNCTION NAME : ti_ppd_create_session
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to create a session.
 *
 * RETURNS       :
 *      0   -   Success
 *      <0  -   Error
 **************************************************************************/
int ti_ppd_create_session (TI_PP_SESSION *session_cfg, TI_PPD_IF *ingress_if, TI_PPD_IF *egress_if)
{
    return ti_pp_create_session (session_cfg, ingress_if, egress_if);
}

/**************************************************************************
 * FUNCTION NAME : ti_ppd_modify_session
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to modify the session.
 *
 * RETURNS       :
 *      0   -   Success
 *      <0  -   Error
 **************************************************************************/
int ti_ppd_modify_session (TI_PP_SESSION *session_cfg, TI_PPD_IF *ingress_if, TI_PPD_IF *egress_if)
{
    return ti_pp_modify_session (session_cfg, ingress_if, egress_if);
}

/**************************************************************************
 * FUNCTION NAME : ti_ppd_delete_session
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to delete the session.
 *
 * RETURNS       :
 *      Handle to the session   -   Success
 *      <0                      -   Error
 **************************************************************************/
int ti_ppd_delete_session(unsigned char ses_id)
{
    return ti_pp_delete_session (ses_id);
}

/**************************************************************************
 * FUNCTION NAME : ti_ppd_event_handler
 **************************************************************************
 * DESCRIPTION   :
 *  The function is the event handler which receives events from the
 *  Packet Processor.
 **************************************************************************/
void ti_ppd_event_handler (unsigned int event_id, unsigned int param1, unsigned int param2)
{
    /* Pass the events to all registered handlers. */
    if (ppd_event_handler)
        ppd_event_handler (event_id, param1, param2);
    return;
}

/**************************************************************************
 * FUNCTION NAME : ti_ppd_register_event_handler
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to register an event handler with the PP.
 *
 * RETURNS       :
 *      Handle to the event handler   -   Success
 *      <0                            -   Error
 **************************************************************************/
int ti_ppd_register_event_handler (TI_PPD_EVENT_HANDLER event_handler)
{
    /* Override the event handler. */
    ppd_event_handler = event_handler;
    return (int)ppd_event_handler;
}

/**************************************************************************
 * FUNCTION NAME : ti_ppd_set_vpid_flags
 **************************************************************************
 * DESCRIPTION   :
 *  The function is called to modify VPID flags.
 *
 * RETURNS       :
 *      0   -   Success
 *      <0  -   Error
 **************************************************************************/
int ti_ppd_set_vpid_flags (TI_PP_VPID* ptr_vpid, unsigned int new_flags)
{
    return ti_pp_set_vpid_flags (ptr_vpid, new_flags);
}

/**************************************************************************
 * FUNCTION NAME : ti_ppd_get_session_pkt_stats
 **************************************************************************
 * DESCRIPTION   :
 *  The function is called to get the statistics of a particular session.
 *
 * RETURNS       :
 *      0   -   Success
 *      <0  -   Error
 **************************************************************************/
int ti_ppd_get_session_pkt_stats(unsigned char ses_id, TI_PP_SESSION_STATS *stats)
{
    return ti_pp_get_session_stats(ses_id, stats);
}

/**************************************************************************
 * FUNCTION NAME : ti_ppd_get_n_clear_ses_pkt_stats
 **************************************************************************
 * DESCRIPTION   :
 *  The function is called to clear the statistics of a particular session.
 *
 * RETURNS       :
 *      0   -   Success
 *      <0  -   Error
 **************************************************************************/
int ti_ppd_clear_session_pkt_stats(unsigned char ses_id)
{
    return ti_pp_clear_session_stats(ses_id);
}

/**************************************************************************
 * FUNCTION NAME : ti_ppd_get_vpid_stats
 **************************************************************************
 * DESCRIPTION   :
 *  The function is called to get the statistics of a VPID.
 *
 * RETURNS       :
 *      0   -   Success
 *      <0  -   Error
 **************************************************************************/
int ti_ppd_get_vpid_stats (unsigned char vpid, TI_PP_VPID_STATS *stats)
{
    return ti_pp_get_vpid_stats(vpid, stats);
}

/**************************************************************************
 * FUNCTION NAME : ti_ppd_get_n_clear_vpid_stats
 **************************************************************************
 * DESCRIPTION   :
 *  The function is called to clear the statistics of a particular vpid.
 *
 * RETURNS       :
 *      0   -   Success
 *      <0  -   Error
 **************************************************************************/
int ti_ppd_get_n_clear_vpid_stats(unsigned char vpid, TI_PP_VPID_STATS *stats)
{
    return ti_pp_get_n_clear_vpid_stats(vpid, stats);
}

/**************************************************************************
 * FUNCTION NAME : ti_ppd_get_global_stats
 **************************************************************************
 * DESCRIPTION   :
 *  The function is called to get the statistics of the packet processor
 *
 * RETURNS       :
 *      0   -   Success
 *      <0  -   Error
 **************************************************************************/
int ti_ppd_get_srl_pkt_stats(TI_PP_GLOBAL_STATS *stats)
{
    return ti_pp_get_srl_pkt_stats(stats);
}

/**************************************************************************
 * FUNCTION NAME : ti_ppd_clear_global_stats
 **************************************************************************
 * DESCRIPTION   :
 *  The function is called to clear the statistics of the packet processor
 *
 * RETURNS       :
 *      0   -   Success
 *      <0  -   Error
 **************************************************************************/
int ti_ppd_get_n_clear_srl_pkt_stats(TI_PP_GLOBAL_STATS *stats)
{
    return ti_pp_get_n_clear_srl_pkt_stats(stats);
}

/**************************************************************************
 * FUNCTION NAME : ti_ppd_initialize
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to initialize the Session Router Driver and PDSP.
 *
 * RETURNS       :
 *      0   -   Success
 *      <0  -   Error
 **************************************************************************/
int ti_ppd_init(unsigned int num_of_fw, TI_PP_FIRMWARE *firmware, TI_PPD_CONFIG *cfg)
{
    /* Initialize the Packet Processor */
    if (ti_pp_initialize(num_of_fw, firmware, cfg) < 0)
        return -1;
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_ppd_health_check
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to query the Packet Processor PDSP if they are
 *  running correctly or not?
 *
 * RETURNS       :
 *      0   -   Success
 *      0x1 -   CPDSP Error
 *      0x2 -   MPDSP Error
 *      0x4 -   QPDSP Error
 *      0x8 -   Other Error
 **************************************************************************/
int ti_ppd_health_check (void)
{
    return ti_pp_health_check ();
}

/**************************************************************************
 * FUNCTION NAME : ti_ppd_deinitialize
 **************************************************************************
 * DESCRIPTION   :
 *  The function is called to close the PDSP.
 *
 * RETURNS       :
 *      0   -   Success
 *      <0  -   Error
 **************************************************************************/
int ti_ppd_exit (void)
{
    ti_pp_deinitialize ();
    return 0;
}

