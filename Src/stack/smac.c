/*******************************************************************************
* smac file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#include "usr_common.h"
#include "stack/smac.h"

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

/*******************************************************************************
********************************************************************************
* Private function declarations
********************************************************************************
*******************************************************************************/
static smacErrors_t SMAC_CheckCallbackInterface( const smacCallbackInterface_t* params );
static smacErrors_t SMAC_CheckProtocolType( protocolType_t type );

/*******************************************************************************
********************************************************************************
* Private memory declarations
********************************************************************************
*******************************************************************************/
static smacParams_t g_params = { (protocolType_t)0 };
static uint8_t      g_aesKey[SMAC_ENCRYPT_KEY_LENGTH];
static bool_t       g_isInitialized   = FALSE;
static bool_t       g_isRegistHandler = FALSE;
bool_t              g_canEnterStopMode = TRUE;

/*******************************************************************************
********************************************************************************
* Public functions
********************************************************************************
*******************************************************************************/

/*******************************************************************************
* SMAC_Init
*
* This function initialise SMAC.
*
* Interface assumptions:
*     params            parameters
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_Init( const smacParams_t* params )
{
    smacErrors_t result = gErrorNoError_c;

    // check parameter
    result = SMAC_CheckParams( params );
    if( result != gErrorNoError_c ) { return result; }

    // remember parameter
    g_params = *params;
    if( params->encryptOption.enable )
    {
        memcpy( g_aesKey, params->encryptOption.aesKey, sizeof(g_aesKey) );
        g_params.encryptOption.aesKey = g_aesKey;
    }

    g_isInitialized = TRUE;

    return result;
}

/*******************************************************************************
* SMAC_Start
*
* This function is used to configure the conditions under which
* SMAC will perform a transmission.
*
* Interface assumptions:
*     callback          callback interface
*     rxBuffer          rx data buffer
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_Start( const smacCallbackInterface_t* callback, packet_t* rxBuffer )
{
    smacErrors_t            result = gErrorNoError_c;
    SmacUser_InitParams_t   initParams = { (protocolType_t)0 };

    if( !rxBuffer ) { return gErrorOutOfRange_c; }

    // check parameter
    result = SMAC_CheckCallbackInterface( callback );
    if( result != gErrorNoError_c ) { return result; }

    // initialize protocol stack
    initParams.protocol      = g_params.protocol;
    initParams.callback      = *callback;
    initParams.rxBuffer      = rxBuffer;
    initParams.encryptOption = g_params.encryptOption;
    SmacUser_Init( &initParams );

    g_isRegistHandler = TRUE;

    return result;
}

/*******************************************************************************
* SMAC_GetAllParams
*
* This function gets the all parameters
*
* Interface assumptions:
*     params            parameters
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_GetAllParams( smacParams_t* params )
{
    // check condition and parameter
    if( !g_isInitialized ) { return gErrorNotInitialized_c; }
    if( !params ) { return gErrorOutOfRange_c; }

    // return params
    *params = g_params;

    return gErrorNoError_c;
}

/*******************************************************************************
* SMAC_SetAllParams
*
* This function sets the all parameters
*
* Interface assumptions:
*     params            parameters
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_SetAllParams( const smacParams_t* params )
{
    smacErrors_t result = gErrorNoError_c;

    // check condition and parameter
    if( !g_isInitialized ) { return gErrorNotInitialized_c; }
    if( !params ) { return gErrorOutOfRange_c; }

    // check params
    result = SMAC_CheckParams( params );
    if( result != gErrorNoError_c ) { return result; }

    // set params
    g_params = *params;

    if( params->encryptOption.enable )
    {
        memcpy( g_aesKey, params->encryptOption.aesKey, sizeof(g_aesKey) );
        g_params.encryptOption.aesKey = g_aesKey;
    }

    return result;
}

/*******************************************************************************
* SMAC_GetRouteParams
*
* This function gets the parameter for routing
*
* Interface assumptions:
*     params            parameter
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_GetRouteParams( smacRouteParams_t* params )
{
    // check condition and parameter
    if( !g_isInitialized ) { return gErrorNotInitialized_c; }
    if( !params ) { return gErrorOutOfRange_c; }

    // return params
    *params = g_params.route;

    return gErrorNoError_c;
}

/*******************************************************************************
* SMAC_SetRouteParams
*
* This function sets the parameter for routing
*
* Interface assumptions:
*     params            parameter
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_SetRouteParams( const smacRouteParams_t* params )
{
    smacErrors_t    result = gErrorNoError_c;

    // check condition and parameter
    if( !g_isInitialized ) { return gErrorNotInitialized_c; }
    if( !params ) { return gErrorOutOfRange_c; }

    // check params
    result = SMAC_CheckRouteParams( params, g_params.protocol );
    if( result != gErrorNoError_c ) { return result; }

    // remember params
    g_params.route = *params;

    return result;
}

/*******************************************************************************
* SMAC_GetTransParams
*
* This function gets the parameter for transport
*
* Interface assumptions:
*     params            parameter
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_GetTransParams( SmacTransParams_t* params )
{
    // check condition and parameter
    if( !g_isInitialized ) { return gErrorNotInitialized_c; }
    if( !params ) { return gErrorOutOfRange_c; }

    // return params
    *params = g_params.trans;

    return gErrorNoError_c;
}

/*******************************************************************************
* SMAC_SetTransParams
*
* This function sets the parameter for transport
*
* Interface assumptions:
*     params            parameter
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_SetTransParams( const SmacTransParams_t* params )
{
    smacErrors_t result = gErrorNoError_c;

    // check condition
    if( !g_isInitialized ) { return gErrorNotInitialized_c; }
    if( !params ) { return gErrorOutOfRange_c; }

    // check parameter
    result = SMAC_CheckTransParams( params );
    if( result != gErrorNoError_c ) { return result; }

    // remember params
    g_params.trans = *params;

    return result;
}

/*******************************************************************************
* SMAC_GetRadioParams
*
* This function gets the parameter for radio
*
* Interface assumptions:
*     params            parameter
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_GetRadioParams( PhyRadioParams_t* params )
{
    // check condition and parameter
    if( !g_isInitialized ) { return gErrorNotInitialized_c; }
    if( !params ) { return gErrorOutOfRange_c; }

    // return params
    *params = g_params.radio;

    return gErrorNoError_c;
}

/*******************************************************************************
* SMAC_SetRadioParams
*
* This function sets the parameter for radio
*
* Interface assumptions:
*     params            parameter
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_SetRadioParams( const PhyRadioParams_t* params )
{
    smacErrors_t    result = gErrorNoError_c;

    // check condition
    if( !g_isInitialized ) { return gErrorNotInitialized_c; }
    if( !params ) { return gErrorOutOfRange_c; }

    // check parameter
    result = SMAC_CheckRadioParams( params, g_params.protocol );
    if( result != gErrorNoError_c ) { return result; }

    // remember params
    g_params.radio = *params;

    return result;
}

/*******************************************************************************
* SMAC_GetEncryptOption
*
* This function gets the parameter for encrypt option
*
* Interface assumptions:
*     params            parameter
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_GetEncryptOption( SmacTransEncryptOption_t* params )
{
    // check condition and parameter
    if( !g_isInitialized ) { return gErrorNotInitialized_c; }
    if( !params ) { return gErrorOutOfRange_c; }

    // return params
    *params = g_params.encryptOption;

    return gErrorNoError_c;
}

/*******************************************************************************
* SMAC_SetEncryptOption
*
* This function sets the parameter for encrypt option
*
* Interface assumptions:
*     params            parameter
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_SetEncryptOption( const SmacTransEncryptOption_t* params )
{
    smacErrors_t result = gErrorNoError_c;

    // check condition
    if( !g_isInitialized ) { return gErrorNotInitialized_c; }
    if( !params ) { return gErrorOutOfRange_c; }

    // check parameter
    result = SMAC_CheckEncryptOption( params );
    if( result != gErrorNoError_c ) { return result; }

    // remember params
    g_params.encryptOption = *params;
    if( params->enable )
    {
        memcpy( g_aesKey, params->aesKey, sizeof(g_aesKey) );
        g_params.encryptOption.aesKey = g_aesKey;
    }

    return result;
}

/*******************************************************************************
* SMAC_RxStart
*
* This function transit the RX state.
*
* Interface assumptions:
*     None
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_RxStart( void )
{
    SmacUser_RxRequestParams_t  reqParams = {0};

    // check condition and parameter
    if( !g_isInitialized || !g_isRegistHandler ) { return gErrorNotInitialized_c; }

    // parameter setting
    reqParams.ownNode   = g_params.trans.addr.srcNode;
    reqParams.radio     = g_params.radio;

    // start rx
    return SmacUser_StartRxData( &reqParams );
}

/*******************************************************************************
* SMAC_TxPacket
*
* This function send packet.
*
* Interface assumptions:
*     data              tx buffer pointer (not need to fill header fields)
*     payloadSize       payload length (not include header)
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_TxPacket( packet_t *data, uint8_t payloadSize )
{
    SmacUser_TxRequestParams_t  reqParams = {0};

    // check condition and parameter
    if( !g_isInitialized || !g_isRegistHandler ) { return gErrorNotInitialized_c; }
    if( !data || payloadSize > MAX_PAYLOAD_LENGTH(g_params.protocol) ) { return gErrorOutOfRange_c; }

    // parameter setting
    reqParams.buff          = data;
    reqParams.payloadSize   = payloadSize;
    reqParams.route         = g_params.route;
    reqParams.trans         = g_params.trans;
    reqParams.radio         = g_params.radio;

    // request tx
    return SmacUser_ReqTxData( &reqParams );
}

/*******************************************************************************
* SMAC_TxPacketEx
*
* This function send packet.
*
* Interface assumptions:
*     data              tx buffer pointer (not need to fill header fields)
*     payloadSize       payload length (not include header)
*     transParams       transport parameter
*     routeParams       routing parameter
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_TxPacketEx( packet_t* data, uint8_t payloadSize, const SmacTransParams_t* transParams, const smacRouteParams_t* routeParams )
{
    smacErrors_t result = gErrorNoError_c;
    SmacUser_TxRequestParams_t  reqParams = {0};

    // check condition and parameter
    if( !g_isInitialized || !g_isRegistHandler ) { return gErrorNotInitialized_c; }
    if( !data || payloadSize > MAX_PAYLOAD_LENGTH(g_params.protocol) ) { return gErrorOutOfRange_c; }

    // check parameter
    result = SMAC_CheckRouteParams( routeParams, g_params.protocol );
    if( result != gErrorNoError_c ) { return result; }

    result = SMAC_CheckTransParams( transParams );
    if( result != gErrorNoError_c ) { return result; }

    // parameter setting
    reqParams.buff          = data;
    reqParams.payloadSize   = payloadSize;
    reqParams.route         = *routeParams;
    reqParams.trans         = *transParams;
    reqParams.radio         = g_params.radio;

    // request tx
    return SmacUser_ReqTxData( &reqParams );
}

/*******************************************************************************
* SMAC_TxContinuous
*
* This function send countinuous wave.
*
* Interface assumptions:
*     isModulate        TRUE : modulated wave
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_TxContinuous( bool_t isModulate )
{
    smacErrors_t    result = gErrorNoError_c;
    uint32_t freq;

    // check condition
    if( !g_isInitialized || !g_isRegistHandler ) { return gErrorNotInitialized_c; }

    // instruct standby to stop running operation
    result = SmacUser_Standby();
    if( result != gErrorNoError_c ) { return result; }

    if( isModulate )
    {
        result = PhyRadio_SetTxInfinitePreamble( &g_params.radio );
    }
    else
    {
        // get frequency of center
        result = PhyRadio_RfFrequency( &freq, &g_params.radio.modulation );
        if( result != gErrorNoError_c ) { return result; }

        // start tx continuous
        PhyRadio_TxContinuous( freq, &g_params.radio.pa );
    }

    return result;
}

/*******************************************************************************
* SMAC_Sleep
*
* This function transit the rf module to sleep state.
*
* Interface assumptions:
*     isWarmStart       TRUE : Warm Start
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_Sleep( bool_t isWarmStart )
{
    // check condition
    if( !g_isInitialized || !g_isRegistHandler ) { return gErrorNotInitialized_c; }

    return SmacUser_Sleep( isWarmStart );
}

/*******************************************************************************
* SMAC_Standby
*
* This function transit the rf module to standby state.
*
* Interface assumptions:
*     None
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_Standby( void )
{
    // check condition
    if( !g_isInitialized || !g_isRegistHandler ) { return gErrorNotInitialized_c; }

    return SmacUser_Standby();
}

/*******************************************************************************
* SMAC_GetIdleMode
*
* This function rf module state while tx idle
*
* Interface assumptions:
*     mode              mode
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_GetIdleMode( SmacTransIdleMode_t* mode )
{
    return SmacTrans_GetIdleMode( mode );
}

/*******************************************************************************
* SMAC_GetRssi
*
* This function get rssi
*
* Interface assumptions:
*     None
*
* Return value:
*     rssi
*
*******************************************************************************/
uint8_t SMAC_GetRssi( void )
{
    return Radio.Rssi( MODEM_FSK/*dummy*/ );
}

