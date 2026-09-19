#include "obd_multiframe.h"

void OBD_MF_Reset(OBD_MF_RxContext_t *ctx)
{
    if (ctx == NULL) return; 
    ctx->state = OBD_MF_STATE_IDLE;
    ctx->total_length = 0;
    ctx->received_length = 0;
    ctx->expected_seq_num = 0;

    for (uint32_t i = 0; i < OBD_MF_MAX_PAYLOAD_SIZE; i++) {
        ctx->buffer[i] = 0;
    }
}

/**
 * @brief Sends an ISO-TP Flow Control (FC) frame to authorize further transmission.
 * @note  This is a private static function used internally by the state machine.
 * 
 * @param response_id The CAN ID of the sender (e.g., 0x7E8). The FC frame 
 *                    will be sent to the corresponding physical request ID (e.g., 0x7E0).
 */
static void OBD_MF_SendFlowControl(uint32_t response_id)
{
    CAN_Frame_t can_frame;

    can_frame.dlc = 8;
    can_frame.is_extended = false;
    can_frame.is_rtr = false;

    can_frame.id = response_id - 8;
    
    // 0x30 = Flow Control (Clear to Send)
    // 0x00 = Block Size
    // 0x00 = STmin 
    can_frame.data[0] = 0x30;
    can_frame.data[1] = 0x00;
    can_frame.data[2] = 0x00;
    can_frame.data[3] = 0x00;
    can_frame.data[4] = 0x00;
    can_frame.data[5] = 0x00;
    can_frame.data[6] = 0x00;
    can_frame.data[7] = 0x00;

    CAN_Transmit(&can_frame, 50);
}

void OBD_MF_ProcessFrame(OBD_MF_RxContext_t *ctx, const CAN_Frame_t *rx_frame) 
{
    if (ctx == NULL || rx_frame == NULL) {
        return;
    }

    // Extract the upper 4 bits of the first byte to determine ISO-TP frame type
    uint8_t frame_type = rx_frame->data[0] >> 4;

    switch(frame_type)
    {
        case 0: // Type 0: Single Frame (message fits in a single frame)
        {
            // Length is the lower 4 bits of the first byte
            uint8_t len = rx_frame->data[0] & 0x0F;
            if (len > 7) {
                len = 7;
            }

            // Copy payload bytes starting from rx_frame->data[1]
            for (uint8_t i = 0; i < len; i++) {
                ctx->buffer[i] = rx_frame->data[1 + i];
            }

            ctx->total_length = len;
            ctx->received_length = len;
            ctx->state = OBD_MF_STATE_COMPLETE;
            break;
        }

        case 1: // Type 1: First Frame (start of multi-frame message, e.g. VIN)
        {
            // Total length is lower 4 bits of byte [0] combined with byte [1]
            ctx->total_length = ((rx_frame->data[0] & 0x0F) << 8) | rx_frame->data[1];

            // Protect against buffer overrun
            if (ctx->total_length > OBD_MF_MAX_PAYLOAD_SIZE) {
                ctx->state = OBD_MF_STATE_ERROR;
                break;
            }

            // First Frame always contains 6 payload bytes (indices 2 to 7)
            for (uint8_t i = 0; i < 6; i++) {
                ctx->buffer[i] = rx_frame->data[2 + i];
            }

            ctx->received_length = 6;
            ctx->expected_seq_num = 1; // Next packet must have sequence number 1
            ctx->state = OBD_MF_STATE_RECEIVING;

            // Send Flow Control frame to authorize further transmission
            OBD_MF_SendFlowControl(rx_frame->id);
            break;
        }

        case 2: // Type 2: Consecutive Frame (subsequent data packets)
        {
            // Ignore Consecutive Frame if not in receiving state
            if (ctx->state != OBD_MF_STATE_RECEIVING) {
                break;
            }

            // Verify sequence number of this frame
            uint8_t seq_num = rx_frame->data[0] & 0x0F;
            if (seq_num != ctx->expected_seq_num) {
                // Sequence number mismatch indicates lost frame
                ctx->state = OBD_MF_STATE_ERROR;
                break;
            }

            // Calculate remaining bytes for the full message
            uint16_t bytes_left = ctx->total_length - ctx->received_length;
            // Consecutive Frame carries up to 7 payload bytes
            uint8_t bytes_to_copy = (bytes_left > 7) ? 7 : (uint8_t)bytes_left;

            // Append received bytes to the buffer
            for (uint8_t i = 0; i < bytes_to_copy; i++) {
                ctx->buffer[ctx->received_length + i] = rx_frame->data[1 + i];
            }

            // Update received length
            ctx->received_length += bytes_to_copy;
            
            // Expect next packet; wrap sequence number from 15 to 0
            ctx->expected_seq_num = (ctx->expected_seq_num + 1) & 0x0F;

            // Check if all bytes have been received
            if (ctx->received_length >= ctx->total_length) {
                ctx->state = OBD_MF_STATE_COMPLETE;
            }
            break;
        }
    }
}