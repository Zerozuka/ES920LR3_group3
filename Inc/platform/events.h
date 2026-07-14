/*******************************************************************************
* events header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/
#ifndef _EVENTS_
#define _EVENTS_

#ifdef __cplusplus
    extern "C" {
#endif

/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/

// event id (bitmap)
#define EVENT_UART_RX_DONE              (1<<0)
#define EVENT_UART_TX_DONE              (1<<1)
#define EVENT_RADIO_IRQ                 (1<<2)
#define EVENT_PHY_CCA                   (1<<3)
#define EVENT_PHY_CCA_TIMER             (1<<4)
#define EVENT_PHY_TX_TIMER              (1<<5)
#define EVENT_PHY_DUTY_TIMER            (1<<6)
#define EVENT_SMAC_CCA_BACKOFF_TIMER    (1<<7)
#define EVENT_SMAC_TX_TIMER             (1<<8)
#define EVENT_WKUP_TIMER                (1<<9)
#define EVENT_WDT_RESET                 (1<<10)
#define EVENT_INT_SW                    (1<<11)
#define EVENT_SEND_TIME                 (1<<12)
#define EVENT_RSSI_CHECK                (1<<13)
#define EVENT_PHY_RX_TIMER              (1<<14)

// fiber id (bitmap)
#define FIBER_PHY_CCA                   (1<<0)

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

void        SetEvent( uint32_t event );
void        ClearEvent( uint32_t event );
uint32_t    GetEvent( void );
uint32_t    GetEvent_IT( void );

void        StartFiber( uint32_t fiber );
void        StopFiber( uint32_t fiber );
uint32_t    GetActiveFiber( void );

#ifdef __cplusplus
}
#endif

/******************************************************************************/
#endif /* _EVENTS_ */
