/*******************************************************************************
* smac_user_protocol_fsk_r header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* usercribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#ifndef __SMAC_USER_PROTOCOL_FSK_R_H__
#define __SMAC_USER_PROTOCOL_FSK_R_H__

#include "usr_common.h"

#if defined(BLD_USE_FSK_R)
    #include "stack/mac/protocol/smac_user_protocol.h"

/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/
    #define SmacUserFrameOverhead_FSK_R                 ( sizeof(smacUserHeader_FSK_R_t) )                              // fsk:14
    #define SmacUserFrameTotalSize_FSK_R(payloadSize)   ( (payloadSize) + SmacUserFrameOverhead_FSK_R )
    #define SmacUserPayloadSizeMax_FSK_R                ( SmacUserFrameTotalSizeMax_FSK - SmacUserFrameOverhead_FSK_R ) // fsk:227

    #define MIN_HOP_COUNT_FSK   1
    #define MAX_HOP_COUNT_FSK   4

/*******************************************************************************
********************************************************************************
* Public type definitions
********************************************************************************
*******************************************************************************/

    // Header
    typedef struct smacUserHeader_FSK_R_tag
    {
        uint8_t     data_type;          /* data kind         */
        uint8_t     hop_cnt;            /* hop count         */
        octet2_t    route1;             /* route address 1   */
        octet2_t    route2;             /* route address 2   */
        octet2_t    route3;             /* route address 3   */
        octet2_t    end_addr;           /* end address       */
        uint8_t     reserved;           /* reserved          */
        uint8_t     length;             /* length            */
        octet2_t    ori_addr;           /* original address  */
    } smacUserHeader_FSK_R_t;

    // Frame
    typedef struct smacUserFrame_FSK_R_tag smacUserFrame_FSK_R_t;

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
    void                    SmacUserFrame_makeFrame_FSK_R( smacUserFrame_FSK_R_t* frame, const smacRouteParams_t* routeInfo, uint8_t payloadSize );
    void                    SmacUserFrame_forwardFrame_FSK_R( smacUserFrame_FSK_R_t* frame, uint16_t* destNodeAddr );
    smacUserReceiveType_t   SmacUserFrame_analyzeFrame_FSK_R( const smacUserFrame_FSK_R_t* frame, uint8_t recvSize, uint8_t* payloadSize, smacRouteParams_t* routeInfo );
#endif

#endif //__SMAC_USER_PROTOCOL_FSK_R_H__
