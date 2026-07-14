/*******************************************************************************
* phy_radio file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#include "usr_common.h"

#include "stack/phy/phy_radio.h"
#include "stack/phy/phy_arib.h"
#include "stack/radio/radio.h"
#include "stack/radio/radio_bsp.h"

/*******************************************************************************
********************************************************************************
* Private macros
********************************************************************************
*******************************************************************************/

#define LORA_PREAMBLE_LEN           8
#define LORA_IS_FIXLEN              false
#define LORA_FIXLEN_PAYLOADLEN      0
#define LORA_CRC_ONOFF              true
#define LORA_FREQ_HOP_ONOFF         false
#define LORA_IQ_INVERTED            0

#if defined(BLD_FSK_FIXED_FORMAT)
#define FSK_IS_FIXLEN               true
#define FSK_FIXLEN_PAYLOADLEN       RadioPayloadSizeMax
#else
#define FSK_IS_FIXLEN               false
#define FSK_FIXLEN_PAYLOADLEN       0
#endif
#define FSK_PREAMBLE_LEN            16   /* [bytes] */
#define FSK_CRC_ONOFF               false

#define LORA_RX_SYMB_TIMEOUT        0
#define FSK_RX_SYMB_TIMEOUT         0

#define MARGIN                      100

#define RX_TIMEOUT                  10000

/*******************************************************************************
********************************************************************************
* Private type definitions
********************************************************************************
*******************************************************************************/
typedef enum PhyRadioTxState_tag
{
    PhyRadioTxState_IDLE,
    PhyRadioTxState_CCA,
    PhyRadioTxState_TX,
} PhyRadioTxState_t;

typedef enum PhyRadioRxState_tag
{
    PhyRadioRxState_IDLE,
    PhyRadioRxState_RX,             // RX mode
    PhyRadioRxState_RX_ERROR,       // RX mode (error occured)
} PhyRadioRxState_t;

typedef struct
{
    uint32_t        bw;
    uint32_t        ccaFreqCnt;
    const int32_t*  ccaFreqs;
} CcaRanges_t;

/*******************************************************************************
********************************************************************************
* Private function declarations
********************************************************************************
*******************************************************************************/
static void CancelTx( void );
static bool_t CheckRssi( void );

static void RadioCallback_TxDone( void );
static void RadioCallback_RxDone( void );
static void RadioCallback_TxTimeout( void );
static void RadioCallback_RxError( void );
static void RadioCallback_PreambleDetected( void );

static void SetRxConfigForCS( uint32_t bandwidth );
static void SetRxConfig( const PhyRadio_ModulationParams_t* modParams );
static void SetTxConfig( const PhyRadio_ModulationParams_t* modParams, const PhyRadio_PaParams_t* paParams, uint8_t size, uint32_t timeout );
static void StartTx( const PhyRadio_TxParams_t* params );
static void CancelRx( void );
static smacErrors_t StartRx( void );

#if defined(BLD_USE_FSK_R)
static void                         ConfigParam_dr( fskDataRate dr, uint32_t* bandwidth, uint32_t* fdev, uint32_t* datarate, uint32_t* bandwidthAfc );
#endif
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
static RadioLoRaSpreadingFactors_t  ConfigParam_sf( uint8_t sf );
static RadioLoRaBandwidths_t        ConfigParam_bw( uint8_t bw );
static RadioLoRaCodingRates_t       ConfigParam_cr( uint8_t cr );
#endif

/*******************************************************************************
********************************************************************************
* Private memory declarations
********************************************************************************
*******************************************************************************/
static PhyRadio_InitParams_t    g_initParams = { (protocolType_t)0 };
static PhyRadioTxState_t        g_txState = PhyRadioTxState_IDLE;
static PhyRadioRxState_t        g_rxState = PhyRadioRxState_IDLE;
static uint8_t                  g_ccaRetryCnt = 0;
static PhyRadio_RxParams_t      g_rxReqParams;
static PhyRadio_TxParams_t      g_txReqParams;
static RadioEvents_t g_radioEvents = { NULL };

// UnitChannel1 [-100KHz, +100KHz] : { [-100KHz, +100KHz] }
static const int32_t        CcaFreq_UC1_BW200K[]    = { 0, };
static const CcaRanges_t    CcaRanges_UC1_BW200K    = { 200000, _countof(CcaFreq_UC1_BW200K), CcaFreq_UC1_BW200K };

