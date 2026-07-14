/*!
 * \file      radio.c
 *
 * \brief     Radio driver API definition
 *
 * \copyright Revised BSD License, see section \ref LICENSE.
 *
 * \code
 *                ______                              _
 *               / _____)             _              | |
 *              ( (____  _____ ____ _| |_ _____  ____| |__
 *               \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 *               _____) ) ____| | | || |_| ____( (___| | | |
 *              (______/|_____)_|_|_| \__)_____)\____)_| |_|
 *              (C)2013-2017 Semtech
 *
 * \endcode
 *
 * \author    Miguel Luis ( Semtech )
 *
 * \author    Gregory Cristian ( Semtech )
 */
/**
  ******************************************************************************
  *
  *          Portions COPYRIGHT 2019 STMicroelectronics
  *
  * @file    radio.c
  * @author  MCD Application Team
  * @brief   radio API definition
  ******************************************************************************
 */
/* Includes ------------------------------------------------------------------*/
#include <math.h>
#include "usr_common.h"
#include "stack/radio/radio.h"
#include "stack/radio/radio_driver.h"
#include "stack/radio/radio_bsp.h"

#define TimerGetCurrentTime()       ( HAL_GetTick() )
#define TimerGetElapsedTime(t0)     ( (HAL_GetTick()-(t0)) )

#define SYNCWORD    { 0x55, 0x55, 0x90, 0x4E, 0x90, 0x4E, 0x90, 0x4E }
#define SYNCWORDLEN 4

#define RADIO_RX_BUF_SIZE           255

/* **********************************************************************
 * Middleware Interface functions prototypes
 ************************************************************************/

/*!
 * \brief Initializes the radio
 *
 * \param [IN] events Structure containing the driver callback functions
 */
void RadioInit( RadioEvents_t *events );

/*!
 * Return current radio status
 *
 * \param status Radio status.[RF_IDLE, RF_RX_RUNNING, RF_TX_RUNNING]
 */
RadioState_t RadioGetStatus( void );

/*!
 * \brief Configures the radio with the given modem
 *
 * \param [IN] modem Modem to be used [0: FSK, 1: LoRa]
 */
void RadioSetModem( RadioModems_t modem );

/*!
 * \brief Sets the channel frequency
 *
 * \param [IN] freq         Channel RF frequency
 */
void RadioSetChannel( uint32_t freq );

/*!
 * \brief Checks if the channel is free for the given time
 *
 * \param [IN] modem      Radio modem to be used [0: FSK, 1: LoRa]
 * \param [IN] freq       Channel RF frequency
 * \param [IN] rssiThresh RSSI threshold
 * \param [IN] maxCarrierSenseTime Max time while the RSSI is measured
 *
 * \retval isFree         [true: Channel is free, false: Channel is not free]
 */
bool RadioIsChannelFree( RadioModems_t modem, uint32_t freq, int16_t rssiThresh, uint32_t maxCarrierSenseTime );

/*!
 * \brief Generates a 32 bits random value based on the RSSI readings
 *
 * \remark This function sets the radio in LoRa modem mode and disables
 *         all interrupts.
 *         After calling this function either Radio.SetRxConfig or
 *         Radio.SetTxConfig functions must be called.
 *
 * \retval randomValue    32 bits random value
 */
uint32_t RadioRandom( void );

/*!
 * \brief Sets the reception parameters
 *
 * \param [IN] modem        Radio modem to be used [0: FSK, 1: LoRa]
 * \param [IN] bandwidth    Sets the bandwidth
 *                          FSK : >= 2600 and <= 250000 Hz
 *                          LoRa: [0: 125 kHz, 1: 250 kHz,
 *                                 2: 500 kHz, 3: Reserved]
 * \param [IN] datarate     Sets the Datarate
 *                          FSK : 600..300000 bits/s
 *                          LoRa: [6: 64, 7: 128, 8: 256, 9: 512,
 *                                10: 1024, 11: 2048, 12: 4096  chips]
 * \param [IN] coderate     Sets the coding rate (LoRa only)
 *                          FSK : N/A ( set to 0 )
 *                          LoRa: [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
 * \param [IN] bandwidthAfc Sets the AFC Bandwidth (FSK only)
 *                          FSK : >= 2600 and <= 250000 Hz
 *                          LoRa: N/A ( set to 0 )
 * \param [IN] preambleLen  Sets the Preamble length
 *                          FSK : Number of bytes
 *                          LoRa: Length in symbols (the hardware adds 4 more symbols)
 * \param [IN] symbTimeout  Sets the RxSingle timeout value
 *                          FSK : timeout in number of bytes
 *                          LoRa: timeout in symbols
 * \param [IN] fixLen       Fixed length packets [0: variable, 1: fixed]
 * \param [IN] payloadLen   Sets payload length when fixed length is used
 * \param [IN] crcOn        Enables/Disables the CRC [0: OFF, 1: ON]
 * \param [IN] FreqHopOn    Enables disables the intra-packet frequency hopping
 *                          FSK : N/A ( set to 0 )
 *                          LoRa: [0: OFF, 1: ON]
 * \param [IN] HopPeriod    Number of symbols between each hop
 *                          FSK : N/A ( set to 0 )
 *                          LoRa: Number of symbols
 * \param [IN] iqInverted   Inverts IQ signals (LoRa only)
 *                          FSK : N/A ( set to 0 )
 *                          LoRa: [0: not inverted, 1: inverted]
 * \param [IN] rxContinuous Sets the reception in continuous mode
 *                          [false: single mode, true: continuous mode]
 */
void RadioSetRxConfig( RadioModems_t modem, uint32_t bandwidth,
                        uint32_t datarate, uint8_t coderate,
                        uint32_t bandwidthAfc, uint16_t preambleLen,
                        uint16_t symbTimeout, bool fixLen,
                        uint8_t payloadLen,
                        bool crcOn, bool FreqHopOn, uint8_t HopPeriod,
                        bool iqInverted, bool rxContinuous );

/*!
 * \brief Sets the transmission parameters
 *
 * \param [IN] modem        Radio modem to be used [0: FSK, 1: LoRa]
 * \param [IN] power        Sets the output power [dBm]
 * \param [IN] fdev         Sets the frequency deviation (FSK only)
 *                          FSK : [Hz]
 *                          LoRa: 0
 * \param [IN] bandwidth    Sets the bandwidth (LoRa only)
 *                          FSK : 0
 *                          LoRa: [0: 125 kHz, 1: 250 kHz,
 *                                 2: 500 kHz, 3: Reserved]
 * \param [IN] datarate     Sets the Datarate
 *                          FSK : 600..300000 bits/s
 *                          LoRa: [6: 64, 7: 128, 8: 256, 9: 512,
 *                                10: 1024, 11: 2048, 12: 4096  chips]
 * \param [IN] coderate     Sets the coding rate (LoRa only)
 *                          FSK : N/A ( set to 0 )
 *                          LoRa: [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
 * \param [IN] preambleLen  Sets the preamble length
 *                          FSK : Number of bytes
 *                          LoRa: Length in symbols (the hardware adds 4 more symbols)
 * \param [IN] fixLen       Fixed length packets [0: variable, 1: fixed]
 * \param [IN] crcOn        Enables disables the CRC [0: OFF, 1: ON]
 * \param [IN] FreqHopOn    Enables disables the intra-packet frequency hopping
 *                          FSK : N/A ( set to 0 )
 *                          LoRa: [0: OFF, 1: ON]
 * \param [IN] HopPeriod    Number of symbols between each hop
 *                          FSK : N/A ( set to 0 )
 *                          LoRa: Number of symbols
 * \param [IN] iqInverted   Inverts IQ signals (LoRa only)
 *                          FSK : N/A ( set to 0 )
 *                          LoRa: [0: not inverted, 1: inverted]
 * \param [IN] timeout      Transmission timeout [ms]
 */
