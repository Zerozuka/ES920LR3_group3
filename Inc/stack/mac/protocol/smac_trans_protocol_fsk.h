/*******************************************************************************
* smac_trans_protocol_fsk header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#ifndef __SMAC_TRANS_PROTOCOL_FSK_H__
#define __SMAC_TRANS_PROTOCOL_FSK_H__

#include "usr_common.h"

#if defined(BLD_USE_FSK_R)

/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/

    #include "stack/mac/protocol/smac_trans_protocol.h"

    #define SmacFrameOverhead_FSK           ( sizeof(smacHeader_FSK_t) )                            // fsk:10
    #define SmacFrameTotalSize_FSK(sduSize) ( (sduSize) + SmacFrameOverhead_FSK )
    #define SmacFrameTotalSizeMax_FSK       PhySduSizeMax_FSK                                       // fsk:251
    #define SmacSduSizeMax_FSK              ( SmacFrameTotalSizeMax_FSK - SmacFrameOverhead_FSK )   // fsk:241
    #define SmacFrameTotalSize_ACK_FSK      ( sizeof(smacHeader_FSK_t) - sizeof(smacAddr_t) )       // fsk:4

    // FrameControl
    #define smacFrameControl_FSK_FRAME_TYPE_MASK            0x0007
    #define smacFrameControl_FSK_FRAME_TYPE_DATA            0x0001
    #define smacFrameControl_FSK_FRAME_TYPE_ACK             0x0002
    #define smacFrameControl_FSK_SECURITY_ENABLE_FLAG       0x0008
    #define smacFrameControl_FSK_FRAME_PENDING_FLAG         0x0010
    #define smacFrameControl_FSK_ACK_REQ_FLAG               0x0020
    #define smacFrameControl_FSK_INTRA_PAN_FLAG             0x0040
    #define smacFrameControl_FSK_CCA2_FLAG                  0x0080
    #define smacFrameControl_FSK_DEST_ADDR_MODE_MASK        0x0C00
    #define smacFrameControl_FSK_DEST_ADDR_MODE_0           0x0000
    #define smacFrameControl_FSK_DEST_ADDR_MODE_2BYTES      0x0800
    #define smacFrameControl_FSK_DEST_ADDR_MODE_8BYTES      0x0C00
    #define smacFrameControl_FSK_FRAME_VERSION_MASK         0x3000
    #define smacFrameControl_FSK_FRAME_VERSION_2003         0x0000
    #define smacFrameControl_FSK_FRAME_VERSION_2006_2011    0x1000
    #define smacFrameControl_FSK_FRAME_VERSION_4GE          0x2000
    #define smacFrameControl_FSK_SRC_ADDR_MODE_MASK         0xC000
    #define smacFrameControl_FSK_SRC_ADDR_MODE_0            0x0000
    #define smacFrameControl_FSK_SRC_ADDR_MODE_2BYTES       0x8000
    #define smacFrameControl_FSK_SRC_ADDR_MODE_8BYTES       0xC000

/*******************************************************************************
********************************************************************************
* Public type definitions
********************************************************************************
*******************************************************************************/

    // Header
    typedef struct smacHeader_FSK_tag
    {
        octet2_t    frameControl;
        uint8_t     seqNo;
        smacAddr_t  addr;
    } smacHeader_FSK_t;

    // Frame
    typedef struct smacFrame_FSK_tag smacFrame_FSK_t;

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
    void                SmacFrame_makeDataFrame_FSK( smacFrame_FSK_t* frame, const SmacTransAddrInfo_t* addr, bool_t ackReq, uint8_t seqNo, uint8_t sduSize, const aes_context* aesContext );
    void                SmacFrame_makeAckFrame_FSK( smacFrame_FSK_t* frame, smacAddrInfo_t srcNode, const smacHeader_FSK_t* recvDataHeader );
    smacReceiveType_t   SmacFrame_analyzeFrame_FSK( smacFrame_FSK_t* frame, uint8_t recvSize, unsigned seqNoOfLastTxData, smacAddrInfo_t ownNode, uint8_t* sduSize, SmacTransAddrInfo_t* addr, bool_t* ackReq, const aes_context* aesContext );
#endif

#endif  //__SMAC_TRANS_PROTOCOL_FSK_H__
