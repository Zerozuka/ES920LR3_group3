/*******************************************************************************
* timerctl header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/
//#ifndef __TIMERCTL_H
#define __TIMERCTL_H
#ifdef __cplusplus
 extern "C" {
#endif

/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/

#define UsrTimer_DurationMax        1296000 // ms (36h)
#define UsrTimer_SleepDurationMax   250     // s
#define UsrWakeupTimer_DurationMax  129600  // s  (36h)

/*******************************************************************************
********************************************************************************
* Public type definitions
********************************************************************************
*******************************************************************************/

typedef enum UserTimerId_tag
{
    UsrSleepTimer,
    UsrLedTimer,
    UsrHeartBeatTimer,
    UsrUartCheckTimer,
    UsrPhyCcaTimer,
    UsrPhyTxTimer,
    UsrPhyRxTimer,
    UsrPhyDutyTimer,
    UsrSmacCcaBackoffTimer,
    UsrSmacTxTimer,
    UsrTxIntervalTimer,
#if defined(BLD_ENABLE_RFMODE_RSSI_CHECK)
    UsrRssiCheckTimer,
#endif
#if defined(BLD_ENABLE_ADAPTIVE_SF)
    UsrAdaptiveSfTimer,
#endif
    UsrTimer_MAX,
} UsrTimerId;

typedef enum UsrTimerMode_tag
{
    UsrTimerMode_OneShot,
    UsrTimerMode_Periodic,
} UsrTimerMode;

typedef void (*UsrTimerHandler)( UsrTimerId timerId );
typedef void (*UsrWakeupTimerHandler)( void );

/*******************************************************************************
********************************************************************************
* Public memory declarations
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Public Prototypes
********************************************************************************
*******************************************************************************/

void UsrTimer_start( UsrTimerId timerId, UsrTimerMode mode, uint32_t millisec, UsrTimerHandler handler );
void UsrTimer_start_PSV( UsrTimerId timerId, UsrTimerMode mode, uint32_t millisec, UsrTimerHandler handler );
void UsrTimer_stop( UsrTimerId timerId );
void UsrTimer_stop_PSV( UsrTimerId timerId );
bool_t UsrTimer_isIdle( void );

void UsrWakeupTimer_start( uint32_t seconds, UsrWakeupTimerHandler handler );
void UsrWakeupTimer_stop( void );
bool_t UsrWakeupTimer_isIdle( void );

void PendSvProcess_UsrTimer_start( void );
void PendSvProcess_UsrTimer_stop( void );
void PendSvProcess_LPTIM_Expired( void );

#ifdef __cplusplus
}

#endif /* __TIMERCTL_H */
