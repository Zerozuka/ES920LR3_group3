/*
  ______                              _
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2017 Semtech

Description: Generic SX126x driver implementation

License: Revised BSD License, see LICENSE.TXT file include in the project

Authors: Miguel Luis, Gregory Cristian
*/
/**
  ******************************************************************************
  *
  *          Portions COPYRIGHT 2019 STMicroelectronics
  *
  * @file    radio_driver.c
  * @author  MCD Application Team
  * @brief   radio driver implementation
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <math.h>
#include "subghz.h"
#include "usr_common.h"
#include "platform/timerctl.h"
#include "platform/powerctl.h"
#include "stack/radio/radio_bsp.h"
#include "stack/radio/radio_driver.h"

/*
 * Local types definition
 */
#define XTA_TRIM    0xB
#define XTB_TRIM    0xB

/*!
 * @brief Radio registers definition
 */
typedef struct
{
    uint16_t      Addr;         //!< The address of the register
    uint8_t       Value;        //!< The value of the register
} RadioRegisters_t;

typedef enum
{
    RF_API_STATE_TCXO_OFF = 0,
    RF_API_STATE_TCXO_ON
} RF_API_Tcxo_State_t;

/*!
* @This is the workaround for tcxo bug in cut 1.0
*/
static void TCXO_Workaround( RF_API_Tcxo_State_t state );

/*!
* @This is the workaround for Reset bug in cut 1.0
*/
static void RadioReset_Workaround( void );

/*!
 * @brief Holds the internal operating mode of the radio
 */
RadioOperatingModes_t OperatingMode;

/*!
 * @brief Stores the current packet type set in the radio
 */
static RadioPacketTypes_t PacketType;

/*!
 * @brief Stores the last frequency error measured on LoRa received packet
 */
volatile uint32_t FrequencyError = 0;

/*!
 * @brief Hold the status of the Image calibration
 */
static bool ImageCalibrated = false;

/*
 * Private functions prototypes
 */
void (*RadioOnDioIrqCb) ( RadioIrqMasks_t radioIrq );

/*
 * Public functions
 */

uint32_t SUBGRF_GetRadioWakeUpTime( void )
{
    return ( uint32_t )BSP_RADIO_GetWakeUpTime();
}

RadioOperatingModes_t SUBGRF_GetOperatingMode( void )
{
    return OperatingMode;
}

int32_t SUBGRF_Init( void (*RadioOnDioIrq) ( RadioIrqMasks_t radioIrq ) )
{
    memset( &SubgRf.PacketParams, 0, sizeof(SubgRf.PacketParams) );
    memset( &SubgRf.ModulationParams, 0, sizeof(SubgRf.ModulationParams) );
    PacketType = SubgRf.ModulationParams.PacketType = SubgRf.PacketParams.PacketType = PACKET_TYPE_NONE;

    if ( RadioOnDioIrq != NULL )
    {
        RadioOnDioIrqCb = RadioOnDioIrq;
    }

    TCXO_Workaround( RF_API_STATE_TCXO_ON );

    ImageCalibrated = false;

    RadioReset_Workaround();

    SUBGRF_SetStandby( STDBY_RC );

    if( 1U == BSP_RADIO_IsTCXO() )
    {
        uint8_t reg = 0x2F;

        SUBGRF_SetTcxoMode( TCXO_CTRL_1_8V, 64 * BSP_RADIO_GetWakeUpTime() );// 100 ms

        SUBGRF_WriteRegisters( REG_XTA_TRIM, (uint8_t*)&reg, 1 );

        /*enable calibration for cut1.1 and later*/
        if( LL_DBGMCU_GetRevisionID() !=0x1000 )
        {
            CalibrationParams_t calibParam;
            calibParam.Value = 0x7F;
            SUBGRF_Calibrate( calibParam );
        }
    }

    /* Init RF Switch */
    BSP_RADIO_Init();

    OperatingMode = MODE_STDBY_RC;

    TCXO_Workaround( RF_API_STATE_TCXO_OFF );

    return SUBGRF_OK;
}

void SUBGRF_SetPayload( uint8_t *payload, uint8_t size )
{
    HAL_NVIC_DisableIRQ( SUBGHZ_Radio_IRQn );
    HAL_SUBGHZ_WriteBuffer( &hsubghz, 0x00, payload, size );
    HAL_NVIC_EnableIRQ( SUBGHZ_Radio_IRQn );
}

uint8_t SUBGRF_GetPayload( uint8_t *buffer, uint8_t *size,  uint8_t maxSize )
{
    uint8_t offset = 0;

    SUBGRF_GetRxBufferStatus( size, &offset );
    if( *size > maxSize )
    {
        return 1;
    }

    HAL_NVIC_DisableIRQ( SUBGHZ_Radio_IRQn );
    HAL_SUBGHZ_ReadBuffer( &hsubghz, offset, buffer, *size );
    HAL_NVIC_EnableIRQ( SUBGHZ_Radio_IRQn );

    return 0;
}

void SUBGRF_SendPayload( uint8_t *payload, uint8_t size, uint32_t timeout )
{
    SUBGRF_SetPayload( payload, size );
    SUBGRF_SetTx( timeout );
}

