/*******************************************************************************
* smac_user file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#include "usr_common.h"
#include "stack/mac/smac_trans.h"
#include "stack/mac/smac_user.h"
#include "stack/mac/protocol/smac_user_protocol.h"

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
    SMAC_USER_STATE_NONE,
    SMAC_USER_STATE_IDLE,       // ProcessReq:(none)            SmacTrans:Standby or Sleep
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    SMAC_USER_STATE_TX_FORWARD, // ProcessReq:RxData            SmacTrans:Tx(forward)
#endif
    SMAC_USER_STATE_TX_DATA,    // ProcessReq:TxData            SmacTrans:Tx(data)
} SmacUserState;

/*******************************************************************************
********************************************************************************
* Private function declarations
********************************************************************************
*******************************************************************************/
static smacErrors_t StartReceive( void );
static smacErrors_t StartTransmitData( void );
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
static smacErrors_t StartForwardData( uint8_t transSduSize, const SmacTransOption_t* transOption );
#endif
static void SmacTransCallback_onNotifyRxData( smacErrors_t result, const SmacTrans_RxResultParams_t* params );
static smacUserReceiveType_t AnalyzeReceiveFrame( uint8_t sduSize, uint8_t* payloadSize, smacRouteParams_t* route );
static void SmacTransCallback_onTxDataComp( smacErrors_t result );

/*******************************************************************************
********************************************************************************
* Private memory declarations
********************************************************************************
*******************************************************************************/
static SmacUserState                g_state         = SMAC_USER_STATE_NONE;
static SmacUser_InitParams_t        g_initParams    = { (protocolType_t)0 };
static SmacUser_TxRequestParams_t   g_txReqParams;
static SmacUser_RxRequestParams_t   g_rxReqParams;
static SmacUser_RxResultParams_t    g_rxResultParams;

/*******************************************************************************
********************************************************************************
* Public functions
********************************************************************************
*******************************************************************************/

void SmacUser_Init( const SmacUser_InitParams_t* params )
{
    SMAC_USER_DEBUG_PRINT("SmacUser_Init\r\n");

    g_state      = SMAC_USER_STATE_NONE;
    g_initParams = *params;

    SmacTrans_InitParams_t transInitParams = { (protocolType_t)0 };

    // initialize lower layer
    transInitParams.protocol                = params->protocol;
    transInitParams.callback.TimerControl   = params->callback.TimerControl;
    transInitParams.callback.FiberControl   = params->callback.FiberControl;
    transInitParams.callback.onNotifyRxData = SmacTransCallback_onNotifyRxData;
    transInitParams.callback.onTxDataComp   = SmacTransCallback_onTxDataComp;
    transInitParams.rxBuffer                = params->rxBuffer;
    transInitParams.encryptOption           = params->encryptOption;

    SmacTrans_Init( &transInitParams );

    g_state = SMAC_USER_STATE_IDLE;
}

smacErrors_t SmacUser_StartRxData( const SmacUser_RxRequestParams_t* params )
{
    smacErrors_t result = gErrorNoError_c;

    SMAC_USER_DEBUG_PRINT("SmacUser_StartRxData\r\n");

    switch( g_state )
    {
    case SMAC_USER_STATE_NONE:
        return gErrorNoValidCondition_c;

#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    case SMAC_USER_STATE_TX_FORWARD:            // ProcessReq:RxData            SmacTrans:TxData(forward)
#endif
    case SMAC_USER_STATE_TX_DATA:               // ProcessReq:TxData            SmacTrans:TxData(data)
    case SMAC_USER_STATE_IDLE:                  // ProcessReq:(none)            SmacTrans:Idle
        // accept request of rx(data)
        memset( &g_rxResultParams, 0, sizeof(g_rxResultParams) );
        g_rxReqParams = *params;

        // start rx
        g_state = SMAC_USER_STATE_IDLE;

        result  = StartReceive();

        break;
    }

    return result;
}

smacErrors_t SmacUser_ReqTxData( const SmacUser_TxRequestParams_t* params )
{
    smacErrors_t result = gErrorNoError_c;

    SMAC_USER_DEBUG_PRINT("SmacUser_ReqTxData\r\n");

    switch( g_state )
    {
    case SMAC_USER_STATE_NONE:
    case SMAC_USER_STATE_TX_DATA:               // ProcessReq:TxData            SmacTrans:TxData(data)
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    case SMAC_USER_STATE_TX_FORWARD:            // ProcessReq:RxData            SmacTrans:TxData(forward)
#endif
        return gErrorNoValidCondition_c;

    case SMAC_USER_STATE_IDLE:                  // ProcessReq:(none)            SmacTrans:Idle
        // accept request of tx(data)
        g_txReqParams = *params;

        // start tx(data)
        result = StartTransmitData();
        if( result != gErrorNoError_c )
        {
            return result;
        }

        // wait for tx(data) to complete
        g_state = SMAC_USER_STATE_TX_DATA;

        break;
    }

    return result;
}

