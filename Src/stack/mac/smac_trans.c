/*******************************************************************************
* smac_trans file.
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
#include "stack/phy/phy_arib.h"
#include "stack/mac/smac_trans.h"
#include "stack/mac/protocol/smac_trans_protocol.h"

/*******************************************************************************
********************************************************************************
* Private macros
********************************************************************************
*******************************************************************************/
#define RETRANS_BACKOFF() ( RETRANS_BACKOFF_MIN + ( PhyRadio_GetRandom() % (RETRANS_BACKOFF_MAX - RETRANS_BACKOFF_MIN) ) )

/*******************************************************************************
********************************************************************************
* Private type definitions
********************************************************************************
*******************************************************************************/
typedef enum
{
    SMAC_TRANS_STATE_NONE,
    SMAC_TRANS_STATE_ENTER_IDLE,        // ProcessReq:(none)               Phy:Standby or Sleep  Radio:Standby or Sleep
    SMAC_TRANS_STATE_IDLE,              // ProcessReq:(none)               Phy:Standby or Sleep  Radio:Standby or Sleep
    SMAC_TRANS_STATE_TX_DATA,           // ProcessReq:TxData  TimerActive  Phy:Tx(data)          Radio:Tx or Standby(duty)
    SMAC_TRANS_STATE_WAIT_ACK,          // ProcessReq:TxData  TimerActive  Phy:Rx                Radio:Rx
    SMAC_TRANS_STATE_TX_ACK,            // ProcessReq:(none)               Phy:Tx(ack)           Radio:Tx or Standby(duty)
} SmacTransState;

/*******************************************************************************
********************************************************************************
* Private function declarations
********************************************************************************
*******************************************************************************/
static smacErrors_t StartReceiveData( void );
static smacErrors_t StartReceiveAck( void );
static void PhyCallback_RxComp( smacErrors_t result, const Phy_RxResultParams_t* params );
static smacReceiveType_t AnalyzeReceiveFrame( packet_t* rxBuffer, uint8_t recvSize, unsigned seqNoOfLastTxData, smacAddrInfo_t ownNode, uint8_t* sduSize, SmacTransAddrInfo_t* addr, bool_t* ackReq );
static void SmacTrans_IdleProcess( void );
static smacErrors_t StartTransmitData( void );
static smacErrors_t StartRetransmitData( void );
static smacErrors_t StartTransmitAck( void );
static void PhyCallback_TxComp( smacErrors_t result );

/*******************************************************************************
********************************************************************************
* Private memory declarations
********************************************************************************
*******************************************************************************/
static SmacTransState               g_state     = SMAC_TRANS_STATE_NONE;
static SmacTransIdleMode_t          g_idleMode  = SmacTransIdleMode_STANDBY;
static SmacTrans_InitParams_t       g_initParams = { (protocolType_t)0 };
static SmacTrans_TxRequestParams_t  g_txReqParams;
static SmacTrans_RxRequestParams_t  g_rxReqParams;
static SmacTrans_RxResultParams_t   g_rxResultParams;
static bool_t                       g_isWarmSleep;
static packet_t                     g_ackBuff;
static uint8_t                      g_seqNo;
static bool_t                       g_enableAckRetrans;
static Phy_TxRequestParams_t        g_phyTxReqParams = { 0 };
static uint32_t                     g_timeout;
static uint8_t                      g_ackRetransCnt;
static uint8_t                      g_ccaRetransCnt;
static aes_context                  g_aesContext;

/*******************************************************************************
********************************************************************************
* Public functions
********************************************************************************
*******************************************************************************/

void SmacTrans_Init( const SmacTrans_InitParams_t* params )
{
    SMAC_TRANS_DEBUG_PRINT("SmacTrans_Init\r\n");

    g_state      = SMAC_TRANS_STATE_NONE;
    g_initParams = *params;

    Phy_InitParams_t phyInitParams = { (protocolType_t)0 };

    // initialize lower layer
    phyInitParams.protocol              = params->protocol;
    phyInitParams.callback.TimerControl = params->callback.TimerControl;
    phyInitParams.callback.FiberControl = params->callback.FiberControl;
    phyInitParams.callback.onRxComp     = PhyCallback_RxComp;
    phyInitParams.callback.onTxComp     = PhyCallback_TxComp;
    Phy_Init( &phyInitParams );

    // initialize aes context
    if( params->encryptOption.enable )
    {
        memset( g_aesContext.ksch, 0, 240 );
        aes_set_key( params->encryptOption.aesKey, SMAC_ENCRYPT_KEY_LENGTH, &g_aesContext );
    }

    g_seqNo    = (uint8_t) PhyRadio_GetRandom();
    g_state    = SMAC_TRANS_STATE_IDLE;
    g_idleMode = SmacTransIdleMode_STANDBY;
}

