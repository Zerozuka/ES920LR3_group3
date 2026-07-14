/*******************************************************************************
* phy_radio file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#include "usr_common.h"

#include "stack/phy/phy_arib.h"
#include "stack/phy/phy.h"

/*******************************************************************************
********************************************************************************
* Private macros
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Private function declarations
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Private memory declarations
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Public functions
********************************************************************************
*******************************************************************************/

AribUnitChannel_t PhyArib_CheckBandWidth( protocolType_t protocol, const PhyRadio_ModulationParams_t* modParams )
{
    switch( protocol )
    {
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
    __cases_Protocol_LORA
        switch( modParams->LORA.bw )
        {
        default:
        case BANDWIDTH7_8:
        case BANDWIDTH10_4:
        case BANDWIDTH15_6:
        case BANDWIDTH20_8:
        case BANDWIDTH31_25:
        case BANDWIDTH41_7:
        case BANDWIDTH62_5:
        case BANDWIDTH125:
            return ARIB_UC1_BW200KHZ;

        case BANDWIDTH250:
            return ARIB_UC2_BW400KHZ;

        case BANDWIDTH500:
            return ARIB_UC3_BW600KHZ;
        }
        break;
#endif

#if defined(BLD_USE_FSK_R)
    __cases_Protocol_FSK
        switch( modParams->FSK.dr )
        {
        default:
        case RATE_50KBPS:
            return ARIB_UC1_BW200KHZ;

        case RATE_100KBPS:
        case RATE_150KBPS:
            return ARIB_UC2_BW400KHZ;

        case RATE_200KBPS:
        case RATE_250KBPS:
            return ARIB_UC3_BW600KHZ;
        }
        break;
#endif
    }
    return ARIB_UC1_BW200KHZ;   // dummy
}