void SUBGRF_SetSwitch( uint8_t power, RFState_t rxtx )
{
    BSP_RADIO_Switch_TypeDef state;

    if( rxtx == RFSWITCH_TX )
    {
        if( power == RFO_LP )
        {
            state = RADIO_SWITCH_RFO_LP;
        }
        if( power == RFO_HP )
        {
            state = RADIO_SWITCH_RFO_HP;
        }
    }
    else if( rxtx == RFSWITCH_RX )
    {
        state = RADIO_SWITCH_RX;
    }

    BSP_RADIO_ConfigRFSwitch( state );
}

uint8_t SUBGRF_SetSyncWord( uint8_t *syncWord )
{
    SUBGRF_WriteRegisters(  REG_LR_SYNCWORDBASEADDRESS, syncWord, 8 );

    return 0;
}

void SUBGRF_SetCrcSeed( uint16_t seed )
{
    uint8_t buf[2];

    buf[0] = ( uint8_t )( ( seed >> 8 ) & 0xFF );
    buf[1] = ( uint8_t )( seed & 0xFF );

    switch( SUBGRF_GetPacketType( ) )
    {
    case PACKET_TYPE_GFSK:
        SUBGRF_WriteRegisters( REG_LR_CRCSEEDBASEADDR, buf, 2 );
        break;
    default:
        break;
    }
}

void SUBGRF_SetCrcPolynomial( uint16_t polynomial )
{
    uint8_t buf[2];

    buf[0] = ( uint8_t )( ( polynomial >> 8 ) & 0xFF );
    buf[1] = ( uint8_t )( polynomial & 0xFF );

    switch( SUBGRF_GetPacketType( ) )
    {
    case PACKET_TYPE_GFSK:
        SUBGRF_WriteRegisters( REG_LR_CRCPOLYBASEADDR, buf, 2 );
        break;
    default:
        break;
    }
}

void SUBGRF_SetWhiteningSeed( uint16_t seed )
{
    uint8_t regValue = 0;

    switch( SUBGRF_GetPacketType( ) )
    {
    case PACKET_TYPE_GFSK:
        SUBGRF_ReadRegisters( REG_LR_WHITSEEDBASEADDR_MSB, &regValue, 1 );
        regValue = regValue & 0xFE;
        regValue = ( ( seed >> 8 ) & 0x01 ) | regValue;
        SUBGRF_WriteRegisters( REG_LR_WHITSEEDBASEADDR_MSB, (uint8_t*)&regValue, 1 ); // only 1 bit.
        SUBGRF_WriteRegisters( REG_LR_WHITSEEDBASEADDR_LSB, (uint8_t*)&seed, 1 );
        break;
    default:
        break;
    }
}

uint32_t SUBGRF_GetRandom( void )
{
    uint8_t buf[] = { 0, 0, 0, 0 };

    // Set radio in continuous reception
    SUBGRF_SetRfFrequency( 921e6 );

    SUBGRF_SetRx( 0 );

    HAL_Delay( 1 );

    SUBGRF_ReadRegisters( RANDOM_NUMBER_GENERATORBASEADDR, buf, 4 );

    SUBGRF_SetStandby( STDBY_RC );

    return ( buf[0] << 24 ) | ( buf[1] << 16 ) | ( buf[2] << 8 ) | buf[3];
}

void SUBGRF_SetSleep( SleepParams_t sleepConfig )
{
    BSP_RADIO_ConfigRFSwitch( RADIO_SWITCH_OFF ); /* switch the antenna OFF by SW */

    SUBGRF_ExecSetCmd( RADIO_SET_SLEEP, &sleepConfig.Value, 1 );

    OperatingMode = MODE_SLEEP;

    TCXO_Workaround( RF_API_STATE_TCXO_OFF );

    if( !sleepConfig.Fields.WarmStart )
    {
        memset( &SubgRf.PacketParams, 0, sizeof(SubgRf.PacketParams) );
        memset( &SubgRf.ModulationParams, 0, sizeof(SubgRf.ModulationParams) );

        PacketType = SubgRf.ModulationParams.PacketType = SubgRf.PacketParams.PacketType = PACKET_TYPE_NONE;
    }
}

void SUBGRF_SetStandby( RadioStandbyModes_t standbyConfig )
{
    SUBGRF_ExecSetCmd( RADIO_SET_STANDBY, ( uint8_t* )&standbyConfig, 1 );

    if( standbyConfig == STDBY_RC )
    {
        OperatingMode = MODE_STDBY_RC;
    }
    else
    {
        OperatingMode = MODE_STDBY_XOSC;

        // wait for complete of transition state
        RadioStatus_t status;
        do
        {
            status = SUBGRF_GetStatus();
        } while( status.Fields.ChipMode != 0x3 );

        // set capacitor trim
        SUBGRF_WriteRegister( REG_XTA_TRIM, XTA_TRIM );
        SUBGRF_WriteRegister( REG_XTB_TRIM, XTB_TRIM );
    }
}

void SUBGRF_SetFs( void )
{
    SUBGRF_ExecSetCmd( RADIO_SET_FS, 0, 0 );

    OperatingMode = MODE_FS;
}