smacErrors_t SmacTrans_StartRxData( const SmacTrans_RxRequestParams_t* params )
{
    smacErrors_t result = gErrorNoError_c;

    SMAC_TRANS_DEBUG_PRINT("SmacTrans_StartRxData\r\n");

    switch( g_state )
    {
    case SMAC_TRANS_STATE_NONE:
        return gErrorNoValidCondition_c;

    case SMAC_TRANS_STATE_TX_DATA:          // ProcessReq:TxData            Phy:Tx(data)            Radio:Tx or Standby(duty)
    case SMAC_TRANS_STATE_WAIT_ACK:         // ProcessReq:TxData            Phy:Rx                  Radio:Rx
    case SMAC_TRANS_STATE_TX_ACK:           // ProcessReq:(none)            Phy:Tx(ack)             Radio:Tx or Standby(duty)
        g_initParams.callback.TimerControl( SmacTxTimer, 0 );
        g_initParams.callback.TimerControl( SmacCcaBackoffTimer, 0 );

        // break through
    case SMAC_TRANS_STATE_ENTER_IDLE:
    case SMAC_TRANS_STATE_IDLE:             // ProcessReq:(none)            Phy:Standby or Sleep    Radio:Standby or Sleep
        // accept request of rx(data)
        memset( &g_rxResultParams, 0, sizeof(g_rxResultParams) );
        g_rxReqParams = *params;

        g_state = SMAC_TRANS_STATE_IDLE;
        g_idleMode = SmacTransIdleMode_RX;

        // start rx
        result = StartReceiveData();

        break;
    }

    return result;
}

smacErrors_t SmacTrans_ReqTxData( const SmacTrans_TxRequestParams_t* params )
{
    smacErrors_t result = gErrorNoError_c;

    SMAC_TRANS_DEBUG_PRINT("SmacTrans_ReqTxData\r\n");

    switch( g_state )
    {
    case SMAC_TRANS_STATE_NONE:
    case SMAC_TRANS_STATE_TX_DATA:          // ProcessReq:TxData            Phy:Tx(data)            Radio:Tx or Standby(duty)
    case SMAC_TRANS_STATE_TX_ACK:           // ProcessReq:(none)            Phy:Tx(ack)             Radio:Tx or Standby(duty)
    case SMAC_TRANS_STATE_WAIT_ACK:         // ProcessReq:TxData            Phy:Rx                  Radio:Rx
        return gErrorNoValidCondition_c;

    case SMAC_TRANS_STATE_ENTER_IDLE:
    case SMAC_TRANS_STATE_IDLE:             // ProcessReq:(none)            Phy:Standby or Sleep    Radio:Standby or Sleep
        // accept request of tx(data)
        g_txReqParams = *params;

        g_ackRetransCnt = 0;
        g_ccaRetransCnt = 0;

        g_state = SMAC_TRANS_STATE_TX_DATA;

        // start tx(data)
        result = StartTransmitData();
        if( (result == gErrorChannelBusy_c) && (g_ccaRetransCnt++ < g_txReqParams.trans.option.ccaRetryCount) )
        {
            // cca retransmit
            g_initParams.callback.TimerControl( SmacCcaBackoffTimer, RETRANS_BACKOFF() );
            result = gErrorNoError_c;
        }
        else if( result != gErrorNoError_c )
        {
            g_state = SMAC_TRANS_STATE_ENTER_IDLE;
            SmacTrans_IdleProcess();
            return result;
        }

        // wait for tx(data) to complete
        g_state = SMAC_TRANS_STATE_TX_DATA;
        break;
    }

    return result;
}

