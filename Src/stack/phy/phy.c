/*******************************************************************************
* phy file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#include "usr_common.h"

#include "stack/phy/phy.h"
#include "stack/phy/phy_radio.h"

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

typedef enum
{
    PHY_STATE_NONE,
    PHY_STATE_SLEEP,        // ProccessReq:(none)   Radio:Sleep
    PHY_STATE_STANDBY,      // ProccessReq:(none)   Radio:Standby
    PHY_STATE_RX,           // ProccessReq:Rx       Radio:Rx
    PHY_STATE_TX,           // ProccessReq:Tx       Radio:Tx or Standby(duty)
} PhyState;

typedef enum
{
    PHY_DUTY_STATE_TX_IDLE,     // tx available
    PHY_DUTY_STATE_WAIT_DUTY,   // duty
} PhyDutyState;

/*******************************************************************************
********************************************************************************
* Private function declarations
********************************************************************************
*******************************************************************************/
#if defined(BLD_USE_FSK_R) && defined(BLD_FSK_FIXED_FORMAT)
static void           PhyRadioCallback_onSyncWordValid(void);
#endif
static void         PhyRadioCallback_onRxDone( smacErrors_t result, const PhyRadio_RxResultParams_t* params );
static void         PhyRadioCallback_onTxDone( smacErrors_t result );

static void         CancelAllReq( void );
static smacErrors_t StartReceive( void );
static smacErrors_t StartTransmit( void );

/*******************************************************************************
********************************************************************************
* Private memory declarations
********************************************************************************
*******************************************************************************/
static PhyState                 g_radioState = PHY_STATE_NONE;
static PhyDutyState             g_dutyState = PHY_DUTY_STATE_TX_IDLE;
static uint32_t                 g_dutyDuration;
static Phy_InitParams_t         g_initParams = { (protocolType_t)0 };
static Phy_RxRequestParams_t    g_rxReqParams;
static Phy_TxRequestParams_t    g_txReqParams;
static Phy_RxResultParams_t     g_rxResultParams;
extern bool_t                   g_canEnterStopMode;

/*******************************************************************************
********************************************************************************
* Public functions
********************************************************************************
*******************************************************************************/

void Phy_Init( const Phy_InitParams_t* params )
{
    PhyRadio_InitParams_t radioInitParams;

    PHY_DEBUG_PRINT("Phy_Init\r\n");

    g_radioState    = PHY_STATE_NONE;
    g_dutyState     = PHY_DUTY_STATE_TX_IDLE;
    g_dutyDuration  = 0;
    g_initParams    = *params;

    radioInitParams.protocol = params->protocol;
    radioInitParams.callback.onSyncWordValid = NULL;
#if defined(BLD_USE_FSK_R) && defined(BLD_FSK_FIXED_FORMAT)
    if( params->protocol == Protocol_FSK_R )
    {
        radioInitParams.callback.onSyncWordValid = PhyRadioCallback_onSyncWordValid;
    }
#endif
    radioInitParams.callback.onRxDone       = PhyRadioCallback_onRxDone;
    radioInitParams.callback.onTxDone       = PhyRadioCallback_onTxDone;
    radioInitParams.callback.TimerControl   = params->callback.TimerControl;
    radioInitParams.callback.FiberControl   = params->callback.FiberControl;

    PhyRadio_Init( &radioInitParams );

    g_radioState = PHY_STATE_STANDBY;
}

smacErrors_t Phy_Standby( void )
{
    smacErrors_t result = gErrorNoError_c;

    PHY_DEBUG_PRINT("Phy_Standby\r\n");

    switch( g_radioState )
    {
    default:
    case PHY_STATE_NONE:
        return gErrorNoValidCondition_c;

    case PHY_STATE_TX:
    case PHY_STATE_RX:
        CancelAllReq();

        // break through
    case PHY_STATE_SLEEP:
        g_radioState = PHY_STATE_STANDBY;
        PhyRadio_Standby();
        break;

    case PHY_STATE_STANDBY:
        // to do nothing
        break;
    }

    return result;
}

smacErrors_t Phy_Sleep( bool_t isWarmStart )
{
    smacErrors_t result = gErrorNoError_c;

    PHY_DEBUG_PRINT("Phy_Sleep\r\n");

    switch( g_radioState )
    {
    default:
    case PHY_STATE_NONE:
        return gErrorNoValidCondition_c;

    case PHY_STATE_SLEEP:
        // to do nothing
        break;

    case PHY_STATE_TX:
    case PHY_STATE_RX:
        CancelAllReq();

        // break through
    case PHY_STATE_STANDBY:
        g_radioState = PHY_STATE_SLEEP;
        PhyRadio_Sleep( isWarmStart );
        break;
    }

    return result;
}

