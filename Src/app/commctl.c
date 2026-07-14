/*******************************************************************************
* commctl file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#include "usr_common.h"
#include "platform/pendsv.h"
#include "platform/serial.h"
#include "platform/timerctl.h"
#include "app/params.h"
#include "app/commctl.h"

/*******************************************************************************
********************************************************************************
* Private macros
********************************************************************************
*******************************************************************************/

typedef enum CommRxState_tag
{
    COMM_RX_STATE_NONE,
    COMM_RX_STATE_IDLE,
    COMM_RX_STATE_BINARY_LEN,
    COMM_RX_STATE_BINARY_DATA,
    COMM_RX_STATE_TEXT_WAIT_CR,
    COMM_RX_STATE_TEXT_WAIT_LF,
} CommRxState_t;

typedef enum
{
    RX_RESULT_COMPLETED,
    RX_RESULT_CONTINUE,
    RX_RESULT_DISCARD,
} CommRxResult_t;

/*******************************************************************************
********************************************************************************
* Private prototypes
********************************************************************************
*******************************************************************************/

static void InputCommTextLine_PSV( InputCommDataCallback callback, bool_t enableSpCmd );
static void InputCommBinaryBlock_PSV( InputCommDataCallback callback, bool_t enableSpCmd );
static void InputCommAbort_PSV( void );
static void SerialRxCallback( void );
static void SerialRxProcess_PSV( void );
static CommRxResult_t CommInput_rxByte( uint8_t nextByte );
static CommRxResult_t CommSpcmd_rxByte( uint8_t nextByte );
static void UartCheckTimerCallBack( UsrTimerId timerId );
static void UartCheckTimerCallBack_PSV( UsrTimerId timerId );

/*******************************************************************************
********************************************************************************
* Private memory declarations
********************************************************************************
*******************************************************************************/

static const char* g_commSpCmd[CommSpCmd_MAX] = {
    "config",
#ifdef BLD_USE_SPCMD_RESET
    "reset",
#endif
};

static CommRxState_t            g_commRxState = COMM_RX_STATE_NONE;
static InputCommDataCallback    g_rxCallback  = NULL;
static uint8_t                  g_bufferIdx;
static uint8_t                  g_nextByte;

static bool_t                   g_enableSpCmd = FALSE;
static uint8_t                  g_commSpCmdMatchCnt[CommSpCmd_MAX];
static uint8_t                  g_commSpCmdIdx;
static bool_t                   g_isCr;

/*******************************************************************************
********************************************************************************
* Public memory declarations
********************************************************************************
*******************************************************************************/

CommInputResult_t CommInputResult;
uint8_t CommDataBuffer[MAX_COMM_SIZE];
uint8_t CommDataLen;
CommSpCmd_t CommSpCmd = CommSpCmd_MAX;

/*******************************************************************************
********************************************************************************
* inline functions
********************************************************************************
*******************************************************************************/

__STATIC_INLINE void CommCtl_SpCmd_reset( void )
{
    memset( g_commSpCmdMatchCnt, 0, sizeof(g_commSpCmdMatchCnt) );
    g_commSpCmdIdx = 0;
    g_isCr = FALSE;
}

/*******************************************************************************
********************************************************************************
* Public functions
********************************************************************************
*******************************************************************************/

void CommCtl_Init( void )
{
    Serial1_Init();
    Serial1_rxStart();

    g_commRxState = COMM_RX_STATE_IDLE;
    g_rxCallback  = NULL;
}

struct
{
    InputCommDataCallback   callback;
    bool_t                  enableSpCmd;
} g_usrParams_InputCommTextLine;

void InputCommTextLine( InputCommDataCallback callback, bool_t enableSpCmd )
{
    g_usrParams_InputCommTextLine.callback    = callback;
    g_usrParams_InputCommTextLine.enableSpCmd = enableSpCmd;

    PendSV_SetEvent( PSV_EVENT_CALL_InputCommTextLine );
}

void PendSvProcess_InputCommTextLine( void )
{
    InputCommTextLine_PSV( g_usrParams_InputCommTextLine.callback,
                           g_usrParams_InputCommTextLine.enableSpCmd );
}