void RadioSetTxConfig( RadioModems_t modem, int8_t power, uint32_t fdev,
                        uint32_t bandwidth, uint32_t datarate,
                        uint8_t coderate, uint16_t preambleLen,
                        bool fixLen, uint8_t payloadLength, bool crcOn, bool FreqHopOn,
                        uint8_t HopPeriod, bool iqInverted, uint32_t timeout );

/*!
 * \brief Checks if the given RF frequency is supported by the hardware
 *
 * \param [IN] frequency RF frequency to be checked
 * \retval isSupported [true: supported, false: unsupported]
 */
bool RadioCheckRfFrequency( uint32_t frequency );

/*!
 * \brief Computes the packet time on air in ms for the given payload
 *
 * \Remark Can only be called once SetRxConfig or SetTxConfig have been called
 *
 * \param [IN] modem      Radio modem to be used [0: FSK, 1: LoRa]
 * \param [IN] pktLen     Packet payload length
 * \param [IN] fdev         Sets the frequency deviation (FSK only)
 *                          FSK : [Hz]
 *                          LoRa: 0
 * \param [IN] bandwidth    Sets the bandwidth (LoRa only)
 *                          FSK : 0
 *                          LoRa: [0: 125 kHz, 1: 250 kHz,
 *                                 2: 500 kHz, 3: Reserved]
 * \param [IN] datarate     Sets the Datarate
 *                          FSK : 600..300000 bits/s
 *                          LoRa: [6: 64, 7: 128, 8: 256, 9: 512,
 *                                10: 1024, 11: 2048, 12: 4096  chips]
 * \param [IN] coderate     Sets the coding rate (LoRa only)
 *                          FSK : N/A ( set to 0 )
 *                          LoRa: [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
 * \param [IN] preambleLen  Sets the preamble length
 *                          FSK : Number of bytes
 *                          LoRa: Length in symbols (the hardware adds 4 more symbols)
 * \param [IN] fixLen       Fixed length packets [0: variable, 1: fixed]
 * \param [IN] crcOn        Enables disables the CRC [0: OFF, 1: ON]
 * \param [IN] FreqHopOn    Enables disables the intra-packet frequency hopping
 *                          FSK : N/A ( set to 0 )
 *                          LoRa: [0: OFF, 1: ON]
 * \param [IN] HopPeriod    Number of symbols between each hop
 *                          FSK : N/A ( set to 0 )
 *                          LoRa: Number of symbols
 * \param [IN] iqInverted   Inverts IQ signals (LoRa only)
 *                          FSK : N/A ( set to 0 )
 *                          LoRa: [0: not inverted, 1: inverted]
 *
 * \retval airTime        Computed airTime (ms) for the given packet payload length
 */
uint32_t RadioTimeOnAir( RadioModems_t modem, uint8_t pktLen, uint32_t fdev,
                         uint32_t bandwidth, uint32_t datarate,
                         uint8_t coderate, uint16_t preambleLen,
                         bool fixLen, bool crcOn, bool freqHopOn,
                         uint8_t hopPeriod, bool iqInverted );

/*!
 * \brief Sends the buffer of size. Prepares the packet to be sent and sets
 *        the radio in transmission
 *
 * \param [IN]: buffer     Buffer pointer
 * \param [IN]: size       Buffer size
 */
void RadioSend( uint8_t *buffer, uint8_t size );

/*!
 * \brief Sets the radio in sleep mode
 */
void RadioSleep( bool isWarmStart );

/*!
 * \brief Sets the radio in standby mode
 */
void RadioStandby( void );

/*!
 * \brief Sets the radio in reception mode for the given time
 * \param [IN] timeout Reception timeout [ms]
 *                     [0: continuous, others timeout]
 */
void RadioRx( uint32_t timeout );

/*!
 * \brief Read received data
 * \param [OUT] buffer      Buffer pointer
 * \param [IN] size         Buffer size
 * \param [OUT] datasize    Data size
 * \param [OUT] rssi        RSSI
 * \param [OUT] snr         SNR
 */
void RadioReadRxData( uint8_t* buff, uint8_t size, uint8_t* datasize, uint8_t* rssi, int8_t* snr );

/*!
 * \brief Start a Channel Activity Detection
 */
void RadioStartCad( void );

/*!
 * \brief Sets the radio in continuous wave transmission mode
 *
 * \param [IN]: freq       Channel RF frequency
 * \param [IN]: power      Sets the output power [dBm]
 * \param [IN]: time       Transmission mode timeout [s]
 */
void RadioSetTxContinuousWave( uint32_t freq, int8_t power, uint16_t time );

/*!
 * \brief Sets the radio in TX infinite preamble mode
 *
 */
void RadioSetTxInfinitePreamble( void );

/*!
 * \brief Reads the current RSSI value
 *
 * \retval rssiValue Current RSSI value in [dBm]
 */
uint8_t RadioRssi( RadioModems_t modem );

/*!
 * \brief Writes the radio register at the specified address
 *
 * \param [IN]: addr Register address
 * \param [IN]: data New register value
 */
void RadioWrite( uint16_t addr, uint8_t data );

/*!
 * \brief Reads the radio register at the specified address
 *
 * \param [IN]: addr Register address
 * \retval data Register value
 */
uint8_t RadioRead( uint16_t addr );

/*!
 * \brief Writes multiple radio registers starting at address
 *
 * \param [IN] addr   First Radio register address
 * \param [IN] buffer Buffer containing the new register's values
 * \param [IN] size   Number of registers to be written
 */
void RadioWriteBuffer( uint16_t addr, uint8_t *buffer, uint8_t size );

/*!
 * \brief Reads multiple radio registers starting at address
 *
 * \param [IN] addr First Radio register address
 * \param [OUT] buffer Buffer where to copy the registers data
 * \param [IN] size Number of registers to be read
 */
void RadioReadBuffer( uint16_t addr, uint8_t *buffer, uint8_t size );

/*!
 * \brief Writes multiple radio registers starting at address
 *
 * \param [IN] buffer Buffer containing the new register's values
 * \param [IN] size   Number of registers to be written
 */
void RadioWriteFifo( uint8_t *buffer, uint8_t size );