// UnitChannel2 [-200KHz, +200KHz] : { [-200KHz, +200KHz] }
static const int32_t        CcaFreq_UC2_BW400K[]    = { 0, };
static const CcaRanges_t    CcaRanges_UC2_BW400K    = { 400000, _countof(CcaFreq_UC2_BW400K), CcaFreq_UC2_BW400K };

// UnitChannel3 [-300KHz, +300KHz] : { [-100KHz, +300KHz] ¨ [-300KHz, -100KHz] ¨ [-100KHz, +100KHz] }
static const int32_t        CcaFreq_UC3_BW600K[]    = { 200000, -200000, 0, };
static const CcaRanges_t    CcaRanges_UC3_BW600K    = { 200000, _countof(CcaFreq_UC3_BW600K), CcaFreq_UC3_BW600K };

// UnitChannel4 [-400KHz, +400KHz] : { [+0KHz, +400KHz] ¨ [-400KHz, +0KHz] ¨ [-200KHz, +200KHz] }
static const int32_t        CcaFreq_UC4_BW800K[]    = { 200000, -200000, 0, };
static const CcaRanges_t    CcaRanges_UC4_BW800K    = { 400000, _countof(CcaFreq_UC4_BW800K), CcaFreq_UC4_BW800K };

// UnitChannel5 [-500KHz, +500KHz] : { [+100KHz, +500KHz] ¨ [-500KHz, -100KHz] ¨ [-200KHz, +200KHz] }
static const int32_t        CcaFreq_UC5_BW1000K[]   = { 300000, -300000, 0, };
static const CcaRanges_t    CcaRanges_UC5_BW1000K   = { 400000, _countof(CcaFreq_UC5_BW1000K), CcaFreq_UC5_BW1000K };

static const CcaRanges_t*   g_CurrentCcaRanges;
static uint32_t             g_TxFreq;

/*******************************************************************************
********************************************************************************
* Public functions
********************************************************************************
*******************************************************************************/

void PhyRadio_Init( const PhyRadio_InitParams_t* params )
{
    PHY_RADIO_DEBUG_PRINT("PhyRadio_Init\r\n");

    g_initParams = *params;

    memset( &g_radioEvents, 0, sizeof(g_radioEvents) );
    g_radioEvents.TxDone            = RadioCallback_TxDone;
    g_radioEvents.RxDone            = RadioCallback_RxDone;
    g_radioEvents.TxTimeout         = RadioCallback_TxTimeout;
    g_radioEvents.RxTimeout         = NULL;
    g_radioEvents.RxError           = RadioCallback_RxError;
    g_radioEvents.FhssChangeChannel = NULL;
    g_radioEvents.CadDone           = NULL;
    g_radioEvents.SyncWordValid     = params->callback.onSyncWordValid;
    g_radioEvents.PreambleDetected  = RadioCallback_PreambleDetected;

    Radio.Init( &g_radioEvents );
}

void PhyRadio_Standby( void )
{
    // cancel tx/rx sequence if in process
    CancelTx();
    CancelRx();

    PHY_RADIO_DEBUG_PRINT("PhyRadio_Standby\r\n");

    Radio.Standby();
}

void PhyRadio_Sleep( bool_t isWarmStart )
{
    // cancel tx/rx sequence if in process
    CancelTx();
    CancelRx();

    PHY_RADIO_DEBUG_PRINT("PhyRadio_Sleep\r\n");

    Radio.Sleep( isWarmStart );
}

smacErrors_t PhyRadio_Rx( const PhyRadio_RxParams_t* params )
{
    PHY_RADIO_DEBUG_PRINT("PhyRadio_Rx\r\n");

    // cancel tx/rx sequence if in process
    CancelTx();
    CancelRx();

    g_rxReqParams = *params;

    /* start receive */
    return StartRx();
}

static smacErrors_t StartRx( void )
{
    smacErrors_t    result = gErrorNoError_c;
    uint32_t        freq;

    result = PhyRadio_RfFrequency( &freq, &g_rxReqParams.radio.modulation );
    if( result != gErrorNoError_c ) { return result; }

    Radio.SetChannel( freq );

    SetRxConfig( &g_rxReqParams.radio.modulation );

    SUBGRF_ClearIrqStatus( 0xFFFF );

    // start RX Timer
    g_initParams.callback.TimerControl( PhyRxTimer, RX_TIMEOUT );

    g_rxState = PhyRadioRxState_RX;

    if( g_rxReqParams.radio.rxBoost )
    {
        Radio.RxBoosted( 0 );
    }
    else
    {
        Radio.Rx( 0 );
    }

    /* start receive */
    return result;
}