smacErrors_t SmacTrans_Standby( void )
{
    SMAC_TRANS_DEBUG_PRINT("SmacTrans_Standby\r\n");

    switch( g_state )
    {
    default:
    case SMAC_TRANS_STATE_NONE:
        return gErrorNoValidCondition_c;

    case SMAC_TRANS_STATE_ENTER_IDLE:
    case SMAC_TRANS_STATE_IDLE:             // ProcessReq:(none)            Phy:Standby or Sleep    Radio:Standby or Sleep
    case SMAC_TRANS_STATE_TX_ACK:           // ProcessReq:(none)            Phy:Tx(ack)             Radio:Tx or Standby(duty)
    case SMAC_TRANS_STATE_TX_DATA:          // ProcessReq:TxData            Phy:Tx(data)            Radio:Tx or Standby(duty)
    case SMAC_TRANS_STATE_WAIT_ACK:         // ProcessReq:TxData            Phy:Rx                  Radio:Rx
        g_initParams.callback.TimerControl( SmacTxTimer, 0 );
        g_initParams.callback.TimerControl( SmacCcaBackoffTimer, 0 );
        g_state    = SMAC_TRANS_STATE_IDLE;
        g_idleMode = SmacTransIdleMode_STANDBY;

        return Phy_Standby();
    }
}

smacErrors_t SmacTrans_Sleep( bool_t isWarmStart )
{
    SMAC_TRANS_DEBUG_PRINT("SmacTrans_Sleep\r\n");

    switch( g_state )
    {
    default:
    case SMAC_TRANS_STATE_NONE:
        return gErrorNoValidCondition_c;

    case SMAC_TRANS_STATE_ENTER_IDLE:
    case SMAC_TRANS_STATE_IDLE:             // ProcessReq:(none)            Phy:Standby or Sleep    Radio:Standby or Sleep
    case SMAC_TRANS_STATE_TX_ACK:           // ProcessReq:(none)            Phy:Tx(ack)             Radio:Tx or Standby(duty)
    case SMAC_TRANS_STATE_TX_DATA:          // ProcessReq:TxData            Phy:Tx(data)            Radio:Tx or Standby(duty)
    case SMAC_TRANS_STATE_WAIT_ACK:         // ProcessReq:TxData            Phy:Rx                  Radio:Rx
        g_initParams.callback.TimerControl( SmacTxTimer, 0 );
        g_initParams.callback.TimerControl( SmacCcaBackoffTimer, 0 );
        g_state       = SMAC_TRANS_STATE_IDLE;
        g_idleMode    = SmacTransIdleMode_SLEEP;
        g_isWarmSleep = isWarmStart;

        return Phy_Sleep( isWarmStart );
    }
}

smacErrors_t SmacTrans_GetIdleMode( SmacTransIdleMode_t* mode )
{
    switch( g_state )
    {
    default:
    case SMAC_TRANS_STATE_NONE:
        return gErrorNoValidCondition_c;

    case SMAC_TRANS_STATE_ENTER_IDLE:
    case SMAC_TRANS_STATE_IDLE:             // ProcessReq:(none)            Phy:Standby or Sleep    Radio:Standby or Sleep
    case SMAC_TRANS_STATE_TX_ACK:           // ProcessReq:(none)            Phy:Tx(ack)             Radio:Tx or Standby(duty)
    case SMAC_TRANS_STATE_TX_DATA:          // ProcessReq:TxData            Phy:Tx(data)            Radio:Tx or Standby(duty)
    case SMAC_TRANS_STATE_WAIT_ACK:         // ProcessReq:TxData            Phy:Rx                  Radio:Rx
        *mode = g_idleMode = SmacTransIdleMode_SLEEP;
        return gErrorNoError_c;
    }
}

