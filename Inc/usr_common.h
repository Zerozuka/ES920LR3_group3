/*******************************************************************************
* usr_common header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#ifndef _USR_COMMON_H_
#define _USR_COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include "stm32wlxx_hal.h"
#include "config/usr_config.h"

/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/

extern unsigned gIntDisableRecursion;

#define _countof(array) (sizeof(array)/sizeof(*array))
#define OUT_OF_RANGE(n, min, max)   ( (n) < (min) || (max) < (n) )

#define usrDISABLE_INTERRUPTS() { __disable_irq(); gIntDisableRecursion++; }
#define usrENABLE_INTERRUPTS()  { if( --gIntDisableRecursion == 0 ) { __enable_irq(); } }

/* For Debug ------------------------------------------------------------*/

    #define ESCSEQ(c)   "\033[" c
    #define DEF         ESCSEQ("39m")
    #define BLA         ESCSEQ("30m")
    #define RED         ESCSEQ("31m")
    #define GRE         ESCSEQ("32m")
    #define YEL         ESCSEQ("33m")
    #define BLU         ESCSEQ("34m")
    #define MAG         ESCSEQ("35m")
    #define CYA         ESCSEQ("36m")
    #define WHY         ESCSEQ("37m")

#ifdef BLD_DEBUG_PRINT
    #define DEBUG_PRINT(...)              Serial1_Print(__VA_ARGS__)
#else
    #define DEBUG_PRINT(...)
#endif
#define DEBUG_PRINT_NL(...)               DEBUG_PRINT("%08u\t", HAL_GetTick()); DEBUG_PRINT(__VA_ARGS__)

#ifdef RADIO_DEBUG_CMD
    #define RADIO_DEBUG_CMD_PRINT_NL(...) DEBUG_PRINT_NL(RED "            " __VA_ARGS__); DEBUG_PRINT(DEF)
    #define RADIO_DEBUG_CMD_PRINT(...)    DEBUG_PRINT(RED __VA_ARGS__); DEBUG_PRINT(DEF)
#else
    #define RADIO_DEBUG_CMD_PRINT_NL(...)
#endif

#ifdef RADIO_DEBUG
    #define RADIO_DEBUG_PRINT(...)        DEBUG_PRINT_NL(GRE "          " __VA_ARGS__); DEBUG_PRINT(DEF)
#else
    #define RADIO_DEBUG_PRINT(...)
#endif

#ifdef PHY_RADIO_DEBUG
    #define PHY_RADIO_DEBUG_PRINT(...)    DEBUG_PRINT_NL(YEL "        " __VA_ARGS__); DEBUG_PRINT(DEF)
#else
    #define PHY_RADIO_DEBUG_PRINT(...)
#endif

#ifdef PHY_DEBUG
    #define PHY_DEBUG_PRINT(...)          DEBUG_PRINT_NL(BLU "      " __VA_ARGS__); DEBUG_PRINT(DEF)
#else
    #define PHY_DEBUG_PRINT(...)
#endif

#ifdef SMAC_TRANS_DEBUG
    #define SMAC_TRANS_DEBUG_PRINT(...)   DEBUG_PRINT_NL(MAG "    " __VA_ARGS__); DEBUG_PRINT(DEF)
#else
    #define SMAC_TRANS_DEBUG_PRINT(...)
#endif

#ifdef SMAC_USER_DEBUG
    #define SMAC_USER_DEBUG_PRINT(...)    DEBUG_PRINT_NL(CYA "  " __VA_ARGS__); DEBUG_PRINT(DEF)
#else
    #define SMAC_USER_DEBUG_PRINT(...)
#endif

/*******************************************************************************
********************************************************************************
* Public type definitions
********************************************************************************
*******************************************************************************/

typedef uint8_t    bool_t;
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef __cplusplus
typedef uint8_t    bool;
#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif
#endif

typedef struct octet2_tag
{
    uint8_t  l;
    uint8_t  h;
} octet2_t;

#define octet2_get(p)       ( ( (uint16_t)(p)->l    ) | \
                            ( (uint16_t)(p)->h << 8 ) )
#define octet2_set(p, val)  { (p)->l = (uint8_t)(((val) >> 0) & 0xFF);  \
                              (p)->h = (uint8_t)(((val) >> 8) & 0xFF);  }

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


#include "platform/serial.h"
#include "app/usr_util.h"
#include "app/usr_main.h"

#endif /* _USR_COMMON_H_ */
