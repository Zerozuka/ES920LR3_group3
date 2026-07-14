/*******************************************************************************
* smac_stack header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#ifndef __SMAC_STACK_H__
#define __SMAC_STACK_H__

#include "radio/radio.h"
#include "stack/phy/protocol/phy_protocol.h"
#include "stack/mac/protocol/smac_trans_protocol.h"
#include "stack/mac/protocol/smac_user_protocol.h"

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

// Error code
typedef enum smacErrors_tag
{
    gErrorNoError_c = 0,
    gErrorBusy_c,
    gErrorChannelBusy_c,
    gErrorNoAck_c,
    gErrorOutOfRange_c,
    gErrorNoResourcesAvailable_c,
    gErrorNoValidCondition_c,
    gErrorCorrupted_c,
    gErrorTimeout_c,
    gErrorTxDurationOver_c,
    gErrorNotInitialized_c,
    gErrorMaxError_c,
} smacErrors_t;

// Timer type
typedef enum stackTimerType_tag
{
    PhyCcaTimer,
    PhyTxTimer,
    PhyDutyTimer,
    SmacCcaBackoffTimer,
    SmacTxTimer,
    PhyRxTimer,
} stackTimerType_t;

// Fiber type
typedef enum stackFiberType_tag
{
    PhyCcaFiber,
} stackFiberType_t;

// ProtocolType
typedef enum protocoType_tag
{
#if defined(BLD_USE_LORA_NR)
    Protocol_LORA_NR    = 1,
#endif
#if defined(BLD_USE_LORA_R)
    Protocol_LORA_R     = 2,
#endif
#if defined(BLD_USE_FSK_R)
    Protocol_FSK_R      = 3,
#endif
} protocolType_t;

#if defined(BLD_USE_LORA_NR)
    #define __case_Protocol_LORA_NR     case Protocol_LORA_NR:
#else
    #define __case_Protocol_LORA_NR
#endif

#if defined(BLD_USE_LORA_R)
    #define __case_Protocol_LORA_R      case Protocol_LORA_R:
#else
    #define __case_Protocol_LORA_R
#endif

#if defined(BLD_USE_FSK_R)
    #define __case_Protocol_FSK_R       case Protocol_FSK_R:
#else
    #define __case_Protocol_FSK_R
#endif

#define __cases_Protocol_NR     __case_Protocol_LORA_NR

#define __cases_Protocol_R      __case_Protocol_LORA_R  \
                                __case_Protocol_FSK_R
#define __cases_Protocol_LORA   __case_Protocol_LORA_NR \
                                __case_Protocol_LORA_R
#define __cases_Protocol_FSK    __case_Protocol_FSK_R


//------------------------------------------------------------------------------
// Protocol Stack
//------------------------------------------------------------------------------

// (Application)   [1]LoRa w/o StaticRouting   [2]LoRa w/ StaticRouting   [3]FSK w/ StaticRouting
//               +---------------------------+--------------------------+-------------------------+
//  1. SmacUser  | 1-1.SmacUserFrame_LORA_NR | 1-2.SmacUserFrame_LORA_R | 1-3.SmacUserFrame_FSK_R | (Routing)
//               +---------------------------+--------------------------+-------------------------+
//  2. SmacTrans |                2-1. SmacTransFrame_LORA              | 2-2. SmacTransFrame_FSK | (ACK)
//               +------------------------------------------------------+-------------------------+
//  3. Phy       |                   3-1. PhyFrame_LORA                 |    3-2. PhyFrame_FSK    | (CCA, TxDuty)
//               +------------------------------------------------------+-------------------------+
//  4. Radio     |                 4. ( Frame format is specified in the RF chip )                | (Communication)
//               +--------------------------------------------------------------------------------+
//   (RF chip)

#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)

    //--------------------------------------------------------------
    // 1-1. Smac User Frame of Private LoRa without Static Routing
    //--------------------------------------------------------------
  #if defined(BLD_USE_LORA_NR)
    // SDU
    typedef struct smacUserSdu_LORA_NR_tag
    {
        uint8_t                 payload[SmacUserPayloadSizeMax_LORA_NR];    // variable
    } smacUserSdu_LORA_NR_t;

    // PDU
    struct smacUserFrame_LORA_NR_tag
    {
        smacUserSdu_LORA_NR_t   sdu;    // SDU
    } ;
  #endif

    //--------------------------------------------------------------
    // 1-2. Smac User Frame of Private LoRa with Static Routing
    //--------------------------------------------------------------
  #if defined(BLD_USE_LORA_R)
    // SDU
    typedef struct smacUserSdu_LORA_R_tag
    {
        uint8_t                 payload[SmacUserPayloadSizeMax_LORA_R];     // variable
    } smacUserSdu_LORA_R_t;

    // PDU
    struct smacUserFrame_LORA_R_tag
    {
        smacUserHeader_LORA_R_t header; // Header
        smacUserSdu_LORA_R_t    sdu;    // SDU
    } ;
  #endif

    //--------------------------------------------------------------
    // 2-1. Smac Trans Frame of Private LoRa
    //--------------------------------------------------------------

    // SDU
    typedef union smacSdu_LORA_tag
    {
        uint8_t                 data[SmacSduSizeMax_LORA];  // variable
  #if defined(BLD_USE_LORA_NR)
        smacUserFrame_LORA_NR_t user_NR;
  #endif
  #if defined(BLD_USE_LORA_R)
        smacUserFrame_LORA_R_t  user_R;
  #endif
    } smacSdu_LORA_t;

    // PDU
    struct smacFrame_LORA_tag
    {
        smacHeader_LORA_t       header;     // Header
        smacSdu_LORA_t          sdu;        // SDU
    } ;

    //--------------------------------------------------------------
    // 3-1. Phy Frame of Private LoRa
    //--------------------------------------------------------------

    // SDU
    typedef union phySdu_LORA_tag
    {
        uint8_t                 data[PhySduSizeMax_LORA];   // variable
        smacFrame_LORA_t        smac;
    } phySdu_LORA_t;

    // PDU
    struct phyFrame_LORA_tag
    {
        phySdu_LORA_t           sdu;        // SDU
    } ;
#endif

#if defined(BLD_USE_FSK_R)

    //--------------------------------------------------------------
    // 1-3. Smac User Frame of FSK with Static Routing
    //--------------------------------------------------------------

    // SDU
    typedef struct smacUserSdu_FSK_R_tag
    {
        uint8_t                 payload[SmacUserPayloadSizeMax_FSK_R];  // variable
    } smacUserSdu_FSK_R_t;

    // PDU
    struct smacUserFrame_FSK_R_tag
    {
        smacUserHeader_FSK_R_t  header; // Header
        smacUserSdu_FSK_R_t     sdu;    // SDU
    } ;

    //--------------------------------------------------------------
    // 2-2. Smac Trans Frame of FSK
    //--------------------------------------------------------------

    // SDU
    typedef union smacSdu_FSK_tag
    {
        uint8_t                 data[SmacSduSizeMax_FSK];   // variable
        smacUserFrame_FSK_R_t   user_R;
    } smacSdu_FSK_t;

    // PDU
    struct smacFrame_FSK_tag
    {
        smacHeader_FSK_t        header;         // Header
        smacSdu_FSK_t           sdu;            // SDU
    } ;

    //--------------------------------------------------------------
    // 3-2. Phy Frame of FSK
    //--------------------------------------------------------------

    // SDU
    typedef union phySdu_FSK_tag
    {
        uint8_t                 data[PhySduSizeMax_FSK];    // variable
        smacFrame_FSK_t         smac;
    } phySdu_FSK_t;

    // PDU
    struct phyFrame_FSK_tag
    {
        phyHeader_FSK_t         header;         // Header
        phySdu_FSK_t            sdu;            // SDU
    } ;

#endif

    //--------------------------------------------------------------
    // 4. Radio Frame of FSK
    //--------------------------------------------------------------

    // SDU
    typedef union radioSdu_tag
    {
        uint8_t                 data[RadioPayloadSizeMax];  // variable
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
        phyFrame_LORA_t         phy_LORA;
#endif
#if defined(BLD_USE_FSK_R)
        phyFrame_FSK_t          phy_FSK;
#endif
    } radioSdu_t;

    // Radio frame format is specified in the RF chip

//--------------------------------------------------------
// PacketBuffer
//--------------------------------------------------------
typedef radioSdu_t  packet_t;

__STATIC_INLINE uint8_t* getPacketPayload( packet_t* packet, protocolType_t protocol )
{
    switch( protocol )
    {
#if defined(BLD_USE_LORA_NR)
    case Protocol_LORA_NR:
        return packet->phy_LORA.sdu.smac.sdu.user_NR.sdu.payload;
#endif
#if defined(BLD_USE_LORA_R)
    case Protocol_LORA_R:
        return packet->phy_LORA.sdu.smac.sdu.user_R.sdu.payload;
#endif
#if defined(BLD_USE_FSK_R)
    case Protocol_FSK_R:
        return packet->phy_FSK.sdu.smac.sdu.user_R.sdu.payload;
#endif
    }
    return NULL;
}

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


#endif //__SMAC_STACK_H__
