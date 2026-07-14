/*******************************************************************************
* params file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#include "usr_common.h"
#include "stm32wlxx_hal.h"
#include "app/params.h"
#include "app/memmap.h"

/*******************************************************************************
********************************************************************************
* Private macros
********************************************************************************
*******************************************************************************/

#define ESLORA      "ESLORA"

#define ALIGNUP(value, align) (((value)+(align)-1) & ~((align)-1))

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

/*******************************************************************************
********************************************************************************
* Public memory declarations
********************************************************************************
*******************************************************************************/

TERMINAL_PARAM  mTermParam;

const uint8_t NullAesKey[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/*******************************************************************************
********************************************************************************
* Public functions
********************************************************************************
*******************************************************************************/

/*******************************************************************************
*
* TermParam_loadFromRom
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
void TermParam_loadFromRom( void )
{
    int ret = 0;

    memset( &mTermParam, 0x00, sizeof(mTermParam) );

    *((uint16_t*)&mTermParam.Once[0]) = FLASH_ReadShortWord( FLASH_OFS_ONCE     );
    *((uint16_t*)&mTermParam.Once[2]) = FLASH_ReadShortWord( FLASH_OFS_ONCE + 2 );
    *((uint16_t*)&mTermParam.Once[4]) = FLASH_ReadShortWord( FLASH_OFS_ONCE + 4 );

    ret = strncmp( (char*)mTermParam.Once, ESLORA, 6 );
    if( 0 == ret )
    {
        /* baudrate */
        mTermParam.Baudrate     = FLASH_ReadLongWord( FLASH_OFS_BAUDRATE );

        /* sleeptime */
        mTermParam.SleepTime    = FLASH_ReadLongWord( FLASH_OFS_SLEEPTIME );

        /* power */
        mTermParam.Power        = (int32_t)FLASH_ReadLongWord( FLASH_OFS_POWER );

        /* ack */
        mTermParam.Ack          = FLASH_ReadShortWord( FLASH_OFS_ACK );

        /* rate */
        mTermParam.Rate         = FLASH_ReadShortWord( FLASH_OFS_RATE );

        /* bw */
        mTermParam.Bw           = FLASH_ReadShortWord( FLASH_OFS_BW );

        /* channel */
        mTermParam.Channel      = FLASH_ReadShortWord( FLASH_OFS_CHANNEL );

        /* dstid */
        mTermParam.DstId        = FLASH_ReadShortWord( FLASH_OFS_DSTID );

        /* endid */
        mTermParam.EndId        = FLASH_ReadShortWord( FLASH_OFS_ENDID );

        /* hopcount */
        mTermParam.HopCnt       = FLASH_ReadShortWord( FLASH_OFS_HOPCNT );

        /* mode */
        mTermParam.Mode         = FLASH_ReadShortWord( FLASH_OFS_MODE );

        /* node */
        mTermParam.Node         = FLASH_ReadShortWord( FLASH_OFS_NODE );

        /* operation */
        mTermParam.Operation    = FLASH_ReadShortWord( FLASH_OFS_OPERATION );

        /* panid */
        mTermParam.PanId        = FLASH_ReadShortWord( FLASH_OFS_PANID );

        /* rcvid */
        mTermParam.RcvId        = FLASH_ReadShortWord( FLASH_OFS_RCVID );

        /* retry */
        mTermParam.Retry        = FLASH_ReadShortWord( FLASH_OFS_RETRY );

        /* route1 */
        mTermParam.Route[0]     = FLASH_ReadShortWord( FLASH_OFS_ROUTE1 );

        /* route2 */
        mTermParam.Route[1]     = FLASH_ReadShortWord( FLASH_OFS_ROUTE2 );

        /* route3 */
        mTermParam.Route[2]     = FLASH_ReadShortWord( FLASH_OFS_ROUTE3 );

        /* rssi */
        mTermParam.Rssi         = FLASH_ReadShortWord( FLASH_OFS_RSSI );

        /* sf */
        mTermParam.Sf           = FLASH_ReadShortWord( FLASH_OFS_SF );

        /* sleep */
        mTermParam.Sleep        = FLASH_ReadShortWord( FLASH_OFS_SLEEP );

        /* lpmode(mcu) */
        mTermParam.LpModeMcu    = FLASH_ReadShortWord( FLASH_OFS_LPMODE_MCU );

        /* lpmode(rf) */
        mTermParam.LpModeRf     = FLASH_ReadShortWord( FLASH_OFS_LPMODE_RF );

        /* srcid */
        mTermParam.SrcId        = FLASH_ReadShortWord( FLASH_OFS_SRCID );

        /* transmode */
        mTermParam.TransMode    = FLASH_ReadShortWord( FLASH_OFS_TRANSMODE );

        /* format */
        mTermParam.Format       = FLASH_ReadShortWord( FLASH_OFS_FORMAT );

        /* rfmode */
        mTermParam.RfMode       = FLASH_ReadShortWord( FLASH_OFS_RFMODE );

        /* protocol */
        mTermParam.Protocol     = FLASH_ReadShortWord( FLASH_OFS_PROTOCOL );

        /* rxboost */
        mTermParam.RxBoost      = FLASH_ReadShortWord( FLASH_OFS_RXBOOST );

        /* sendtime */
        mTermParam.SendTime     = FLASH_ReadLongWord( FLASH_OFS_SENDTIME );

        /* backoff */
        mTermParam.Backoff 		= FLASH_ReadLongWord(FLASH_OFS_BACKOFF);

        /* senddata */
        FLASH_ReadBytes( mTermParam.SendData, FLASH_OFS_SENDDATA, sizeof(mTermParam.SendData) );

        /* aeskey */
        FLASH_ReadBytes( mTermParam.AesKey, FLASH_OFS_AESKEY, sizeof(mTermParam.AesKey) );
    }
    else
    {
        mTermParam.Once[0] = 'E';
        mTermParam.Once[1] = 'S';
        mTermParam.Once[2] = 'L';
        mTermParam.Once[3] = 'O';
        mTermParam.Once[4] = 'R';
        mTermParam.Once[5] = 'A';

        TermParam_loadDefault();
    }
}

