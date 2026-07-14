/*******************************************************************************
* phy_arib header file.
*
* (c) Copyright 2021, EASEL, Inc.  All rights reserved.
*
* No part of this document may be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from EASEL.
*
*******************************************************************************/

#ifndef __PHY_ARIB_H__
#define __PHY_ARIB_H__

#include "stack/phy/phy_radio.h"

/*******************************************************************************
********************************************************************************
* Public macros
********************************************************************************
*******************************************************************************/

// send duty time
#define DUTY_TIME_CH24_38                       50              // 50ms
#define DUTY_TIME_CH33_61_UC1_LT6MS             0               //  0ms
#define DUTY_TIME_CH33_61_UC1_LT200MS           2               //  2ms
#define DUTY_TIME_CH33_61_UC1_LT400MS(txtime)   ((txtime)*10)   //  sendTime * 10
#define DUTY_TIME_CH33_61_UC2_LT3MS             0               //  0ms
#define DUTY_TIME_CH33_61_UC2_LT200MS           2               //  2ms
#define DUTY_TIME_CH33_61_UC345_LT2MS           0               //  0ms
#define DUTY_TIME_CH33_61_UC345_LT100MS         2               //  2ms
#define DUTY_TIME_MAX_CH24_38                   4000            // 4000ms

// max of sending duration
#define SEND_DURATION_MAX_CH24_38               4000            //  4s
#define SEND_DURATION_MAX_CH33_61_UC1           400             // 400ms
#define SEND_DURATION_MAX_CH33_61_UC2           200             // 200ms
#define SEND_DURATION_MAX_CH33_61_UC345         100             // 100ms

// carrier sense time
#define CCA_TIME_CH24_38                        5               // 5ms
#define CCA_TIME_CH33_61                        1               // 128us

// max of channel of range range of sending duration 4sec
#define MAX_CH_SEND_DURATION_4S_UC1             15              // 1-15CH (ARIB 24-38CH)
#define MAX_CH_SEND_DURATION_4S_UC2             7               // 1-7CH  (ARIB 24-37CH)
#define MAX_CH_SEND_DURATION_4S_UC3             5               // 1-5CH  (ARIB 24-38CH)
#define MAX_CH_SEND_DURATION_4S_UC4             3               // 1-3CH  (ARIB 24-35CH)
#define MAX_CH_SEND_DURATION_4S_UC5             3               // 1-3CH  (ARIB 24-38CH)

// max of channel
#define MAX_CH_UC1          38     // 1-38CH
#define MAX_CH_UC2          19     // 1-7CH
#define MAX_CH_UC3          12     // 1-5CH
#define MAX_CH_UC4           9     // 1-3CH
#define MAX_CH_UC5           7     // 1-3CH

// definition of channel frequency
#define FREQ_BASE_UC1   920600000  // 920.6MHz
#define FREQ_STEP_UC1      200000  //   200KHz
#define BANDWIDTH_UC1      200000  //   200KHz
#define FREQ_BASE_UC2   920700000  // 920.7MHz
#define FREQ_STEP_UC2      400000  //   400KHz
#define BANDWIDTH_UC2      400000  //   400KHz
#define FREQ_BASE_UC3   920800000  // 920.8MHz
#define FREQ_STEP_UC3      600000  //   600KHz
#define BANDWIDTH_UC3      600000  //   600KHz
#define FREQ_BASE_UC4   920900000  // 920.9MHz
#define FREQ_STEP_UC4      800000  //   800KHz
#define BANDWIDTH_UC4      800000  //   800KHz
#define FREQ_BASE_UC5   921000000  // 921.0MHz
#define FREQ_STEP_UC5     1000000  //  1000KHz
#define BANDWIDTH_UC5     1000000  //  1000KHz

/*******************************************************************************
********************************************************************************
* Public type definitions
********************************************************************************
*******************************************************************************/

// unit channel
typedef enum
{
    ARIB_UC1_BW200KHZ,
    ARIB_UC2_BW400KHZ,
    ARIB_UC3_BW600KHZ,
    ARIB_UC4_BW800KHZ,
    ARIB_UC5_BW1000KHZ,
} AribUnitChannel_t;

// time info
typedef struct
{
    uint32_t txDurationMax; // max of sending duration
    uint32_t ccaTime;       // carrier sense time
    uint32_t txDutyTime;    // sending duty time
    uint32_t txDutyTimeMax; // sending duty time
} AribTimeInfo_t;

/*******************************************************************************
********************************************************************************
* Public memory declarations
********************************************************************************
*******************************************************************************/

/*******************************************************************************
********************************************************************************
* Public Prototypes
********************************************************************************
*******************************************************************************/

AribUnitChannel_t   PhyArib_CheckBandWidth( protocolType_t protocol, const PhyRadio_ModulationParams_t* modParams );
smacErrors_t        PhyArib_CheckTimes( protocolType_t protocol, const PhyRadio_ModulationParams_t* modParams, uint32_t txDuration, AribTimeInfo_t* info );

#endif //__PHY_ARIB_H__