smacErrors_t Phy_ReqRx( const Phy_RxRequestParams_t* params )
{
    smacErrors_t result = gErrorNoError_c;

    PHY_DEBUG_PRINT("Phy_ReqRx\r\n");

    switch( g_radioState )
    {
    default:
    case PHY_STATE_NONE:
        return gErrorNoValidCondition_c;

    case PHY_STATE_SLEEP:
    case PHY_STATE_TX:
    case PHY_STATE_RX:
        result = Phy_Standby();
        if( result != gErrorNoError_c )
        {
            break;
        }

        // break through
    case PHY_STATE_STANDBY:
        g_rxReqParams = *params;
        g_radioState  = PHY_STATE_RX;

        /* start receive */
        result = StartReceive();
        if( result != gErrorNoError_c )
        {
            PhyRadio_Standby();
            g_radioState = PHY_STATE_STANDBY;
            break;
        }
        break;
    }

    return result;
}

smacErrors_t Phy_ReqTx( const Phy_TxRequestParams_t* params )
{
    smacErrors_t result = gErrorNoError_c;

    PHY_DEBUG_PRINT("Phy_ReqTx\r\n");

#if defined(BLD_USE_FSK_R)
    if( !params->isRecycleBuff )
    {
        switch( g_initParams.protocol )
        {
        __cases_Protocol_FSK
            PhyFrame_makeFrame_FSK( &params->buff->phy_FSK, params->sduSize );
            break;
        default:
            break;
        }
    }
#endif

    switch( g_radioState )
    {
    default:
    case PHY_STATE_NONE:
        return gErrorNoValidCondition_c;

    case PHY_STATE_SLEEP:
    case PHY_STATE_TX:
    case PHY_STATE_RX:
        result = Phy_Standby();
        if( result != gErrorNoError_c )
        {
            break;
        }

        // break through
    case PHY_STATE_STANDBY:
        if( g_dutyState == PHY_DUTY_STATE_WAIT_DUTY )
        {
            if( params->waitDutyElapsed )
            {
                g_txReqParams = *params;
                g_radioState  = PHY_STATE_TX;

                // wait for duty fin
            }
            else
            {
                result = gErrorNoValidCondition_c;
            }
        }
        else
        {
            g_txReqParams = *params;
            g_radioState  = PHY_STATE_TX;

            /* start transmit */
            result = StartTransmit();
            if( result != gErrorNoError_c )
            {
                PhyRadio_Standby();
                g_radioState = PHY_STATE_STANDBY;
                break;
            }
        }
        break;
    }

    return result;
}

uint32_t Phy_GetTxDuty( void )
{
    return g_dutyDuration;
}

void Phy_TimerDutyFin( void )
{
    PHY_DEBUG_PRINT("Phy_TimerDutyFin\r\n");

    if( g_dutyState == PHY_DUTY_STATE_WAIT_DUTY )
    {
        g_dutyState    = PHY_DUTY_STATE_TX_IDLE;
        g_dutyDuration = 0;

        if( g_radioState == PHY_STATE_TX ) // transmit requested
        {
            smacErrors_t result;

            /* start transmit */
            result = StartTransmit();
            if( result != gErrorNoError_c )
            {
                PhyRadio_Standby();
                g_radioState = PHY_STATE_STANDBY;

                // carrier sense failed
                g_initParams.callback.onTxComp( result );
            }
        }
    }
}

/*******************************************************************************
********************************************************************************
* Private functions
********************************************************************************
*******************************************************************************/

static void CancelAllReq( void )
{
    PHY_DEBUG_PRINT("CancelAllReq\r\n");

    switch( g_radioState )
    {
    case PHY_STATE_TX:
        if( g_dutyState != PHY_DUTY_STATE_WAIT_DUTY && PhyRadio_IsTxBusy() )
        {
            g_dutyState    = PHY_DUTY_STATE_WAIT_DUTY;
            g_dutyDuration = g_txReqParams.dutyTime;
            if( g_dutyDuration == 0 )
            {
                Phy_TimerDutyFin();
            }
            else
            {
                g_initParams.callback.TimerControl( PhyDutyTimer, g_dutyDuration );
            }
        }
        break;

    case PHY_STATE_RX:
        g_canEnterStopMode = TRUE;
        break;

    default:
        break;
    }
}

#if defined(BLD_USE_FSK) && defined(BLD_FSK_FIXED_FORMAT)
static void PhyRadioCallback_onSyncWordValid( void )
{
    phyHeader_FSK_t* header;

    PHY_DEBUG_PRINT("PhyRadioCallback_onSyncWordValid\r\n");

    switch( g_radioState )
    {
    default:
    case PHY_STATE_NONE:
    case PHY_STATE_SLEEP:
    case PHY_STATE_STANDBY:
    case PHY_STATE_TX:
        break;

    case PHY_STATE_RX:
        switch( g_initParams.protocol )
        {
        __cases_Protocol_FSK
            header = &g_rxReqParams.buff->phy_FSK.header;

            // pre-read phy header(2bytes) to know sdu length
            volatile uint8_t start = HAL_GetTick();
            while( HAL_GetTick() - start < 5 )
            {
                PhyRadio_ReadRxBuffer( (uint8_t*)header, sizeof(*header) ); // 2bytes

                if( header->frameLength != 0xFF )
                {
                    PhyRadio_ModifyRxSize( sizeof(*header) + header->frameLength );
                    g_canEnterStopMode = FALSE;
                    break;
                }
            }
            break;
        }
        break;
    }
}
#endif

