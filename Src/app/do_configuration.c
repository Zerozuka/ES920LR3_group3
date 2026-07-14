/*******************************************************************************
* do_configuration file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#include "usr_common.h"
#include "stack/smac.h"
#include "app/params.h"
#include "app/do_configuration.h"
#include "app/commctl.h"
#include "app/usr_util.h"

/*******************************************************************************
********************************************************************************
* Private macros
********************************************************************************
*******************************************************************************/
#define BIT(val)                (1 << (val))
#define CMD_ENABLE_IS_MATCH(enableMask, param)  ( ( (enableMask).protocol   & BIT( (param).Protocol )   ) && \
                                                  ( (enableMask).node       & BIT( (param).Node )       ) && \
                                                  ( (enableMask).sleep      & BIT( (param).Sleep )      ) )

// cmdEnableMask.node
#define NODE_COORDINATOR        BIT(COORDINATOR)
#define NODE_END_DEVICE         BIT(END_DEVICE)
#define NODE_ROUTER             BIT(ROUTER)
#define NODE_ALL                0xFF

// cmdEnableMask.sleep
#define SLEEP_NO_SLEEP          BIT(NO_SLEEP)
#define SLEEP_TIMER_WAKEUP      BIT(TIMER_WAKEUP)
#define SLEEP_INT_WAKEUP        BIT(INT_WAKEUP)
#define SLEEP_INT_WAKEUP_1TX    BIT(INT_WAKEUP_1TX)
#define SLEEP_UART_WAKEUP       BIT(UART_WAKEUP)
#define SLEEP_ENABLE            ~SLEEP_NO_SLEEP
#define SLEEP_ALL               0xFF

// cmdEnableMask.protocol
#if defined(BLD_USE_LORA_NR)
    #define PROT_LORA_NR        BIT(Protocol_LORA_NR)
#endif
#if defined(BLD_USE_LORA_R)
    #define PROT_LORA_R         BIT(Protocol_LORA_R)
#endif
#if defined(BLD_USE_FSK_R)
    #define PROT_FSK_R          BIT(Protocol_FSK_R)
#endif

#if defined(BLD_USE_LORA_NR) && defined(BLD_USE_LORA_R)
    #define PROT_LORA           (PROT_LORA_NR | PROT_LORA_R)
#elif defined(BLD_USE_LORA_NR) && !defined(BLD_USE_LORA_R)
    #define PROT_LORA           (PROT_LORA_NR)
#elif !defined(BLD_USE_LORA_NR) && defined(BLD_USE_LORA_R)
    #define PROT_LORA           (PROT_LORA_R)
#endif

#if defined(BLD_USE_FSK_R)
    #define PROT_FSK            (PROT_FSK_R)
#endif

#if defined(BLD_USE_LORA_R) && defined(BLD_USE_FSK_R)
    #define PROT_R              (PROT_LORA_R | PROT_FSK_R)
#elif defined(BLD_USE_LORA_R) && !defined(BLD_USE_FSK_R)
    #define PROT_R              (PROT_LORA_R)
#elif !defined(BLD_USE_LORA_R) && defined(BLD_USE_FSK_R)
    #define PROT_R              (PROT_FSK_R)
#endif

#if defined(BLD_USE_LORA_NR)
    #define PROT_NR             (PROT_LORA_NR)
#endif

    #define PROT_ALL            0xFF

/*******************************************************************************
********************************************************************************
* Private type definitions
********************************************************************************
*******************************************************************************/

typedef struct
{
    uint8_t protocol;
    uint8_t node;
    uint8_t sleep;
} CommandEnableMask_t;

typedef struct
{
    const char* sname;
    const char* name;
    bool_t (*func)(int, int);
    const char* description;
    bool_t isPublic;
    CommandEnableMask_t enableMask;
} CommandEntry_t;

/*******************************************************************************
********************************************************************************
* Private prototypes
********************************************************************************
*******************************************************************************/
static void WaitForInputCommLine( void );
static uint8_t SelectMode( void );
static void PrintMessage( const char* const pu8Menu[] );
static void TerminalPrintMessage( const char* const pu8Menu[] );
static void CommShowModeSel( void );
static void CommShowPrompt( void );
static void CommShowHelp( void );

static bool_t CommExecSelectNode( int, int );
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
static bool_t CommExecSelectBandWidth( int, int );
static bool_t CommExecSetSpreadingFactor( int, int );
#endif
#if defined(BLD_USE_FSK_R)
static bool_t CommExecSelectRate( int, int );
#endif
static bool_t CommExecSetChannel( int, int );
static bool_t CommExecSetPanId( int, int );
static bool_t CommExecSetSourceId( int, int );
static bool_t CommExecSetDestinationId( int, int );
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
static bool_t CommExecSetHopCount( int, int );
static bool_t CommExecSetEndId( int, int );
static bool_t CommExecSetRoute1( int, int );
static bool_t CommExecSetRoute2( int, int );
#if defined(BLD_USE_FSK_R)
static bool_t CommExecSetRoute3( int, int );
#endif
static bool_t CommExecSetRoute( int, int, int );
#endif
static bool_t CommExecSetAck( int, int );
static bool_t CommExecSetRetry( int, int );
static bool_t CommExecSelectTransMode( int, int );
static bool_t CommExecSelectRcvId( int, int );
static bool_t CommExecSelectRssi( int, int );
static bool_t CommExecSelectOperation( int, int );
static bool_t CommExecSelectBaudrate( int, int );
static bool_t CommExecSelectSleep( int, int );
static bool_t CommExecSelectMcuLpMode( int, int );
static bool_t CommExecSelectRfLpMode( int, int );
static bool_t CommExecSelectProtocol( int, int );
static bool_t CommExecSetRxBoost( int, int );
static bool_t CommExecSetSleepTime( int, int );
static bool_t CommExecSetPower( int, int );
static bool_t CommExecSetFormat( int, int );
static bool_t CommExecSetSendTime( int, int );
static bool_t CommExecSetSendData( int, int );
static bool_t CommExecSetAesKey( int, int );
static bool_t CommExecSelectRfMode( int, int );
static bool_t CommExecSetBackoff( int, int );
static bool_t CommExecVersion( int, int );
static bool_t CommExecSaveParameter( int, int );
static bool_t CommExecLoadParameter( int, int );
static bool_t CommExecShow( int, int );
static bool_t CommExecStart( int, int );
static bool_t CommExecHelp( int, int );
static bool_t CommExecPortCheck( int, int );

static bool_t InputArgumentUint8( int nameLen, uint8_t* param, int width );
static bool_t InputArgumentUint32( int nameLen, uint32_t* param, int width );
static bool_t InputArgumentInt32( int nameLen, int32_t* param, int width );
static bool_t InputArgumentHex16( int nameLen, uint16_t* param, int width );
static bool_t InputArgumentHex( int nameLen, uint8_t* dest, uint8_t length );
static void InputArgumentStr( int nameLen, uint8_t* dest, uint8_t length );

static bool_t ModifyNetworkSetting_protocol( uint8_t protocol, uint8_t ch, uint8_t hopCnt );
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
static bool_t ModifyNetworkSetting_hopCnt( uint8_t val );
static bool_t ModifyNetworkSetting_route( uint16_t val, uint8_t idx );
static bool_t ModifyNetworkSetting_endId( uint16_t val );
#endif
static bool_t ModifyNetworkSetting_panId( uint16_t val );
static bool_t ModifyNetworkSetting_srcId( uint16_t val );
static bool_t ModifyNetworkSetting_dstId( uint16_t val );
static bool_t ModifyNetworkSetting_ack( bool_t val );
static bool_t ModifyNetworkSetting_retry( uint8_t val );
static bool_t ModifyNetworkSetting_power( uint8_t val );
static bool_t ModifyNetworkSetting_rxBoost( bool_t val );
static bool_t ModifyNetworkSetting_channel( uint8_t val );
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
static bool_t ModifyNetworkSetting_bw( uint8_t bw, uint8_t ch );
static bool_t ModifyNetworkSetting_sf( uint8_t val );
#endif
#if defined(BLD_USE_FSK_R)
static bool_t ModifyNetworkSetting_rate( uint8_t val, uint8_t ch );
#endif
static bool_t ModifyNetworkSetting_aesKey( const uint8_t aesKey[SMAC_ENCRYPT_KEY_LENGTH] );
static void CommRxCallBack( void );

/*******************************************************************************
********************************************************************************
* Private memory declarations
********************************************************************************
*******************************************************************************/
static const char* cu8ModeSel = { "Select Mode [1.terminal or 2.processor]" };

#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
static const char* cu8Prompt_LORA  = { "LORA > " };
#endif
#if defined(BLD_USE_FSK_R)
static const char* cu8Prompt_FSK  = { "FSK > " };
#endif

static const char* cu8Version = { SOFTWARE_VERSION };

static const char* const cu8Logo[] = {
    "\r\n",
    "\r\n",
    "\r Software Version : ",
    SOFTWARE_VERSION,
    "\r\n",
    NULL
};

static const char* const cu8MainMenu[] = {
    "\r\n\r Configuration Mode\n",
    "\r------------------------------------------\n",
    NULL
};

static const char* const cuOnOffMenu[] = {
    "\r\n  1. ON",
    "\r\n  2. OFF",
    "\r\n",
    "\r\n  select number > ",
    NULL
};

static const char* const cuBaudrateMenu[] = {
    "\r\n  1. 9600",
    "\r\n  2. 19200",
    "\r\n  3. 38400",
    "\r\n  4. 57600",
    "\r\n  5. 115200",
    "\r\n  6. 230400",
    "\r\n",
    "\r\n  select number > ",
    NULL
};

#if defined(BLD_USE_LORA_NR)
static const char* const cuNodeMenu_NR[] = {
    "\r\n  1. Coordinator",
    "\r\n  2. EndDevice",
    "\r\n",
    "\r\n  select number > ",
    NULL
};
#endif

#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
static const char* const cuNodeMenu_R[] = {
    "\r\n  1. Coordinator",
    "\r\n  2. EndDevice",
    "\r\n  3. Router",
    "\r\n",
    "\r\n  select number > ",
    NULL
};
#endif

static const char* const cuOperationMenu[] = {
    "\r\n  1. Configuration",
    "\r\n  2. Operation",
    "\r\n",
    "\r\n  select number > ",
    NULL
};

#if defined(BLD_USE_FSK_R)
static const char* const cuRateMenu[] = {
    "\r\n  1. 50kbps",
    "\r\n  2. 100kbps",
    "\r\n  3. 150kbps",
    "\r\n",
    "\r\n  select number > ",
    NULL
};
#endif

#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
static const char* const cuBwMenu[] = {
#if 0
    "\r\n  1. 31.25kHz",
    "\r\n  2. 41.7kHz",
#endif
    "\r\n  3. 62.5kHz",
    "\r\n  4. 125kHz",
    "\r\n  5. 250kHz",
    "\r\n",
    "\r\n  select number > ",
    NULL
};
#endif

static const char* const cuSleepMenu[] = {
    "\r\n  1. No Sleep",
    "\r\n  2. Timer Wakeup",
    "\r\n  3. INT Wakeup (Tx continue)",
    "\r\n  4. INT Wakeup (One time Tx)",
    "\r\n  5. UART Wakeup",
    "\r\n",
    "\r\n  select number > ",
    NULL
};

static const char* const cuMcuLpModeMenu[] = {
    "\r\n  1. STOP Mode (lowest power)",
    "\r\n  2. SLEEP Mode",
    "\r\n",
    "\r\n  select number > ",
    NULL
};

static const char* const cuRfLpModeMenu[] = {
    "\r\n  1. Sleep with Cold Start (lowest power)",
    "\r\n  2. Sleep with Warm Start (fast wakeup)",
    "\r\n  3. Active",
    "\r\n",
    "\r\n  select number > ",
    NULL
};