static void InputCommTextLine_PSV( InputCommDataCallback callback, bool_t enableSpCmd )
{
    // stop rx if running
    switch( g_commRxState )
    {
    case COMM_RX_STATE_TEXT_WAIT_CR:
    case COMM_RX_STATE_TEXT_WAIT_LF:
        // to do nothing
        break;

    case COMM_RX_STATE_BINARY_LEN:
    case COMM_RX_STATE_BINARY_DATA:
        // cancel input sequence
        Serial1_FlushRead();

        // break through

    case COMM_RX_STATE_IDLE:
        g_commRxState = COMM_RX_STATE_TEXT_WAIT_CR;
        g_rxCallback  = callback;
        CommDataLen   = 0;
        g_bufferIdx   = 0;
        g_enableSpCmd = enableSpCmd;
        if( enableSpCmd )
        {
            CommCtl_SpCmd_reset();
        }

        // start rx first byte
        if( Serial1_Read( &g_nextByte, 1, SerialRxCallback ) )
        {
            // rx complete
            SerialRxProcess_PSV();
        }
        break;

    default:
        break;
    }
}

struct
{
    InputCommDataCallback   callback;
    bool_t                  enableSpCmd;
} g_usrParams_InputCommBinaryBlock;

void InputCommBinaryBlock( InputCommDataCallback callback, bool_t enableSpCmd )
{
    g_usrParams_InputCommBinaryBlock.callback    = callback;
    g_usrParams_InputCommBinaryBlock.enableSpCmd = enableSpCmd;

    PendSV_SetEvent( PSV_EVENT_CALL_InputCommBinaryBlock );
}

void PendSvProcess_InputCommBinaryBlock( void )
{
    InputCommBinaryBlock_PSV( g_usrParams_InputCommBinaryBlock.callback,
                              g_usrParams_InputCommBinaryBlock.enableSpCmd );
}

static void InputCommBinaryBlock_PSV( InputCommDataCallback callback, bool_t enableSpCmd )
{
    // stop rx if running
    switch( g_commRxState )
    {
    case COMM_RX_STATE_TEXT_WAIT_CR:
    case COMM_RX_STATE_TEXT_WAIT_LF:
        // cancel input sequence
        Serial1_FlushRead();

        // break through

    case COMM_RX_STATE_IDLE:
        g_commRxState = COMM_RX_STATE_BINARY_LEN;
        g_rxCallback  = callback;
        CommDataLen   = 0;
        g_bufferIdx   = 0;
        g_enableSpCmd = enableSpCmd;
        if( enableSpCmd )
        {
            CommCtl_SpCmd_reset();
        }

        // start rx length field
        if( Serial1_Read( &g_nextByte, 1, SerialRxCallback ) )
        {
            // rx complete
            SerialRxProcess_PSV();
        }
        break;

    case COMM_RX_STATE_BINARY_LEN:
    case COMM_RX_STATE_BINARY_DATA:
        // to do nothing
        break;

    default:
        break;
    }
}

void InputCommAbort( void )
{
    PendSV_SetEvent( PSV_EVENT_CALL_InputCommAbort );
}

void PendSvProcess_InputCommAbort( void )
{
    InputCommAbort_PSV();
}

static void InputCommAbort_PSV( void )
{
    Serial1_rxStop();
    g_commRxState = COMM_RX_STATE_IDLE;
    if( g_enableSpCmd )
    {
        CommCtl_SpCmd_reset();
    }
    UsrTimer_stop( UsrUartCheckTimer );
}

#ifdef BLD_DEBUG_PRINT
void Debug_Print( char* pString )
{
    Serial1_Print( pString );
}
#endif

void Terminal_Print( char* pString, ... )
{
    va_list arg;

    if( TERMINAL == mTermParam.Mode )
    {
        va_start( arg, pString );
        Serial1_PrintV( pString, arg );
        va_end( arg );
    }
}

void Terminal_PrintHex( const uint8_t* buff, uint16_t length, const char* separator )
{
    if( TERMINAL == mTermParam.Mode )
    {
        Serial1_PrintHex( buff, length, separator );
    }
}

void Terminal_Write( uint8_t *data, uint8_t len )
{
    if( TERMINAL == mTermParam.Mode )
    {
        Serial1_Write( data, len );
    }
}

void Processor_Print( char* pString, ... )
{
    va_list arg;

    if( PROCESSOR == mTermParam.Mode )
    {
        va_start( arg, pString );
        Serial1_PrintV( pString, arg );
        va_end( arg );
    }
}

void Processor_Write( uint8_t *data, uint8_t len )
{
    if( PROCESSOR == mTermParam.Mode )
    {
        Serial1_Write( data, len );
    }
}

void Processor_Write1( uint8_t data )
{
    Processor_Write( &data, sizeof(data) );
}

void Processor_OutputOperationResult( char* pString )
{
    uint8_t len;

    if( PROCESSOR == mTermParam.Mode )
    {
        len = strlen( pString );

        if( FMT_ASCII == mTermParam.Format )
        {
            // write text
            Serial1_Write( (uint8_t*)pString, len );

            // CR + LF
            Serial1_Write( (uint8_t*)"\r\n", 2 );
        }
        else
        {
            // write length
            Serial1_Write( &len, 1 );

            // write text
            Serial1_Write( (uint8_t*)pString, len );
        }
    }
}

