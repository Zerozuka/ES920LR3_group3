/*******************************************************************************
* smac_trans_protocol_lora header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#ifndef __SMAC_TRANS_PROTOCOL_LORA_H__
#define __SMAC_TRANS_PROTOCOL_LORA_H__

#include "usr_common.h"

#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)

    #include "stack/mac/protocol/smac_trans_protocol.h"

/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/
    #define SmacFrameOverhead_LORA              ( sizeof(smacHeader_LORA_t) + sizeof(smacFooter_LORA_t) )   // lora:12
    #define SmacFrameTotalSize_LORA(sduSize)    ( (sduSize) + SmacFrameOverhead_LORA )
    #define SmacFrameTotalSizeMax_LORA          PhySduSizeMax_LORA                                          // lora:255
    #define SmacSduSizeMax_LORA                 ( SmacFrameTotalSizeMax_LORA - SmacFrameOverhead_LORA )     // lora:243
    #define SmacFrameTotalSize_ACK_LORA         ( sizeof(smacHeader_LORA_t) )                               // lora:10

    // FrameControl
    #define smacFrameControl_LORA_ACK_REQ_FLAG              0x0080
    #define smacFrameControl_LORA_FRAME_TYPE_MASK           0x0300
    #define smacFrameControl_LORA_FRAME_TYPE_DATA           0x0100
    #define smacFrameControl_LORA_FRAME_TYPE_ACK            0x0000
    #define smacFrameControl_LORA_ROUTING_FLAG              0x8000

/*******************************************************************************
********************************************************************************
* Public type definitions
********************************************************************************
*******************************************************************************/

    // Header
    typedef struct smacHeader_LORA_tag
    {
        uint8_t     totalLength;
        octet2_t    frameControl;
        uint8_t     seqNo;
        smacAddr_t  addr;
    } smacHeader_LORA_t;

    // Footer
    typedef struct smacFooter_LORA_tag
    {
        octet2_t    checkSum;
    } smacFooter_LORA_t;

    // Frame
    typedef struct smacFrame_LORA_tag smacFrame_LORA_t;

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
    void                SmacFrame_makeDataFrame_LORA( smacFrame_LORA_t* frame, const SmacTransAddrInfo_t* addr, bool_t ackReq, uint8_t seqNo, uint8_t sduSize, const aes_context* aesContext );
    void                SmacFrame_makeAckFrame_LORA( smacFrame_LORA_t* frame, smacAddrInfo_t srcNode, const smacHeader_LORA_t* recvDataHeader );
    smacReceiveType_t   SmacFrame_analyzeFrame_LORA( smacFrame_LORA_t* frame, uint8_t recvSize, unsigned seqNoOfLastTxData, smacAddrInfo_t ownNode, uint8_t* sduSize, SmacTransAddrInfo_t* addr, bool_t* ackReq, const aes_context* aesContext );

#endif

#endif //__SMAC_RADIO_CTRL_H__
