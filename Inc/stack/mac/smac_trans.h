/*******************************************************************************
* smac_trans header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#ifndef __SMAC_TRANS_H__
#define __SMAC_TRANS_H__

#include "stack/smac_stack.h"
#include "stack/phy/phy.h"

/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/

#define RETRANS_BACKOFF_MIN     1
#define RETRANS_BACKOFF_MAX     32
#define SMAC_ENCRYPT_KEY_LENGTH 16

/*******************************************************************************
********************************************************************************
* Public type definitions
********************************************************************************
*******************************************************************************/

typedef enum SmacTransIdleMode_tag
{
    SmacTransIdleMode_STANDBY,
    SmacTransIdleMode_SLEEP,
    SmacTransIdleMode_RX,
} SmacTransIdleMode_t;

typedef struct SmacTransOption_tag
{
    bool_t                      ackReq;
    uint8_t                     ackRetryCount;
    uint8_t                     ccaRetryCount;
} SmacTransOption_t;

typedef struct SmacTransParams_tag
{
    SmacTransAddrInfo_t         addr;
    SmacTransOption_t           option;
} SmacTransParams_t;

typedef struct SmacTrans_RxResultParams_tag
{
    uint8_t                     sduSize;
    SmacTransAddrInfo_t         addr;
    bool_t                      ackReq;
    Phy_RxResultParams_t        phy;
} SmacTrans_RxResultParams_t;

typedef struct SmacTrans_RxRequestParams_tag
{
    smacAddrInfo_t              ownNode;
    PhyRadioParams_t            radio;
} SmacTrans_RxRequestParams_t;

typedef struct SmacTrans_TxRequestParams_tag
{
    packet_t*                   buff;
    uint8_t                     sduSize;
    SmacTransParams_t           trans;
    PhyRadioParams_t            radio;
} SmacTrans_TxRequestParams_t;

typedef struct SmacTrans_CallbackInterface_tag
{
    void (*onNotifyRxData)(smacErrors_t result, const SmacTrans_RxResultParams_t* params);
    void (*onTxDataComp)(smacErrors_t result);
    void (*TimerControl)(stackTimerType_t type, uint32_t duration);
    void (*FiberControl)(stackFiberType_t type, bool_t enable);
} SmacTrans_CallbackInterface_t;

typedef struct SmacTransEncryptOption_tag
{
    bool_t                      enable;
    const uint8_t*              aesKey; // 16bytes
} SmacTransEncryptOption_t;

typedef struct SmacTrans_InitParams_tag
{
    protocolType_t                  protocol;
    SmacTrans_CallbackInterface_t   callback;
    packet_t*                       rxBuffer;
    SmacTransEncryptOption_t        encryptOption;
} SmacTrans_InitParams_t;

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

void            SmacTrans_Init( const SmacTrans_InitParams_t* params );
smacErrors_t    SmacTrans_StartRxData( const SmacTrans_RxRequestParams_t* params );
smacErrors_t    SmacTrans_ReqTxData( const SmacTrans_TxRequestParams_t* params );
smacErrors_t    SmacTrans_Standby( void );
smacErrors_t    SmacTrans_Sleep( bool_t isWarmStart );
smacErrors_t    SmacTrans_GetIdleMode( SmacTransIdleMode_t* mode );
void            SmacTrans_CcaBackoffTimeout( void );
void            SmacTrans_TxDataTimeout( void );

#define         SmacTrans_TimeOnAir(modParams, sduSize) ( Phy_TimeOnAir( modParams, SmacTransFrameTotalSizeMax(sduSize) ) )

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif //__SMAC_TRANS_H__