/*******************************************************************************
********************************************************************************
* Private functions
********************************************************************************
*******************************************************************************/

/*******************************************************************************
*
* SerialRxCallback
*
* Interface assumptions:
*
* Return value:
*     None
*
*******************************************************************************/
static void SerialRxCallback( void )
{
    PendSV_SetEvent( PSV_EVENT_SERIAL_RxDone );
}

void PendSvProcess_SERIAL_RxDone( void )
{
    SerialRxProcess_PSV();
}

static void SerialRxProcess_PSV( void )
{
    const SerialRxCompCallback_t rxCallback = g_rxCallback;

    bool_t complete = FALSE;

    for(;;)
    {
        WDG_Refresh();

        bool_t isContinue = FALSE;

        // input byte for special command interpret
        if( g_enableSpCmd )
        {
            switch( CommSpcmd_rxByte( g_nextByte ) )
            {
            case RX_RESULT_COMPLETED:
                // completed!
                CommInputResult = CommInputResult_spCmd;
                complete = TRUE;
                break;

            case RX_RESULT_CONTINUE:
                isContinue = TRUE;
                break;

            case RX_RESULT_DISCARD:
                break;
            }
            if( complete )
            {
                break;
            }
        }

        // input byte for command line interpret
        switch( CommInput_rxByte( g_nextByte ) )
        {
        case RX_RESULT_COMPLETED:
            // completed!
            CommInputResult = CommInputResult_buffer;
            complete = TRUE;
            break;

        case RX_RESULT_CONTINUE:
            isContinue = TRUE;
            break;

        case RX_RESULT_DISCARD:
            break;
        }
        if( complete )
        {
            break;
        }

        if( !isContinue )
        {
            break;
        }

        // read next byte
        if( !Serial1_Read( &g_nextByte, 1, SerialRxCallback ) )
        {
            break;
        }

        WDG_Refresh();
    }

    if( complete )
    {
        if( rxCallback )
        {
            rxCallback();
        }
    }
}

/*******************************************************************************
*
* CommInput_rxByte
*
* Interface assumptions:
*
* Return value:
*     is rx request completed
*
*******************************************************************************/
static CommRxResult_t CommInput_rxByte( uint8_t nextByte )
{
    switch( g_commRxState )
    {
    //------------------------
    // Binary mode sequence
    //------------------------

    // length field read done
    case COMM_RX_STATE_BINARY_LEN:
        // Data Length
        CommDataLen   = nextByte;

        // start read data field
        g_commRxState = COMM_RX_STATE_BINARY_DATA;
        g_bufferIdx   = 0;

        UsrTimer_start( UsrUartCheckTimer, UsrTimerMode_OneShot, UART_CHK_TIME, UartCheckTimerCallBack );

        return RX_RESULT_CONTINUE;

    // data field read done
    case COMM_RX_STATE_BINARY_DATA:
        // Input Byte
        CommDataBuffer[g_bufferIdx++] = nextByte;

        if( g_bufferIdx < CommDataLen )
        {
            return RX_RESULT_CONTINUE;
        }
        else
        {
            UsrTimer_stop( UsrUartCheckTimer );

            // finish
            g_commRxState = COMM_RX_STATE_IDLE;
            g_rxCallback  = NULL;

            //--------------
            // completed!!!
            //--------------
            return RX_RESULT_COMPLETED;
        }

    //------------------------
    // Text mode sequence
    //------------------------

    // wait for CR
    case COMM_RX_STATE_TEXT_WAIT_CR:
        switch( nextByte )
        {
        case '\b':    // Backspace
            if( g_bufferIdx > 0 )
            {
                g_bufferIdx--;
            }
            break;

        case '\r':    // CR
            // wait for LF
            g_commRxState = COMM_RX_STATE_TEXT_WAIT_LF;
            break;

        default:     // other
            if( g_bufferIdx < MAX_COMM_SIZE - 1 )
            {
                // accept
                CommDataBuffer[g_bufferIdx++] = nextByte;
            }
            else
            {
                // restart input sequence
                g_bufferIdx = 0;
            }
            break;
        }
        return RX_RESULT_CONTINUE;

    // wait for LF
    case COMM_RX_STATE_TEXT_WAIT_LF:
        switch( nextByte )
        {
        case '\b':    // Backspace
            g_bufferIdx   = 0;
            g_commRxState = COMM_RX_STATE_TEXT_WAIT_CR;
            return RX_RESULT_CONTINUE;

        case '\n':    // LF
            // finish
            CommDataBuffer[g_bufferIdx] = '\0';
            CommDataLen   = g_bufferIdx;
            g_commRxState = COMM_RX_STATE_IDLE;
            g_rxCallback  = NULL;

            //--------------
            // completed!!!
            //--------------
            return RX_RESULT_COMPLETED;

        default:    // other
            CommDataBuffer[0] = nextByte;
            g_bufferIdx   = 1;
            g_commRxState = COMM_RX_STATE_TEXT_WAIT_CR;
            return RX_RESULT_CONTINUE;
        }
        break;

    //------------------------
    // sequence already finished
    //------------------------
    case COMM_RX_STATE_IDLE:
        break;

    default:
        break;
    }

    return RX_RESULT_DISCARD;
}

