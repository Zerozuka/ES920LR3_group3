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
#include "usr_common.h"
#include "stack/smac.h"
#include "stack/radio/radio.h"
#include "stack/radio/radio_bsp.h"


#define IS_TCXO_SUPPORTED               0U
#define RF_WAKEUP_TIME                  100U
#define IS_DCDC_SUPPORTED               1U

/**
  * @brief  Init Radio Switch
  * @retval BSP status
  */
int32_t BSP_RADIO_Init( void )
{
    GPIO_InitTypeDef  gpio_init_structure = {0};

    /* Enable the Radio Switch Clock */
    RF_SW_CTRL1_GPIO_CLK_ENABLE();

    /* Configure the Radio Switch pin */
    gpio_init_structure.Pin   = RF_SW_CTRL1_PIN;
    gpio_init_structure.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio_init_structure.Pull  = GPIO_NOPULL;
    gpio_init_structure.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init( RF_SW_CTRL1_GPIO_PORT, &gpio_init_structure );

    gpio_init_structure.Pin   = RF_SW_CTRL2_PIN;
    HAL_GPIO_Init( RF_SW_CTRL2_GPIO_PORT, &gpio_init_structure );

    HAL_GPIO_WritePin( RF_SW_CTRL2_GPIO_PORT, RF_SW_CTRL2_PIN, GPIO_PIN_RESET );
    HAL_GPIO_WritePin( RF_SW_CTRL1_GPIO_PORT, RF_SW_CTRL1_PIN, GPIO_PIN_RESET );

    return 0;
}

/**
  * @brief  DeInit Radio Swicth
  * @retval BSP status
  */
int32_t BSP_RADIO_DeInit( void )
{
    RF_SW_CTRL1_GPIO_CLK_DISABLE();

    /* Turn off switch */
    HAL_GPIO_WritePin( RF_SW_CTRL1_GPIO_PORT, RF_SW_CTRL1_PIN, GPIO_PIN_RESET );
    HAL_GPIO_WritePin( RF_SW_CTRL2_GPIO_PORT, RF_SW_CTRL2_PIN, GPIO_PIN_RESET );

    /* DeInit the Radio Switch pin */
    HAL_GPIO_DeInit( RF_SW_CTRL1_GPIO_PORT, RF_SW_CTRL1_PIN );
    HAL_GPIO_DeInit( RF_SW_CTRL2_GPIO_PORT, RF_SW_CTRL2_PIN );

    return 0;
}

/**
  * @brief  Configure Radio Switch.
  * @param  Config: Specifies the Radio RF switch path to be set.
  *         This parameter can be one of following parameters:
  *           @arg RADIO_SWITCH_OFF
  *           @arg RADIO_SWITCH_RX
  *           @arg RADIO_SWITCH_RFO_LP
  *           @arg RADIO_SWITCH_RFO_HP
  * @retval BSP status
  */
int32_t BSP_RADIO_ConfigRFSwitch( BSP_RADIO_Switch_TypeDef Config )
{
    switch( Config )
    {
    case RADIO_SWITCH_OFF:
    {
        /* Turn off switch */
        HAL_GPIO_WritePin(RF_SW_CTRL1_GPIO_PORT, RF_SW_CTRL1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(RF_SW_CTRL2_GPIO_PORT, RF_SW_CTRL2_PIN, GPIO_PIN_RESET);
        break;
    }
    case RADIO_SWITCH_RX:
    {
        /*Turns On in Rx Mode the RF Swicth */
        HAL_GPIO_WritePin(RF_SW_CTRL1_GPIO_PORT, RF_SW_CTRL1_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(RF_SW_CTRL2_GPIO_PORT, RF_SW_CTRL2_PIN, GPIO_PIN_RESET);
        break;
    }
    case RADIO_SWITCH_RFO_LP:
    {
        /*Turns On in Tx Low Power the RF Swicth */
        HAL_GPIO_WritePin(RF_SW_CTRL1_GPIO_PORT, RF_SW_CTRL1_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(RF_SW_CTRL2_GPIO_PORT, RF_SW_CTRL2_PIN, GPIO_PIN_SET);
        break;
    }
    case RADIO_SWITCH_RFO_HP:
    {
        /*Turns On in Tx High Power the RF Swicth */
        HAL_GPIO_WritePin(RF_SW_CTRL1_GPIO_PORT, RF_SW_CTRL1_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(RF_SW_CTRL2_GPIO_PORT, RF_SW_CTRL2_PIN, GPIO_PIN_SET);
        break;
    }
    default:
        break;
    }

    return 0;
}

/**
  * @brief  Return Board Configuration
  * @retval
  *  RADIO_CONF_RFO_LP_HP
  *  RADIO_CONF_RFO_LP
  *  RADIO_CONF_RFO_HP
  */
int32_t BSP_RADIO_GetTxConfig( void )
{
    return RADIO_CONF_RFO_LP;
}

/**
  * @brief  Get Radio Wake Time
  * @retval the wake upt time in ms
  */
int32_t BSP_RADIO_GetWakeUpTime( void )
{
    return  RF_WAKEUP_TIME;
}

/**
  * @brief  Get If TCXO is to be present on board
  * @note   never remove called by MW,
  * @retval return 1 if present, 0 if not present
  */
int32_t BSP_RADIO_IsTCXO( void )
{
    return IS_TCXO_SUPPORTED;
}

/**
  * @brief  Get If DCDC is to be present on board
  * @note   never remove called by MW,
  * @retval return 1 if present, 0 if not present
  */
int32_t BSP_RADIO_IsDCDC( void )
{
    return IS_DCDC_SUPPORTED;
}
