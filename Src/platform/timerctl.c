/*******************************************************************************
* timerctl file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#include "usr_common.h"
#include "lptim.h"
#include "rtc.h"
#include "platform/timerctl.h"
#include "platform/pendsv.h"

/*******************************************************************************
********************************************************************************
* Private macros
********************************************************************************
*******************************************************************************/

// lptimer clock source     : LSE (32kHz)
#define LPTIM_CLK_FREQ      LSE_VALUE
#define MS_TO_CLKCNT(ms)    ( ( ((ms) / 1000) * LPTIM_CLK_FREQ ) + \
                              ( ((ms) % 1000) * LPTIM_CLK_FREQ / 1000 ) )   // ((ms) * LPTIM_CLK_FREQ / 1000)

/*******************************************************************************
********************************************************************************
* Private type definitions
********************************************************************************
*******************************************************************************/

typedef enum UsrTimerCtrlState_tag
{
    UsrTimerCtrlState_Idle = 0,
    UsrTimerCtrlState_Running,
    UsrTimerCtrlState_Splitting,
} UsrTimerCtrlState_t;

typedef enum UsrTimerState_tag
{
    UsrTimerState_Disabled = 0,
    UsrTimerState_Enabled,
    UsrTimerState_Expired,
} UsrTimerState_t;

typedef struct
{
    uint32_t        maxClkCnt;
    uint32_t        prescaler;
    uint32_t        clkScale;
} LptimMode_t;

typedef struct UsrTimerContext_tag
{
    UsrTimerState_t state;
    uint32_t        periodicClkCnt; // 0: OneShot, >0: Periodic clock count
    uint32_t        remainClkCnt;   // 0: disable, >0: clock count to next expire timing
    UsrTimerHandler handler;        // callback handler
} UsrTimerContext_t;

typedef struct HwTimerStatus_tag
{
    uint32_t        durationClkCnt;
    uint8_t         clkScale;       // source clock per timer clock
} HwTimerStatus_t;

/*******************************************************************************
********************************************************************************
* Private prototypes
********************************************************************************
*******************************************************************************/
static void UsrTimer_splitTimerProcess( uint32_t elapsedClkCnt );
static void UsrTimer_StartLPTIM( uint32_t clkCnt );
static void UsrTimer_StopLPTIM( void );

/*******************************************************************************
********************************************************************************
* Private memory declarations
********************************************************************************
*******************************************************************************/
static const LptimMode_t LptimModeTable[] = {
    //  maxClkCnt           prescaler              clkScale        cycle   precision
    {   1 * LPTIM_CLK_FREQ, LPTIM_PRESCALER_DIV1,     1     },  //    2s     0.03ms
    {   3 * LPTIM_CLK_FREQ, LPTIM_PRESCALER_DIV2,     2     },  //    4s     0.06ms
    {   7 * LPTIM_CLK_FREQ, LPTIM_PRESCALER_DIV4,     4     },  //    8s     0.12ms
    {  15 * LPTIM_CLK_FREQ, LPTIM_PRESCALER_DIV8,     8     },  //   16s     0.24ms
    {  30 * LPTIM_CLK_FREQ, LPTIM_PRESCALER_DIV16,   16     },  //   32s     0.49ms
    {  60 * LPTIM_CLK_FREQ, LPTIM_PRESCALER_DIV32,   32     },  //   64s     0.98ms
    { 120 * LPTIM_CLK_FREQ, LPTIM_PRESCALER_DIV64,   64     },  //  128s     1.15ms
    { 250 * LPTIM_CLK_FREQ, LPTIM_PRESCALER_DIV128, 128     },  //  256s     3.91ms
};

static UsrTimerCtrlState_t     g_ctrlState = UsrTimerCtrlState_Idle;
static UsrTimerContext_t       g_timerCtx[UsrTimer_MAX] = { UsrTimerState_Disabled };
static HwTimerStatus_t         g_lptmrStatus = { 0 };

static UsrWakeupTimerHandler   g_wakeupTimerHandler = NULL;

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

struct
{
    UsrTimerId      timerId;
    UsrTimerMode    mode;
    uint32_t        millisec;
    UsrTimerHandler handler;
} g_usrParams_UsrTimer_start;

