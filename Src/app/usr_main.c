/*******************************************************************************
* usr_main file.
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
#include "platform/powerctl.h"
#include "app/usr_main.h"
#include "app/commctl.h"
#include "app/memmap.h"

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

static void LoadParameters( void );
static bool_t SetNetworkParameters( void );

/*******************************************************************************
********************************************************************************
* Private memory declarations
********************************************************************************
*******************************************************************************/
unsigned gIntDisableRecursion = 0;

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

/*******************************************************************************
*
* UsrMain
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
void UsrMain( void )
{
    /* init power mode */
    UsrPower_init();

    /* Enable UART serial interface */
    CommCtl_Init();

    /* Load user parameters from flashROM */
    LoadParameters();

    /* WDG reset ? */
    if( GetResetType() == WDG_RESET )
    {
    }
    /* show configuration menu */
    else if( mTermParam.Operation == CONFIG || !HAL_GPIO_ReadPin(SW_INT_GPIO_Port, SW_INT_Pin) )
    {
        /* Start Led Flashing */
        LED_StartFlash( LED_ALL );

        // Execute configuration mode
        DoConfiguration();

        /* Stop Led Flashing */
        LED_StopFlash( LED_ALL );
        LED_TurnOff( LED_ALL );
    }

    // Execute operation mode
    DoOperation();
}

/*******************************************************************************
*
* MakeSmacParams
*
* Interface assumptions:
*     params            parameters
*
* Return value:
*     None
*
*******************************************************************************/
void MakeSmacParams( smacParams_t* params )
{
    memset( params, 0, sizeof(*params) );

    params->protocol                    = (protocolType_t)mTermParam.Protocol;

#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    params->route.hop_cnt               = mTermParam.HopCnt;
    params->route.route[0]              = mTermParam.Route[0];
    params->route.route[1]              = mTermParam.Route[1];
    params->route.route[2]              = mTermParam.Route[2];
    params->route.endNodeAddr           = mTermParam.EndId;
    params->route.oriNodeAddr           = mTermParam.SrcId;
#endif
    params->trans.addr.srcNode.panId    = mTermParam.PanId;
    params->trans.addr.srcNode.nodeAddr = mTermParam.SrcId;
    params->trans.addr.destNodeAddr     = mTermParam.DstId;
    params->trans.option.ackReq         = (mTermParam.Ack == MODE_ON);
    params->trans.option.ackRetryCount  = mTermParam.Retry;
    params->trans.option.ccaRetryCount  = 5;

    params->radio.pa.power              = mTermParam.Power;
    params->radio.rxBoost               = (mTermParam.RxBoost == MODE_ON);
    params->radio.modulation.ch         = mTermParam.Channel;
#if defined(BLD_USE_FSK_R)
    params->radio.modulation.FSK.dr     = mTermParam.Rate;
#endif

#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
    params->radio.modulation.LORA.bw    = mTermParam.Bw;
    params->radio.modulation.LORA.sf    = mTermParam.Sf;
    params->radio.modulation.LORA.cr    = CR_4_5;
#endif

    if( memcmp(mTermParam.AesKey, NullAesKey, sizeof(mTermParam.AesKey)) )
    {
        params->encryptOption.enable    = TRUE;
        params->encryptOption.aesKey    = mTermParam.AesKey;
    }
    else
    {
        params->encryptOption.enable    = FALSE;
    }
}

/*******************************************************************************
*
* GetResetType
*
* Interface assumptions:
*     None
*
* Return value:
*     Reset Type
*
*******************************************************************************/
ResetType_t GetResetType( void )
{
    static ResetType_t  type;
    static bool_t  init = FALSE;

    if( !init )
    {
        if( LL_RCC_IsActiveFlag_WWDGRST() || LL_RCC_IsActiveFlag_IWDGRST() )
        {
            type = WDG_RESET;
        }
        else if( LL_RCC_IsActiveFlag_BORRST() )
        {
            type = POR_RESET;
        }
        else if( LL_RCC_IsActiveFlag_SFTRST() )
        {
            type = SOFT_RESET;
        }
        else // if( LL_RCC_IsActiveFlag_PINRST() )
        {
            type = PIN_RESET;
        }

        init = TRUE;

        // clear reset flag
        LL_RCC_ClearResetFlags();
    }

    return type;
}

/*******************************************************************************
********************************************************************************
* Private functions
********************************************************************************
*******************************************************************************/

static void LoadParameters( void )
{
    /* Read configuration parameter from internal FlashROM */
    TermParam_loadFromRom();

    /* Set default baud */
    Serial1_SetBaudRate( mTermParam.Baudrate );

    /* Set network parameters */
    if( !SetNetworkParameters() )
    {
        // error (parameter value is invalid)

        // set parameters to default value
        TermParam_loadDefault();

        // retry setting network parameters
        SetNetworkParameters();
    }
}

static bool_t SetNetworkParameters( void )
{
    smacParams_t params;

    MakeSmacParams( &params );

    if( SMAC_Init(&params) != gErrorNoError_c )
    {
        return FALSE;
    }

    return TRUE;
}
