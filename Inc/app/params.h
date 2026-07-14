/*******************************************************************************
* params header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/
#ifndef _PARAMS_H_
#define _PARAMS_H_

#include "app/do_configuration.h"
#include "app/do_operation.h"

#ifdef __cplusplus
    extern "C" {
#endif

/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/

    #define MODE_ON                 1
    #define MODE_OFF                2

    #define COORDINATOR             1
    #define END_DEVICE              2
    #define ROUTER                  3

    #define CONFIG                  1
    #define OPERATION               2

    #define TERMINAL                1
    #define PROCESSOR               2

    #define RFMODE_TXRX             1
    #define RFMODE_TXONLY           2
#if defined(BLD_ENABLE_RFMODE_BURST)
    #define RFMODE_BURST            3
#endif
#if defined(BLD_ENABLE_RFMODE_CW)
    #define RFMODE_CW               4
#endif
#if defined(BLD_ENABLE_RFMODE_TX_INFINITE)
    #define RFMODE_TX_INFINITE      5
#endif
#if defined(BLD_ENABLE_RFMODE_RSSI_CHECK)
    #define RFMODE_RSSI_CHECK       6
#endif

    #define NO_SLEEP                1
    #define TIMER_WAKEUP            2
    #define INT_WAKEUP              3
    #define INT_WAKEUP_1TX          4
    #define UART_WAKEUP             5

    #define IS_SLEEP(param)             ( ( (param).Node==END_DEVICE ) && ( (param).Sleep!=NO_SLEEP ) )
    #define IS_TIMER_SLEEP(param)       ( ( (param).Node==END_DEVICE ) && ( (param).Sleep==TIMER_WAKEUP ) )
    #define IS_INT_SLEEP(param)         ( ( (param).Node==END_DEVICE ) && ( (param).Sleep==INT_WAKEUP || (param).Sleep==INT_WAKEUP_1TX ) )
    #define IS_INT_SLEEP_1TX(param)     ( ( (param).Node==END_DEVICE ) && ( (param).Sleep==INT_WAKEUP_1TX ) )
    #define IS_SLEEP_ON_TX_DONE(param)  ( ( (param).Node==END_DEVICE ) && ( (param).Sleep==TIMER_WAKEUP || (param).Sleep==INT_WAKEUP_1TX ) )
    #define IS_UART_SLEEP(param)        ( ( (param).Node==END_DEVICE ) && ( (param).Sleep==UART_WAKEUP ) )

    #define MCU_STOP                1
    #define MCU_SLEEP               2

    #define IS_MCU_STOP(param)          ( IS_SLEEP(param) && ( (param).LpModeMcu!=MCU_SLEEP ) )
    #define IS_MCU_SLEEP(param)         ( IS_SLEEP(param) && ( (param).LpModeMcu==MCU_SLEEP ) )

    #define RF_SLEEP_COLD           1
    #define RF_SLEEP_WARM           2
    #define RF_ACTIVE               3

    #define IS_RF_SLEEP_WARM(param)     ( IS_SLEEP(param) && ( (param).LpModeRf==RF_SLEEP_WARM ) )
    #define IS_RF_SLEEP_COLD(param)     ( IS_SLEEP(param) && ( (param).LpModeRf==RF_SLEEP_COLD ) )
    #define IS_RF_SLEEP(param)          ( IS_SLEEP(param) && ( (param).LpModeRf!=RF_ACTIVE ) )
    #define IS_RF_ACTIVE(param)         ( !IS_SLEEP(param) || ( (param).LpModeRf==RF_ACTIVE ) )

    #define TRANS_PAYLOAD           1
    #define TRANS_FRAME             2

    #define FMT_ASCII               1
    #define FMT_BINARY              2

    #define MIN_SLEEP_TIME          1
    #define MAX_SLEEP_TIME          864000
    #define MIN_SEND_TIME           0
    #define MAX_SEND_TIME           86400

    #define MIN_RETRY_BACKOFF       0
    #define MAX_RETRY_BACKOFF       60000
		
    // default setting
    #define DEFAULT_Baudrate        UART_BAUD_RATE_115200_c
    #define DEFAULT_SleepTime       50
    #define DEFAULT_Power           13
    #define DEFAULT_Ack             MODE_ON
    #define DEFAULT_Rate            RATE_50KBPS
    #define DEFAULT_Bw              BANDWIDTH125
    #define DEFAULT_Channel         1
    #define DEFAULT_DstId           0x0000
    #define DEFAULT_EndId           0x0000
    #define DEFAULT_HopCnt          0x0001
    #define DEFAULT_Node            END_DEVICE
    #define DEFAULT_Operation       CONFIG
    #define DEFAULT_PanId           0x0001
    #define DEFAULT_RcvId           MODE_OFF
    #define DEFAULT_Retry           3
    #define DEFAULT_Route           0x0001
    #define DEFAULT_Rssi            MODE_OFF
    #define DEFAULT_Sf              7
    #define DEFAULT_Sleep           NO_SLEEP
    #define DEFAULT_LpModeMcu       MCU_STOP
    #define DEFAULT_LpModeRf        RF_SLEEP_COLD
    #define DEFAULT_SrcId           0x0001
    #define DEFAULT_TransMode       TRANS_PAYLOAD
    #define DEFAULT_Format          FMT_ASCII
    #define DEFAULT_RfMode          RFMODE_TXRX
#if defined(BLD_USE_LORA_NR)
    #define DEFAULT_Protocol        Protocol_LORA_NR
#elif defined(BLD_USE_LORA_R)
    #define DEFAULT_Protocol        Protocol_LORA_R
#elif defined(BLD_USE_FSK_R)
    #define DEFAULT_Protocol        Protocol_FSK_R
#endif
    #define DEFAULT_RxBoost         MODE_ON
    #define DEFAULT_SendTime        0
    #define DEFAULT_SendData        ""
    #define DEFAULT_AesKey          NullAesKey
    #define DEFAULT_Backoff	        0

/*******************************************************************************
********************************************************************************
* Public type definitions
********************************************************************************
*******************************************************************************/