static const char* const cuProtocolTypeMenu[] = {
#if defined(BLD_USE_LORA_NR)
    "\r\n  1. Private LoRa (ES920LR compatible)",
#endif
#if defined(BLD_USE_LORA_R)
    "\r\n  2. Private LoRa with Static Routing",
#endif
#if defined(BLD_USE_FSK_R)
    "\r\n  3. FSK with Static Routing",
#endif
    "\r\n",
    "\r\n  select number > ",
    NULL
};

static const char* const cuTransMenu[] = {
    "\r\n  1. Payload",
    "\r\n  2. Frame",
    "\r\n",
    "\r\n  select number > ",
    NULL
};

static const char* const cuFormatMenu[] = {
    "\r\n  1. ASCII",
    "\r\n  2. BINARY",
    "\r\n",
    "\r\n  select number > ",
    NULL
};

static const char* const cuRfModeMenu[] = {
    "\r\n  1. TxRx",
    "\r\n  2. Tx Only",
#if defined(BLD_ENABLE_RFMODE_BURST)
    "\r\n  3. Tx Burst (for testing)",
#endif
#if defined(BLD_ENABLE_RFMODE_CW)
    "\r\n  4. Tx Continuous Wave (for testing)",
#endif
#if defined(BLD_ENABLE_RFMODE_TX_INFINITE)
    "\r\n  5. Tx Infinite Preamble (for testing)",
#endif
#if defined(BLD_ENABLE_RFMODE_RSSI_CHECK)
    "\r\n  6. RSSI Check (for testing)",
#endif
    "\r\n",
    "\r\n  select number > ",
    NULL
};

static const CommandEntry_t command1[] = {

#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    { "a"   , "node"        , CommExecSelectNode            , "select Coordinator or EndDevice or Router"   , TRUE, { PROT_R, NODE_ALL, SLEEP_ALL, } },
#endif

#if defined(BLD_USE_LORA_NR)
    { "a"   , "node"        , CommExecSelectNode            , "select Coordinator or EndDevice"             , TRUE, { PROT_LORA_NR, NODE_ALL, SLEEP_ALL, } },
#endif

#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
    { "b"   , "bw"          , CommExecSelectBandWidth       , "select Band Width"                           , TRUE, { PROT_LORA, NODE_ALL, SLEEP_ALL, } },
    { "c"   , "sf"          , CommExecSetSpreadingFactor    , "set Spreading Factor"                        , TRUE, { PROT_LORA, NODE_ALL, SLEEP_ALL, } },
    { "d"   , "channel"     , CommExecSetChannel            , "set channel"                                 , TRUE, { PROT_LORA, NODE_ALL, SLEEP_ALL, } },
    { "e"   , "panid"       , CommExecSetPanId              , "set PAN ID"                                  , TRUE, { PROT_LORA, NODE_ALL, SLEEP_ALL, } },
    { "f"   , "ownid"       , CommExecSetSourceId           , "set Own Node ID"                             , TRUE, { PROT_LORA, NODE_ALL, SLEEP_ALL, } },
    { "g"   , "dstid"       , CommExecSetDestinationId      , "set Destination ID"                          , TRUE, { PROT_LORA, NODE_ALL, SLEEP_ALL, } },
#endif

#if defined(BLD_USE_LORA_R)
    { "h"   , "hopcount"    , CommExecSetHopCount           , "set Hop Count"                               , TRUE, { PROT_LORA_R, NODE_ALL, SLEEP_ALL, } },
    { "i"   , "endid"       , CommExecSetEndId              , "set End ID"                                  , TRUE, { PROT_LORA_R, NODE_ALL, SLEEP_ALL, } },
    { "j"   , "route1"      , CommExecSetRoute1             , "set 1st Router ID"                           , TRUE, { PROT_LORA_R, NODE_ALL, SLEEP_ALL, } },
    { "k"   , "route2"      , CommExecSetRoute2             , "set 2nd Router ID"                           , TRUE, { PROT_LORA_R, NODE_ALL, SLEEP_ALL, } },
#endif

#if defined(BLD_USE_FSK_R)
    { "b"   , "channel"     , CommExecSetChannel            , "set channel"                                 , TRUE, { PROT_FSK, NODE_ALL, SLEEP_ALL, } },
    { "c"   , "panid"       , CommExecSetPanId              , "set PAN ID"                                  , TRUE, { PROT_FSK, NODE_ALL, SLEEP_ALL, } },
    { "d"   , "ownid"       , CommExecSetSourceId           , "set Own Node ID"                             , TRUE, { PROT_FSK, NODE_ALL, SLEEP_ALL, } },
    { "e"   , "dstid"       , CommExecSetDestinationId      , "set Destination ID"                          , TRUE, { PROT_FSK, NODE_ALL, SLEEP_ALL, } },
    { "f"   , "hopcount"    , CommExecSetHopCount           , "set Hop Count"                               , TRUE, { PROT_FSK, NODE_ALL, SLEEP_ALL, } },
    { "g"   , "endid"       , CommExecSetEndId              , "set End ID"                                  , TRUE, { PROT_FSK, NODE_ALL, SLEEP_ALL, } },
    { "h"   , "route1"      , CommExecSetRoute1             , "set 1st Router ID"                           , TRUE, { PROT_FSK, NODE_ALL, SLEEP_ALL, } },
    { "i"   , "route2"      , CommExecSetRoute2             , "set 2nd Router ID"                           , TRUE, { PROT_FSK, NODE_ALL, SLEEP_ALL, } },
    { "j"   , "route3"      , CommExecSetRoute3             , "set 3rd Router ID"                           , TRUE, { PROT_FSK, NODE_ALL, SLEEP_ALL, } },
    { "k"   , "rate"        , CommExecSelectRate            , "select data rate"                            , TRUE, { PROT_FSK, NODE_ALL, SLEEP_ALL, } },
#endif

    { "l"   , "ack"         , CommExecSetAck                , "set Acknowledge Mode"                        , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "m"   , "retry"       , CommExecSetRetry              , "set send retry count"                        , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "n"   , "transmode"   , CommExecSelectTransMode       , "select Transfer Mode"                        , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "o"   , "rcvid"       , CommExecSelectRcvId           , "set received Node ID information"            , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "p"   , "rssi"        , CommExecSelectRssi            , "set RSSI information"                        , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "q"   , "operation"   , CommExecSelectOperation       , "select Configuration or Operation"           , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "r"   , "baudrate"    , CommExecSelectBaudrate        , "select UART baudrate"                        , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "s"   , "sleep"       , CommExecSelectSleep           , "select Sleep Mode"                           , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "t"   , "sleeptime"   , CommExecSetSleepTime          , "set Sleep Wakeup Timer value"                , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "u"   , "power"       , CommExecSetPower              , "set Output Power"                            , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "v"   , "version"     , CommExecVersion               , "software version"                            , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "w"   , "save"        , CommExecSaveParameter         , "save parameters"                             , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "x"   , "load"        , CommExecLoadParameter         , "load default parameters"                     , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "y"   , "show"        , CommExecShow                  , "show parameters"                             , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "z"   , "start"       , CommExecStart                 , "transite to Operation Mode"                  , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "A"   , "format"      , CommExecSetFormat             , "set Data Format"                             , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "B"   , "sendtime"    , CommExecSetSendTime           , "set test send interval"                      , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "C"   , "senddata"    , CommExecSetSendData           , "set test send data"                          , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "D"   , "aeskey"      , CommExecSetAesKey             , "set AES key"                                 , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "E"   , "rfmode"      , CommExecSelectRfMode          , "set RF Mode"                                 , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "F"   , "mculpmode"   , CommExecSelectMcuLpMode       , "select MCU Low Power Mode"                   , TRUE, { PROT_ALL, NODE_END_DEVICE, SLEEP_ENABLE, } },
    { "G"   , "rflpmode"    , CommExecSelectRfLpMode        , "select RF Low Power Mode"                    , TRUE, { PROT_ALL, NODE_END_DEVICE, SLEEP_ENABLE, } },
    { "H"   , "protocol"    , CommExecSelectProtocol        , "select Protocol Type"                        , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "I"   , "rxboost"     , CommExecSetRxBoost            , "set Rx Boosted Mode"                         , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "J"   , "backoff"     , CommExecSetBackoff         	, "set send retry backoff time"                 , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { "?"   , "help"        , CommExecHelp                  , "help"                                        , TRUE, { PROT_ALL, NODE_ALL, SLEEP_ALL, } },
    { NULL  , "@portcheck"  , CommExecPortCheck             , "port check"                                  , FALSE,{ PROT_ALL, NODE_ALL, SLEEP_ALL, } },

    { NULL  , NULL          , 0                             }
};

static bool_t g_evDataFromUART = FALSE;

/*******************************************************************************
********************************************************************************
* Public functions
********************************************************************************
*******************************************************************************/

/*******************************************************************************
* DoConfiguration
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
void DoConfiguration( void )
{
    int32_t i;
    uint8_t mode;
    uint8_t tmpcmd[16] = { 0 };
    bool_t isEnd = FALSE;

    mode = SelectMode();
    if( TERMINAL == mode )
    {
        PrintMessage( cu8Logo );
        CommShowHelp();
    }

    while( !isEnd )
    {
        CommShowPrompt();

        WaitForInputCommLine();

        for( i = 0; i < 15; i++ )
        {
            if( ('\0' == CommDataBuffer[i]) || (' ' == CommDataBuffer[i]) )
            {
                tmpcmd[i] = '\0';
                break;
            }
            else
            {
                tmpcmd[i] = CommDataBuffer[i];
            }
        }

        for( i = 0; command1[i].func != 0; i++ )
        {
            if( CMD_ENABLE_IS_MATCH(command1[i].enableMask, mTermParam) )
            {
                if( command1[i].name && !strcmp(command1[i].name, (char*)tmpcmd) )
                {
                    isEnd = command1[i].func( i, strlen(command1[i].name) );
                    break;
                }

                if( command1[i].sname && !strcmp(command1[i].sname, (char*)tmpcmd) )
                {
                    isEnd = command1[i].func( i, strlen(command1[i].sname) );
                    break;
                }
            }
        }

        if( NULL == command1[i].func )
        {
            Processor_Print( "NG 001\r\n" );
        }
    }
}

/*******************************************************************************
********************************************************************************
* private functions
********************************************************************************
*******************************************************************************/

/*******************************************************************************
* WaitForInputCommLine
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
static void WaitForInputCommLine( void )
{
    g_evDataFromUART = FALSE;

    // input text from uart
    InputCommTextLine( CommRxCallBack, FALSE );

    // wait for complete
    while( !g_evDataFromUART )
    {
        WDG_Refresh();
    }
}

/*******************************************************************************
* CommRxCallBack
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
static void CommRxCallBack( void )
{
    g_evDataFromUART = TRUE;
}

/*******************************************************************************
* SelectMode
*
* Interface assumptions:
*     None
*
* Return value:
*     Mode      Terminal or Processor
*
*******************************************************************************/
static uint8_t SelectMode( void )
{
    int     ret;

    CommShowModeSel();

    /* wait terminal or processor */
    while( 1 )
    {
        WaitForInputCommLine();

        ret = strcmp( "terminal", (char*)CommDataBuffer );
        if( 0 == ret )
        {
            Serial1_Print( "OK\r\n" );
            mTermParam.Mode = TERMINAL;
            break;
        }

        ret = strcmp( "processor", (char*)CommDataBuffer );
        if( 0 == ret )
        {
            Serial1_Print( "OK\r\n" );
            mTermParam.Mode = PROCESSOR;
            break;
        }

        ret = strcmp( "1", (char*)CommDataBuffer );
        if( 0 == ret )
        {
            Serial1_Print( "OK\r\n" );
            mTermParam.Mode = TERMINAL;
            break;
        }

        ret = strcmp( "2", (char*)CommDataBuffer );
        if( 0 == ret )
        {
            Serial1_Print( "OK\r\n" );
            mTermParam.Mode = PROCESSOR;
            break;
        }

        Serial1_Print( "NG 002\r\n" );
    }

    return( mTermParam.Mode );
}

/*******************************************************************************
* PrintMessage
*
* Interface assumptions:
*     pu8Menu       print message
*
* Return value:
*     None
*
*******************************************************************************/
static void PrintMessage( const char* const pu8Menu[] )
{
    uint8_t u8Index = 0;

    while( pu8Menu[u8Index] )
    {
        WDG_Refresh();
        Serial1_Print( pu8Menu[u8Index] );
        u8Index++;
    }
}

/*******************************************************************************
* TerminalPrintMessage
*
* Interface assumptions:
*     pu8Menu       print message
*
* Return value:
*     None
*
*******************************************************************************/
static void TerminalPrintMessage( const char* const pu8Menu[] )
{
    if( TERMINAL == mTermParam.Mode )
    {
        PrintMessage( pu8Menu );
    }
}

/*******************************************************************************
* CommShowModeSel
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
static void CommShowModeSel( void )
{
    Serial1_Print( "\r\n" );
    Serial1_Print( (char*)cu8ModeSel );
    Serial1_Print( "\r\n" );
}

/*******************************************************************************
* CommShowPrompt
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
static void CommShowPrompt( void )
{
    /* Erase old line */
    if( TERMINAL == mTermParam.Mode )
    {
        Serial1_Print( "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b" );
        Serial1_Print( "                    " );
        Serial1_Print( "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b" );

        switch( mTermParam.Protocol )
        {
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
        __cases_Protocol_LORA
            Serial1_Print( (char*)cu8Prompt_LORA );
            break;
#endif
#if defined(BLD_USE_FSK_R)
        __cases_Protocol_FSK
            Serial1_Print( (char*)cu8Prompt_FSK );
            break;
#endif
        default:
            break;
        }
    }
}