/*!
 * \brief Reads multiple radio registers starting at address
 *
 * \param [OUT] buffer Buffer where to copy the registers data
 * \param [IN] size Number of registers to be read
 */
void RadioReadFifo( uint8_t *buffer, uint8_t size );

/*!
 * \brief Sets the maximum payload length.
 *
 * \param [IN] modem      Radio modem to be used [0: FSK, 1: LoRa]
 * \param [IN] max        Maximum payload length in bytes
 */
void RadioSetMaxPayloadLength( RadioModems_t modem, uint8_t max );

/*!
 * \brief Sets the network to public or private. Updates the sync byte.
 *
 * \remark Applies to LoRa modem only
 *
 * \param [IN] enable if true, it enables a public network
 */
void RadioSetPublicNetwork( bool enable );

/*!
 * \brief Gets the time required for the board plus radio to get out of sleep.[ms]
 *
 * \retval time Radio plus board wakeup time in ms.
 */
uint32_t RadioGetWakeUpTime( void );

/*!
 * \brief Process radio irq
 */
uint16_t RadioIrqProcess( void );

/*!
 * \brief Sets the radio in reception mode with Max LNA gain for the given time
 * \param [IN] timeout Reception timeout [ms]
 *                     [0: continuous, others timeout]
 */
void RadioRxBoosted( uint32_t timeout );

/*!
 * \brief Sets the Rx duty cycle management parameters
 *
 * \param [in]  rxTime        Structure describing reception timeout value
 * \param [in]  sleepTime     Structure describing sleep timeout value
 */
void RadioSetRxDutyCycle( uint32_t rxTime, uint32_t sleepTime );

 /*
 * Private global constants
 */

/*!
 * Radio driver structure initialization
 */
const struct Radio_s Radio =
{
    RadioInit,
    RadioGetStatus,
    RadioSetModem,
    RadioSetChannel,
    RadioIsChannelFree,
    RadioRandom,
    RadioSetRxConfig,
    RadioSetTxConfig,
    RadioCheckRfFrequency,
    RadioTimeOnAir,
    RadioSend,
    RadioSleep,
    RadioStandby,
    RadioRx,
    RadioReadRxData,
    RadioStartCad,
    RadioSetTxContinuousWave,
    RadioSetTxInfinitePreamble,
    RadioRssi,
    RadioWrite,
    RadioRead,
    RadioWriteFifo,
    RadioReadFifo,
    RadioSetMaxPayloadLength,
    RadioSetPublicNetwork,
    RadioGetWakeUpTime,
    RadioIrqProcess,
    RadioRxBoosted,
    RadioSetRxDutyCycle
};

static void MakeModulationParams( ModulationParams_t* params, RadioModems_t modem, uint32_t fdev, uint32_t bandwidth, uint32_t datarate, uint8_t coderate );
static void MakePacketParams( PacketParams_t* params, RadioModems_t modem, const ModulationParams_t* modulationParams, uint16_t preambleLen, bool fixLen, uint8_t maxPayloadLength, bool crcOn, bool freqHopOn, uint8_t hopPeriod, bool iqInverted );
static void MakeModulationParams_FSK( ModulationParams_t* params, uint32_t fdev, uint32_t bandwidth, uint32_t datarate );
static void MakeModulationParams_LoRa( ModulationParams_t* params, uint32_t bandwidth, uint32_t datarate, uint8_t coderate );
static void MakePacketParams_FSK( PacketParams_t* params, uint16_t preambleLen, bool fixLen, uint8_t maxPayloadLength, bool crcOn, bool freqHopOn );
static void MakePacketParams_LoRa( PacketParams_t* params, RadioLoRaSpreadingFactors_t sf, uint16_t preambleLen, bool fixLen, uint8_t maxPayloadLength, bool crcOn, bool iqInverted );

/*
 * Local types definition
 */

 /*!
 * FSK bandwidth definition
 */
typedef struct
{
    uint32_t bandwidth;
    uint8_t  RegValue;
} FskBandwidth_t;

/*!
 * Precomputed FSK bandwidth registers values
 */
const FskBandwidth_t FskBandwidths[] =
{
    { 4800  , 0x1F },
    { 5800  , 0x17 },
    { 7300  , 0x0F },
    { 9700  , 0x1E },
    { 11700 , 0x16 },
    { 14600 , 0x0E },
    { 19500 , 0x1D },
    { 23400 , 0x15 },
    { 29300 , 0x0D },
    { 39000 , 0x1C },
    { 46900 , 0x14 },
    { 58600 , 0x0C },
    { 78200 , 0x1B },
    { 93800 , 0x13 },
    { 117300, 0x0B },
    { 156200, 0x1A },
    { 187200, 0x12 },
    { 234300, 0x0A },
    { 312000, 0x19 },
    { 373600, 0x11 },
    { 467000, 0x09 },
    { 500000, 0x00 }, // Invalid Bandwidth
};

const static uint32_t SymbTime[10][8] = { { 4096, 8192, 16384, 32768, 65536, 131072, 262019, 524456, },  //  7.81 KHz
                                            { 3071, 6142, 12284, 24568, 49136,  98273, 196545, 393090, },  // 10.42 KHz
                                            { 2048, 4096,  8192, 16384, 32768,  65536, 131072, 262019, },  // 15.63 KHz
                                            { 1536, 3072,  6145, 12290, 24580,  49160,  98320, 196639, },  // 20.83 KHz
                                            { 1024, 2048,  4096,  8192, 16384,  32768,  65536, 131072, },  // 31.25 KHz
                                            {  768, 1536,  3072,  6144, 12287,  24574,  49148,  98296, },  // 41.67 KHz
                                            {  512, 1024,  2048,  4096,  8192,  16384,  32768,  65536, },  // 62.5 KHz
                                            {  256,  512,  1024,  2048,  4096,   8192,  16384,  32768, },  // 125 KHz
                                            {  128,  256,   512,  1024,  2048,   4096,   8192,  16384, },  // 250 KHz
                                            {   64,  128,   256,   512,  1024,   2048,   4096,   8192, },  // 500 KHz
                                          };

static uint32_t RadioLoRaSymbTime1000( RadioLoRaBandwidths_t bw, RadioLoRaSpreadingFactors_t sf )
{
    uint8_t i, j;

    switch( bw )
    {
    case LORA_BW_007:  i = 0;  break;
    case LORA_BW_010:  i = 1;  break;
    case LORA_BW_015:  i = 2;  break;
    case LORA_BW_020:  i = 3;  break;
    case LORA_BW_031:  i = 4;  break;
    case LORA_BW_041:  i = 5;  break;
    case LORA_BW_062:  i = 6;  break;
    case LORA_BW_125:  i = 7;  break;
    case LORA_BW_250:  i = 8;  break;
    case LORA_BW_500:  i = 9;  break;
    default: return 0;
    }

    switch(sf)
    {
    case LORA_SF5:     j = 0;  break;
    case LORA_SF6:     j = 1;  break;
    case LORA_SF7:     j = 2;  break;
    case LORA_SF8:     j = 3;  break;
    case LORA_SF9:     j = 4;  break;
    case LORA_SF10:    j = 5;  break;
    case LORA_SF11:    j = 6;  break;
    case LORA_SF12:    j = 7;  break;
    default: return 0;
    }

    return SymbTime[i][j];
}

