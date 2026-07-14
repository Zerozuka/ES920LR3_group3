/*******************************************************************************
* usr_config header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#ifndef __USR_CONFIG_H
#define __USR_CONFIG_H

/*******************************************************************************
********************************************************************************
* Compile Switch
********************************************************************************
*******************************************************************************/

// Protocols to support
#define BLD_USE_LORA_NR     // EASEL Private LoRa (ES920LR compatible)
#define BLD_USE_LORA_R      // EASEL Private LoRa with Static Routing
#define BLD_USE_FSK_R       // EASEL FSK with Static Routing (ES920 compatible)

// Option
//#define BLD_USE_SPCMD_RESET

// Adaptive spreading-factor control loop (SF7 <-> SF10 by interference).
// See app/adaptive_sf.h. Requires a LoRa protocol and RfMode == RFMODE_TXRX.
#define BLD_ENABLE_ADAPTIVE_SF

// for Debug
//#define BLD_DEBUG_PRINT

// for Test
//#define BLD_ENABLE_RFMODE_BURST
//#define BLD_ENABLE_RFMODE_CW
//#define BLD_ENABLE_RFMODE_TX_INFINITE
//#define BLD_ENABLE_RFMODE_RSSI_CHECK


/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/

// Version
#define SOFTWARE_VERSION    "VER 1.05"

#endif /* __USR_CONFIG_H */