smacErrors_t PhyRadio_Tx( const PhyRadio_TxParams_t* params )
{
    smacErrors_t result = gErrorNoError_c;

    PHY_RADIO_DEBUG_PRINT("PhyRadio_Tx\r\n");

    // cancel tx/rx sequence if in process
    CancelTx();
    CancelRx();

    result = PhyRadio_RfFrequency( &g_TxFreq, &params->radio.modulation );
    if( result != gErrorNoError_c ) { return result; }

    // carrier sense
    if( params->doCca )
    {
        // set frequency
        switch( PhyArib_CheckBandWidth(g_initParams.protocol, &params->radio.modulation) )
        {
        default:
        case ARIB_UC1_BW200KHZ:
            g_CurrentCcaRanges = &CcaRanges_UC1_BW200K;
            break;
        case ARIB_UC2_BW400KHZ:
            g_CurrentCcaRanges = &CcaRanges_UC2_BW400K;
            break;
        case ARIB_UC3_BW600KHZ:
            g_CurrentCcaRanges = &CcaRanges_UC3_BW600K;
            break;
        case ARIB_UC4_BW800KHZ:
            g_CurrentCcaRanges = &CcaRanges_UC4_BW800K;
            break;
        case ARIB_UC5_BW1000KHZ:
            g_CurrentCcaRanges = &CcaRanges_UC5_BW1000K;
            break;
        }

        if( g_CurrentCcaRanges->ccaFreqCnt == 1 )
        {
            // set frequency
            Radio.SetChannel( g_TxFreq );
        }

        // set band width
        SetRxConfigForCS( g_CurrentCcaRanges->bw );

        // set to rx mode
        SUBGRF_ClearIrqStatus( 0xFFFF );

        Radio.Rx( 0xFFFFFF );

        // first check RSSI
        for( g_ccaRetryCnt = 0; g_ccaRetryCnt < CCA_RETRY; g_ccaRetryCnt++ )
        {
            if( CheckRssi() )
            {
                // OK
                break;
            }
        }
        if( g_ccaRetryCnt == CCA_RETRY )
        {
            // failed CCA
            Radio.Standby();

            return gErrorChannelBusy_c;
        }

        if( params->ccaTime )
        {
            g_txState     = PhyRadioTxState_CCA;
            g_txReqParams = *params;

            // start CCA timer
            g_initParams.callback.TimerControl( PhyCcaTimer, params->ccaTime );

            // start CCA process fiber
            g_initParams.callback.FiberControl( PhyCcaFiber, TRUE );

            return gErrorNoError_c;
        }
    }

    StartTx( params );

    return gErrorNoError_c;
}

void PhyRadio_CcaFiber( void )
{
    PHY_RADIO_DEBUG_PRINT("PhyRadio_CcaFiber\r\n");

    if( g_txState == PhyRadioTxState_CCA )
    {
        // last check RSSI
        if( CheckRssi() )
        {
            // OK -> continue CCA process
        }
        else
        {
            // retry
            for( ; g_ccaRetryCnt < CCA_RETRY; g_ccaRetryCnt++)
            {
                if( CheckRssi() )
                {
                    // OK -> retry CCA
                    g_initParams.callback.TimerControl( PhyCcaTimer, g_txReqParams.ccaTime );

                    return;
                }
            }

            Radio.Standby();

            // stop CCA process fiber
            g_initParams.callback.FiberControl( PhyCcaFiber, FALSE );

            // failed CCA
            g_txState = PhyRadioTxState_IDLE;

            // notify error
            g_initParams.callback.onTxDone( gErrorChannelBusy_c );
        }
    }
}

void PhyRadio_TimerCcaFin( void )
{
    PHY_RADIO_DEBUG_PRINT("PhyRadio_TimerCcaFin\r\n");

    if( g_txState == PhyRadioTxState_CCA )
    {
        // exit cca process
        g_initParams.callback.FiberControl( PhyCcaFiber, FALSE );

        // last check RSSI
        if( CheckRssi() )
        {
            // OK -> start tx
            StartTx( &g_txReqParams );
        }
        else if( g_ccaRetryCnt++ < CCA_RETRY )
        {
            // cca retry (re-enter cca process)
            g_initParams.callback.FiberControl( PhyCcaFiber, TRUE );
            g_initParams.callback.TimerControl( PhyCcaTimer, g_txReqParams.ccaTime );
            return;
        }
        else // cca failed
        {
            Radio.Standby();

            // notify error
            g_initParams.callback.TimerControl( PhyCcaTimer, 0 );
            g_txState = PhyRadioTxState_IDLE;
            g_initParams.callback.onTxDone( gErrorChannelBusy_c );
            return;
        }
    }
}

