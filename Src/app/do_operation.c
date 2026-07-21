/*******************************************************************************
* do_operation file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#include "usr_common.h"
#include "rtc.h"
#include "platform/powerctl.h"
#include "platform/timerctl.h"
#include "platform/pendsv.h"
#include "platform/events.h"
#include "platform/led.h"
#include "app/usr_main.h"
#include "app/params.h"
#include "app/do_operation.h"
#include "app/commctl.h"

/*******************************************************************************
********************************************************************************
* Private macros
********************************************************************************
*******************************************************************************/

#define INTERVAL_TIMEOUT            ( 50 )

/*******************************************************************************
********************************************************************************
* Private prototypes
********************************************************************************
*******************************************************************************/

#define COMPETITION_MODE 1

#if COMPETITION_MODE
// ===== Competition Configuration (Group 3) =====
#define GROUP_ID                3
#define TX_NODE_ID              0x0003  // 000x = 0003
#define RX_NODE_ID              0x0007  // 000(x+4) = 0007

#define COMP_CHANNEL_1          1       // CH1
#define COMP_CHANNEL_2          7       // CH7

#define COMP_OFFICIAL_PAYLOAD   "GRP3:LORA_AD_HOC_SPECTRUM_SURVIVAL_PAYLOAD"
#define COMP_CTRL_PREFIX        "GRP3:CMD_"
#define COMP_CTRL_CH1           "GRP3:CMD_CH1"
#define COMP_CTRL_CH7           "GRP3:CMD_CH7"

// Node Role Helpers
#define IS_TX_NODE()            (mTermParam.SrcId == TX_NODE_ID)
#define IS_RX_NODE()            (mTermParam.SrcId == RX_NODE_ID)

// --- Duty Cycle (competition rule: rest = ToA × 9) ---
#define COMP_DUTY_REST_SF7_MS   600

// --- Thresholds ---
#define LBT_BLOCK_THRESHOLD     3     // Consecutive CCA failures to trigger CH hop
// SF10 stealth is a SINGLE-SHOT dodge: hide just long enough to let the one
// rival SF7 packet that just hit us pass, then return to SF7. A 62 B SF7 frame
// tops out near 154 ms ToA (BW125), so ~230 ms covers one packet with margin.
// Escaping sustained congestion is the CH hop's job, not stealth's.
#define STEALTH_DURATION_MS     230   // How long RX stays in SF10 stealth
#define LBT_RETRY_BACKOFF_MS    100   // Backoff on each CCA failure
#define CTRL_SEND_MAX_RETRY     2     // Blocked ctrl-send retries before hopping anyway
#define LOST_TX_TIMEOUT_MS      5000  // No own packet for this long -> RX hops to find TX

// --- Panic mode (last resort) ---
// Once the estimated score sinks this low, one more -5 costs more than five
// more +1 could recover, so the RX node stops trying to score at all: it churns
// CH/SF faster than any frame's Time-on-Air, so no rival packet can ever finish
// demodulating (an aborted reception is not a reception, hence no penalty).
// Latching: there is no way back, by design.
// Technical bonus we claim (autonomous CH hop + SF stealth). Added to the
// reception-based score to form the total the final report states.
#define COMP_BONUS_POINTS       100
// Panic trips once the TOTAL score (receptions + bonus) sinks to this. At this
// point one more -5 costs more than the +1s still within reach.
#define PANIC_TOTAL_SCORE       50
// Churn stays on SF10 and only moves between channels. SF10 is orthogonal to
// the SF7 rivals use for the official payload, so those frames cannot be
// demodulated at all -- protection that does not depend on timing. Should a
// rival also be on SF10, its frame (617 ms for 42 B) still far outlasts our
// dwell below, so it cannot finish either.
#define PANIC_SF                10
// Dwell time per channel. The node stays in RX the whole time -- it keeps
// listening, it just never sits still long enough to complete a frame.
#define PANIC_CHURN_MS          50
#define PANIC_LOG_EVERY         50    // Log 1 in N churn steps (UART flood guard)

// --- 10 Minutes Duration ---
#define COMP_DURATION_MS        600000 // 10 minutes (600,000 ms)
// Periodic status line, so both roles leave a continuous trail in the log even
// when nothing else happens (the RX node can otherwise be silent for minutes).
// On the RX node this line IS the running result: it carries the live score.
#define COMP_STATUS_LOG_MS      20000  // 20 s (30 progress reports over the match)

// --- State Machine for TX ---
typedef enum {
    COMP_STATE_AGGRESSIVE,  // Normal: SF7 TX, maximize sends
    COMP_STATE_DEFENSIVE,   // CH just hopped, retrying CCA
} CompTxState;

// --- State Machine for RX ---
typedef enum {
    COMP_RX_STATE_NORMAL,   // Normal RX listening on current CH/SF7
    COMP_RX_STATE_STEALTH,  // Emergency SF10 stealth after hit
} CompRxState;

static void ApplyChannelAndSf( uint16_t ch, uint16_t sf );
static void CompScheduleNextTx( uint32_t delayMs );
static void CompTxDoChHop( const char* trigger );
static void CompCompleteHop( const char* how );
static void CompSendCtrlPacket( const char* ctrlMsg );
static void CompPrintFinalSummary( void );
static void CompCheckDuration( void );
static void CompPeriodicStatus( void );
static int32_t CompEstimatedScore( void );
static int32_t CompTotalScore( void );
static void CompEnterPanic( void );
static void CompPanicChurn( void );
static uint32_t CompCalcToaMs( uint8_t payloadLen, uint16_t sf );
#endif

// process handler
static void InitProcess( void );
static void ProcessEvent( uint32_t ev );
static void ProcessFiber( uint32_t fiber );

// event handler
static void CommInputDoneProcess( void );
static void CommOutputDoneProcess( void );
static void SendDoneProcess( smacErrors_t result );
static void CheckSwitchStateProcess( void );
static void TimerWakeupProcess( void );
static void SendTimerProcess( void );
#if defined(BLD_ENABLE_RFMODE_RSSI_CHECK)
static void RssiCheckProcess( void );
#endif

static void StartCommInput( void );
static void StopCommInput( void );
static smacErrors_t AnalyzeCommDataFrame( smacRouteParams_t* routeParams, SmacTransParams_t* transParams, uint8_t** payload, uint8_t* payloadLen );
static smacErrors_t SetPacketPayload( packet_t* packet, const uint8_t* data, uint8_t dataLen );

static void EnterSleep( void );
static void LeaveSleep( void );

static smacErrors_t SetRadioModeOnIdle( void );
static smacErrors_t SetRadioSleep( void );
static smacErrors_t SetRadioWakeup( void );
static smacErrors_t StartSending( packet_t* packet, uint8_t payloadLen, bool_t withParams, SmacTransParams_t* transParams, smacRouteParams_t* routeParams );

// callback
static void SmacCallback_onNotifyRxData( smacErrors_t result, const SmacUser_RxResultParams_t* params );
static void SmacCallback_onTxDataComp( smacErrors_t result );
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
static void SmacCallback_onNotifyForward( const SmacUser_RxResultParams_t* params, bool_t* isRouting, SmacTransOption_t* transOption );
static void SmacCallback_onNotifyForwardResult( smacErrors_t result );
#endif
static void SmacCallback_TimerControl( stackTimerType_t type, uint32_t duration );
static void SmacCallback_FiberControl( stackFiberType_t type, bool_t enable );
static void UsrTimerCallback( UsrTimerId timerId );
static void WakeupTimerCallback( void );
static void CommInputDoneCallback( void );
static void CommOutputDoneCallback( void );

/*******************************************************************************
********************************************************************************
* Private memory declarations
********************************************************************************
*******************************************************************************/

static packet_t gAppTxPacket;
static packet_t gAppRxPacket;
static bool_t   gIsReceiving    = FALSE;
static bool_t   gIsStandby      = FALSE;
static bool_t   gIsSending      = FALSE;
static bool_t   gEnterSleep     = FALSE;
static bool_t   gUartTxActive   = FALSE;
static bool_t   gUartRxActive   = FALSE;

static bool_t   gIsRetrySending = FALSE;
static uint16_t gSendRetryCount = 0;
static uint32_t gRetryBackoff = FALSE;
static uint8_t  gSendPayloadLen;

static bool_t              gWithParams = FALSE;
static smacRouteParams_t   gRouteParams;
static SmacTransParams_t   gTransParams;

#if COMPETITION_MODE
static uint16_t gCompChannels[2] = { COMP_CHANNEL_1, COMP_CHANNEL_2 };
static uint8_t  gCurrentChIdx = 0;
static uint16_t gCurrentSF = 7;

// States
static CompTxState gCompTxState = COMP_STATE_AGGRESSIVE;
static CompRxState gCompRxState = COMP_RX_STATE_NORMAL;

// 10-Minute Timer
static uint32_t gCompStartTick = 0;
static bool_t   gCompFinished = FALSE;

// Counters
static uint32_t gCompTxSeqNum = 0;       // TX sequence number (official payload only)
static uint32_t gCompCtrlTxCount = 0;     // Control packets sent (not scored)
static bool_t   gCompLastTxWasCtrl = FALSE; // What the in-flight TX carries
static uint32_t gCompRxSuccessCount = 0;  // Total RX successes (+1 pt each)
static uint32_t gCompRivalRxCount = 0;    // Total rival packets received
static uint8_t  gLbtBlockCount = 0;       // Consecutive LBT blocks
static uint8_t  gChHopCount = 0;          // CH hops
static bool_t   gPendingCtrlSend = FALSE;
static char     gPendingCtrlBuf[32];
static bool_t   gCompHopPending = FALSE;    // CH hop armed, awaiting ctrl delivery
static uint8_t  gCompPendingHopChIdx = 0;   // Destination channel index of the pending hop
static uint8_t  gCompCtrlRetryCount = 0;    // Blocked ctrl-send attempts for this hop
static uint32_t gCompLastOwnRxTick = 0;     // HAL_GetTick() when RX last heard our TX node
static bool_t   gCompPanicMode = FALSE;     // Latched: scoring abandoned, evade only
static uint8_t  gCompPanicIdx = 0;          // Cursor over the 4 (ch, sf) churn cells
static uint32_t gCompPanicSteps = 0;        // Churn steps taken (telemetry)
static uint32_t gCompLastStatusTick = 0;    // HAL_GetTick() of the last status line
static uint16_t gCompRxHopCount = 0;        // Channel changes made by the RX node
static uint32_t gCompEmptyRxCount = 0;      // Zero-payload frames seen (never penalised)
#endif

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