/*******************************************************************************
* SMAC_RadioIrqProcess
*
* This function is called when interrupt asserted by RF module
*
* Interface assumptions:
*     None
*
* Return value:
*     irq mask processed
*
*******************************************************************************/
uint16_t SMAC_RadioIrqProcess( void )
{
    // check condition
    if( !g_isInitialized || !g_isRegistHandler ) { return gErrorNotInitialized_c; }

    return PhyRadio_IrqProcess();
}

/*******************************************************************************
* SMAC_CanEnterStopMode
*
* This function is called for check if able to enter stop mode
*
* Interface assumptions:
*     None
*
* Return value:
*     enable            enable sleep mode during delay or not
*
*******************************************************************************/
bool_t SMAC_CanEnterStopMode( void )
{
    return g_canEnterStopMode;
}

/*******************************************************************************
* SMAC_TimerExpiredProcess
*
* This function is called when timer expired
*
* Interface assumptions:
*     type              timer type
*
* Return value:
*     None
*
*******************************************************************************/
void SMAC_TimerExpiredProcess( stackTimerType_t type )
{
    // check condition
    if( !g_isInitialized || !g_isRegistHandler ) { return; }

    switch( type )
    {
    case PhyCcaTimer:
        PhyRadio_TimerCcaFin();
        break;
    case PhyTxTimer:
        PhyRadio_TxTimeout();
        break;
    case PhyRxTimer:
        PhyRadio_RxTimeout();
        break;
    case PhyDutyTimer:
        Phy_TimerDutyFin();
        break;
    case SmacCcaBackoffTimer:
        SmacTrans_CcaBackoffTimeout();
        break;
    case SmacTxTimer:
        SmacTrans_TxDataTimeout();
        break;
    default:
        break;
    }
}