smacErrors_t PhyArib_CheckTimes( protocolType_t protocol, const PhyRadio_ModulationParams_t* modParams, uint32_t txDuration, AribTimeInfo_t* info )
{
    bool_t isCH24_38 = FALSE;

    if( modParams->ch < 1 )
    {
        return gErrorOutOfRange_c;
    }

    switch( PhyArib_CheckBandWidth( protocol, modParams ) )
    {
    // unit channel 1
    case ARIB_UC1_BW200KHZ:
        // ch1-15
        if( modParams->ch <= MAX_CH_SEND_DURATION_4S_UC1 )
        {
            info->txDurationMax = SEND_DURATION_MAX_CH24_38;
            info->ccaTime       = CCA_TIME_CH24_38;
            info->txDutyTime    = DUTY_TIME_CH24_38;
            isCH24_38           = TRUE;
        }
        // ch16-38
        else if( modParams->ch <= MAX_CH_UC1 )
        {
            info->txDurationMax = SEND_DURATION_MAX_CH33_61_UC1;
            info->ccaTime       = CCA_TIME_CH33_61;
            if( txDuration <= 6 )
            {
                info->txDutyTime = DUTY_TIME_CH33_61_UC1_LT6MS;
            }
            else if( txDuration <= 200 )
            {
                info->txDutyTime = DUTY_TIME_CH33_61_UC1_LT200MS;
            }
            else if( txDuration <= SEND_DURATION_MAX_CH33_61_UC1 )
            {
                info->txDutyTime = DUTY_TIME_CH33_61_UC1_LT400MS(txDuration);
            }
            else
            {
                return gErrorTxDurationOver_c;
            }
        }
        else
        {
            return gErrorOutOfRange_c;
        }
        break;

    // unit channel 2
    case ARIB_UC2_BW400KHZ:
        // ch1-7
        if( modParams->ch <= MAX_CH_SEND_DURATION_4S_UC2 )
        {
            info->txDurationMax = SEND_DURATION_MAX_CH24_38;
            info->ccaTime       = CCA_TIME_CH24_38;
            info->txDutyTime    = DUTY_TIME_CH24_38;
            isCH24_38           = TRUE;
        }
        // ch8-19
        else if( modParams->ch <= MAX_CH_UC2 )
        {
            info->txDurationMax = SEND_DURATION_MAX_CH33_61_UC2;
            info->ccaTime       = CCA_TIME_CH33_61;
            if( txDuration <= 3 )
            {
                info->txDutyTime = DUTY_TIME_CH33_61_UC2_LT3MS;
            }
            else if( txDuration <= SEND_DURATION_MAX_CH33_61_UC2 )
            {
                info->txDutyTime = DUTY_TIME_CH33_61_UC2_LT200MS;
            }
            else
            {
                return gErrorTxDurationOver_c;
            }
        }
        else
        {
            return gErrorOutOfRange_c;
        }
        break;

    // unit channel 3
    case ARIB_UC3_BW600KHZ:
        // ch1-5
        if( modParams->ch <= MAX_CH_SEND_DURATION_4S_UC3 )
        {
            info->txDurationMax = SEND_DURATION_MAX_CH24_38;
            info->ccaTime       = CCA_TIME_CH24_38;
            info->txDutyTime    = DUTY_TIME_CH24_38;
            isCH24_38           = TRUE;
        }
        // ch6-12
        else if( modParams->ch <= MAX_CH_UC3 )
        {
            info->txDurationMax = SEND_DURATION_MAX_CH33_61_UC345;
            info->ccaTime       = CCA_TIME_CH33_61;
            if( txDuration <= 2 )
            {
                info->txDutyTime = DUTY_TIME_CH33_61_UC345_LT2MS;
            }
            else if( txDuration <= SEND_DURATION_MAX_CH33_61_UC345 )
            {
                info->txDutyTime = DUTY_TIME_CH33_61_UC345_LT100MS;
            }
            else
            {
                return gErrorTxDurationOver_c;
            }
        }
        else
        {
            return gErrorOutOfRange_c;
        }
        break;

    // unit channel 4
    case ARIB_UC4_BW800KHZ:
        // ch1-3
        if( modParams->ch <= MAX_CH_SEND_DURATION_4S_UC4 )
        {
            info->txDurationMax = SEND_DURATION_MAX_CH24_38;
            info->ccaTime       = CCA_TIME_CH24_38;
            info->txDutyTime    = DUTY_TIME_CH24_38;
            isCH24_38           = TRUE;
        }
        // ch4-9
        else if( modParams->ch <= MAX_CH_UC4 )
        {
            info->txDurationMax = SEND_DURATION_MAX_CH33_61_UC345;
            info->ccaTime       = CCA_TIME_CH33_61;
            if( txDuration <= 2 )
            {
                info->txDutyTime = DUTY_TIME_CH33_61_UC345_LT2MS;
            }
            else if( txDuration <= SEND_DURATION_MAX_CH33_61_UC345 )
            {
                info->txDutyTime = DUTY_TIME_CH33_61_UC345_LT100MS;
            }
            else
            {
                return gErrorTxDurationOver_c;
            }
        }
        else
        {
            return gErrorOutOfRange_c;
        }
        break;

    // unit channel 5
    case ARIB_UC5_BW1000KHZ:
        // ch1-3
        if( modParams->ch <= MAX_CH_SEND_DURATION_4S_UC5 )
        {
            info->txDurationMax = SEND_DURATION_MAX_CH24_38;
            info->ccaTime       = CCA_TIME_CH24_38;
            info->txDutyTime    = DUTY_TIME_CH24_38;
            isCH24_38           = TRUE;
        }
        // ch4-7
        else if( modParams->ch <= MAX_CH_UC5 )
        {
            info->txDurationMax = SEND_DURATION_MAX_CH33_61_UC345;
            info->ccaTime       = CCA_TIME_CH33_61;
            if( txDuration <= 2 )
            {
                info->txDutyTime = DUTY_TIME_CH33_61_UC345_LT2MS;
            }
            else if( txDuration <= SEND_DURATION_MAX_CH33_61_UC345 )
            {
                info->txDutyTime = DUTY_TIME_CH33_61_UC345_LT100MS;
            }
            else
            {
                return gErrorTxDurationOver_c;
            }
        }
        else
        {
            return gErrorOutOfRange_c;
        }
        break;

    default:
        return gErrorOutOfRange_c;
    }

    if( isCH24_38 )
    {
        info->txDutyTimeMax = DUTY_TIME_CH24_38;
    }
    else
    {
        switch( protocol )
        {
        default:
            return gErrorOutOfRange_c;
#if defined(BLD_USE_LORA_NR) || defined(BLD_USE_LORA_R)
        __cases_Protocol_LORA
            info->txDutyTimeMax = DUTY_TIME_MAX_CH24_38;

            switch( modParams->LORA.bw )
            {
            default:
                return gErrorOutOfRange_c;
            case BANDWIDTH7_8:
            case BANDWIDTH10_4:
            case BANDWIDTH15_6:
            case BANDWIDTH20_8:
            case BANDWIDTH31_25:
                break;

            case BANDWIDTH41_7:
                if( modParams->LORA.sf <= 5 )
                {
                    info->txDutyTimeMax = DUTY_TIME_CH33_61_UC1_LT200MS;
                }
                break;

            case BANDWIDTH62_5:
                if( modParams->LORA.sf <= 6 )
                {
                    info->txDutyTimeMax = DUTY_TIME_CH33_61_UC1_LT200MS;
                }
                break;

            case BANDWIDTH125:
                if( modParams->LORA.sf <= 7 )
                {
                    info->txDutyTimeMax = DUTY_TIME_CH33_61_UC1_LT200MS;
                }
                break;

            case BANDWIDTH250:
                info->txDutyTimeMax = DUTY_TIME_CH33_61_UC2_LT200MS;
                break;

            case BANDWIDTH500:
                info->txDutyTimeMax = DUTY_TIME_CH33_61_UC345_LT100MS;
                break;
            }
            break;
#endif

#if defined(BLD_USE_FSK_R)
        __cases_Protocol_FSK
            switch( modParams->FSK.dr )
            {
            default:
                return gErrorOutOfRange_c;

            case RATE_50KBPS:
                info->txDutyTimeMax = DUTY_TIME_CH33_61_UC1_LT200MS;
                break;

            case RATE_150KBPS:
            case RATE_100KBPS:
                info->txDutyTimeMax = DUTY_TIME_CH33_61_UC2_LT200MS;
                break;

            case RATE_200KBPS:
            case RATE_250KBPS:
                info->txDutyTimeMax = DUTY_TIME_CH33_61_UC345_LT100MS;
                break;
            }
            break;
#endif
        }
    }

    return gErrorNoError_c;
}
