/*******************************************************************************
* smac_user_protocol_lora_r header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* usercribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#ifndef __SMAC_USER_PROTOCOL_LORA_R_H__
#define __SMAC_USER_PROTOCOL_LORA_R_H__

#include "usr_common.h"

#if defined(BLD_USE_LORA_R)
    #include "stack/mac/protocol/smac_user_protocol.h"

/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/
    #define SmacUserFrameOverhead_LORA_R                ( sizeof(smacUserHeader_LORA_R_t) )  // lora(staticRouting):10
    #define SmacUserFrameTotalSize_LORA_R(payloadSize)  ( (payloadSize) + SmacUserFrameOverhead_LORA_R )
    #define SmacUserPayloadSizeMax_LORA_R               ( SmacUserFrameTotalSizeMax_LORA - SmacUserFrameOverhead_LORA_R )  // lora:243, lora(staticRouting):233

    #define MIN_HOP_COUNT_LORA  1
    #define MAX_HOP_COUNT_LORA  3

/*******************************************************************************
********************************************************************************
* Public type definitions
********************************************************************************
*******************************************************************************/

    // Header
    typedef struct smacUserHeader_LORA_R_tag
    {
        uint8_t     hop_cnt;            /* hop count         */
        uint8_t     reserved;           /* reserved          */
        octet2_t    route1;             /* route address 1   */
        octet2_t    route2;             /* route address 2   */
        octet2_t    end_addr;           /* end address       */
        octet2_t    ori_addr;           /* original address  */
    } smacUserHeader_LORA_R_t;

    // Frame
    typedef struct smacUserFrame_LORA_R_tag smacUserFrame_LORA_R_t;

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

    void                    SmacUserFrame_makeFrame_LORA_R( smacUserFrame_LORA_R_t* frame, const smacRouteParams_t* routeInfo, uint8_t payloadSize );
    void                    SmacUserFrame_forwardFrame_LORA_R( smacUserFrame_LORA_R_t* frame, uint16_t* destNodeAddr );
    smacUserReceiveType_t   SmacUserFrame_analyzeFrame_LORA_R( const smacUserFrame_LORA_R_t* frame, uint8_t recvSize, uint8_t* payloadSize, smacRouteParams_t* routeInfo );
#endif

#endif //__SMAC_USER_PROTOCOL_LORA_R_H__
