#ifndef BSP_CAN_H
#define BSP_CAN_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32l4xx.h"
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_rcc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Status codes for BSP CAN operations.
 */
typedef enum {
    CAN_OK = 0,               /*!< Operation successful */
    CAN_ERR_NULL_PTR,         /*!< Pointer parameter is NULL */
    CAN_ERR_TIMEOUT,          /*!< Hardware timeout */
    CAN_ERR_BUSY,             /*!< Mailbox full or peripheral busy */
    CAN_ERR_FIFO_EMPTY,       /*!< No message available in RX FIFO */
    CAN_ERR_PARAM             /*!< Invalid parameter (e.g. DLC > 8) */
} CAN_Status_t;

/**
 * @brief CAN Frame structure (Standard & Extended).
 */
typedef struct {
    uint32_t id;              /*!< CAN Identifier (11-bit standard or 29-bit extended) */
    bool     is_extended;     /*!< false = Standard (11-bit), true = Extended (29-bit) */
    bool     is_rtr;          /*!< false = Data Frame, true = Remote Frame */
    uint8_t  dlc;             /*!< Data Length Code (0 - 8 bytes) */
    uint8_t  data[8];         /*!< Payload bytes */
} CAN_Frame_t;

/**
 * @brief Initializes the CAN1 peripheral.
 *        Configures GPIO (PA11-RX, PA12-TX AF9), sets baudrate to 500 kbps (for PCLK1 = 80 MHz,
 *        NBT = 16 tq, sample point = 87.5%), enables ABOM, and configures Filter 0 in accept-all mode.
 * @return CAN_OK on success, error code otherwise.
 */
CAN_Status_t CAN_Init(void);

/**
 * @brief Transmits a CAN frame using one of the hardware mailboxes.
 * @param frame Pointer to the CAN frame to send.
 * @param timeout_ms Timeout in milliseconds (0 for non-blocking attempt).
 * @return CAN_OK on success, CAN_ERR_BUSY if no mailbox free, CAN_ERR_TIMEOUT if TX timed out.
 */
CAN_Status_t CAN_Transmit(const CAN_Frame_t *frame, uint32_t timeout_ms);

/**
 * @brief Checks if a frame is available and reads it from FIFO 0.
 * @param frame Pointer to structure where received frame will be copied.
 * @return CAN_OK if frame read, CAN_ERR_FIFO_EMPTY if FIFO is empty.
 */
CAN_Status_t CAN_Receive(CAN_Frame_t *frame);

/**
 * @brief Checks whether at least one message is pending in RX FIFO 0.
 * @return true if message pending, false otherwise.
 */
bool CAN_IsRxPending(void);

/**
 * @brief Configures Filter Bank 0 to accept all CAN frames (logger / promiscuous mode).
 * @return CAN_OK on success.
 */
CAN_Status_t CAN_FilterAcceptAll(void);

/**
 * @brief Configures Filter Bank 0 for standard OBD-II ECU response range (0x7E8 - 0x7EF).
 * @return CAN_OK on success.
 */
CAN_Status_t CAN_FilterOBD2(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_CAN_H */