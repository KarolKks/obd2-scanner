#ifndef BSP_CLK_H
#define BSP_CLK_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32l4xx.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Status codes for BSP Clock initialization.
 */
typedef enum {
    BSP_CLK_OK = 0,
    BSP_CLK_ERR_INIT
} BSP_CLK_Status_t;

/**
 * @brief Initializes the system clock to 80 MHz using HSI16 + PLL.
 *        Configures AHB = 80 MHz, APB1 = 80 MHz (for CAN1), APB2 = 80 MHz.
 *        Enables Flash 4WS, prefetch, IC/DC caches.
 *        Configures NVIC Priority Group 4 for FreeRTOS compatibility.
 * @return BSP_CLK_OK on success, BSP_CLK_ERR_INIT otherwise.
 */
BSP_CLK_Status_t BSP_CLK_Init(void);

/**
 * @brief Returns the current APB1 peripheral clock frequency in Hz.
 * @return Frequency in Hz (e.g. 80000000).
 */
uint32_t BSP_CLK_GetPCLK1Freq(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_CLK_H */