/*******************************************************************************
* CommShowHelp
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
static void CommShowHelp( void )
{
    PrintMessage( cu8MainMenu );

    for( int i = 0; command1[i].func != 0; i++ )
    {
        if( command1[i].isPublic && CMD_ENABLE_IS_MATCH( command1[i].enableMask, mTermParam ) )
        {
            Serial1_Print("\r %s. %-12s%s\n", command1[i].sname, command1[i].name, command1[i].description);
        }
        WDG_Refresh();
    }

    Serial1_Print("\r\n");
}

/*******************************************************************************
* CommExecSelectNode
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSelectNode( int index, int nameLen )
{
    uint8_t tmpMode = 0;

    switch( mTermParam.Protocol )
    {
#if defined(BLD_USE_LORA_NR)
    __cases_Protocol_NR
        TerminalPrintMessage( cuNodeMenu_NR );
        break;
#endif

#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    __cases_Protocol_R
        TerminalPrintMessage( cuNodeMenu_R );
        break;
#endif

    default:
        break;
    }

    // input parameter
    if( !InputArgumentUint8(nameLen, &tmpMode, 1) )
    {
        Terminal_Print( "\r\n  selected number is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    // check and change parameter
    else
    {
        switch( tmpMode )
        {
        case COORDINATOR:
            mTermParam.Node = COORDINATOR;
            Terminal_Print( "\r\n  Coordinator is selected." );
            Processor_Print( "OK\r\n" );
            break;

        case END_DEVICE:
            mTermParam.Node = END_DEVICE;
            Terminal_Print( "\r\n  EndDevice is selected." );
            Processor_Print( "OK\r\n" );
            break;

#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
        case ROUTER:
            switch( mTermParam.Protocol )
            {
            __cases_Protocol_R
                mTermParam.Node = ROUTER;
                Terminal_Print( "\r\n  Router is selected." );
                Processor_Print( "OK\r\n" );
                break;

            default:
                Terminal_Print( "\r\n  selected number is invalid." );
                Processor_Print( "NG 002\r\n" );
                break;
            }
            break;
#endif

        default:
            Terminal_Print( "\r\n  selected number is invalid." );
            Processor_Print( "NG 002\r\n" );
            break;
        }
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

#ifdef BLD_USE_FSK_R
/*******************************************************************************
* ConvMenuId_rate
*
* Interface assumptions:
*     menuId        menu id
*     dr            data rate
*
* Return value:
*     is parameter valid
*
*******************************************************************************/
static bool_t ConvMenuId_rate( uint8_t menuId, uint8_t* dr )
{
    switch( menuId )
    {
    case 1:     *dr = RATE_50KBPS; break;
    case 2:     *dr = RATE_100KBPS; break;
    case 3:     *dr = RATE_150KBPS; break;
    default:    return FALSE;
    }
    return TRUE;
}

/*******************************************************************************
* CommExecSelectRate
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSelectRate( int index, int nameLen )
{
    uint8_t tmpId = 0;
    uint8_t tmpRate = 0;
    uint8_t tmpCh = 0;

    TerminalPrintMessage( cuRateMenu );

    // input parameter
    if( !InputArgumentUint8(nameLen, &tmpId, 1) || !ConvMenuId_rate(tmpId, &tmpRate) )
    {
        Terminal_Print( "\r\n  selected number is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    // check and change parameter
    else
    {
        // adjust channel
        tmpCh = mTermParam.Channel;
        if( MAX_CH_FSK(tmpRate) < tmpCh )
        {
            tmpCh = 1;
        }

        if( !ModifyNetworkSetting_rate(tmpRate, tmpCh) )
        {
            Terminal_Print( "\r\n  selected number is invalid." );
            Processor_Print( "NG 002\r\n" );
        }
        else
        {
            mTermParam.Rate    = tmpRate;
            mTermParam.Channel = tmpCh;
            switch( tmpRate )
            {
            case RATE_50KBPS:
                Terminal_Print( "\r\n  50kbps is selected.");
                break;

            case RATE_100KBPS:
                Terminal_Print( "\r\n  100kbps is selected.");
                break;

            case RATE_150KBPS:
                Terminal_Print( "\r\n  150kbps is selected.");
                break;
            }
            Processor_Print( "OK\r\n" );
        }
    }

    Terminal_Print( "\r\n\r\n");

    return FALSE;
}
#endif

#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
/*******************************************************************************
* ConvMenuId_bw
*
* Interface assumptions:
*     menuId        menu id
*     bw            band width
*
* Return value:
*     is parameter valid
*
*******************************************************************************/
static bool_t ConvMenuId_bw( uint8_t menuId, uint8_t* bw )
{
    switch(menuId)
    {
#if 0
    case 1:     *bw = BANDWIDTH31_25; break;
    case 2:     *bw = BANDWIDTH41_7; break;
#endif
    case 3:     *bw = BANDWIDTH62_5; break;
    case 4:     *bw = BANDWIDTH125; break;
    case 5:     *bw = BANDWIDTH250; break;
    default:    return FALSE;
    }
    return TRUE;
}

