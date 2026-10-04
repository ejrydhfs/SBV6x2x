/*
 * ti_ppd.h
 *
 * Packet processor driver header file.
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

/* Version 0.3 */

#ifndef _INCLUDE_TI_PPD_H
#define _INCLUDE_TI_PPD_H

/* Import the TI standard primitive "C" types defines */
#include "_tistdtypes.h"

#include "ti_ppm.h"

/**********************************************************************
 * TI_PPD_PP_EVENT_..... 
 *
 *  The following events are supported by Packet Processor Firmware.
 */
#define TI_PPD_PP_EVENT_SESSION_IDLE            0x01
#define TI_PPD_PP_EVENT_SESSION_NOTIDLE         0x02
#define TI_PPD_PP_EVENT_SESSION_SYNCTIMEOUT     0x03
#define TI_PPD_PP_EVENT_CONFIG_ERROR            0x04
#define TI_PPD_PP_EVENT_SESSION_SYNCOVERFLOW    0x05
#define TI_PPD_PP_EVENT_OVERFLOW_GLOBAL         0x10
#define TI_PPD_PP_EVENT_OVERFLOW_VPID           0x11
#define TI_PPD_PP_EVENT_OVERFLOW_SESSION        0x12
#define TI_PPD_PP_EVENT_INTFATAL_SSTATE         0x80


/*
 * ti_ppd_init -
 *
 * Desctiption:
 *  This API initializes the Packet Processor Driver (PPD). It resets
 *  and downloads the firmware to PDSPs in the Packet Processor system. The
 *  caller should provide all the required firmware code for PDSPs along with
 *  the initialization configuration for Packet Processor system. The PP System
 *  is brought into operational state after successful completion of this API.
 *
 * Parameters:
 *  num_of_fw (IN)  - Number of firmwares to be downloaded.
 *  firmware (IN)   - Array of num_of_fw
 *  cfg (IN)        - Pointer to default configuration
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 ti_ppd_init(Uint32 num_of_fw, TI_PP_FIRMWARE *firmware,
                  TI_PPD_CONFIG *cfg);

/*
 * ti_ppd_exit -
 *
 * Description:
 *  This API de-initializes PP System. It triggers cleanup sequence in PP
 *  Firmware. All held packets will be forwarded to default host queue specified
 *  during ti_ppd_init. Packet Processor firmware will be closed and PDSP layer
 *  will be shut down as a result of this API.
 *
 * Precondition:
 *  - ti_ppd_init
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 ti_ppd_exit(void);

/*
 * TI_PPD_IF -
 *
 * Description:
 *  Interface information associated with session's ingress/egress property.
 *  Consists of PID and VPID parameters.
 */
typedef struct ti_ppd_if
{
	TI_PP_PID	*pid;
	TI_PP_VPID	*vpid;

} TI_PPD_IF;


/*
 *                      Session management APIs
 */