void SmacTrans_CcaBackoffTimeout( void )
{
    smacErrors_t result = gErrorNoError_c;
    bool_t idleProcess = FALSE;

    SMAC_TRANS_DEBUG_PRINT("SmacTrans_CcaBackoffTimeout\r\n");

    switch( g_state )
    {
    case SMAC_TRANS_STATE_NONE:
    case SMAC_TRANS_STATE_ENTER_IDLE:
    case SMAC_TRANS_STATE_IDLE:             // ProcessReq:(none)            Phy:Standby or Sleep    Radio:Standby or Sleep
    case SMAC_TRANS_STATE_TX_ACK:           // ProcessReq:(none)            Phy:Tx(ack)             Radio:Tx or Standby(duty)
    case SMAC_TRANS_STATE_WAIT_ACK:         // ProcessReq:TxData            TimerActive     Phy:Rx  Radio:Rx
        // to do nothing
        break;

    case SMAC_TRANS_STATE_TX_DATA:          // ProcessReq:TxData            TimerActive     Phy:Tx(data)    Radio:Tx or Standby(duty)

        // retransmit tx(data)
        result = StartRetransmitData();
        if( (result == gErrorChannelBusy_c) && (g_ccaRetransCnt++ < g_txReqParams.trans.option.ccaRetryCount) )
        {
            g_initParams.callback.TimerControl( SmacCcaBackoffTimer, RETRANS_BACKOFF() );
        }
        else if( result != gErrorNoError_c )
        {
            Phy_Standby();

            // error complete
            g_state     = SMAC_TRANS_STATE_ENTER_IDLE;
            idleProcess = TRUE;
            g_initParams.callback.onTxDataComp( result );
        }

        if( idleProcess )
        {
            SmacTrans_IdleProcess();
        }
        break;
    }
}

void SmacTrans_TxDataTimeout( void )
{
    smacErrors_t result = gErrorNoError_c;
    bool_t idleProcess = FALSE;

    SMAC_TRANS_DEBUG_PRINT("SmacTrans_TxDataTimeout\r\n");

    switch( g_state )
    {
    case SMAC_TRANS_STATE_NONE:
    case SMAC_TRANS_STATE_ENTER_IDLE:
    case SMAC_TRANS_STATE_IDLE:             // ProcessReq:(none)                Phy:Standby or Sleep    Radio:Standby or Sleep
    case SMAC_TRANS_STATE_TX_ACK:           // ProcessReq:(none)                Phy:Tx(ack)             Radio:Tx or Standby(duty)
        // to do nothing
        break;

    case SMAC_TRANS_STATE_TX_DATA:          // ProcessReq:TxData    TimerActive Phy:Tx(data)            Radio:Tx or Standby(duty)
    case SMAC_TRANS_STATE_WAIT_ACK:         // ProcessReq:TxData    TimerActive Phy:Rx                  Radio:Rx

        if( g_enableAckRetrans && (g_ackRetransCnt++ < g_txReqParams.trans.option.ackRetryCount) )
        {
            g_state = SMAC_TRANS_STATE_TX_DATA;

            g_ccaRetransCnt = 0;

            // retransmit tx(data)
            result = StartRetransmitData();
            if( result != gErrorNoError_c )
            {
                Phy_Standby();

                // error complete
                g_state     = SMAC_TRANS_STATE_ENTER_IDLE;
                idleProcess = TRUE;
                g_initParams.callback.onTxDataComp( result );
            }
        }
        else
        {
            Phy_Standby();

            if( g_state == SMAC_TRANS_STATE_WAIT_ACK )
            {
                result = gErrorNoAck_c;
            }
            else
            {
                result = gErrorTimeout_c;
            }

            // error complete
            g_state     = SMAC_TRANS_STATE_ENTER_IDLE;
            idleProcess = TRUE;
            g_initParams.callback.onTxDataComp( result );
        }

        if( idleProcess )
        {
            SmacTrans_IdleProcess();
        }
        break;
    }
}

/*******************************************************************************
********************************************************************************
* Private functions
********************************************************************************
*******************************************************************************/

static smacErrors_t StartReceiveData( void )
{
    Phy_RxRequestParams_t phyParams = { 0 };

    phyParams.buff  = g_initParams.rxBuffer;
    phyParams.radio = g_rxReqParams.radio;

    return Phy_ReqRx( &phyParams );
}

static smacErrors_t StartReceiveAck( void )
{
    Phy_RxRequestParams_t phyParams = { 0 };

    phyParams.buff  = &g_ackBuff;
    phyParams.radio = g_txReqParams.radio;

    return Phy_ReqRx( &phyParams );
}

