/*******************************************************************************
* serial file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#include "usr_common.h"
#include "platform/serial.h"
#include "platform/pendsv.h"

/*******************************************************************************
********************************************************************************
* Private macros
********************************************************************************
*******************************************************************************/
#define SERIAL_RX_BUFFER_LENGTH    512
#define SERIAL_TX_BUFFER_LENGTH    512  // 0: no buffering

/*******************************************************************************
********************************************************************************
* Private type definitions
********************************************************************************
*******************************************************************************/

typedef enum serialTxState_tag
{
    SERIAL_TX_STATE_IDLE,
    SERIAL_TX_STATE_TX,
} serialTxState_t;

typedef enum serialRxState_tag
{
    SERIAL_RX_STATE_IDLE,               // idle : no request, local buffer empty
    SERIAL_RX_STATE_RX_TO_LOC,          // buffering  : reading byte to local buffer, no request
    SERIAL_RX_STATE_RX_TO_LOC_USR_REQ,  // buffering  : reading byte to local buffer, user requesting, local buffer empty
    SERIAL_RX_STATE_RX_TO_USR_USR_REQ,  // buffering  : reading to usr buffer, user requesting, local buffer empty
} serialRxState_t;

/*******************************************************************************
********************************************************************************
* Private prototypes
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Private memory declarations
********************************************************************************
*******************************************************************************/
static serialRxState_t              g_serialRxState = SERIAL_RX_STATE_IDLE;
static volatile serialTxState_t    g_serialTxState = SERIAL_TX_STATE_IDLE;

static uint8_t                 g_rxBuff[SERIAL_RX_BUFFER_LENGTH];
static uint16_t                g_rxBuffDataIdx;
static uint16_t                g_rxBuffDataLen;
static uint8_t*                g_usrRxBuff;
static uint16_t                g_usrRxLen;
static uint16_t                g_usrRxFilled;
static SerialRxCompCallback_t  g_rxCallback    = NULL;

static uint8_t                 g_txBuff[SERIAL_TX_BUFFER_LENGTH];
static uint16_t                g_txBuffDataIdx;
static uint16_t                g_txBuffDataLen;
static uint16_t                g_uartTxSize;
static SerialTxCompCallback_t  g_txCallback    = NULL;

/*******************************************************************************
********************************************************************************
* Public memory declarations
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Public functions
********************************************************************************
*******************************************************************************/

void Serial1_Init( void )
{
    g_serialTxState = SERIAL_TX_STATE_IDLE;
    g_serialRxState = SERIAL_RX_STATE_IDLE;

    g_rxBuffDataIdx = 0;
    g_rxBuffDataLen = 0;
    g_usrRxBuff     = NULL;
    g_usrRxLen      = 0;
    g_usrRxFilled   = 0;
    g_rxCallback    = NULL;

    g_txBuffDataIdx = 0;
    g_txBuffDataLen = 0;
    g_uartTxSize    = 0;
    g_txCallback    = NULL;
}

void Serial1_SetBaudRate( uint32_t baudRate )
{
    HAL_UART_AbortReceive_IT( &huart2 );

    MX_USART_SetBaudrate( &huart2, baudRate );

    HAL_UART_Receive_IT( &huart2, g_rxBuff, 1 );
}

void Serial1_rxStart( void )
{
    usrDISABLE_INTERRUPTS();
    {
        switch( g_serialRxState )
        {
        case SERIAL_RX_STATE_RX_TO_LOC:
        case SERIAL_RX_STATE_RX_TO_LOC_USR_REQ:
        case SERIAL_RX_STATE_RX_TO_USR_USR_REQ:
            // stop rx if running
            HAL_UART_AbortReceive_IT( &huart2 );

        // break through
        case SERIAL_RX_STATE_IDLE:
            // start rx buffering
            g_serialRxState = SERIAL_RX_STATE_RX_TO_LOC;
            g_rxBuffDataIdx = 0;
            g_rxBuffDataLen = 0;
            g_usrRxBuff     = NULL;
            g_usrRxLen      = 0;
            g_usrRxFilled   = 0;
            g_rxCallback    = NULL;

            HAL_UART_Receive_IT( &huart2, g_rxBuff, 1 );

            break;
        }
    }
    usrENABLE_INTERRUPTS();
}