/*******************************************************************************
* CommExecSelectBandWidth
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSelectBandWidth( int index, int nameLen )
{
    uint8_t tmpId = 0;
    uint8_t tmpBw = 0;
    uint8_t tmpCh = 0;

    TerminalPrintMessage( cuBwMenu );

    // input parameter
    if( !InputArgumentUint8(nameLen, &tmpId, 1) || !ConvMenuId_bw(tmpId, &tmpBw) )
    {
        Terminal_Print( "\r\n  selected number is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    // check and change parameter
    else
    {
        // adjust channel
        tmpCh = mTermParam.Channel;
        if( MAX_CH_LORA(tmpBw) < tmpCh )
        {
            tmpCh = 1;
        }

        if( !ModifyNetworkSetting_bw(tmpBw, tmpCh) )
        {
            Terminal_Print( "\r\n  selected number is invalid." );
            Processor_Print( "NG 002\r\n" );
        }
        else
        {
            mTermParam.Bw      = tmpBw;
            mTermParam.Channel = tmpCh;
            switch( tmpBw )
            {
#if 0
            case BANDWIDTH31_25:
                Terminal_Print( "\r\n  31.25kHz is selected." );
                break;

            case BANDWIDTH41_7:
                Terminal_Print( "\r\n  41.7kHz is selected." );
                break;
#endif
            case BANDWIDTH62_5:
                Terminal_Print( "\r\n  62.5kHz is selected." );
                break;

            case BANDWIDTH125:
                Terminal_Print( "\r\n  125kHz is selected." );
                break;

            case BANDWIDTH250:
                Terminal_Print( "\r\n  250kHz is selected." );
                break;
            }
            Processor_Print( "OK\r\n" );

            uint32_t dataRate = 0;
            SMAC_CalcDataRate( mTermParam.Bw, mTermParam.Sf, &dataRate );
            Terminal_Print( "  Effective Bitrate is %ubps", (unsigned)dataRate );
        }
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSetSpreadingFactor
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSetSpreadingFactor( int index, int nameLen )
{
    uint8_t     tmpSf = 0;

    Terminal_Print( "\r\n  please set Spreading Factor (5 - 12) > " );

    // input and check and change parameter
    if( !InputArgumentUint8(nameLen, &tmpSf, 2) || !ModifyNetworkSetting_sf(tmpSf) )
    {
        Terminal_Print( "\r\n  spreading factor is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    else
    {
        mTermParam.Sf = tmpSf;

        Terminal_Print( "\r\n  spreading factor is %u", (unsigned)mTermParam.Sf );
        Processor_Print( "OK\r\n" );

        uint32_t dataRate = 0;
        SMAC_CalcDataRate( mTermParam.Bw, mTermParam.Sf, &dataRate );
        Terminal_Print( "  Effective Bitrate is %ubps.", (unsigned)dataRate );
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}
#endif

/*******************************************************************************
* CommExecSetChannel
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSetChannel( int index, int nameLen )
{
    uint8_t tmpChannel = 0;

    switch( mTermParam.Protocol )
    {
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
    __cases_Protocol_LORA
        Terminal_Print( "\r\n  please set channel (1 - %u) > ", (unsigned)MAX_CH_LORA(mTermParam.Bw) );
        break;
#endif
#if defined(BLD_USE_FSK_R)
    __cases_Protocol_FSK
        Terminal_Print( "\r\n  please set channel (1 - %u) > ", (unsigned)MAX_CH_FSK(mTermParam.Rate) );
        break;
#endif
    }

    // input and check and change parameter
    if( !InputArgumentUint8(nameLen, &tmpChannel, 2) || !ModifyNetworkSetting_channel(tmpChannel) )
    {
        Terminal_Print( "\r\n  channel is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    else
    {
        mTermParam.Channel = tmpChannel;

        Terminal_Print( "\r\n  channel is %u", (unsigned)mTermParam.Channel );
        Processor_Print( "OK\r\n" );
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSetPanId
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSetPanId( int index, int nameLen )
{
    uint16_t    tmpId = 0;

    Terminal_Print( "\r\n  please set PAN ID (0001 - FFFE) > " );

    // input and check and change parameter
    if( !InputArgumentHex16(nameLen, &tmpId, 4) || !ModifyNetworkSetting_panId(tmpId) )
    {
        Terminal_Print( "\r\n  PAN ID is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    else
    {
        mTermParam.PanId = tmpId;

        Terminal_Print( "\r\n  PAN ID is 0x%04X", (unsigned)mTermParam.PanId );
        Processor_Print( "OK\r\n" );
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSetSourceId
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSetSourceId( int index, int nameLen )
{
    uint16_t    tmpId = 0;

    Terminal_Print( "\r\n  please set Own Node ID (0000 - FFFE) > " );

    // input and check and change parameter
    if( !InputArgumentHex16(nameLen, &tmpId, 4) || !ModifyNetworkSetting_srcId(tmpId) )
    {
        Terminal_Print( "\r\n  Own Node ID is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    else
    {
        mTermParam.SrcId = tmpId;

        Terminal_Print( "\r\n  Own Node ID is 0x%04X", (unsigned)mTermParam.SrcId );
        Processor_Print( "OK\r\n" );
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSetDestinationId
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSetDestinationId( int index, int nameLen )
{
    uint16_t    tmpId = 0;

    Terminal_Print( "\r\n  please set Destination ID (0000 - FFFF) > " );

    // input and check and change parameter
    if( !InputArgumentHex16(nameLen, &tmpId, 4) || !ModifyNetworkSetting_dstId(tmpId) )
    {
        Terminal_Print( "\r\n  Destination ID is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    else
    {
        mTermParam.DstId = tmpId;

        Terminal_Print( "\r\n  Destination ID is 0x%04X", (unsigned)mTermParam.DstId );
        Processor_Print( "OK\r\n" );
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
/*******************************************************************************
* CommExecSetHopCount
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSetHopCount( int index, int nameLen )
{
    uint8_t tmpCount = 0;
    const uint8_t maxHopCnt = MAX_HOP_CNT( (protocolType_t)mTermParam.Protocol );

    Terminal_Print( "\r\n  please set hop count (1 - %u) > ", (unsigned)maxHopCnt );

    // input and check and change parameter
    if( !InputArgumentUint8(nameLen, &tmpCount, 1) || !ModifyNetworkSetting_hopCnt(tmpCount) )
    {
        Terminal_Print( "\r\n  hop count is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    else
    {
        mTermParam.HopCnt = tmpCount;

        Terminal_Print( "\r\n  hop count is %u", (unsigned)mTermParam.HopCnt );
        Processor_Print( "OK\r\n" );
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSetEndId
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSetEndId( int index, int nameLen )
{
    uint16_t    tmpId = 0;

    Terminal_Print( "\r\n  please set End ID (0000 - FFFE) > " );

    // input and check and change parameter
    if( !InputArgumentHex16(nameLen, &tmpId, 4) || !ModifyNetworkSetting_endId(tmpId) )
    {
        Terminal_Print( "\r\n  End ID is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    else
    {
        mTermParam.EndId = tmpId;

        Terminal_Print( "\r\n  End ID is 0x%04X", (unsigned) mTermParam.EndId );
        Processor_Print( "OK\r\n" );
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSetRoute1
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSetRoute1( int index, int nameLen )
{
    return CommExecSetRoute( index, 0, nameLen );
}

/*******************************************************************************
* CommExecSetRoute2
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSetRoute2( int index, int nameLen )
{
    return CommExecSetRoute( index, 1, nameLen );
}

#if defined(BLD_USE_FSK_R)
/*******************************************************************************
* CommExecSetRoute3
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSetRoute3( int index, int nameLen )
{
    return CommExecSetRoute( index, 2, nameLen );
}
#endif

/*******************************************************************************
* CommExecSetRoute
*
* Interface assumptions:
*     index         command index
*     route         route index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSetRoute( int index, int route, int nameLen )
{
    uint16_t    tmpId = 0;

    Terminal_Print( "\r\n  please set Router%u ID (0001 - FFFE) > ", (unsigned)route + 1 );

    // input and check and change parameter
    if( !InputArgumentHex16(nameLen, &tmpId, 4) ||
        OUT_OF_RANGE( tmpId, MIN_ROUTE_ID, MAX_ROUTE_ID ) ||
        !ModifyNetworkSetting_route(tmpId, route) )
    {
        Terminal_Print( "\r\n  Router%u ID is invalid.", (unsigned)route + 1 );
        Processor_Print( "NG 002\r\n" );
    }
    else
    {
        mTermParam.Route[route] = tmpId;

        Terminal_Print(  "\r\n  Router%u ID is 0x%04X", (unsigned)route + 1, (unsigned)mTermParam.Route[route]);
        Processor_Print( "OK\r\n" );
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}
#endif

/*******************************************************************************
* ConvMenuId_ack
*
* Interface assumptions:
*     menuId        menu id
*     ackReq        ack request
*
* Return value:
*     is parameter valid
*
*******************************************************************************/
static bool_t ConvMenuId_ack( uint8_t menuId, bool_t* ackReq )
{
    switch( menuId )
    {
    case MODE_OFF: *ackReq = FALSE; break;
    case MODE_ON:  *ackReq = TRUE; break;
    default:       return FALSE;
    }
    return TRUE;
}

/*******************************************************************************
* CommExecSetAck
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSetAck( int index, int nameLen )
{
    uint8_t tmpMode = 0;
    bool_t ackReq;

    TerminalPrintMessage( cuOnOffMenu );

    // input and check and change parameter
    if( !InputArgumentUint8(nameLen, &tmpMode, 1) ||
        !ConvMenuId_ack(tmpMode, &ackReq) ||
        !ModifyNetworkSetting_ack(ackReq) )
    {
        Terminal_Print( "\r\n  selected number is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    else
    {
        mTermParam.Ack = tmpMode;
        switch( tmpMode )
        {
        case MODE_ON:
            Terminal_Print( "\r\n  Acknowledge is ON." );
            break;

        case MODE_OFF:
            Terminal_Print( "\r\n  Acknowledge is OFF." );
            break;
        }
        Processor_Print( "OK\r\n" );
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSetRetry
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSetRetry( int index, int nameLen )
{
    uint8_t tmpCount = 0;

    Terminal_Print( "\r\n  please set retry count (0 - 10) > " );

    // input and check and change parameter
    if( !InputArgumentUint8(nameLen, &tmpCount, 2) || !ModifyNetworkSetting_retry(tmpCount) )
    {
        Terminal_Print( "\r\n  retry count is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    else
    {
        mTermParam.Retry = tmpCount;

        Terminal_Print( "\r\n  retry count is %u", (unsigned)mTermParam.Retry );
        Processor_Print( "OK\r\n" );
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSelectTransMode
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSelectTransMode( int index, int nameLen )
{
    uint8_t tmpMode = 0;

    TerminalPrintMessage( cuTransMenu );

    // input parameter
    if( !InputArgumentUint8(nameLen, &tmpMode, 1) )
    {
        Terminal_Print( "\r\n  selected number is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    // check and change parameter
    else
    {
        switch( tmpMode )
        {
        case TRANS_PAYLOAD:
            mTermParam.TransMode = TRANS_PAYLOAD;
            Terminal_Print( "\r\n  Payload Mode is selected." );
            Processor_Print( "OK\r\n" );
            break;

        case TRANS_FRAME:
            mTermParam.TransMode = TRANS_FRAME;
            Terminal_Print( "\r\n  Frame Mode is selected." );
            Processor_Print( "OK\r\n" );
            break;

        default:
            Terminal_Print( "\r\n  selected number is invalid." );
            Processor_Print( "NG 002\r\n" );
            break;
        }
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSelectRcvId
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSelectRcvId( int index, int nameLen )
{
    uint8_t tmpMode = 0;

    TerminalPrintMessage( cuOnOffMenu );

    // input parameter
    if( !InputArgumentUint8(nameLen, &tmpMode, 1) )
    {
        Terminal_Print( "\r\n  selected number is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    // check and change parameter
    else
    {
        switch( tmpMode )
        {
        case MODE_ON:
            mTermParam.RcvId = MODE_ON;
            Terminal_Print( "\r\n  Receive Node ID information is ON." );
            Processor_Print( "OK\r\n" );
            break;

        case MODE_OFF:
            mTermParam.RcvId = MODE_OFF;
            Terminal_Print( "\r\n  Receive Node ID information is OFF." );
            Processor_Print( "OK\r\n" );
            break;

        default:
            Terminal_Print( "\r\n  selected number is invalid." );
            Processor_Print( "NG 002\r\n" );
            break;
        }
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSelectRssi
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSelectRssi( int index, int nameLen )
{
    uint8_t tmpMode = 0;

    TerminalPrintMessage( cuOnOffMenu );

    // input parameter
    if( !InputArgumentUint8(nameLen, &tmpMode, 1) )
    {
        Terminal_Print( "\r\n  selected number is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    // check and change parameter
    else
    {
        switch( tmpMode )
        {
        case MODE_ON:
            mTermParam.Rssi = MODE_ON;
            Terminal_Print( "\r\n  RSSI information is ON." );
            Processor_Print( "OK\r\n" );
            break;

        case MODE_OFF:
            mTermParam.Rssi = MODE_OFF;
            Terminal_Print( "\r\n  RSSI information is OFF." );
            Processor_Print( "OK\r\n" );
            break;

        default:
            Terminal_Print( "\r\n  selected number is invalid." );
            Processor_Print( "NG 002\r\n" );
            break;
        }
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSelectOperation
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSelectOperation( int index, int nameLen )
{
    uint8_t tmpMode = 0;

    TerminalPrintMessage( cuOperationMenu );

    // input parameter
    if( !InputArgumentUint8(nameLen, &tmpMode, 1) )
    {
        Terminal_Print( "\r\n  selected number is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    // check and change parameter
    else
    {
        switch( tmpMode )
        {
        case CONFIG:
            mTermParam.Operation = CONFIG;
            Terminal_Print( "\r\n  Configuration is selected." );
            Processor_Print( "OK\r\n" );
            break;

        case OPERATION:
            mTermParam.Operation = OPERATION;
            Terminal_Print( "\r\n  Operation is selected." );
            Processor_Print( "OK\r\n" );
            break;

        default:
            Terminal_Print( "\r\n  selected number is invalid." );
            Processor_Print( "NG 002\r\n" );
            break;
        }
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSelectBaudrate
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSelectBaudrate( int index, int nameLen )
{
    uint8_t tmpMode = 0;

    TerminalPrintMessage( cuBaudrateMenu );

    // input parameter
    if( !InputArgumentUint8(nameLen, &tmpMode, 1) )
    {
        Terminal_Print( "\r\n  selected number is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    // check and change parameter
    else
    {
        switch( tmpMode )
        {
        case 1:
            mTermParam.Baudrate = UART_BAUD_RATE_9600_c;
            Terminal_Print( "\r\n  9600 baud is selected." );
            Processor_Print( "OK\r\n" );
            break;

        case 2:
            mTermParam.Baudrate = UART_BAUD_RATE_19200_c;
            Terminal_Print( "\r\n  19200 baud is selected." );
            Processor_Print( "OK\r\n" );
            break;

        case 3:
            mTermParam.Baudrate = UART_BAUD_RATE_38400_c;
            Terminal_Print( "\r\n  38400 baud is selected." );
            Processor_Print( "OK\r\n" );
            break;

        case 4:
            mTermParam.Baudrate = UART_BAUD_RATE_57600_c;
            Terminal_Print( "\r\n  57600 baud is selected." );
            Processor_Print( "OK\r\n" );
            break;

        case 5:
            mTermParam.Baudrate = UART_BAUD_RATE_115200_c;
            Terminal_Print( "\r\n  115200 baud is selected." );
            Processor_Print( "OK\r\n" );
            break;

        case 6:
            mTermParam.Baudrate = UART_BAUD_RATE_230400_c;
            Terminal_Print( "\r\n  230400 baud is selected." );
            Processor_Print( "OK\r\n" );
            break;

        default:
            Terminal_Print( "\r\n  selected number is invalid." );
            Processor_Print( "NG 002\r\n" );
            break;
        }

        Serial1_FlushWrite();
        Serial1_SetBaudRate( mTermParam.Baudrate );
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSelectSleep
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSelectSleep( int index, int nameLen )
{
    uint8_t tmpMode = 0;

    TerminalPrintMessage( cuSleepMenu );

    // input parameter
    if( !InputArgumentUint8(nameLen, &tmpMode, 1) )
    {
        Terminal_Print( "\r\n  selected number is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    // check and change parameter
    else
    {
        switch( tmpMode )
        {
        case NO_SLEEP:
            mTermParam.Sleep = NO_SLEEP;
            Terminal_Print( "\r\n  No Sleep Mode is selected." );
            Processor_Print( "OK\r\n" );
            break;

        case TIMER_WAKEUP:
            mTermParam.Sleep = TIMER_WAKEUP;
            Terminal_Print( "\r\n  Timer Wakeup Mode is selected." );
            Processor_Print( "OK\r\n" );
            break;

        case INT_WAKEUP:
            mTermParam.Sleep = INT_WAKEUP;
            Terminal_Print( "\r\n  INT Wakeup Mode (Tx continue) is selected." );
            Processor_Print( "OK\r\n" );
            break;

        case INT_WAKEUP_1TX:
            mTermParam.Sleep = INT_WAKEUP_1TX;
            Terminal_Print( "\r\n  INT Wakeup (One time Tx) Mode is selected." );
            Processor_Print( "OK\r\n" );
            break;

        case UART_WAKEUP:
            mTermParam.Sleep = UART_WAKEUP;
            Terminal_Print( "\r\n  UART Wakeup Mode is selected." );
            Processor_Print( "OK\r\n" );
            break;

        default:
            Terminal_Print( "\r\n  selected number is invalid." );
            Processor_Print( "NG 002\r\n" );
            break;
        }
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSelectMcuLpMode
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSelectMcuLpMode( int index, int nameLen )
{
    uint8_t tmpMode = 0;

    TerminalPrintMessage( cuMcuLpModeMenu );

    // input parameter
    if( !InputArgumentUint8(nameLen, &tmpMode, 1) )
    {
        Terminal_Print( "\r\n  selected number is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    // check and change parameter
    else
    {
        switch( tmpMode )
        {
        case MCU_STOP:
            mTermParam.LpModeMcu = MCU_STOP;
            Terminal_Print( "\r\n  STOP Mode is selected." );
            Processor_Print( "OK\r\n" );
            break;

        case MCU_SLEEP:
            mTermParam.LpModeMcu = MCU_SLEEP;
            Terminal_Print( "\r\n  SLEEP Mode is selected." );
            Processor_Print( "OK\r\n" );
            break;

        default:
            Terminal_Print( "\r\n  selected number is invalid." );
            Processor_Print( "NG 002\r\n" );
            break;
        }
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSelectRfLpMode
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSelectRfLpMode( int index, int nameLen )
{
    uint8_t tmpMode = 0;

    TerminalPrintMessage( cuRfLpModeMenu );

    // input parameter
    if( !InputArgumentUint8(nameLen, &tmpMode, 1) )
    {
        Terminal_Print( "\r\n  selected number is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    // check and change parameter
    else
    {
        switch( tmpMode )
        {
        case RF_SLEEP_COLD:
            mTermParam.LpModeRf = RF_SLEEP_COLD;
            Terminal_Print( "\r\n  Sleep with Cold Start is selected." );
            Processor_Print( "OK\r\n" );
            break;

        case RF_SLEEP_WARM:
            mTermParam.LpModeRf = RF_SLEEP_WARM;
            Terminal_Print( "\r\n  Sleep with Warm Start is selected." );
            Processor_Print( "OK\r\n" );
            break;

        case RF_ACTIVE:
            mTermParam.LpModeRf = RF_ACTIVE;
            Terminal_Print( "\r\n  Active is selected." );
            Processor_Print( "OK\r\n" );
            break;

        default:
            Terminal_Print( "\r\n  selected number is invalid." );
            Processor_Print( "NG 002\r\n" );
            break;
        }
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* ConvMenuId_protocol
*
* Interface assumptions:
*     menuId        menu id
*     protocol      protocol
*
* Return value:
*     is parameter valid
*
*******************************************************************************/
static bool_t ConvMenuId_protocol( uint8_t menuId, uint8_t* protocol )
{
    switch(menuId)
    {
#if defined(BLD_USE_LORA_NR)
    case 1:     *protocol = Protocol_LORA_NR; break;
#endif
#if defined(BLD_USE_LORA_R)
    case 2:     *protocol = Protocol_LORA_R; break;
#endif
#if defined(BLD_USE_FSK_R)
    case 3:     *protocol = Protocol_FSK_R; break;
#endif
    default:   return FALSE;
    }
    return TRUE;
}