void PhyRadio_TxTimeout( void )
{
    PHY_RADIO_DEBUG_PRINT("PhyRadio_TxTimeout\r\n");

    // stop Tx timer
    g_initParams.callback.TimerControl( PhyTxTimer, 0 );

    // exit tx process
    g_txState = PhyRadioTxState_IDLE;

    g_initParams.callback.onTxDone( gErrorTimeout_c );
}

void PhyRadio_RxTimeout( void )
{
    PHY_RADIO_DEBUG_PRINT("PhyRadio_RxTimeout\r\n");

    switch( g_rxState )
    {
    case PhyRadioRxState_IDLE:
        // to do nothing
        break;

    case PhyRadioRxState_RX:
    case PhyRadioRxState_RX_ERROR:

        // standby
        Radio.Standby();

        // restart rx
        StartRx();
        break;
    }
}

uint16_t PhyRadio_IrqProcess( void )
{
    PHY_RADIO_DEBUG_PRINT("PhyRadio_IrqProcess\r\n");

    return Radio.IrqProcess();
}

void PhyRadio_WriteRxBuffer( uint8_t* buff, size_t size )
{
    Radio.WriteFifo( buff, size );
}

void PhyRadio_ReadRxBuffer( uint8_t* buff, size_t size )
{
    Radio.ReadFifo( buff, size );
}

#if defined(BLD_USE_FSK_R)
void PhyRadio_ModifyRxSize( size_t size )
{
    PHY_RADIO_DEBUG_PRINT("PhyRadio_ModifyRxSize\r\n");

    if( g_rxReqParams.size != size )
    {
        Radio.SetMaxPayloadLength( MODEM_FSK, size );
    }
}
#endif

void PhyRadio_TxContinuous( uint32_t freq, const PhyRadio_PaParams_t* paParams )
{
    PHY_RADIO_DEBUG_PRINT("PhyRadio_TxContinuous\r\n");

    Radio.SetTxContinuousWave( freq, paParams->power, 0xFFFF );
}

smacErrors_t PhyRadio_SetTxInfinitePreamble( const PhyRadioParams_t* params )
{
    smacErrors_t result = gErrorNoError_c;

    PHY_RADIO_DEBUG_PRINT("PhyRadio_SetTxInfinitePreamble\r\n");

    result = PhyRadio_RfFrequency( &g_TxFreq, &params->modulation );
    if( result != gErrorNoError_c ) { return result; }

    // set frequency
    Radio.SetChannel( g_TxFreq );

    // set modulation setting
    SetTxConfig( &params->modulation, &params->pa, 0, 0xFFFF );

    Radio.SetTxInfinitePreamble();

    return result;
}

uint32_t PhyRadio_GetRandom( void )
{
    return Radio.Random();
}