void SUBGRF_SetTx( uint32_t timeout )
{
    uint8_t buf[3];

    OperatingMode = MODE_TX;

    buf[0] = ( uint8_t )( ( timeout >> 16 ) & 0xFF );
    buf[1] = ( uint8_t )( ( timeout >> 8 ) & 0xFF );
    buf[2] = ( uint8_t )( timeout & 0xFF );

    SUBGRF_ExecSetCmd( RADIO_SET_TX, buf, 3 );

    SUBGRF_WriteRegister( REG_XTA_TRIM, XTA_TRIM );
    SUBGRF_WriteRegister( REG_XTB_TRIM, XTB_TRIM );
}

void SUBGRF_SetRx( uint32_t timeout )
{
    uint8_t buf[3];

    OperatingMode = MODE_RX;

    SUBGRF_WriteRegister( REG_RX_GAIN, 0x94 ); // Rx power saving gain (default)

    buf[0] = ( uint8_t )( ( timeout >> 16 ) & 0xFF );
    buf[1] = ( uint8_t )( ( timeout >> 8 ) & 0xFF );
    buf[2] = ( uint8_t )( timeout & 0xFF );

    SUBGRF_ExecSetCmd( RADIO_SET_RX, buf, 3 );

    SUBGRF_WriteRegister( REG_XTA_TRIM, XTA_TRIM );
    SUBGRF_WriteRegister( REG_XTB_TRIM, XTB_TRIM );
}

void SUBGRF_SetRxBoosted( uint32_t timeout )
{
    uint8_t buf[3];

    OperatingMode = MODE_RX;

    SUBGRF_WriteRegister( REG_RX_GAIN, 0x96 ); // max LNA gain, increase current by ~2mA for around ~3dB in sensivity

    buf[0] = ( uint8_t )( ( timeout >> 16 ) & 0xFF );
    buf[1] = ( uint8_t )( ( timeout >> 8 ) & 0xFF );
    buf[2] = ( uint8_t )( timeout & 0xFF );

    SUBGRF_ExecSetCmd( RADIO_SET_RX, buf, 3 );

    SUBGRF_WriteRegister( REG_XTA_TRIM, XTA_TRIM );
    SUBGRF_WriteRegister( REG_XTB_TRIM, XTB_TRIM );
}

void SUBGRF_SetRxDutyCycle( uint32_t rxTime, uint32_t sleepTime )
{
    uint8_t buf[6];

    buf[0] = ( uint8_t )( ( rxTime >> 16 ) & 0xFF );
    buf[1] = ( uint8_t )( ( rxTime >> 8 ) & 0xFF );
    buf[2] = ( uint8_t )( rxTime & 0xFF );
    buf[3] = ( uint8_t )( ( sleepTime >> 16 ) & 0xFF );
    buf[4] = ( uint8_t )( ( sleepTime >> 8 ) & 0xFF );
    buf[5] = ( uint8_t )( sleepTime & 0xFF );

    SUBGRF_ExecSetCmd( RADIO_SET_RXDUTYCYCLE, buf, 6 );

    OperatingMode = MODE_RX_DC;
}

void SUBGRF_SetCad( void )
{
    SUBGRF_ExecSetCmd( RADIO_SET_CAD, 0, 0 );

    OperatingMode = MODE_CAD;
}

void SUBGRF_SetTxContinuousWave( void )
{
    SUBGRF_ExecSetCmd( RADIO_SET_TXCONTINUOUSWAVE, 0, 0 );

    SUBGRF_WriteRegister( REG_XTA_TRIM, XTA_TRIM );
    SUBGRF_WriteRegister( REG_XTB_TRIM, XTB_TRIM );
}

void SUBGRF_SetTxInfinitePreamble( void )
{
    SUBGRF_ExecSetCmd( RADIO_SET_TXCONTINUOUSPREAMBLE, 0, 0 );

    SUBGRF_WriteRegister( REG_XTA_TRIM, XTA_TRIM );
    SUBGRF_WriteRegister( REG_XTB_TRIM, XTB_TRIM );
}

void SUBGRF_SetStopRxTimerOnPreambleDetect( bool enable )
{
    SUBGRF_ExecSetCmd( RADIO_SET_STOPRXTIMERONPREAMBLE, ( uint8_t* )&enable, 1 );
}

void SUBGRF_SetLoRaSymbNumTimeout( uint8_t SymbNum )
{
    SUBGRF_ExecSetCmd( RADIO_SET_LORASYMBTIMEOUT, &SymbNum, 1 );
}

void SUBGRF_SetRegulatorMode( void )
{
    RadioRegulatorMode_t mode;

    if( 1U == BSP_RADIO_IsDCDC() )
    {
        mode = USE_DCDC;
    }
    else
    {
        mode = USE_LDO;
    }

    SUBGRF_ExecSetCmd( RADIO_SET_REGULATORMODE, ( uint8_t* )&mode, 1 );
}

void SUBGRF_Calibrate( CalibrationParams_t calibParam )
{
    SUBGRF_ExecSetCmd( RADIO_CALIBRATE, ( uint8_t* )&calibParam, 1 );
}

