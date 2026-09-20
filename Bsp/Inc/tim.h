/**
 * @file    tim.h
 * @brief   Hardware Timer low-level driver for rotary encoder interface.
 * @details Configures STM32L476RG TIM3 peripheral in Quadrature Encoder Interface
 *          mode on pins PB4 (TIM3_CH1) and PB5 (TIM3_CH2).
 */

#ifndef TIM_H
#define TIM_H

#include <stdint.h>
#include <stdbool.h>

#include "stm32l4xx.h"
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_tim.h"


/**
 * @brief Return status codes for timer driver operations.
 */
typedef enum {
    TIM_OK       = 0,  /**< Peripheral successfully initialized */
    TIM_ERR_INIT = 1   /**< Initialization failure */
} TIM_Status_t;

/**
 * @brief  Initializes TIM3 in Quadrature Encoder mode (X2 TI1) on PB4/PB5
 *         with 8-clock digital input filter.
 * @param  None.
 * @return TIM_OK on success, TIM_ERR_INIT on failure.
 */
TIM_Status_t TIM3_Encoder_Init(void);

/**
 * @brief  Reads the current hardware counter value of TIM3.
 * @param  None.
 * @return 16-bit counter value from TIM3->CNT.
 */
uint16_t TIM3_Encoder_GetCount(void);

/**
 * @brief  Sets the hardware counter value of TIM3.
 * @param  val Target 16-bit counter value.
 * @return None.
 */
void TIM3_Encoder_SetCount(uint16_t val);


#endif /* TIM_H */
