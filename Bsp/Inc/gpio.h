#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>
#include <stdbool.h>

#include "stm32l4xx.h"
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_gpio.h"


/**
 * @brief Return status codes for GPIO driver operations.
 */
typedef enum {
    GPIO_OK       = 0,  /**< Operation successful */
    GPIO_ERR_INIT = 1   /**< Initialization error */
} GPIO_Status_t;

/**
 * @brief  Initializes standalone GPIO pins including the PA10 (Arduino D2) push-button input with pull-up.
 * @param  None.
 * @return GPIO_OK on success.
 */
GPIO_Status_t GPIO_Init(void);

/**
 * @brief  Reads the instantaneous physical state of the encoder push-button (PA10 / Arduino D2).
 * @note   Active LOW: returns true when button is physically pressed (shorted to GND).
 * @param  None.
 * @return true if button is pressed, false otherwise.
 */
bool GPIO_Button_IsPressed(void);


#endif /* GPIO_H */
