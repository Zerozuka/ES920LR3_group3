/*******************************************************************************
* powerctl file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/
#include "usr_common.h"
#include "platform/powerctl.h"
#include "platform/timerctl.h"
#include "app/commctl.h"
#include "main.h"
#include "gpio.h"
#include "usart.h"

/*******************************************************************************
********************************************************************************
* Private macros
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Private type definitions
********************************************************************************
*******************************************************************************/
typedef enum PowerState_tag
{
    PowerState_Run_PLL,
    PowerState_Run_HSI,
    PowerState_Run_MSI,
    PowerState_LowPowerRun_MSI,
    PowerState_LowPowerRun_HSI,
} PowerState_t;

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

/*******************************************************************************
********************************************************************************
* Public memory declarations
********************************************************************************
*******************************************************************************/
PowerState_t    g_powerState = PowerState_Run_PLL;

/*******************************************************************************
********************************************************************************
* Public functions
********************************************************************************
*******************************************************************************/

void UsrPower_init( void )
{
    // select HSI as system clock source after Wake-Up from Stop mode
    __HAL_RCC_WAKEUPSTOP_CLK_CONFIG( RCC_STOP_WAKEUPCLOCK_HSI );

    UART_WakeUpTypeDef wakeup;
    wakeup.WakeUpEvent = UART_WAKEUP_ON_READDATA_NONEMPTY;
    HAL_UARTEx_StopModeWakeUpSourceConfig( &huart2, wakeup );
}

void UsrPower_enterActiveRun( void )
{
    switch( g_powerState )
    {
    case PowerState_Run_PLL:
        // to do nothing
        break;

    case PowerState_LowPowerRun_MSI:
        // set regulator mode to main power
        HAL_PWREx_DisableLowPowerRunMode();

        // break through

    case PowerState_Run_HSI:
    case PowerState_Run_MSI:
        // config system clock
        SystemClock_UsePLL();
        g_powerState = PowerState_Run_PLL;
        break;

    default:
        break;
    }
}

void UsrPower_enterLowPowerRun( void )
{
    switch( g_powerState )
    {
    case PowerState_Run_PLL:
    case PowerState_Run_HSI:
        // config system clock
        SystemClock_UseMSI();

        // break through

    case PowerState_Run_MSI:
        // set regulator mode to low power run
        HAL_PWREx_EnableLowPowerRunMode();
        g_powerState = PowerState_LowPowerRun_MSI;
        break;

    case PowerState_LowPowerRun_MSI:
        // to do nothing
        break;

    default:
        break;
    }
}

uint32_t UsrPower_waitEventInStopMode( uint32_t (*GetEventRoutine)(void), uint32_t baudRate )
{
    uint32_t event;
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};


    //------------------------------------
    // set clock source for low power
    //------------------------------------
    // WWDG
    __HAL_RCC_WWDG_CLK_DISABLE();

    // LPTIM
    bool_t lptimIsIdle = UsrTimer_isIdle();
    if( lptimIsIdle )
    {
        PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_LPTIM1;
        PeriphClkInit.Lptim1ClockSelection = RCC_LPTIM1CLKSOURCE_PCLK;
        HAL_RCCEx_PeriphCLKConfig( &PeriphClkInit );
    }

    //------------------------------------
    // enter stop mode
    //------------------------------------
    MX_USART2_UART_DeInit();

    HAL_PWREx_SMPS_SetMode( PWR_SMPS_STEP_DOWN );
    HAL_PWREx_EnableBORPVD_ULP();

    HAL_SuspendTick();

    usrDISABLE_INTERRUPTS();
    event = GetEventRoutine();
    while( event == 0 )
    {
        if( !lptimIsIdle )
        {
            // if low power timer became idle, disable lptimer clock
            lptimIsIdle = UsrTimer_isIdle();
            if( lptimIsIdle )
            {
                PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_LPTIM1;
                PeriphClkInit.Lptim1ClockSelection = RCC_LPTIM1CLKSOURCE_PCLK;
                HAL_RCCEx_PeriphCLKConfig( &PeriphClkInit );
            }
        }
        else
        {
            // if low power timer became running, enable lptimer clock
            lptimIsIdle = UsrTimer_isIdle();
            if( !lptimIsIdle )
            {
                PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_LPTIM1;
                PeriphClkInit.Lptim1ClockSelection = RCC_LPTIM1CLKSOURCE_LSE;
                HAL_RCCEx_PeriphCLKConfig( &PeriphClkInit );
            }
        }

        /* Clear Status Flag before entering STOP/STANDBY Mode */
        LL_PWR_ClearFlag_C1STOP_C1STB();

        // wait for interrupt in STOP Mode (cpu sleeping)
        HAL_PWREx_EnterSTOP2Mode( PWR_STOPENTRY_WFI );

        usrENABLE_INTERRUPTS();

        WDG_Refresh();

        usrDISABLE_INTERRUPTS();

        event = GetEventRoutine();

        if(__HAL_RCC_GET_SYSCLK_SOURCE() == RCC_SYSCLKSOURCE_STATUS_HSI)
        {
            g_powerState = PowerState_Run_HSI;
        }
    }
    usrENABLE_INTERRUPTS();

    //------------------------------------
    // leaved stop mode
    //------------------------------------
    HAL_ResumeTick();

    HAL_PWREx_DisableBORPVD_ULP();
    HAL_PWREx_SMPS_SetMode( PWR_SMPS_BYPASS );

    if( lptimIsIdle )
    {
        PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_LPTIM1;
        PeriphClkInit.Lptim1ClockSelection = RCC_LPTIM1CLKSOURCE_LSE;
        HAL_RCCEx_PeriphCLKConfig( &PeriphClkInit );
    }

    // WWDG
    __HAL_RCC_WWDG_CLK_ENABLE();

    /*to re-enable lost UART settings*/
    MX_USART2_UART_Init();

    MX_USART_SetBaudrate( &huart2, baudRate );

    HAL_NVIC_EnableIRQ( USART2_IRQn );

    return event;
}

uint32_t UsrPower_waitEventInCpuSleep( uint32_t (*GetEventRoutine)(void) )
{
    uint32_t event;

    //------------------------------------
    // enter cpu sleep
    //------------------------------------
    usrDISABLE_INTERRUPTS();
    event = GetEventRoutine();
    while( event == 0 )
    {
        // wait for interrupt while cpu sleeping
        __WFI();

        usrENABLE_INTERRUPTS();

        WDG_Refresh();

        usrDISABLE_INTERRUPTS();

        event = GetEventRoutine();
    }
    usrENABLE_INTERRUPTS();

    return event;
}
