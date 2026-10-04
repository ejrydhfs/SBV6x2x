/*
 *
 * csl_switchPrvt.h
 * Description:
 * see below
 *
 *
 * Copyright (C) 2008, Texas Instruments, Incorporated
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 */


#ifndef __SWITCH_PRVT_H__
#define __SWITCH_PRVT_H__

#include "ddc_switch.h"

/* Internal enumeration for the statistic counters */

typedef enum
{
       InBytes = 0, 
       InBytesHigh,
       InUnicastPkts,
       InUnicastPktsHigh,
       InMulticastPkts,
       InMulticastPktsHigh,
       InBroadcastPkts,
       InBroadcastPktsHigh,
       InDiscardPkts,
       InErrorPkts,
       InUnknownProtPkts,
       OutBytes,
       OutBytesHigh,
       OutUnicastPkts,
       OutUnicastPktsHigh,
       OutMulticastPkts,
       OutMulticastPktsHigh,
       OutBroadcastPkts,
       OutBroadcastPktsHigh,
       OutDiscardPkts,
       OutErrorPkts,
       ethAlignmentErrors,    
       ethFCSErrors,          
       ethSingleCollisions,   
       ethMultipleCollisions, 
       ethSQETestErrors,      
       ethDeferredTxFrames,   
       ethLateCollisions,     
       ethExcessiveCollisions,
       ethInternalMacTxErrors,
       ethCarrierSenseErrors, 
       ethTooLongRxFrames,    
       ethInternalMacRxErrors,
       ethSymbolErrors

} SWITCH_STAT_E;


/* Operation type */

typedef enum
{
    OPER_STATUS,
    OPER_LINK,
    OPER_DUPLEX,
    OPER_DUPLEX_SET,
    OPER_SPEED,
    OPER_SPEED_SET,
    OPER_FC,
    OPER_AUTO,
    OPER_INGRESS,
    OPER_INGRESS_OFF,
    OPER_PORT_RECOGNITION_ON,
    OPER_TRANSMIT_ON_FIXED_PORT,
    OPER_VLANTUNNEL,
    OPER_RESET_STAT,
	OPER_VLAN_TABLE_SET,
}OPER_TYPE_E;

typedef  struct _csl_switch_vlan_table CSL_SWITCH_VLAN_TABLE;
typedef  struct _csl_switch_vlan_table *PCSL_SWITCH_VLAN_TABLE;

/* callbacks types for the switch */
/* base switch */
typedef Int32  (*CSL_SWITCH_START                )(void);
typedef Int32   (*CSL_REG_VAL2OPER_STATUS        )(OPER_TYPE_E oper, Uint16 data);   
typedef Uint16  (*CSL_OPER_STATUS2REG_VAL        )(OPER_TYPE_E oper,Int32 operValue,Uint16 dat);  
typedef void    (*CSL_SWITCH_GET_VERSION         )(PSW_VER_INFO pSwitchVerInfo);   
typedef Int32   (*CSL_PREPARE_READ_STAT_SEQUENCE )(Uint32 port);   
typedef Int32   (*CSL_GET_PREPARED_COUNTERS      )(Uint32 statIndex,Uint32* counter);   
typedef Uint32  (*CSL_PORT_NUM2REG_ADDR          )(OPER_TYPE_E type, Uint32 portNum);
typedef Uint32  (*CSL_PORT_NUM2PHY_ADDR          )(OPER_TYPE_E type, Uint32 portNum);

/* advanced switch */
typedef Int32   (*CSL_VLN_DEFAULT_SET )(Uint32 portNum,PCSL_SWITCH_VLAN_TABLE pVlnEntry);
typedef Int32  (*CSL_VLN_UPDATE      )(PCSL_SWITCH_VLAN_TABLE pVlnEntry);
typedef Int32   (*CSL_VLN_PRIORITY_SET)(Uint32 port,Uint32 priority);
typedef Uint32  (*CSL_VLN_PRIORITY_GET)(Uint32 port);
typedef Int32   (*CSL_VLN_CAPS_SET    )(Uint32 cmd, Uint32 arg);
typedef Int32   (*CSL_DELETE_MAC_ADD  )(char MacAdd[6]);
typedef Int32  (*CSL_SWITCH_LEARNING_DIS)(void);


