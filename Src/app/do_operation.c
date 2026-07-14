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

#include "app/do_operation.h"
#include "app/commctl.h"
#include "app/params.h"
#include "app/usr_main.h"
#include "platform/events.h"
#include "platform/led.h"
#include "platform/pendsv.h"
#include "platform/powerctl.h"
#include "platform/timerctl.h"
#include "rtc.h"
#include "usr_common.h"

/*******************************************************************************
********************************************************************************
* Private macros
********************************************************************************
*******************************************************************************/

#define INTERVAL_TIMEOUT (50)

/*******************************************************************************
********************************************************************************
* Private prototypes
********************************************************************************
*******************************************************************************/

// process handler
static void InitProcess(void);
static void ProcessEvent(uint32_t ev);
static void ProcessFiber(uint32_t fiber);

// event handler
static void CommInputDoneProcess(void);
static void CommOutputDoneProcess(void);
static void SendDoneProcess(smacErrors_t result);
static void CheckSwitchStateProcess(void);
static void TimerWakeupProcess(void);
static void SendTimerProcess(void);
#if defined(BLD_ENABLE_RFMODE_RSSI_CHECK)
static void RssiCheckProcess(void);
#endif

static void StartCommInput(void);
static void StopCommInput(void);
static smacErrors_t AnalyzeCommDataFrame(smacRouteParams_t *routeParams,
                                         SmacTransParams_t *transParams,
                                         uint8_t **payload,
                                         uint8_t *payloadLen);
static smacErrors_t SetPacketPayload(packet_t *packet, const uint8_t *data,
                                     uint8_t dataLen);

static void EnterSleep(void);
static void LeaveSleep(void);

static smacErrors_t SetRadioModeOnIdle(void);
static smacErrors_t SetRadioSleep(void);
static smacErrors_t SetRadioWakeup(void);
static smacErrors_t StartSending(packet_t *packet, uint8_t payloadLen,
                                 bool_t withParams,
                                 SmacTransParams_t *transParams,
                                 smacRouteParams_t *routeParams);

// callback
static void
SmacCallback_onNotifyRxData(smacErrors_t result,
                            const SmacUser_RxResultParams_t *params);
static void SmacCallback_onTxDataComp(smacErrors_t result);
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
static void
SmacCallback_onNotifyForward(const SmacUser_RxResultParams_t *params,
                             bool_t *isRouting, SmacTransOption_t *transOption);
static void SmacCallback_onNotifyForwardResult(smacErrors_t result);
#endif
static void SmacCallback_TimerControl(stackTimerType_t type, uint32_t duration);
static void SmacCallback_FiberControl(stackFiberType_t type, bool_t enable);
static void UsrTimerCallback(UsrTimerId timerId);
static void WakeupTimerCallback(void);
static void CommInputDoneCallback(void);
static void CommOutputDoneCallback(void);

/*******************************************************************************
********************************************************************************
* Private memory declarations
********************************************************************************
*******************************************************************************/

static packet_t gAppTxPacket;
static packet_t gAppRxPacket;
static bool_t gIsReceiving = FALSE;
static bool_t gIsStandby = FALSE;
static bool_t gIsSending = FALSE;
static bool_t gEnterSleep = FALSE;
static bool_t gUartTxActive = FALSE;
static bool_t gUartRxActive = FALSE;

static bool_t gIsRetrySending = FALSE;
static uint16_t gSendRetryCount = 0;
static uint32_t gRetryBackoff = FALSE;
static uint8_t gSendPayloadLen;

