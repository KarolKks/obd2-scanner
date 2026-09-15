#include "obd2.h"
#include <stddef.h>

void OBD2_BuildRequest(uint8_t service, uint8_t pid, CAN_Frame_t *tx_frame) 
{
    // Guard against NULL pointer dereference
    if (tx_frame == NULL) {
        return;
    }

    // 0x7DF is the standardized functional broadcast CAN ID for OBD-II requests
    tx_frame->id = 0x7DF; 
    tx_frame->is_extended = false;
    tx_frame->is_rtr = false;
    tx_frame->dlc = 8; // OBD-II on CAN standard requires an 8-byte frame length
    
    // Single Frame (ISO-TP) request layout:
    // data[0] = Number of additional payload bytes (2 bytes: Service + PID)
    tx_frame->data[0] = 0x02;
    tx_frame->data[1] = service; // 0x01 (Live Data) or 0x09 (Vehicle Info)
    tx_frame->data[2] = pid;     // 0x0C (Engine RPM) or 0x02 (VIN)
    
    // Standard requires padding unused bytes with zeros
    tx_frame->data[3] = 0x00;
    tx_frame->data[4] = 0x00;
    tx_frame->data[5] = 0x00;
    tx_frame->data[6] = 0x00;
    tx_frame->data[7] = 0x00;
}


bool OBD2_IsResponseValid(const CAN_Frame_t *rx_frame, uint8_t requested_service, uint8_t requested_pid)
{
    if (rx_frame == NULL) {
        return false;
    }

    // The ECU acknowledges a requested service by adding 0x40 to the service ID
    // Request 0x01 -> Positive response is 0x41 (0x01 + 0x40)
    if (rx_frame->data[1] != (requested_service + 0x40)) {
        return false;
    }
    
    // Verify that the response matches the requested PID
    if (rx_frame->data[2] != requested_pid) {
        return false;
    }
    
    return true;
}

float OBD2_ParseSensorValue(const CAN_Frame_t *rx_frame)
{
    if (rx_frame == NULL) {
        return -1.0f;
    }

    uint8_t pid = rx_frame->data[2]; // Parameter ID
    uint32_t a  = rx_frame->data[3]; // First data byte (Byte A)
    uint32_t b  = rx_frame->data[4]; // Second data byte (Byte B)

    switch (pid)
    {
        // Engine Load & Throttle Position: Formula (A * 100) / 255 [%]
        case OBD2_PID_ENGINE_LOAD:
        case OBD2_PID_THROTTLE_POS:
            return (float)(a * 100) / 255.0f;

        // Coolant Temperature: Formula A - 40 [°C] (Range: -40 to +215 °C)
        case OBD2_PID_COOLANT_TEMP:
            return (float)((int32_t)a - 40);

        // Engine RPM: Formula ((A * 256) + B) / 4 [RPM]
        case OBD2_PID_ENGINE_RPM:
            return (float)((a * 256) + b) / 4.0f;

        // Vehicle Speed: Formula A [km/h]
        case OBD2_PID_VEHICLE_SPEED:
            return (float)a;

        // Mass Air Flow (MAF): Formula ((A * 256) + B) / 100 [g/s]
        case OBD2_PID_MAF_AIR_FLOW:
            return (float)((a * 256) + b) / 100.0f;

        default:
            return -1.0f; // Return -1.0f for unhandled or unknown PIDs
    }
}

uint32_t OBD2_ParseBitmask(const CAN_Frame_t *rx_frame)
{
    if (rx_frame == NULL) {
        return 0;
    }

    // Explicit casting to uint32_t prevents bit-truncation during multi-byte left shifts.
    // Concatenates bytes in Big-Endian order: data[3] (MSB) to data[6] (LSB)
    uint32_t result = (((uint32_t)rx_frame->data[3]) << 24) |
                      (((uint32_t)rx_frame->data[4]) << 16) |
                      (((uint32_t)rx_frame->data[5]) << 8)  |
                      ((uint32_t)rx_frame->data[6]);

    return result;
}


void OBD2_BuildClearDTCRequest(CAN_Frame_t *tx_frame)
{
    if (tx_frame == NULL) {
        return;
    }

    tx_frame->id = 0x7DF;
    tx_frame->is_extended = false;
    tx_frame->is_rtr = false;
    tx_frame->dlc = 8;

    // Service 04 does not use PIDs, so payload length is only 1 byte
    tx_frame->data[0] = 0x01;
    tx_frame->data[1] = OBD2_SERVICE_04_CLEAR_DTC; // 0x04

    // Pad remaining bytes with zeros
    for (uint32_t i = 2; i <= 7; i++) {
        tx_frame->data[i] = 0x00; 
    }    
}

/**
 * @brief Parses 2-byte Diagnostic Trouble Codes (DTCs) from a raw payload buffer into an output array.
 */
uint8_t OBD2_ParseDTCs(const uint8_t *payload_buffer, uint16_t payload_length, uint16_t *dtc_list, uint8_t max_dtcs)
{
    uint8_t count = 0;

    // Safety checks against NULL pointers or zero capacity
    if (payload_buffer == NULL || dtc_list == NULL || max_dtcs == 0) {
        return 0;
    }

    // Iterate with step of 2: ensure at least 2 bytes remain and destination bounds are not exceeded
    for (size_t i = 0; (i + 1 < payload_length) && (count < max_dtcs); i += 2) {
        // Assemble 2 consecutive bytes into a 16-bit DTC (High Byte << 8 | Low Byte)
        uint16_t dtc = ((uint16_t)payload_buffer[i] << 8) | payload_buffer[i + 1];

        // 0x0000 denotes empty slot or padding; only store valid DTCs
        if (dtc != 0x0000) {
            dtc_list[count] = dtc;
            count++;
        }
    }

    return count; // Returns the total number of decoded DTCs
}