/*******************************************************************************
* phy_radio header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#ifndef __PHY_RADIO_H__
#define __PHY_RADIO_H__

#include "stack/smac_stack.h"

#define CCA_RSSI        -85  // dBm
#define CCA_RETRY       3
#define TIMEOUT_MARGIN  100

typedef enum loraBandWidth_tag
{
    BANDWIDTH7_8    = 0,
    BANDWIDTH10_4,
    BANDWIDTH15_6,
    BANDWIDTH20_8,
    BANDWIDTH31_25,
    BANDWIDTH41_7,
    BANDWIDTH62_5,
    BANDWIDTH125,
    BANDWIDTH250,
    BANDWIDTH500
} loraBandWidth;

typedef enum
{
    CR_4_5          = 1,
    CR_4_6          = 2,
    CR_4_7          = 3,
    CR_4_8          = 4,
} loraCodingRate;

typedef enum fskDataRate_tag
{
    RATE_50KBPS     = 0,
    RATE_100KBPS,
    RATE_150KBPS,
    RATE_200KBPS,
    RATE_250KBPS,
} fskDataRate;

typedef struct PhyRadio_ModulationParams_tag
{
    uint8_t             ch;

#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
    struct
    {
        uint8_t             bw;
        uint8_t             sf;
        uint8_t             cr;
    } LORA;
#endif

#ifdef BLD_USE_FSK_R
    struct
    {
        uint8_t             dr;
    } FSK;
#endif

} PhyRadio_ModulationParams_t;

typedef struct PhyRadio_PaParams_tag
{
    int8_t              power;
} PhyRadio_PaParams_t;

typedef struct PhyRadioParams_tag
{
    PhyRadio_PaParams_t         pa;
    PhyRadio_ModulationParams_t modulation;
    bool_t                      rxBoost;
} PhyRadioParams_t;

typedef struct PhyRadioRxStatus_tag
{
    uint8_t             rssi;
    int8_t              snr;
} PhyRadioRxStatus_t;

typedef struct PhyRadio_RxParams_tag
{
    uint8_t*            buff;
    uint8_t             size;
    PhyRadioParams_t    radio;
} PhyRadio_RxParams_t;

typedef struct PhyRadio_TxParams_tag
{
    uint8_t*            buff;
    uint8_t             size;
    PhyRadioParams_t    radio;
    bool_t              doCca;
    uint32_t            ccaTime;
} PhyRadio_TxParams_t;

typedef struct PhyRadio_RxResultParams_tag
{
    uint8_t             recvSize;
    PhyRadioRxStatus_t  rxStatus;
} PhyRadio_RxResultParams_t;

typedef struct PhyRadio_CallbackInterface_tag
{
    void (*onSyncWordValid)(void);
    void (*onRxDone)(smacErrors_t result, const PhyRadio_RxResultParams_t* params);
    void (*onTxDone)(smacErrors_t result);
    void (*TimerControl)(stackTimerType_t type, uint32_t duration);
    void (*FiberControl)(stackFiberType_t type, bool_t enable);
} PhyRadio_CallbackInterface_t;

typedef struct PhyRadio_InitParams_tag
{
    protocolType_t                  protocol;
    PhyRadio_CallbackInterface_t    callback;
} PhyRadio_InitParams_t;

void            PhyRadio_Init( const PhyRadio_InitParams_t* params );
void            PhyRadio_Standby( void );
void            PhyRadio_Sleep( bool_t isWarmStart );
smacErrors_t    PhyRadio_Rx( const PhyRadio_RxParams_t* params );
smacErrors_t    PhyRadio_Tx( const PhyRadio_TxParams_t* params );

void            PhyRadio_CcaFiber( void );
void            PhyRadio_TimerCcaFin( void );
void            PhyRadio_TxTimeout( void );
void            PhyRadio_RxTimeout( void );
uint16_t        PhyRadio_IrqProcess( void );

void            PhyRadio_WriteRxBuffer( uint8_t* buff, size_t size );
void            PhyRadio_ReadRxBuffer( uint8_t* buff, size_t size );
#if defined(BLD_USE_FSK_R)
void            PhyRadio_ModifyRxSize( size_t size );
#endif

void            PhyRadio_TxContinuous( uint32_t freq, const PhyRadio_PaParams_t* paParams );
smacErrors_t    PhyRadio_SetTxInfinitePreamble( const PhyRadioParams_t* params );
uint32_t        PhyRadio_GetRandom( void );
smacErrors_t    PhyRadio_RfFrequency( uint32_t* freq, const PhyRadio_ModulationParams_t* modParams );
uint32_t        PhyRadio_TimeOnAir( const PhyRadio_ModulationParams_t* modParams, uint8_t dataLen );
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
uint32_t        PhyRadio_DataRate_LORA( uint8_t bw, uint8_t sf );
#endif
bool_t			PhyRadio_IsTxBusy(void);

#endif //__PHY_RADIO_H__