static void PhyCallback_RxComp( smacErrors_t result, const Phy_RxResultParams_t* params )
{
    uint8_t             sduSize;
    SmacTransAddrInfo_t addrInfo;
    bool_t              ackReq;
    bool_t              idleProcess = FALSE;

    SMAC_TRANS_DEBUG_PRINT("PhyCallback_RxComp\r\n");

    switch( g_state )
    {
    case SMAC_TRANS_STATE_NONE:
    case SMAC_TRANS_STATE_TX_ACK:           // ProcessReq:(none)            Phy:Tx(ack)             Radio:Tx or Standby(duty)
    case SMAC_TRANS_STATE_TX_DATA:          // ProcessReq:TxData            Phy:Tx(data)            Radio:Tx or Standby(duty)
        // to do nothing
        break;

    case SMAC_TRANS_STATE_ENTER_IDLE:
    case SMAC_TRANS_STATE_IDLE:             // ProcessReq:(none)            Phy:Standby or Sleep    Radio:Standby or Sleep
        switch( g_idleMode )
        {
        case SmacTransIdleMode_STANDBY:
        case SmacTransIdleMode_SLEEP:
            // to do nothing
            break;

        case SmacTransIdleMode_RX:
            if( result != gErrorNoError_c )
            {
                // error complete
                g_state     = SMAC_TRANS_STATE_ENTER_IDLE;
                idleProcess = TRUE;

                g_initParams.callback.onNotifyRxData( result, NULL );
                break;
            }

            // check header
            switch( AnalyzeReceiveFrame( g_initParams.rxBuffer,
                                          params->sduSize,
                                          INVALID_SEQNO,
                                          g_rxReqParams.ownNode,
                                          &sduSize,
                                          &addrInfo,
                                          &ackReq ) )
            {
            case smacReceiveType_DISCARD:
            case smacReceiveType_ACK_TO_OWN:
                // restart rx
                g_state     = SMAC_TRANS_STATE_ENTER_IDLE;
                idleProcess = TRUE;
                break;

            case smacReceiveType_DATA_TO_OWN:
                // remember result params
                g_rxResultParams.sduSize = sduSize;
                g_rxResultParams.addr    = addrInfo;
                g_rxResultParams.ackReq  = IS_ACK_REQ(ackReq, &addrInfo);
                g_rxResultParams.phy     = *params;

                // ack reqested
                if( g_rxResultParams.ackReq )
                {
                    // wait for tx(ack) to complete
                    g_state = SMAC_TRANS_STATE_TX_ACK;

                    // transmit ack
                    result = StartTransmitAck();
                    if( result != gErrorNoError_c )
                    {
                        // failed
                        g_state     = SMAC_TRANS_STATE_ENTER_IDLE;
                        idleProcess = TRUE;
                        break;
                    }
                }
                // no ack
                else
                {
                    g_state     = SMAC_TRANS_STATE_ENTER_IDLE;
                    idleProcess = TRUE;

                    // complete rx
                    g_initParams.callback.onNotifyRxData( gErrorNoError_c, &g_rxResultParams );
                }
                break;
            }
            break;
        }
        break;

    case SMAC_TRANS_STATE_WAIT_ACK:             // ProcessReq:TxData            Phy:Rx          Radio:Rx
        if( result != gErrorNoError_c )
        {
            // continue rx ack
            result = StartReceiveAck();
            if(result != gErrorNoError_c)
            {
                // stop timer
                g_initParams.callback.TimerControl(SmacTxTimer, 0);
                g_initParams.callback.TimerControl(SmacCcaBackoffTimer, 0);

                // error complete
                g_state = SMAC_TRANS_STATE_ENTER_IDLE;
                idleProcess = TRUE;
                g_initParams.callback.onTxDataComp(result);
            }
        }
        else
        {
            // check header
            switch( AnalyzeReceiveFrame( &g_ackBuff,
                                          params->sduSize,
                                          g_seqNo,
                                          g_txReqParams.trans.addr.srcNode,
                                          &sduSize,
                                          &addrInfo,
                                          &ackReq ) )
            {
            case smacReceiveType_DISCARD:
            case smacReceiveType_DATA_TO_OWN:
                // restart rx ack
                result = StartReceiveAck();
                if( result != gErrorNoError_c )
                {
                    // stop timer
                    g_initParams.callback.TimerControl( SmacTxTimer, 0 );
                    g_initParams.callback.TimerControl( SmacCcaBackoffTimer, 0 );

                    // error complete
                    g_state     = SMAC_TRANS_STATE_ENTER_IDLE;
                    idleProcess = TRUE;
                    g_initParams.callback.onTxDataComp( result );
                }
                break;

            case smacReceiveType_ACK_TO_OWN:

                // stop timer
                g_initParams.callback.TimerControl( SmacTxTimer, 0 );
                g_initParams.callback.TimerControl( SmacCcaBackoffTimer, 0 );

                // tx complete
                g_state     = SMAC_TRANS_STATE_ENTER_IDLE;
                idleProcess = TRUE;
                g_initParams.callback.onTxDataComp( gErrorNoError_c );
                break;
            }
        }
        break;
    }

    if( idleProcess )
    {
        SmacTrans_IdleProcess();
    }
}

