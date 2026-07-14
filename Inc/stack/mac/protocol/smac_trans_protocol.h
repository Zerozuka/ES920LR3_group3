/*******************************************************************************
* smac_trans_protocol header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#ifndef __SMAC_TRANS_PROTOCOL_H__
#define __SMAC_TRANS_PROTOCOL_H__

#include "stack/phy/protocol/phy_protocol.h"
#include "middle/AES/aes.h"

/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/

#define BROADCAST_ADDR  0xFFFF
#define INVALID_SEQNO   0xFFFFFFFF

#define IS_ACK_REQ(ackReq, addrInfo)    (   (ackReq)  \
                                        &&  (addrInfo)->srcNode.nodeAddr != BROADCAST_ADDR  \
                                        &&  (addrInfo)->destNodeAddr != BROADCAST_ADDR  \
                                        )

/*******************************************************************************
********************************************************************************
* Public type definitions
********************************************************************************
*******************************************************************************/

// Address info fields
typedef struct smacAddr_tag
{
    octet2_t    panId;
    octet2_t    destAddr;
    octet2_t    srcAddr;
} smacAddr_t;

typedef enum
{
    smacReceiveType_DISCARD,
    smacReceiveType_DATA_TO_OWN,
    smacReceiveType_ACK_TO_OWN,
} smacReceiveType_t;

typedef struct
{
    uint16_t    panId;
    uint16_t    nodeAddr;
} smacAddrInfo_t;

typedef struct SmacTransAddrInfo_tag
{
    smacAddrInfo_t  srcNode;
    uint16_t        destNodeAddr;
} SmacTransAddrInfo_t;

/*******************************************************************************
********************************************************************************
* Public Prototypes
********************************************************************************
*******************************************************************************/

bool_t  SmacAddr_analyze( const smacAddr_t* addr, smacAddrInfo_t ownNode, SmacTransAddrInfo_t* addrInfo );
void   SmacPayloadEncrypt( uint8_t* buffer, uint16_t size, uint16_t panid, uint16_t srcid, const aes_context* aesContext );
#define SmacPayloadDecrypt(...) SmacPayloadEncrypt(__VA_ARGS__)

#include "stack/mac/protocol/smac_trans_protocol_lora.h"
#include "stack/mac/protocol/smac_trans_protocol_fsk.h"

#endif  //__SMAC_TRANS_PROTOCOL_H__