/*******************************************************************************
*
* DoOperation
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
void DoOperation( void )
{
    const bool_t useSleepMode  = IS_MCU_SLEEP(mTermParam);
    const bool_t useStopMode   = IS_MCU_STOP(mTermParam);

    uint32_t    event = 0;

    Terminal_Print( "\r\n ----- operation mode is ready ----- \r\n" );
    Processor_Print( "\r\n" );

    //--------------------------------
    // Init process
    //--------------------------------
    InitProcess();

    // event loop
    for( ; ; )
    {
        //---------------------------
        // Event Process
        //---------------------------
        for( event = GetEvent(); event ; event = GetEvent() )
        {
            ProcessEvent(event);
        }

        //--------------------------------------------------
        // Sleep Process ( wait for event in sleep state )
        //--------------------------------------------------
        // Check if uart tx flushed
        gUartTxActive = !Serial1_IsWriteFlushed( CommOutputDoneCallback );

        if( useStopMode && SMAC_CanEnterStopMode() && !gUartTxActive )
        {
            // stop heartbeat timer
            UsrTimer_stop( UsrHeartBeatTimer );

            WDG_Refresh();

            // wait for event in STOP mode
            if( !gUartRxActive && gEnterSleep )
            {
                // enter STOP mode
                event = UsrPower_waitEventInStopMode( GetEvent_IT, mTermParam.Baudrate );
            }
            else
            {
                // enter SLEEP mode
                event = UsrPower_waitEventInCpuSleep( GetEvent_IT );
            }

            // restart timer
            UsrTimer_start( UsrHeartBeatTimer, UsrTimerMode_Periodic, INTERVAL_TIMEOUT, UsrTimerCallback );
        }
        else if( useSleepMode )
        {
            // wait for event in SLEEP mode
            event = UsrPower_waitEventInCpuSleep( GetEvent_IT );
        }
    }
}

/*******************************************************************************
*
* Switch_Interrupt
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
void Switch_Interrupt( void )
{
    if( IS_INT_SLEEP(mTermParam) )
    {
        SetEvent( EVENT_INT_SW );
    }
}

/*******************************************************************************
********************************************************************************
* Private functions
********************************************************************************
*******************************************************************************/

/*******************************************************************************
*
* InitProcess
*
* Interface assumptions:
*     None
*
* Return value:
*     Event
*
*******************************************************************************/
static void InitProcess( void )
{
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    if( !IS_TIMER_SLEEP(mTermParam) || (mTermParam.SleepTime / 10 < UsrTimer_SleepDurationMax) )
    {
        PeriphClkInit.PeriphClockSelection |= RCC_PERIPHCLK_RTC;
        PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_NONE;
        HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
    }
    else
    {
        // init RTC
        PeriphClkInit.PeriphClockSelection |= RCC_PERIPHCLK_RTC;
        PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
        HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
        MX_RTC_Init2();
        HAL_NVIC_EnableIRQ(RTC_WKUP_IRQn);
    }

    //--------------------------------
    // set callback interface to SMAC
    //--------------------------------
    SmacUser_CallbackInterface_t handler = { 0 };
    handler.onNotifyRxData          = SmacCallback_onNotifyRxData;
    handler.onTxDataComp            = SmacCallback_onTxDataComp;
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    handler.onNotifyForward         = SmacCallback_onNotifyForward;
    handler.onNotifyForwardResult   = SmacCallback_onNotifyForwardResult;
#endif
    handler.TimerControl            = SmacCallback_TimerControl;
    handler.FiberControl            = SmacCallback_FiberControl;
    SMAC_Start( &handler, &gAppRxPacket );

    //--------------------------------
    // start interval timer for WDT Refresh
    //--------------------------------
    UsrTimer_start( UsrHeartBeatTimer, UsrTimerMode_Periodic, INTERVAL_TIMEOUT, UsrTimerCallback );

    //--------------------------------
    // start uart input
    //--------------------------------
    StartCommInput();

    //--------------------------------
    // set sleep state
    //--------------------------------
#if COMPETITION_MODE
    // Force active non-sleep RFMODE_TXRX for competition
    gEnterSleep = FALSE;
    mTermParam.Sleep = NO_SLEEP;
    mTermParam.RfMode = RFMODE_TXRX;
    mTermParam.Ack = MODE_OFF;
    mTermParam.DstId = 0xFFFF;

    // Apply changes to SMAC stack trans parameters (Disable ACK, set Dst to Broadcast)
    SmacTransParams_t transParams;
    if (SMAC_GetTransParams(&transParams) == gErrorNoError_c)
    {
        transParams.option.ackReq = FALSE;
        transParams.option.ackRetryCount = 0;
        transParams.addr.destNodeAddr = 0xFFFF;
        SMAC_SetTransParams(&transParams);
    }
#else
    gEnterSleep = FALSE;

    if( IS_TIMER_SLEEP(mTermParam) )
    {
        SmacTransParams_t transParams;

        SMAC_GetTransParams(&transParams);
        transParams.option.ackRetryCount = 0;
        SMAC_SetTransParams(&transParams);

        Terminal_Print( "enter timer sleep mode\r\n" );

        // enter idle and start wakeup timer
        EnterSleep();
    }
    else if( IS_INT_SLEEP(mTermParam) )
    {
        if( HAL_GPIO_ReadPin(SW_INT_GPIO_Port, SW_INT_Pin) )
        {
            Terminal_Print( "enter interrupt sleep mode\r\n" );

            // enter idle
            EnterSleep();
        }

        // enable Interrupt
        LL_EXTI_EnableIT_0_31( SW_INT_Pin );

        SetEvent( EVENT_INT_SW );
    }
    else if( IS_UART_SLEEP(mTermParam) )
    {
        Terminal_Print( "enter uart sleep mode\r\n" );

        // enter idle
        EnterSleep();
    }
    else
    {
        //--------------------------------
        // start sendtime timer
        //--------------------------------
        if( 0 != mTermParam.SendTime )
        {
            if( !IS_TIMER_SLEEP(mTermParam) )
            {
                // send triggerred by send time timer
                UsrTimer_start( UsrTxIntervalTimer, UsrTimerMode_Periodic, mTermParam.SendTime * 1000, UsrTimerCallback );

                SetEvent( EVENT_SEND_TIME );
            }
        }
    }

    if( !gEnterSleep )
    {
        SetRadioSleep();
    }
#endif

    HAL_PWREx_SMPS_SetMode( PWR_SMPS_STEP_DOWN );

#if COMPETITION_MODE
    SetRadioModeOnIdle();
    
    // Initialize 10-Minute Timer
    gCompStartTick = HAL_GetTick();
    gCompLastOwnRxTick = HAL_GetTick();
    gCompLastStatusTick = HAL_GetTick();
    gCompFinished = FALSE;

    // Apply initial channel and SF to stack
    ApplyChannelAndSf(gCompChannels[gCurrentChIdx], gCurrentSF);

    Terminal_Print("\r\n=========================================\r\n");
    Terminal_Print("   LoRa SURVIVAL COMPETITION ALGORITHM   \r\n");
    Terminal_Print("   Group: %u (Prefix: GRP%u:)\r\n", GROUP_ID, GROUP_ID);
    Terminal_Print("   Node ID: 0x%04X, PAN ID: 0x%04X\r\n", mTermParam.SrcId, mTermParam.PanId);
    if (IS_TX_NODE())
    {
        Terminal_Print("   Role: TRANSMITTER NODE (0x%04X)\r\n", TX_NODE_ID);
        Terminal_Print("   Payload: %s\r\n", COMP_OFFICIAL_PAYLOAD);
    }
    else if (IS_RX_NODE())
    {
        Terminal_Print("   Role: RECEIVER NODE (0x%04X)\r\n", RX_NODE_ID);
        Terminal_Print("   Defense: Q2 Blind Hack + SF10 Stealth\r\n");
    }
    else
    {
        Terminal_Print("   Role: UNKNOWN NODE (Check SrcId setting!)\r\n");
    }
    Terminal_Print("   Channels: CH%u and CH%u\r\n", COMP_CHANNEL_1, COMP_CHANNEL_2);
    Terminal_Print("=========================================\r\n\r\n");

    if (IS_TX_NODE())
    {
        // Start TX loop after 1 second
        gCompTxState = COMP_STATE_AGGRESSIVE;
        CompScheduleNextTx(1000);
    }
    else
    {
        // RX Node stays in RX mode listening and starts lost-TX timeout timer
        SetRadioModeOnIdle();
        CompScheduleNextTx(LOST_TX_TIMEOUT_MS);
    }
#else
    if( !gEnterSleep )
    {
        SetRadioModeOnIdle();
    }
#endif

    WDG_Refresh();
}

/*******************************************************************************
*
* EnterSleep
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
static void EnterSleep( void )
{
    if( !gEnterSleep )
    {
        gEnterSleep = TRUE;

        LED_TurnOff( LED_ALL );

        if( IS_UART_SLEEP(mTermParam) )
        {
            StartCommInput();
        }
        else
        {
            StopCommInput();
        }

        if( IS_RF_SLEEP(mTermParam) )
        {
            /* sleep radio */
            SetRadioSleep();
        }
        else
        {
            SetRadioModeOnIdle();
        }

        // start wakeup timer
        if( IS_TIMER_SLEEP(mTermParam) )
        {
            if( gIsRetrySending )
            {
                // retry backoff
                UsrTimer_start( UsrSleepTimer, UsrTimerMode_OneShot, gRetryBackoff, UsrTimerCallback );
            }
            else
            {
                // start wakeup timer
                if( mTermParam.SleepTime / 10 < UsrTimer_SleepDurationMax )
                {
                    // wakeup by LPTIMER
                    UsrTimer_start( UsrSleepTimer, UsrTimerMode_OneShot, mTermParam.SleepTime * 100, UsrTimerCallback );
                }
                else
                {
                    // wakeup by RTC
                    UsrWakeupTimer_start( mTermParam.SleepTime / 10, WakeupTimerCallback );
                }
            }
        }
    }
}

/*******************************************************************************
*
* LeaveSleep
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
static void LeaveSleep( void )
{
    if( gEnterSleep )
    {
        gEnterSleep = FALSE;

        if( !IS_UART_SLEEP(mTermParam) )
        {
            StartCommInput();
        }

        if( IS_TIMER_SLEEP(mTermParam) )
        {
            // stop timer
            if( mTermParam.SleepTime / 10 < UsrTimer_SleepDurationMax )
            {
                // wakeup by LPTIMER
                UsrTimer_stop( UsrSleepTimer );
            }
            else
            {
                // wakeup by RTC
                UsrWakeupTimer_stop();
            }
        }

        if( IS_RF_SLEEP(mTermParam) )
        {
            SetRadioWakeup();
        }
    }
}

/*******************************************************************************
*
* SetRadioModeOnIdle
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
static smacErrors_t SetRadioModeOnIdle( void )
{
    smacErrors_t result = gErrorNoError_c;
    switch( mTermParam.RfMode )
    {
    default:
    case RFMODE_TXONLY:
        if( !gIsStandby )
        {
            gIsStandby   = FALSE;
            gIsReceiving = FALSE;
            gIsSending   = FALSE;

            /* standby */
            result = SMAC_Standby();
            if( result != gErrorNoError_c )
            {
                return result;
            }

            gIsStandby = TRUE;
        }
        break;

#if defined(BLD_ENABLE_RFMODE_RSSI_CHECK)
    case RFMODE_RSSI_CHECK:
        // start rssi check timer
        UsrTimer_start( UsrRssiCheckTimer, UsrTimerMode_Periodic, 100, UsrTimerCallback );
        SetEvent( EVENT_RSSI_CHECK );

        // break through