static smacReceiveType_t AnalyzeReceiveFrame( packet_t* rxBuffer, uint8_t recvSize, unsigned seqNoOfLastTxData, smacAddrInfo_t ownNode, uint8_t* sduSize, SmacTransAddrInfo_t* addr, bool_t* ackReq )
{
    smacReceiveType_t recvType = smacReceiveType_DISCARD;

    switch( g_initParams.protocol )
    {
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
    __cases_Protocol_LORA
        recvType = SmacFrame_analyzeFrame_LORA( &rxBuffer->phy_LORA.sdu.smac,
                                                recvSize, seqNoOfLastTxData,
                                                ownNode, sduSize, addr, ackReq,
                                                g_initParams.encryptOption.enable ? &g_aesContext : NULL );
        break;
#endif
#if defined(BLD_USE_FSK_R)
    __cases_Protocol_FSK
        recvType = SmacFrame_analyzeFrame_FSK( &rxBuffer->phy_FSK.sdu.smac,
                                               recvSize, seqNoOfLastTxData,
                                               ownNode, sduSize, addr, ackReq,
                                               g_initParams.encryptOption.enable ? &g_aesContext : NULL );
        break;
#endif
    }

    return recvType;
}

static void SmacTrans_IdleProcess( void )
{
    smacErrors_t result = gErrorNoError_c;

    switch( g_state )
    {
    case SMAC_TRANS_STATE_ENTER_IDLE:
        switch( g_idleMode )
        {
        case SmacTransIdleMode_STANDBY:
            Phy_Standby();
            break;

        case SmacTransIdleMode_SLEEP:
            Phy_Sleep( g_isWarmSleep );
            break;

        case SmacTransIdleMode_RX:
            result = StartReceiveData();
            if( result != gErrorNoError_c )
            {
                // error complete
                g_initParams.callback.onNotifyRxData( result, NULL );
            }
            break;
        }
        g_state = SMAC_TRANS_STATE_IDLE;
        break;

    default:
        break;
    }
}