static uint8_t MaxPayloadLength = RADIO_RX_BUF_SIZE;

 /* RF switch Power variable */
static uint8_t AntSwitchPower;

static uint8_t RadioRxPayload[RADIO_RX_BUF_SIZE];

/*
 * Private global variables
 */

/*!
 * Radio callbacks variable
 */
static RadioEvents_t* RadioEvents;

/*
 * Public global variables
 */

/*!
 * Radio hardware and global parameters
 */
SubgRf_t SubgRf;

static bool_t IsColdSleep = FALSE;

/*!
 * Returns the known FSK bandwidth registers value
 *
 * \param [IN] bandwidth Bandwidth value in Hz
 * \retval regValue Bandwidth register value.
 */
static uint8_t RadioGetFskBandwidthRegValue( uint32_t bandwidth )
{
    uint8_t i;

    if( bandwidth == 0 )
    {
        return( 0x1F );
    }

    for( i = 0; i < ( sizeof( FskBandwidths ) / sizeof( FskBandwidth_t ) ); i++ )
    {
        if ( bandwidth < FskBandwidths[i].bandwidth )
        {
            return FskBandwidths[i].RegValue;
        }
    }
    // ERROR: Value not found
    while(1);
}

void RadioInit( RadioEvents_t *events )
{
    RadioEvents = events;

    SubgRf.RxContinuous = false;
    SubgRf.TxTimeout = 0;
    SubgRf.RxTimeout = 0;

    if( SUBGRF_Init(RadioOnDioIrq) != SUBGRF_OK )
    {
        /* Initialization Error */
        while(1);
    }

    SUBGRF_SetStandby( STDBY_RC );

    /*SubgRf.publicNetwork set to false*/
    RadioSetPublicNetwork( false );

    SUBGRF_SetRegulatorMode( );

    SUBGRF_SetBufferBaseAddress( 0x00, 0x00 );

    SUBGRF_SetTxParams( RFO_LP, 0, RADIO_RAMP_200_US );

    /* Radio IRQ is set to DIO1 by default */
    SUBGRF_SetDioIrqParams( IRQ_RADIO_ALL, IRQ_RADIO_ALL, IRQ_RADIO_NONE, IRQ_RADIO_NONE );

    SUBGRF_SetSyncWord( (uint8_t[])SYNCWORD );
}

RadioState_t RadioGetStatus( void )
{
    switch( SUBGRF_GetOperatingMode( ) )
    {
        case MODE_TX:
            return RF_TX_RUNNING;
        case MODE_RX:
            return RF_RX_RUNNING;
        case RF_CAD:
            return RF_CAD;
        default:
            return RF_IDLE;
    }
}

void RadioSetModem( RadioModems_t modem )
{
    SubgRf.modem = modem;

    switch( modem )
    {
    default:
    case MODEM_FSK:
        SUBGRF_SetPacketType( PACKET_TYPE_GFSK );
        // When switching to GFSK mode the LoRa SyncWord register value is reset
        // Thus, we also reset the RadioPublicNetwork variable
        SubgRf.publicNetwork = false;
        break;
    case MODEM_LORA:
        SUBGRF_SetPacketType( PACKET_TYPE_LORA );
        // Public/Private network register is reset when switching modems
        RadioSetPublicNetwork( SubgRf.publicNetwork );
        break;
    }
}

void RadioSetChannel( uint32_t freq )
{
    SUBGRF_SetRfFrequency( freq );
}

bool RadioIsChannelFree( RadioModems_t modem, uint32_t freq, int16_t rssiThresh, uint32_t maxCarrierSenseTime )
{
    bool status = true;
    uint8_t rssi = 0;
    uint32_t carrierSenseTime = 0;

    RadioStandby( );

    RadioSetModem( modem );

    RadioSetChannel( freq );

    RadioRx( 0 );

    HAL_Delay( 1 );

    carrierSenseTime = TimerGetCurrentTime( );

    // Perform carrier sense for maxCarrierSenseTime
    while( TimerGetElapsedTime(carrierSenseTime) < maxCarrierSenseTime )
    {
        rssi = RadioRssi( modem );

        if( -(int16_t)(uint16_t)rssi > rssiThresh * 2 )
        {
            status = false;
            break;
        }
    }

    RadioStandby( );

    return status;
}

uint32_t RadioRandom( void )
{
    uint32_t rnd = SUBGRF_GetRandom();

    return rnd;
}

void RadioSetRxConfig( RadioModems_t modem, uint32_t bandwidth,
                        uint32_t datarate, uint8_t coderate,
                        uint32_t bandwidthAfc, uint16_t preambleLen,
                        uint16_t symbTimeout, bool fixLen,
                        uint8_t payloadLen,
                        bool crcOn, bool freqHopOn, uint8_t hopPeriod,
                        bool iqInverted, bool rxContinuous )
{
    ModulationParams_t modulationParams;
    PacketParams_t packetParams;

    SubgRf.RxContinuous = rxContinuous;
    if( rxContinuous == true )
    {
        symbTimeout = 0;
    }

    if( fixLen == true )
    {
        MaxPayloadLength = payloadLen;
    }
    else
    {
        MaxPayloadLength = 0xFF;
    }

    uint32_t dummyFdev = 0;

    switch( modem )
    {
    case MODEM_FSK:
        if( SubgRf.ModulationParams.PacketType == PACKET_TYPE_GFSK )
        {
            dummyFdev = SubgRf.ModulationParams.Params.Gfsk.Fdev;
        }
        MakeModulationParams_FSK( &modulationParams, dummyFdev, bandwidth, datarate );
        MakePacketParams_FSK( &packetParams, preambleLen, fixLen, MaxPayloadLength, crcOn, freqHopOn );

        if( SUBGRF_GetOperatingMode() != MODE_STDBY_RC )
        {
            RadioStandby( );
        }
        RadioSetModem( MODEM_FSK );
        SUBGRF_SetModulationParams( &modulationParams );
        SUBGRF_SetPacketParams( &packetParams );

        SubgRf.RxTimeout = ( (uint32_t)symbTimeout * 8 * 1000 ) / datarate;
        break;

    case MODEM_LORA:
        MakeModulationParams_LoRa( &modulationParams, bandwidth, datarate, coderate );
        MakePacketParams_LoRa( &packetParams, modulationParams.Params.LoRa.SpreadingFactor, preambleLen, fixLen, MaxPayloadLength, crcOn, iqInverted );

        SUBGRF_SetLoRaSymbNumTimeout( symbTimeout );

        RadioSetModem( MODEM_LORA );
        SUBGRF_SetModulationParams( &modulationParams );
        SUBGRF_SetPacketParams( &packetParams );

        // WORKAROUND - Optimizing the Inverted IQ Operation, see DS_SX1261-2_V1.2 datasheet chapter 15.4
        if( SubgRf.PacketParams.Params.LoRa.InvertIQ == LORA_IQ_INVERTED )
        {
            // RegIqPolaritySetup = @address 0x0736
            SUBGRF_WriteRegister( 0x0736, SUBGRF_ReadRegister(0x0736) & ~(1 << 2) );
        }
        else
        {
            // RegIqPolaritySetup @address 0x0736
            SUBGRF_WriteRegister( 0x0736, SUBGRF_ReadRegister(0x0736) | (1 << 2) );
        }
        // WORKAROUND END

        // Timeout Max, Timeout handled directly in SetRx function
        SubgRf.RxTimeout = 0xFFFF;
        break;

    default:
        break;
    }
}