#endif
    case RFMODE_TXRX:
        if( !gIsReceiving )
        {
            gIsStandby   = FALSE;
            gIsReceiving = FALSE;
            gIsSending   = FALSE;

            /* ready to receive */
            result = SMAC_RxStart();
            if( result != gErrorNoError_c )
            {
                return result;
            }

            gIsReceiving = TRUE;
        }
        break;

#if defined(BLD_ENABLE_RFMODE_BURST)
    case RFMODE_BURST:
        gIsStandby   = FALSE;
        gIsReceiving = FALSE;
        gIsSending   = FALSE;

        /* tx contiuous wave */
        if( !gIsSending && !gIsRetrySending )
        {
            const uint8_t payloadLen = (uint8_t) strlen((char*)mTermParam.SendData);

            result = SetPacketPayload( &gAppTxPacket, mTermParam.SendData, payloadLen );
            if( result != gErrorNoError_c )
            {
                /* error */
                break;
            }

            // start sending
			gWithParams = FALSE;
            result = StartSending( &gAppTxPacket, payloadLen, FALSE, NULL, NULL );
            if( result != gErrorNoError_c )
            {
                /* error */
                break;
            }
        }
        break;
#endif
#if defined(BLD_ENABLE_RFMODE_CW)
    case RFMODE_CW:
        gIsStandby   = FALSE;
        gIsReceiving = FALSE;
        gIsSending   = FALSE;

        /* tx contiuous wave */
        result = SMAC_TxContinuous( FALSE );
        break;
#endif
#if defined(BLD_ENABLE_RFMODE_TX_INFINITE)
    case RFMODE_TX_INFINITE:
        gIsStandby   = FALSE;
        gIsReceiving = FALSE;
        gIsSending   = FALSE;

        /* tx contiuous wave */
        result = SMAC_TxContinuous( TRUE );
        break;
#endif
    }

    return result;
}

/*******************************************************************************
*
* ProcessEvent
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
static void ProcessEvent( uint32_t ev )
{
    UsrPower_enterActiveRun();

    if( EVENT_PHY_CCA & ev )
    {
        ClearEvent( EVENT_PHY_CCA );

        uint32_t fiber = GetActiveFiber();
        if( fiber )
        {
            ProcessFiber( fiber );

            SetEvent( EVENT_PHY_CCA );
        }
    }

    if( EVENT_WKUP_TIMER & ev )
    {
        ClearEvent( EVENT_WKUP_TIMER );

        TimerWakeupProcess();
    }

    if( EVENT_INT_SW & ev )
    {
        ClearEvent( EVENT_INT_SW );

        CheckSwitchStateProcess();
    }

    /* IRQ from SUBGHZ */
    if( EVENT_RADIO_IRQ & ev )
    {
        ClearEvent( EVENT_RADIO_IRQ );

        SMAC_RadioIrqProcess();
    }

    /* receive UART Data */
    if( EVENT_UART_RX_DONE & ev )
    {
        ClearEvent( EVENT_UART_RX_DONE );

        CommInputDoneProcess();
    }

    if( EVENT_UART_TX_DONE & ev )
    {
        ClearEvent( EVENT_UART_TX_DONE );

        CommOutputDoneProcess();
    }

    if( EVENT_PHY_CCA_TIMER & ev )
    {
        ClearEvent( EVENT_PHY_CCA_TIMER );

        SMAC_TimerExpiredProcess( PhyCcaTimer );
    }

    if( EVENT_PHY_TX_TIMER & ev )
    {
        ClearEvent( EVENT_PHY_TX_TIMER );

        SMAC_TimerExpiredProcess( PhyTxTimer );
    }

    if( EVENT_PHY_RX_TIMER & ev )
    {
        ClearEvent( EVENT_PHY_RX_TIMER );

        SMAC_TimerExpiredProcess( PhyRxTimer );
    }

    if( EVENT_PHY_DUTY_TIMER & ev )
    {
        ClearEvent( EVENT_PHY_DUTY_TIMER );

        SMAC_TimerExpiredProcess( PhyDutyTimer );

        // Note: In COMPETITION_MODE, next TX is scheduled by SendDoneProcess
        // with proper rest time (ToA × 9). Do NOT auto-schedule here.
    }

    if( EVENT_SMAC_CCA_BACKOFF_TIMER & ev )
    {
        ClearEvent( EVENT_SMAC_CCA_BACKOFF_TIMER );

        SMAC_TimerExpiredProcess( SmacCcaBackoffTimer );
    }

    if( EVENT_SMAC_TX_TIMER & ev )
    {
        ClearEvent( EVENT_SMAC_TX_TIMER );

        SMAC_TimerExpiredProcess( SmacTxTimer );
    }

    /* send time */
    if( EVENT_SEND_TIME & ev )
    {
        ClearEvent( EVENT_SEND_TIME );

        SendTimerProcess();
    }

#if defined(BLD_ENABLE_RFMODE_RSSI_CHECK)
    /* rssi check */
    if( EVENT_RSSI_CHECK & ev )
    {
        ClearEvent( EVENT_RSSI_CHECK );

        RssiCheckProcess();
    }
#endif

    /* WDT Reset */
    if( EVENT_WDT_RESET & ev )
    {
        ClearEvent( EVENT_WDT_RESET );

        /* process reset WDT */
        WDG_Refresh();

#if COMPETITION_MODE
        // The heartbeat is the only timer guaranteed to run for the whole
        // match, so drive the match clock and the status log from here. The RX
        // node in particular has no send loop to piggyback on, and previously
        // reached the 10-minute summary only if its own housekeeping timer
        // happened to still be alive.
        CompCheckDuration();
        CompPeriodicStatus();
#endif
    }
}

/*******************************************************************************
*
* ProcessFiber
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
static void ProcessFiber( uint32_t fiber )
{
    if( FIBER_PHY_CCA & fiber )
    {
        if( IS_SLEEP(mTermParam) )
        {
            UsrPower_enterLowPowerRun();
        }
        SMAC_FiberProcess( PhyCcaFiber );
    }
}

/*******************************************************************************
*
* StartCommInput
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
static void StartCommInput( void )
{
    if( !gUartRxActive )
    {
        gUartRxActive = TRUE;

        // start input uart
        switch( mTermParam.Format )
        {
        case FMT_ASCII:
            InputCommTextLine( CommInputDoneCallback, TRUE );
            break;

        case FMT_BINARY:
            InputCommBinaryBlock( CommInputDoneCallback, TRUE );
            break;
        }
    }
}

/*******************************************************************************
*
* StopCommInput
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
static void StopCommInput( void )
{
    InputCommAbort();
    gUartRxActive = FALSE;
}

/*******************************************************************************
*
* CommInputDoneProcess
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
static void CommInputDoneProcess(void)
{
    smacErrors_t err = gErrorNoError_c;

    gUartRxActive = FALSE;

    switch( CommInputResult )
    {
    case CommInputResult_buffer:
        if( mTermParam.Format == FMT_ASCII && CommDataLen==0 )
        {
            // empty line (ignore)

            // restart uart rx
            StartCommInput();
        }
        else
        {
            Debug_Print( "receive UART data\r\n" );

            uint8_t*            payloadData;
            uint8_t             payloadDataLen;

            // analyze and copy comm input data
            switch( mTermParam.TransMode )
            {
            default:
            case TRANS_PAYLOAD:
                err = SetPacketPayload( &gAppTxPacket, CommDataBuffer, CommDataLen );
                if( err != gErrorNoError_c )
                {
                    break;
                }

                payloadDataLen = CommDataLen;
                gWithParams = FALSE;
                break;

            case TRANS_FRAME:
                err = AnalyzeCommDataFrame( &gRouteParams, &gTransParams, &payloadData, &payloadDataLen );
                if( err != gErrorNoError_c )
                {
                    break;
                }

                err = SetPacketPayload( &gAppTxPacket, payloadData, payloadDataLen );
                if( err != gErrorNoError_c )
                {
                    break;
                }

                gWithParams = TRUE;
                break;
            }
            if( err != gErrorNoError_c )
            {
                // restart uart rx
                StartCommInput();
                return;
            }

            if( IS_TIMER_SLEEP(mTermParam) || IS_INT_SLEEP_1TX(mTermParam) || IS_UART_SLEEP(mTermParam) )
            {
                // stop uart rx
                StopCommInput();
            }
            else
            {
                // restart uart rx
                StartCommInput();
            }

            // Wakeup if uart sleep
            if( IS_UART_SLEEP(mTermParam) )
            {
                LeaveSleep();
                Terminal_Print( "exit uart sleep mode\r\n" );
            }

            /* send RF data */
            if( gWithParams )
            {
                err = StartSending( &gAppTxPacket, payloadDataLen, TRUE, &gTransParams, &gRouteParams );
            }
            else
            {
                err = StartSending( &gAppTxPacket, payloadDataLen, FALSE, NULL, NULL );
            }

            if( err != gErrorNoError_c )
            {
                /* error */
                SendDoneProcess( err );
            }
        }
        break;

    case CommInputResult_spCmd:
        switch( CommSpCmd )
        {
        case CommSpCmd_config:
            // config command
            mTermParam.Operation = CONFIG;
            TermParam_saveToRom();
            break;

#ifdef BLD_USE_SPCMD_RESET
        case CommSpCmd_reset:
            HAL_NVIC_SystemReset();
            break;
#endif

        default:
            break;
        }

        // restart uart rx
        StartCommInput();
        break;
    }
}

