/*******************************************************************************
**+--------------------------------------------------------------------------+**
**|                            ****                                          |**
**|                            ****                                          |**
**|                            ******o***                                    |**
**|                      ********_///_****                                   |**
**|                      ***** /_//_/ ****                                   |**
**|                       ** ** (__/ ****                                    |**
**|                           *********                                      |**
**|                            ****                                          |**
**|                            ***                                           |**
**|                                                                          |**
**|         Copyright (c) 2007-2008 Texas Instruments Incorporated           |**
**|                        ALL RIGHTS RESERVED                               |**
**|                                                                          |**
**+--------------------------------------------------------------------------+**
*******************************************************************************/

/** \file   pal_osTimer.h
    \brief  OsTIMER Services Header File

    This file declares OS abstraction services for OS Timers.
    All services run in the context of the calling thread or program.
    OsTIMER does not spawn a thread of its own to implement the APIs
    declared here.

 
    \author     PSP Architecture Team
    \version    1.0
 */

#ifndef __PAL_OSTIMER_H__
#define __PAL_OSTIMER_H__

#include "pal_defs.h"
#include "pal_os.h"

/**
 * \defgroup PalOSTimer PAL OS Timer Interface
 * 
 * PAL OS Timer Interface
 * @{
 */

/** \name PAL OS Timer Interface
 *  PAL OS Timer Interface
 * @{
 */

typedef Ptr PAL_OsTimerHandle;
typedef void (*PAL_OsTimerFunc) (unsigned long);


/**
 * \brief   PAL OS Timer Create
 * 
 *      This function creates and initializes a timer with user provided timer
 *      function. 
 * \warn    Note that the timer function is called in (software) interrupt
 *          context and thus all the constraints related to interrupt handlers
 *          are applicable.
 * \param   pfn [IN] pointer to timer function 
 * \param   arg [IN] parameter to be passed to timer function
 * \param   phTimer [OUT] location to recieve the handle to timer just created
 * \return  PAL_SOK if succesful, else a suitable error code.
 */
PAL_INLINE PAL_Result PAL_osTimerCreate(PAL_OsTimerFunc pfn, 
                                Uint32 arg, 
                                PAL_OsTimerHandle* phTimer);

/**
 * \brief   PAL OS Timer Destroy
 * 
 *      This function destroys the specified timer. The timer is stopped if
 *      active and data associated is cleaned up. 
 * \param   hTimer [IN] handle to the timer
 * \return  PAL_SOK if succesful, else a suitable error code.
 */
PAL_INLINE PAL_Result PAL_osTimerDestroy(PAL_OsTimerHandle hTimer);

/**
 * \brief   PAL OS Timer Start
 * 
 *      This function activates the specified timer. The timer is areloaded
 *      with new timeout if already started.
 * \param   hTimer [IN] handle to the timer
 * \param   msec [IN] timeout in milli seconds before the timer expires
 * \return  PAL_SOK if succesful, else a suitable error code.
 */
PAL_INLINE PAL_Result PAL_osTimerStart(PAL_OsTimerHandle hTimer, Uint32 msec);

/**
 * \brief   PAL OS Timer Stop
 * 
 *      This function deactivates the specified timer. 
 * \param   hTimer [IN] handle to the timer
 * \return  PAL_SOK if succesful, else a suitable error code.
 */
PAL_INLINE PAL_Result PAL_osTimerStop(PAL_OsTimerHandle hTimer);

/*@}*/
/*@}*/

#endif /* _PAL_OSSTIMER_H_ */
