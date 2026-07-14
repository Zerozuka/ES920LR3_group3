/*******************************************************************************
* smac_user_protocol_r file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* usercribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#include "usr_common.h"

#if defined(BLD_USE_LORA_R)
#include "stack/mac/protocol/smac_user_protocol_lora_r.h"

/*******************************************************************************
********************************************************************************
* Private macros
********************************************************************************
*******************************************************************************/
#define SmacFrame_calcSduSize_LORA_R(totalLength)  ( (totalLength) - SmacFrameOverhead_LORA_R )

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

void SmacUserFrame_makeFrame_LORA_R( smacUserFrame_LORA_R_t* frame, const smacRouteParams_t* routeInfo, uint8_t payloadSize )
{
    smacUserHeader_LORA_R_t* header = &frame->header;

    // Fill header
    memset( header, 0, sizeof(*header) );
    header->hop_cnt = routeInfo->hop_cnt;
    octet2_set( &header->route1, routeInfo->route[0] );
    octet2_set( &header->route2, routeInfo->route[1] );
    octet2_set( &header->end_addr, routeInfo->endNodeAddr );
    octet2_set( &header->ori_addr, routeInfo->oriNodeAddr );
}

void SmacUserFrame_forwardFrame_LORA_R( smacUserFrame_LORA_R_t* frame, uint16_t* destNodeAddr )
{
    smacUserHeader_LORA_R_t* header = &frame->header;

    switch( header->hop_cnt )
    {
    case 2:
        octet2_set( &header->route1, 0 );
        octet2_set( &header->route2, 0 );
        *destNodeAddr = octet2_get( &header->end_addr );
        break;

    case 3:
        header->route1 = header->route2;
        octet2_set( &header->route2, 0 );
        *destNodeAddr = octet2_get( &header->route1 );
        break;
    }

    header->hop_cnt--;
}

smacUserReceiveType_t SmacUserFrame_analyzeFrame_LORA_R( const smacUserFrame_LORA_R_t* frame, uint8_t recvSize, uint8_t* payloadSize, smacRouteParams_t* routeInfo )
{
    const smacUserHeader_LORA_R_t* header;

    /* check receive size (header) */
    if( recvSize < sizeof(smacUserHeader_LORA_R_t) )
    {
        return smacUserReceiveType_DISCARD;
    }

    /* get header */
    header = &frame->header;

    /* check hop count */
    if( header->hop_cnt < MIN_HOP_COUNT_LORA || MAX_HOP_COUNT_LORA < header->hop_cnt )
    {
        return smacUserReceiveType_DISCARD;
    }

    *payloadSize           = recvSize - SmacUserFrameOverhead_LORA_R;
    routeInfo->hop_cnt     = header->hop_cnt;
    routeInfo->route[0]    = octet2_get( &header->route1 );
    routeInfo->route[1]    = octet2_get( &header->route2 );
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