/*******************************************************************************
* CommExecSelectProtocol
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSelectProtocol( int index, int nameLen )
{
    uint8_t tmpId = 0;
    uint8_t tmpProtocol;
    uint16_t tmpNode;
    uint16_t tmpCh;
    uint16_t tmpHopCnt;

    TerminalPrintMessage( cuProtocolTypeMenu );

    // input parameter
    if( !InputArgumentUint8(nameLen, &tmpId, 1) || !ConvMenuId_protocol(tmpId, &tmpProtocol) )
    {
        Terminal_Print( "\r\n  selected number is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    // check and change parameter
    else
    {
        tmpNode   = mTermParam.Node;
        tmpCh     = mTermParam.Channel;
        tmpHopCnt = mTermParam.HopCnt;

        // adjust node type
#if defined(BLD_USE_LORA_NR)
        if( tmpNode == ROUTER )
        {
            switch( tmpProtocol )
            {
            __cases_Protocol_NR
                tmpNode = END_DEVICE;
                break;
            }
        }
#endif

        // adjust channel
        switch( tmpProtocol )
        {
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
        __cases_Protocol_LORA
            if( MAX_CH_LORA(mTermParam.Bw) < tmpCh )
            {
                tmpCh = 1;
            }
            break;
#endif

#if defined(BLD_USE_FSK_R)
        __cases_Protocol_FSK
            if( MAX_CH_FSK(mTermParam.Rate) < tmpCh )
            {
                tmpCh = 1;
            }
            break;
#endif
        }

        // adjust hopcount
        if( MAX_HOP_CNT( (protocolType_t)tmpProtocol ) < tmpHopCnt )
        {
            tmpHopCnt = MAX_HOP_CNT( (protocolType_t)tmpProtocol );
        }

        // set
        if( !ModifyNetworkSetting_protocol(tmpProtocol, tmpCh, tmpHopCnt) )
        {
            Terminal_Print( "\r\n  selected number is invalid." );
            Processor_Print( "NG 002\r\n" );
        }
        else
        {
            mTermParam.Protocol = tmpProtocol;
            mTermParam.Node     = tmpNode;
            mTermParam.Channel  = tmpCh;
            mTermParam.HopCnt   = tmpHopCnt;

            switch( tmpProtocol )
            {
#if defined(BLD_USE_LORA_NR)
            case Protocol_LORA_NR:
                Terminal_Print( "\r\n  Private LoRa is selected." );
                break;
#endif

#if defined(BLD_USE_LORA_R)
            case Protocol_LORA_R:
                Terminal_Print( "\r\n  Private LoRa with Static Routing is selected." );
                break;
#endif

#if defined(BLD_USE_FSK_R)
            case Protocol_FSK_R:
                Terminal_Print( "\r\n  FSK with Static Routing is selected." );
                break;
#endif
            }
            Processor_Print( "OK\r\n" );
        }
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* ConvMenuId_rxBoost
*
* Interface assumptions:
*     menuId        menu id
*     rxBoost       Rx Boosted
*
* Return value:
*     is parameter valid
*
*******************************************************************************/
static bool_t ConvMenuId_rxBoost( uint8_t menuId, bool_t* rxBoost )
{
    switch( menuId )
    {
    case MODE_OFF: *rxBoost = FALSE; break;
    case MODE_ON:  *rxBoost = TRUE; break;
    default:    return FALSE;
    }
    return TRUE;
}