smacErrors_t PhyRadio_RfFrequency( uint32_t* freq, const PhyRadio_ModulationParams_t* modParams )
{
    uint8_t ch = modParams->ch - 1;

    switch( g_initParams.protocol )
    {
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
    __cases_Protocol_LORA
        switch( modParams->LORA.bw )
        {
        case BANDWIDTH7_8:
        case BANDWIDTH10_4:
        case BANDWIDTH15_6:
        case BANDWIDTH20_8:
        case BANDWIDTH31_25:
        case BANDWIDTH41_7:
        case BANDWIDTH62_5:
        case BANDWIDTH125:
            if( MAX_CH_BW125 <= ch )
            {
                return gErrorOutOfRange_c;
            }
            *freq = FREQ_BASE_UC1 + (FREQ_STEP_UC1 * ch);
            break;

        case BANDWIDTH250:
            if( MAX_CH_BW250 <= ch )
            {
                return gErrorOutOfRange_c;
            }
            *freq = FREQ_BASE_UC2 + (FREQ_STEP_UC2 * ch);
            break;

        case BANDWIDTH500:
            if( MAX_CH_BW500 <= ch )
            {
                return gErrorOutOfRange_c;
            }
            *freq = FREQ_BASE_UC3 + (FREQ_STEP_UC3 * ch);
            break;

        default:
            return gErrorOutOfRange_c;
        }
        break;
#endif

#if defined(BLD_USE_FSK_R)
    __cases_Protocol_FSK
        switch( modParams->FSK.dr )
        {
        case RATE_50KBPS:
            if( MAX_CH_DR50 <= ch )
            {
                return gErrorOutOfRange_c;
            }
            *freq = FREQ_BASE_UC1 + (FREQ_STEP_UC1 * ch);
            break;

        case RATE_100KBPS:
        case RATE_150KBPS:
            if( MAX_CH_DR150 <= ch )
            {
                return gErrorOutOfRange_c;
            }
            *freq = FREQ_BASE_UC2 + (FREQ_STEP_UC2 * ch);
            break;

        case RATE_200KBPS:
        case RATE_250KBPS:
            if( MAX_CH_DR250 <= ch )
            {
                return gErrorOutOfRange_c;
            }
            *freq = FREQ_BASE_UC3 + (FREQ_STEP_UC3 * ch);
            break;

        default:
            return gErrorOutOfRange_c;
        }
        break;
#endif
    }

    return gErrorNoError_c;
}

uint32_t PhyRadio_TimeOnAir( const PhyRadio_ModulationParams_t* modParams, uint8_t dataLen )
{
    uint32_t timeOnAir = 0;

#if defined(BLD_USE_FSK_R)
    uint32_t bandwidth;
    uint32_t fdev;
    uint32_t datarate;
    uint32_t dummy;
#endif

    switch( g_initParams.protocol )
    {
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
    __cases_Protocol_LORA
        timeOnAir = Radio.TimeOnAir(MODEM_LORA,
                                    dataLen,
                                    0,
                                    ConfigParam_bw(modParams->LORA.bw),
                                    ConfigParam_sf(modParams->LORA.sf),
                                    ConfigParam_cr(modParams->LORA.cr),
                                    LORA_PREAMBLE_LEN,
                                    LORA_IS_FIXLEN,
                                    LORA_CRC_ONOFF,
                                    LORA_FREQ_HOP_ONOFF,
                                    0,
                                    LORA_IQ_INVERTED );
        break;
#endif

#if defined(BLD_USE_FSK_R)
    __cases_Protocol_FSK
        ConfigParam_dr( (fskDataRate)modParams->FSK.dr, &bandwidth, &fdev, &datarate, &dummy );

        timeOnAir = Radio.TimeOnAir(MODEM_FSK,
                                    dataLen,
                                    fdev,
                                    bandwidth,
                                    datarate,
                                    0,
                                    FSK_PREAMBLE_LEN,
                                    FSK_IS_FIXLEN,
                                    FSK_CRC_ONOFF,
                                    0,
                                    0,
                                    0 );
        break;
#endif
    }

    return timeOnAir;
}

#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
uint32_t PhyRadio_DataRate_LORA( uint8_t bw, uint8_t sf )
{
    static const uint32_t gDataRate[10][8] = {
    //SF:   5      6      7      8      9       10      11      12
        {   976,   586,   342,   195,   110,     61,     34,     18 },   //   7.81 kHz
        {  1303,   782,   456,   261,   147,     81,     45,     24 },   //  10.42 kHz
        {  1954,  1172,   684,   391,   220,    122,     67,     37 },   //  15.63 kHz
        {  2604,  1562,   911,   521,   293,    163,     90,     49 },   //  20.83 kHz
        {  3906,  2344,  1367,   781,   439,    244,    134,     73 },   //  31.25 kHz
        {  5209,  3125,  1823,  1042,   586,    326,    179,     98 },   //  41.67 kHz
        {  7813,  4688,  2734,  1563,   879,    488,    269,    146 },   //  62.5  kHz
        { 15625,  9375,  5469,  3125,  1758,    977,    537,    293 },   // 125    kHz
        { 31250, 18750, 10938,  6250,  3516,   1953,   1074,    586 },   // 250    kHz
        { 62500, 37500, 21875, 12500,  7031,   3906,   2148,   1172 },   // 500    kHz
    };

    return gDataRate[bw][sf-5];
}
#endif

bool_t PhyRadio_IsTxBusy(void)
{
	return (g_txState == PhyRadioTxState_TX);
}