static smacErrors_t StartTransmitData( void )
{
    smacErrors_t result = gErrorNoError_c;
    uint32_t txDuration;
    uint32_t ackTxDuration;
    AribTimeInfo_t arib;

    const bool_t isAckReq = IS_ACK_REQ(g_txReqParams.trans.option.ackReq, &g_txReqParams.trans.addr);

    switch( g_initParams.protocol )
    {
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
    __cases_Protocol_LORA
        g_phyTxReqParams.sduSize = SmacFrameTotalSize_LORA( g_txReqParams.sduSize );

        // calc tx duration
        txDuration = Phy_TimeOnAir_LORA( &g_txReqParams.radio.modulation, g_phyTxReqParams.sduSize );
        if( isAckReq )
        {
            ackTxDuration = Phy_TimeOnAir_LORA( &g_txReqParams.radio.modulation, SmacFrameTotalSize_ACK_LORA );
        }

        // check tx duration, cca time and tx duty time
        result = PhyArib_CheckTimes( g_initParams.protocol, &g_txReqParams.radio.modulation, txDuration, &arib );
        if( result != gErrorNoError_c )
        {
            return result;
        }

        // make frame
        SmacFrame_makeDataFrame_LORA( &g_txReqParams.buff->phy_LORA.sdu.smac,
                                      &g_txReqParams.trans.addr,
                                      isAckReq,
                                      ++g_seqNo,
                                      g_txReqParams.sduSize,
                                      g_initParams.encryptOption.enable ? &g_aesContext : NULL );
        break;
#endif

#if defined(BLD_USE_FSK_R)
    __cases_Protocol_FSK
        g_phyTxReqParams.sduSize = SmacFrameTotalSize_FSK( g_txReqParams.sduSize );

        // calc tx duration
        txDuration = Phy_TimeOnAir_FSK( &g_txReqParams.radio.modulation, g_phyTxReqParams.sduSize );
        if( isAckReq )
        {
            ackTxDuration = Phy_TimeOnAir_FSK( &g_txReqParams.radio.modulation, SmacFrameTotalSize_ACK_FSK );
        }

        // check tx duration, cca time and tx duty time
        result = PhyArib_CheckTimes( g_initParams.protocol, &g_txReqParams.radio.modulation, txDuration, &arib );
        if( result != gErrorNoError_c )
        {
            return result;
        }

        // make frame
        SmacFrame_makeDataFrame_FSK( &g_txReqParams.buff->phy_FSK.sdu.smac,
                                     &g_txReqParams.trans.addr,
                                     isAckReq,
                                     ++g_seqNo,
                                     g_txReqParams.sduSize,
                                     g_initParams.encryptOption.enable ? &g_aesContext : NULL );
        break;
#endif
    }

    // calc timeout
    g_timeout = Phy_GetTxDuty() + arib.ccaTime + txDuration + TIMEOUT_MARGIN;

    if( isAckReq )
    {
        g_timeout += arib.txDutyTimeMax + arib.ccaTime + ackTxDuration + TIMEOUT_MARGIN;
    }

    // start timer
    g_initParams.callback.TimerControl( SmacTxTimer, g_timeout );

    // start tx
    g_phyTxReqParams.buff            = g_txReqParams.buff;
    g_phyTxReqParams.isRecycleBuff   = FALSE;
    g_phyTxReqParams.radio           = g_txReqParams.radio;
    g_phyTxReqParams.doCca           = TRUE;
    g_phyTxReqParams.waitDutyElapsed = TRUE;
    g_phyTxReqParams.ccaTime         = arib.ccaTime;
    g_phyTxReqParams.dutyTime        = arib.txDutyTime;
    result = Phy_ReqTx( &g_phyTxReqParams );
    if( result != gErrorNoError_c )
    {
        // stop timer
        g_initParams.callback.TimerControl( SmacTxTimer, 0 );
        g_initParams.callback.TimerControl( SmacCcaBackoffTimer, 0 );
        return result;
    }

    g_enableAckRetrans = isAckReq;

    return result;
}

static smacErrors_t StartRetransmitData( void )
{
    smacErrors_t result = gErrorNoError_c;

    // request
    g_phyTxReqParams.isRecycleBuff = TRUE;

    // start timer
    g_initParams.callback.TimerControl( SmacTxTimer, g_timeout );

    // start tx
    result = Phy_ReqTx( &g_phyTxReqParams );
    if( result != gErrorNoError_c )
    {
        g_initParams.callback.TimerControl( SmacTxTimer, 0 );
        g_initParams.callback.TimerControl( SmacCcaBackoffTimer, 0 );
        return result;
    }

    return result;
}

static smacErrors_t StartTransmitAck( void )
{
    smacErrors_t result = gErrorNoError_c;
    Phy_TxRequestParams_t phyParams = { 0 };
    uint32_t txDuration;
    AribTimeInfo_t arib;

    switch( g_initParams.protocol )
    {
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
    __cases_Protocol_LORA
        // make frame
        SmacFrame_makeAckFrame_LORA( &g_ackBuff.phy_LORA.sdu.smac, g_rxReqParams.ownNode, &g_initParams.rxBuffer->phy_LORA.sdu.smac.header );
        phyParams.sduSize = SmacFrameTotalSize_ACK_LORA;

        // calc tx duration
        txDuration = Phy_TimeOnAir_LORA( &g_rxReqParams.radio.modulation, SmacFrameTotalSize_ACK_LORA );
        break;
#endif

#if defined(BLD_USE_FSK_R)
    __cases_Protocol_FSK
        // make frame
        SmacFrame_makeAckFrame_FSK( &g_ackBuff.phy_FSK.sdu.smac, g_rxReqParams.ownNode, &g_initParams.rxBuffer->phy_FSK.sdu.smac.header );
        phyParams.sduSize = SmacFrameTotalSize_ACK_FSK;

        // calc tx duration
        txDuration = Phy_TimeOnAir_FSK( &g_rxReqParams.radio.modulation, SmacFrameTotalSize_ACK_FSK );
        break;
#endif
    }

    // check tx duration, cca time and tx duty time
    result = PhyArib_CheckTimes( g_initParams.protocol, &g_rxReqParams.radio.modulation, txDuration, &arib );
    if( result != gErrorNoError_c )
    {
        return result;
    }

    // request
    phyParams.buff            = &g_ackBuff;
    phyParams.isRecycleBuff   = FALSE;
    phyParams.radio           = g_rxReqParams.radio;
    phyParams.waitDutyElapsed = TRUE;
    phyParams.dutyTime        = arib.txDutyTime;
    phyParams.doCca           = TRUE;
    phyParams.ccaTime         = arib.ccaTime;
    return Phy_ReqTx( &phyParams );
}