smacErrors_t SmacUser_Sleep( bool_t isWarmStart )
{
    SMAC_USER_DEBUG_PRINT("SmacUser_Sleep\r\n");

    switch( g_state )
    {
    default:
    case SMAC_USER_STATE_NONE:
        return gErrorNoValidCondition_c;

    case SMAC_USER_STATE_IDLE:                  // ProcessReq:(none)            SmacTrans:Idle
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    case SMAC_USER_STATE_TX_FORWARD:            // ProcessReq:RxData            SmacTrans:TxData(forward)
#endif
    case SMAC_USER_STATE_TX_DATA:               // ProcessReq:TxData            SmacTrans:TxData(data)
        g_state = SMAC_USER_STATE_IDLE;

        return SmacTrans_Sleep( isWarmStart );
    }
}

smacErrors_t SmacUser_Standby( void )
{
    SMAC_USER_DEBUG_PRINT("SmacUser_Standby\r\n");

    switch( g_state )
    {
    default:
    case SMAC_USER_STATE_NONE:
        return gErrorNoValidCondition_c;

    case SMAC_USER_STATE_IDLE:                  // ProcessReq:(none)            SmacTrans:Idle
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    case SMAC_USER_STATE_TX_FORWARD:            // ProcessReq:RxData            SmacTrans:TxData(forward)
#endif
    case SMAC_USER_STATE_TX_DATA:               // ProcessReq:TxData            SmacTrans:TxData(data)
        g_state = SMAC_USER_STATE_IDLE;

        return SmacTrans_Standby();
    }
}

/*******************************************************************************
********************************************************************************
* Private functions
********************************************************************************
*******************************************************************************/

static smacErrors_t StartReceive( void )
{
    SmacTrans_RxRequestParams_t transParams = { 0 };

    transParams.ownNode = g_rxReqParams.ownNode;
    transParams.radio   = g_rxReqParams.radio;

    return SmacTrans_StartRxData( &transParams );
}

static smacErrors_t StartTransmitData( void )
{
    SmacTrans_TxRequestParams_t transParams = { 0 };

    switch( g_initParams.protocol )
    {
#if defined(BLD_USE_LORA_NR)
    case Protocol_LORA_NR:
        transParams.sduSize = SmacUserFrameTotalSize_LORA_NR( g_txReqParams.payloadSize );
        break;
#endif

#if defined(BLD_USE_LORA_R)
    case Protocol_LORA_R:
        // make frame
        SmacUserFrame_makeFrame_LORA_R( &g_txReqParams.buff->phy_LORA.sdu.smac.sdu.user_R,
                                        &g_txReqParams.route,
                                        g_txReqParams.payloadSize );
        transParams.sduSize = SmacUserFrameTotalSize_LORA_R( g_txReqParams.payloadSize );
        break;
#endif

#if defined(BLD_USE_FSK_R)
    case Protocol_FSK_R:
        // make frame
        SmacUserFrame_makeFrame_FSK_R( &g_txReqParams.buff->phy_FSK.sdu.smac.sdu.user_R,
                                       &g_txReqParams.route,
                                       g_txReqParams.payloadSize );
        transParams.sduSize = SmacUserFrameTotalSize_FSK_R( g_txReqParams.payloadSize );
        break;
#endif
    }

    // request
    transParams.buff  = g_txReqParams.buff;
    transParams.trans = g_txReqParams.trans;
    transParams.radio = g_txReqParams.radio;

    return SmacTrans_ReqTxData( &transParams );
}

#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
static smacErrors_t StartForwardData( uint8_t transSduSize, const SmacTransOption_t* transOption )
{
    SmacTrans_TxRequestParams_t transParams = { 0 };
    uint16_t    destNodeAddr;

    switch( g_initParams.protocol )
    {
#if defined(BLD_USE_LORA_R)
    case Protocol_LORA_R:
        // make frame
        SmacUserFrame_forwardFrame_LORA_R( &g_initParams.rxBuffer->phy_LORA.sdu.smac.sdu.user_R, &destNodeAddr );
        break;
#endif
#if defined(BLD_USE_FSK_R)
    case Protocol_FSK_R:
        // make frame
        SmacUserFrame_forwardFrame_FSK_R( &g_initParams.rxBuffer->phy_FSK.sdu.smac.sdu.user_R, &destNodeAddr );
        break;
#endif
    default:
        break;
    }

    // request
    transParams.buff                    = g_initParams.rxBuffer;
    transParams.sduSize                 = transSduSize;
    transParams.trans.addr.srcNode      = g_rxReqParams.ownNode;
    transParams.trans.addr.destNodeAddr = destNodeAddr;
    transParams.trans.option            = *transOption;
    transParams.radio                   = g_rxReqParams.radio;

    return SmacTrans_ReqTxData( &transParams );
}
#endif

