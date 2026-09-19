#ifndef CLK_H
#define CLK_H

#include <stdint.h>

#include "stm32l4xx_ll_rcc.h"
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_system.h"
#include "stm32l4xx_ll_pwr.h"
#include "stm32l4xx_ll_utils.h"
#include "stm32l4xx_ll_cortex.h"

#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief Clock module return status codes.
 */
typedef enum {
    CLK_OK       = 0,  /**< Clock configuration completed successfully */
    CLK_ERR_INIT = 1   /**< Oscillator, PLL, or clock switch timeout error */
} CLK_Status_t;

/**
 * @brief  Initializes the system clock tree to 80 MHz and configures SysTick.
 * @note   Configures Power Scale 1, enables HSI16 (16 MHz), configures Flash
 *         latency to 4 wait states with prefetch and cache enabled, sets PLL
 *         (M=1, N=10, R=2) to generate 80 MHz SYSCLK, updates SystemCoreClock,
 *         sets NVIC priority grouping to 4 bits (required by FreeRTOS), and
 *         starts the Cortex-M SysTick timer for 1 ms interrupts.
 * @param  None
 * @return CLK_OK on success, CLK_ERR_INIT on oscillator or PLL lock timeout.
 */
CLK_Status_t CLK_Init(void);

/**
 * @brief  Retrieves the current APB1 peripheral clock frequency.
 * @note   Used by communication peripherals such as CAN1 to calculate baudrate prescalers.
 * @param  None
 * @return Frequency of PCLK1 in Hertz (Hz).
 */
uint32_t CLK_GetPCLK1Freq(void);

/**
 * @brief  Returns the elapsed monotonic system time in milliseconds.
 * @note   Incremented inside the 1 ms SysTick interrupt.
 * @param  None
 * @return Number of elapsed milliseconds since system startup.
 */
uint32_t CLK_GetTick(void);

/**
 * @brief  Provides a blocking busy-wait delay using the monotonic SysTick counter.
 * @param  delay_ms Duration of the delay in milliseconds.
 * @return None
 */
void CLK_Delay(uint32_t delay_ms);

#endif /* CLK_H */