typedef struct 
{
    CSL_SWITCH_START                cslSwitchStart; 
    CSL_REG_VAL2OPER_STATUS         cslRegVal2operStatus;   
    CSL_OPER_STATUS2REG_VAL         cslOperStatus2regVal;   
    CSL_SWITCH_GET_VERSION          cslSwitchGetVersion;    
    CSL_PREPARE_READ_STAT_SEQUENCE  cslPrepareReadStatSequence;
    CSL_GET_PREPARED_COUNTERS       cslGetPreparedCounters;     
    CSL_PORT_NUM2REG_ADDR           cslPortNum2RegAddr;     
    CSL_PORT_NUM2PHY_ADDR           cslPortNum2PhyAddr;   
    CSL_SWITCH_LEARNING_DIS         cslSwitchLearningDisable;
/* advanced switch management */

    CSL_VLN_DEFAULT_SET             cslVlnDefaultSet;
    CSL_VLN_UPDATE                  cslVlnUpdate;
    CSL_VLN_PRIORITY_SET            cslVlnPrioritySet;
    CSL_VLN_PRIORITY_GET            cslVlnPriorityGet;
    CSL_VLN_CAPS_SET                cslVlnCapsSet;
    CSL_DELETE_MAC_ADD              cslDeleteMacAddEntry; 
}CSL_SWITCH_CLBKS,*PCSL_SWITCH_CLBKS;


struct _csl_switch_vlan_table
{
    Bool valid;
    Uint32 vlanId;
    Uint32 portMask;
    Uint32 portTagMask;
    PCSL_SWITCH_VLAN_TABLE next;
};



/*******************************************************************************
* cslConnectSwitch - switch initialization
*
*  This API does the callback structure initialization. Here the type of the switch is considered.
*
* INPUTS:
*       [In/Out] PCSL_SWITCH_DRV_HANDLE swHead
*
*******************************************************************************/

Int32 cslConnectSwitch(PCSL_SWITCH_CLBKS pCslSwitchClbks);

/*******************************************************************************
* cslGetVLANEntry - gets VLAN entry
*
*  Retreives vlnId entry from VLAN table. List manager.
*
* INPUTS:
*       [In] Uint32 vlnId
*
*******************************************************************************/

PCSL_SWITCH_VLAN_TABLE cslGetVLANEntry(Uint32 vlnId);

/*******************************************************************************
* cslCreateVLANEntry - creates and adds VLAN entry to the VLAN table
*
*  Creates and adds VLAN entry to the logical VLAN table. List manager.
*
* INPUTS:
*
*******************************************************************************/

PCSL_SWITCH_VLAN_TABLE cslCreateVLANEntry(void);

/*******************************************************************************
* cslDestroyVLANEntry - destroys VLAN entry
*
*  Removes VLAN entry from VLAN table. List manager.
*
* INPUTS:
*
*******************************************************************************/

Int32 cslDestroyVLANEntry(PCSL_SWITCH_VLAN_TABLE elem);

/*******************************************************************************
*
* cslMarvell6063PrioritySet - Sets the default priority of the packets from this port
*   
*   Sets the default priority of the packets from this port.
*
* INPUTS:
*     [In]   PCSL_SWITCH_DRV_HANDLE pDrvHead
*     [In]   Uint32                 portNum
*     [In]   Uint32                 priority
*
*
* RETURNS: 
*       Status
*
*
*/

Int32 cslMarvell6063SpecialCapsSet(Uint32 cmd, Uint32 arg);

/*******************************************************************************
* cslCleanMem - Clean up VLAN entry
*
*
* INPUTS:
*
*******************************************************************************/
PAL_Result cslCleanMem(void);

#endif /* __SWITCH_PRVT_H__ */