static void PhyCallback_TxComp( smacErrors_t result )
{
    bool_t idleProcess = FALSE;

    SMAC_TRANS_DEBUG_PRINT("PhyCallback_TxComp\r\n");

    switch( g_state )
    {
    case SMAC_TRANS_STATE_NONE:
    case SMAC_TRANS_STATE_ENTER_IDLE:
    case SMAC_TRANS_STATE_IDLE:             // ProcessReq:(none)            Phy:Standby or Sleep    Radio:Standby or Sleep
    case SMAC_TRANS_STATE_WAIT_ACK:         // ProcessReq:TxData            Phy:Rx                  Radio:Rx
        // to do nothing
        break;

    case SMAC_TRANS_STATE_TX_ACK:           // ProcessReq:(None)            Phy:Tx(ack)             Radio:Tx or Standby(duty)
        // complete rx
        g_state = SMAC_TRANS_STATE_ENTER_IDLE;
        idleProcess = TRUE;

        if( g_idleMode == SmacTransIdleMode_RX )
        {
            // complete rx
            g_initParams.callback.onNotifyRxData( gErrorNoError_c, &g_rxResultParams );
        }
        break;

    case SMAC_TRANS_STATE_TX_DATA:          // ProcessReq:TxData            Phy:Tx(data)            Radio:Tx or Standby(duty)

        // cca failed
        if( (result == gErrorChannelBusy_c) && (g_ccaRetransCnt++ < g_txReqParams.trans.option.ccaRetryCount) )
        {
            // stop timer
            g_initParams.callback.TimerControl( SmacTxTimer, 0 );
            g_initParams.callback.TimerControl( SmacCcaBackoffTimer, RETRANS_BACKOFF() );
        }
        // error
        else if( result != gErrorNoError_c )
        {
            Phy_Standby();

            // error complete
            g_state     = SMAC_TRANS_STATE_ENTER_IDLE;
            idleProcess = TRUE;
            g_initParams.callback.onTxDataComp( result );
        }
        // tx succeeded (with ACK)
        else if( IS_ACK_REQ(g_txReqParams.trans.option.ackReq, &g_txReqParams.trans.addr) )
        {
            // wait for rx(ack)
            g_state = SMAC_TRANS_STATE_WAIT_ACK;

            result = StartReceiveAck();
            if( result != gErrorNoError_c )
            {
                // stop timer
                g_initParams.callback.TimerControl( SmacTxTimer, 0 );
                g_initParams.callback.TimerControl( SmacCcaBackoffTimer, 0 );

                Phy_Standby();

                // error complete
                g_state     = SMAC_TRANS_STATE_ENTER_IDLE;
                idleProcess = TRUE;
                g_initParams.callback.onTxDataComp( result );
            }
        }
        // tx succeeded (no ACK)
        else
        {
            // stop timer
            g_initParams.callback.TimerControl( SmacTxTimer, 0 );
            g_initParams.callback.TimerControl( SmacCcaBackoffTimer, 0 );

            Phy_Standby();

            // tx complete
            g_state = SMAC_TRANS_STATE_ENTER_IDLE;
            idleProcess = TRUE;
            g_initParams.callback.onTxDataComp( gErrorNoError_c );
        }

        break;
    }

    if( idleProcess )
    {
        SmacTrans_IdleProcess();
    }
}
