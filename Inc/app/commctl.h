/*******************************************************************************
* commctl header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/
#ifndef _COMMCTL_H_
#define _COMMCTL_H_

#ifdef __cplusplus
    extern "C" {
#endif

/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/

#define MAX_COMM_SIZE       256

#define UART_CHK_TIME       500

/*******************************************************************************
********************************************************************************
* Public type definitions
********************************************************************************
*******************************************************************************/

// input result
typedef enum
{
    CommInputResult_buffer,
    CommInputResult_spCmd,
} CommInputResult_t;

// special command
typedef enum
{
    CommSpCmd_config,
#ifdef BLD_USE_SPCMD_RESET
    CommSpCmd_reset,
#endif
    CommSpCmd_MAX,
} CommSpCmd_t;

extern CommInputResult_t CommInputResult;
extern uint8_t CommDataBuffer[MAX_COMM_SIZE];
extern uint8_t CommDataLen;
extern CommSpCmd_t CommSpCmd;

typedef void (*InputCommDataCallback)(void);

/*******************************************************************************
********************************************************************************
* Public Prototypes
********************************************************************************
*******************************************************************************/

void CommCtl_Init( void );
void InputCommTextLine( InputCommDataCallback callback, bool_t enableSpCmd );
void InputCommBinaryBlock( InputCommDataCallback callback, bool_t enableSpCmd );
void InputCommAbort( void );

void PendSvProcess_InputCommTextLine( void );
void PendSvProcess_InputCommBinaryBlock( void );
void PendSvProcess_InputCommAbort( void );
void PendSvProcess_SERIAL_RxDone( void );
void PendSvProcess_UartCheckTimerCallBack( void );

#ifdef BLD_DEBUG_PRINT
    void Debug_Print( char* pString );
#else
    #define Debug_Print(...)
#endif

void Terminal_Print( char* pString, ... );
void Terminal_PrintHex( const uint8_t* buff, uint16_t length, const char* separator );
void Terminal_Write( uint8_t* data, uint8_t len );
void Processor_Print( char* pString, ... );
void Processor_Write( uint8_t* data, uint8_t len );
void Processor_Write1( uint8_t data );
void Processor_OutputOperationResult( char* pString );

#ifdef __cplusplus
}
#endif

/******************************************************************************/
#endif /* _COMMCTL_H_ */