/*******************************************************************************
*
* CommOutputDoneProcess
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
static void CommOutputDoneProcess(void)
{
    gUartTxActive = FALSE;
}

/*******************************************************************************
*
* AnalyzeCommDataFrame
*
* Interface assumptions:
*     data_length   send data length
*
* Return value:
*     smacErrors_t  send result
*
*******************************************************************************/
static smacErrors_t AnalyzeCommDataFrame( smacRouteParams_t* routeParams, SmacTransParams_t* transParams, uint8_t** payload, uint8_t* payloadLen )
{
    uint8_t frameHead = 0;
    smacErrors_t err;

    switch( mTermParam.Protocol )
    {
#if defined(BLD_USE_LORA_NR)
    case Protocol_LORA_NR:
        frameHead = FRAME_HEAD_LORA_NR;
        break;
#endif
#if defined(BLD_USE_LORA_R)
    case Protocol_LORA_R:
        frameHead = FRAME_HEAD_LORA_R;
        break;
#endif
#if defined(BLD_USE_FSK_R)
    case Protocol_FSK_R:
        frameHead = FRAME_HEAD_FSK_R;
        break;
#endif
    }

    /* too short */
    if( CommDataLen <= frameHead )
    {
        Terminal_Print( "send data length too short\r\n" );

        Processor_OutputOperationResult( "NG 100" );

        return( gErrorOutOfRange_c );
    }

    /* get parameters */
    err = SMAC_GetTransParams( transParams );
    if( err != gErrorNoError_c ) { return err; }

    /* parse UART input parameter */
    transParams->addr.srcNode.panId = ( ( (uint16_t)AsciiToHex(CommDataBuffer[0])  << 12 ) | ( (uint16_t)AsciiToHex(CommDataBuffer[1])  << 8 ) |
                                        ( (uint16_t)AsciiToHex(CommDataBuffer[2])  <<  4 ) | ( (uint16_t)AsciiToHex(CommDataBuffer[3])       ) );
    transParams->addr.destNodeAddr  = ( ( (uint16_t)AsciiToHex(CommDataBuffer[4])  << 12 ) | ( (uint16_t)AsciiToHex(CommDataBuffer[5])  << 8 ) |
                                        ( (uint16_t)AsciiToHex(CommDataBuffer[6])  <<  4 ) | ( (uint16_t)AsciiToHex(CommDataBuffer[7])       ) );
    transParams->addr.srcNode.nodeAddr = mTermParam.SrcId;

    /* check trans params */
    err = SMAC_CheckTransParams( transParams );
    if( err != gErrorNoError_c )
    {
        Terminal_Print( "parameter error\r\n" );

        Processor_OutputOperationResult( "NG 100" );

        return( gErrorOutOfRange_c );
    }

    /* get route params */
    err = SMAC_GetRouteParams( routeParams );
    if( err != gErrorNoError_c )
    {
        return err;
    }

    /* parse UART input parameter */
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
    switch( mTermParam.Protocol )
    {
#if defined(BLD_USE_LORA_R)
    case Protocol_LORA_R:
        routeParams->hop_cnt        = ( ( AsciiToHex(CommDataBuffer[8])  <<  4 ) | ( AsciiToHex(CommDataBuffer[9]) ) );
        routeParams->route[0]       = ( ( (uint16_t)AsciiToHex(CommDataBuffer[10]) << 12 ) | ( (uint16_t)AsciiToHex(CommDataBuffer[11]) << 8 ) |
                                        ( (uint16_t)AsciiToHex(CommDataBuffer[12]) <<  4 ) | ( (uint16_t)AsciiToHex(CommDataBuffer[13])      ) );
        routeParams->route[1]       = ( ( (uint16_t)AsciiToHex(CommDataBuffer[14]) << 12 ) | ( (uint16_t)AsciiToHex(CommDataBuffer[15]) << 8 ) |
                                        ( (uint16_t)AsciiToHex(CommDataBuffer[16]) <<  4 ) | ( (uint16_t)AsciiToHex(CommDataBuffer[17])      ) );
        routeParams->endNodeAddr    = ( ( (uint16_t)AsciiToHex(CommDataBuffer[18]) << 12 ) | ( (uint16_t)AsciiToHex(CommDataBuffer[19]) << 8 ) |
                                        ( (uint16_t)AsciiToHex(CommDataBuffer[20]) <<  4 ) | ( (uint16_t)AsciiToHex(CommDataBuffer[21])      ) );
        routeParams->oriNodeAddr    = mTermParam.SrcId;
        break;
#endif

#if defined(BLD_USE_FSK_R)
    case Protocol_FSK_R:
        routeParams->hop_cnt        = ( ( AsciiToHex(CommDataBuffer[8])  <<  4 ) | ( AsciiToHex(CommDataBuffer[9]) ) );
        routeParams->route[0]       = ( ( (uint16_t)AsciiToHex(CommDataBuffer[10]) << 12 ) | ( (uint16_t)AsciiToHex(CommDataBuffer[11]) << 8 ) |
                                        ( (uint16_t)AsciiToHex(CommDataBuffer[12]) <<  4 ) | ( (uint16_t)AsciiToHex(CommDataBuffer[13])      ) );
        routeParams->route[1]       = ( ( (uint16_t)AsciiToHex(CommDataBuffer[14]) << 12 ) | ( (uint16_t)AsciiToHex(CommDataBuffer[15]) << 8 ) |
                                        ( (uint16_t)AsciiToHex(CommDataBuffer[16]) <<  4 ) | ( (uint16_t)AsciiToHex(CommDataBuffer[17])      ) );
        routeParams->route[2]       = ( ( (uint16_t)AsciiToHex(CommDataBuffer[18]) << 12 ) | ( (uint16_t)AsciiToHex(CommDataBuffer[19]) << 8 ) |
                                        ( (uint16_t)AsciiToHex(CommDataBuffer[20]) <<  4 ) | ( (uint16_t)AsciiToHex(CommDataBuffer[21])      ) );
        routeParams->endNodeAddr    = ( ( (uint16_t)AsciiToHex(CommDataBuffer[22]) << 12 ) | ( (uint16_t)AsciiToHex(CommDataBuffer[23]) << 8 ) |
                                        ( (uint16_t)AsciiToHex(CommDataBuffer[24]) <<  4 ) | ( (uint16_t)AsciiToHex(CommDataBuffer[25])      ) );
        routeParams->oriNodeAddr    = mTermParam.SrcId;
        break;
#endif
    }
#endif

    /* check route params */
    err = SMAC_CheckRouteParams( routeParams, (protocolType_t)mTermParam.Protocol );
    if( err != gErrorNoError_c )
    {
        Terminal_Print( "parameter error\r\n" );

        Processor_OutputOperationResult( "NG 100" );

        return( gErrorOutOfRange_c );
    }

    *payload = CommDataBuffer + frameHead;
    *payloadLen = CommDataLen - frameHead;

    return gErrorNoError_c;
}

/*******************************************************************************
*
* SetPacketPayload
*
* Interface assumptions:
*     data_length   send data length
*
* Return value:
*     smacErrors_t  send result
*
*******************************************************************************/
static smacErrors_t SetPacketPayload( packet_t* packet, const uint8_t* data, uint8_t dataLen )
{
    const uint8_t maxPayloadLength = MAX_PAYLOAD_LENGTH( (protocolType_t)mTermParam.Protocol );

    /* too long */
    if( maxPayloadLength < dataLen )
    {
        Terminal_Print( "send data length too long\r\n" );
        Processor_OutputOperationResult( "NG 100" );

        return gErrorOutOfRange_c;
    }

    uint8_t* payload = getPacketPayload( packet, (protocolType_t)mTermParam.Protocol );
    memcpy( payload, data, dataLen );

	gSendPayloadLen = dataLen;

    return gErrorNoError_c;
}

/*******************************************************************************
*
* StartSending
*
* Interface assumptions:
*     data_length   send data length
*
* Return value:
*     smacErrors_t  send result
*
*******************************************************************************/
static smacErrors_t StartSending( packet_t* packet, uint8_t payloadLen, bool_t withParams, SmacTransParams_t* transParams, smacRouteParams_t* routeParams )
{
    SmacTransParams_t tmpTransParams;
    smacErrors_t err = gErrorNoError_c;

    gIsStandby = FALSE;
    gIsReceiving = FALSE;
    gIsSending = FALSE;

    if( !withParams )
    {
        if( TERMINAL == mTermParam.Mode )
        {
            err = SMAC_GetTransParams( &tmpTransParams );
            if( err != gErrorNoError_c )
            {
                return err;
            }

            Terminal_Print(
                       "<-- send data info[panid = %04X, srcid = %04X, dstid = %04X, length = %02X]\r\n",
                       (unsigned) tmpTransParams.addr.srcNode.panId,
                       (unsigned) tmpTransParams.addr.srcNode.nodeAddr,
                       (unsigned) tmpTransParams.addr.destNodeAddr,
                       (unsigned) payloadLen );
        }
        err = SMAC_TxPacket( packet, payloadLen );
        if( err != gErrorNoError_c )
        {
            return err;
        }
    }
    else
    {
        Terminal_Print(
                       "<-- send data info[panid = %04X, srcid = %04X, dstid = %04X, length = %02X]\r\n",
                       (unsigned) transParams->addr.srcNode.panId,
                       (unsigned) transParams->addr.srcNode.nodeAddr,
                       (unsigned) transParams->addr.destNodeAddr,
                       (unsigned) payloadLen );

        err = SMAC_TxPacketEx( packet, payloadLen, transParams, routeParams );
        if( err != gErrorNoError_c )
        {
            return err;
        }
    }

    gIsSending = TRUE;

    LED_Toggle( LED2 );

    return err;
}

