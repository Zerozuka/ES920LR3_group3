/*******************************************************************************
* memmap header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#ifndef _MEMMAP_H_
#define _MEMMAP_H_

#include "app/params.h"

/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/

#define FLASH_PARAM_TOP             0x0803F800

#define FLASH_OFS(membername)       ((uint32_t)&((TERMINAL_PARAM*)0)->membername)

#define FLASH_OFS_ONCE              FLASH_OFS(Once)             // +0
#define FLASH_OFS_BAUDRATE          FLASH_OFS(Baudrate)
#define FLASH_OFS_SLEEPTIME         FLASH_OFS(SleepTime)
#define FLASH_OFS_POWER             FLASH_OFS(Power)
#define FLASH_OFS_SENDTIME          FLASH_OFS(SendTime)
#define FLASH_OFS_SENDDATA          FLASH_OFS(SendData)
#define FLASH_OFS_AESKEY            FLASH_OFS(AesKey)
#define FLASH_OFS_ACK               FLASH_OFS(Ack)              // +128
#define FLASH_OFS_RATE              FLASH_OFS(Rate)
#define FLASH_OFS_BW                FLASH_OFS(Bw)
#define FLASH_OFS_CHANNEL           FLASH_OFS(Channel)
#define FLASH_OFS_DSTID             FLASH_OFS(DstId)
#define FLASH_OFS_ENDID             FLASH_OFS(EndId)
#define FLASH_OFS_HOPCNT            FLASH_OFS(HopCnt)
#define FLASH_OFS_MODE              FLASH_OFS(Mode)
#define FLASH_OFS_NODE              FLASH_OFS(Node)
#define FLASH_OFS_OPERATION         FLASH_OFS(Operation)
#define FLASH_OFS_PANID             FLASH_OFS(PanId)
#define FLASH_OFS_RCVID             FLASH_OFS(RcvId)
#define FLASH_OFS_RETRY             FLASH_OFS(Retry)
#define FLASH_OFS_ROUTE1            FLASH_OFS(Route[0])
#define FLASH_OFS_ROUTE2            FLASH_OFS(Route[1])
#define FLASH_OFS_ROUTE3            FLASH_OFS(Route[2])
#define FLASH_OFS_RSSI              FLASH_OFS(Rssi)
#define FLASH_OFS_SF                FLASH_OFS(Sf)
#define FLASH_OFS_SLEEP             FLASH_OFS(Sleep)
#define FLASH_OFS_LPMODE_MCU        FLASH_OFS(LpModeMcu)
#define FLASH_OFS_LPMODE_RF         FLASH_OFS(LpModeRf)
#define FLASH_OFS_SRCID             FLASH_OFS(SrcId)
#define FLASH_OFS_TRANSMODE         FLASH_OFS(TransMode)
#define FLASH_OFS_FORMAT            FLASH_OFS(Format)
#define FLASH_OFS_RFMODE            FLASH_OFS(RfMode)
#define FLASH_OFS_PROTOCOL          FLASH_OFS(Protocol)
#define FLASH_OFS_RXBOOST           FLASH_OFS(RxBoost)
#define FLASH_OFS_BACKOFF           FLASH_OFS(Backoff)

#define FLASH_ReadShortWord(offset)             (*((uint16_t*)(FLASH_PARAM_TOP + (offset))))
#define FLASH_ReadLongWord(offset)              (*((uint32_t*)(FLASH_PARAM_TOP + (offset))))
#define FLASH_ReadBytes(dst, offset, length)    memcpy( (dst), (uint8_t*)(FLASH_PARAM_TOP + (offset)), (length))

#endif
