/*******************************************************************************
* smac_trans_protocol file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#include "usr_common.h"

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

bool_t SmacAddr_analyze( const smacAddr_t* addr, smacAddrInfo_t ownNode, SmacTransAddrInfo_t* addrInfo )
{
    const uint16_t panId  = octet2_get(&addr->panId);
    const uint16_t destId = octet2_get(&addr->destAddr);
    const uint16_t srcId  = octet2_get(&addr->srcAddr);

    addrInfo->srcNode.panId     = panId;
    addrInfo->srcNode.nodeAddr  = srcId;
    addrInfo->destNodeAddr      = destId;

    /* check PAN ID */
    if( panId != ownNode.panId )
    {
        return FALSE;
    }

    /* check DEST ID */
    if( destId == BROADCAST_ADDR )
    {
        // broadcast
    }
    else if( destId == ownNode.nodeAddr )
    {
        // unicast to own node
    }
    else
    {
        // unicast to other node
        return FALSE;
    }

    return TRUE;
}

/*******************************************************************************
*
* SmacPayloadEncrypt
*
* Interface assumptions:
*     buffer    data
*     size      data size
*     panid     PAN ID
*     srcid     OWN ID
*     encBuffer encode data
*
* Return value:
*     None
*
*******************************************************************************/
void SmacPayloadEncrypt( uint8_t* buffer, uint16_t size, uint16_t panid, uint16_t srcid, const aes_context* aesContext )
{
    uint16_t    i;
    uint16_t    ctr = 1;

    uint8_t aBlock[N_BLOCK];
    uint8_t sBlock[N_BLOCK];

    memset( aBlock, 0, sizeof(aBlock) );

    aBlock[0]  = 0x01;
    aBlock[8]  = ( panid >> 0 ) & 0xFF;
    aBlock[9]  = ( panid >> 8 ) & 0xFF;
    aBlock[12] = ( srcid >> 0 ) & 0xFF;
    aBlock[13] = ( srcid >> 8 ) & 0xFF;

    while( size >= N_BLOCK )
    {
        aBlock[15] = ( ( ctr++ ) & 0xFF );

        aes_encrypt( aBlock, sBlock, aesContext );
        for( i = 0; i < N_BLOCK; i++ )
        {
            buffer[i] ^= sBlock[i];
        }

        size -= N_BLOCK;
        buffer += N_BLOCK;
    }

    if( size > 0 )
    {
        aBlock[15] = ( ( ctr ) & 0xFF );
        aes_encrypt( aBlock, sBlock, aesContext );
        for( i = 0; i < size; i++ )
        {
            buffer[i] ^= sBlock[i];
        }
    }
}
