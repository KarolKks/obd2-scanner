#ifndef SPI_H
#define SPI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "stm32l4xx_ll_spi.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_bus.h"

typedef enum {
    SPI_STATUS_OK = 0,
    SPI_STATUS_ERROR,
    SPI_STATUS_BUSY,
    SPI_STATUS_TIMEOUT
} SPI_Status_t;

/* SPI Bus Handle Structure  */
typedef struct {
    SPI_TypeDef *instance;       /**< Pointer to hardware SPI peripheral (e.g., SPI1, SPI2) */
    uint32_t    baudrate_div;    /**< Clock prescaler divisor*/
    bool        is_initialized;  /**< Peripheral initialization state flag */
} SPI_Handle_t;

/**
 * @brief  Initializes the hardware SPI bus and corresponding bus GPIO lines (SCK, MISO, MOSI).
 * 
 * @param[in,out] hspi Pointer to the generic SPI handle structure.
 * 
 * @return SPI_STATUS_OK on success, SPI_STATUS_ERROR on invalid pointer or hardware failure.
 */
SPI_Status_t SPI_Init(SPI_Handle_t *hspi);

/**
 * @brief  Updates the SPI clock prescaler on the fly.
 * 
 * @param[in,out] hspi         Pointer to the generic SPI handle structure.
 * @param[in]     baudrate_div New baud rate prescaler (LL_SPI_BAUDRATEPRESCALER_DIVx).
 * 
 * @return SPI_STATUS_OK on success, SPI_STATUS_ERROR if the handle is uninitialized.
 */
SPI_Status_t SPI_SetBaudrate(SPI_Handle_t *hspi, uint32_t baudrate_div);

/**
 * @brief  Transmits a single byte over the SPI bus and simultaneously reads the incoming byte.
 * 
 * @param[in]  hspi    Pointer to the generic SPI handle structure.
 * @param[in]  tx_byte The 8-bit data value to send.
 * @param[out] rx_byte Pointer to store the received byte (can be NULL if ignoring RX).
 * 
 * @return SPI_STATUS_OK on success, or error/timeout code.
 */
SPI_Status_t SPI_TransferByte(SPI_Handle_t *hspi, uint8_t tx_byte, uint8_t *rx_byte);

/**
 * @brief  Sends a dummy clock byte (0xFF) to receive a single byte from the bus.
 * 
 * @param[in]  hspi    Pointer to the generic SPI handle structure.
 * @param[out] rx_byte Pointer to store the received byte.
 * 
 * @return SPI_STATUS_OK on success, or error/timeout code.
 */
SPI_Status_t SPI_ReceiveByte(SPI_Handle_t *hspi, uint8_t *rx_byte);

/**
 * @brief  Transmits a contiguous buffer of bytes over the SPI bus.
 * 
 * @param[in] hspi      Pointer to the generic SPI handle structure.
 * @param[in] tx_buffer Pointer to the byte array to transmit.
 * @param[in] length    Number of bytes to transmit.
 * 
 * @return SPI_STATUS_OK on success, SPI_STATUS_ERROR on NULL pointer or invalid length.
 */
SPI_Status_t SPI_TransmitBuffer(SPI_Handle_t *hspi, const uint8_t *tx_buffer, size_t length);

/**
 * @brief  Receives a contiguous block of bytes by transmitting dummy bytes (0xFF).
 * 
 * @param[in]  hspi      Pointer to the generic SPI handle structure.
 * @param[out] rx_buffer Pointer to the destination buffer to store received data.
 * @param[in]  length    Number of bytes to receive.
 * 
 * @return SPI_STATUS_OK on success, SPI_STATUS_ERROR on NULL pointer or invalid length.
 */
SPI_Status_t SPI_ReceiveBuffer(SPI_Handle_t *hspi, uint8_t *rx_buffer, size_t length);

#endif /* SPI_H */