void RadioSetTxConfig( RadioModems_t modem, int8_t power, uint32_t fdev,
                        uint32_t bandwidth, uint32_t datarate,
                        uint8_t coderate, uint16_t preambleLen,
                        bool fixLen, uint8_t payloadLength, bool crcOn, bool freqHopOn,
                        uint8_t hopPeriod, bool iqInverted, uint32_t timeout )
{
    ModulationParams_t modulationParams;
    PacketParams_t packetParams;

    switch( modem )
    {
    case MODEM_FSK:
        MakeModulationParams_FSK( &modulationParams, fdev, bandwidth, datarate );
        MakePacketParams_FSK( &packetParams, preambleLen, fixLen, payloadLength, crcOn, freqHopOn );

        if( SUBGRF_GetOperatingMode() != MODE_STDBY_RC )
        {
            RadioStandby( );
        }
        RadioSetModem( MODEM_FSK );
        SUBGRF_SetModulationParams( &modulationParams );
        SUBGRF_SetPacketParams( &packetParams );
        break;

    case MODEM_LORA:
        MakeModulationParams_LoRa( &modulationParams, bandwidth, datarate, coderate );
        MakePacketParams_LoRa( &packetParams, modulationParams.Params.LoRa.SpreadingFactor, preambleLen, fixLen, payloadLength, crcOn, iqInverted );

        if( SUBGRF_GetOperatingMode() != MODE_STDBY_RC )
        {
            RadioStandby( );
        }
        RadioSetModem( MODEM_LORA );
        SUBGRF_SetModulationParams( &modulationParams );
        SUBGRF_SetPacketParams( &packetParams );
        break;
    }

    // WORKAROUND - Modulation Quality with 500 kHz LoRa Bandwidth, see DS_SX1261-2_V1.2 datasheet chapter 15.1
    if( (modem == MODEM_LORA) && (modulationParams.Params.LoRa.Bandwidth == LORA_BW_500) )
    {
        // RegTxModulation = @address 0x0889
        SUBGRF_WriteRegister( 0x0889, SUBGRF_ReadRegister(0x0889) & ~(1 << 2) );
    }
    else
    {
        // RegTxModulation = @address 0x0889
        SUBGRF_WriteRegister( 0x0889, SUBGRF_ReadRegister(0x0889) | (1 << 2) );
    }
    // WORKAROUND END

    AntSwitchPower = SUBGRF_SetTxPower( power, RADIO_RAMP_40_US );
    SubgRf.TxTimeout = timeout;
}

static void MakeModulationParams( ModulationParams_t* params, RadioModems_t modem, uint32_t fdev, uint32_t bandwidth, uint32_t datarate, uint8_t coderate )
{
    switch( modem )
    {
    case MODEM_FSK:
        params->PacketType                      = PACKET_TYPE_GFSK;
        params->Params.Gfsk.BitRate             = datarate;
        params->Params.Gfsk.ModulationShaping   = MOD_SHAPING_G_BT_05;
        params->Params.Gfsk.Bandwidth           = RadioGetFskBandwidthRegValue( bandwidth );
        params->Params.Gfsk.Fdev                = fdev;
        break;

    case MODEM_LORA:
        params->PacketType                      = PACKET_TYPE_LORA;
        params->Params.LoRa.SpreadingFactor     = (RadioLoRaSpreadingFactors_t)datarate;
        params->Params.LoRa.Bandwidth           = (RadioLoRaBandwidths_t)bandwidth;
        params->Params.LoRa.CodingRate          = (RadioLoRaCodingRates_t)coderate;
        params->Params.LoRa.LowDatarateOptimize = 0x01;
        break;
    }
}

static void MakeModulationParams_FSK( ModulationParams_t* params, uint32_t fdev, uint32_t bandwidth, uint32_t datarate )
{
    params->PacketType                      = PACKET_TYPE_GFSK;
    params->Params.Gfsk.BitRate             = datarate;
    params->Params.Gfsk.ModulationShaping   = MOD_SHAPING_G_BT_05;
    params->Params.Gfsk.Bandwidth           = RadioGetFskBandwidthRegValue( bandwidth );
    params->Params.Gfsk.Fdev                = fdev;
}

static void MakeModulationParams_LoRa( ModulationParams_t* params, uint32_t bandwidth, uint32_t datarate, uint8_t coderate )
{
    params->PacketType                      = PACKET_TYPE_LORA;
    params->Params.LoRa.SpreadingFactor     = (RadioLoRaSpreadingFactors_t)datarate;
    params->Params.LoRa.Bandwidth           = (RadioLoRaBandwidths_t)bandwidth;
    params->Params.LoRa.CodingRate          = (RadioLoRaCodingRates_t)coderate;
    params->Params.LoRa.LowDatarateOptimize = 0x01;
}

static void MakePacketParams( PacketParams_t* params, RadioModems_t modem, const ModulationParams_t* modulationParams,
                                uint16_t preambleLen, bool fixLen, uint8_t maxPayloadLength, bool crcOn, bool freqHopOn,
                                uint8_t hopPeriod, bool iqInverted)
{
    switch( modem )
    {
    case MODEM_FSK:
        params->PacketType                    = PACKET_TYPE_GFSK;
        params->Params.Gfsk.PreambleLength    = ( preambleLen << 3 ); // convert byte into bit
        params->Params.Gfsk.PreambleMinDetect = RADIO_PREAMBLE_DETECTOR_32_BITS;
        params->Params.Gfsk.SyncWordLength    = SYNCWORDLEN << 3 ; // convert byte into bit
        params->Params.Gfsk.AddrComp          = RADIO_ADDRESSCOMP_FILT_OFF;
        params->Params.Gfsk.HeaderType        = ( fixLen == true ) ? RADIO_PACKET_FIXED_LENGTH : RADIO_PACKET_VARIABLE_LENGTH;
        params->Params.Gfsk.PayloadLength     = maxPayloadLength;

        if( crcOn == true )
        {
            params->Params.Gfsk.CrcLength     = RADIO_CRC_2_BYTES_CCIT;
        }
        else
        {
            params->Params.Gfsk.CrcLength     = RADIO_CRC_OFF;
        }

        params->Params.Gfsk.DcFree            = RADIO_DC_FREE_OFF;
        break;

    case MODEM_LORA:
        params->PacketType = PACKET_TYPE_LORA;

        if( ( modulationParams->Params.LoRa.SpreadingFactor == LORA_SF5 ) ||
            ( modulationParams->Params.LoRa.SpreadingFactor == LORA_SF6 ) )
        {
            if( preambleLen < 12 )
            {
                params->Params.LoRa.PreambleLength = 12;
            }
            else
            {
                params->Params.LoRa.PreambleLength = preambleLen;
            }
        }
        else
        {
            params->Params.LoRa.PreambleLength = preambleLen;
        }

        params->Params.LoRa.HeaderType    = ( RadioLoRaPacketLengthsMode_t )fixLen;
        params->Params.LoRa.PayloadLength = maxPayloadLength;
        params->Params.LoRa.CrcMode       = ( RadioLoRaCrcModes_t )crcOn;
        params->Params.LoRa.InvertIQ      = ( RadioLoRaIQModes_t )iqInverted;
        break;
    }
}

