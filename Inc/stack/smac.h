/*******************************************************************************
* smac header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#ifndef __SMAC_H__
#define __SMAC_H__

#include "stack/smac_stack.h"
#include "stack/mac/smac_user.h"

/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/

#define MIN_HOP_CNT             1

#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
    #define MAX_PAYLOAD_LENGTH_LORA     50
    #define MAX_HOP_CNT_LORA            3
#endif

#if defined(BLD_USE_FSK_R)
    #define MAX_PAYLOAD_LENGTH_FSK      225
    #define MAX_HOP_CNT_FSK             4
#endif

#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    #define MIN_ROUTE_ID        0x0001
    #define MAX_ROUTE_ID        0xFFFE

    #define MIN_END_ID          0x0000
    #define MAX_END_ID          0xFFFE
#endif

    #define MIN_RETRY_CNT       0
    #define MAX_RETRY_CNT       10

    #define MIN_SF              5
    #define MAX_SF              12

    #define MIN_PAN_ID          0x0001
    #define MAX_PAN_ID          0xFFFE

    #define MIN_OWN_ID          0x0000
    #define MAX_OWN_ID          0xFFFE

    #define MIN_DEST_ID         0x0000
    #define MAX_DEST_ID         0xFFFF

    #define MIN_POWER           -4
    #define MAX_POWER           13

    #define MIN_CH              1

#if defined(BLD_USE_FSK_R)
    #define MAX_CH_DR50         38  // unit channel 1 (920.6MHz - 928.0MHz)
    #define MAX_CH_DR150        19  // unit channel 2 (920.7MHz - 927.9MHz)
    #define MAX_CH_DR250        12  // unit channel 3 (920.8MHz - 927.4MHz)

    #define MAX_CH_FSK(dr)  ( ( (dr) <= RATE_50KBPS )   ? MAX_CH_DR50   :   \
                              ( (dr) <= RATE_150KBPS )  ? MAX_CH_DR150  :   \
                              ( (dr) <= RATE_250KBPS )  ? MAX_CH_DR250  :   0 )
#endif

#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
    #define MAX_CH_BW125        38  // unit channel 1 (920.6MHz - 928.0MHz)
    #define MAX_CH_BW250        19  // unit channel 2 (920.7MHz - 927.9MHz)
    #define MAX_CH_BW500        12  // unit channel 3 (920.8MHz - 927.4MHz)

    #define MAX_CH_LORA(bw) ( ( (bw) <= BANDWIDTH125 )  ? MAX_CH_BW125  :   \
                              ( (bw) <= BANDWIDTH250 )  ? MAX_CH_BW250  :   \
                              ( (bw) <= BANDWIDTH500 )  ? MAX_CH_BW500  : 0 )
#endif


__STATIC_INLINE uint8_t MAX_PAYLOAD_LENGTH( protocolType_t protocol )
{
    uint8_t val = 0;

    switch( protocol )
    {
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
    __cases_Protocol_LORA
        val = MAX_PAYLOAD_LENGTH_LORA;
        break;
#endif
#if defined(BLD_USE_FSK_R)
    __cases_Protocol_FSK
        val = MAX_PAYLOAD_LENGTH_FSK;
        break;
#endif
    default:
        val = 0;
        break;
    }

    return val;
}

__STATIC_INLINE uint8_t MAX_HOP_CNT( protocolType_t protocol )
{
    uint8_t val = -1;

    switch( protocol )
    {
#if defined(BLD_USE_LORA_R)
    case Protocol_LORA_R:
        val = MAX_HOP_CNT_LORA;
        break;
#endif
#if defined(BLD_USE_FSK_R)
    case Protocol_FSK_R:
        val = MAX_HOP_CNT_FSK;
        break;
#endif
    default:
        val = 0xFF;
        break;
    }

    return val;
}

/*******************************************************************************
********************************************************************************
* Public type definitions
********************************************************************************
*******************************************************************************/

typedef struct smacParams_t
{
    protocolType_t              protocol;       // protocol type
    smacRouteParams_t           route;          // hopcnt, routeId, endId, oriId
    SmacTransParams_t           trans;          // panId, srcId, dstId, ackReq, retryCount
    PhyRadioParams_t            radio;          // power, ch, dr(fsk), bw(lora), sf(lora), cr(lora)
    SmacTransEncryptOption_t    encryptOption;  // aesKey
} smacParams_t;

typedef SmacUser_CallbackInterface_t smacCallbackInterface_t;

/*******************************************************************************
********************************************************************************
* Public memory declarations
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Public Prototypes
********************************************************************************
*******************************************************************************/

// initializer
smacErrors_t SMAC_Init( const smacParams_t* params );
smacErrors_t SMAC_Start( const smacCallbackInterface_t* callback, packet_t* rxBuffer );

// accessor ( for modify setting )
smacErrors_t SMAC_GetAllParams( smacParams_t* params );
smacErrors_t SMAC_GetRouteParams( smacRouteParams_t* params );
smacErrors_t SMAC_GetTransParams( SmacTransParams_t* params );
smacErrors_t SMAC_GetRadioParams( PhyRadioParams_t* params );
smacErrors_t SMAC_GetEncryptOption( SmacTransEncryptOption_t* params );
smacErrors_t SMAC_SetAllParams( const smacParams_t* params );
smacErrors_t SMAC_SetRouteParams( const smacRouteParams_t* params );
smacErrors_t SMAC_SetTransParams( const SmacTransParams_t* params );
smacErrors_t SMAC_SetRadioParams( const PhyRadioParams_t* params );
smacErrors_t SMAC_SetEncryptOption( const SmacTransEncryptOption_t* params );
smacErrors_t SMAC_CheckParams( const smacParams_t* params );
smacErrors_t SMAC_CheckRouteParams( const smacRouteParams_t* params, protocolType_t protocol );
smacErrors_t SMAC_CheckTransParams( const SmacTransParams_t* params );
smacErrors_t SMAC_CheckRadioParams( const PhyRadioParams_t* params, protocolType_t protocol );
smacErrors_t SMAC_CheckEncryptOption( const SmacTransEncryptOption_t* params );

// operation method
smacErrors_t SMAC_RxStart( void );
smacErrors_t SMAC_TxPacket( packet_t* data, uint8_t payloadSize );
smacErrors_t SMAC_TxPacketEx( packet_t* data, uint8_t payloadSize, const SmacTransParams_t* transParams, const smacRouteParams_t* routeParams );
smacErrors_t SMAC_TxContinuous( bool_t isModulate );
smacErrors_t SMAC_Sleep( bool_t isWarmStart );
smacErrors_t SMAC_Standby( void );
smacErrors_t SMAC_GetIdleMode( SmacTransIdleMode_t* mode );
uint8_t SMAC_GetRssi( void );

// event method
uint16_t SMAC_RadioIrqProcess( void );
void SMAC_TimerExpiredProcess( stackTimerType_t type );
void SMAC_FiberProcess( stackFiberType_t type );

// utility method
uint32_t SMAC_GetRandom( uint32_t min,  uint32_t max );
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
smacErrors_t    SMAC_CalcDataRate( uint8_t bw, uint8_t sf, uint32_t* dataRate );
#endif

bool_t  SMAC_CanEnterStopMode( void );

#endif // __SMAC_H__