static void SmacTransCallback_onNotifyRxData( smacErrors_t result, const SmacTrans_RxResultParams_t* params )
{
    uint8_t             payloadSize;
    smacRouteParams_t   route;
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    bool_t              isRouting = FALSE;
    SmacTransOption_t   transOption;
#endif

    SMAC_USER_DEBUG_PRINT("SmacTransCallback_onNotifyRxData\r\n");

    switch( g_state )
    {
    case SMAC_USER_STATE_NONE:
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    case SMAC_USER_STATE_TX_FORWARD:            // ProcessReq:RxData            SmacTrans:TxData(forward)
#endif
    case SMAC_USER_STATE_TX_DATA:               // ProcessReq:TxData            SmacTrans:TxData(data)
        // to do nothing
        break;

    case SMAC_USER_STATE_IDLE:                  // ProcessReq:(none)            SmacTrans:Idle
        if( result != gErrorNoError_c )
        {
            // error complete
            g_initParams.callback.onNotifyRxData( result, NULL );
            break;
        }

        // check header
        switch( AnalyzeReceiveFrame(params->sduSize, &payloadSize, &route) )
        {
        case smacUserReceiveType_DISCARD:
            break;

        case smacUserReceiveType_INPUT:
            // remember result params
            g_rxResultParams.payloadSize = payloadSize;
            g_rxResultParams.route       = route;
            g_rxResultParams.trans       = *params;

            // complete rx
            g_initParams.callback.onNotifyRxData( gErrorNoError_c, &g_rxResultParams );
            break;

#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
        case smacUserReceiveType_FORWARD:
            // remember result params
            g_rxResultParams.payloadSize = payloadSize;
            g_rxResultParams.route       = route;
            g_rxResultParams.trans       = *params;

            // notify
            g_initParams.callback.onNotifyForward( &g_rxResultParams, &isRouting, &transOption );

            if( isRouting )
            {
                // wait for tx(ack) to complete
                g_state = SMAC_USER_STATE_TX_FORWARD;

                // transmit forward
                result = StartForwardData( params->sduSize, &transOption );
                if( result != gErrorNoError_c )
                {
                    g_state = SMAC_USER_STATE_IDLE;
                    g_initParams.callback.onNotifyForwardResult( result );
                }
            }
            break;
#endif
        }
        break;
    }
}

static smacUserReceiveType_t AnalyzeReceiveFrame( uint8_t sduSize, uint8_t* payloadSize, smacRouteParams_t* route )
{
    smacUserReceiveType_t receiveType = smacUserReceiveType_DISCARD;

    switch( g_initParams.protocol )
    {
#if defined(BLD_USE_LORA_NR)
    case Protocol_LORA_NR:
        *payloadSize = sduSize;
        receiveType  = smacUserReceiveType_INPUT;
        break;
#endif

#if defined(BLD_USE_LORA_R)
    case Protocol_LORA_R:
        receiveType = SmacUserFrame_analyzeFrame_LORA_R( &g_initParams.rxBuffer->phy_LORA.sdu.smac.sdu.user_R, sduSize, payloadSize, route );
        break;
#endif

#if defined(BLD_USE_FSK_R)
    case Protocol_FSK_R:
        receiveType = SmacUserFrame_analyzeFrame_FSK_R( &g_initParams.rxBuffer->phy_FSK.sdu.smac.sdu.user_R, sduSize, payloadSize, route );
        break;
#endif
    }

    return receiveType;
}

static void SmacTransCallback_onTxDataComp( smacErrors_t result )
{
    SMAC_USER_DEBUG_PRINT("SmacTransCallback_onTxDataComp\r\n");

    switch( g_state )
    {
    case SMAC_USER_STATE_NONE:
    case SMAC_USER_STATE_IDLE:                  // ProcessReq:(none)            SmacTrans:Idle
        // to do nothing
        break;

#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    case SMAC_USER_STATE_TX_FORWARD:            // ProcessReq:RxData            SmacTrans:TxData(forward)
        g_state = SMAC_USER_STATE_IDLE;
        g_initParams.callback.onNotifyForwardResult(result);
        break;
#endif

    case SMAC_USER_STATE_TX_DATA:               // ProcessReq:TxData            SmacTrans:TxData(data)
        // tx complete
        g_state = SMAC_USER_STATE_IDLE;
        g_initParams.callback.onTxDataComp(result);
        break;
    }
}
