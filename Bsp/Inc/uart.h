#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Status codes for UART operations.
 */
typedef enum {
    UART_OK = 0,
    UART_ERR_NULL_PTR,
    UART_ERR_TIMEOUT,
    UART_ERR_BUSY
} UART_Status_t;

/**
 * @brief Callback function type for receiving a single byte asynchronously.
 * @param data Received byte.
 */
typedef void (*UART_RxCallback_t)(uint8_t data);

/**
 * @brief Initializes the UART hardware.
 * @note  Call this after the auto-generated CubeMX initialization if needed, 
 *        or use it to wrap the LL init functions.
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
 * @return UART_OK on success, BSP_UART_ERR_NULL_PTR if str is NULL.
 */
UART_Status_t UART_SendString(const char* str);

/**
 * @brief Registers a callback function for UART Rx interrupt.
 * @param callback Pointer to the function to be called when a byte is received.
 * @return UART_OK on success, BSP_UART_ERR_NULL_PTR if callback is NULL.
 */
UART_Status_t UART_RegisterRxCallback(UART_RxCallback_t callback);

/**
 * @brief Must be called from the UART hardware interrupt handler (e.g., USARTx_IRQHandler).
 * @note  This function checks flags and fires the registered callback.
 */
void UART_IRQHandler(void);

#endif /* BSP_UART_H */