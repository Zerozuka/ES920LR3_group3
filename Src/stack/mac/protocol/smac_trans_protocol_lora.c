/*******************************************************************************
* smac_trans_protocol_lora file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#include "usr_common.h"

#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
#include "stack/mac/protocol/smac_trans_protocol.h"

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

#define SmacFrame_getFooter_LORA(frame, sduSize) ( (smacFooter_LORA_t*)( (uint8_t*)(frame->sdu.data) + (sduSize) ) )

#define FrameControl_LORA_Mask  (   smacFrameControl_LORA_FRAME_TYPE_MASK   )
#define FrameControl_LORA_Data  (   smacFrameControl_LORA_FRAME_TYPE_DATA   )
#define FrameControl_LORA_Ack   (   smacFrameControl_LORA_FRAME_TYPE_ACK    )

#define SmacFrame_calcSduSize_LORA(totalLength) ( (totalLength) - SmacFrameOverhead_LORA )

/*******************************************************************************
********************************************************************************
* Private function declarations
********************************************************************************
*******************************************************************************/
static uint16_t SmacFrame_calcCheckSum_LORA(const uint8_t* base, uint8_t length);

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

void SmacFrame_makeDataFrame_LORA( smacFrame_LORA_t* frame, const SmacTransAddrInfo_t* addr, bool_t ackReq, uint8_t seqNo, uint8_t sduSize, const aes_context* aesContext )
{
    smacHeader_LORA_t*  header   = &frame->header;
    smacFooter_LORA_t*  footer   = SmacFrame_getFooter_LORA(frame, sduSize);
    uint16_t            frameControl = FrameControl_LORA_Data;

    if( ackReq )
    {
        frameControl |= smacFrameControl_LORA_ACK_REQ_FLAG;
    }

    // Calc check sum
    const uint16_t  checkSum = SmacFrame_calcCheckSum_LORA(frame->sdu.data, sduSize);

    // Fill header
    memset( header, 0, sizeof(*header) );
    header->totalLength = SmacFrameTotalSize_LORA(sduSize);
    octet2_set( &header->frameControl, frameControl );
    header->seqNo = seqNo;
    octet2_set( &header->addr.panId,    addr->srcNode.panId );
    octet2_set( &header->addr.destAddr, addr->destNodeAddr );
    octet2_set( &header->addr.srcAddr, addr->srcNode.nodeAddr );

    // Fill footer
    memset( footer, 0, sizeof(*footer) );
    octet2_set( &footer->checkSum, checkSum );

    // Encrypt SDU
    if( aesContext )
    {
        SmacPayloadEncrypt( frame->sdu.data,
                            header->totalLength - sizeof(smacHeader_LORA_t),
                            addr->srcNode.panId,
                            addr->srcNode.nodeAddr,
                            aesContext );
    }
}

void SmacFrame_makeAckFrame_LORA( smacFrame_LORA_t* frame, smacAddrInfo_t srcNode, const smacHeader_LORA_t* recvDataHeader )
{
    smacHeader_LORA_t*  header   = &frame->header;
    const uint16_t  frameControl = FrameControl_LORA_Ack;

    const uint16_t destNodeAddr = octet2_get( &recvDataHeader->addr.srcAddr );

    // Fill header
    memset( header, 0, sizeof(*header) );
    header->totalLength = SmacFrameTotalSize_ACK_LORA;
    octet2_set( &header->frameControl, frameControl );
    header->seqNo = recvDataHeader->seqNo;
    octet2_set( &header->addr.panId, srcNode.panId );
    octet2_set( &header->addr.destAddr, destNodeAddr );
    octet2_set( &header->addr.srcAddr, srcNode.nodeAddr );
}

smacReceiveType_t SmacFrame_analyzeFrame_LORA( smacFrame_LORA_t* frame, uint8_t recvSize, unsigned seqNoOfLastTxData, smacAddrInfo_t ownNode, uint8_t* sduSize, SmacTransAddrInfo_t* addr, bool_t* ackReq, const aes_context* aesContext )
{
    const smacHeader_LORA_t*   header;
    const smacFooter_LORA_t*   footer;
    uint16_t                   checkSum;

    *sduSize = 0;
    memset(addr, 0, sizeof(*addr));
    *ackReq  = FALSE;

    /* check receive size (header) */
    if( recvSize < sizeof(smacHeader_LORA_t) )
    {
        return smacReceiveType_DISCARD;
    }

    /* get header */
    header = &frame->header;
    const uint8_t pduSize = header->totalLength;

    /* check length */
    if( pduSize < sizeof(smacHeader_LORA_t) || recvSize < pduSize )
    {
        return smacReceiveType_DISCARD;
    }

    /* check address */
    if( !SmacAddr_analyze(&header->addr, ownNode, addr) )
    {
        return smacReceiveType_DISCARD;
    }

    /* check FrameControl */
    const uint16_t frameControl = octet2_get(&header->frameControl);
    switch( frameControl & FrameControl_LORA_Mask )
    {
    /* ACK */
    case FrameControl_LORA_Ack:

        /* to other node */
        if( addr->destNodeAddr == BROADCAST_ADDR )
        {
            return smacReceiveType_DISCARD;
        }

        /* check sequence No */
        if( seqNoOfLastTxData == INVALID_SEQNO || header->seqNo != seqNoOfLastTxData )
        {
            return smacReceiveType_DISCARD;
        }

        return smacReceiveType_ACK_TO_OWN;

    /* DATA */
    case FrameControl_LORA_Data:
    default:

        /* calc length */
        *sduSize = SmacFrame_calcSduSize_LORA(header->totalLength);

        // Decrypt SDU
        if( aesContext )
        {
            SmacPayloadDecrypt( frame->sdu.data,
                                header->totalLength - sizeof(smacHeader_LORA_t),
                                addr->srcNode.panId,
                                addr->srcNode.nodeAddr,
                                aesContext );
        }

        /* get footer */
        footer = SmacFrame_getFooter_LORA( frame, *sduSize );

        // calc checkSum
        checkSum = SmacFrame_calcCheckSum_LORA( frame->sdu.data, *sduSize );

        /* check checksum */
        if( octet2_get(&footer->checkSum) != checkSum )
        {
            /* Checksum Error */
            return smacReceiveType_DISCARD;
        }

        /* required acknowledge */
        if( frameControl & smacFrameControl_LORA_ACK_REQ_FLAG )
        {
            *ackReq = TRUE;
        }
        return smacReceiveType_DATA_TO_OWN;
    }
}

/*******************************************************************************
********************************************************************************
* Private functions
********************************************************************************
*******************************************************************************/

static uint16_t SmacFrame_calcCheckSum_LORA( const uint8_t* base, uint8_t length )
{
    uint16_t checkSum = 0;

    /* calc checksum */
    for( uint8_t idx = 0; idx < length; idx++ )
    {
        checkSum += base[idx];
    }

    return checkSum;
}

#endif