/*
 * ti_ppd_create_session -
 *
 * Description:
 *  This API creates a session. Following information is required:
 *      -   Session identifier (0-255)
 *      -   Ingress parameters for classification based on L2 and/or L3
 *      -   Session type and Egress parameters for modification (if applicable)
 *
 * Notes:
 *  Following points list the constraints imposed:
 *      -   Session state is forced as SYNC REQUIRED
 *      -   L3 Modification prepared only when session is routable.
 *      -   Every L3/L4 field to be modified should have corresponding ingress
 *          field provided for checksum delta calculation. The enables flags in
 *          the descriptor will be used to deduce this information
 *      -   L2 header will be modified when enables flags in egress l2
 *          descriptor are set.
 *      -   *IMPORTANT NOTE* Current implementation requires routable sessions to
 *          have new L2 header provided * CHK *
 *      -   Minimum header information must be provided for egress L2. The enables
 *          flags in the descriptor will be used to deduce the availability of
 *          header fields.
 *      -   PID entry is always enabled in LUT L2 data
 *      -   When timeout is specified, only IDLE based timeout is supported
 *      -   For sessions with multiple egress properties, Modification record of
 *          1st egress record only is configured (and used for subsequent records).
 *          Thus, only one set of modifications is allowed across all applicable
 *          egress records.
 *
 *
 * Precondition:
 *  - ti_ppd_create_pid for ingress and egress PID
 *  - ti_ppd_enable_vpid for ingress and egress VPID
 *
 * Parameters:
 *  session_cfg (IN)    - Pointer to session description. Should be allocated
 *                        and filled by caller
 *  ingress_if (IN)     - Ingress PID and VPID information
 *  egress_if (IN)      - Array of Egress PID and VPID information
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 ti_ppd_create_session(TI_PP_SESSION *session_cfg, TI_PPD_IF *ingress_if,
						    TI_PPD_IF *egress_if);

/*
 * ti_ppd_modify_session -
 *
 * Description:
 *  This API modifies previously created session. Following information is required:
 *      -   Valid session identifier (0-255)
 *      -   Ingress parameters for classification based on L2 and/or L3
 *      -   Session type and Egress parameters for modification (if applicable)
 *
 * Notes:
 *      -   This API supports updating egress record(s), modification record(s)
 *          and LUT entry for the session. Modification of session configuration
 *          (e.g., session timeout, session flags) is not supported currently.
 *      -   Following sequence is used for session modification:
 *          1) Put the session in DIVERSION
 *          2) Modify egress information
 *          3) Update LUT information
 *          4) SYNCHRONIZE the session
 *      -   Session PAUSE/RESUME is NOT used.
 *      -   Since existing record blocks (egress, modification and header
 *          blocks) are released before adding new records, this API may fail
 *          due to resource avalability failure (in case the resources are
 *          exausted before this API gets chance to reserve them).
 *      -   All other constraints related to egress configuration as for
 *          ti_ppd_create_session are applicable.
 *
 *      -   __IMPORTANT__ This API destroyes the sesison in case of error duirng
 *          session modification steps. Only the following 2 error case will
 *          retain previous (unmodified) session:
 *          1.  Specified session not present.
 *          2.  Failure in putting session in DIVERSION mode.
 *
 * Precondition:
 *  Session must a valid session created using ti_ppd_create_session
 *
 * Parameters:
 *  session_cfg (IN)    - Pointer to session description. Should be allocated
 *                        and filled by caller.
 *  ingress_if (IN)     - Ingress PID and VPID information
 *  egress_if (IN)      - Array of Egress PID and VPID information
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 ti_ppd_modify_session(TI_PP_SESSION *ses_cfg, TI_PPD_IF *ingress_if,
							TI_PPD_IF *egress_if);

/*
 * ti_ppd_get_session_dump -
 *
 * Description:
 *  This API provides a dump of various session configurations data like Base
 *  Session Record. Egress Record(s) and Modification Record(s). The provided
 *  dump is a NULL terminated string.
 *
 * Notes:
 *  This API requires a buffer of at least 1000 bytes per data it needs to dump
 *  for the specified session. For example, if a session has 1 Egress Record and
 *  1 Modification Record, then the minimum buffer size that need to be provided
 *  to this API would be 1000*3 bytes (1000 bytes each for Base Session, Egress
 *  and Modification Record Dump).
 *
 * Precondition:
 *  Session must a valid session created using ti_ppd_create_session
 *
 * Parameters:
 *  session_handle (IN) - Session identifier
 *  session_state (IN)  - New state for specified session
 *
 * Return:
 *      -   -1 on Error.
 *      -   0 when provided buffer is not sufficient to hold dump.
 *      -   Size of the null terminated dump string otherwise.
 */
Int32 ti_ppd_get_session_dump (char *buf, char **start, Uint32 offset, int count, int *eof, void *data);

/*
 * ti_ppd_delete_session -
 *
 * Description:
 *  This API removes previously created session.
 *
 * Precondition:
 *  Session must a valid session created using ti_ppd_create_session
 *
 * Parameters:
 *  session_handle (IN) - Session identifier
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 ti_ppd_delete_session(Uint8 session_handle);

/*
 *                          PID related APIs
 */