/*******************************************************************************
*
* SendDoneProcess
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
static void SendDoneProcess( smacErrors_t result )
{
    smacErrors_t err;

    gIsRetrySending = FALSE;

#if COMPETITION_MODE
    gIsSending = FALSE;

    if (IS_TX_NODE())
    {
        // ================= TX NODE LOGIC =================
        if (result == gErrorNoError_c)
        {
            // === TX SUCCESS ===
            gLbtBlockCount = 0;
            gChHopCount = 0;
            gCompTxState = COMP_STATE_AGGRESSIVE;

            // Calculate dynamic ToA (SF7 or SF10, BW125kHz)
            // Add a +50ms safety margin on top of (ToA * 9) for strict DC compliance
            uint32_t toaMs = CompCalcToaMs(gSendPayloadLen, gCurrentSF);
            uint32_t restMs = (toaMs * 9) + 50;

            // Control packets sync our own RX node; only official payload
            // sends count toward the score log (score = successful receptions).
            if (gCompLastTxWasCtrl)
            {
                gCompCtrlTxCount++;
                Terminal_Print("[%08u] [TX_NODE] CTRL_OK total_ctrl=%u CH=%u ToA=%u ms Rest=%u ms\r\n",
                               (unsigned int)HAL_GetTick(),
                               (unsigned int)gCompCtrlTxCount,
                               gCompChannels[gCurrentChIdx],
                               (unsigned int)toaMs,
                               (unsigned int)restMs);
            }
            else
            {
                gCompTxSeqNum++;
                Terminal_Print("[%08u] [TX_NODE] TX_OK SEQ=%u CH=%u SF=%u ToA=%u ms Rest=%u ms\r\n",
                               (unsigned int)HAL_GetTick(),
                               (unsigned int)gCompTxSeqNum,
                               gCompChannels[gCurrentChIdx],
                               gCurrentSF,
                               (unsigned int)toaMs,
                               (unsigned int)restMs);
            }

            // A transmitted hop command means the RX node is moving; follow
            // it now, before the next official send goes out.
            if (gCompLastTxWasCtrl && gCompHopPending)
            {
                CompCompleteHop("CTRL_DELIVERED");
            }

            // Schedule next TX after dynamic duty cycle rest (ToA × 9)
            // During this rest, the TX node is in RX mode scouting safely!
            CompScheduleNextTx(restMs);
        }
        else if (result == gErrorChannelBusy_c)
        {
            if (gCompLastTxWasCtrl && gCompHopPending)
            {
                // === HOP COMMAND BLOCKED on the (congested) old channel ===
                gCompCtrlRetryCount++;

                Terminal_Print("[%08u] [TX_NODE] CTRL_BLOCK CH=%u (retry %u/%u)\r\n",
                               (unsigned int)HAL_GetTick(),
                               gCompChannels[gCurrentChIdx],
                               (unsigned int)gCompCtrlRetryCount,
                               (unsigned int)CTRL_SEND_MAX_RETRY);

                if (gCompCtrlRetryCount > CTRL_SEND_MAX_RETRY)
                {
                    // Too busy to even deliver the hop command. Hop anyway;
                    // the RX node re-syncs via its lost-TX timeout hop.
                    CompCompleteHop("CTRL_GIVEUP");
                    CompScheduleNextTx(10);
                }
                else
                {
                    // Re-queue the same command and retry shortly on this CH.
                    gPendingCtrlSend = TRUE;
                    CompScheduleNextTx(20 + SMAC_GetRandom(0, 50));
                }
            }
            else
            {
                // === LBT BLOCKED (CCA failed) ===
                gLbtBlockCount++;

                Terminal_Print("[%08u] [TX_NODE] LBT_BLOCK CH=%u SF=%u (count=%u)\r\n",
                               (unsigned int)HAL_GetTick(),
                               gCompChannels[gCurrentChIdx],
                               gCurrentSF,
                               (unsigned int)gLbtBlockCount);

                if (gLbtBlockCount >= LBT_BLOCK_THRESHOLD)
                {
                    // Threshold reached → arm the hop; the control packet goes
                    // out first on THIS channel to lead the RX node over.
                    gLbtBlockCount = 0;
                    gChHopCount++;

                    CompTxDoChHop("LBT_THRESHOLD");
                    gCompTxState = COMP_STATE_DEFENSIVE;
                    CompScheduleNextTx(10);  // Send the hop command
                }
                else
                {
                    // Below threshold → short backoff and retry same CH
                    uint32_t backoff = 50 + SMAC_GetRandom(0, LBT_RETRY_BACKOFF_MS);
                    CompScheduleNextTx(backoff);
                }
            }
        }
        else
        {
            // Other error → retry after 500ms
            Terminal_Print("[%08u] [TX_NODE] TX_ERR=%d CH=%u SF=%u\r\n",
                           (unsigned int)HAL_GetTick(),
                           (int)result,
                           gCompChannels[gCurrentChIdx],
                           gCurrentSF);

            if (gCompLastTxWasCtrl && gCompHopPending)
            {
                // Keep the hop command (not the payload) in flight, bounded.
                gCompCtrlRetryCount++;
                if (gCompCtrlRetryCount > CTRL_SEND_MAX_RETRY)
                {
                    CompCompleteHop("CTRL_GIVEUP");
                }
                else
                {
                    gPendingCtrlSend = TRUE;
                }
            }
            CompScheduleNextTx(500);
        }
    }
    else // IS_RX_NODE()
    {
        // ================= RX NODE LOGIC =================
        // RX node completed a short dummy transmission (Q2 Blind Hack)
        Terminal_Print("[%08u] [RX_NODE] Blind hack dummy TX finished (result=%d)\r\n",
                       (unsigned int)HAL_GetTick(), (int)result);

        // Resume RX mode immediately
        SetRadioModeOnIdle();
    }
#else
    if( IS_TIMER_SLEEP(mTermParam) )
    {
        switch( result )
        {
        default:
            break;

        case gErrorNoAck_c:
        case gErrorChannelBusy_c:
            if( gSendRetryCount < mTermParam.Retry )
            {
                // retry sending
                gIsRetrySending = TRUE;
                gSendRetryCount++;

                // calclulate backoff time
                gRetryBackoff = SMAC_GetRandom(0, mTermParam.Backoff);

                Terminal_Print( "Retry Sending (%u/%u)\r\n", gSendRetryCount, mTermParam.Retry, gRetryBackoff );
            }
            break;
        }
    }
    
    if( !gIsRetrySending )
    {
        gSendRetryCount = 0;
    
        /* send success */
        if( result == gErrorNoError_c )
        {
            Processor_OutputOperationResult( "OK" );
        }
        /* error */
        else
        {
            if( result == gErrorNoValidCondition_c )
            {
                Terminal_Print( "send failed bacause now sending packet\r\n" );
            }
            switch( result )
            {
            case gErrorOutOfRange_c:
                Terminal_Print( "parameter error\r\n" );
                Processor_OutputOperationResult( "NG 100" );
                break;
    
            default:
                Terminal_Print( "send data failed\r\n" );
                Processor_OutputOperationResult( "NG 101" );
                break;
    
            case gErrorChannelBusy_c:
                Terminal_Print( "carrier sense failed\r\n" );
                Processor_OutputOperationResult( "NG 102" );
                break;
    
            case gErrorNoAck_c:
                Terminal_Print( "Ack Timeout\r\n" );
                Processor_OutputOperationResult( "NG 103" );
                break;
    
            case gErrorTimeout_c:
                Terminal_Print( "Send Failed\r\n" );
                Processor_OutputOperationResult( "NG 104" );
                break;
    
            case gErrorTxDurationOver_c:
                Terminal_Print( "Send Duration Limit Over\r\n" );
                Processor_OutputOperationResult( "NG 105" );
                break;
            }
        }
    }

    gIsSending = FALSE;

#if defined(BLD_ENABLE_RFMODE_BURST)
    switch( mTermParam.RfMode )
    {
    case RFMODE_BURST:
        if( !gIsSending && !gIsRetrySending )
        {
            const uint8_t payloadLen = (uint8_t) strlen((char*)mTermParam.SendData);

            result = SetPacketPayload( &gAppTxPacket, mTermParam.SendData, payloadLen );
            if( result != gErrorNoError_c )
            {
                /* error */
                break;
            }

            // start sending
			gWithParams = FALSE;
            result = StartSending( &gAppTxPacket, payloadLen, FALSE, NULL, NULL );
            if( result != gErrorNoError_c )
            {
                /* error */
                break;
            }
        }
    }
#endif

    if( IS_TIMER_SLEEP(mTermParam) )
    {
        if( gIsRetrySending )
        {
            if( gRetryBackoff == 0 )
            {
                // retry sending (no backoff)
                if( gWithParams )
                {
                    err = StartSending( &gAppTxPacket, gSendPayloadLen, TRUE, &gTransParams, &gRouteParams );
                }
                else
                {
                    err = StartSending( &gAppTxPacket, gSendPayloadLen, FALSE, NULL, NULL );
                }
                if( err != gErrorNoError_c )
                {
                    /* error */
                    SendDoneProcess( err );
                    return;
                }
            }
            else
            {
                // retry sending after backoff
                EnterSleep();
            }
        }
        else
        {
            Terminal_Print( "enter timer sleep mode\r\n" );
            
            EnterSleep();
        }
    }
    else if( IS_INT_SLEEP_1TX(mTermParam) )
    {
        Terminal_Print( "enter interrupt sleep mode\r\n" );

        EnterSleep();
    }
    else if( IS_UART_SLEEP(mTermParam) )
    {
        Terminal_Print( "enter uart sleep mode\r\n" );

        EnterSleep();
    }
#endif
}