void Serial1_rxStop( void )
{
    usrDISABLE_INTERRUPTS();
    {
        switch( g_serialRxState )
        {
        case SERIAL_RX_STATE_RX_TO_LOC:
        case SERIAL_RX_STATE_RX_TO_LOC_USR_REQ:
        case SERIAL_RX_STATE_RX_TO_USR_USR_REQ:
            // stop rx if running
            HAL_UART_AbortReceive_IT( &huart2 );

        // break through
        case SERIAL_RX_STATE_IDLE:
            // start rx buffering
            g_serialRxState = SERIAL_RX_STATE_IDLE;
            g_rxBuffDataIdx = 0;
            g_rxBuffDataLen = 0;
            g_usrRxBuff     = NULL;
            g_usrRxLen      = 0;
            g_usrRxFilled   = 0;
            g_rxCallback    = NULL;

            break;
        }
    }
    usrENABLE_INTERRUPTS();
}

void Serial1_FlushRead( void )
{
    usrDISABLE_INTERRUPTS();
    {
        switch( g_serialRxState )
        {
        case SERIAL_RX_STATE_IDLE:
            break;

        case SERIAL_RX_STATE_RX_TO_LOC:
        case SERIAL_RX_STATE_RX_TO_LOC_USR_REQ:
        case SERIAL_RX_STATE_RX_TO_USR_USR_REQ:
            // abort rx
            HAL_UART_AbortReceive_IT( &huart2 );

            // flush
            g_rxBuffDataIdx = 0;
            g_rxBuffDataLen = 0;
            g_usrRxBuff     = NULL;
            g_usrRxLen      = 0;
            g_usrRxFilled   = 0;
            g_rxCallback    = NULL;

            // restart rx buffering
            g_serialRxState = SERIAL_RX_STATE_RX_TO_LOC;

            HAL_UART_Receive_IT( &huart2, g_rxBuff, 1 );

            break;
        }
    }
    usrENABLE_INTERRUPTS();
}

bool_t Serial1_Read( uint8_t* buff, uint16_t len, SerialRxCompCallback_t callback )
{
    bool_t complete = FALSE;
    uint16_t copyLen;

    usrDISABLE_INTERRUPTS();
    {
        switch( g_serialRxState )
        {
        case SERIAL_RX_STATE_RX_TO_LOC_USR_REQ:
        case SERIAL_RX_STATE_RX_TO_USR_USR_REQ:
            // cancel previous user req
            HAL_UART_AbortReceive_IT( &huart2 );

            g_serialRxState = SERIAL_RX_STATE_IDLE;
            g_rxBuffDataIdx = 0;
            g_rxBuffDataLen = 0;
            g_usrRxBuff     = NULL;
            g_usrRxLen      = 0;
            g_usrRxFilled   = 0;
            g_rxCallback    = NULL;

            // break through

        case SERIAL_RX_STATE_IDLE:
            // direct read from uart
            g_serialRxState = SERIAL_RX_STATE_RX_TO_USR_USR_REQ;
            g_rxCallback    = callback;

            HAL_UART_Receive_IT( &huart2, buff, len );

            break;

        case SERIAL_RX_STATE_RX_TO_LOC:
            // copy to local buffer to user buffer
            if( len <= g_rxBuffDataLen )
            {
                // user size <= local filled size
                copyLen  = len;
                complete = TRUE;
            }
            else
            {
                // local filled size < user size
                copyLen         = g_rxBuffDataLen;
                g_serialRxState = SERIAL_RX_STATE_RX_TO_LOC_USR_REQ;
                g_rxCallback    = callback;
                g_usrRxBuff     = buff;
                g_usrRxLen      = len;
                g_usrRxFilled   = copyLen;
            }

            // copy local buffer to user buffer
            if( g_rxBuffDataIdx + copyLen < SERIAL_RX_BUFFER_LENGTH )
            {
                memcpy( buff, &g_rxBuff[g_rxBuffDataIdx], copyLen );
            }
            else
            {
                memcpy( buff, &g_rxBuff[g_rxBuffDataIdx], SERIAL_RX_BUFFER_LENGTH - g_rxBuffDataIdx );
                memcpy( buff, &g_rxBuff[0], g_rxBuffDataIdx + copyLen - SERIAL_RX_BUFFER_LENGTH );
            }
            g_rxBuffDataLen -= copyLen;
            g_rxBuffDataIdx += copyLen;
            g_rxBuffDataIdx %= SERIAL_RX_BUFFER_LENGTH;

            break;
        }
    }
    usrENABLE_INTERRUPTS();

    return complete;
}

void Serial1_Print( const char* pString, ... )
{
    WDG_Refresh();

    va_list arg;

    va_start(arg, pString);

    Serial1_PrintV(pString, arg);

    va_end(arg);
}

void Serial1_PrintV( const char* pString, va_list arg )
{
    char str[256];
    uint16_t len;

    // make text
    len = (uint16_t) usr_vsnprintf(str, sizeof(str), pString, arg);

    // write text
    Serial1_Write((uint8_t*)str, len);
}