static void PhyRadioCallback_onRxDone( smacErrors_t result, const PhyRadio_RxResultParams_t* params )
{
    uint8_t sduSize;

    PHY_DEBUG_PRINT("PhyRadioCallback_onRxDone\r\n");

    switch( g_radioState )
    {
    default:
    case PHY_STATE_NONE:
    case PHY_STATE_SLEEP:
    case PHY_STATE_STANDBY:
    case PHY_STATE_TX:
        break;

    case PHY_STATE_RX:

        g_canEnterStopMode = TRUE;

        // rx error
        if( result != gErrorNoError_c )
        {
            g_radioState = PHY_STATE_STANDBY;

            // error complete
            g_initParams.callback.onRxComp( result, NULL );
            break;
        }

        switch( g_initParams.protocol )
        {
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
        __cases_Protocol_LORA
            sduSize = params->recvSize;
            break;
#endif

#if defined(BLD_USE_FSK_R)
        __cases_Protocol_FSK
            /* analyze frame */
            if( !PhyFrame_analyzeFrame_FSK(&g_rxReqParams.buff->phy_FSK, params->recvSize, &sduSize) )
            {
                // discard rxdata and restart rx
                result = StartReceive();
                if( result != gErrorNoError_c )
                {
                    PhyRadio_Standby();
                    g_radioState = PHY_STATE_STANDBY;

                    g_initParams.callback.onRxComp( result, NULL );
                    return;
                }
                return;
            }
            break;
#endif
        }

        g_radioState = PHY_STATE_STANDBY;

        // rx complete
        memset( &g_rxResultParams, 0, sizeof(g_rxResultParams) );
        g_rxResultParams.sduSize = sduSize;
        g_rxResultParams.radio   = *params;
        g_initParams.callback.onRxComp( gErrorNoError_c, &g_rxResultParams );
        break;
    }
}

static void PhyRadioCallback_onTxDone( smacErrors_t result )
{
    PHY_DEBUG_PRINT("PhyRadioCallback_onTxDone\r\n");

    switch( g_radioState )
    {
    default:
    case PHY_STATE_NONE:
    case PHY_STATE_SLEEP:
    case PHY_STATE_STANDBY:
    case PHY_STATE_RX:
        break;

    case PHY_STATE_TX:

        g_radioState = PHY_STATE_STANDBY;

        if( g_dutyState != PHY_DUTY_STATE_WAIT_DUTY && result != gErrorChannelBusy_c )
        {
            g_dutyState    = PHY_DUTY_STATE_WAIT_DUTY;
            g_dutyDuration = g_txReqParams.dutyTime;
            if( g_dutyDuration == 0 )
            {
                Phy_TimerDutyFin();
            }
            else
            {
                g_initParams.callback.TimerControl( PhyDutyTimer, g_dutyDuration );
            }
        }

        // complete
        g_initParams.callback.onTxComp( result );
        break;
    }
}

static smacErrors_t StartReceive( void )
{
    PhyRadio_RxParams_t radioParams;

#if defined(BLD_USE_FSK_R)
    phyHeader_FSK_t*    header;
    switch( g_initParams.protocol )
    {
    __cases_Protocol_FSK
        header = &g_rxReqParams.buff->phy_FSK.header;

        // fill dummy data
        memset( header, 0xFF, sizeof(*header) );    // 2bytes
        PhyRadio_WriteRxBuffer( (uint8_t*)header, sizeof(*header) );
        break;
    default:
        break;
    }
#endif
    memset( &radioParams, 0, sizeof(radioParams) );
    radioParams.buff    = g_rxReqParams.buff->data;
    radioParams.size    = sizeof(*g_rxReqParams.buff);
    radioParams.radio   = g_rxReqParams.radio;

    return PhyRadio_Rx( &radioParams );
}

static smacErrors_t StartTransmit( void )
{
    PhyRadio_TxParams_t radioParams;

    memset( &radioParams, 0, sizeof(radioParams) );

    switch( g_initParams.protocol )
    {
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
    __cases_Protocol_LORA
        radioParams.size    = PhyFrameTotalSize_LORA(g_txReqParams.sduSize);
        break;
#endif

#if defined(BLD_USE_FSK_R)
    __cases_Protocol_FSK
        radioParams.size    = PhyFrameTotalSize_FSK(g_txReqParams.sduSize);
        break;
#endif
    }

    radioParams.buff    = g_txReqParams.buff->data;
    radioParams.radio   = g_txReqParams.radio;
    radioParams.doCca   = g_txReqParams.doCca;
    radioParams.ccaTime = g_txReqParams.ccaTime;

    return PhyRadio_Tx( &radioParams );
}