static void MakePacketParams_FSK( PacketParams_t* params,
                                    uint16_t preambleLen, bool fixLen, uint8_t maxPayloadLength, bool crcOn, bool freqHopOn )
{
    params->PacketType                    = PACKET_TYPE_GFSK;
    params->Params.Gfsk.PreambleLength    = ( preambleLen << 3 ); // convert byte into bit
    params->Params.Gfsk.PreambleMinDetect = RADIO_PREAMBLE_DETECTOR_32_BITS;
    params->Params.Gfsk.SyncWordLength    = SYNCWORDLEN << 3 ; // convert byte into bit
    params->Params.Gfsk.AddrComp          = RADIO_ADDRESSCOMP_FILT_OFF;
    params->Params.Gfsk.HeaderType        = ( fixLen == true ) ? RADIO_PACKET_FIXED_LENGTH : RADIO_PACKET_VARIABLE_LENGTH;
    params->Params.Gfsk.PayloadLength     = maxPayloadLength;

    if( crcOn == true )
    {
        params->Params.Gfsk.CrcLength     = RADIO_CRC_2_BYTES_CCIT;
    }
    else
    {
        params->Params.Gfsk.CrcLength     = RADIO_CRC_OFF;
    }

    params->Params.Gfsk.DcFree            = RADIO_DC_FREE_OFF;
}

static void MakePacketParams_LoRa( PacketParams_t* params, RadioLoRaSpreadingFactors_t sf,
                                     uint16_t preambleLen, bool fixLen, uint8_t maxPayloadLength, bool crcOn, bool iqInverted )
{
    params->PacketType = PACKET_TYPE_LORA;

    if( ( sf == LORA_SF5 ) || ( sf == LORA_SF6 ) )
    {
        if( preambleLen < 12 )
        {
            params->Params.LoRa.PreambleLength = 12;
        }
        else
        {
            params->Params.LoRa.PreambleLength = preambleLen;
        }
    }
    else
    {
        params->Params.LoRa.PreambleLength = preambleLen;
    }

    params->Params.LoRa.HeaderType      = ( RadioLoRaPacketLengthsMode_t )fixLen;
    params->Params.LoRa.PayloadLength   = maxPayloadLength;
    params->Params.LoRa.CrcMode         = ( RadioLoRaCrcModes_t )crcOn;
    params->Params.LoRa.InvertIQ        = ( RadioLoRaIQModes_t )iqInverted;
}

bool RadioCheckRfFrequency( uint32_t frequency )
{
    return true;
}

uint32_t RadioTimeOnAir( RadioModems_t modem, uint8_t pktLen, uint32_t fdev,
                         uint32_t bandwidth, uint32_t datarate,
                         uint8_t coderate, uint16_t preambleLen,
                         bool fixLen, bool crcOn, bool freqHopOn,
                         uint8_t hopPeriod, bool iqInverted )
{
    uint32_t airTime = 0;

    ModulationParams_t modulationParams;
    PacketParams_t packetParams;

    MakeModulationParams( &modulationParams, modem, fdev, bandwidth, datarate, coderate );
    MakePacketParams( &packetParams, modem, &modulationParams, preambleLen, fixLen, MaxPayloadLength, crcOn, freqHopOn, hopPeriod, iqInverted );

    switch( modem )
    {
    case MODEM_FSK:
        {
            uint32_t tmp2 = ( ( (uint32_t)( packetParams.Params.Gfsk.PreambleLength + packetParams.Params.Gfsk.SyncWordLength >> 3 )
                              + ( ( packetParams.Params.Gfsk.HeaderType == RADIO_PACKET_FIXED_LENGTH ) ? 0 : 1 )
                              + pktLen
                              + ( ( packetParams.Params.Gfsk.CrcLength == RADIO_CRC_2_BYTES ) ? 2 : 0 )
                              )
                              * 8000
                            );
            airTime = div_round( tmp2, modulationParams.Params.Gfsk.BitRate );
        }
        break;
    case MODEM_LORA:
        {
            uint32_t ts1000 = RadioLoRaSymbTime1000( modulationParams.Params.LoRa.Bandwidth, modulationParams.Params.LoRa.SpreadingFactor );

            // time of preamble
            uint32_t tPreamble1000 = packetParams.Params.LoRa.PreambleLength * ts1000 + ts1000 * 425 / 100;

            // Symbol length of payload and time
            int32_t tmp =  (int32_t)8 * pktLen - 4 * modulationParams.Params.LoRa.SpreadingFactor +
                            28 + 16 * packetParams.Params.LoRa.CrcMode -
                            ( ( packetParams.Params.LoRa.HeaderType == LORA_PACKET_FIXED_LENGTH ) ? 20 : 0 );
            int32_t tmp2 = 4 * ( (int32_t)modulationParams.Params.LoRa.SpreadingFactor -
                            ( ( modulationParams.Params.LoRa.LowDatarateOptimize > 0 ) ? 2 : 0 ) );
            tmp = div_ceil(tmp, tmp2) * ( ( modulationParams.Params.LoRa.CodingRate % 4 ) + 4 );
            uint32_t nPayload = 8 + ( ( tmp > 0 ) ? (uint32_t)tmp : 0 );
            uint32_t tPayload1000 = nPayload * ts1000;

            // Time on air
            uint32_t tOnAir1000 = tPreamble1000 + tPayload1000;

            // return milli seconds
            airTime = div_ceil( tOnAir1000, 1000 );
        }
        break;
    }
    return airTime;
}

void RadioSend( uint8_t *buffer, uint8_t size )
{
    PacketParams_t params;

    /* Radio IRQ is set to DIO1 by default */
    SUBGRF_SetDioIrqParams( IRQ_TX_DONE | IRQ_RX_TX_TIMEOUT,
                            IRQ_TX_DONE | IRQ_RX_TX_TIMEOUT,
                            IRQ_RADIO_NONE,
                            IRQ_RADIO_NONE );

    /* Set RF switch */
    SUBGRF_SetSwitch( AntSwitchPower, RFSWITCH_TX );

    params = SubgRf.PacketParams;
    if( SUBGRF_GetPacketType( ) == PACKET_TYPE_LORA )
    {
        params.Params.LoRa.PayloadLength = size;
    }
    else
    {
        params.Params.Gfsk.PayloadLength = size;
    }

    SUBGRF_SetPacketParams( &params );

    SUBGRF_SendPayload( buffer, size, 0 );
}

