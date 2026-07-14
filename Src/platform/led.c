/*******************************************************************************
* led file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#include "usr_common.h"
#include "platform/led.h"
#include "platform/timerctl.h"

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
static void LED_TimerCallback( UsrTimerId timerId );

/*******************************************************************************
********************************************************************************
* Private memory declarations
********************************************************************************
*******************************************************************************/
static LED_t    flashLED    = LED_None;

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

void LED_TurnOn( LED_t type )
{
    if( type & LED1 )
    {
        HAL_GPIO_WritePin( LED_T1_GPIO_Port, LED_T1_Pin, GPIO_PIN_RESET );
    }
    if( type & LED2 )
    {
        HAL_GPIO_WritePin( LED_T0_GPIO_Port, LED_T0_Pin, GPIO_PIN_RESET );
    }
}

void LED_TurnOff( LED_t type )
{
    if( type & LED1 )
    {
        HAL_GPIO_WritePin( LED_T1_GPIO_Port, LED_T1_Pin, GPIO_PIN_SET );
    }
    if( type & LED2 )
    {
        HAL_GPIO_WritePin( LED_T0_GPIO_Port, LED_T0_Pin, GPIO_PIN_SET );
    }
}

void LED_Toggle( LED_t type )
{
    if( type & LED1 )
    {
        HAL_GPIO_TogglePin( LED_T1_GPIO_Port, LED_T1_Pin );
    }
    if( type & LED2 )
    {
        HAL_GPIO_TogglePin( LED_T0_GPIO_Port, LED_T0_Pin );
    }
}

void LED_StartFlash( LED_t type )
{
    LED_t prev = flashLED;

    flashLED |= (type & LED_ALL);

    if( (prev == LED_None) && (type != LED_None) )
    {
        UsrTimer_start( UsrLedTimer, UsrTimerMode_Periodic, LED_INTERVAL, LED_TimerCallback );
    }
}

void LED_StopFlash( LED_t type )
{
    LED_t prev = flashLED;

    flashLED &= ~(type & LED_ALL);

    if( (prev != LED_None) && (flashLED == LED_None) )
    {
        UsrTimer_stop( UsrLedTimer );
    }

    flashLED &= ~(type & LED_ALL);
}

/*******************************************************************************
********************************************************************************
* Private functions
********************************************************************************
*******************************************************************************/

static void LED_TimerCallback( UsrTimerId timerId )
{
    LED_Toggle( flashLED );
}