/*
 * ti_ppd_config_pid_range -
 *
 * Desctiption:
 *  This API configures PID range for specified CPPI port and type.
 *
 * Precondition:
 *  ti_ppd_init
 *
 * Parameters:
 *  pid_range (IN)  - Pointer to PID range configuration structure.
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 ti_ppd_config_pid_range(TI_PP_PID_RANGE *pid_range);

/*
 * ti_ppd_remove_pid_range -
 *
 * Desctiption:
 *  This API removes the PID range for the specified CPPI port.
 *
 * Precondition:
 *  ti_ppd_config_pid_range
 *
 * Parameters:
 *  port_num (IN)  - CPPI port numberfor the range to be removed.
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 ti_ppd_remove_pid_range(Uint32 port_num);

/*
 * ti_ppd_create_pid -
 *
 * Desctiption:
 *  This API creates and configures PID.
 *
 * Precondition:
 *  The PID must fall within the range installed previously by
 *  ti_ppd_config_pid_range
 *
 * Parameters:
 *  pid_params (IN)  - Pointer to PID configuration structure.
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 ti_ppd_create_pid(TI_PP_PID *pid_params);

/*
 * ti_ppd_set_pid_flags -
 *
 * Desctiption:
 *  This API sets the PID flags to specified value.
 *
 * Precondition:
 *  PID must have been created successfully by ti_ppd_create_pid
 *
 * Parameters:
 *  pid_params (IN) -   Pointer to PID configuration structure.
 *  new_flags (IN)  -   New flags.
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 ti_ppd_set_pid_flags(TI_PP_PID *pid_params, Uint32 new_flags);

/*
 * ti_ppd_delete_pid -
 *
 * Desctiption:
 *  This API removes the specified PID.
 *
 * Precondition:
 *  PID must have been created successfully by ti_ppd_create_pid
 *
 * Note:
 *  The caller must ensure that all the VPIDs based on specified PID are removed
 *  before this call (this implicitly means that all the sessions based on the
 *  respective VPID should have been removed).
 *
 * Parameters:
 *  pid_handle (IN) - PID index.
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 ti_ppd_delete_pid(Uint8 pid_handle);


/*
 *                  VPID related APIs
 */

/*
 * ti_ppd_create_vpid -
 *
 * Desctiption:
 *  This API creates specified VPID with default flags.
 *
 * Precondition:
 *  PPD must have been initilized using ti_ppd_init
 *
 * Parameters:
 *  vpid_params (IN)  - Pointer to VPID configuration structure.
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 ti_ppd_create_vpid(TI_PP_VPID *vpid_params);

/*
 * ti_ppd_set_vpid_flags -
 *
 * Desctiption:
 *  This API sets VPID flags to specified value. Note that the old flags will be
 *  overwritten with the new ones. It is advised that the caller maintains the
 *  current copy of flags value to ensure that the VPID enable/disable status is
 *  as desired.
 *
 * Precondition:
 *  VPID must have been created successfully by ti_ppd_create_vpid
 *
 * Parameters:
 *  vpid_params (IN) - Pointer to VPID configuration structure.
 *  new_flags (IN)   - New flags
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 ti_ppd_set_vpid_flags(TI_PP_VPID *vpid_params, Uint32 new_flags);

/*
 * ti_ppd_delete_vpid -
 *
 * Desctiption:
 *  This API removes the specified VPID.
 *
 * Precondition:
 *  VPID must have been created successfully by ti_ppd_create_vpid
 *
 * Note:
 *  The caller must also ensure that all sessions that depend on the specified
 *  VPID are removed using ti_ppd_delete_session before calling this API.
 *
 * Parameters:
 *  vpid_handle (IN) - VPID index.
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 ti_ppd_delete_vpid(Uint8 vpid_handle);


/*
 *                  Statistics APIs
 */