void Serial1_PrintStr( char* pString )
{
    Serial1_Write((uint8_t*)pString, (uint16_t)strlen(pString));
}

void Serial1_PrintHex( const uint8_t* buff, uint16_t length, const char* separator )
{
    uint16_t i;
    for(i = 0; i < length; i++)
    {
        Serial1_Print( "%02X%s", (unsigned)buff[i], separator );
    }
}

uint8_t* g_usrTxBuff = NULL;
volatile uint16_t g_usrTxLen = 0;

void Serial1_Write( uint8_t* buff, uint16_t len )
{
    if( len > 0 )
    {
        usrDISABLE_INTERRUPTS();
        {
            while( g_usrTxLen > 0 )
            {
                usrENABLE_INTERRUPTS();
                usrDISABLE_INTERRUPTS();
            }

            g_usrTxBuff = buff;
            g_usrTxLen  = len;

            PendSV_SetEvent( PSV_EVENT_CALL_Serial1_Write );
        }
        usrENABLE_INTERRUPTS();
    }

    while( g_usrTxLen > 0 );
}

void PendSvProcess_Serial1_Write( void )
{
    uint16_t writePoint;
    uint16_t writePointToTail;
    uint16_t freeSize;
    uint16_t copySize;

    switch( g_serialTxState )
    {
    case SERIAL_TX_STATE_IDLE:
        // copy to local buffer
        if( g_usrTxLen < SERIAL_TX_BUFFER_LENGTH )
        {
            copySize = g_usrTxLen;
        }
        else
        {
            copySize = SERIAL_TX_BUFFER_LENGTH;
        }
        memcpy( g_txBuff, g_usrTxBuff, copySize );
        g_usrTxBuff     += copySize;
        g_usrTxLen      -= copySize;
        g_txBuffDataIdx = 0;
        g_txBuffDataLen = copySize;

        // start uart tx
        g_uartTxSize    = copySize;
        HAL_UART_Transmit_IT( &huart2, g_txBuff, g_uartTxSize );
        g_serialTxState = SERIAL_TX_STATE_TX;

        break;

    case SERIAL_TX_STATE_TX:
        if( g_usrTxLen )
        {
            freeSize = SERIAL_TX_BUFFER_LENGTH - g_txBuffDataLen;
            if( freeSize == 0 )
            {
                // buffer full -> wait for complete to uart tx
            }
            else
            {
                if( g_usrTxLen < freeSize )
                {
                    copySize = g_usrTxLen;
                }
                else
                {
                    copySize = freeSize;
                }

                // copy to local buffer
                writePoint       = (g_txBuffDataIdx + g_txBuffDataLen) % SERIAL_TX_BUFFER_LENGTH;
                writePointToTail = SERIAL_TX_BUFFER_LENGTH - writePoint;

                if( copySize <= writePointToTail )
                {
                    memcpy( g_txBuff + writePoint, g_usrTxBuff, copySize );
                }
                else
                {
                    memcpy( g_txBuff + writePoint, g_usrTxBuff, writePointToTail );
                    memcpy( g_txBuff, g_usrTxBuff + writePointToTail, copySize - writePointToTail );
                }
                g_usrTxBuff     += copySize;
                g_usrTxLen      -= copySize;
                g_txBuffDataLen += copySize;
            }
        }
        break;
    }
}

void Serial1_FlushWrite( void )
{
    while( g_serialTxState != SERIAL_TX_STATE_IDLE );
}

bool_t Serial1_IsWriteFlushed( SerialTxCompCallback_t callback )
{
    bool_t complete = FALSE;

    usrDISABLE_INTERRUPTS();
    {
        switch( g_serialTxState )
        {
        case SERIAL_TX_STATE_IDLE:
            complete = TRUE;
            break;

        case SERIAL_TX_STATE_TX:
            // wait for complete all data to transmitted
            g_txCallback = callback;
            break;
        }
    }
    usrENABLE_INTERRUPTS();

    return complete;
}

