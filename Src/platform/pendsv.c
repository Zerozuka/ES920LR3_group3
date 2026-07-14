/*******************************************************************************
* pendsv file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#include "usr_common.h"
#include "platform/pendsv.h"
#include "platform/timerctl.h"
#include "app/commctl.h"

/*******************************************************************************
********************************************************************************
* Private macros
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Private prototypes
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Private memory declarations
********************************************************************************
*******************************************************************************/
static volatile uint32_t    gEvents = 0;

/*******************************************************************************
********************************************************************************
* Public memory declarations
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Public functions
********************************************************************************
*******************************************************************************/

void PendSV_SetEvent( uint32_t event )
{
    usrDISABLE_INTERRUPTS();
    {
        gEvents |= event;

        SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
    }
    usrENABLE_INTERRUPTS();
}

void PendSV_ClearEvent( uint32_t event )
{
    usrDISABLE_INTERRUPTS();
    {
        gEvents &= ~event;
    }
    usrENABLE_INTERRUPTS();
}

void PendSvProcess( void )
{
    uint32_t event = 0;

    for(;;)
    {
        usrDISABLE_INTERRUPTS();
        {
            event = gEvents;
            gEvents = 0;
        }
        usrENABLE_INTERRUPTS();

        if( event == 0 )
        {
            break;
        }

        if( event & PSV_EVENT_SERIAL_RxDone )
        {
            PendSvProcess_SERIAL_RxDone();
        }
        if( event & PSV_EVENT_LPTIM_Expired )
        {
            PendSvProcess_LPTIM_Expired();
        }
        if( event & PSV_EVENT_UART_TxCplt )
        {
            PendSvProcess_UART_TxCplt();
        }
        if( event & PSV_EVENT_CALL_UsrTimer_start )
        {
            PendSvProcess_UsrTimer_start();
        }
        if( event & PSV_EVENT_CALL_UsrTimer_stop )
        {
            PendSvProcess_UsrTimer_stop();
        }
        if( event & PSV_EVENT_CALL_InputCommTextLine )
        {
            PendSvProcess_InputCommTextLine();
        }
        if( event & PSV_EVENT_CALL_InputCommBinaryBlock )
        {
            PendSvProcess_InputCommBinaryBlock();
        }
        if( event & PSV_EVENT_CALL_InputCommAbort )
        {
            PendSvProcess_InputCommAbort();
        }
        if( event & PSV_EVENT_CALL_Serial1_Write )
        {
            PendSvProcess_Serial1_Write();
        }
        if( event & PSV_EVENT_CALL_UartCheckTimerCallBack )
        {
            PendSvProcess_UartCheckTimerCallBack();
        }
    }
}