/*
 * ti_ppd_get_vpid_stats -
 *
 * Description:
 *  This API provides the statistics for the specified VPID. The statistics
 *  maintained in firmware are NOT cleared by this API.
 *
 * Note:
 *  The statistics structure should match the corresponding statistics block
 *  provided by the firmware since this API directly fills the statistics query
 *  result in the provided structure.
 *
 * Precondition:
 *  VPID must have been created successfully by ti_ppd_create_vpid
 *
 * Parameters:
 *  vpid_handle (IN)    - VPID index
 *  stats (OUT)         - Pointer to the structure to hold the statistics in.
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 ti_ppd_get_vpid_stats(Uint8 vpid_handle, TI_PP_VPID_STATS *stats);

/*
 * ti_ppd_clear_vpid_stats -
 *
 * Description:
 *  This API clears the statistics for specified VPID.
 *
 * Precondition:
 *  VPID must have been created successfully by ti_ppd_create_vpid
 *
 * Parameters:
 *  vpid_handle (IN)    - VPID index
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 ti_ppd_clear_vpid_stats(Uint8 vpid_handle);

/*
 * ti_ppd_get_n_clear_vpid_stats -
 *
 * Description:
 *  This API provides the statistics for the specified VPID. The statistics
 *  maintained in firmware are cleared by this API.
 *
 * Note:
 *  The statistics structure should match the corresponding statistics block
 *  provided by the firmware since this API directly fills the statistics query
 *  result in the provided structure.
 *
 * Precondition:
 *  VPID must have been created successfully by ti_ppd_create_vpid
 *
 * Parameters:
 *  vpid_handle (IN)    - VPID index
 *  stats (OUT)         - Pointer to the structure to hold the statistics in.
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 ti_ppd_get_n_clear_vpid_stats(Uint8 vpid_handle,
                                    TI_PP_VPID_STATS *stats);

/*
 *  ti_ppd_get_srl_pkt_stats -
 *
 * Description:
 *  This API provides the Packet Processor global statistics. The statistics
 *  maintained in firmware are NOT cleared by this API.
 *
 * Note:
 *  The statistics structure should match the corresponding statistics block
 *  provided by the firmware since this API directly fills the statistics query
 *  result in the provided structure.
 *
 * Precondition:
 *  PPD must have been initialized by ti_ppd_init
 *
 * Parameters:
 *  stats (OUT)         - Pointer to the structure to hold the statistics in.
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 ti_ppd_get_srl_pkt_stats(TI_PP_GLOBAL_STATS *stats);

/*
 * ti_ppd_clear_srl_pkt_stats -
 *
 * Description:
 *  This API clears the global statistics.
 *
 * Precondition:
 *  PPD must have been initialized by ti_ppd_init
 *
 * Parameters:
 *  None.
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 ti_ppd_clear_srl_pkt_stats(void);

/*
 *  ti_ppd_get_n_clear_srl_pkt_stats -
 *
 * Description:
 *  This API provides the Packet Processor global statistics. The statistics
 *  maintained in firmware are cleared by this API.
 *
 * Note:
 *  The statistics structure should match the corresponding statistics block
 *  provided by the firmware since this API directly fills the statistics query
 *  result in the provided structure.
 *
 * Precondition:
 *  PPD must have been initialized by ti_ppd_init
 *
 * Parameters:
 *  stats (OUT)         - Pointer to the structure to hold the statistics in.
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 ti_ppd_get_n_clear_srl_pkt_stats(TI_PP_GLOBAL_STATS *stats);

/*
 * ti_ppd_get_session_pkt_stats -
 *
 * Description:
 *  This API provides the statistics for the specified session. The statistics
 *  maintained in firmware are NOT cleared by this API.
 *
 * Note:
 *  The statistics structure should match the corresponding statistics block
 *  provided by the firmware since this API directly fills the statistics query
 *  result in the provided structure.
 *
 * Precondition:
 *  The session  must have been created successfully by ti_ppd_create_session
 *
 * Parameters:
 *  session_handle (IN) - Session id
 *  stats (OUT)         - Pointer to the structure to hold the statistics in.
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 ti_ppd_get_session_pkt_stats(Uint8 session_handle,
                                    TI_PP_SESSION_STATS *stats);

/*
 * ti_ppd_clear_session_pkt_stats -
 *
 * Description:
 *  This API clears the statistics for the specified session.
 *
 * Precondition:
 *  The session  must have been created successfully by ti_ppd_create_session
 *
 * Parameters:
 *  session_handle (IN) - Session id
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 ti_ppd_clear_session_pkt_stats(Uint8 session_handle);

/*
 * ti_ppd_get_n_clear_session_pkt_stats -
 *
 * Description:
 *  This API provides the statistics for the specified session. The statistics
 *  maintained in firmware are cleared by this API.
 *
 * Note:
 *  The statistics structure should match the corresponding statistics block
 *  provided by the firmware since this API directly fills the statistics query
 *  result in the provided structure.
 *
 * Precondition:
 *  The session  must have been created successfully by ti_ppd_create_session
 *
 * Parameters:
 *  session_handle (IN) - Session id
 *  stats (OUT)         - Pointer to the structure to hold the statistics in.
 *
 * Return:
 *  0 on Success, <0 on error.
 */
