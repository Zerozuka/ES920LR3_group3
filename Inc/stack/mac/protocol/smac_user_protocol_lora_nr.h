/*******************************************************************************
* smac_user_protocol_lora_nr header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* usercribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#ifndef __SMAC_USER_PROTOCOL_LORA_NR_H__
#define __SMAC_USER_PROTOCOL_LORA_NR_H__

#include "usr_common.h"

#if defined(BLD_USE_LORA_NR)
    #include "stack/mac/protocol/smac_user_protocol_lora.h"

/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/
    #define SmacUserFrameOverhead_LORA_NR               0                                                       // lora:0
    #define SmacUserFrameTotalSize_LORA_NR(payloadSize) ( (payloadSize) + SmacUserFrameOverhead_LORA_NR )
    #define SmacUserPayloadSizeMax_LORA_NR              ( SmacUserFrameTotalSizeMax_LORA - SmacUserFrameOverhead_LORA_NR ) // lora:243

/*******************************************************************************
********************************************************************************
* Public type definitions
********************************************************************************
*******************************************************************************/
    // Frame
    typedef struct smacUserFrame_LORA_NR_tag smacUserFrame_LORA_NR_t;

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

#endif

#endif //__SMAC_USER_PROTOCOL_LORA_NR_H__