/*******************************************************************************
* CommExecSetRxBoost
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSetRxBoost( int index, int nameLen )
{
    uint8_t tmpMode = 0;
    bool_t rxBoost;

    TerminalPrintMessage( cuOnOffMenu );

    // input and check and change parameter
    if( !InputArgumentUint8(nameLen, &tmpMode, 1) ||
        !ConvMenuId_rxBoost(tmpMode, &rxBoost) ||
        !ModifyNetworkSetting_rxBoost(rxBoost) )
    {
        Terminal_Print( "\r\n  selected number is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    else
    {
        mTermParam.RxBoost = tmpMode;
        switch( tmpMode )
        {
        case MODE_ON:
            Terminal_Print( "\r\n  Rx Boosted is ON." );
            break;

        case MODE_OFF:
            Terminal_Print( "\r\n  Rx Boosted is OFF." );
            break;
        }
        Processor_Print( "OK\r\n" );
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSetSleepTime
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSetSleepTime( int index, int nameLen )
{
    uint32_t tmpValue = 0;

    Terminal_Print( "\r\n  please set sleep time (1 - 864000) > " );

    // input and check parameter
    if( !InputArgumentUint32(nameLen, &tmpValue, 6) ||
        OUT_OF_RANGE(tmpValue, MIN_SLEEP_TIME, MAX_SLEEP_TIME ) )
    {
        Terminal_Print( "\r\n  sleep time is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    // change parameter
    else
    {
        mTermParam.SleepTime = tmpValue;

        Terminal_Print( "\r\n  sleep time is %u", (unsigned)mTermParam.SleepTime );
        Processor_Print( "OK\r\n" );
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSetPower
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSetPower( int index, int nameLen )
{
    int32_t tmpValue = 0;

    Terminal_Print( "\r\n  please set output power (-4 - 13) > " );

    // input and check and change parameter
    if( !InputArgumentInt32(nameLen, &tmpValue, 3) || !ModifyNetworkSetting_power(tmpValue) )
    {
        Terminal_Print( "\r\n  output power is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    // change parameter
    else
    {
        mTermParam.Power = tmpValue;

        Terminal_Print( "\r\n  output power is %d", (int)mTermParam.Power );
        Processor_Print( "OK\r\n" );
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSetFormat
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSetFormat( int index, int nameLen )
{
    uint8_t tmpMode = 0;

    TerminalPrintMessage( cuFormatMenu );

    // input parameter
    if( !InputArgumentUint8(nameLen, &tmpMode, 1) )
    {
        Terminal_Print( "\r\n  selected number is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    // check and change parameter
    else
    {
        switch( tmpMode )
        {
        case FMT_ASCII:
            mTermParam.Format = FMT_ASCII;
            Terminal_Print( "\r\n  ASCII format is selected." );
            Processor_Print( "OK\r\n" );
            break;

        case FMT_BINARY:
            mTermParam.Format = FMT_BINARY;
            Terminal_Print( "\r\n  BINARY format is selected." );
            Processor_Print( "OK\r\n" );
            break;

        default:
            Terminal_Print( "\r\n  selected number is invalid." );
            Processor_Print( "NG 002\r\n" );
            break;
        }
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSetSendTime
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSetSendTime( int index, int nameLen )
{
    uint32_t tmpValue = 0;

    Terminal_Print( "\r\n  please set send time (0 - 86400) > " );

    // input and check parameter
    if( !InputArgumentUint32(nameLen, &tmpValue, 5) || (tmpValue > MAX_SEND_TIME) )
    {
        Terminal_Print( "\r\n  send time is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    // change parameter
    else
    {
        mTermParam.SendTime = tmpValue;

        Terminal_Print( "\r\n  send time is %u", (unsigned)mTermParam.SendTime );
        Processor_Print( "OK\r\n" );
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSetSendData
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSetSendData( int index, int nameLen )
{
    Terminal_Print( "\r\n  please set send data > " );

    // input parameter
    InputArgumentStr( nameLen, mTermParam.SendData, 50 );
    mTermParam.SendData[50] = '\0';

    Terminal_Print( "\r\n  send data is %s", mTermParam.SendData );
    Processor_Print( "OK\r\n" );

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSetAesKey
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSetAesKey( int index, int nameLen )
{
    uint8_t tmpAesKey[sizeof(mTermParam.AesKey)];

    Terminal_Print( "\r\n  please set AES Key (16bytes Hex) > " );

    // input parameter
    if( !InputArgumentHex(nameLen, tmpAesKey, sizeof(mTermParam.AesKey) ) ||
        !ModifyNetworkSetting_aesKey(tmpAesKey) )
    {
        Terminal_Print( "\r\n  AES Key is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    // change parameter
    else
    {
        memcpy( mTermParam.AesKey, tmpAesKey, sizeof(mTermParam.AesKey) );

        Terminal_Print( "\r\n  AES Key is ");
        Terminal_PrintHex( mTermParam.AesKey, sizeof(mTermParam.AesKey), "" );

        Processor_Print( "OK\r\n" );
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSelectRfMode
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSelectRfMode( int index, int nameLen )
{
    uint8_t tmpMode = 0;

    TerminalPrintMessage( cuRfModeMenu );

    // input parameter
    if( !InputArgumentUint8(nameLen, &tmpMode, 1) )
    {
        Terminal_Print( "\r\n  selected number is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    // check and change parameter
    else
    {
        switch( tmpMode )
        {
        case RFMODE_TXRX:
            mTermParam.RfMode = RFMODE_TXRX;
            Terminal_Print( "\r\n  TxRx Mode is selected." );
            Processor_Print( "OK\r\n" );
            break;

        case RFMODE_TXONLY:
            mTermParam.RfMode = RFMODE_TXONLY;
            Terminal_Print( "\r\n  Tx Only Mode is selected." );
            Processor_Print( "OK\r\n" );
            break;

#if defined(BLD_ENABLE_RFMODE_BURST)
        case RFMODE_BURST:
            mTermParam.RfMode = RFMODE_BURST;
            Terminal_Print( "\r\n  Burst Mode is selected. (for testing)" );
            Processor_Print( "OK\r\n" );
            break;
#endif
#if defined(BLD_ENABLE_RFMODE_CW)
        case RFMODE_CW:
            mTermParam.RfMode = RFMODE_CW;
            Terminal_Print( "\r\n  Tx Continuous Wave Mode is selected. (for testing)" );
            Processor_Print( "OK\r\n" );
            break;
#endif
#if defined(BLD_ENABLE_RFMODE_TX_INFINITE)
        case RFMODE_TX_INFINITE:
            mTermParam.RfMode = RFMODE_TX_INFINITE;
            Terminal_Print( "\r\n   Tx Infinite Preamble Mode is selected. (for testing)" );
            Processor_Print( "OK\r\n" );
            break;
#endif
#if defined(BLD_ENABLE_RFMODE_RSSI_CHECK)
        case RFMODE_RSSI_CHECK:
            mTermParam.RfMode = RFMODE_RSSI_CHECK;
            Terminal_Print( "\r\n  RSSI Check is selected. (for testing)" );
            Processor_Print( "OK\r\n" );
            break;
#endif
        default:
            Terminal_Print( "\r\n  selected number is invalid." );
            Processor_Print( "NG 002\r\n" );
            break;
        }
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSetBackoff
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSetBackoff( int index, int nameLen )
{
    uint32_t tmpValue = 0;

    Terminal_Print( "\r\n  please set backoff time (%u - %u) > ", MIN_RETRY_BACKOFF, MAX_RETRY_BACKOFF );

    // input and check parameter
    if( !InputArgumentUint32(nameLen, &tmpValue, 6) || (tmpValue > MAX_RETRY_BACKOFF) )
    {
        Terminal_Print( "\r\n  backoff time is invalid." );
        Processor_Print( "NG 002\r\n" );
    }
    else
    {
        mTermParam.Backoff = tmpValue;

        Terminal_Print( "\r\n  backoff time is %u", (unsigned)mTermParam.Backoff );
        Processor_Print( "OK\r\n" );
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecVersion
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecVersion( int index, int nameLen )
{
    Terminal_Print( "\r\n%s", cu8Version );
    Processor_Print( "%s\r\n", cu8Version );

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecSaveParameter
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecSaveParameter( int index, int nameLen )
{
    uint8_t ret;

    Terminal_Print( "\r\nsave parameter ..." );

    ret = TermParam_saveToRom();
    switch(ret)
    {
    case 0:
        Terminal_Print( " Done" );
        Processor_Print( "OK\r\n" );
        break;

    case 3:
        Terminal_Print( " FlashROM erase error\r\n\r\n" );
        Processor_Print( "NG 003\r\n" );
        break;

    case 4:
        Terminal_Print( " FlashROM write error\r\n\r\n" );
        Processor_Print( "NG 004\r\n" );
        break;

    case 5:
        Terminal_Print( " FlashROM read error\r\n\r\n" );
        Processor_Print( "NG 005\r\n" );
        break;

    case 6:
        Terminal_Print( " FlashROM verify error\r\n\r\n" );
        Processor_Print( "NG 006\r\n" );
        break;
    }

    Terminal_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecLoadParameter
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecLoadParameter( int index, int nameLen )
{
    smacParams_t params;

    Terminal_Print( "\r\nload default parameter ..." );

    TermParam_loadDefault();

    MakeSmacParams( &params );

    if( SMAC_SetAllParams(&params) != gErrorNoError_c )
    {
        Terminal_Print( "\r\n  network parameter setting failed." );
        Processor_Print( "NG 002\r\n" );
    }
    else
    {
        Terminal_Print( " Done" );
        Processor_Print( "OK\r\n" );
    }

    Terminal_Print( "\r\n\r\n" );

    Serial1_FlushWrite();
    Serial1_SetBaudRate( mTermParam.Baudrate );

    return FALSE;
}

/*******************************************************************************
* CommExecShow
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecShow( int index, int nameLen )
{
    Serial1_Print( "\r\n  configuration setting is below." );
    Serial1_Print( "\r\n  -------------------------------------" );

    switch( mTermParam.Protocol )
    {
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    __cases_Protocol_R
        Serial1_Print( "\r\n  Node                        : %s", (  mTermParam.Node == COORDINATOR  ? "Coordinator" :
                                                                    mTermParam.Node == END_DEVICE   ? "EndDevice"   :
                                                                    mTermParam.Node == ROUTER       ? "Router"      : "???" ) );
        break;
#endif
    default:
        Serial1_Print( "\r\n  Node                        : %s", ( mTermParam.Node == COORDINATOR   ? "Coordinator" :
                                                                   mTermParam.Node == END_DEVICE    ? "EndDevice"   : "???" ) );
        break;
    }

    switch( mTermParam.Protocol )
    {
#if defined(BLD_USE_FSK_R)
    __cases_Protocol_FSK
        Serial1_Print( "\r\n  Data Rate                   : %s", (  mTermParam.Rate == RATE_50KBPS  ? "50kbps"  :
                                                                    mTermParam.Rate == RATE_100KBPS ? "100kbps" :
                                                                    mTermParam.Rate == RATE_150KBPS ? "150kbps" :
                                                                    "???" ) );
        break;
#endif
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
    __cases_Protocol_LORA
        Serial1_Print( "\r\n  Band Width                  : %s", (  mTermParam.Bw == BANDWIDTH31_25 ? "31.25kHz":
                                                                    mTermParam.Bw == BANDWIDTH41_7  ? "41.7kHz" :
                                                                    mTermParam.Bw == BANDWIDTH62_5  ? "62.5kHz" :
                                                                    mTermParam.Bw == BANDWIDTH125   ? "125kHz"  :
                                                                    mTermParam.Bw == BANDWIDTH250   ? "250kHz"  : "???" ) );

        Serial1_Print( "\r\n  Spreading Factor            : %u", (unsigned)mTermParam.Sf );

        uint32_t dataRate = 0;
        SMAC_CalcDataRate( mTermParam.Bw, mTermParam.Sf, &dataRate);
        Serial1_Print( "\r\n  Effective Bitrate           : %ubps", (unsigned)dataRate );
        break;
#endif
    }

    WDG_Refresh();

    Serial1_Print( "\r\n  Channel                     : %u", (unsigned)mTermParam.Channel );

    Serial1_Print( "\r\n  PAN ID                      : %04X", (unsigned)mTermParam.PanId );

    Serial1_Print( "\r\n  Own Node ID                 : %04X", (unsigned)mTermParam.SrcId );

    Serial1_Print( "\r\n  Destination ID              : %04X", (unsigned)mTermParam.DstId, 2, 0 );

    WDG_Refresh();

#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    switch( mTermParam.Protocol )
    {
    __cases_Protocol_R
        Serial1_Print( "\r\n  Hop Count                   : %u", (unsigned)mTermParam.HopCnt );

        Serial1_Print( "\r\n  End ID                      : %04X", (unsigned)mTermParam.EndId );

        Serial1_Print( "\r\n  1st Route ID                : %04X", (unsigned)mTermParam.Route[0] );

        Serial1_Print( "\r\n  2nd Route ID                : %04X", (unsigned)mTermParam.Route[1]);

        WDG_Refresh();
        break;
    }
#endif
#if defined(BLD_USE_FSK_R)
    if( mTermParam.Protocol == Protocol_FSK_R )
    {
        Serial1_Print( "\r\n  3rd Route ID                : %04X", (unsigned)mTermParam.Route[2]);
    }
#endif

    Serial1_Print( "\r\n  Acknowledge                 : %s", (  mTermParam.Ack == MODE_ON  ? "ON"   :
                                                                mTermParam.Ack == MODE_OFF ? "OFF"  : "???" ) );

    Serial1_Print( "\r\n  Retry count                 : %u", (unsigned)mTermParam.Retry );

    WDG_Refresh();

    Serial1_Print( "\r\n  Transfer Mode               : %s", (  mTermParam.TransMode == TRANS_PAYLOAD ? "Payload"   :
                                                                mTermParam.TransMode == TRANS_FRAME   ? "Frame"     : "???" ) );

    Serial1_Print( "\r\n  Receive Node ID information : %s", (  mTermParam.RcvId == MODE_ON  ? "ON" :
                                                                mTermParam.RcvId == MODE_OFF ? "OFF": "???" ) );

    Serial1_Print( "\r\n  RSSI information            : %s", (  mTermParam.Rssi == MODE_ON  ? "ON"  :
                                                                mTermParam.Rssi == MODE_OFF ? "OFF" : "???" ) );

    WDG_Refresh();

    Serial1_Print( "\r\n  Config/Operation            : %s", (  mTermParam.Operation == CONFIG    ? "Configuration" :
                                                                mTermParam.Operation == OPERATION ? "Operation"     : "???" ) );

    Serial1_Print( "\r\n  UART baudrate               : %u", (unsigned)mTermParam.Baudrate );

    Serial1_Print( "\r\n  Sleep Mode                  : %s", (  mTermParam.Sleep == NO_SLEEP       ? "No Sleep"                 :
                                                                mTermParam.Sleep == TIMER_WAKEUP   ? "Timer Wakeup"             :
                                                                mTermParam.Sleep == INT_WAKEUP     ? "INT Wakeup (Tx continue)" :
                                                                mTermParam.Sleep == INT_WAKEUP_1TX ? "INT Wakeup (One time Tx)" :
                                                                mTermParam.Sleep == UART_WAKEUP    ? "UART Wakeup"              : "???" ) );

    WDG_Refresh();

    Serial1_Print( "\r\n  Sleep Time                  : %u", (unsigned)mTermParam.SleepTime );

    Serial1_Print( "\r\n  Retry backoff               : %u", (unsigned)mTermParam.Backoff );

    if( IS_SLEEP(mTermParam) )
    {
        Serial1_Print( "\r\n  MCU Low Power Mode          : %s", (  mTermParam.LpModeMcu == MCU_STOP        ? "STOP Mode"   :
                                                                    mTermParam.LpModeMcu == MCU_SLEEP       ? "SLEEP Mode"             : "???" ) );
        Serial1_Print( "\r\n  RF Low Power Mode           : %s", (  mTermParam.LpModeRf == RF_SLEEP_COLD    ? "Sleep with Cold start"  :
                                                                    mTermParam.LpModeRf == RF_SLEEP_WARM    ? "Sleep with Warm start"  :
                                                                    mTermParam.LpModeRf == RF_ACTIVE        ? "Active"                 : "???" ) );
    }

    WDG_Refresh();

    Serial1_Print( "\r\n  Output Power                : %ddBm", (int)mTermParam.Power );

    Serial1_Print( "\r\n  Format                      : %s", (  mTermParam.Format == FMT_ASCII  ? "ASCII"   :
                                                                mTermParam.Format == FMT_BINARY ? "BINARY"  : "???" ) );

    Serial1_Print( "\r\n  Send Time                   : %u", (unsigned)mTermParam.SendTime );

    WDG_Refresh();

    Serial1_Print( "\r\n  Send Data                   : %s", mTermParam.SendData );

    Serial1_Print( "\r\n  AES Key                     : " );
    Serial1_PrintHex( mTermParam.AesKey, sizeof(mTermParam.AesKey), "" );

    Serial1_Print( "\r\n  RF Mode                     : %s", (  mTermParam.RfMode == RFMODE_TXRX        ? "TxRx"                                :
                                                                mTermParam.RfMode == RFMODE_TXONLY      ? "Tx Only"                             :
#if defined(BLD_ENABLE_RFMODE_BURST)
                                                                mTermParam.RfMode == RFMODE_BURST       ? "Tx Burst (for testing)"              :
#endif
#if defined(BLD_ENABLE_RFMODE_CW)
                                                                mTermParam.RfMode == RFMODE_CW          ? "Tx Continuous Wave (for testing)"    :
#endif
#if defined(BLD_ENABLE_RFMODE_TX_INFINITE)
                                                                mTermParam.RfMode == RFMODE_TX_INFINITE ? "Tx Infinite Preamble (for testing)"  :
#endif
#if defined(BLD_ENABLE_RFMODE_RSSI_CHECK)
                                                                mTermParam.RfMode == RFMODE_RSSI_CHECK  ? "RSSI Check (for testing)"            :
#endif
                                                                                                        "???" ) );

    WDG_Refresh();

    Serial1_Print( "\r\n  Protocol Type               : %s", (
#if defined(BLD_USE_LORA_NR)
                                                                mTermParam.Protocol == Protocol_LORA_NR ? "Private LoRa"                        :
#endif
#if defined(BLD_USE_LORA_R)
                                                                mTermParam.Protocol == Protocol_LORA_R  ? "Private LoRa with Static Routing"    :
#endif
#if defined(BLD_USE_FSK_R)
                                                                mTermParam.Protocol == Protocol_FSK_R   ? "FSK with Static Routing"             :
#endif
                                                                                                         "???" ) );

    Serial1_Print( "\r\n  Rx Boosted                  : %s", (  mTermParam.RxBoost == MODE_ON   ? "ON"  :
                                                                mTermParam.RxBoost == MODE_OFF  ? "OFF" : "???" ) );

    Serial1_Print( "\r\n\r\n" );

    return FALSE;
}

/*******************************************************************************
* CommExecStart
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecStart( int index, int nameLen )
{
    Serial1_Print( "OK\r\n" );

    return TRUE;
}

/*******************************************************************************
* CommExecHelp
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecHelp( int index, int nameLen )
{
    CommShowHelp();

    return FALSE;
}

/*******************************************************************************
* CommExecPortCheck
*
* Interface assumptions:
*     index         command index
*     nameLen       command name length
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t CommExecPortCheck( int index, int nameLen )
{
    uint32_t tmpVal  = 0;
    uint8_t  tmpMode = 0;
    uint8_t  tmpPin  = 0;
    GPIO_InitTypeDef GPIO_InitStruct;
    struct
    {
        GPIO_TypeDef*   port;
        uint16_t        pin;
    } Pins[] = {
        { NULL,  0           }, // 1
        { NULL,  0           }, // 2
        { NULL,  0           }, // 3
        { GPIOB, GPIO_PIN_6  }, // 4  PB6
        { GPIOB, GPIO_PIN_7  }, // 5  PB7
        { GPIOB, GPIO_PIN_12 }, // 6  PB12
        { GPIOB, GPIO_PIN_14 }, // 7  PB14
        { GPIOA, GPIO_PIN_10 }, // 8  PA10
        { GPIOB, GPIO_PIN_13 }, // 9  PB13
        { GPIOA, GPIO_PIN_11 }, // 10 PA11
        { GPIOA, GPIO_PIN_12 }, // 11 PA12
        { GPIOA, GPIO_PIN_13 }, // 12 PA13
        { GPIOA, GPIO_PIN_14 }, // 13 PA14
        { NULL,  0           }, // 14
        { NULL,  0           }, // 15
        { GPIOA, GPIO_PIN_15 }, // 16 PA15
        { GPIOA, GPIO_PIN_0  }, // 17 PA0
        { GPIOB, GPIO_PIN_9  }, // 18 PB9
        { GPIOB, GPIO_PIN_8  }, // 19 PB8
        { GPIOB, GPIO_PIN_2  }, // 20 PB2
        { GPIOB, GPIO_PIN_3  }, // 21 PB3
        { GPIOB, GPIO_PIN_4  }, // 22 PB4
        { GPIOB, GPIO_PIN_5  }, // 23 PB5
        { GPIOC, GPIO_PIN_1  }, // 24 PC1
        { GPIOA, GPIO_PIN_2  }, // 25 PA2
        { GPIOA, GPIO_PIN_3  }, // 26 PA3
        { GPIOA, GPIO_PIN_6  }, // 27 PA6
        { GPIOA, GPIO_PIN_1  }, // 28 PA1
        { GPIOB, GPIO_PIN_0  }, // 29 PB10
        { NULL,  0           }, // 30
    };

    // input parameter
    if( !InputArgumentUint32(nameLen, &tmpVal, 3) )
    {
        Terminal_Print( "NG 002\r\n" );
        Processor_Print( "NG 002\r\n" );
        return FALSE;
    }

    tmpMode = tmpVal / 100;
    tmpPin  = tmpVal % 100;

    if( Pins[tmpPin-1].port )
    {
        switch( tmpMode )
        {
        // set
        case 1:
            HAL_GPIO_DeInit( Pins[tmpPin-1].port, Pins[tmpPin-1].pin );

            GPIO_InitStruct.Pin  = Pins[tmpPin-1].pin;
            GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
            GPIO_InitStruct.Pull = GPIO_NOPULL;
            HAL_GPIO_Init( Pins[tmpPin-1].port, &GPIO_InitStruct );

            HAL_GPIO_WritePin( Pins[tmpPin-1].port, Pins[tmpPin-1].pin, GPIO_PIN_SET );
            break;

        // reset
        case 2:
            HAL_GPIO_DeInit( Pins[tmpPin-1].port, Pins[tmpPin-1].pin );

            GPIO_InitStruct.Pin  = Pins[tmpPin-1].pin;
            GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
            GPIO_InitStruct.Pull = GPIO_NOPULL;
            HAL_GPIO_Init( Pins[tmpPin-1].port, &GPIO_InitStruct );

            HAL_GPIO_WritePin( Pins[tmpPin-1].port, Pins[tmpPin-1].pin, GPIO_PIN_RESET );
            break;

        // init
        case 3:
            HAL_GPIO_DeInit( Pins[tmpPin-1].port, Pins[tmpPin-1].pin );

            GPIO_InitStruct.Pin   = Pins[tmpPin-1].pin;
            if( 16 == tmpPin )
            {
                GPIO_InitStruct.Mode  = GPIO_MODE_IT_RISING_FALLING;
                GPIO_InitStruct.Pull  = GPIO_NOPULL;
            }
            else if( (19 == tmpPin) || (20 == tmpPin) )
            {
                GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
                GPIO_InitStruct.Pull  = GPIO_NOPULL;
                GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
            }
            else
            {
                GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
                GPIO_InitStruct.Pull = GPIO_NOPULL;
            }
            HAL_GPIO_Init( Pins[tmpPin-1].port, &GPIO_InitStruct );
        }
    }

    Terminal_Print( "OK\r\n" );
    Processor_Print( "OK\r\n" );

    return FALSE;
}

/*******************************************************************************
* InputArgumentUint8
*
* Interface assumptions:
*     nameLen       command name length
*     param         argument value
*     width         max text width
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t InputArgumentUint8( int nameLen, uint8_t* param, int width )
{
    /* terminal mode */
    if( TERMINAL == mTermParam.Mode )
    {
        WaitForInputCommLine();

        return StrToUint8( CommDataBuffer, param, width );
    }
    /* processor mode */
    else if( PROCESSOR == mTermParam.Mode )
    {
        if( CommDataBuffer[nameLen] == '\0' )
        {
            return FALSE;
        }
        return StrToUint8( CommDataBuffer+nameLen+1, param, width );
    }
    return FALSE;
}

