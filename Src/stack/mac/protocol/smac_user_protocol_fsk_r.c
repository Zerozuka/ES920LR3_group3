/*******************************************************************************
* smac_user_protocol file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* usercribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#include "usr_common.h"

#if defined(BLD_USE_FSK_R)
#include "stack/mac/protocol/smac_user_protocol.h"

/*******************************************************************************
********************************************************************************
* Private macros
********************************************************************************
*******************************************************************************/
#define DataType_FSK_MSG_TYPE_MASK          0x0F
#define DataType_FSK_MSG_TYPE_DATA          0x01
#define DataType_FSK_ADD_FLAG_MASK          0x80
#define DataType_FSK_ADD_FLAG_IS_ROUTING    0x80

#define SmacFrame_calcSduSize_FSK_R(recvSize)     ( (recvSize) - SmacFrameOverhead_FSK_R )


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

void SmacUserFrame_makeFrame_FSK_R( smacUserFrame_FSK_R_t* frame, const smacRouteParams_t* routeInfo, uint8_t payloadSize )
{
    smacUserHeader_FSK_R_t* header = &frame->header;

    // Fill header
    memset( header, 0, sizeof(*header) );
    header->data_type = DataType_FSK_MSG_TYPE_DATA;
    header->hop_cnt   = routeInfo->hop_cnt;
    octet2_set( &header->route1, routeInfo->route[0] );
    octet2_set( &header->route2, routeInfo->route[1] );
    octet2_set( &header->route3, routeInfo->route[2] );
    octet2_set( &header->end_addr, routeInfo->endNodeAddr );
    header->length    = payloadSize;
    octet2_set( &header->ori_addr, routeInfo->oriNodeAddr );
}

#if defined(BLD_USE_FSK_R)
void SmacUserFrame_forwardFrame_FSK_R( smacUserFrame_FSK_R_t* frame, uint16_t* destNodeAddr )
{
    smacUserHeader_FSK_R_t* header = &frame->header;

    switch( header->hop_cnt )
    {
    case 2:
        octet2_set( &header->route1, 0 );
        octet2_set( &header->route2, 0 );
        octet2_set( &header->route3, 0 );
        *destNodeAddr = octet2_get( &header->end_addr );
        break;

    case 3:
        header->route1 = header->route2;
        octet2_set( &header->route2, 0 );
        octet2_set( &header->route3, 0 );
        *destNodeAddr  = octet2_get( &header->route1 );
        break;

    case 4:
        header->route1 = header->route2;
        header->route2 = header->route3;
        octet2_set( &header->route3, 0 );
        *destNodeAddr  = octet2_get( &header->route1 );
        break;
    }

    header->hop_cnt--;
}
#endif

smacUserReceiveType_t SmacUserFrame_analyzeFrame_FSK_R( const smacUserFrame_FSK_R_t* frame, uint8_t recvSize, uint8_t* payloadSize, smacRouteParams_t* routeInfo )
{
    const smacUserHeader_FSK_R_t* header;

    /* check receive size (header) */
    if( recvSize < sizeof(smacUserHeader_FSK_R_t) )
    {
        return smacUserReceiveType_DISCARD;
    }

    /* get header */
    header = &frame->header;

    /* check length */
    *payloadSize = header->length;
    if( recvSize < *payloadSize + SmacUserFrameOverhead_FSK_R )
    {
        return smacUserReceiveType_DISCARD;
    }

    /* check data type */
    switch( header->data_type )
    {
    case DataType_FSK_MSG_TYPE_DATA:
        break;
    default:
        return smacUserReceiveType_DISCARD;
    }

    /* check hop count */
    if( header->hop_cnt < MIN_HOP_COUNT_FSK || MAX_HOP_COUNT_FSK < header->hop_cnt )
    {
        return smacUserReceiveType_DISCARD;
    }

    routeInfo->hop_cnt     = header->hop_cnt;
    routeInfo->route[0]    = octet2_get( &header->route1 );
    routeInfo->route[1]    = octet2_get( &header->route2 );
    routeInfo->route[2]    = octet2_get( &header->route3 );
    routeInfo->endNodeAddr = octet2_get( &header->end_addr );
    routeInfo->oriNodeAddr = octet2_get( &header->ori_addr );

    if( header->hop_cnt > 1 )
    {
        return smacUserReceiveType_FORWARD;
    }
    else
    {
        return smacUserReceiveType_INPUT;
    }
}

#endif
