/*******************************************************************************
* phy header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#ifndef __PHY_H__
#define __PHY_H__

#include "stack/smac_stack.h"
#include "stack/phy/phy_radio.h"

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

typedef struct Phy_RxRequestParams_tag
{
    packet_t*           buff;
    PhyRadioParams_t    radio;
} Phy_RxRequestParams_t;

typedef struct Phy_TxRequestParams_tag
{
    packet_t*           buff;
    uint8_t             sduSize;
    bool_t              isRecycleBuff;
    PhyRadioParams_t    radio;
    bool_t              doCca;
    uint32_t            ccaTime;
    bool_t              waitDutyElapsed;
    uint32_t            dutyTime;
} Phy_TxRequestParams_t;

typedef struct Phy_RxResultParams_tag
{
    uint8_t                     sduSize;
    PhyRadio_RxResultParams_t   radio;
} Phy_RxResultParams_t;

typedef struct Phy_CallbackInterface_tag
{
    void (*onRxComp)(smacErrors_t result, const Phy_RxResultParams_t* params);
    void (*onTxComp)(smacErrors_t result);
    void (*TimerControl)(stackTimerType_t type, uint32_t duration);
    void (*FiberControl)(stackFiberType_t type, bool_t enable);
} Phy_CallbackInterface_t;

typedef struct Phy_InitParams_tag
{
    protocolType_t          protocol;
    Phy_CallbackInterface_t callback;
} Phy_InitParams_t;

/*******************************************************************************
********************************************************************************
* Public memory declarations
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Public Prototypes
*****************************************v**************************************
*******************************************************************************/

void Phy_Init( const Phy_InitParams_t* params );
smacErrors_t Phy_Standby( void );
smacErrors_t Phy_Sleep( bool_t isWarmStart );
smacErrors_t Phy_ReqRx( const Phy_RxRequestParams_t* params );
smacErrors_t Phy_ReqTx( const Phy_TxRequestParams_t* params );
uint32_t     Phy_GetTxDuty( void );
void Phy_TimerDutyFin( void );

#define Phy_TimeOnAir_LORA(modParams, sduSize)  ( PhyRadio_TimeOnAir( modParams, PhyFrameTotalSize_LORA(sduSize) ) )
#define Phy_TimeOnAir_FSK(modParams, sduSize)   ( PhyRadio_TimeOnAir( modParams, PhyFrameTotalSize_FSK(sduSize) ) )

#endif //__PHY_H__
