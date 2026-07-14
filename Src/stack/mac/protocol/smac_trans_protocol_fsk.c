/*******************************************************************************
* smac_trans_protocol_fsk file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#include "usr_common.h"

#if defined(BLD_USE_FSK_R)

#include "stack/mac/protocol/smac_trans_protocol_fsk.h"

/*******************************************************************************
********************************************************************************
* Private macros
********************************************************************************
*******************************************************************************/

#define smacAddr_get(pAddr, pPanId, pDestId, pSrcId) { *(pPanId)   = octet2_get(&(pAddr)->panId);      \
                                                       *(pDestId)  = octet2_get(&(pAddr)->destAddr);   \
                                                       *(pSrcId)   = octet2_get(&(pAddr)->srcAddr);    }
#define smacAddr_set(pAddr, panId, destId, srcId)    { octet2_set( &(pAddr)->panId,    (panId)     );  \
                                                       octet2_set( &(pAddr)->destAddr, (destAddr)  );  \
                                                       octet2_set( &(pAddr)->srcAddr,  (srcAddr)   );  }

#define FrameControl_FSK_Mask   (   smacFrameControl_FSK_FRAME_TYPE_MASK        \
                                |   smacFrameControl_FSK_INTRA_PAN_FLAG         \
                                |   smacFrameControl_FSK_DEST_ADDR_MODE_MASK    \
                                |   smacFrameControl_FSK_FRAME_VERSION_MASK     \
                                |   smacFrameControl_FSK_SRC_ADDR_MODE_MASK     \
                                )
#define FrameControl_FSK_Data   (   smacFrameControl_FSK_FRAME_TYPE_DATA        \
                                |   smacFrameControl_FSK_INTRA_PAN_FLAG         \
                                |   smacFrameControl_FSK_DEST_ADDR_MODE_2BYTES  \
                                |   smacFrameControl_FSK_FRAME_VERSION_2003     \
                                |   smacFrameControl_FSK_SRC_ADDR_MODE_2BYTES   \
                                )
#define FrameControl_FSK_Ack    (   smacFrameControl_FSK_FRAME_TYPE_ACK         \
                                |   smacFrameControl_FSK_DEST_ADDR_MODE_0       \
                                |   smacFrameControl_FSK_SRC_ADDR_MODE_0        \
                                )

#define SmacFrame_calcSduSize_FSK(recvSize)    ( (recvSize) - SmacFrameOverhead_FSK )


/*******************************************************************************
********************************************************************************
* Private function declarations
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Private memory declarations
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Public functions
********************************************************************************
*******************************************************************************/

void SmacFrame_makeDataFrame_FSK( smacFrame_FSK_t* frame, const SmacTransAddrInfo_t* addr, bool_t ackReq, uint8_t seqNo, uint8_t sduSize, const aes_context* aesContext )
{
    smacHeader_FSK_t* header = &frame->header;
    uint16_t frameControl    = FrameControl_FSK_Data;

    if( IS_ACK_REQ(ackReq, addr) )
    {
        frameControl |= smacFrameControl_FSK_ACK_REQ_FLAG;
    }

    // Fill header
    memset( header, 0, sizeof(*header) );
    octet2_set( &header->frameControl, frameControl );
    header->seqNo = seqNo;
    octet2_set( &header->addr.panId,    addr->srcNode.panId );
    octet2_set( &header->addr.destAddr, addr->destNodeAddr );
    octet2_set( &header->addr.srcAddr, addr->srcNode.nodeAddr );

    // Encrypt SDU
    if( aesContext )
    {
        SmacPayloadEncrypt( frame->sdu.data,
                            sduSize,
                            addr->srcNode.panId,
                            addr->srcNode.nodeAddr,
                            aesContext );
    }
}

void SmacFrame_makeAckFrame_FSK( smacFrame_FSK_t* frame, smacAddrInfo_t srcNode, const smacHeader_FSK_t* recvDataHeader )
{
    smacHeader_FSK_t* header    = &frame->header;
    const uint16_t frameControl = FrameControl_FSK_Ack;

    // Fill header
    memset( header, 0, sizeof(*header) );
    octet2_set( &header->frameControl, frameControl );
    header->seqNo = recvDataHeader->seqNo;
}

smacReceiveType_t SmacFrame_analyzeFrame_FSK( smacFrame_FSK_t* frame, uint8_t recvSize, unsigned seqNoOfLastTxData, smacAddrInfo_t ownNode, uint8_t* sduSize, SmacTransAddrInfo_t* addr, bool_t* ackReq, const aes_context* aesContext )
{
    const smacHeader_FSK_t* header;

    *sduSize = 0;
    memset(addr, 0, sizeof(*addr));
    *ackReq  = FALSE;

    /* check receive size (header) */
    if( recvSize < sizeof(smacHeader_FSK_t) - sizeof(smacAddr_t) )
    {
        return smacReceiveType_DISCARD;
    }

    /* get header */
    header = &frame->header;
    const uint16_t frameControl = octet2_get(&header->frameControl);

    switch( frameControl & FrameControl_FSK_Mask )
    {
    /* ACK */
    case FrameControl_FSK_Ack:

        /* check sequence No */
        if( (seqNoOfLastTxData == INVALID_SEQNO) || (header->seqNo != seqNoOfLastTxData) )
        {
            return smacReceiveType_DISCARD;
        }

        return smacReceiveType_ACK_TO_OWN;

    /* DATA */
    case FrameControl_FSK_Data:
    default:
        {
            /* check receive size (header) */
            if( recvSize < sizeof(smacHeader_FSK_t) )
            {
                return smacReceiveType_DISCARD;
            }

            /* check address */
            if( !SmacAddr_analyze(&header->addr, ownNode, addr) )
            {
                return smacReceiveType_DISCARD;
            }

            /* required acknowledge */
            if( frameControl & smacFrameControl_FSK_ACK_REQ_FLAG )
            {
                *ackReq = TRUE;
            }

            /* calc length */
            *sduSize = SmacFrame_calcSduSize_FSK(recvSize);

            // Decrypt SDU
            if( aesContext )
            {
                SmacPayloadDecrypt( frame->sdu.data,
                                    *sduSize,
                                    addr->srcNode.panId,
                                    addr->srcNode.nodeAddr,
                                    aesContext );
            }

            return smacReceiveType_DATA_TO_OWN;
        }
        break;
    }
}

#endif