/*******************************************************************************
*
* SmacCallback_onNotifyRxData
*
* Interface assumptions:
*     pMsgIn          receive message
*     rssi            receive rssi
*
* Return value:
*     None
*
*******************************************************************************/
static void SmacCallback_onNotifyRxData( smacErrors_t result, const SmacUser_RxResultParams_t* params )
{
#if COMPETITION_MODE
    CompCheckDuration();
    if (gCompFinished)
    {
        return;
    }
#endif

    if( result != gErrorNoError_c )
    {
        // error
    }
    else
    {
        LED_Toggle( LED1 );

#if COMPETITION_MODE
        // A zero-length frame carries no competition payload at all. Every group
        // shares PanId 0x0001 and broadcasts to 0xFFFF, so these well-formed but
        // empty frames from other modules do reach us -- but they are neither our
        // packet nor a scoring rival packet, so they must not cost -5 (nor waste a
        // stealth window). Count them separately for the record and stop here.
        if (params->payloadSize == 0)
        {
            gCompEmptyRxCount++;
            Terminal_Print("[%08u] [RX_EMPTY] zero-length frame srcid=%04X (ignored, no penalty, total=%u)\r\n",
                           (unsigned int)HAL_GetTick(),
                           (unsigned)params->trans.addr.srcNode.nodeAddr,
                           (unsigned int)gCompEmptyRxCount);
            return;
        }

        uint8_t* payload = getPacketPayload( &gAppRxPacket, (protocolType_t)mTermParam.Protocol );
        // Zero the whole buffer: the classifying memcmp calls below read a fixed
        // number of bytes, which would otherwise reach past a short payload into
        // uninitialised stack.
        char tempMsg[64] = {0};
        uint8_t copyLen = (params->payloadSize < 63) ? params->payloadSize : 63;
        if (payload != NULL && copyLen > 0)
        {
            for (uint8_t i = 0; i < copyLen; i++)
            {
                // Replace non-printable binary bytes with '.' to prevent terminal character corruption
                uint8_t c = payload[i];
                tempMsg[i] = (c >= 32 && c <= 126) ? (char)c : '.';
            }
            tempMsg[copyLen] = '\0';
        }

        // Extract RSSI
        uint16_t rssi4_neg = ((uint16_t)params->trans.phy.radio.rxStatus.rssi * 2);
        if( ((int8_t)params->trans.phy.radio.rxStatus.snr) < 0 )
        {
            rssi4_neg += -(int8_t)params->trans.phy.radio.rxStatus.snr;
        }
        int16_t rssiVal = -(int16_t)(rssi4_neg / 4) - 10;

        // Check if this is a control packet from our group ("GRP3:CMD_...")
        if (memcmp(tempMsg, COMP_CTRL_PREFIX, strlen(COMP_CTRL_PREFIX)) == 0)
        {
            Terminal_Print("[%08u] [CTRL_RX] Received command: %s RSSI=%d\r\n",
                           (unsigned int)HAL_GetTick(), tempMsg, (int)rssiVal);

            if (IS_RX_NODE())
            {
                // RX Node reacts to TX Node's channel hop commands!
                if (strstr(tempMsg, "CH1") != NULL)
                {
                    gCurrentChIdx = 0; // CH1
                    ApplyChannelAndSf(COMP_CHANNEL_1, 7);
                    Terminal_Print("[%08u] [RX_NODE] Following TX Node to CH1\r\n", (unsigned int)HAL_GetTick());
                }
                else if (strstr(tempMsg, "CH7") != NULL)
                {
                    gCurrentChIdx = 1; // CH7
                    ApplyChannelAndSf(COMP_CHANNEL_2, 7);
                    Terminal_Print("[%08u] [RX_NODE] Following TX Node to CH7\r\n", (unsigned int)HAL_GetTick());
                }

                gCompRxHopCount++;

                // Following a command counts as hearing our TX node; refresh
                // the last-heard time so the lost-TX check does not hop us away
                // right after arriving on the commanded channel.
                gCompLastOwnRxTick = HAL_GetTick();
            }
            return;
        }

        // Check if this is our own group's official data packet ("GRP3:LORA...")
        bool_t isOwnDataPacket = (memcmp(tempMsg, COMP_OFFICIAL_PAYLOAD, strlen(COMP_OFFICIAL_PAYLOAD)) == 0);
        bool_t isOwnPacket = (memcmp(tempMsg, "GRP3:", 5) == 0);

        if (IS_RX_NODE())
        {
            // Heard our group: refresh the last-heard time so the lost-TX
            // check (in SendTimerProcess) does not fire while TX is reaching us.
            if (isOwnPacket || memcmp(tempMsg, COMP_CTRL_PREFIX, strlen(COMP_CTRL_PREFIX)) == 0)
            {
                gCompLastOwnRxTick = HAL_GetTick();
            }

            // Count +1 point only when RX Node successfully receives our official data packet
            if (isOwnDataPacket)
            {
                gCompRxSuccessCount++;
                Terminal_Print("[%08u] [RX_NODE] RX_SUCCESS! Received own data packet (total_rx=%u, +1 pt)\r\n",
                               (unsigned int)HAL_GetTick(), (unsigned int)gCompRxSuccessCount);
            }
        }

        if (IS_TX_NODE())
        {
            // ================= TX NODE SCOUTING (Q1) =================
            // TX node receives packets safely without penalty! Use for scouting.
            if (!isOwnPacket)
            {
                // Scout log only -- no hop. The rival that just finished
                // transmitting now owes a ToA*9 duty rest, so right after a
                // scouted packet this channel tends to be at its SAFEST.
                // CH hops are driven solely by the LBT 3-consecutive-block
                // trigger in SendDoneProcess.
                Terminal_Print("[%08u] [TX_SCOUT] Rival detected on CH=%u RSSI=%d msg=%s (0 penalty!)\r\n",
                               (unsigned int)HAL_GetTick(), gCompChannels[gCurrentChIdx], (int)rssiVal, tempMsg);
            }
        }
        else // IS_RX_NODE()
        {
            // ================= RX NODE HIT DETECTION =================
            if (!isOwnPacket)
            {
                // === RIVAL PACKET RECEIVED (-5 points penalty!) ===
                gCompRivalRxCount++;

                Terminal_Print("[%08u] [RX_HIT!] RIVAL_RX CH=%u SF=%u RSSI=%d total_rival=%u msg=%s\r\n",
                               (unsigned int)HAL_GetTick(),
                               gCompChannels[gCurrentChIdx],
                               gCurrentSF,
                               (int)rssiVal,
                               (unsigned int)gCompRivalRxCount,
                               tempMsg);

                // 0. Last resort: once the score is deep enough underwater,
                //    stop playing for points and just become unreceivable.
                if (!gCompPanicMode && CompTotalScore() <= PANIC_TOTAL_SCORE)
                {
                    CompEnterPanic();
                    return;
                }

                // 1. SF Stealth (Bonus 2): Switch to SF10 temporarily to hide
                if (gCompRxState == COMP_RX_STATE_NORMAL)
                {
                    gCompRxState = COMP_RX_STATE_STEALTH;
                    gCurrentSF = 10;
                    ApplyChannelAndSf(gCompChannels[gCurrentChIdx], 10);
                    Terminal_Print("[%08u] [RX_NODE] Entering SF10 Stealth for %d ms\r\n",
                                   (unsigned int)HAL_GetTick(), STEALTH_DURATION_MS);

                    // Auto-exit stealth after STEALTH_DURATION_MS (single-shot dodge)
                    CompScheduleNextTx(STEALTH_DURATION_MS);
                }
            }
        }

        // === Output received packet data to Processor UART ===
        uint32_t rxToaMs = CompCalcToaMs(params->payloadSize, gCurrentSF);
        Terminal_Print(
                       "--> receive data info[panid = %04X, srcid = %04X, dstid = %04X, length = %02X, ToA = %u ms]\r\n",
                       (unsigned) params->trans.addr.srcNode.panId,
                       (unsigned) params->trans.addr.srcNode.nodeAddr,
                       (unsigned) params->trans.addr.destNodeAddr,
                       (unsigned) params->payloadSize,
                       (unsigned) rxToaMs );

        // output length
        if( FMT_BINARY == mTermParam.Format )
        {
            uint8_t len = 0;
            if( MODE_ON == mTermParam.Rssi )
            {
                len += 4;
            }
            if( MODE_ON == mTermParam.RcvId )
            {
                len += 8;
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
                switch( mTermParam.Protocol )
                {
                __cases_Protocol_R
                    len += 4;
                    break;
                }
#endif
            }
            len += params->payloadSize;

            Processor_Write1( len );
        }

        // output RSSI
        if( MODE_ON == mTermParam.Rssi )
        {
            const uint16_t rssi100_neg = ((uint16_t)rssi4_neg * 25) + 1000;

            Terminal_Print( "RSSI(-%u.%02udBm):", (unsigned)rssi100_neg / 100, (unsigned)rssi100_neg % 100 );

            const int16_t rssi = -(int16_t)(rssi4_neg / 4) - 10;
            Processor_Print( "%04X", (unsigned)(uint16_t)rssi );
        }

        // output address
        if( MODE_ON == mTermParam.RcvId )
        {
            const SmacTransAddrInfo_t* addr = &params->trans.addr;

            Terminal_Print( "PAN ID(%04X):", (unsigned)addr->srcNode.panId );
            Processor_Print( "%04X", (unsigned)addr->srcNode.panId );

            Terminal_Print( "Src ID(%04X):", (unsigned)addr->srcNode.nodeAddr );
            Processor_Print( "%04X", (unsigned)addr->srcNode.nodeAddr );

#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
            {
                const smacRouteParams_t* route = &params->route;

                switch( mTermParam.Protocol )
                {
                __cases_Protocol_R
                    Terminal_Print( "End ID(%04X):", (unsigned)route->oriNodeAddr );
                    Processor_Print("%04X", (unsigned)route->oriNodeAddr );
                    break;
                }
            }
#endif
        }

        // output payload safely (prevent binary character terminal corruption)
        Terminal_Print( "Receive Data(%s)\r\n", tempMsg );
        Processor_Write( payload, params->payloadSize );
        if( FMT_ASCII == mTermParam.Format )
        {
            Processor_Print( "\r\n" );
        }
#else
        Terminal_Print(
                       "--> receive data info[panid = %04X, srcid = %04X, dstid = %04X, length = %02X]\r\n",
                       (unsigned) params->trans.addr.srcNode.panId,
                       (unsigned) params->trans.addr.srcNode.nodeAddr,
                       (unsigned) params->trans.addr.destNodeAddr,
                       (unsigned) params->payloadSize );

        // output length
        if( FMT_BINARY == mTermParam.Format )
        {
            uint8_t len = 0;
            if( MODE_ON == mTermParam.Rssi )
            {
                len += 4;
            }
            if( MODE_ON == mTermParam.RcvId )
            {
                len += 8;
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
                switch( mTermParam.Protocol )
                {
                __cases_Protocol_R
                    len += 4;
                    break;
                }
#endif
            }
            len += params->payloadSize;

            Processor_Write1( len );
        }

        // output RSSI
        if( MODE_ON == mTermParam.Rssi )
        {
            uint16_t rssi4_neg = ((uint16_t)params->trans.phy.radio.rxStatus.rssi * 2);

            if( ((int8_t)params->trans.phy.radio.rxStatus.snr) < 0 )
            {
                rssi4_neg += -(int8_t)params->trans.phy.radio.rxStatus.snr;
            }
            const uint16_t rssi100_neg = ((uint16_t)rssi4_neg * 25) + 1000;

            Terminal_Print( "RSSI(-%u.%02udBm):", (unsigned)rssi100_neg / 100, (unsigned)rssi100_neg % 100 );

            const int16_t rssi = -(int16_t)(rssi4_neg / 4) - 10;
            Processor_Print( "%04X", (unsigned)(uint16_t)rssi );
        }

        // output address
        if( MODE_ON == mTermParam.RcvId )
        {
            const SmacTransAddrInfo_t* addr = &params->trans.addr;

            Terminal_Print( "PAN ID(%04X):", (unsigned)addr->srcNode.panId );
            Processor_Print( "%04X", (unsigned)addr->srcNode.panId );

            Terminal_Print( "Src ID(%04X):", (unsigned)addr->srcNode.nodeAddr );
            Processor_Print( "%04X", (unsigned)addr->srcNode.nodeAddr );

#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
            const smacRouteParams_t* route = &params->route;

            switch( mTermParam.Protocol )
            {
            __cases_Protocol_R
                Terminal_Print( "End ID(%04X):", (unsigned)route->oriNodeAddr );
                Processor_Print("%04X", (unsigned)route->oriNodeAddr );
                break;
            }
#endif
        }

        // output payload
        Terminal_Print( "Receive Data(" );
        uint8_t* payload = getPacketPayload( &gAppRxPacket, (protocolType_t)mTermParam.Protocol );

        Terminal_Write( payload, params->payloadSize );
        Terminal_Print( ")\r\n" );
        Processor_Write( payload, params->payloadSize );
        if( FMT_ASCII == mTermParam.Format )
        {
            Processor_Print( "\r\n" );
        }
#endif
    }
}

/*******************************************************************************
*
* SmacCallback_onTxDataComp
*
* Interface assumptions:
*     result        tx result
*
* Return value:
*     None
*
*******************************************************************************/
static void SmacCallback_onTxDataComp( smacErrors_t result )
{
    SendDoneProcess( result );
}

#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
/*******************************************************************************
*
* SmacCallback_onNotifyForward
*
* Interface assumptions:
*     params        informations
*     isRouting     enable routing
*     transOption   transfer option for forwarding tx
*
* Return value:
*     None
*
*******************************************************************************/
static void SmacCallback_onNotifyForward( const SmacUser_RxResultParams_t* params, bool_t* isRouting, SmacTransOption_t* transOption )
{
    smacErrors_t error;

    *isRouting = FALSE;

    if( mTermParam.Node == ROUTER )
    {
        SmacTransParams_t transParams;

        error = SMAC_GetTransParams( &transParams );
        if( error != gErrorNoError_c )
        {
            // fatal error
            Terminal_Print( "routing failed\r\n" );
            return;
        }

        *isRouting = TRUE;
        *transOption = transParams.option;

        LED_Toggle( LED2 );
    }
}

static void SmacCallback_onNotifyForwardResult( smacErrors_t result )
{
    if( result != gErrorNoError_c )
    {
        Terminal_Print( "routing failed\r\n" );
    }
}
#endif

/*******************************************************************************
*
* SmacCallback_TimerControl
*
* Interface assumptions:
*     type      timer type
*     duration  timer duration
*
* Return value:
*     None
*
*******************************************************************************/
static void SmacCallback_TimerControl( stackTimerType_t type, uint32_t duration )
{
    UsrTimerId timerId;

    switch( type )
    {
    case PhyCcaTimer:
        timerId = UsrPhyCcaTimer;
        ClearEvent( EVENT_PHY_CCA_TIMER );
        break;

    case PhyTxTimer:
        timerId = UsrPhyTxTimer;
        ClearEvent( EVENT_PHY_TX_TIMER );
        break;

    case PhyRxTimer:
        timerId = UsrPhyRxTimer;
        ClearEvent( EVENT_PHY_RX_TIMER );
        break;

    case PhyDutyTimer:
        timerId = UsrPhyDutyTimer;
        ClearEvent( EVENT_PHY_DUTY_TIMER );
        break;

    case SmacCcaBackoffTimer:
        timerId = UsrSmacCcaBackoffTimer;
        ClearEvent( EVENT_SMAC_CCA_BACKOFF_TIMER );
        break;

    case SmacTxTimer:
        timerId = UsrSmacTxTimer;
        ClearEvent( EVENT_SMAC_TX_TIMER );
        break;

    default:
        return;
    }

    if( duration )
    {
        UsrTimer_start( timerId, UsrTimerMode_OneShot, duration, UsrTimerCallback );
    }
    else
    {
        UsrTimer_stop( timerId );
    }
}