static bool_t gWithParams = FALSE;
static smacRouteParams_t gRouteParams;
static SmacTransParams_t gTransParams;

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
void DoOperation(void) {
  const bool_t useSleepMode = IS_MCU_SLEEP(mTermParam);
  const bool_t useStopMode = IS_MCU_STOP(mTermParam);

  uint32_t event = 0;

  Terminal_Print("\r\n ----- operation mode is ready ----- \r\n");
  Processor_Print("\r\n");

  //--------------------------------
  // Init process
  //--------------------------------
  InitProcess();

  // event loop
  for (;;) {
    //---------------------------
    // Event Process
    //---------------------------
    for (event = GetEvent(); event; event = GetEvent()) {
      ProcessEvent(event);
    }

    //--------------------------------------------------
    // Sleep Process ( wait for event in sleep state )
    //--------------------------------------------------
    // Check if uart tx flushed
    gUartTxActive = !Serial1_IsWriteFlushed(CommOutputDoneCallback);

    if (useStopMode && SMAC_CanEnterStopMode() && !gUartTxActive) {
      // stop heartbeat timer
      UsrTimer_stop(UsrHeartBeatTimer);

      WDG_Refresh();

      // wait for event in STOP mode
      if (!gUartRxActive && gEnterSleep) {
        // enter STOP mode
        event = UsrPower_waitEventInStopMode(GetEvent_IT, mTermParam.Baudrate);
      } else {
        // enter SLEEP mode
        event = UsrPower_waitEventInCpuSleep(GetEvent_IT);
      }

      // restart timer
      UsrTimer_start(UsrHeartBeatTimer, UsrTimerMode_Periodic, INTERVAL_TIMEOUT,
                     UsrTimerCallback);
    } else if (useSleepMode) {
      // wait for event in SLEEP mode
      event = UsrPower_waitEventInCpuSleep(GetEvent_IT);
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
void Switch_Interrupt(void) {
  if (IS_INT_SLEEP(mTermParam)) {
    SetEvent(EVENT_INT_SW);
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
static void InitProcess(void) {
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  if (!IS_TIMER_SLEEP(mTermParam) ||
      (mTermParam.SleepTime / 10 < UsrTimer_SleepDurationMax)) {
    PeriphClkInit.PeriphClockSelection |= RCC_PERIPHCLK_RTC;
    PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_NONE;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
  } else {
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
  SmacUser_CallbackInterface_t handler = {0};
  handler.onNotifyRxData = SmacCallback_onNotifyRxData;
  handler.onTxDataComp = SmacCallback_onTxDataComp;
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
  handler.onNotifyForward = SmacCallback_onNotifyForward;
  handler.onNotifyForwardResult = SmacCallback_onNotifyForwardResult;
#endif
  handler.TimerControl = SmacCallback_TimerControl;
  handler.FiberControl = SmacCallback_FiberControl;
  SMAC_Start(&handler, &gAppRxPacket);

  //--------------------------------
  // start interval timer for WDT Refresh
  //--------------------------------
  UsrTimer_start(UsrHeartBeatTimer, UsrTimerMode_Periodic, INTERVAL_TIMEOUT,
                 UsrTimerCallback);

  //--------------------------------
  // start uart input
  //--------------------------------
  StartCommInput();

  //--------------------------------
  // set sleep state
  //--------------------------------
  gEnterSleep = FALSE;

  if (IS_TIMER_SLEEP(mTermParam)) {
    SmacTransParams_t transParams;

    SMAC_GetTransParams(&transParams);
    transParams.option.ackRetryCount = 0;
    SMAC_SetTransParams(&transParams);

    Terminal_Print("enter timer sleep mode\r\n");

    // enter idle and start wakeup timer
    EnterSleep();
  } else if (IS_INT_SLEEP(mTermParam)) {
    if (HAL_GPIO_ReadPin(SW_INT_GPIO_Port, SW_INT_Pin)) {
      Terminal_Print("enter interrupt sleep mode\r\n");

      // enter idle
      EnterSleep();
    }

    // enable Interrupt
    LL_EXTI_EnableIT_0_31(SW_INT_Pin);

    SetEvent(EVENT_INT_SW);
  } else if (IS_UART_SLEEP(mTermParam)) {
    Terminal_Print("enter uart sleep mode\r\n");

    // enter idle
    EnterSleep();
  } else {
    //--------------------------------
    // start sendtime timer
    //--------------------------------
    if (0 != mTermParam.SendTime) {
      if (!IS_TIMER_SLEEP(mTermParam)) {
        // send triggerred by send time timer
        UsrTimer_start(UsrTxIntervalTimer, UsrTimerMode_Periodic,
                       mTermParam.SendTime * 1000, UsrTimerCallback);

        SetEvent(EVENT_SEND_TIME);
      }
    }
  }

  if (!gEnterSleep) {
    SetRadioSleep();
  }

  HAL_PWREx_SMPS_SetMode(PWR_SMPS_STEP_DOWN);

  if (!gEnterSleep) {
    SetRadioModeOnIdle();
  }

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
static void EnterSleep(void) {
  if (!gEnterSleep) {
    gEnterSleep = TRUE;

    LED_TurnOff(LED_ALL);

    if (IS_UART_SLEEP(mTermParam)) {
      StartCommInput();
    } else {
      StopCommInput();
    }

    if (IS_RF_SLEEP(mTermParam)) {
      /* sleep radio */
      SetRadioSleep();
    } else {
      SetRadioModeOnIdle();
    }

    // start wakeup timer
    if (IS_TIMER_SLEEP(mTermParam)) {
      if (gIsRetrySending) {
        // retry backoff
        UsrTimer_start(UsrSleepTimer, UsrTimerMode_OneShot, gRetryBackoff,
                       UsrTimerCallback);
      } else {
        // start wakeup timer
        if (mTermParam.SleepTime / 10 < UsrTimer_SleepDurationMax) {
          // wakeup by LPTIMER
          UsrTimer_start(UsrSleepTimer, UsrTimerMode_OneShot,
                         mTermParam.SleepTime * 100, UsrTimerCallback);
        } else {
          // wakeup by RTC
          UsrWakeupTimer_start(mTermParam.SleepTime / 10, WakeupTimerCallback);
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
static void LeaveSleep(void) {
  if (gEnterSleep) {
    gEnterSleep = FALSE;

    if (!IS_UART_SLEEP(mTermParam)) {
      StartCommInput();
    }

    if (IS_TIMER_SLEEP(mTermParam)) {
      // stop timer
      if (mTermParam.SleepTime / 10 < UsrTimer_SleepDurationMax) {
        // wakeup by LPTIMER
        UsrTimer_stop(UsrSleepTimer);
      } else {
        // wakeup by RTC
        UsrWakeupTimer_stop();
      }
    }

    if (IS_RF_SLEEP(mTermParam)) {
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
static smacErrors_t SetRadioModeOnIdle(void) {
  smacErrors_t result = gErrorNoError_c;
  switch (mTermParam.RfMode) {
  default:
  case RFMODE_TXONLY:
    if (!gIsStandby) {
      gIsStandby = FALSE;
      gIsReceiving = FALSE;
      gIsSending = FALSE;

      /* standby */
      result = SMAC_Standby();
      if (result != gErrorNoError_c) {
        return result;
      }

      gIsStandby = TRUE;
    }
    break;

#if defined(BLD_ENABLE_RFMODE_RSSI_CHECK)
  case RFMODE_RSSI_CHECK:
    // start rssi check timer
    UsrTimer_start(UsrRssiCheckTimer, UsrTimerMode_Periodic, 100,
                   UsrTimerCallback);
    SetEvent(EVENT_RSSI_CHECK);

    // break through
#endif
  case RFMODE_TXRX:
    if (!gIsReceiving) {
      gIsStandby = FALSE;
      gIsReceiving = FALSE;
      gIsSending = FALSE;

      /* ready to receive */
      result = SMAC_RxStart();
      if (result != gErrorNoError_c) {
        return result;
      }

      gIsReceiving = TRUE;
    }
    break;

#if defined(BLD_ENABLE_RFMODE_BURST)
  case RFMODE_BURST:
    gIsStandby = FALSE;
    gIsReceiving = FALSE;
    gIsSending = FALSE;

    /* tx contiuous wave */
    if (!gIsSending && !gIsRetrySending) {
      const uint8_t payloadLen = (uint8_t)strlen((char *)mTermParam.SendData);

      result = SetPacketPayload(&gAppTxPacket, mTermParam.SendData, payloadLen);
      if (result != gErrorNoError_c) {
        /* error */
        break;
      }

      // start sending
      gWithParams = FALSE;
      result = StartSending(&gAppTxPacket, payloadLen, FALSE, NULL, NULL);
      if (result != gErrorNoError_c) {
        /* error */
        break;
      }
    }
    break;
#endif
#if defined(BLD_ENABLE_RFMODE_CW)
  case RFMODE_CW:
    gIsStandby = FALSE;
    gIsReceiving = FALSE;
    gIsSending = FALSE;

    /* tx contiuous wave */
    result = SMAC_TxContinuous(FALSE);
    break;
#endif
#if defined(BLD_ENABLE_RFMODE_TX_INFINITE)
  case RFMODE_TX_INFINITE:
    gIsStandby = FALSE;
    gIsReceiving = FALSE;
    gIsSending = FALSE;

    /* tx contiuous wave */
    result = SMAC_TxContinuous(TRUE);
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
static void ProcessEvent(uint32_t ev) {
  UsrPower_enterActiveRun();

  if (EVENT_PHY_CCA & ev) {
    ClearEvent(EVENT_PHY_CCA);

    uint32_t fiber = GetActiveFiber();
    if (fiber) {
      ProcessFiber(fiber);

      SetEvent(EVENT_PHY_CCA);
    }
  }

  if (EVENT_WKUP_TIMER & ev) {
    ClearEvent(EVENT_WKUP_TIMER);

    TimerWakeupProcess();
  }

  if (EVENT_INT_SW & ev) {
    ClearEvent(EVENT_INT_SW);

    CheckSwitchStateProcess();
  }

  /* IRQ from SUBGHZ */
  if (EVENT_RADIO_IRQ & ev) {
    ClearEvent(EVENT_RADIO_IRQ);

    SMAC_RadioIrqProcess();
  }

  /* receive UART Data */
  if (EVENT_UART_RX_DONE & ev) {
    ClearEvent(EVENT_UART_RX_DONE);

    CommInputDoneProcess();
  }

  if (EVENT_UART_TX_DONE & ev) {
    ClearEvent(EVENT_UART_TX_DONE);

    CommOutputDoneProcess();
  }

  if (EVENT_PHY_CCA_TIMER & ev) {
    ClearEvent(EVENT_PHY_CCA_TIMER);

    SMAC_TimerExpiredProcess(PhyCcaTimer);
  }

  if (EVENT_PHY_TX_TIMER & ev) {
    ClearEvent(EVENT_PHY_TX_TIMER);

    SMAC_TimerExpiredProcess(PhyTxTimer);
  }

  if (EVENT_PHY_RX_TIMER & ev) {
    ClearEvent(EVENT_PHY_RX_TIMER);

    SMAC_TimerExpiredProcess(PhyRxTimer);
  }

  if (EVENT_PHY_DUTY_TIMER & ev) {
    ClearEvent(EVENT_PHY_DUTY_TIMER);

    SMAC_TimerExpiredProcess(PhyDutyTimer);
  }

  if (EVENT_SMAC_CCA_BACKOFF_TIMER & ev) {
    ClearEvent(EVENT_SMAC_CCA_BACKOFF_TIMER);

    SMAC_TimerExpiredProcess(SmacCcaBackoffTimer);
  }

  if (EVENT_SMAC_TX_TIMER & ev) {
    ClearEvent(EVENT_SMAC_TX_TIMER);

    SMAC_TimerExpiredProcess(SmacTxTimer);
  }

  /* send time */
  if (EVENT_SEND_TIME & ev) {
    ClearEvent(EVENT_SEND_TIME);

    SendTimerProcess();
  }

#if defined(BLD_ENABLE_RFMODE_RSSI_CHECK)
  /* rssi check */
  if (EVENT_RSSI_CHECK & ev) {
    ClearEvent(EVENT_RSSI_CHECK);

    RssiCheckProcess();
  }
#endif

  /* WDT Reset */
  if (EVENT_WDT_RESET & ev) {
    ClearEvent(EVENT_WDT_RESET);

    /* process reset WDT */
    WDG_Refresh();
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
static void ProcessFiber(uint32_t fiber) {
  if (FIBER_PHY_CCA & fiber) {
    if (IS_SLEEP(mTermParam)) {
      UsrPower_enterLowPowerRun();
    }
    SMAC_FiberProcess(PhyCcaFiber);
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
static void StartCommInput(void) {
  if (!gUartRxActive) {
    gUartRxActive = TRUE;

    // start input uart
    switch (mTermParam.Format) {
    case FMT_ASCII:
      InputCommTextLine(CommInputDoneCallback, TRUE);
      break;

    case FMT_BINARY:
      InputCommBinaryBlock(CommInputDoneCallback, TRUE);
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
static void StopCommInput(void) {
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
static void CommInputDoneProcess(void) {
  smacErrors_t err = gErrorNoError_c;

  gUartRxActive = FALSE;

  switch (CommInputResult) {
  case CommInputResult_buffer:
    if (mTermParam.Format == FMT_ASCII && CommDataLen == 0) {
      // empty line (ignore)

      // restart uart rx
      StartCommInput();
    } else {
      Debug_Print("receive UART data\r\n");

      uint8_t *payloadData;
      uint8_t payloadDataLen;

      // analyze and copy comm input data
      switch (mTermParam.TransMode) {
      default:
      case TRANS_PAYLOAD:
        err = SetPacketPayload(&gAppTxPacket, CommDataBuffer, CommDataLen);
        if (err != gErrorNoError_c) {
          break;
        }

        payloadDataLen = CommDataLen;
        gWithParams = FALSE;
        break;

      case TRANS_FRAME:
        err = AnalyzeCommDataFrame(&gRouteParams, &gTransParams, &payloadData,
                                   &payloadDataLen);
        if (err != gErrorNoError_c) {
          break;
        }

        err = SetPacketPayload(&gAppTxPacket, payloadData, payloadDataLen);
        if (err != gErrorNoError_c) {
          break;
        }

        gWithParams = TRUE;
        break;
      }
      if (err != gErrorNoError_c) {
        // restart uart rx
        StartCommInput();
        return;
      }

      if (IS_TIMER_SLEEP(mTermParam) || IS_INT_SLEEP_1TX(mTermParam) ||
          IS_UART_SLEEP(mTermParam)) {
        // stop uart rx
        StopCommInput();
      } else {
        // restart uart rx
        StartCommInput();
      }

      // Wakeup if uart sleep
      if (IS_UART_SLEEP(mTermParam)) {
        LeaveSleep();
        Terminal_Print("exit uart sleep mode\r\n");
      }

      /* send RF data */
      if (gWithParams) {
        err = StartSending(&gAppTxPacket, payloadDataLen, TRUE, &gTransParams,
                           &gRouteParams);
      } else {
        err = StartSending(&gAppTxPacket, payloadDataLen, FALSE, NULL, NULL);
      }

      if (err != gErrorNoError_c) {
        /* error */
        SendDoneProcess(err);
      }
    }
    break;

  case CommInputResult_spCmd:
    switch (CommSpCmd) {
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
static void CommOutputDoneProcess(void) { gUartTxActive = FALSE; }

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
static smacErrors_t AnalyzeCommDataFrame(smacRouteParams_t *routeParams,
                                         SmacTransParams_t *transParams,
                                         uint8_t **payload,
                                         uint8_t *payloadLen) {
  uint8_t frameHead = 0;
  smacErrors_t err;

  switch (mTermParam.Protocol) {
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
  if (CommDataLen <= frameHead) {
    Terminal_Print("send data length too short\r\n");

    Processor_OutputOperationResult("NG 100");

    return (gErrorOutOfRange_c);
  }

  /* get parameters */
  err = SMAC_GetTransParams(transParams);
  if (err != gErrorNoError_c) {
    return err;
  }

  /* parse UART input parameter */
  transParams->addr.srcNode.panId =
      (((uint16_t)AsciiToHex(CommDataBuffer[0]) << 12) |
       ((uint16_t)AsciiToHex(CommDataBuffer[1]) << 8) |
       ((uint16_t)AsciiToHex(CommDataBuffer[2]) << 4) |
       ((uint16_t)AsciiToHex(CommDataBuffer[3])));
  transParams->addr.destNodeAddr =
      (((uint16_t)AsciiToHex(CommDataBuffer[4]) << 12) |
       ((uint16_t)AsciiToHex(CommDataBuffer[5]) << 8) |
       ((uint16_t)AsciiToHex(CommDataBuffer[6]) << 4) |
       ((uint16_t)AsciiToHex(CommDataBuffer[7])));
  transParams->addr.srcNode.nodeAddr = mTermParam.SrcId;

  /* check trans params */
  err = SMAC_CheckTransParams(transParams);
  if (err != gErrorNoError_c) {
    Terminal_Print("parameter error\r\n");

    Processor_OutputOperationResult("NG 100");

    return (gErrorOutOfRange_c);
  }

  /* get route params */
  err = SMAC_GetRouteParams(routeParams);
  if (err != gErrorNoError_c) {
    return err;
  }

  /* parse UART input parameter */
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
  switch (mTermParam.Protocol) {
#if defined(BLD_USE_LORA_R)
  case Protocol_LORA_R:
    routeParams->hop_cnt = ((AsciiToHex(CommDataBuffer[8]) << 4) |
                            (AsciiToHex(CommDataBuffer[9])));
    routeParams->route[0] = (((uint16_t)AsciiToHex(CommDataBuffer[10]) << 12) |
                             ((uint16_t)AsciiToHex(CommDataBuffer[11]) << 8) |
                             ((uint16_t)AsciiToHex(CommDataBuffer[12]) << 4) |
                             ((uint16_t)AsciiToHex(CommDataBuffer[13])));
    routeParams->route[1] = (((uint16_t)AsciiToHex(CommDataBuffer[14]) << 12) |
                             ((uint16_t)AsciiToHex(CommDataBuffer[15]) << 8) |
                             ((uint16_t)AsciiToHex(CommDataBuffer[16]) << 4) |
                             ((uint16_t)AsciiToHex(CommDataBuffer[17])));
    routeParams->endNodeAddr =
        (((uint16_t)AsciiToHex(CommDataBuffer[18]) << 12) |
         ((uint16_t)AsciiToHex(CommDataBuffer[19]) << 8) |
         ((uint16_t)AsciiToHex(CommDataBuffer[20]) << 4) |
         ((uint16_t)AsciiToHex(CommDataBuffer[21])));
    routeParams->oriNodeAddr = mTermParam.SrcId;
    break;
#endif

#if defined(BLD_USE_FSK_R)
  case Protocol_FSK_R:
    routeParams->hop_cnt = ((AsciiToHex(CommDataBuffer[8]) << 4) |
                            (AsciiToHex(CommDataBuffer[9])));
    routeParams->route[0] = (((uint16_t)AsciiToHex(CommDataBuffer[10]) << 12) |
                             ((uint16_t)AsciiToHex(CommDataBuffer[11]) << 8) |
                             ((uint16_t)AsciiToHex(CommDataBuffer[12]) << 4) |
                             ((uint16_t)AsciiToHex(CommDataBuffer[13])));
    routeParams->route[1] = (((uint16_t)AsciiToHex(CommDataBuffer[14]) << 12) |
                             ((uint16_t)AsciiToHex(CommDataBuffer[15]) << 8) |
                             ((uint16_t)AsciiToHex(CommDataBuffer[16]) << 4) |
                             ((uint16_t)AsciiToHex(CommDataBuffer[17])));
    routeParams->route[2] = (((uint16_t)AsciiToHex(CommDataBuffer[18]) << 12) |
                             ((uint16_t)AsciiToHex(CommDataBuffer[19]) << 8) |
                             ((uint16_t)AsciiToHex(CommDataBuffer[20]) << 4) |
                             ((uint16_t)AsciiToHex(CommDataBuffer[21])));
    routeParams->endNodeAddr =
        (((uint16_t)AsciiToHex(CommDataBuffer[22]) << 12) |
         ((uint16_t)AsciiToHex(CommDataBuffer[23]) << 8) |
         ((uint16_t)AsciiToHex(CommDataBuffer[24]) << 4) |
         ((uint16_t)AsciiToHex(CommDataBuffer[25])));
    routeParams->oriNodeAddr = mTermParam.SrcId;
    break;
#endif
  }
#endif

  /* check route params */
  err = SMAC_CheckRouteParams(routeParams, (protocolType_t)mTermParam.Protocol);
  if (err != gErrorNoError_c) {
    Terminal_Print("parameter error\r\n");

    Processor_OutputOperationResult("NG 100");

    return (gErrorOutOfRange_c);
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
static smacErrors_t SetPacketPayload(packet_t *packet, const uint8_t *data,
                                     uint8_t dataLen) {
  const uint8_t maxPayloadLength =
      MAX_PAYLOAD_LENGTH((protocolType_t)mTermParam.Protocol);

  /* too long */
  if (maxPayloadLength < dataLen) {
    Terminal_Print("send data length too long\r\n");
    Processor_OutputOperationResult("NG 100");

    return gErrorOutOfRange_c;
  }

  uint8_t *payload =
      getPacketPayload(packet, (protocolType_t)mTermParam.Protocol);
  memcpy(payload, data, dataLen);

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
static smacErrors_t StartSending(packet_t *packet, uint8_t payloadLen,
                                 bool_t withParams,
                                 SmacTransParams_t *transParams,
                                 smacRouteParams_t *routeParams) {
  SmacTransParams_t tmpTransParams;
  smacErrors_t err = gErrorNoError_c;

  gIsStandby = FALSE;
  gIsReceiving = FALSE;
  gIsSending = FALSE;

  if (!withParams) {
    if (TERMINAL == mTermParam.Mode) {
      err = SMAC_GetTransParams(&tmpTransParams);
      if (err != gErrorNoError_c) {
        return err;
      }

      Terminal_Print("<-- send data info[panid = %04X, srcid = %04X, dstid = "
                     "%04X, length = %02X]\r\n",
                     (unsigned)tmpTransParams.addr.srcNode.panId,
                     (unsigned)tmpTransParams.addr.srcNode.nodeAddr,
                     (unsigned)tmpTransParams.addr.destNodeAddr,
                     (unsigned)payloadLen);
    }
    err = SMAC_TxPacket(packet, payloadLen);
    if (err != gErrorNoError_c) {
      return err;
    }
  } else {
    Terminal_Print("<-- send data info[panid = %04X, srcid = %04X, dstid = "
                   "%04X, length = %02X]\r\n",
                   (unsigned)transParams->addr.srcNode.panId,
                   (unsigned)transParams->addr.srcNode.nodeAddr,
                   (unsigned)transParams->addr.destNodeAddr,
                   (unsigned)payloadLen);

    err = SMAC_TxPacketEx(packet, payloadLen, transParams, routeParams);
    if (err != gErrorNoError_c) {
      return err;
    }
  }

  gIsSending = TRUE;

  LED_Toggle(LED2);

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
static void SendDoneProcess(smacErrors_t result) {
  smacErrors_t err;

  gIsRetrySending = FALSE;

  if (IS_TIMER_SLEEP(mTermParam)) {
    switch (result) {
    default:
      break;

    case gErrorNoAck_c:
    case gErrorChannelBusy_c:
      if (gSendRetryCount < mTermParam.Retry) {
        // retry sending
        gIsRetrySending = TRUE;
        gSendRetryCount++;

        // calclulate backoff time
        gRetryBackoff = SMAC_GetRandom(0, mTermParam.Backoff);

        Terminal_Print("Retry Sending (%u/%u)\r\n", gSendRetryCount,
                       mTermParam.Retry, gRetryBackoff);
      }
      break;
    }
  }

  if (!gIsRetrySending) {
    gSendRetryCount = 0;

    /* send success */
    if (result == gErrorNoError_c) {
      Processor_OutputOperationResult("OK");
    }
    /* error */
    else {
      if (result == gErrorNoValidCondition_c) {
        Terminal_Print("send failed bacause now sending packet\r\n");
      }
      switch (result) {
      case gErrorOutOfRange_c:
        Terminal_Print("parameter error\r\n");
        Processor_OutputOperationResult("NG 100");
        break;

      default:
        Terminal_Print("send data failed\r\n");
        Processor_OutputOperationResult("NG 101");
        break;

      case gErrorChannelBusy_c:
        Terminal_Print("carrier sense failed\r\n");
        Processor_OutputOperationResult("NG 102");
        break;

      case gErrorNoAck_c:
        Terminal_Print("Ack Timeout\r\n");
        Processor_OutputOperationResult("NG 103");
        break;

      case gErrorTimeout_c:
        Terminal_Print("Send Failed\r\n");
        Processor_OutputOperationResult("NG 104");
        break;

      case gErrorTxDurationOver_c:
        Terminal_Print("Send Duration Limit Over\r\n");
        Processor_OutputOperationResult("NG 105");
        break;
      }
    }
  }

  gIsSending = FALSE;

#if defined(BLD_ENABLE_RFMODE_BURST)
  switch (mTermParam.RfMode) {
  case RFMODE_BURST:
    if (!gIsSending && !gIsRetrySending) {
      const uint8_t payloadLen = (uint8_t)strlen((char *)mTermParam.SendData);

      result = SetPacketPayload(&gAppTxPacket, mTermParam.SendData, payloadLen);
      if (result != gErrorNoError_c) {
        /* error */
        break;
      }

      // start sending
      gWithParams = FALSE;
      result = StartSending(&gAppTxPacket, payloadLen, FALSE, NULL, NULL);
      if (result != gErrorNoError_c) {
        /* error */
        break;
      }
    }
  }
#endif

  if (IS_TIMER_SLEEP(mTermParam)) {
    if (gIsRetrySending) {
      if (gRetryBackoff == 0) {
        // retry sending (no backoff)
        if (gWithParams) {
          err = StartSending(&gAppTxPacket, gSendPayloadLen, TRUE,
                             &gTransParams, &gRouteParams);
        } else {
          err = StartSending(&gAppTxPacket, gSendPayloadLen, FALSE, NULL, NULL);
        }
        if (err != gErrorNoError_c) {
          /* error */
          SendDoneProcess(err);
          return;
        }
      } else {
        // retry sending after backoff
        EnterSleep();
      }
    } else {
      Terminal_Print("enter timer sleep mode\r\n");

      EnterSleep();
    }
  } else if (IS_INT_SLEEP_1TX(mTermParam)) {
    Terminal_Print("enter interrupt sleep mode\r\n");

    EnterSleep();
  } else if (IS_UART_SLEEP(mTermParam)) {
    Terminal_Print("enter uart sleep mode\r\n");

    EnterSleep();
  }
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
static void
SmacCallback_onNotifyRxData(smacErrors_t result,
                            const SmacUser_RxResultParams_t *params) {
  if (result != gErrorNoError_c) {
    // error
  } else {
    LED_Toggle(LED1);

    Terminal_Print("--> receive data info[panid = %04X, srcid = %04X, dstid = "
                   "%04X, length = %02X]\r\n",
                   (unsigned)params->trans.addr.srcNode.panId,
                   (unsigned)params->trans.addr.srcNode.nodeAddr,
                   (unsigned)params->trans.addr.destNodeAddr,
                   (unsigned)params->payloadSize);

    // output length
    if (FMT_BINARY == mTermParam.Format) {
      uint8_t len = 0;
      if (MODE_ON == mTermParam.Rssi) {
        len += 4;
      }
      if (MODE_ON == mTermParam.RcvId) {
        len += 8;
#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
        switch (mTermParam.Protocol) {
          __cases_Protocol_R len += 4;
          break;
        }
#endif
      }
      len += params->payloadSize;

      Processor_Write1(len);
    }

    // output RSSI
    if (MODE_ON == mTermParam.Rssi) {
      uint16_t rssi4_neg =
          ((uint16_t)params->trans.phy.radio.rxStatus.rssi * 2);

      if (((int8_t)params->trans.phy.radio.rxStatus.snr) < 0) {
        rssi4_neg += -(int8_t)params->trans.phy.radio.rxStatus.snr;
      }
      const uint16_t rssi100_neg = ((uint16_t)rssi4_neg * 25) + 1000;

      Terminal_Print("RSSI(-%u.%02udBm):", (unsigned)rssi100_neg / 100,
                     (unsigned)rssi100_neg % 100);

      const int16_t rssi = -(int16_t)(rssi4_neg / 4) - 10;
      Processor_Print("%04X", (unsigned)(uint16_t)rssi);
    }

    // output address
    if (MODE_ON == mTermParam.RcvId) {
      const SmacTransAddrInfo_t *addr = &params->trans.addr;

      Terminal_Print("PAN ID(%04X):", (unsigned)addr->srcNode.panId);
      Processor_Print("%04X", (unsigned)addr->srcNode.panId);

      Terminal_Print("Src ID(%04X):", (unsigned)addr->srcNode.nodeAddr);
      Processor_Print("%04X", (unsigned)addr->srcNode.nodeAddr);

#if defined(BLD_USE_LORA_R) || defined(BLD_USE_FSK_R)
      const smacRouteParams_t *route = &params->route;

      switch (mTermParam.Protocol) {
        __cases_Protocol_R Terminal_Print("End ID(%04X):",
                                          (unsigned)route->oriNodeAddr);
        Processor_Print("%04X", (unsigned)route->oriNodeAddr);
        break;
      }
#endif
    }

    // output payload
    Terminal_Print("Receive Data(");
    uint8_t *payload =
        getPacketPayload(&gAppRxPacket, (protocolType_t)mTermParam.Protocol);

    Terminal_Write(payload, params->payloadSize);
    Terminal_Print(")\r\n");
    Processor_Write(payload, params->payloadSize);
    if (FMT_ASCII == mTermParam.Format) {
      Processor_Print("\r\n");
    }
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
static void SmacCallback_onTxDataComp(smacErrors_t result) {
  SendDoneProcess(result);
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
static void
SmacCallback_onNotifyForward(const SmacUser_RxResultParams_t *params,
                             bool_t *isRouting,
                             SmacTransOption_t *transOption) {
  smacErrors_t error;

  *isRouting = FALSE;

  if (mTermParam.Node == ROUTER) {
    SmacTransParams_t transParams;

    error = SMAC_GetTransParams(&transParams);
    if (error != gErrorNoError_c) {
      // fatal error
      Terminal_Print("routing failed\r\n");
      return;
    }

    *isRouting = TRUE;
    *transOption = transParams.option;

    LED_Toggle(LED2);
  }
}

static void SmacCallback_onNotifyForwardResult(smacErrors_t result) {
  if (result != gErrorNoError_c) {
    Terminal_Print("routing failed\r\n");
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
static void SmacCallback_TimerControl(stackTimerType_t type,
                                      uint32_t duration) {
  UsrTimerId timerId;

  switch (type) {
  case PhyCcaTimer:
    timerId = UsrPhyCcaTimer;
    ClearEvent(EVENT_PHY_CCA_TIMER);
    break;

  case PhyTxTimer:
    timerId = UsrPhyTxTimer;
    ClearEvent(EVENT_PHY_TX_TIMER);
    break;

  case PhyRxTimer:
    timerId = UsrPhyRxTimer;
    ClearEvent(EVENT_PHY_RX_TIMER);
    break;

  case PhyDutyTimer:
    timerId = UsrPhyDutyTimer;
    ClearEvent(EVENT_PHY_DUTY_TIMER);
    break;

  case SmacCcaBackoffTimer:
    timerId = UsrSmacCcaBackoffTimer;
    ClearEvent(EVENT_SMAC_CCA_BACKOFF_TIMER);
    break;

  case SmacTxTimer:
    timerId = UsrSmacTxTimer;
    ClearEvent(EVENT_SMAC_TX_TIMER);
    break;

  default:
    return;
  }

  if (duration) {
    UsrTimer_start(timerId, UsrTimerMode_OneShot, duration, UsrTimerCallback);
  } else {
    UsrTimer_stop(timerId);
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
static void SmacCallback_FiberControl(stackFiberType_t type, bool_t enable) {
  uint32_t fiber;

  switch (type) {
  case PhyCcaFiber:
    fiber = FIBER_PHY_CCA;
    break;

  default:
    return;
  }

  if (enable) {
    StartFiber(fiber);

    SetEvent(EVENT_PHY_CCA);
  } else {
    StopFiber(fiber);
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
static void CheckSwitchStateProcess(void) {
  if (IS_INT_SLEEP(mTermParam)) {
    if (HAL_GPIO_ReadPin(SW_INT_GPIO_Port, SW_INT_Pin)) {
      if (!gEnterSleep) {
        Terminal_Print("enter interrupt sleep mode\r\n");

        EnterSleep();
      }
    } else {
      if (gEnterSleep) {
        LeaveSleep();

        Terminal_Print("exit interrupt sleep mode\r\n");

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
static void TimerWakeupProcess(void) {
  smacErrors_t err;

  if (IS_TIMER_SLEEP(mTermParam)) {
    // wakeup RF
    LeaveSleep();

    if (gIsRetrySending) {
      // retry sending
      if (gWithParams) {
        err = StartSending(&gAppTxPacket, gSendPayloadLen, TRUE, &gTransParams,
                           &gRouteParams);
      } else {
        err = StartSending(&gAppTxPacket, gSendPayloadLen, FALSE, NULL, NULL);
      }
      if (err != gErrorNoError_c) {
        /* error */
        SendDoneProcess(err);
        return;
      }
    } else {
      Terminal_Print("exit timer sleep mode\r\n");

      if (0 != mTermParam.SendTime) {
        // start sending
        SendTimerProcess();
      } else {
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
static void SendTimerProcess(void) {
  smacErrors_t err;

  if (!gIsSending && !gIsRetrySending) {
    const uint8_t payloadLen = (uint8_t)strlen((char *)mTermParam.SendData);

    err = SetPacketPayload(&gAppTxPacket, mTermParam.SendData, payloadLen);
    if (err != gErrorNoError_c) {
      /* error */
      SendDoneProcess(err);
      return;
    }

    // start sending
    gWithParams = FALSE;
    err = StartSending(&gAppTxPacket, payloadLen, FALSE, NULL, NULL);
    if (err != gErrorNoError_c) {
      /* error */
      SendDoneProcess(err);
      return;
    }
  }
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
static void RssiCheckProcess(void) {
  const uint8_t rssi2_neg = SMAC_GetRssi();

  const uint16_t rssi10_neg = ((uint16_t)rssi2_neg * 5);
  Terminal_Print("-%u.%u\r\n", (unsigned)rssi10_neg / 10,
                 (unsigned)rssi10_neg % 10);
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
smacErrors_t SetRadioSleep(void) {
  smacErrors_t err;

  gIsStandby = FALSE;
  gIsReceiving = FALSE;
  gIsSending = FALSE;

  //-------------------------
  // sleep SX1261
  //-------------------------
  err = SMAC_Sleep(IS_RF_SLEEP_WARM(mTermParam)); // sleep
  if (err != gErrorNoError_c) {
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
smacErrors_t SetRadioWakeup(void) {
  smacErrors_t err;

  //--------------------------------
  // wakeup SX1261
  //--------------------------------
  gIsStandby = FALSE;
  gIsReceiving = FALSE;
  gIsSending = FALSE;

  err = SMAC_Standby(); // wakeup
  if (err != gErrorNoError_c) {
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
static void UsrTimerCallback(UsrTimerId timerId) {
  switch (timerId) {
  case UsrSleepTimer:
    SetEvent(EVENT_WKUP_TIMER);
    break;

  case UsrHeartBeatTimer:
    SetEvent(EVENT_WDT_RESET);
    break;

  case UsrTxIntervalTimer:
    SetEvent(EVENT_SEND_TIME);
    break;

#if defined(BLD_ENABLE_RFMODE_RSSI_CHECK)
  case UsrRssiCheckTimer:
    SetEvent(EVENT_RSSI_CHECK);
    break;
#endif

  case UsrPhyTxTimer:
    SetEvent(EVENT_PHY_TX_TIMER);
    break;

  case UsrPhyRxTimer:
    SetEvent(EVENT_PHY_RX_TIMER);
    break;

  case UsrPhyCcaTimer:
    SetEvent(EVENT_PHY_CCA_TIMER);
    break;

  case UsrPhyDutyTimer:
    SetEvent(EVENT_PHY_DUTY_TIMER);
    break;

  case UsrSmacCcaBackoffTimer:
    SetEvent(EVENT_SMAC_CCA_BACKOFF_TIMER);
    break;

  case UsrSmacTxTimer:
    SetEvent(EVENT_SMAC_TX_TIMER);
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
static void WakeupTimerCallback(void) { SetEvent(EVENT_WKUP_TIMER); }

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
static void CommInputDoneCallback(void) { SetEvent(EVENT_UART_RX_DONE); }

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
static void CommOutputDoneCallback(void) { SetEvent(EVENT_UART_TX_DONE); }