void UsrTimer_start( UsrTimerId timerId, UsrTimerMode mode, uint32_t millisec, UsrTimerHandler handler )
{
    if( handler == NULL )
    {
        return;
    }

    usrDISABLE_INTERRUPTS();
    {
        g_usrParams_UsrTimer_start.timerId  = timerId;
        g_usrParams_UsrTimer_start.mode     = mode;
        g_usrParams_UsrTimer_start.millisec = millisec;
        g_usrParams_UsrTimer_start.handler  = handler;

        PendSV_SetEvent( PSV_EVENT_CALL_UsrTimer_start );
    }
    usrENABLE_INTERRUPTS();
}

void PendSvProcess_UsrTimer_start( void )
{
    UsrTimer_start_PSV( g_usrParams_UsrTimer_start.timerId,
                        g_usrParams_UsrTimer_start.mode,
                        g_usrParams_UsrTimer_start.millisec,
                        g_usrParams_UsrTimer_start.handler );
}

void UsrTimer_start_PSV( UsrTimerId timerId, UsrTimerMode mode, uint32_t millisec, UsrTimerHandler handler )
{
    UsrTimerContext_t* ctx = &g_timerCtx[timerId];
    uint32_t elapsedClkCnt;

    const uint32_t clkCnt = MS_TO_CLKCNT(millisec);

    switch( g_ctrlState )
    {
    // lptimer not started
    case UsrTimerCtrlState_Idle:
        // remember parameter
        ctx->handler        = handler;
        ctx->remainClkCnt   = clkCnt;
        ctx->periodicClkCnt = (mode == UsrTimerMode_Periodic ? clkCnt : 0);
        ctx->state          = UsrTimerState_Enabled;

        // start timer
        UsrTimer_StartLPTIM( clkCnt );
        g_ctrlState         = UsrTimerCtrlState_Running;
        break;

    // lptimer expired
    case UsrTimerCtrlState_Splitting:
        // remember parameter
        ctx->handler        = handler;
        ctx->remainClkCnt   = clkCnt;
        ctx->periodicClkCnt = (mode == UsrTimerMode_Periodic ? clkCnt : 0);
        ctx->state          = UsrTimerState_Enabled;
        break;

    // lptimer already started
    case UsrTimerCtrlState_Running:
        elapsedClkCnt = HAL_LPTIM_ReadCounter(&hlptim1) * g_lptmrStatus.clkScale;
        if( elapsedClkCnt > g_lptmrStatus.durationClkCnt )
        {
            elapsedClkCnt = g_lptmrStatus.durationClkCnt;
        }

        // remember parameter
        ctx->handler        = handler;
        ctx->remainClkCnt   = elapsedClkCnt + clkCnt;
        ctx->periodicClkCnt = (mode == UsrTimerMode_Periodic ? clkCnt : 0);
        ctx->state          = UsrTimerState_Enabled;

        // check if need to advance lptimer's next expire timing
        if( g_lptmrStatus.durationClkCnt < ctx->remainClkCnt )
        {
            // continue lptimer
        }
        // restart lptimer to advance next expire timing
        else
        {
            // stop timer
            UsrTimer_StopLPTIM();
            g_ctrlState    = UsrTimerCtrlState_Splitting;

            // reflect elapsed clock count at this time in remainCnt of each timer entry
            UsrTimer_splitTimerProcess( elapsedClkCnt );
        }
        break;
    }
}

struct
{
    UsrTimerId timerId;
} g_usrParams_UsrTimer_stop;

void UsrTimer_stop( UsrTimerId timerId )
{
    usrDISABLE_INTERRUPTS();
    {
        g_usrParams_UsrTimer_stop.timerId = timerId;

        PendSV_SetEvent( PSV_EVENT_CALL_UsrTimer_stop );
    }
    usrENABLE_INTERRUPTS();
}

void PendSvProcess_UsrTimer_stop( void )
{
    UsrTimer_stop_PSV( g_usrParams_UsrTimer_stop.timerId );
}