/*******************************************************************************
* InputArgumentUint32
*
* Interface assumptions:
*     nameLen       command name length
*     param         argument value
*     width         max text width
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t InputArgumentUint32( int nameLen, uint32_t* param, int width )
{
    /* terminal mode */
    if( TERMINAL == mTermParam.Mode )
    {
        WaitForInputCommLine();

        return StrToUint32( CommDataBuffer, param, width );
    }
    /* processor mode */
    else if( PROCESSOR == mTermParam.Mode )
    {
        if( CommDataBuffer[nameLen] == '\0' )
        {
            return FALSE;
        }
        return StrToUint32( CommDataBuffer+nameLen+1, param, width );
    }
    return FALSE;
}

/*******************************************************************************
* InputArgumentInt32
*
* Interface assumptions:
*     nameLen       command name length
*     param         argument value
*     width         max text width
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t InputArgumentInt32( int nameLen, int32_t* param, int width )
{
    /* terminal mode */
    if( TERMINAL == mTermParam.Mode )
    {
        WaitForInputCommLine();

        return StrToInt32( CommDataBuffer, param, width );
    }
    /* processor mode */
    else if( PROCESSOR == mTermParam.Mode )
    {
        if( CommDataBuffer[nameLen] == '\0' )
        {
            return FALSE;
        }
        return StrToInt32( CommDataBuffer+nameLen+1, param, width );
    }
    return FALSE;
}

/*******************************************************************************
* InputArgumentHex16
*
* Interface assumptions:
*     nameLen       command name length
*     param         argument value
*     width         max text width
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t InputArgumentHex16( int nameLen, uint16_t* param, int width )
{
    /* terminal mode */
    if( TERMINAL == mTermParam.Mode )
    {
        WaitForInputCommLine();

        return StrToHex16( CommDataBuffer, param, width );
    }
    /* processor mode */
    else if( PROCESSOR == mTermParam.Mode )
    {
        if( CommDataBuffer[nameLen] == '\0' )
        {
            return FALSE;
        }
        return StrToHex16( CommDataBuffer+nameLen+1, param, width );
    }
    return FALSE;
}