/*******************************************************************************
********************************************************************************
* Private functions
********************************************************************************
*******************************************************************************/

static bool_t CheckRssi( void )
{
    uint32_t i;

    if( g_CurrentCcaRanges->ccaFreqCnt == 1 )
    {
        if( -(int16_t)(uint16_t)Radio.Rssi( MODEM_FSK ) < CCA_RSSI * 2 )
        {
            // OK
        }
        else
        {
            // NG
            return FALSE;
        }
    }
    else
    {
        for( i = 0; i < g_CurrentCcaRanges->ccaFreqCnt; i++ )
        {
            // set frequency
            Radio.SetChannel( g_TxFreq + g_CurrentCcaRanges->ccaFreqs[i] );

            if( -(int16_t)(uint16_t)Radio.Rssi( MODEM_FSK ) < CCA_RSSI * 2 )
            {
                // OK
            }
            else
            {
                // NG
                return FALSE;
            }
        }
    }

    return TRUE;
}

static void CancelTx( void )
{
    switch( g_txState )
    {
    case PhyRadioTxState_CCA:
        PHY_RADIO_DEBUG_PRINT("Cca canceled...\r\n");

        // stop CCA timer
        g_initParams.callback.TimerControl( PhyCcaTimer, 0 );

        // stop CCA process fiber
        g_initParams.callback.FiberControl( PhyCcaFiber, FALSE );

        // exit cca process
        g_txState = PhyRadioTxState_IDLE;
        break;

    case PhyRadioTxState_TX:
        // stop TX timer
        g_initParams.callback.TimerControl( PhyTxTimer, 0 );

        // exit tx process
        g_txState = PhyRadioTxState_IDLE;
        break;

    case PhyRadioTxState_IDLE:
        // to do nothing
        break;
    }
}

static void CancelRx( void )
{
    switch( g_rxState )
    {
    case PhyRadioRxState_IDLE:
        // to do nothing
        break;

    case PhyRadioRxState_RX:
    case PhyRadioRxState_RX_ERROR:

        // stop RX Timer
        g_initParams.callback.TimerControl( PhyRxTimer, 0 );

        // exit Rx process
        g_rxState = PhyRadioRxState_IDLE;
        break;
    }
}

static void RadioCallback_TxDone( void )
{
    PHY_RADIO_DEBUG_PRINT("RadioCallback_TxDone\r\n");

    // stop Tx timer
    g_initParams.callback.TimerControl( PhyTxTimer, 0 );

    // exit tx process
    g_txState = PhyRadioTxState_IDLE;

    g_initParams.callback.onTxDone( gErrorNoError_c );
}

static void RadioCallback_RxDone( void )
{
    PhyRadio_RxResultParams_t resultParams;

    PHY_RADIO_DEBUG_PRINT("RadioCallback_RxDone\r\n");

    // exit rx process
    switch( g_rxState )
    {
    case PhyRadioRxState_IDLE:
        // to do nothing
        break;

    case PhyRadioRxState_RX:
        // stop RX Timer
        g_initParams.callback.TimerControl( PhyRxTimer, 0 );

        /* get rx data */
        Radio.ReadRxData( g_rxReqParams.buff,
                          g_rxReqParams.size,
                          &resultParams.recvSize,
                          &resultParams.rxStatus.rssi,
                          &resultParams.rxStatus.snr );

        g_rxState = PhyRadioRxState_IDLE;

        // complete (success)
        g_initParams.callback.onRxDone( gErrorNoError_c, &resultParams );
        break;

    case PhyRadioRxState_RX_ERROR:
        // stop RX Timer
        g_initParams.callback.TimerControl( PhyRxTimer, 0 );

        g_rxState = PhyRadioRxState_IDLE;

        // complete (error)
        g_initParams.callback.onRxDone( gErrorBusy_c, &resultParams );
        break;
    }
}

static void RadioCallback_TxTimeout( void )
{
    PHY_RADIO_DEBUG_PRINT("RadioCallback_TxTimeout\r\n");

    // exit tx process
    g_txState = PhyRadioTxState_IDLE;

    g_initParams.callback.onTxDone( gErrorTimeout_c );
}