void SUBGRF_CalibrateImage( uint32_t freq )
{
    uint8_t calFreq[2];

    if( freq > 900000000 )
    {
        calFreq[0] = 0xE1;
        calFreq[1] = 0xE9;
    }
    else if( freq > 850000000 )
    {
        calFreq[0] = 0xD7;
        calFreq[1] = 0xDB;
    }
    else if( freq > 770000000 )
    {
        calFreq[0] = 0xC1;
        calFreq[1] = 0xC5;
    }
    else if( freq > 460000000 )
    {
        calFreq[0] = 0x75;
        calFreq[1] = 0x81;
    }
    else if( freq > 425000000 )
    {
        calFreq[0] = 0x6B;
        calFreq[1] = 0x6F;
    }

    SUBGRF_ExecSetCmd( RADIO_CALIBRATEIMAGE, calFreq, 2 );
}

void SUBGRF_SetPaConfig( uint8_t paDutyCycle, uint8_t hpMax, uint8_t deviceSel, uint8_t paLut )
{
    uint8_t buf[4];

    buf[0] = paDutyCycle;
    buf[1] = hpMax;
    buf[2] = deviceSel;
    buf[3] = paLut;

    SUBGRF_ExecSetCmd( RADIO_SET_PACONFIG, buf, 4 );
}

void SUBGRF_SetRxTxFallbackMode( uint8_t fallbackMode )
{
    SUBGRF_ExecSetCmd( RADIO_SET_TXFALLBACKMODE, &fallbackMode, 1 );
}

void SUBGRF_SetDioIrqParams( uint16_t irqMask, uint16_t dio1Mask, uint16_t dio2Mask, uint16_t dio3Mask )
{
    uint8_t buf[8];

    buf[0] = ( uint8_t )( ( irqMask >> 8 ) & 0x00FF );
    buf[1] = ( uint8_t )( irqMask & 0x00FF );
    buf[2] = ( uint8_t )( ( dio1Mask >> 8 ) & 0x00FF );
    buf[3] = ( uint8_t )( dio1Mask & 0x00FF );
    buf[4] = ( uint8_t )( ( dio2Mask >> 8 ) & 0x00FF );
    buf[5] = ( uint8_t )( dio2Mask & 0x00FF );
    buf[6] = ( uint8_t )( ( dio3Mask >> 8 ) & 0x00FF );
    buf[7] = ( uint8_t )( dio3Mask & 0x00FF );

    SUBGRF_ExecSetCmd( RADIO_CFG_DIOIRQ, buf, 8 );
}

uint16_t SUBGRF_GetIrqStatus( void )
{
    uint8_t irqStatus[2];

    SUBGRF_ExecGetCmd( RADIO_GET_IRQSTATUS, irqStatus, 2 );

    return ( irqStatus[0] << 8 ) | irqStatus[1];
}

void SUBGRF_SetTcxoMode (RadioTcxoCtrlVoltage_t tcxoVoltage, uint32_t timeout )
{
    uint8_t buf[4];

    buf[0] = tcxoVoltage & 0x07;
    buf[1] = ( uint8_t )( ( timeout >> 16 ) & 0xFF );
    buf[2] = ( uint8_t )( ( timeout >> 8 ) & 0xFF );
    buf[3] = ( uint8_t )( timeout & 0xFF );

    SUBGRF_ExecSetCmd( RADIO_SET_TCXOMODE, buf, 4 );
}

void SUBGRF_SetRfFrequency( uint32_t frequency )
{
    uint8_t buf[4];
    uint32_t chan;

    TCXO_Workaround( RF_API_STATE_TCXO_ON );

    if( ImageCalibrated == false )
    {
        SUBGRF_CalibrateImage( frequency );
        ImageCalibrated = true;
    }

    SX_FREQ_TO_CHANNEL( chan, frequency );
    buf[0] = ( uint8_t )( ( chan >> 24 ) & 0xFF );
    buf[1] = ( uint8_t )( ( chan >> 16 ) & 0xFF );
    buf[2] = ( uint8_t )( ( chan >> 8 ) & 0xFF );
    buf[3] = ( uint8_t )( chan & 0xFF );

    SUBGRF_ExecSetCmd( RADIO_SET_RFFREQUENCY, buf, 4 );
}

void SUBGRF_SetPacketType( RadioPacketTypes_t packetType )
{
    uint8_t reg = 0x00;

    // Save packet type internally to avoid questioning the radio
    if( PacketType != packetType )
    {
        memset(&SubgRf.PacketParams, 0, sizeof(SubgRf.PacketParams));
        memset(&SubgRf.ModulationParams, 0, sizeof(SubgRf.ModulationParams));
        SubgRf.ModulationParams.PacketType = SubgRf.PacketParams.PacketType = PACKET_TYPE_NONE;

        PacketType = packetType;

        if( packetType == PACKET_TYPE_GFSK )
        {
            SUBGRF_WriteRegisters( REG_BIT_SYNC, (uint8_t*)&reg, 1 );
        }

        SUBGRF_ExecSetCmd( RADIO_SET_PACKETTYPE, ( uint8_t* )&packetType, 1 );
    }
}

RadioPacketTypes_t SUBGRF_GetPacketType( void )
{
    return PacketType;
}