/*******************************************************************************
*
* CommSpcmd_rxByte
*
* Interface assumptions:
*
* Return value:
*     is rx request completed
*
*******************************************************************************/
static CommRxResult_t CommSpcmd_rxByte( uint8_t nextByte )
{
    int i;

    switch( nextByte )
    {
    case '\r':
        g_isCr = TRUE;
        break;

    case '\n':
        if( g_isCr )
        {
            for( i = 0; i < CommSpCmd_MAX; i++ )
            {
                if( g_commSpCmdMatchCnt[i] == g_commSpCmdIdx && g_commSpCmd[i][g_commSpCmdIdx] == '\0' )
                {
                    CommSpCmd = (CommSpCmd_t)i;

                    CommDataBuffer[0] = '\0';
                    CommDataLen   = 0;
                    g_bufferIdx   = 0;
                    g_commRxState = COMM_RX_STATE_IDLE;
                    g_rxCallback  = NULL;

                    //--------------
                    // completed!!!
                    //--------------
                    return RX_RESULT_COMPLETED;
                }
            }
            CommCtl_SpCmd_reset();
        }
        break;

    case '\b':
        if( g_isCr )
        {
            g_isCr = FALSE;
        }
        else if( g_commSpCmdIdx > 0 )
        {
            g_commSpCmdIdx--;
        }
        break;

    default:
        g_isCr = FALSE;

        for( i = 0; i < CommSpCmd_MAX; i++ )
        {
            if( g_commSpCmdMatchCnt[i] == g_commSpCmdIdx &&
                g_commSpCmd[i][g_commSpCmdIdx] != '\0' &&
                g_commSpCmd[i][g_commSpCmdIdx] == g_nextByte )
            {
                g_commSpCmdMatchCnt[i]++;
            }
        }
        g_commSpCmdIdx++;
        break;
    }

    return RX_RESULT_CONTINUE;
}

/*******************************************************************************
*
* UartCheckTimerCallBack
*
* Interface assumptions:
*     param       timer ID
*
* Return value:
*     None
*
*******************************************************************************/

UsrTimerId g_usrParams_timerId;

static void UartCheckTimerCallBack( UsrTimerId timerId )
{
    g_usrParams_timerId = timerId;

    PendSV_SetEvent( PSV_EVENT_CALL_UartCheckTimerCallBack );
}

void PendSvProcess_UartCheckTimerCallBack( void )
{
    UartCheckTimerCallBack_PSV( g_usrParams_timerId );
}

static void UartCheckTimerCallBack_PSV( UsrTimerId timerId )
{
    // stop rx if running
    switch( g_commRxState )
    {
    //------------------------
    // Binary mode sequence
    //------------------------
    case COMM_RX_STATE_BINARY_LEN:
    case COMM_RX_STATE_BINARY_DATA:
        // restart binary input sequence
        Serial1_FlushRead();
        g_commRxState = COMM_RX_STATE_BINARY_LEN;
        CommDataLen   = 0;
        g_bufferIdx   = 0;
        CommCtl_SpCmd_reset();

        // start rx length field
        if( Serial1_Read( &g_nextByte, 1, SerialRxCallback ) )
        {
            // rx complete
            SerialRxProcess_PSV();
        }
        break;

    //------------------------
    // Text mode sequence
    //------------------------
    case COMM_RX_STATE_TEXT_WAIT_CR:
    case COMM_RX_STATE_TEXT_WAIT_LF:
        // restart text input sequence
        g_commRxState = COMM_RX_STATE_TEXT_WAIT_CR;
        CommDataLen   = 0;
        g_bufferIdx   = 0;
        CommCtl_SpCmd_reset();

        // start rx first byte
        if( Serial1_Read( &g_nextByte, 1, SerialRxCallback ) )
        {
            // rx complete
            SerialRxProcess_PSV();
        }
        break;

    //------------------------
    // sequence finished
    //------------------------
    case COMM_RX_STATE_IDLE:
        break;

    default:
        break;
    }
}
