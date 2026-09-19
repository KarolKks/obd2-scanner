#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32l4xx.h"
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_usart.h"
#include "stm32l4xx_ll_rcc.h"
#include <stddef.h>


/**
 * @brief Status codes for UART operations.
 */
typedef enum {
    UART_OK = 0,
    UART_ERR_NULL_PTR,
    UART_ERR_TIMEOUT,
    UART_ERR_BUSY,
    UART_ERR_EMPTY
} UART_Status_t;

/**
 * @brief Callback function type for receiving a single byte asynchronously.
 * @param data Received byte.
 */
typedef void (*UART_RxCallback_t)(uint8_t data);

/**
 * @brief Initializes USART2 on PA2 (TX) and PA3 (RX) at 115200 baud, 8N1.
 *        Sets NVIC priority to 6 (safe for FreeRTOS syscalls).
 * @return UART_OK on success.
 */
UART_Status_t UART_Init(void);

/**
 * @brief Sends a single character over UART (blocking).
 * @param ch Character to send.
 * @return UART_OK on success.
 */
UART_Status_t UART_SendChar(char ch);

/**
 * @brief Sends a null-terminated string over UART (blocking).
 * @param str Pointer to the null-terminated string.
 * @return UART_OK on success, UART_ERR_NULL_PTR if str is NULL.
 */
UART_Status_t UART_SendString(const char* str);

/**
 * @brief Registers an asynchronous callback function called on character reception.
 * @param callback Pointer to the function.
 * @return UART_OK on success.
 */
UART_Status_t UART_RegisterRxCallback(UART_RxCallback_t callback);

/**
 * @brief Checks if a character is available in the internal RX ring buffer.
 * @return true if data available, false otherwise.
 */
bool UART_IsRxDataAvailable(void);

/**
 * @brief Reads a single character from the RX ring buffer.
 * @param[out] out_char Pointer to store the received character.
 * @return UART_OK on success, UART_ERR_EMPTY if buffer is empty.
 */
UART_Status_t UART_ReadChar(char *out_char);

/**
 * @brief Core UART ISR processing function. Handles RXNE, error clearing, and callbacks.
 */
void UART_IRQHandler(void);

#endif /* UART_H */