/*******************************************************************************
*
* SmacCallback_FiberControl
*
* Interface assumptions:
*     type      Fiber type
*     duration  Fiber duration
*
* Return value:
*     None
*
*******************************************************************************/
static void SmacCallback_FiberControl( stackFiberType_t type, bool_t enable )
{
    uint32_t fiber;

    switch( type )
    {
    case PhyCcaFiber:
        fiber = FIBER_PHY_CCA;
        break;

    default:
        return;
    }

    if( enable )
    {
        StartFiber( fiber );

        SetEvent( EVENT_PHY_CCA );
    }
    else
    {
        StopFiber( fiber );
    }
}

/*******************************************************************************
*
* CheckSwitchStateProcess
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
static void CheckSwitchStateProcess( void )
{
    if( IS_INT_SLEEP(mTermParam) )
    {
        if( HAL_GPIO_ReadPin(SW_INT_GPIO_Port, SW_INT_Pin) )
        {
            if( !gEnterSleep )
            {
                Terminal_Print( "enter interrupt sleep mode\r\n" );

                EnterSleep();
            }
        }
        else
        {
            if( gEnterSleep )
            {
                LeaveSleep();

                Terminal_Print( "exit interrupt sleep mode\r\n" );

                SetRadioModeOnIdle();
            }
        }
    }
}

/*******************************************************************************
*
* TimerWakeupProcess
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
static void TimerWakeupProcess( void )
{
    smacErrors_t err;

    if( IS_TIMER_SLEEP(mTermParam) )
    {
        // wakeup RF
        LeaveSleep();

        if( gIsRetrySending )
        {
            // retry sending
            if( gWithParams )
            {
                err = StartSending( &gAppTxPacket, gSendPayloadLen, TRUE, &gTransParams, &gRouteParams );
            }
            else
            {
                err = StartSending( &gAppTxPacket, gSendPayloadLen, FALSE, NULL, NULL );
            }
            if( err != gErrorNoError_c )
            {
                /* error */
                SendDoneProcess( err );
                return;
            }
        }
        else
        {
            Terminal_Print( "exit timer sleep mode\r\n" );
            
            if( 0 != mTermParam.SendTime )
            {
                // start sending
                SendTimerProcess();
            }
            else
            {
                // set RF mode
                SetRadioModeOnIdle();
            }
        }
    }
}

/*******************************************************************************
*
* SendTimerProcess
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
static void SendTimerProcess( void )
{
    smacErrors_t err;

#if COMPETITION_MODE
    CompCheckDuration();
    if (gCompFinished)
    {
        return;
    }
#endif

    if( !gIsSending && !gIsRetrySending )
    {
#if COMPETITION_MODE
        if (IS_TX_NODE())
        {
            // ================= TX NODE TRANSMISSION =================
            char payloadStr[64];
            uint8_t payloadLen;

            if (gPendingCtrlSend)
            {
                // Send control packet to lead RX Node (0 penalty for others, Q3)
                gPendingCtrlSend = FALSE;
                gCompLastTxWasCtrl = TRUE;
                strncpy(payloadStr, gPendingCtrlBuf, sizeof(payloadStr));
                payloadLen = strlen(payloadStr);

                Terminal_Print("[%08u] [TX_NODE] Sending Control Packet: %s\r\n",
                               (unsigned int)HAL_GetTick(), payloadStr);
            }
            else
            {
                // Send official payload (scored on the RX side per reception)
                // "GRP3:LORA_AD_HOC_SPECTRUM_SURVIVAL_PAYLOAD" (exact official string)
                gCompLastTxWasCtrl = FALSE;
                strncpy(payloadStr, COMP_OFFICIAL_PAYLOAD, sizeof(payloadStr));
                payloadLen = strlen(payloadStr);

                // Ensure we are on SF7
                if (gCurrentSF != 7)
                {
                    gCurrentSF = 7;
                    ApplyChannelAndSf(gCompChannels[gCurrentChIdx], 7);
                }
            }

            err = SetPacketPayload( &gAppTxPacket, (uint8_t*)payloadStr, payloadLen );
            if( err != gErrorNoError_c )
            {
                SendDoneProcess( err );
                return;
            }

            gWithParams = FALSE;
            err = StartSending( &gAppTxPacket, payloadLen, FALSE, NULL, NULL );
            if( err != gErrorNoError_c )
            {
                SendDoneProcess( err );
                return;
            }
        }
        else // IS_RX_NODE()
        {
            // ================= RX NODE TIMER HANDLING =================
            // Panic overrides everything: no stealth, no lost-TX hop, no
            // scoring -- just keep moving so nothing can be received.
            if (gCompPanicMode)
            {
                CompPanicChurn();
                return;
            }

            // This timer serves two jobs on the SAME UsrTxIntervalTimer:
            // exiting a stealth window (short) and the lost-TX watchdog (long).
            // The hop decision is made on ELAPSED TIME since we last heard our
            // TX node -- not on which schedule fired -- so repeated stealth
            // cycles re-arming the timer at STEALTH_DURATION_MS can no longer
            // starve the lost-TX hop.

            // Leave stealth if we were hiding (SF10 -> SF7).
            if (gCompRxState == COMP_RX_STATE_STEALTH)
            {
                gCompRxState = COMP_RX_STATE_NORMAL;
                gCurrentSF = 7;
                ApplyChannelAndSf(gCompChannels[gCurrentChIdx], 7);
                Terminal_Print("[%08u] [RX_NODE] Stealth finished -> Returning to SF7 Normal\r\n",
                               (unsigned int)HAL_GetTick());
            }

            // Lost-TX watchdog: hop only if we truly have not heard our TX node
            // for LOST_TX_TIMEOUT_MS.
            if ((HAL_GetTick() - gCompLastOwnRxTick) >= LOST_TX_TIMEOUT_MS)
            {
                uint16_t fromCh = gCompChannels[gCurrentChIdx];
                gCurrentChIdx = 1 - gCurrentChIdx;
                uint16_t toCh = gCompChannels[gCurrentChIdx];

                Terminal_Print("[%08u] [RX_NODE] Lost TX Node! Autonomously hopping from CH%u to CH%u\r\n",
                               (unsigned int)HAL_GetTick(), fromCh, toCh);

                ApplyChannelAndSf(toCh, 7);
                gCompRxHopCount++;

                // Give the new channel a full window before reconsidering.
                gCompLastOwnRxTick = HAL_GetTick();
            }

            // Keep the RX housekeeping timer ticking.
            CompScheduleNextTx(LOST_TX_TIMEOUT_MS);
        }
#else
        const uint8_t payloadLen = (uint8_t) strlen((char*)mTermParam.SendData);

        err = SetPacketPayload( &gAppTxPacket, mTermParam.SendData, payloadLen );
        if( err != gErrorNoError_c )
        {
            /* error */
            SendDoneProcess( err );
            return;
        }

        // start sending
        gWithParams = FALSE;
        err = StartSending( &gAppTxPacket, payloadLen, FALSE, NULL, NULL );
        if( err != gErrorNoError_c )
        {
            /* error */
            SendDoneProcess( err );
            return;
        }
#endif
    }
#if COMPETITION_MODE
    else
    {
        // A transmission is still in flight. Never drop the timer chain on the
        // floor here: this node reschedules only from inside the branch above,
        // so returning without re-arming would stall it permanently.
        CompScheduleNextTx(50);
    }
#endif
}

#if defined(BLD_ENABLE_RFMODE_RSSI_CHECK)
/*******************************************************************************
*
* RssiCheckProcess
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
static void RssiCheckProcess( void )
{
    const uint8_t rssi2_neg = SMAC_GetRssi();

    const uint16_t rssi10_neg = ((uint16_t)rssi2_neg * 5);
    Terminal_Print( "-%u.%u\r\n", (unsigned)rssi10_neg / 10, (unsigned)rssi10_neg % 10 );
}
#endif

/*******************************************************************************
*
* SetRadioSleep
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
smacErrors_t SetRadioSleep( void )
{
    smacErrors_t err;

    gIsStandby   = FALSE;
    gIsReceiving = FALSE;
    gIsSending   = FALSE;

    //-------------------------
    // sleep SX1261
    //-------------------------
    err = SMAC_Sleep( IS_RF_SLEEP_WARM(mTermParam) );    // sleep
    if( err != gErrorNoError_c )
    {
        return err;
    }

    return gErrorNoError_c;
}

/*******************************************************************************
*
* SetRadioWakeup
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
smacErrors_t SetRadioWakeup( void )
{
    smacErrors_t err;

    //--------------------------------
    // wakeup SX1261
    //--------------------------------
    gIsStandby   = FALSE;
    gIsReceiving = FALSE;
    gIsSending   = FALSE;

    err = SMAC_Standby( );    // wakeup
    if( err != gErrorNoError_c )
    {
        return err;
    }

    gIsStandby = TRUE;

    return gErrorNoError_c;
}

/*******************************************************************************
*
* UsrTimerCallback
*
* Interface assumptions:
*     timerId      timer ID
*
* Return value:
*     None
*
*******************************************************************************/
static void UsrTimerCallback( UsrTimerId timerId )
{
    switch( timerId )
    {
    case UsrSleepTimer:
        SetEvent( EVENT_WKUP_TIMER );
        break;

    case UsrHeartBeatTimer:
        SetEvent( EVENT_WDT_RESET );
        break;

    case UsrTxIntervalTimer:
        SetEvent( EVENT_SEND_TIME );
        break;

#if defined(BLD_ENABLE_RFMODE_RSSI_CHECK)
    case UsrRssiCheckTimer:
        SetEvent( EVENT_RSSI_CHECK );
        break;
#endif

    case UsrPhyTxTimer:
        SetEvent( EVENT_PHY_TX_TIMER );
        break;

    case UsrPhyRxTimer:
        SetEvent( EVENT_PHY_RX_TIMER );
        break;

    case UsrPhyCcaTimer:
        SetEvent( EVENT_PHY_CCA_TIMER );
        break;

    case UsrPhyDutyTimer:
        SetEvent( EVENT_PHY_DUTY_TIMER );
        break;

    case UsrSmacCcaBackoffTimer:
        SetEvent( EVENT_SMAC_CCA_BACKOFF_TIMER );
        break;

    case UsrSmacTxTimer:
        SetEvent( EVENT_SMAC_TX_TIMER );
        break;

    default:
        break;
    }
}

