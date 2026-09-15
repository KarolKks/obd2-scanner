#ifndef CORE_OBD_MULTIFRAME_H
#define CORE_OBD_MULTIFRAME_H

#include <stdint.h>
#include <stdbool.h>
#include "can.h"

/**
 * @brief Maximum payload size for reassembled OBD-II multi-frame messages.
 *        Standard ISO-TP allows up to 4095, but for basic OBD-II (like VIN), 
 *        64 or 128 bytes is more than enough and saves RAM.
 */
#define OBD_MF_MAX_PAYLOAD_SIZE 128U

/**
 * @brief Current state of the multi-frame reception process.
 */
typedef enum {
    OBD_MF_STATE_IDLE = 0,      /*!< Ready to receive a new message */
    OBD_MF_STATE_RECEIVING,     /*!< Currently assembling consecutive frames */
    OBD_MF_STATE_COMPLETE,      /*!< Payload is complete and ready to be read */
    OBD_MF_STATE_ERROR          /*!< Error occurred (e.g., wrong sequence number) */
} OBD_MF_State_t;

/**
 * @brief Context structure for the multi-frame state machine.
 */
typedef struct {
    OBD_MF_State_t state;                     /*!< Current state of the machine */
    uint8_t buffer[OBD_MF_MAX_PAYLOAD_SIZE];  /*!< Buffer for the reassembled payload */
    uint16_t total_length;                    /*!< Expected total payload length */
    uint16_t received_length;                 /*!< Number of bytes received so far */
    uint8_t expected_seq_num;                 /*!< Expected Sequence Number for the next CF */
    uint32_t request_id;                      /*!< CAN ID used to send Flow Control (e.g., 0x7E0) */
} OBD_MF_RxContext_t;

/**
 * @brief Initializes or resets the multi-frame reception context.
 * 
 * @param[in,out] ctx Pointer to the multi-frame context structure.
 */
void OBD_MF_Reset(OBD_MF_RxContext_t *ctx);

/**
 * @brief Processes an incoming CAN frame and updates the assembly state.
 * @details If a First Frame (FF) is received, this function will automatically
 *          transmit a Flow Control (FC) frame via the CAN driver.
 * 
 * @param[in,out] ctx       Pointer to the multi-frame context structure.
 * @param[in]     rx_frame  Pointer to the received CAN frame.
 */
void OBD_MF_ProcessFrame(OBD_MF_RxContext_t *ctx, const CAN_Frame_t *rx_frame);

#endif /* CORE_OBD_MULTIFRAME_H */