void HAL_UART_RxCpltCallback( UART_HandleTypeDef *huart )
{
    SerialRxCompCallback_t callback = NULL;
    uint16_t writePoint;

    switch( g_serialRxState )
    {
    case SERIAL_RX_STATE_IDLE:
        break;

    //------------------------
    // Buffering
    //------------------------
    case SERIAL_RX_STATE_RX_TO_LOC:
        if( g_rxBuffDataLen < SERIAL_RX_BUFFER_LENGTH )
        {
            g_rxBuffDataLen++;
        }

        // read next byte
        writePoint = (g_rxBuffDataIdx + g_rxBuffDataLen) % SERIAL_RX_BUFFER_LENGTH;
        HAL_UART_Receive_IT( &huart2, &g_rxBuff[writePoint], 1 );

        break;

    //------------------------
    // User requesting
    //------------------------
    case SERIAL_RX_STATE_RX_TO_LOC_USR_REQ:
        // copy to user buffer
        writePoint = (g_rxBuffDataIdx + g_rxBuffDataLen) % SERIAL_RX_BUFFER_LENGTH;
        g_usrRxBuff[g_usrRxFilled++] = g_rxBuff[writePoint];
        g_rxBuffDataIdx %= SERIAL_RX_BUFFER_LENGTH;

        if( g_usrRxFilled < g_usrRxLen )
        {
            g_serialRxState = SERIAL_RX_STATE_RX_TO_USR_USR_REQ;

            // read remain
            HAL_UART_Receive_IT( &huart2, &g_usrRxBuff[g_usrRxFilled], g_usrRxLen - g_usrRxFilled );

            break;
        }
        else
        {
            // break through
        }

    case SERIAL_RX_STATE_RX_TO_USR_USR_REQ:

        // start rx buffering
        g_serialRxState = SERIAL_RX_STATE_RX_TO_LOC;
        g_rxBuffDataIdx = 0;
        g_rxBuffDataLen = 0;

        HAL_UART_Receive_IT( &huart2, g_rxBuff, 1 );

        //----------------------
        // complete request
        //----------------------
        callback      = g_rxCallback;
        g_usrRxBuff   = NULL;
        g_usrRxLen    = 0;
        g_usrRxFilled = 0;
        g_rxCallback  = NULL;

        if( callback != NULL )
        {
            callback();
        }
        break;
    }
}

void HAL_UART_TxCpltCallback( UART_HandleTypeDef *huart )
{
    PendSV_SetEvent( PSV_EVENT_UART_TxCplt );
}

void PendSvProcess_UART_TxCplt( void )
{
    SerialTxCompCallback_t callback = NULL;
    uint16_t readPointToTail;

    switch( g_serialTxState )
    {
    case SERIAL_TX_STATE_IDLE:
        // to do nothing
        break;

    case SERIAL_TX_STATE_TX:
        g_txBuffDataLen -= g_uartTxSize;
        g_txBuffDataIdx += g_uartTxSize;
        g_txBuffDataIdx %= SERIAL_TX_BUFFER_LENGTH;
        g_uartTxSize = 0;

        if( g_usrTxLen > 0 )
        {
            if( g_txBuffDataLen == 0 )
            {
                g_txBuffDataIdx = 0;
            }

            const uint16_t freeSize = SERIAL_TX_BUFFER_LENGTH - g_txBuffDataLen;
            if( freeSize == 0 )
            {
                // buffer full -> wait for complete to uart tx
            }
            else
            {
                uint16_t copySize = g_usrTxLen;
                if( copySize > freeSize )
                {
                    copySize = freeSize;
                }

                // copy to local buffer
                const uint16_t writePoint = (g_txBuffDataIdx + g_txBuffDataLen) % SERIAL_TX_BUFFER_LENGTH;
                const uint16_t writePointToTail = SERIAL_TX_BUFFER_LENGTH - writePoint;

                if( copySize <= writePointToTail )
                {
                    memcpy( g_txBuff + writePoint, g_usrTxBuff, copySize );
                }
                else
                {
                    memcpy( g_txBuff + writePoint, g_usrTxBuff, writePointToTail );
                    memcpy( g_txBuff, g_usrTxBuff + writePointToTail, copySize - writePointToTail );
                }
                g_usrTxBuff += copySize;
                g_usrTxLen -= copySize;
                g_txBuffDataLen += copySize;
            }
        }

        if( g_txBuffDataLen == 0 )
        {
            // complete
            callback        = g_txCallback;
            g_serialTxState = SERIAL_TX_STATE_IDLE;
            g_txBuffDataIdx = 0;
            g_txCallback    = NULL;

            // flush complete
            if( callback != NULL )
            {
                callback();
            }
        }
        else
        {
            // tx remain data
            readPointToTail = SERIAL_TX_BUFFER_LENGTH - g_txBuffDataIdx;
            if( readPointToTail < g_txBuffDataLen )
            {
                g_uartTxSize = readPointToTail;
            }
            else
            {
                g_uartTxSize = g_txBuffDataLen;
            }
            HAL_UART_Transmit_IT( &huart2, g_txBuff + g_txBuffDataIdx, g_uartTxSize );
        }
        break;
    }
}
