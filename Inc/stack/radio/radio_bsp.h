/*
  ______                              _
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech

Description: SX126x driver specific target board functions implementation

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis and Gregory Cristian
*/
#ifndef __RADIO_BSP_H__
#define __RADIO_BSP_H__

#include "stack/radio/radio_driver.h"


/** @defgroup STM32WLXX_NUCLEO_RADIO_LOW_LEVEL_Exported_Types RADIO LOW LEVEL Exported Types
  * @{
  */

typedef enum
{
    RADIO_SWITCH_OFF    = 0,
    RADIO_SWITCH_RX     = 1,
    RADIO_SWITCH_RFO_LP = 2,
    RADIO_SWITCH_RFO_HP = 3,
} BSP_RADIO_Switch_TypeDef;

/**
  * @}
  */

/** @defgroup STM32WLXX_NUCLEO_RADIO_LOW_LEVEL_Exported_Constants RADIO LOW LEVEL Exported Constants
  * @{
  */

/** @defgroup STM32WLXX_NUCLEO_RADIO_LOW_LEVEL_RADIOCONFIG RADIO LOW LEVEL RADIO CONFIG Constants
  * @{
  */

#define RADIO_CONF_RFO_LP_HP  0
#define RADIO_CONF_RFO_LP     1
#define RADIO_CONF_RFO_HP     2

/**
  * @}
  */

/** @defgroup STM32WLXX_NUCLEO_RADIO_LOW_LEVEL_RFSWITCH RADIO LOW LEVEL RF SWITCH Constants
  * @{
  */
#define RF_SW_CTRL1_PIN                          GPIO_PIN_4
#define RF_SW_CTRL1_GPIO_PORT                    GPIOC
#define RF_SW_CTRL1_GPIO_CLK_ENABLE()            __HAL_RCC_GPIOC_CLK_ENABLE()
#define RF_SW_CTRL1_GPIO_CLK_DISABLE()           __HAL_RCC_GPIOC_CLK_DISABLE()

#define RF_SW_CTRL2_PIN                          GPIO_PIN_5
#define RF_SW_CTRL2_GPIO_PORT                    GPIOC
#define RF_SW_CTRL2_GPIO_CLK_ENABLE()            __HAL_RCC_GPIOC_CLK_ENABLE()
#define RF_SW_CTRL2_GPIO_CLK_DISABLE()           __HAL_RCC_GPIOC_CLK_DISABLE()

extern SubgRf_t SubgRf;
/**
 * @}
 */

/**
  * @}
  */

/** @defgroup STM32WLXX_NUCLEO_RADIO_LOW_LEVEL_Exported_Functions RADIO LOW LEVEL Exported Functions
  * @{
  */
int32_t BSP_RADIO_Init( void );
int32_t BSP_RADIO_DeInit( void );
int32_t BSP_RADIO_ConfigRFSwitch( BSP_RADIO_Switch_TypeDef Config );
int32_t BSP_RADIO_GetTxConfig( void );
int32_t BSP_RADIO_GetWakeUpTime( void );
int32_t BSP_RADIO_IsTCXO( void );
int32_t BSP_RADIO_IsDCDC( void );
#endif // __RADIO_BSP_H__