static void RadioCallback_RxError( void )
{
    PHY_RADIO_DEBUG_PRINT("RadioCallback_RxError\r\n");

    switch( g_rxState )
    {
    case PhyRadioRxState_IDLE:
        // to do nothing
        break;

    case PhyRadioRxState_RX:
    case PhyRadioRxState_RX_ERROR:
        // restart RX Timer
        g_initParams.callback.TimerControl( PhyRxTimer, 0 );
        g_initParams.callback.TimerControl( PhyRxTimer, RX_TIMEOUT );

        g_rxState = PhyRadioRxState_RX_ERROR;
        break;
    }
}

static void RadioCallback_PreambleDetected(void)
{
    switch( g_rxState )
    {
    case PhyRadioRxState_IDLE:
        // to do nothing
        break;

    case PhyRadioRxState_RX:
    case PhyRadioRxState_RX_ERROR:
        // restart RX Timer
        g_initParams.callback.TimerControl( PhyRxTimer, 0 );
        g_initParams.callback.TimerControl( PhyRxTimer, RX_TIMEOUT );

        g_rxState = PhyRadioRxState_RX;
        break;
    }
}

/*******************************************************************************
* SetRxConfigForCS
*
* This function set modulation params for rx.
*
* Interface assumptions:
*
* Return value:
*
*******************************************************************************/
static void SetRxConfigForCS( uint32_t bandwidth )
{
    Radio.SetRxConfig( MODEM_FSK,
                       bandwidth,
                       50000,
                       0,
                       bandwidth,
                       FSK_PREAMBLE_LEN,
                       FSK_RX_SYMB_TIMEOUT,
                       FSK_IS_FIXLEN,
                       FSK_FIXLEN_PAYLOADLEN,
                       FSK_CRC_ONOFF,
                       0,
                       0,
                       0,
                       TRUE );
}

/*******************************************************************************
* SetRxConfig
*
* This function set modulation params for rx.
*
* Interface assumptions:
*
* Return value:
*
*******************************************************************************/
static void SetRxConfig( const PhyRadio_ModulationParams_t* modParams )
{
#if defined(BLD_USE_FSK_R)
    uint32_t bandwidth;
    uint32_t datarate;
    uint32_t bandwidthAfc;
    uint32_t dummy;
#endif

    switch( g_initParams.protocol )
    {
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
    __cases_Protocol_LORA
        Radio.SetRxConfig( MODEM_LORA,
                           ConfigParam_bw(modParams->LORA.bw),
                           ConfigParam_sf(modParams->LORA.sf),
                           ConfigParam_cr(modParams->LORA.cr),
                           0,
                           LORA_PREAMBLE_LEN,
                           LORA_RX_SYMB_TIMEOUT,
                           LORA_IS_FIXLEN,
                           LORA_FIXLEN_PAYLOADLEN,
                           LORA_CRC_ONOFF,
                           LORA_FREQ_HOP_ONOFF,
                           0,
                           LORA_IQ_INVERTED,
                           TRUE );
        break;
#endif
#if defined(BLD_USE_FSK_R)
    __cases_Protocol_FSK
        ConfigParam_dr( (fskDataRate)modParams->FSK.dr, &bandwidth, &dummy, &datarate, &bandwidthAfc );

        Radio.SetRxConfig( MODEM_FSK,
                           bandwidth,
                           datarate,
                           0,
                           bandwidthAfc,
                           FSK_PREAMBLE_LEN,
                           FSK_RX_SYMB_TIMEOUT,
                           FSK_IS_FIXLEN,
                           FSK_FIXLEN_PAYLOADLEN,
                           FSK_CRC_ONOFF,
                           0,
                           0,
                           0,
                           TRUE );
        break;
#endif
    }
}