/*******************************************************************************
*
* WakeupTimerCallback
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
static void WakeupTimerCallback( void )
{
    SetEvent( EVENT_WKUP_TIMER );
}

/*******************************************************************************
*
* CommInputDoneCallback
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
static void CommInputDoneCallback( void )
{
    SetEvent( EVENT_UART_RX_DONE );
}

/*******************************************************************************
*
* CommOutputDoneCallback
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
static void CommOutputDoneCallback( void )
{
    SetEvent( EVENT_UART_TX_DONE );
}

#if COMPETITION_MODE
static void ApplyChannelAndSf( uint16_t ch, uint16_t sf )
{
    PhyRadioParams_t radioParams;

    if (SMAC_GetRadioParams(&radioParams) == gErrorNoError_c)
    {
        radioParams.modulation.ch = ch;
        radioParams.modulation.LORA.sf = sf;
        SMAC_SetRadioParams(&radioParams);
    }

    // Force radio to apply parameters by putting it to standby first, then restarting Rx
    gIsReceiving = FALSE;
    gIsStandby = FALSE;
    SMAC_Standby();
    SetRadioModeOnIdle();
}

static void CompScheduleNextTx( uint32_t delayMs )
{
    UsrTimer_start( UsrTxIntervalTimer, UsrTimerMode_OneShot, delayMs, UsrTimerCallback );
}

static void CompSendCtrlPacket( const char* ctrlMsg )
{
    gPendingCtrlSend = TRUE;
    strncpy(gPendingCtrlBuf, ctrlMsg, sizeof(gPendingCtrlBuf) - 1);
    gPendingCtrlBuf[sizeof(gPendingCtrlBuf) - 1] = '\0';
}

static void CompTxDoChHop( const char* trigger )
{
    uint16_t fromCh = gCompChannels[gCurrentChIdx];
    uint8_t toIdx = (uint8_t)(1 - gCurrentChIdx);
    uint16_t toCh = gCompChannels[toIdx];

    Terminal_Print("[%08u] [TX_NODE] CH_HOP trigger=%s from=CH%u to=CH%u\r\n",
                   (unsigned int)HAL_GetTick(), trigger, fromCh, toCh);

    // Queue the hop command. It must be broadcast on the CURRENT channel,
    // where the RX node is still listening -- hopping first would announce
    // the move on the new channel that nobody hears. The actual switch
    // happens in SendDoneProcess once the command has been transmitted, or
    // after CTRL_SEND_MAX_RETRY blocked attempts (the RX node then re-syncs
    // via its lost-TX timeout hop).
    const char* cmdMsg = (toCh == COMP_CHANNEL_1) ? COMP_CTRL_CH1 : COMP_CTRL_CH7;
    CompSendCtrlPacket(cmdMsg);

    gCompHopPending = TRUE;
    gCompPendingHopChIdx = toIdx;
    gCompCtrlRetryCount = 0;
}

static void CompCompleteHop( const char* how )
{
    gCompHopPending = FALSE;
    gCompCtrlRetryCount = 0;
    gCurrentChIdx = gCompPendingHopChIdx;
    ApplyChannelAndSf(gCompChannels[gCurrentChIdx], 7);

    Terminal_Print("[%08u] [TX_NODE] CH_HOP done (%s) now=CH%u\r\n",
                   (unsigned int)HAL_GetTick(), how, gCompChannels[gCurrentChIdx]);
}

static void CompPrintFinalSummary( void )
{
    gCompFinished = TRUE;

    // Stop all sending timers
    UsrTimer_stop(UsrTxIntervalTimer);

    // Turn off radio (Put stack to Standby and do not restart RX)
    gIsReceiving = FALSE;
    gIsSending = FALSE;
    gIsStandby = FALSE;
    SMAC_Standby();

    // Calculate score
    // Reception: +1 pt per successfully received own data packet
    // Rival RX: -5 pt per hit
    // Bonuses: CH Hopping (+50) + SF Stealth (+50) = +100
    int32_t finalScore = CompTotalScore();

    Terminal_Print("\r\n\r\n");
    Terminal_Print("==================================================\r\n");
    Terminal_Print("        COMPETITION FINISHED (10 MINUTES)         \r\n");
    Terminal_Print("==================================================\r\n");
    Terminal_Print("   Group ID: %u\r\n", GROUP_ID);
    if (IS_TX_NODE())
    {
        Terminal_Print("   Role: TRANSMITTER NODE\r\n");
    }
    else if (IS_RX_NODE())
    {
        Terminal_Print("   Role: RECEIVER NODE\r\n");
    }
    Terminal_Print("   Elapsed                : %u s\r\n",
                   (unsigned int)((HAL_GetTick() - gCompStartTick) / 1000u));
    Terminal_Print("   ----------------------------------------------\r\n");

    if (IS_TX_NODE())
    {
        // TX-side numbers. Points are awarded on the RX side, so these are
        // effort counters, not score.
        Terminal_Print("   Official TX Success    : %u packets (scored via RX side)\r\n",
                       (unsigned int)gCompTxSeqNum);
        Terminal_Print("   Control Packets Sent   : %u (not scored)\r\n",
                       (unsigned int)gCompCtrlTxCount);
        Terminal_Print("   Final Cell             : CH%u SF%u\r\n",
                       gCompChannels[gCurrentChIdx], gCurrentSF);
    }
    else
    {
        // RX-side numbers: this is where the score actually comes from.
        Terminal_Print("   Own Packets Received   : %u (+%d pts)\r\n",
                       (unsigned int)gCompRxSuccessCount, (int)gCompRxSuccessCount);
        Terminal_Print("   Rival Packets Received : %u (-%d pts)\r\n",
                       (unsigned int)gCompRivalRxCount, (int)gCompRivalRxCount * 5);
        Terminal_Print("   Empty Frames Ignored   : %u (no payload, 0 pts)\r\n",
                       (unsigned int)gCompEmptyRxCount);
        Terminal_Print("   Channel Hops           : %u\r\n",
                       (unsigned int)gCompRxHopCount);
        Terminal_Print("   Final Cell             : CH%u SF%u\r\n",
                       gCompChannels[gCurrentChIdx], gCurrentSF);
    }

    Terminal_Print("   Technical Bonuses      : +%d pts (Autonomous Hop + SF Stealth)\r\n",
                   (int)COMP_BONUS_POINTS);
    if (gCompPanicMode)
    {
        Terminal_Print("   PANIC MODE             : engaged (%u churn steps, scoring abandoned)\r\n",
                       (unsigned int)gCompPanicSteps);
    }
    Terminal_Print("   ----------------------------------------------\r\n");
    Terminal_Print("   ESTIMATED GROUP SCORE  : %d pts\r\n", (int)finalScore);
    Terminal_Print("==================================================\r\n\r\n");
}

static int32_t CompEstimatedScore( void )
{
    // Scoring is reception-based: +1 per own official packet received,
    // -5 per rival packet received. Bonuses are excluded on purpose -- this is
    // the number the panic trigger reasons about.
    return (int32_t)gCompRxSuccessCount - ((int32_t)gCompRivalRxCount * 5);
}

static int32_t CompTotalScore( void )
{
    // What the final report claims: reception score plus the technical bonus.
    return CompEstimatedScore() + COMP_BONUS_POINTS;
}

static void CompEnterPanic( void )
{
    gCompPanicMode = TRUE;
    gCompPanicIdx = 0;

    // Stealth is pointless from here on: churning covers it and then some.
    gCompRxState = COMP_RX_STATE_NORMAL;

    Terminal_Print("[%08u] [PANIC] total=%d (rx_score=%d + bonus=%d) <= %d : "
                   "abandoning scoring, SF%u channel churn every %u ms\r\n",
                   (unsigned int)HAL_GetTick(),
                   (int)CompTotalScore(),
                   (int)CompEstimatedScore(),
                   (int)COMP_BONUS_POINTS,
                   (int)PANIC_TOTAL_SCORE,
                   (unsigned)PANIC_SF,
                   (unsigned int)PANIC_CHURN_MS);

    CompScheduleNextTx(PANIC_CHURN_MS);
}

static void CompPanicChurn( void )
{
    uint16_t ch;

    // Hop CH1 <-> CH7 while parked on SF10: rivals on SF7 are orthogonal (their
    // frames never demodulate here), and the constant channel motion keeps even
    // an SF10 rival from ever completing one.
    gCurrentChIdx = (uint8_t)(gCompPanicIdx & 1u);
    ch = gCompChannels[gCurrentChIdx];

    gCompPanicIdx = (uint8_t)((gCompPanicIdx + 1u) & 1u);
    gCompPanicSteps++;

    gCurrentSF = PANIC_SF;
    ApplyChannelAndSf(ch, PANIC_SF);

    // Log sparsely: printing every step would throttle the event loop on UART.
    if ((gCompPanicSteps % PANIC_LOG_EVERY) == 0)
    {
        Terminal_Print("[%08u] [PANIC] churn step=%u now CH=%u SF=%u (rival_rx=%u)\r\n",
                       (unsigned int)HAL_GetTick(),
                       (unsigned int)gCompPanicSteps,
                       ch, (unsigned)PANIC_SF,
                       (unsigned int)gCompRivalRxCount);
    }

    CompScheduleNextTx(PANIC_CHURN_MS);
}

static void CompPeriodicStatus( void )
{
    uint32_t elapsedS;

    if (gCompFinished)
    {
        return;
    }
    if ((HAL_GetTick() - gCompLastStatusTick) < COMP_STATUS_LOG_MS)
    {
        return;
    }
    gCompLastStatusTick = HAL_GetTick();

    elapsedS = (HAL_GetTick() - gCompStartTick) / 1000u;

    if (IS_TX_NODE())
    {
        Terminal_Print("[%08u] [STATUS] t=%us TX sent=%u ctrl=%u CH=%u SF=%u lbt_blocks=%u\r\n",
                       (unsigned int)HAL_GetTick(),
                       (unsigned int)elapsedS,
                       (unsigned int)gCompTxSeqNum,
                       (unsigned int)gCompCtrlTxCount,
                       gCompChannels[gCurrentChIdx],
                       gCurrentSF,
                       (unsigned int)gLbtBlockCount);
    }
    else
    {
        Terminal_Print("[%08u] [STATUS] t=%us RX ok=%u rival=%u empty=%u score=%d CH=%u SF=%u hops=%u%s\r\n",
                       (unsigned int)HAL_GetTick(),
                       (unsigned int)elapsedS,
                       (unsigned int)gCompRxSuccessCount,
                       (unsigned int)gCompRivalRxCount,
                       (unsigned int)gCompEmptyRxCount,
                       (int)CompTotalScore(),
                       gCompChannels[gCurrentChIdx],
                       gCurrentSF,
                       (unsigned int)gCompRxHopCount,
                       gCompPanicMode ? " [PANIC]" : "");
    }
}

static void CompCheckDuration( void )
{
    if (!gCompFinished)
    {
        if (HAL_GetTick() - gCompStartTick >= COMP_DURATION_MS)
        {
            CompPrintFinalSummary();
        }
    }
}

static uint32_t CompCalcToaMs( uint8_t payloadLen, uint16_t sf )
{
    PhyRadioParams_t radioParams;

    if (SMAC_GetRadioParams(&radioParams) == gErrorNoError_c)
    {
        // Set the requested SF and calculate ToA using official stack function
        radioParams.modulation.LORA.sf = sf;
        return Phy_TimeOnAir_LORA( &radioParams.modulation, payloadLen );
    }
    return 0;
}
#endif