void RadioSleep( bool isWarmStart )
{
    SleepParams_t params = { 0 };

    params.Fields.WarmStart = isWarmStart ? 1 : 0;
    SUBGRF_SetSleep( params );

    if( !isWarmStart )
    {
        IsColdSleep = TRUE;
    }
}

void RadioStandby( void )
{
    SUBGRF_SetStandby( STDBY_RC );

    if( IsColdSleep )
    {
        SUBGRF_SetRegulatorMode();
        SUBGRF_SetTxParams( RFO_LP, 0, RADIO_RAMP_200_US );
        SUBGRF_SetSyncWord( (uint8_t[])SYNCWORD );
        IsColdSleep = FALSE;
    }
}

void RadioRx( uint32_t timeout )
{
    uint16_t irqMask = IRQ_RX_DONE | IRQ_RX_TX_TIMEOUT;

    if( ( RadioEvents != NULL ) && ( RadioEvents->SyncWordValid != NULL ) )
    {
        irqMask |= IRQ_SYNCWORD_VALID;
    }

    if( ( RadioEvents != NULL ) && ( RadioEvents->PreambleDetected != NULL ) )
    {
        irqMask |= IRQ_PREAMBLE_DETECTED;
    }

    if( ( RadioEvents != NULL ) && ( RadioEvents->RxError != NULL ) )
    {
        irqMask |= IRQ_HEADER_ERROR | IRQ_CRC_ERROR;
    }

    /* Radio IRQ is set to DIO1 by default */
    SUBGRF_SetDioIrqParams( irqMask,
                            irqMask,
                            IRQ_RADIO_NONE,
                            IRQ_RADIO_NONE );

    /* RF switch configuration */
    SUBGRF_SetSwitch( AntSwitchPower, RFSWITCH_RX );

    if( SubgRf.RxContinuous == true )
    {
        SUBGRF_SetRx( 0xFFFFFF ); // Rx Continuous
    }
    else
    {
        SUBGRF_SetRx( SubgRf.RxTimeout << 6 );
    }
}

void RadioReadRxData( uint8_t* buff, uint8_t size, uint8_t* datasize, uint8_t* rssi, int8_t* snr )
{
    PacketStatus_t pktStatus;

    SUBGRF_GetPayload( buff, datasize , size );

    SUBGRF_GetPacketStatus( &pktStatus );

    switch( pktStatus.packetType )
    {
    case PACKET_TYPE_GFSK:
        if( rssi )
        {
            *rssi = pktStatus.Params.Gfsk.RssiAvg;
        }
        if( snr )
        {
            *snr = 0;
        }
        break;

    case PACKET_TYPE_LORA:
        if( rssi )
        {
            *rssi = pktStatus.Params.LoRa.RssiPkt;
        }
        if( snr )
        {
            *snr = pktStatus.Params.LoRa.SnrPkt;
        }
        break;

    default:
        break;
    }
}

void RadioRxBoosted( uint32_t timeout )
{
    uint16_t irqMask = IRQ_RX_DONE | IRQ_RX_TX_TIMEOUT;

    if( ( RadioEvents != NULL ) && ( RadioEvents->SyncWordValid != NULL ) )
    {
        irqMask |= IRQ_SYNCWORD_VALID;
    }

    if( ( RadioEvents != NULL ) && ( RadioEvents->PreambleDetected != NULL ) )
    {
        irqMask |= IRQ_PREAMBLE_DETECTED;
    }

    if( ( RadioEvents != NULL ) && ( RadioEvents->RxError != NULL ) )
    {
        irqMask |= IRQ_HEADER_ERROR | IRQ_CRC_ERROR;
    }

    SUBGRF_SetDioIrqParams( irqMask,
                            irqMask,
                            IRQ_RADIO_NONE,
                            IRQ_RADIO_NONE );

    /* RF switch configuration */
    SUBGRF_SetSwitch( AntSwitchPower, RFSWITCH_RX );

    if( SubgRf.RxContinuous == true )
    {
        SUBGRF_SetRxBoosted( 0xFFFFFF ); // Rx Continuous
    }
    else
    {
        SUBGRF_SetRxBoosted( SubgRf.RxTimeout << 6 );
    }
}

void RadioSetRxDutyCycle( uint32_t rxTime, uint32_t sleepTime )
{
    /* RF switch configuration */
    SUBGRF_SetSwitch( AntSwitchPower, RFSWITCH_RX );
    SUBGRF_SetRxDutyCycle( rxTime, sleepTime );
}

void RadioStartCad( void )
{
    SUBGRF_SetCad( );
}

void RadioSetTxContinuousWave( uint32_t freq, int8_t power, uint16_t time )
{
    uint8_t antswitchpow;

    SUBGRF_SetRfFrequency( freq );

    antswitchpow = SUBGRF_SetTxPower( power, RADIO_RAMP_40_US );

    /* Set RF switch */
    SUBGRF_SetSwitch( antswitchpow, RFSWITCH_TX );

    SUBGRF_SetTxContinuousWave( );
}

void RadioSetTxInfinitePreamble( void )
{
    PacketParams_t params;

    params = SubgRf.PacketParams;
    if( SUBGRF_GetPacketType( ) == PACKET_TYPE_LORA )
    {
        params.Params.LoRa.PayloadLength = 0;
    }
    else
    {
        params.Params.Gfsk.PayloadLength = 0;
    }
    SUBGRF_SetPacketParams( &params );

    SUBGRF_SetTxInfinitePreamble( );
}

uint8_t RadioRssi( RadioModems_t modem )
{
    return SUBGRF_GetRssiInst( );
}

void RadioWrite( uint16_t addr, uint8_t data )
{
    SUBGRF_WriteRegister(addr, data );
}

uint8_t RadioRead( uint16_t addr )
{
    return SUBGRF_ReadRegister(addr);
}

void RadioWriteBuffer( uint16_t addr, uint8_t *buffer, uint8_t size )
{
    SUBGRF_WriteRegisters( addr, buffer, size );
}

void RadioReadBuffer( uint16_t addr, uint8_t *buffer, uint8_t size )
{
    SUBGRF_ReadRegisters( addr, buffer, size );
}

void RadioWriteFifo( uint8_t *buffer, uint8_t size )
{
    RadioWriteBuffer( 0, buffer, size );
}

void RadioReadFifo( uint8_t *buffer, uint8_t size )
{
    RadioReadBuffer( 0, buffer, size );
}

void RadioSetMaxPayloadLength( RadioModems_t modem, uint8_t max )
{
    if( modem == MODEM_LORA )
    {
        SubgRf.PacketParams.Params.LoRa.PayloadLength = MaxPayloadLength = max;
    }
    else
    {
        SubgRf.PacketParams.Params.Gfsk.PayloadLength = MaxPayloadLength = max;
    }

    SUBGRF_SetPacketParams( &SubgRf.PacketParams );
}