Int32 ti_ppd_get_n_clear_session_pkt_stats(Uint8 session_handle,
                                           TI_PP_SESSION_STATS *stats);

typedef void (*TI_PPD_EVENT_HANDLER) (unsigned short event_id,
                                        unsigned int param1,
                                        unsigned int param2);
/*
 * ti_ppd_register_event_handler -
 *
 * Description:
 *  This API is used to register Packet Processor event handler with the PPD.
 *
 * Note 1:
 *  The registered event handler will be called in (software) interrupt context
 *  and thus caller must ensure that all the constraints related to interrupt
 *  handlers are followed. For example, the event handler could just post the
 *  event to some listner and return immediately and the even would be processed
 *  in some different context.
 *
 * Note 2:
 *  The current implementation provides only one event per timer execution, that
 *  is, if there are more than one events queued up by PP before timer
 *  expiration, only the first queued event will be provided to the callback and
 *  next event(s) will be dispatched after subsequent timer expiration(s).
 *
 * Parameters:
 *  ptr_event_handler (IN) -    Pointer to event handler to be called for PPD
 *                              events.
 *
 * Return:
 *  0 on Success, -1 on Error.
 */
Int32 ti_ppd_register_event_handler (TI_PPD_EVENT_HANDLER ptr_event_handler);

/*
 * ti_ppd_deregister_event_handler -
 *
 * Description:
 *  This API is used to de-register Packet Processor event handler with the PPD.
 *
 * Parameters:
 *  None.
 *
 * Return:
 *  Always 0.
 */
Int32 ti_ppd_deregister_event_handler (void);

/*
 * ti_ppd_health_check -
 *
 * Description:
 *  This API is used to get the running status of PDSP and firmware.
 *
 * Parameters:
 *  None.
 *
 * Return:
 *  0 if the PDSPs and firmware are running properly.
 *  Non-zero on firmware/PDSP error condition :-
 *      0x01    - CPDSP error
 *      0x02    - MPDSP error
 *      0x03    - Both CPDSP and MPDSP in error
 *      -1      - PP System Failure
 *      <-1     - indicate corresponding command error code from
 *                firmware.
 *
 * Note 1:
 *  As per the PP firmware implementation, the PDSPs respond to the health
 *  check sequentially, i.e., first, CPDSP fills its status and propagates the
 *  command to MPDSP which then adds its own status and signals the host about
 *  information availability. Thus, the status check command may not return when
 *  a PDSP is in error. Also, the status of subsequent PDSPs is NOT
 *  checked when previous PDSP was unable to respond. In such cases error code
 *  "PP System Failure" (-1) is returned. This means error code "Both PDSPs in
 *  error" is not applicable in current implementation.
 *
 * Note 2:
 *  In case of error, this API waits for sufficiently long time (TODO: approx
 *  time) to flag a PDSP error.
 */
Int32 ti_ppd_health_check (void);


#endif /* !_INCLUDE_TI_PPD_H */
