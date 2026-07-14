/*******************************************************************************
* pendsv header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/
#ifndef _PEND_SV_
#define _PEND_SV_

#ifdef __cplusplus
    extern "C" {
#endif

/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/

// event id (bitmap)
#define PSV_EVENT_CALL_UsrTimer_start           (1<<0)
#define PSV_EVENT_CALL_UsrTimer_stop            (1<<1)
#define PSV_EVENT_LPTIM_Expired                 (1<<2)
#define PSV_EVENT_CALL_InputCommTextLine        (1<<3)
#define PSV_EVENT_CALL_InputCommBinaryBlock     (1<<4)
#define PSV_EVENT_CALL_InputCommAbort           (1<<5)
#define PSV_EVENT_SERIAL_RxDone                 (1<<6)
#define PSV_EVENT_CALL_Serial1_Write            (1<<7)
#define PSV_EVENT_UART_TxCplt                   (1<<8)
#define PSV_EVENT_CALL_UartCheckTimerCallBack   (1<<9)

/*******************************************************************************
********************************************************************************
* Public type definitions
********************************************************************************
*******************************************************************************/

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

void PendSV_SetEvent( uint32_t event );
void PendSV_ClearEvent( uint32_t event );
void PendSvProcess( void );

#ifdef __cplusplus
}
#endif

/******************************************************************************/
#endif /* _PEND_SV_ */