/*******************************************************************************
* SMAC_FiberProcess
*
* This function is called while fiber is active
*
* Interface assumptions:
*     type              timer type
*
* Return value:
*     None
*
*******************************************************************************/
void SMAC_FiberProcess( stackFiberType_t type )
{
    // check condition
    if( !g_isInitialized || !g_isRegistHandler ) { return; }

    switch( type )
    {
    case PhyCcaFiber:
        PhyRadio_CcaFiber();
        break;
    default:
        break;
    }
}

#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
/*******************************************************************************
* SMAC_CalcDataRate
*
* This function is calculate data rate
*
* Interface assumptions:
*     bw                band width
*     sf                spreading factor
*     dataRate          data rate
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_CalcDataRate( uint8_t bw, uint8_t sf, uint32_t* dataRate )
{
    switch( bw )
    {
    case BANDWIDTH7_8:
    case BANDWIDTH10_4:
    case BANDWIDTH15_6:
    case BANDWIDTH20_8:
    case BANDWIDTH31_25:
    case BANDWIDTH41_7:
    case BANDWIDTH62_5:
    case BANDWIDTH125:
    case BANDWIDTH250:
    case BANDWIDTH500:
        break;
    default:
        return gErrorOutOfRange_c;
    }

    if( OUT_OF_RANGE(sf, MIN_SF, MAX_SF) )
    {
        return gErrorOutOfRange_c;
    }

    *dataRate = PhyRadio_DataRate_LORA( bw, sf );

    return gErrorNoError_c;
}
#endif

/*******************************************************************************
* SMAC_CheckParams
*
* This function check the parameters are in correct range
*
* Interface assumptions:
*     params            parameters
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_CheckParams( const smacParams_t* params )
{
    smacErrors_t    result = gErrorNoError_c;

    // check parameter
    if( !params ) { return gErrorOutOfRange_c; }

    result = SMAC_CheckProtocolType( params->protocol );
    if( result != gErrorNoError_c ) { return result; }

    result = SMAC_CheckRouteParams( &params->route, params->protocol );
    if( result != gErrorNoError_c ) { return result; }

    result = SMAC_CheckTransParams( &params->trans );
    if( result != gErrorNoError_c ) { return result; }

    result = SMAC_CheckRadioParams( &params->radio, params->protocol );
    if( result != gErrorNoError_c ) { return result; }

    result = SMAC_CheckEncryptOption( &params->encryptOption );
    if( result != gErrorNoError_c ) { return result; }

    return result;
}

/*******************************************************************************
* SMAC_CheckRouteParams
*
* This function check the parameters are in correct range
*
* Interface assumptions:
*     params            parameters
*     protocol          protocol type
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_CheckRouteParams( const smacRouteParams_t* params, protocolType_t protocol )
{
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    unsigned i;
#endif

    if( !params
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
        || OUT_OF_RANGE( params->hop_cnt, MIN_HOP_CNT, MAX_HOP_CNT(protocol) )
        || ( params->endNodeAddr > MAX_END_ID )
        || ( params->oriNodeAddr > MAX_OWN_ID )
#endif
    )
    {
        return gErrorOutOfRange_c;
    }

#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    for( i = 1; i < params->hop_cnt; i++ )
    {
        if( OUT_OF_RANGE( params->route[i-1], MIN_ROUTE_ID, MAX_ROUTE_ID ) )
        {
            return gErrorOutOfRange_c;
        }
    }
#endif

    return gErrorNoError_c;
}

/*******************************************************************************
* SMAC_CheckTransParams
*
* This function check the parameters are in correct range
*
* Interface assumptions:
*     params            parameters
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_CheckTransParams( const SmacTransParams_t* params )
{
    if( !params
        || OUT_OF_RANGE( params->addr.srcNode.panId,    MIN_PAN_ID,     MAX_PAN_ID    )
//      || OUT_OF_RANGE( params->addr.srcNode.nodeAddr, MIN_OWN_ID,     MAX_OWN_ID    )
        ||             ( params->addr.srcNode.nodeAddr > MAX_OWN_ID )
//      || OUT_OF_RANGE( params->addr.destNodeAddr,     MIN_DEST_ID,    MAX_DEST_ID   )
//      || OUT_OF_RANGE( params->option.ackRetryCount,  MIN_RETRY_CNT,  MAX_RETRY_CNT )
        ||             ( params->option.ackRetryCount > MAX_RETRY_CNT )
//      || OUT_OF_RANGE( params->option.ccaRetryCount,  MIN_RETRY_CNT,  MAX_RETRY_CNT )
        ||             ( params->option.ccaRetryCount > MAX_RETRY_CNT )
        )
    {
        return gErrorOutOfRange_c;
    }

    return gErrorNoError_c;
}

/*******************************************************************************
* SMAC_CheckRadioParams
*
* This function check the parameters are in correct range
*
* Interface assumptions:
*     params            parameters
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_CheckRadioParams( const PhyRadioParams_t* params, protocolType_t protocol )
{
    uint8_t maxCh;

    if( !params || OUT_OF_RANGE( params->pa.power, MIN_POWER, MAX_POWER ) )
    {
        return gErrorOutOfRange_c;
    }

    switch( protocol )
    {
#if defined(BLD_USE_FSK_R)
    __cases_Protocol_FSK
        switch( params->modulation.FSK.dr )
        {
        case RATE_50KBPS:
            maxCh = MAX_CH_DR50;
            break;
        case RATE_100KBPS:
        case RATE_150KBPS:
            maxCh = MAX_CH_DR150;
            break;
        case RATE_200KBPS:
        case RATE_250KBPS:
            maxCh = MAX_CH_DR250;
            break;
        default:
            return gErrorOutOfRange_c;
        }
        break;
#endif

#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
    __cases_Protocol_LORA
        switch( params->modulation.LORA.bw )
        {
        case BANDWIDTH7_8:
        case BANDWIDTH10_4:
        case BANDWIDTH15_6:
        case BANDWIDTH20_8:
        case BANDWIDTH31_25:
        case BANDWIDTH41_7:
        case BANDWIDTH62_5:
        case BANDWIDTH125:
            maxCh = MAX_CH_BW125;
            break;
        case BANDWIDTH250:
            maxCh = MAX_CH_BW250;
            break;
        case BANDWIDTH500:
            maxCh = MAX_CH_BW500;
            break;
        default:
            return gErrorOutOfRange_c;
        }

        switch(params->modulation.LORA.cr)
        {
        case CR_4_5:
        case CR_4_6:
        case CR_4_7:
        case CR_4_8:
            break;
        default:
            return gErrorOutOfRange_c;
        }

        if( OUT_OF_RANGE( params->modulation.LORA.sf, MIN_SF, MAX_SF ) )
        {
            return gErrorOutOfRange_c;
        }
        break;
#endif

    default:
        return gErrorOutOfRange_c;
    }

    if( OUT_OF_RANGE( params->modulation.ch, MIN_CH, maxCh ) )
    {
        return gErrorOutOfRange_c;
    }

    return gErrorNoError_c;
}

/*******************************************************************************
* SMAC_CheckEncryptOption
*
* This function check the parameters are in correct range
*
* Interface assumptions:
*     params            parameters
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
smacErrors_t SMAC_CheckEncryptOption( const SmacTransEncryptOption_t* params )
{
    if( params->enable && params->aesKey == NULL )
    {
        return gErrorOutOfRange_c;
    }
    return gErrorNoError_c;
}

/*******************************************************************************
********************************************************************************
* Private functions
********************************************************************************
*******************************************************************************/