/*******************************************************************************
* InputArgumentHex
*
* Interface assumptions:
*     nameLen       command name length
*     param         argument value
*     width         max text width
*
* Return value:
*     None
*
*******************************************************************************/
static bool_t InputArgumentHex( int nameLen, uint8_t* dest, uint8_t length )
{
    /* terminal mode */
    if( TERMINAL == mTermParam.Mode )
    {
        WaitForInputCommLine();

        return StrToHex( CommDataBuffer, dest, length );
    }
    /* processor mode */
    else if( PROCESSOR == mTermParam.Mode )
    {
        if( CommDataBuffer[nameLen] == '\0' )
        {
            return FALSE;
        }
        return StrToHex( CommDataBuffer+nameLen+1, dest, length );
    }
    return FALSE;
}

/*******************************************************************************
* InputArgumentStr
*
* Interface assumptions:
*     nameLen       command name length
*     dest          argument value
*     length        max text width
*
* Return value:
*     None
*
*******************************************************************************/
static void InputArgumentStr( int nameLen, uint8_t* dest, uint8_t length )
{
    /* terminal mode */
    if( TERMINAL == mTermParam.Mode )
    {
        WaitForInputCommLine();

        strncpy( (char*)dest, (const char*)CommDataBuffer, length );
    }
    /* processor mode */
    else if( PROCESSOR == mTermParam.Mode )
    {
        if( CommDataBuffer[nameLen] == '\0' )
        {
            dest[0] = '\0';
        }
        else
        {
            strncpy( (char*)dest, (const char*)CommDataBuffer+nameLen+1, length );
        }
    }
}

/*******************************************************************************
* ModifyNetworkSetting_protocol
*
* Interface assumptions:
*     protocol      new protocol value
*     ch            new channel value
*     hopCnt        new hop count value
*
* Return value:
*     result (0: failed, 1:succeed)
*
*******************************************************************************/
static bool_t ModifyNetworkSetting_protocol( uint8_t protocol, uint8_t ch, uint8_t hopCnt )
{
    smacParams_t params;

    if( SMAC_GetAllParams(&params) != gErrorNoError_c ) { return FALSE; }

    params.protocol = (protocolType_t)protocol;
    params.radio.modulation.ch = ch;
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    params.route.hop_cnt = hopCnt;
#endif
    if( SMAC_SetAllParams(&params) != gErrorNoError_c ) { return FALSE; }

    return TRUE;
}

#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
/*******************************************************************************
* ModifyNetworkSetting_hopCnt
*
* Interface assumptions:
*     val           new setting value
*
* Return value:
*     result (0: failed, 1:succeed)
*
*******************************************************************************/
static bool_t ModifyNetworkSetting_hopCnt( uint8_t val )
{
    smacRouteParams_t params;

    if( SMAC_GetRouteParams(&params) != gErrorNoError_c ) { return FALSE; }

    params.hop_cnt = val;

    if( SMAC_SetRouteParams(&params) != gErrorNoError_c ) { return FALSE; }

    return TRUE;
}

/*******************************************************************************
* ModifyNetworkSetting_route
*
* Interface assumptions:
*     idx           index of route id array
*     val           new setting value
*
* Return value:
*     result (0: failed, 1:succeed)
*
*******************************************************************************/
static bool_t ModifyNetworkSetting_route( uint16_t val, uint8_t idx)
{
    smacRouteParams_t params;

    if( SMAC_GetRouteParams(&params) != gErrorNoError_c ) { return FALSE; }

    params.route[idx] = val;

    if( SMAC_SetRouteParams(&params) != gErrorNoError_c ) { return FALSE; }

    return TRUE;
}

/*******************************************************************************
* ModifyNetworkSetting_endId
*
* Interface assumptions:
*     val           new setting value
*
* Return value:
*     result (0: failed, 1:succeed)
*
*******************************************************************************/
static bool_t ModifyNetworkSetting_endId( uint16_t val )
{
    smacRouteParams_t params;

    if( SMAC_GetRouteParams(&params) != gErrorNoError_c ) { return FALSE; }

    params.endNodeAddr = val;

    if( SMAC_SetRouteParams(&params) != gErrorNoError_c ) { return FALSE; }

    return TRUE;
}
#endif

/*******************************************************************************
* ModifyNetworkSetting_panId
*
* Interface assumptions:
*     val       new setting value
*
* Return value:
*     result (0: failed, 1:succeed)
*
*******************************************************************************/
static bool_t ModifyNetworkSetting_panId( uint16_t val )
{
    SmacTransParams_t params;

    if( SMAC_GetTransParams(&params) != gErrorNoError_c ) { return FALSE; }

    params.addr.srcNode.panId = val;

    if( SMAC_SetTransParams(&params) != gErrorNoError_c ) { return FALSE; }

    return TRUE;
}

/*******************************************************************************
* ModifyNetworkSetting_srcId
*
* Interface assumptions:
*     val       new setting value
*
* Return value:
*     result (0: failed, 1:succeed)
*
*******************************************************************************/
static bool_t ModifyNetworkSetting_srcId( uint16_t val )
{
    SmacTransParams_t transParams;
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    smacRouteParams_t routeParams;
#endif

    if( SMAC_GetTransParams(&transParams) != gErrorNoError_c
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
        || SMAC_GetRouteParams(&routeParams) != gErrorNoError_c
#endif
        )
    {
        return FALSE;
    }

    transParams.addr.srcNode.nodeAddr = val;
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    routeParams.oriNodeAddr = val;
#endif

    if( SMAC_SetTransParams(&transParams) != gErrorNoError_c
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
        || SMAC_SetRouteParams(&routeParams) != gErrorNoError_c
#endif
        )
    {
        return FALSE;
    }

    return TRUE;
}

/*******************************************************************************
* ModifyNetworkSetting_dstId
*
* Interface assumptions:
*     val       new setting value
*
* Return value:
*     result (0: failed, 1:succeed)
*
*******************************************************************************/
static bool_t ModifyNetworkSetting_dstId( uint16_t val )
{
    SmacTransParams_t params;

    if( SMAC_GetTransParams(&params) != gErrorNoError_c ) { return FALSE; }

    params.addr.destNodeAddr = val;

    if( SMAC_SetTransParams(&params) != gErrorNoError_c ) { return FALSE; }

    return TRUE;
}

/*******************************************************************************
* ModifyNetworkSetting_ack
*
* Interface assumptions:
*     val       new setting value
*
* Return value:
*     result (0: failed, 1:succeed)
*
*******************************************************************************/
static bool_t ModifyNetworkSetting_ack( bool_t val )
{
    SmacTransParams_t params;

    if( SMAC_GetTransParams(&params) != gErrorNoError_c ) { return FALSE; }

    params.option.ackReq = val;

    if( SMAC_SetTransParams(&params) != gErrorNoError_c ) { return FALSE; }

    return TRUE;
}

/*******************************************************************************
* ModifyNetworkSetting_retry
*
* Interface assumptions:
*     val       new setting value
*
* Return value:
*     result (0: failed, 1:succeed)
*
*******************************************************************************/
static bool_t ModifyNetworkSetting_retry( uint8_t val )
{
    SmacTransParams_t params;

    if( SMAC_GetTransParams(&params) != gErrorNoError_c ) { return FALSE; }

    params.option.ackRetryCount = val;

    if( SMAC_SetTransParams(&params) != gErrorNoError_c ) { return FALSE; }

    return TRUE;
}

/*******************************************************************************
* ModifyNetworkSetting_power
*
* Interface assumptions:
*     val       new setting value
*
* Return value:
*     result (0: failed, 1:succeed)
*
*******************************************************************************/
static bool_t ModifyNetworkSetting_power( uint8_t val )
{
    PhyRadioParams_t params;

    if( SMAC_GetRadioParams(&params) != gErrorNoError_c ) { return FALSE; }

    params.pa.power = val;

    if( SMAC_SetRadioParams(&params) != gErrorNoError_c ) { return FALSE; }

    return TRUE;
}

/*******************************************************************************
* ModifyNetworkSetting_rxBoost
*
* Interface assumptions:
*     val       new setting value
*
* Return value:
*     result (0: failed, 1:succeed)
*
*******************************************************************************/
static bool_t ModifyNetworkSetting_rxBoost( bool_t val )
{
    PhyRadioParams_t params;

    if( SMAC_GetRadioParams(&params) != gErrorNoError_c ) { return FALSE; }

    params.rxBoost = val;

    if( SMAC_SetRadioParams(&params) != gErrorNoError_c ) { return FALSE; }

    return TRUE;
}

/*******************************************************************************
* ModifyNetworkSetting_channel
*
* Interface assumptions:
*     val       new setting value
*
* Return value:
*     result (0: failed, 1:succeed)
*
*******************************************************************************/
static bool_t ModifyNetworkSetting_channel( uint8_t val )
{
    PhyRadioParams_t params;

    if( SMAC_GetRadioParams(&params) != gErrorNoError_c ) { return FALSE; }

    params.modulation.ch = val;

    if( SMAC_SetRadioParams(&params) != gErrorNoError_c ) { return FALSE; }

    return TRUE;
}

#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
/*******************************************************************************
* ModifyNetworkSetting_bw
*
* Interface assumptions:
*     val       new setting value
*
* Return value:
*     result (0: failed, 1:succeed)
*
*******************************************************************************/
static bool_t ModifyNetworkSetting_bw( uint8_t bw, uint8_t ch )
{
    PhyRadioParams_t params;

    if( SMAC_GetRadioParams(&params) != gErrorNoError_c ) { return FALSE; }

    params.modulation.LORA.bw = bw;
    params.modulation.ch = ch;

    if( SMAC_SetRadioParams(&params) != gErrorNoError_c ) { return FALSE; }

    return TRUE;
}

/*******************************************************************************
* ModifyNetworkSetting_sf
*
* Interface assumptions:
*     val       new setting value
*
* Return value:
*     result (0: failed, 1:succeed)
*
*******************************************************************************/
static bool_t ModifyNetworkSetting_sf( uint8_t val )
{
    PhyRadioParams_t params;

    if( SMAC_GetRadioParams(&params) != gErrorNoError_c ) { return FALSE; }

    params.modulation.LORA.sf = val;

    if( SMAC_SetRadioParams(&params) != gErrorNoError_c ) { return FALSE; }

    return TRUE;
}
#endif

#if defined(BLD_USE_FSK_R)
/*******************************************************************************
* ModifyNetworkSetting_rate
*
* Interface assumptions:
*     val       new setting value
*
* Return value:
*     result (0: failed, 1:succeed)
*
*******************************************************************************/
static bool_t ModifyNetworkSetting_rate( uint8_t dr, uint8_t ch )
{
    PhyRadioParams_t params;

    if( SMAC_GetRadioParams(&params) != gErrorNoError_c ) { return FALSE; }

    params.modulation.FSK.dr = dr;
    params.modulation.ch = ch;

    if( SMAC_SetRadioParams(&params) != gErrorNoError_c ) { return FALSE; }

    return TRUE;
}
#endif

/*******************************************************************************
* ModifyNetworkSetting_aesKey
*
* Interface assumptions:
*     aesKey    new setting value
*
* Return value:
*     result (0: failed, 1:succeed)
*
*******************************************************************************/
static bool_t ModifyNetworkSetting_aesKey( const uint8_t aesKey[SMAC_ENCRYPT_KEY_LENGTH] )
{
    SmacTransEncryptOption_t params;

    if( SMAC_GetEncryptOption(&params) != gErrorNoError_c ) { return FALSE; }

    params.enable = FALSE;
    if( memcmp( aesKey, NullAesKey, SMAC_ENCRYPT_KEY_LENGTH ) )
    {
        params.enable = TRUE;
        params.aesKey = aesKey;
    }

    if( SMAC_SetEncryptOption(&params) != gErrorNoError_c ) { return FALSE; }

    return TRUE;
}
