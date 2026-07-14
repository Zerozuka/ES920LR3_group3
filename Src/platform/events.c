/*******************************************************************************
* events file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#include "usr_common.h"
#include "platform/events.h"

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
static volatile uint32_t    gActiveFibers = 0;

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

void SetEvent( uint32_t event )
{
    usrDISABLE_INTERRUPTS();
    {
        gEvents |= event;
    }
    usrENABLE_INTERRUPTS();
}

void ClearEvent( uint32_t event )
{
    usrDISABLE_INTERRUPTS();
    {
        gEvents &= ~event;
    }
    usrENABLE_INTERRUPTS();
}

uint32_t GetEvent( void )
{
    uint32_t event = 0;

    usrDISABLE_INTERRUPTS();
    {
        event = gEvents;
    }
    usrENABLE_INTERRUPTS();

    return event;
}

uint32_t GetEvent_IT( void )
{
    return gEvents;
}

void StartFiber( uint32_t fiber )
{
    usrDISABLE_INTERRUPTS();
    {
        gActiveFibers |= fiber;
    }
    usrENABLE_INTERRUPTS();
}

void StopFiber( uint32_t fiber )
{
    usrDISABLE_INTERRUPTS();
    {
        gActiveFibers &= ~fiber;
    }
    usrENABLE_INTERRUPTS();
}

uint32_t GetActiveFiber( void )
{
    return gActiveFibers;
}