void UsrTimer_stop_PSV( UsrTimerId timerId )
{
    UsrTimerContext_t*  ctx = &g_timerCtx[timerId];
    bool_t              remain = FALSE;

    switch( g_ctrlState )
    {
    // lptimer not started
    case UsrTimerCtrlState_Idle:
    case UsrTimerCtrlState_Splitting:

        // clear existing status
        memset( ctx, 0, sizeof(*ctx) );
        break;

    // lptimer already started
    case UsrTimerCtrlState_Running:

        // clear existing status
        memset( ctx, 0, sizeof(*ctx) );     // ctx->state = UsrTimerState_Disabled;

        // check if active timer entry remaining
        for( int i = 0; i < UsrTimer_MAX; i++ )
        {
            switch( g_timerCtx[i].state )
            {
            case UsrTimerState_Disabled:
            case UsrTimerState_Expired:
                break;

            case UsrTimerState_Enabled:
                remain = TRUE;
                break;
            }
        }

        if( !remain )
        {
            // stop timer
            UsrTimer_StopLPTIM();
            g_ctrlState = UsrTimerCtrlState_Idle;
        }

        break;
    }
}

bool_t UsrTimer_isIdle( void )
{
    bool_t isIdle = FALSE;

    usrDISABLE_INTERRUPTS();
    {
        switch( g_ctrlState )
        {
        case UsrTimerCtrlState_Idle:
            isIdle = TRUE;
            break;

        case UsrTimerCtrlState_Running:
        case UsrTimerCtrlState_Splitting:
            break;
        }
    }
    usrENABLE_INTERRUPTS();

    return isIdle;
}

void UsrWakeupTimer_start( uint32_t seconds, UsrWakeupTimerHandler handler )
{
    bool_t init = FALSE;

    usrDISABLE_INTERRUPTS();
    {
        if( g_wakeupTimerHandler )
        {
            g_wakeupTimerHandler = NULL;
            init = TRUE;
        }
    }
    usrENABLE_INTERRUPTS();

    if( init )
    {
        HAL_RTCEx_DeactivateWakeUpTimer( &hrtc );
    }

    g_wakeupTimerHandler = handler;

    if( seconds - 1 < 0x10000 )
    {
        HAL_RTCEx_SetWakeUpTimer_IT( &hrtc, seconds - 1, RTC_WAKEUPCLOCK_CK_SPRE_16BITS, 0 );
    }
    else if( seconds - 1 < 0x20000 )
    {
        HAL_RTCEx_SetWakeUpTimer_IT( &hrtc, seconds - 1 - 0x10000, RTC_WAKEUPCLOCK_CK_SPRE_17BITS, 0 );
    }
}

void UsrWakeupTimer_stop( void )
{
    bool_t stop = FALSE;

    usrDISABLE_INTERRUPTS();
    {
        if( g_wakeupTimerHandler )
        {
            g_wakeupTimerHandler = NULL;
            stop = TRUE;
        }
    }
    usrENABLE_INTERRUPTS();

    if( stop )
    {
        HAL_RTCEx_DeactivateWakeUpTimer( &hrtc );
    }
}

bool_t UsrWakeupTimer_isIdle( void )
{
    bool_t isIdle = FALSE;

    usrDISABLE_INTERRUPTS();
    {
        if(g_wakeupTimerHandler == NULL)
        {
            isIdle = TRUE;
        }
    }
    usrENABLE_INTERRUPTS();

    return isIdle;
}

void HAL_LPTIM_CompareMatchCallback( LPTIM_HandleTypeDef *hlptim )
{
    PendSV_SetEvent( PSV_EVENT_LPTIM_Expired );
}

void PendSvProcess_LPTIM_Expired( void )
{
    uint32_t nextLptimClkCnt;

    switch( g_ctrlState )
    {
    case UsrTimerCtrlState_Idle:
    case UsrTimerCtrlState_Splitting:
        break;

    // lptimer already started
    case UsrTimerCtrlState_Running:
        nextLptimClkCnt = g_lptmrStatus.durationClkCnt + 1;

        // stop timer
        UsrTimer_StopLPTIM();
        g_ctrlState = UsrTimerCtrlState_Splitting;

        // reflect elapsed clock count at this time in remainCnt of each timer entry
        UsrTimer_splitTimerProcess( nextLptimClkCnt );
        break;
    }
}