typedef struct tagTERMINAL_PARAM
{
    // [0-127]
    uint8_t     Once[8];            /* write once */
    uint32_t    Baudrate;           /* baudrate   */
    uint32_t    SleepTime;          /* sleeptime  */
    int32_t     Power;              /* power      */
    uint32_t    SendTime;           /* sendtime   */
    uint8_t     SendData[52];       /* senddata   */
    uint8_t     AesKey[16];         /* aeskey     */
    uint32_t    Backoff;		    /* backoff    */
    uint32_t    rsv_1[8];

    // [128-]
    uint16_t    Ack;                /* ack        */
    uint16_t    Rate;               /* rate       */
    uint16_t    Bw;                 /* bw         */
    uint16_t    Channel;            /* channel    */
    uint16_t    DstId;              /* dstid      */
    uint16_t    EndId;              /* endid      */
    uint16_t    HopCnt;             /* hopcount   */
    uint16_t    Mode;               /* mode       */
    uint16_t    Node;               /* node       */
    uint16_t    Operation;          /* operation  */
    uint16_t    PanId;              /* panid      */
    uint16_t    RcvId;              /* rcvid      */
    uint16_t    Retry;              /* retry      */
    uint16_t    Route[3];           /* route      */
    uint16_t    Rssi;               /* rssi       */
    uint16_t    Sf;                 /* sf         */
    uint16_t    Sleep;              /* sleep      */
    uint16_t    LpModeMcu;          /* lpmode(MCU)*/
    uint16_t    LpModeRf;           /* lpmode(RF) */
    uint16_t    SrcId;              /* srcid      */
    uint16_t    TransMode;          /* transmode  */
    uint16_t    Format;             /* format     */
    uint16_t    RfMode;             /* rfmode     */
    uint16_t    Protocol;           /* protocol   */
    uint16_t    RxBoost;            /* rxboost    */
} TERMINAL_PARAM;

extern TERMINAL_PARAM   mTermParam;
extern const uint8_t NullAesKey[16];

/*******************************************************************************
********************************************************************************
* Public Prototypes
********************************************************************************
*******************************************************************************/

void    TermParam_loadFromRom( void );
uint8_t TermParam_saveToRom( void );
void    TermParam_loadDefault( void );

#ifdef __cplusplus
}
#endif

/******************************************************************************/
#endif /* _PARAMS_H_ */