/*******************************************************************************
* SetTxConfig
*
* This function set modulation params for tx.
*
* Interface assumptions:
*
* Return value:
*
*******************************************************************************/
static void SetTxConfig( const PhyRadio_ModulationParams_t* modParams, const PhyRadio_PaParams_t* paParams, uint8_t size, uint32_t timeout )
{
#if defined(BLD_USE_FSK_R)
    uint32_t bandwidth;
    uint32_t fdev;
    uint32_t datarate;
    uint32_t dummy;
#endif

    switch( g_initParams.protocol )
    {
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
    __cases_Protocol_LORA
        Radio.SetTxConfig( MODEM_LORA,
                           paParams->power,
                           0,
                           ConfigParam_bw(modParams->LORA.bw),
                           ConfigParam_sf(modParams->LORA.sf),
                           ConfigParam_cr(modParams->LORA.cr),
                           LORA_PREAMBLE_LEN,
                           LORA_IS_FIXLEN,
                           size,
                           LORA_CRC_ONOFF,
                           LORA_FREQ_HOP_ONOFF,
                           0,
                           LORA_IQ_INVERTED,
                           0xFFFFFFFF );
        break;
#endif

#if defined(BLD_USE_FSK_R)
    __cases_Protocol_FSK
        ConfigParam_dr( (fskDataRate)modParams->FSK.dr, &bandwidth, &fdev, &datarate, &dummy );

        Radio.SetTxConfig( MODEM_FSK,
                           paParams->power,
                           fdev,
                           bandwidth,
                           datarate,
                           0,
                           FSK_PREAMBLE_LEN,
                           FSK_IS_FIXLEN,
                           size,
                           FSK_CRC_ONOFF,
                           0,
                           0,
                           0,
                           timeout );
        break;
#endif
    }
}

static void StartTx( const PhyRadio_TxParams_t* params )
{
    // calc timeout
    const uint32_t timeout = PhyRadio_TimeOnAir( &params->radio.modulation, params->size ) + MARGIN;

    g_txState = PhyRadioTxState_TX;

    SUBGRF_ClearIrqStatus( 0xFFFF );

    // start transmit
    SetTxConfig( &params->radio.modulation, &params->radio.pa, params->size, timeout );

    Radio.Send( params->buff, params->size );

    // start tx timer
    g_initParams.callback.TimerControl( PhyTxTimer, timeout );
}

#if defined(BLD_USE_FSK_R)
static void ConfigParam_dr( fskDataRate dr, uint32_t* bandwidth, uint32_t* fdev, uint32_t* datarate, uint32_t* bandwidthAfc )
{
    switch( dr )
    {
    default:

    case RATE_50KBPS:
        *bandwidth    = 83300;
        *fdev         = 25000;
        *datarate     = 50000;
        *bandwidthAfc = 125000;
        break;

    case RATE_100KBPS:
        *bandwidth    = 166700;
        *fdev         = 50000;
        *datarate     = 100000;
        *bandwidthAfc = 250000;
        break;

    case RATE_150KBPS:
        *bandwidth    = 250000;
        *fdev         = 75000;
        *datarate     = 150000;
        *bandwidthAfc = 375000;
        break;

    case RATE_200KBPS:
        *bandwidth    = 333300;
        *fdev         = 100000;
        *datarate     = 200000;
        *bandwidthAfc = 500000;
        break;

    case RATE_250KBPS:
        *bandwidth    = 416700;
        *fdev         = 125000;
        *datarate     = 250000;
        *bandwidthAfc = 625000;
        break;
    }
}
#endif

#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
static RadioLoRaSpreadingFactors_t ConfigParam_sf( uint8_t sf )
{
    // SF
    switch( sf )
    {
    case 5:
        return LORA_SF5;
    case 6:
        return LORA_SF6;
    default:
    case 7:
        return LORA_SF7;
    case 8:
        return LORA_SF8;
    case 9:
        return LORA_SF9;
    case 10:
        return LORA_SF10;
    case 11:
        return LORA_SF11;
    case 12:
        return LORA_SF12;
    }
}

static RadioLoRaBandwidths_t ConfigParam_bw( uint8_t bw )
{
    // BW
    switch( bw )
    {
    case BANDWIDTH7_8:
        return LORA_BW_007;
    case BANDWIDTH10_4:
        return LORA_BW_010;
    case BANDWIDTH15_6:
        return LORA_BW_015;
    case BANDWIDTH20_8:
        return LORA_BW_020;
    case BANDWIDTH31_25:
        return LORA_BW_031;
    case BANDWIDTH41_7:
        return LORA_BW_041;
    case BANDWIDTH62_5:
        return LORA_BW_062;
    default:
    case BANDWIDTH125:
        return LORA_BW_125;
    case BANDWIDTH250:
        return LORA_BW_250;
    case BANDWIDTH500:
        return LORA_BW_500;
    }
}

static RadioLoRaCodingRates_t ConfigParam_cr( uint8_t cr )
{
    // CR
    switch( cr )
    {
    default:
    case CR_4_5:
        return LORA_CR_4_5;
    case CR_4_6:
        return LORA_CR_4_6;
    case CR_4_7:
        return LORA_CR_4_7;
    case CR_4_8:
        return LORA_CR_4_8;
    }
}
#endif
