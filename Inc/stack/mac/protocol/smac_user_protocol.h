/*******************************************************************************
* smac_user_protocol header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* usercribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#ifndef __SMAC_USER_PROTOCOL_H__
#define __SMAC_USER_PROTOCOL_H__

#include "stack/mac/protocol/smac_trans_protocol.h"

/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Public type definitions
********************************************************************************
*******************************************************************************/

typedef enum
{
    smacUserReceiveType_DISCARD,
    smacUserReceiveType_INPUT,
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    smacUserReceiveType_FORWARD,
#endif
} smacUserReceiveType_t;

typedef struct smacRouteParams_tag
{
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    uint8_t     hop_cnt;
    uint16_t    route[3];
    uint16_t    endNodeAddr;
    uint16_t    oriNodeAddr;
#else
    uint8_t dummy;
#endif
} smacRouteParams_t;

#include "stack/mac/protocol/smac_user_protocol_lora.h"
#include "stack/mac/protocol/smac_user_protocol_fsk.h"

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

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif //__SMAC_USER_PROTOCOL_H__