/*******************************************************************************
*
* TermParam_saveToRom
*
* Interface assumptions:
*     None
*
* Return value:
*     result
*
*******************************************************************************/
uint8_t TermParam_saveToRom( void )
{
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;
    uint8_t err = 0;

    /* Unlock the Flash to enable the flash control register access */
    HAL_FLASH_Unlock();

    /* Fill EraseInit structure*/
    EraseInitStruct.TypeErase    = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.Page         = 127;
    EraseInitStruct.NbPages      = 1;
    HAL_FLASHEx_Erase( &EraseInitStruct, &SectorError );

    /* Note: If an erase operation in Flash memory also concerns data in
     * the data or instruction cache, you have to make sure that these data
     * are rewritten before they are accessed during code execution.
     * If this cannot be done safely, it is recommended to flush the caches
     * by setting the DCRST and ICRST bits in the FLASH_CR register.
     */
    __HAL_FLASH_DATA_CACHE_DISABLE();
    __HAL_FLASH_INSTRUCTION_CACHE_DISABLE();
    __HAL_FLASH_DATA_CACHE_RESET();

    __HAL_FLASH_INSTRUCTION_CACHE_RESET();
    __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
    __HAL_FLASH_DATA_CACHE_ENABLE();

    /* Program the user Flash area word by word */
    uint64_t* pBuf = (uint64_t*)&mTermParam;
    int len = sizeof(mTermParam);
    for( int i = 0; i < len; i += 8 )
    {
        HAL_FLASH_Program( FLASH_TYPEPROGRAM_DOUBLEWORD, 0x0803F800 + i, *pBuf );
        pBuf++;
    }

    /* Lock the Flash to disable the flash control register access */
    HAL_FLASH_Lock();

    // verify
    if( memcmp( (void*)FLASH_PARAM_TOP, &mTermParam, sizeof(mTermParam) ) )
    {
        err = 6;
    }

    return err;
}

/*******************************************************************************
*
* TermParam_loadDefault
*
* Interface assumptions:
*     None
*
* Return value:
*     None
*
*******************************************************************************/
void TermParam_loadDefault( void )
{
    /* baudrate */
    mTermParam.Baudrate     = DEFAULT_Baudrate;

    /* sleeptime */
    mTermParam.SleepTime    = DEFAULT_SleepTime;

    /* power */
    mTermParam.Power        = DEFAULT_Power;

    /* ack */
    mTermParam.Ack          = DEFAULT_Ack;

    /* rate */
    mTermParam.Rate         = DEFAULT_Rate;

    /* bw */
    mTermParam.Bw           = DEFAULT_Bw;

    /* channel */
    mTermParam.Channel      = DEFAULT_Channel;

    /* dstid */
    mTermParam.DstId        = DEFAULT_DstId;

    /* endid */
    mTermParam.EndId        = DEFAULT_EndId;

    /* hopcount */
    mTermParam.HopCnt       = DEFAULT_HopCnt;

    /* node */
    mTermParam.Node         = DEFAULT_Node;

    /* operation */
    mTermParam.Operation    = DEFAULT_Operation;

    /* panid */
    mTermParam.PanId        = DEFAULT_PanId;

    /* rcvid */
    mTermParam.RcvId        = DEFAULT_RcvId;

    /* retry */
    mTermParam.Retry        = DEFAULT_Retry;

    /* route1 */
    mTermParam.Route[0]     = DEFAULT_Route;

    /* route2 */
    mTermParam.Route[1]     = DEFAULT_Route;

    /* route3 */
    mTermParam.Route[2]     = DEFAULT_Route;

    /* rssi */
    mTermParam.Rssi         = DEFAULT_Rssi;

    /* sf */
    mTermParam.Sf           = DEFAULT_Sf;

    /* sleep */
    mTermParam.Sleep        = DEFAULT_Sleep;

    /* low power mode (MCU) */
    mTermParam.LpModeMcu    = DEFAULT_LpModeMcu;

    /* low power mode (RF) */
    mTermParam.LpModeRf     = DEFAULT_LpModeRf;

    /* srcid */
    mTermParam.SrcId        = DEFAULT_SrcId;

    /* transmode */
    mTermParam.TransMode    = DEFAULT_TransMode;

    /* format */
    mTermParam.Format       = DEFAULT_Format;

    /* rfmode */
    mTermParam.RfMode       = DEFAULT_RfMode;

    /* protocol */
    mTermParam.Protocol     = DEFAULT_Protocol;

    /* rxboost */
    mTermParam.RxBoost      = DEFAULT_RxBoost;

    /* sendtime */
    mTermParam.SendTime     = DEFAULT_SendTime;

    /* backoff */
    mTermParam.Backoff		= DEFAULT_Backoff;

    /* senddata */
    strcpy( (char*)mTermParam.SendData, DEFAULT_SendData );

    /* aeskey */
    memcpy( (char*)mTermParam.AesKey, DEFAULT_AesKey, sizeof(mTermParam.AesKey) );
}