uint8_t SUBGRF_SetTxPower( int8_t power, RadioRampTimes_t rampTime )
{
    uint8_t paSelect;

    int32_t TxConfig = BSP_RADIO_GetTxConfig();

    switch( TxConfig )
    {
    case RADIO_CONF_RFO_LP_HP:
    {
        if( power > 15 )
        {
            paSelect = RFO_HP;
        }
        else
        {
            paSelect = RFO_LP;
        }
        break;
    }
    case RADIO_CONF_RFO_LP:
    {
        paSelect = RFO_LP;
        break;
    }
    case RADIO_CONF_RFO_HP:
    {
        paSelect = RFO_HP;
        break;
    }
    default:
        break;
    }

    SUBGRF_SetTxParams( paSelect, power, rampTime );

    return paSelect;
}

void SUBGRF_SetTxParams( uint8_t paSelect, int8_t power, RadioRampTimes_t rampTime )
{
    uint8_t buf[2];
    uint8_t reg;

    if( paSelect == RFO_LP )
    {
        if( power == 15 )
        {
            SUBGRF_SetPaConfig( 0x06, 0x00, 0x01, 0x01 );
        }
        else
        {
            SUBGRF_SetPaConfig( 0x04, 0x00, 0x01, 0x01 );
        }
        if( power >= 14 )
        {
            power = 14;
        }
        else if( power < -4 )
        {
            power = -4;
        }
        reg = 0x18;
        SUBGRF_WriteRegisters( REG_OCP, (uint8_t*)&reg, 1 ); // current max is 80 mA for the whole device
    }
    else // rfo_hp
    {
        SUBGRF_SetPaConfig( 0x04, 0x07, 0x00, 0x01 );
        if( power > 22 )
        {
            power = 22;
        }
        else if( power < -3 )
        {
            power = -3;
        }
        reg = 0x38;
        SUBGRF_WriteRegisters( REG_OCP, (uint8_t*)&reg, 1 ); // current max 160mA for the whole device
    }
    buf[0] = power;
    buf[1] = ( uint8_t )rampTime;
    SUBGRF_ExecSetCmd( RADIO_SET_TXPARAMS, buf, 2 );
}