void RadioSetPublicNetwork( bool enable )
{
    uint8_t reg;

    SubgRf.publicNetwork= enable;

    if( enable == true )
    {
        // Change LoRa modem SyncWord
        reg = ( LORA_MAC_PUBLIC_SYNCWORD >> 8 ) & 0xFF;
        SUBGRF_WriteRegisters( REG_LR_SYNCWORD, (uint8_t*)&reg, 1 );
        reg = LORA_MAC_PUBLIC_SYNCWORD & 0xFF;
        SUBGRF_WriteRegisters( REG_LR_SYNCWORD + 1, (uint8_t*)&reg, 1 );
    }
    else
    {
        // Change LoRa modem SyncWord
        reg = ( LORA_MAC_PRIVATE_SYNCWORD >> 8 ) & 0xFF;
        SUBGRF_WriteRegisters( REG_LR_SYNCWORD, (uint8_t*)&reg, 1 );
        reg = LORA_MAC_PRIVATE_SYNCWORD & 0xFF;
        SUBGRF_WriteRegisters( REG_LR_SYNCWORD + 1, (uint8_t*)&reg, 1 );
    }
}

uint32_t RadioGetWakeUpTime( void )
{
  return (SUBGRF_GetRadioWakeUpTime() + RADIO_WAKEUP_TIME);
}

void RadioOnDioIrq( RadioIrqMasks_t radioIrq )
{
    SubgRf.radioIrq |= radioIrq;
}

uint16_t RadioIrqProcess( void )
{
    PacketStatus_t RadioPktStatus;
    uint8_t size;

    if( ( SubgRf.radioIrq & IRQ_TX_DONE ) == IRQ_TX_DONE )
    {
        SubgRf.radioIrq &= ~IRQ_TX_DONE;

        SUBGRF_SetStandby( STDBY_RC );
        if( ( RadioEvents != NULL ) && ( RadioEvents->TxDone != NULL ) )
        {
            RadioEvents->TxDone( );
        }
    }

    if( ( SubgRf.radioIrq & IRQ_HEADER_ERROR ) == IRQ_HEADER_ERROR )
    {
        SubgRf.radioIrq &= ~IRQ_HEADER_ERROR;

        if( SubgRf.RxContinuous == false )
        {
            //!< Update operating mode state to a value lower than \ref MODE_STDBY_XOSC
            SUBGRF_SetStandby( STDBY_RC );
        }
        if( ( RadioEvents != NULL ) && ( RadioEvents->RxError ) )
        {
            RadioEvents->RxError( );
        }
    }

    if( ( SubgRf.radioIrq & IRQ_CRC_ERROR ) == IRQ_CRC_ERROR )
    {
        SubgRf.radioIrq &= ~IRQ_CRC_ERROR;

        if( SubgRf.RxContinuous == false )
        {
            //!< Update operating mode state to a value lower than \ref MODE_STDBY_XOSC
            SUBGRF_SetStandby( STDBY_RC );
        }
        if( ( RadioEvents != NULL ) && ( RadioEvents->RxError ) )
        {
            RadioEvents->RxError( );
        }
    }

    if( ( SubgRf.radioIrq & IRQ_RX_DONE ) == IRQ_RX_DONE )
    {
        SubgRf.radioIrq &= ~IRQ_RX_DONE;

        if( SubgRf.RxContinuous == false )
        {
            //!< Update operating mode state to a value lower than \ref MODE_STDBY_XOSC
            SUBGRF_SetStandby( STDBY_RC );

            // WORKAROUND - Implicit Header Mode Timeout Behavior, see DS_SX1261-2_V1.2 datasheet chapter 15.3
            // RegRtcControl = @address 0x0902
            SUBGRF_WriteRegister( 0x0902, 0x00 );

            // RegEventMask = @address 0x0944
            SUBGRF_WriteRegister( 0x0944, SUBGRF_ReadRegister( 0x0944 ) | ( 1 << 1 ) );
            // WORKAROUND END
        }

        SUBGRF_GetPayload( RadioRxPayload, &size, 255 );
        SUBGRF_GetPacketStatus( &RadioPktStatus );

        if( ( RadioEvents != NULL ) && ( RadioEvents->RxDone != NULL ) )
        {
            RadioEvents->RxDone();
        }
    }

    if( ( SubgRf.radioIrq & IRQ_CAD_CLEAR ) == IRQ_CAD_CLEAR )
    {
        SubgRf.radioIrq &= ~IRQ_CAD_CLEAR;

        //!< Update operating mode state to a value lower than \ref MODE_STDBY_XOSC
        SUBGRF_SetStandby( STDBY_RC );
        if( ( RadioEvents != NULL ) && ( RadioEvents->CadDone != NULL ) )
        {
            RadioEvents->CadDone( false );
        }
    }

    if( ( SubgRf.radioIrq & IRQ_CAD_DETECTED ) == IRQ_CAD_DETECTED )
    {
        SubgRf.radioIrq &= ~IRQ_CAD_DETECTED;

        //!< Update operating mode state to a value lower than \ref MODE_STDBY_XOSC
        SUBGRF_SetStandby( STDBY_RC );
        if( ( RadioEvents != NULL ) && ( RadioEvents->CadDone != NULL ) )
        {
            RadioEvents->CadDone( true );
        }
    }

    if( ( SubgRf.radioIrq & IRQ_RX_TX_TIMEOUT ) == IRQ_RX_TX_TIMEOUT )
    {
        SubgRf.radioIrq &= ~IRQ_RX_TX_TIMEOUT;

        if( SUBGRF_GetOperatingMode( ) == MODE_TX )
        {
            //!< Update operating mode state to a value lower than \ref MODE_STDBY_XOSC
            SUBGRF_SetStandby( STDBY_RC );
            if( ( RadioEvents != NULL ) && ( RadioEvents->TxTimeout != NULL ) )
            {
                RadioEvents->TxTimeout( );
            }
        }
        else if( SUBGRF_GetOperatingMode( ) == MODE_RX )
        {
            //!< Update operating mode state to a value lower than \ref MODE_STDBY_XOSC
            SUBGRF_SetStandby( STDBY_RC );
            if( ( RadioEvents != NULL ) && ( RadioEvents->RxTimeout != NULL ) )
            {
                RadioEvents->RxTimeout( );
            }
        }
    }

    if( ( SubgRf.radioIrq & IRQ_PREAMBLE_DETECTED ) == IRQ_PREAMBLE_DETECTED )
    {
        SubgRf.radioIrq &= ~IRQ_PREAMBLE_DETECTED;

        if( ( SUBGRF_GetOperatingMode( ) == MODE_RX ) &&
            ( RadioEvents != NULL ) && ( RadioEvents->PreambleDetected != NULL ) )
        {
            RadioEvents->PreambleDetected();
        }
    }

    return 0;
}
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
