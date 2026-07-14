/*******************************************************************************
* serial header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/
#ifndef _SERIAL_H_
#define _SERIAL_H_

#include "usart.h"

/*******************************************************************************
********************************************************************************
* Public memory declarations
********************************************************************************
*******************************************************************************/

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
    SerialRxMode_TEXT,
    SerialRxMode_BINARY,
} SerialRxMode_t;

typedef void (*SerialRxCompCallback_t)(void);
typedef void (*SerialTxCompCallback_t)(void);

typedef enum{
    UART_BAUD_RATE_1200_c   =   1200UL,
    UART_BAUD_RATE_2400_c   =   2400UL,
    UART_BAUD_RATE_4800_c   =   4800UL,
    UART_BAUD_RATE_9600_c   =   9600UL,
    UART_BAUD_RATE_19200_c  =  19200UL,
    UART_BAUD_RATE_38400_c  =  38400UL,
    UART_BAUD_RATE_57600_c  =  57600UL,
    UART_BAUD_RATE_115200_c = 115200UL,
    UART_BAUD_RATE_230400_c = 230400UL,
#if 0
    UART_BAUD_RATE_460800_c = 460800UL,
    UART_BAUD_RATE_921600_c = 921600UL,
#endif
} serialUartBaudRate_t;

#define HexToAscii(hex) (uint8_t)( ((hex) & 0x0F) + ((((hex) & 0x0F) <= 9) ? '0' : ('A'-10)) )

/*******************************************************************************
********************************************************************************
* Public memory declarations
********************************************************************************
*******************************************************************************/
extern UART_HandleTypeDef huart2;

/*******************************************************************************
********************************************************************************
* Public Prototypes
********************************************************************************
*******************************************************************************/

void Serial1_Init( void );
void Serial1_SetBaudRate( uint32_t baudRate );

void Serial1_rxStart( void );
void Serial1_rxStop( void );

bool_t Serial1_Read( uint8_t* buff, uint16_t len, SerialRxCompCallback_t callback );
void Serial1_FlushRead( void );

void Serial1_Print( const char* pString, ... );
void Serial1_PrintV( const char* pString, va_list arg );
void Serial1_PrintStr( char* pString );
void Serial1_PrintStr( char* pString );
void Serial1_PrintHex( const uint8_t* buff, uint16_t length, const char* separator );
void Serial1_Write( uint8_t* buff, uint16_t len );
void Serial1_FlushWrite( void );
bool_t Serial1_IsWriteFlushed( SerialTxCompCallback_t callback );

void PendSvProcess_Serial1_Write( void );
void PendSvProcess_UART_TxCplt( void );

#endif /* _SERIAL_H_ */