void HAL_RTCEx_WakeUpTimerEventCallback( RTC_HandleTypeDef *hrtc )
{
    if( g_wakeupTimerHandler )
    {
        // callback
        g_wakeupTimerHandler();
    }
}

/*******************************************************************************
********************************************************************************
* Private functions
********************************************************************************
*******************************************************************************/

static void UsrTimer_splitTimerProcess( uint32_t elapsedClkCnt )
{
    int i;
    UsrTimerContext_t*  ctx;

    bool_t isExpired = FALSE;

    // split timer
    for( i = 0; i < UsrTimer_MAX; i++ )
    {
        ctx = &g_timerCtx[i];

        switch( ctx->state )
        {
        case UsrTimerState_Disabled:
        case UsrTimerState_Expired:
            break;

        case UsrTimerState_Enabled:

            // remain
            if( elapsedClkCnt < ctx->remainClkCnt )
            {
                // update remain count
                ctx->remainClkCnt -= elapsedClkCnt;
            }
            // expired
            else
            {
                ctx->state = UsrTimerState_Expired;
                isExpired  = TRUE;
            }
            break;
        }
    }

    // callback
    if( isExpired )
    {
        for(i = 0; i < UsrTimer_MAX; i++)
        {
            ctx = &g_timerCtx[i];

            switch( ctx->state )
            {
            case UsrTimerState_Disabled:
            case UsrTimerState_Enabled:
                break;

            case UsrTimerState_Expired:
                // callback
                ctx->handler( (UsrTimerId)i );
                break;
            }
        }
    }

    // restart or stop timer
    bool_t isRestartTimer = FALSE;
    uint32_t nextClkCnt   = MS_TO_CLKCNT(UsrTimer_DurationMax);

    for(i = 0; i < UsrTimer_MAX; i++)
    {
        ctx = &g_timerCtx[i];

        switch( ctx->state )
        {
        case UsrTimerState_Disabled:

            break;

        case UsrTimerState_Expired:

            if( ctx->periodicClkCnt == 0 )
            {
                // clear
                memset(ctx, 0, sizeof(*ctx));
                break;
            }

            // restart
            ctx->remainClkCnt = ctx->periodicClkCnt;
            ctx->state        = UsrTimerState_Enabled;

        // break through
        case UsrTimerState_Enabled:

            isRestartTimer = TRUE;

            // minOfRemainClkCnt is min of remainClkCnt
            if( nextClkCnt > ctx->remainClkCnt )
            {
                nextClkCnt = ctx->remainClkCnt;
            }
            break;
        }
    }

    // restart timer
    if( isRestartTimer )
    {
        // start timer
        UsrTimer_StartLPTIM( nextClkCnt );
        g_ctrlState = UsrTimerCtrlState_Running;
    }
    // stop timer
    else
    {
        g_ctrlState = UsrTimerCtrlState_Idle;
    }
}

static void UsrTimer_StartLPTIM( uint32_t clkCnt )
{
    const LptimMode_t* mode = NULL;
    uint32_t timeout;

    for( int i = 0; i < sizeof(LptimModeTable) / sizeof(*LptimModeTable); i++ )
    {
        mode = &LptimModeTable[i];
        if( clkCnt <= mode->maxClkCnt )
        {
            break;
        }
    }

    if( clkCnt > mode->maxClkCnt )
    {
        clkCnt = mode->maxClkCnt;
    }
    clkCnt += clkCnt % mode->clkScale;
    timeout = clkCnt / mode->clkScale;

    // remember status
    g_lptmrStatus.clkScale       = mode->clkScale;
    g_lptmrStatus.durationClkCnt = clkCnt;

    // start lptmr
    MX_LPTIM_SetClockSource( &hlptim1, LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC, mode->prescaler );
    HAL_LPTIM_TimeOut_Start_IT( &hlptim1, 65535, timeout );
}

static void UsrTimer_StopLPTIM( void )
{
    // clear status
    memset( &g_lptmrStatus, 0, sizeof(g_lptmrStatus) );

    // stop timer
    HAL_LPTIM_TimeOut_Stop_IT( &hlptim1 );
}
