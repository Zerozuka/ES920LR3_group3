/*******************************************************************************
* smac_user header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#ifndef __SMAC_USER_H__
#define __SMAC_USER_H__

#include "stack/smac_stack.h"
#include "stack/mac/smac_trans.h"

/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Public type definitions
********************************************************************************
*******************************************************************************/

typedef struct SmacUser_RxRequestParams_tag
{
    smacAddrInfo_t          ownNode;
    PhyRadioParams_t        radio;
} SmacUser_RxRequestParams_t;

typedef struct SmacUser_TxRequestParams_tag
{
    packet_t*               buff;
    uint8_t                 payloadSize;
    smacRouteParams_t       route;
    SmacTransParams_t       trans;
    PhyRadioParams_t        radio;
} SmacUser_TxRequestParams_t;

typedef struct SmacUser_RxResultParams_tag
{
    uint8_t                     payloadSize;
    smacRouteParams_t           route;
    SmacTrans_RxResultParams_t  trans;
} SmacUser_RxResultParams_t;

typedef struct SmacUser_CallbackInterface_tag
{
    void (*onTxDataComp)(smacErrors_t result);
    void (*onNotifyRxData)(smacErrors_t result, const SmacUser_RxResultParams_t* params);
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    void (*onNotifyForward)(const SmacUser_RxResultParams_t* params, bool_t* isRouting, SmacTransOption_t* transOption);
    void (*onNotifyForwardResult)(smacErrors_t result);
#endif
    void (*TimerControl)(stackTimerType_t type, uint32_t duration);
    void (*FiberControl)(stackFiberType_t type, bool_t enable);
} SmacUser_CallbackInterface_t;

typedef struct SmacUser_InitParams_tag
{
    protocolType_t                  protocol;
    SmacUser_CallbackInterface_t    callback;
    packet_t*                       rxBuffer;
    SmacTransEncryptOption_t        encryptOption;
} SmacUser_InitParams_t;

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

void            SmacUser_Init( const SmacUser_InitParams_t* params );
smacErrors_t    SmacUser_StartRxData( const SmacUser_RxRequestParams_t* params );
smacErrors_t    SmacUser_ReqTxData( const SmacUser_TxRequestParams_t* params );
smacErrors_t    SmacUser_Sleep( bool_t isWarmStart );
smacErrors_t    SmacUser_Standby( void);
#define         SmacUser_TimeOnAir( modParams, payloadSize )  ( SmacTrans_TimeOnAir( modParams, SmacUserFrameTotalSizeMax(payloadSize) ) )

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif //__SMAC_USER_H__