void SUBGRF_SetModulationParams( ModulationParams_t *modulationParams )
{
    uint8_t n;
    uint32_t tempVal = 0;
    uint8_t buf[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    // Check if required configuration corresponds to the stored packet type
    // If not, silently update radio packet type
    if( PacketType != modulationParams->PacketType )
    {
        SUBGRF_SetPacketType( modulationParams->PacketType );
    }

    switch( modulationParams->PacketType )
    {
    case PACKET_TYPE_GFSK:
        if( modulationParams->Params.Gfsk.BitRate           != SubgRf.ModulationParams.Params.Gfsk.BitRate ||
            modulationParams->Params.Gfsk.Fdev              != SubgRf.ModulationParams.Params.Gfsk.Fdev ||
            modulationParams->Params.Gfsk.ModulationShaping != SubgRf.ModulationParams.Params.Gfsk.ModulationShaping ||
            modulationParams->Params.Gfsk.Bandwidth         != SubgRf.ModulationParams.Params.Gfsk.Bandwidth )
        {
            n = 8;
            tempVal = ( uint32_t )( (32 * XTAL_FREQ) / modulationParams->Params.Gfsk.BitRate );
            buf[0] = ( tempVal >> 16 ) & 0xFF;
            buf[1] = ( tempVal >> 8 ) & 0xFF;
            buf[2] = tempVal & 0xFF;
            buf[3] = modulationParams->Params.Gfsk.ModulationShaping;
            buf[4] = modulationParams->Params.Gfsk.Bandwidth;
            SX_FREQ_TO_CHANNEL(tempVal, modulationParams->Params.Gfsk.Fdev);
            buf[5] = ( tempVal >> 16 ) & 0xFF;
            buf[6] = ( tempVal >> 8 ) & 0xFF;
            buf[7] = ( tempVal& 0xFF );

            SUBGRF_ExecSetCmd( RADIO_SET_MODULATIONPARAMS, buf, n );

            SubgRf.ModulationParams = *modulationParams;
        }
        break;
    case PACKET_TYPE_LORA:
        if( modulationParams->Params.LoRa.SpreadingFactor       != SubgRf.ModulationParams.Params.LoRa.SpreadingFactor ||
            modulationParams->Params.LoRa.Bandwidth             != SubgRf.ModulationParams.Params.LoRa.Bandwidth ||
            modulationParams->Params.LoRa.CodingRate            != SubgRf.ModulationParams.Params.LoRa.CodingRate ||
            modulationParams->Params.LoRa.LowDatarateOptimize   != SubgRf.ModulationParams.Params.LoRa.LowDatarateOptimize )
        {
            n = 4;
            buf[0] = modulationParams->Params.LoRa.SpreadingFactor;
            buf[1] = modulationParams->Params.LoRa.Bandwidth;
            buf[2] = modulationParams->Params.LoRa.CodingRate;
            buf[3] = modulationParams->Params.LoRa.LowDatarateOptimize;

            SUBGRF_ExecSetCmd( RADIO_SET_MODULATIONPARAMS, buf, n );

            SubgRf.ModulationParams = *modulationParams;
        }
        break;
    default:
    case PACKET_TYPE_NONE:
        break;
    }
}

void SUBGRF_SetPacketParams( PacketParams_t *packetParams )
{
    uint8_t n;
    uint8_t crcVal = 0;
    uint8_t buf[9] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    // Check if required configuration corresponds to the stored packet type
    // If not, silently update radio packet type
    if( PacketType != packetParams->PacketType )
    {
        SUBGRF_SetPacketType( packetParams->PacketType );
    }

    switch( packetParams->PacketType )
    {
    case PACKET_TYPE_GFSK:
        if( packetParams->Params.Gfsk.PreambleLength    != SubgRf.PacketParams.Params.Gfsk.PreambleLength ||
            packetParams->Params.Gfsk.PreambleMinDetect != SubgRf.PacketParams.Params.Gfsk.PreambleMinDetect ||
            packetParams->Params.Gfsk.SyncWordLength    != SubgRf.PacketParams.Params.Gfsk.SyncWordLength ||
            packetParams->Params.Gfsk.AddrComp          != SubgRf.PacketParams.Params.Gfsk.AddrComp ||
            packetParams->Params.Gfsk.HeaderType        != SubgRf.PacketParams.Params.Gfsk.HeaderType ||
            packetParams->Params.Gfsk.PayloadLength     != SubgRf.PacketParams.Params.Gfsk.PayloadLength ||
            packetParams->Params.Gfsk.CrcLength         != SubgRf.PacketParams.Params.Gfsk.CrcLength ||
            packetParams->Params.Gfsk.DcFree            != SubgRf.PacketParams.Params.Gfsk.DcFree )
        {
            if( packetParams->Params.Gfsk.CrcLength == RADIO_CRC_2_BYTES_IBM )
            {
                SUBGRF_SetCrcSeed( CRC_IBM_SEED );
                SUBGRF_SetCrcPolynomial( CRC_POLYNOMIAL_IBM );
                crcVal = RADIO_CRC_2_BYTES;
            }
            else if( packetParams->Params.Gfsk.CrcLength == RADIO_CRC_2_BYTES_CCIT )
            {
                SUBGRF_SetCrcSeed( CRC_CCITT_SEED );
                SUBGRF_SetCrcPolynomial( CRC_POLYNOMIAL_CCITT );
                crcVal = RADIO_CRC_2_BYTES_INV;
            }
            else
            {
                crcVal = packetParams->Params.Gfsk.CrcLength;
            }
            n = 9;
            buf[0] = ( packetParams->Params.Gfsk.PreambleLength >> 8 ) & 0xFF;
            buf[1] = packetParams->Params.Gfsk.PreambleLength;
            buf[2] = packetParams->Params.Gfsk.PreambleMinDetect;
            buf[3] = ( packetParams->Params.Gfsk.SyncWordLength /*<< 3*/ ); // convert from byte to bit
            buf[4] = packetParams->Params.Gfsk.AddrComp;
            buf[5] = packetParams->Params.Gfsk.HeaderType;
            buf[6] = packetParams->Params.Gfsk.PayloadLength;
            buf[7] = crcVal;
            buf[8] = packetParams->Params.Gfsk.DcFree;

            SUBGRF_ExecSetCmd( RADIO_SET_PACKETPARAMS, buf, n );

            SubgRf.PacketParams = *packetParams;
        }
        break;
    case PACKET_TYPE_LORA:
        if( packetParams->Params.LoRa.PreambleLength    != SubgRf.PacketParams.Params.LoRa.PreambleLength ||
            packetParams->Params.LoRa.HeaderType        != SubgRf.PacketParams.Params.LoRa.HeaderType ||
            packetParams->Params.LoRa.PayloadLength     != SubgRf.PacketParams.Params.LoRa.PayloadLength ||
            packetParams->Params.LoRa.CrcMode           != SubgRf.PacketParams.Params.LoRa.CrcMode ||
            packetParams->Params.LoRa.InvertIQ          != SubgRf.PacketParams.Params.LoRa.InvertIQ )
        {
            n = 6;
            buf[0] = ( packetParams->Params.LoRa.PreambleLength >> 8 ) & 0xFF;
            buf[1] = packetParams->Params.LoRa.PreambleLength;
            buf[2] = packetParams->Params.LoRa.HeaderType;
            buf[3] = packetParams->Params.LoRa.PayloadLength;
            buf[4] = packetParams->Params.LoRa.CrcMode;
            buf[5] = packetParams->Params.LoRa.InvertIQ;

            SUBGRF_ExecSetCmd( RADIO_SET_PACKETPARAMS, buf, n );

            SubgRf.PacketParams = *packetParams;
        }
        break;
    default:
    case PACKET_TYPE_NONE:
        return;
    }
}

void SUBGRF_SetCadParams( RadioLoRaCadSymbols_t cadSymbolNum, uint8_t cadDetPeak, uint8_t cadDetMin, RadioCadExitModes_t cadExitMode, uint32_t cadTimeout )
{
    uint8_t buf[7];

    buf[0] = ( uint8_t )cadSymbolNum;
    buf[1] = cadDetPeak;
    buf[2] = cadDetMin;
    buf[3] = ( uint8_t )cadExitMode;
    buf[4] = ( uint8_t )( ( cadTimeout >> 16 ) & 0xFF );
    buf[5] = ( uint8_t )( ( cadTimeout >> 8 ) & 0xFF );
    buf[6] = ( uint8_t )( cadTimeout & 0xFF );

    SUBGRF_ExecSetCmd( RADIO_SET_CADPARAMS, buf, 7 );

    OperatingMode = MODE_CAD;
}

void SUBGRF_SetBufferBaseAddress( uint8_t txBaseAddress, uint8_t rxBaseAddress )
{
    uint8_t buf[2];

    buf[0] = txBaseAddress;
    buf[1] = rxBaseAddress;

    SUBGRF_ExecSetCmd( RADIO_SET_BUFFERBASEADDRESS, buf, 2 );
}

RadioStatus_t SUBGRF_GetStatus( void )
{
    uint8_t stat = 0;
    RadioStatus_t status;

    SUBGRF_ExecGetCmd( RADIO_GET_STATUS, ( uint8_t * )&stat, 1 );

    status.Value = stat;

    return status;
}

uint8_t SUBGRF_GetRssiInst( void )
{
    uint8_t buf[1];
    int8_t rssi = 0;

    SUBGRF_ExecGetCmd( RADIO_GET_RSSIINST, buf, 1 );

    rssi = buf[0];

    return rssi;
}

void SUBGRF_GetRxBufferStatus( uint8_t *payloadLength, uint8_t *rxStartBufferPointer )
{
    uint8_t status[2];
    uint8_t data;

    SUBGRF_ExecGetCmd( RADIO_GET_RXBUFFERSTATUS, status, 2 );

    // In case of LORA fixed header, the payloadLength is obtained by reading
    // the register REG_LR_PAYLOADLENGTH
    SUBGRF_ReadRegisters( REG_LR_PACKETPARAMS, &data, 1 );
    if( ( SUBGRF_GetPacketType( ) == PACKET_TYPE_LORA ) && (data  >> 7 == 1 ) )
    {
        SUBGRF_ReadRegisters( REG_LR_PAYLOADLENGTH, &data, 1 );
        *payloadLength = data;
    }
    else
    {
        *payloadLength = status[0];
    }
    *rxStartBufferPointer = status[1];
}

void SUBGRF_GetPacketStatus( PacketStatus_t *pktStatus )
{
    uint8_t status[3];

    SUBGRF_ExecGetCmd( RADIO_GET_PACKETSTATUS, status, 3 );

    pktStatus->packetType = SUBGRF_GetPacketType( );
    switch( pktStatus->packetType )
    {
        case PACKET_TYPE_GFSK:
            pktStatus->Params.Gfsk.RxStatus = status[0];
            pktStatus->Params.Gfsk.RssiSync = status[1];
            pktStatus->Params.Gfsk.RssiAvg = status[2];
            pktStatus->Params.Gfsk.FreqError = 0;
            break;

        case PACKET_TYPE_LORA:
            pktStatus->Params.LoRa.RssiPkt = status[0];
            pktStatus->Params.LoRa.SnrPkt = status[1];
            pktStatus->Params.LoRa.SignalRssiPkt = status[2];
            pktStatus->Params.LoRa.FreqError = FrequencyError;
            break;

        default:
        case PACKET_TYPE_NONE:
            // In that specific case, we set everything in the pktStatus to zeros
            // and reset the packet type accordingly
            memset( (uint8_t*)pktStatus, 0, sizeof( PacketStatus_t ) );
            pktStatus->packetType = PACKET_TYPE_NONE;
            break;
    }
}

RadioError_t SUBGRF_GetErrors( void )
{
    RadioError_t error;

    SUBGRF_ExecGetCmd( RADIO_GET_ERROR, ( uint8_t * )&error, 2 );

    return error;
}

void SUBGRF_ClearIrqStatus( uint16_t irq )
{
    uint8_t buf[2];

    buf[0] = ( uint8_t )( ( ( uint16_t )irq >> 8 ) & 0x00FF );
    buf[1] = ( uint8_t )( ( uint16_t )irq & 0x00FF );

    SUBGRF_ExecSetCmd( RADIO_CLR_IRQSTATUS, buf, 2 );
}

void SUBGRF_ExecSetCmd( SUBGHZ_RadioSetCmd_t Command, uint8_t *pBuffer, uint16_t Size )
{
    HAL_NVIC_DisableIRQ( SUBGHZ_Radio_IRQn );
    HAL_SUBGHZ_ExecSetCmd( &hsubghz, Command, pBuffer, Size );
    HAL_NVIC_EnableIRQ( SUBGHZ_Radio_IRQn );
}

void SUBGRF_ExecGetCmd( SUBGHZ_RadioGetCmd_t Command, uint8_t *pBuffer, uint16_t Size )
{
    HAL_NVIC_DisableIRQ( SUBGHZ_Radio_IRQn );
    HAL_SUBGHZ_ExecGetCmd( &hsubghz, Command, pBuffer, Size );
    HAL_NVIC_EnableIRQ( SUBGHZ_Radio_IRQn );
}

void SUBGRF_WriteRegister( uint16_t addr, uint8_t data )
{
    HAL_NVIC_DisableIRQ( SUBGHZ_Radio_IRQn );
    HAL_SUBGHZ_WriteRegisters( &hsubghz, addr, (uint8_t*)&data, 1 );
    HAL_NVIC_EnableIRQ( SUBGHZ_Radio_IRQn );
}

uint8_t SUBGRF_ReadRegister( uint16_t addr )
{
    uint8_t data;

    HAL_NVIC_DisableIRQ( SUBGHZ_Radio_IRQn );
    HAL_SUBGHZ_ReadRegisters( &hsubghz, addr, &data, 1 );
    HAL_NVIC_EnableIRQ( SUBGHZ_Radio_IRQn );

    return data;
}

void SUBGRF_WriteRegisters( uint16_t addr, uint8_t *buffer, uint8_t size )
{
    HAL_NVIC_DisableIRQ( SUBGHZ_Radio_IRQn );
    HAL_SUBGHZ_WriteRegisters( &hsubghz, addr, buffer, size );
    HAL_NVIC_EnableIRQ( SUBGHZ_Radio_IRQn );
}

void SUBGRF_ReadRegisters( uint16_t addr, uint8_t *buffer, uint8_t size )
{
    HAL_NVIC_DisableIRQ( SUBGHZ_Radio_IRQn );
    HAL_SUBGHZ_ReadRegisters( &hsubghz, addr, buffer, size );
    HAL_NVIC_EnableIRQ( SUBGHZ_Radio_IRQn );
}

/* HAL_SUBGHz Callbacks definitions */
void HAL_SUBGHZ_TxCpltCallback( SUBGHZ_HandleTypeDef *hsubghz )
{
    RadioOnDioIrqCb( IRQ_TX_DONE );
}

void HAL_SUBGHZ_RxCpltCallback( SUBGHZ_HandleTypeDef *hsubghz )
{
    RadioOnDioIrqCb( IRQ_RX_DONE );
}

void HAL_SUBGHZ_CRCErrorCallback( SUBGHZ_HandleTypeDef *hsubghz )
{
    RadioOnDioIrqCb( IRQ_CRC_ERROR );
}

void HAL_SUBGHZ_CADStatusCallback( SUBGHZ_HandleTypeDef *hsubghz, HAL_SUBGHZ_CadStatusTypeDef cadstatus )
{
    switch ( cadstatus )
    {
    case HAL_SUBGHZ_CAD_CLEAR:
        RadioOnDioIrqCb( IRQ_CAD_CLEAR );
        break;
    case HAL_SUBGHZ_CAD_DETECTED:
        RadioOnDioIrqCb( IRQ_CAD_DETECTED );
        break;
    default:
        break;
    }
}

void HAL_SUBGHZ_RxTxTimeoutCallback( SUBGHZ_HandleTypeDef *hsubghz )
{
    RadioOnDioIrqCb( IRQ_RX_TX_TIMEOUT );
}

void HAL_SUBGHZ_HeaderErrorCallback( SUBGHZ_HandleTypeDef *hsubghz )
{
    RadioOnDioIrqCb( IRQ_HEADER_ERROR );
}

void HAL_SUBGHZ_PreambleDetectedCallback( SUBGHZ_HandleTypeDef *hsubghz )
{
    RadioOnDioIrqCb( IRQ_PREAMBLE_DETECTED );
}

void HAL_SUBGHZ_SyncWordValidCallback( SUBGHZ_HandleTypeDef *hsubghz )
{
    RadioOnDioIrqCb( IRQ_SYNCWORD_VALID );
}

void HAL_SUBGHZ_HeaderValidCallback( SUBGHZ_HandleTypeDef *hsubghz )
{
    RadioOnDioIrqCb( IRQ_HEADER_VALID );
}

/*Workaround 1.0 WL */
#define RCC_CR_HSEBYP            (0x1UL << 18U)

#define RCC_HSE_DisableBypass()  CLEAR_BIT(RCC->CR, RCC_CR_HSEBYP)

#define RCC_HSE_EnableBypass()   SET_BIT(RCC->CR, RCC_CR_HSEBYP)

static void TCXO_Workaround( RF_API_Tcxo_State_t state )
{
    /* Workaround  needed only for cut1.0*/
    uint16_t RevisionID =  LL_DBGMCU_GetRevisionID();

    if( 1U == BSP_RADIO_IsTCXO() && (RevisionID == 0x1000) )
    {
        switch( state )
        {
        case RF_API_STATE_TCXO_OFF:
            LL_RCC_HSE_Disable();

            LL_RCC_HSE_DisableTcxo();

            RCC_HSE_DisableBypass();

            //UTIL_LPM_SetStopMode( LPM_TCXO_WA_Id , UTIL_LPM_ENABLE);

            break;

        case RF_API_STATE_TCXO_ON:
            LL_RCC_HSE_EnableTcxo();

            RCC_HSE_EnableBypass();

            LL_RCC_HSE_Enable();

            //UTIL_LPM_SetStopMode( LPM_TCXO_WA_Id , UTIL_LPM_DISABLE);

            while(LL_RCC_HSE_IsReady() == 0)
            {
            }

            break;

        default:
            break;
        }
    }
}

static void RadioReset_Workaround( void )
{
    if( LL_DBGMCU_GetRevisionID() == 0x1000 )
    {
        uint8_t reg=0;

        SUBGRF_ExecSetCmd( RADIO_SET_SLEEP, &reg, 1 );

        HAL_Delay( 2 );
    }
}
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