/*******************************************************************************
* SMAC_CheckCallbackInterface
*
* This function check the parameters are in correct range
*
* Interface assumptions:
*     params            parameters
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
static smacErrors_t SMAC_CheckCallbackInterface( const smacCallbackInterface_t* params )
{
    if( !params ||
        !params->onNotifyRxData ||
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
        !params->onNotifyForward ||
        !params->onNotifyForwardResult ||
#endif
        !params->onTxDataComp ||
        !params->TimerControl ||
        !params->FiberControl )
    {
        return gErrorOutOfRange_c;
    }

    return gErrorNoError_c;
}

/*******************************************************************************
* SMAC_CheckProtocolType
*
* This function check the parameters are in correct range
*
* Interface assumptions:
*     type              protocol type
*
* Return value:
*     smacErrors_t      gErrorNoError_c or other
*
*******************************************************************************/
static smacErrors_t SMAC_CheckProtocolType( protocolType_t type )
{
    switch( type )
    {
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
    __cases_Protocol_LORA
#endif
#if defined(BLD_USE_FSK_R)
    __cases_Protocol_FSK
#endif
        break;

    default:
        return gErrorOutOfRange_c;
    }

    return gErrorNoError_c;
}

/*******************************************************************************
* SMAC_GetRandom
*
* This function generate random value.
*
* Interface assumptions:
*     min               minimum of random value
*     max               maximum of random value
*
* Return value:
*     uint32_t          random value
*
*******************************************************************************/
uint32_t SMAC_GetRandom( uint32_t min,  uint32_t max )
{
	const uint32_t range = max - min;

	if( range == 0 )
	{
		return min;
	}

	return min + ( PhyRadio_GetRandom() % range );
}
