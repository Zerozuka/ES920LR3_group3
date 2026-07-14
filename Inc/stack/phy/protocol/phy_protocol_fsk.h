/*******************************************************************************
* phy_protocol_fsk header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#ifndef __PHY_PROTOCOL_FSK_H__
#define __PHY_PROTOCOL_FSK_H__

#include "usr_common.h"

/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/

#if defined(BLD_USE_FSK_R)
    #define PhyFrameOverhead_FSK        ( sizeof(phyHeader_FSK_t) + sizeof(phyFooter_FSK_t) )   // fsk:4
    #define PhyFrameTotalSize_FSK(sduSize) ( (sduSize) + PhyFrameOverhead_FSK )
    #define PhyFrameTotalSizeMax_FSK    ( RadioPayloadSizeMax )                                 // fsk:255
    #define PhySduSizeMax_FSK           ( PhyFrameTotalSizeMax_FSK - PhyFrameOverhead_FSK )     // fsk:251

/*******************************************************************************
********************************************************************************
* Public type definitions
********************************************************************************
*******************************************************************************/

    // PhyHeader
    typedef struct phyHeader_FSK_tag
    {
        union{
            uint8_t  byteAccess[2];
            struct{
                uint8_t     modeSwitch          :1;
                uint8_t     reserved            :2;
                uint8_t     fcsType             :1;
                uint8_t     dataWhitening       :1;
                uint8_t     frameLengthRsvd     :3;
                uint8_t     frameLength;
            };
        };
    } phyHeader_FSK_t;

    // PhyFooter
    typedef struct phyFooter_FSK_tag
    {
        octet2_t    fcs;
    } phyFooter_FSK_t;

    // PhyFrame
    typedef struct  phyFrame_FSK_tag phyFrame_FSK_t;

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

    void    PhyFrame_makeFrame_FSK(phyFrame_FSK_t* frame, uint8_t sduSize);
    bool_t  PhyFrame_analyzeFrame_FSK(phyFrame_FSK_t* frame, uint8_t recvSize, uint8_t* sduSize);

#endif

#endif //__PHY_PROTOCOL_FSK_H__
