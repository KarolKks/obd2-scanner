#include "obd2.h"

void OBD2_BuildRequest(uint8_t service, uint8_t pid, CAN_Frame_t *tx_frame) 
{
    OBD2_BuildGenericRequest(service, &pid, 1, tx_frame);
}

void OBD2_BuildGenericRequest(uint8_t service, const uint8_t *payload, uint8_t length, CAN_Frame_t *tx_frame)
{
    if (tx_frame == NULL) {
        return;
    }

    if (length > 6) {
        length = 6;
    }

    tx_frame->id = 0x7DF; 
    tx_frame->is_extended = false;
    tx_frame->is_rtr = false;
    tx_frame->dlc = 8;
    
    // Single Frame layout: length byte counts service + optional parameters
    tx_frame->data[0] = (uint8_t)(1 + length);
    tx_frame->data[1] = service;
    
    for (uint8_t i = 0; i < length; i++) {
        tx_frame->data[2 + i] = (payload != NULL) ? payload[i] : 0x00;
    }
    
    for (uint8_t i = (uint8_t)(2 + length); i < 8; i++) {
        tx_frame->data[i] = 0x00;
    }
}

void OBD2_BuildFreezeFrameRequest(uint8_t pid, uint8_t frame_num, CAN_Frame_t *tx_frame)
{
    uint8_t params[2] = { pid, frame_num };
    OBD2_BuildGenericRequest(OBD2_SERVICE_02_FREEZE_FRAME, params, 2, tx_frame);
}

void OBD2_BuildDTCRequest(uint8_t service, CAN_Frame_t *tx_frame)
{
    OBD2_BuildGenericRequest(service, NULL, 0, tx_frame);
}

bool OBD2_IsResponseValid(const CAN_Frame_t *rx_frame, uint8_t requested_service, uint8_t requested_pid)
{
    if (rx_frame == NULL) {
        return false;
    }

    // Positive response acknowledges requested service by adding 0x40 to service ID
    if (rx_frame->data[1] != (requested_service + 0x40)) {
        return false;
    }
    
    // Verify that response matches requested PID
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

    uint8_t pid = rx_frame->data[2];
    uint32_t a  = rx_frame->data[3];
    uint32_t b  = rx_frame->data[4];

    switch (pid)
    {
        // Engine Load & Throttle Position: (A * 100) / 255 [%]
        case OBD2_PID_ENGINE_LOAD:
        case OBD2_PID_THROTTLE_POS:
            return (float)(a * 100) / 255.0f;

        // Coolant Temperature: A - 40 [degC]
        case OBD2_PID_COOLANT_TEMP:
            return (float)((int32_t)a - 40);

        // Engine RPM: ((A * 256) + B) / 4 [RPM]
        case OBD2_PID_ENGINE_RPM:
            return (float)((a * 256) + b) / 4.0f;

        // Vehicle Speed: A [km/h]
        case OBD2_PID_VEHICLE_SPEED:
            return (float)a;

        // Mass Air Flow (MAF): ((A * 256) + B) / 100 [g/s]
        case OBD2_PID_MAF_AIR_FLOW:
            return (float)((a * 256) + b) / 100.0f;

        default:
            return -1.0f;
    }
}

uint32_t OBD2_ParseBitmask(const CAN_Frame_t *rx_frame)
{
    if (rx_frame == NULL) {
        return 0;
    }

    uint32_t result = (((uint32_t)rx_frame->data[3]) << 24) |
                      (((uint32_t)rx_frame->data[4]) << 16) |
                      (((uint32_t)rx_frame->data[5]) << 8)  |
                      ((uint32_t)rx_frame->data[6]);

    return result;
}

void OBD2_BuildClearDTCRequest(CAN_Frame_t *tx_frame)
{
    OBD2_BuildGenericRequest(OBD2_SERVICE_04_CLEAR_DTC, NULL, 0, tx_frame);
}

uint8_t OBD2_ParseDTCs(const uint8_t *payload_buffer, uint16_t payload_length, uint16_t *dtc_list, uint8_t max_dtcs)
{
    uint8_t count = 0;

    if (payload_buffer == NULL || dtc_list == NULL || max_dtcs == 0) {
        return 0;
    }

    for (size_t i = 0; (i + 1 < payload_length) && (count < max_dtcs); i += 2) {
        uint16_t dtc = ((uint16_t)payload_buffer[i] << 8) | payload_buffer[i + 1];

        // 0x0000 denotes empty slot or padding; only store valid DTCs
        if (dtc != 0x0000) {
            dtc_list[count] = dtc;
            count++;
        }
    }

    return count;
}

void OBD2_FormatDTC(uint16_t dtc, char *out_str)
{
    if (out_str == NULL) return;

    // ISO 15031-6 / SAE J2012 DTC standard:
    // Bits [15:14]: System prefix (00=P, 01=C, 10=B, 11=U)
    // Bits [13:12]: First digit (0..3)
    // Bits [11:8], [7:4], [3:0]: Second, third, fourth hex digits
    const char prefixes[] = { 'P', 'C', 'B', 'U' };
    const char hex_chars[] = "0123456789ABCDEF";

    out_str[0] = prefixes[(dtc >> 14) & 0x03];
    out_str[1] = (char)('0' + ((dtc >> 12) & 0x03));
    out_str[2] = hex_chars[(dtc >> 8) & 0x0F];
    out_str[3] = hex_chars[(dtc >> 4) & 0x0F];
    out_str[4] = hex_chars[dtc & 0x0F];
    out_str[5] = '\0';
}

bool OBD2_QuerySensor(uint8_t pid, float *out_val)
{
    if (out_val == NULL) return false;

    CAN_Frame_t tx_frame;
    CAN_Frame_t rx_frame;

    CAN_FlushRxQueue();
    OBD2_BuildRequest(OBD2_SERVICE_01_LIVE_DATA, pid, &tx_frame);
    if (CAN_Transmit(&tx_frame, 50) != CAN_OK) {
        return false;
    }

    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(50)) {
        if (CAN_Receive(&rx_frame, 50) == CAN_OK) {
            if (OBD2_IsResponseValid(&rx_frame, OBD2_SERVICE_01_LIVE_DATA, pid)) {
                *out_val = OBD2_ParseSensorValue(&rx_frame);
                return true;
            }
        } else {
            break;
        }
    }

    return false;
}

bool OBD2_QueryFreezeFrame(uint8_t pid, uint8_t frame_num, float *out_val)
{
    if (out_val == NULL) return false;

    CAN_Frame_t tx_frame;
    CAN_Frame_t rx_frame;

    CAN_FlushRxQueue();
    OBD2_BuildFreezeFrameRequest(pid, frame_num, &tx_frame);
    if (CAN_Transmit(&tx_frame, 50) != CAN_OK) {
        return false;
    }

    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(100)) {
        if (CAN_Receive(&rx_frame, 50) == CAN_OK) {
            if (rx_frame.data[1] == (OBD2_SERVICE_02_FREEZE_FRAME + 0x40) &&
                rx_frame.data[2] == pid &&
                rx_frame.data[3] == frame_num)
            {
                CAN_Frame_t mock_frame;
                memset(&mock_frame, 0, sizeof(CAN_Frame_t));
                mock_frame.data[2] = pid;
                mock_frame.data[3] = rx_frame.data[4];
                mock_frame.data[4] = rx_frame.data[5];
                *out_val = OBD2_ParseSensorValue(&mock_frame);
                return true;
            }
        } else {
            break;
        }
    }

    return false;
}

bool OBD2_QueryDTCsByService(uint8_t service, uint16_t *dtc_list, uint8_t *out_count, uint8_t max_dtcs)
{
    if (dtc_list == NULL || out_count == NULL) return false;

    CAN_Frame_t tx_frame;
    CAN_Frame_t rx_frame;
    OBD_MF_RxContext_t iso_tp_ctx;

    CAN_FlushRxQueue();
    OBD_MF_Reset(&iso_tp_ctx);
    OBD2_BuildDTCRequest(service, &tx_frame);
    if (CAN_Transmit(&tx_frame, 50) != CAN_OK) {
        return false;
    }

    TickType_t start_time = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(500)) {
        if (CAN_Receive(&rx_frame, 100) == CAN_OK) {
            uint8_t frame_type = rx_frame.data[0] >> 4;
            if (frame_type == 0) {
                if (rx_frame.data[1] == (service + 0x40)) {
                    uint8_t payload_len = rx_frame.data[0] & 0x0F;
                    *out_count = OBD2_ParseDTCs(&rx_frame.data[2], (payload_len >= 1) ? (payload_len - 1) : 0, dtc_list, max_dtcs);
                    return true;
                }
            } else {
                OBD_MF_ProcessFrame(&iso_tp_ctx, &rx_frame);
                if (iso_tp_ctx.state == OBD_MF_STATE_COMPLETE) {
                    if (iso_tp_ctx.buffer[0] == (service + 0x40)) {
                        *out_count = OBD2_ParseDTCs(&iso_tp_ctx.buffer[1], iso_tp_ctx.total_length - 1, dtc_list, max_dtcs);
                        return true;
                    }
                } else if (iso_tp_ctx.state == OBD_MF_STATE_ERROR) {
                    return false;
                }
            }
        } else {
            break;
        }
    }

    return false;
}

bool OBD2_QueryDTCs(uint16_t *dtc_list, uint8_t *out_count, uint8_t max_dtcs)
{
    return OBD2_QueryDTCsByService(OBD2_SERVICE_03_STORED_DTC, dtc_list, out_count, max_dtcs);
}

bool OBD2_QueryPendingDTCs(uint16_t *dtc_list, uint8_t *out_count, uint8_t max_dtcs)
{
    return OBD2_QueryDTCsByService(OBD2_SERVICE_07_PENDING_DTC, dtc_list, out_count, max_dtcs);
}

bool OBD2_QueryPermanentDTCs(uint16_t *dtc_list, uint8_t *out_count, uint8_t max_dtcs)
{
    return OBD2_QueryDTCsByService(OBD2_SERVICE_0A_PERMANENT_DTC, dtc_list, out_count, max_dtcs);
}

bool OBD2_ClearDTCs(void)
{
    CAN_Frame_t tx_frame;
    CAN_Frame_t rx_frame;

    CAN_FlushRxQueue();
    OBD2_BuildClearDTCRequest(&tx_frame);
    if (CAN_Transmit(&tx_frame, 50) != CAN_OK) {
        return false;
    }

    TickType_t start_time = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(300)) {
        if (CAN_Receive(&rx_frame, 50) == CAN_OK) {
            if (rx_frame.data[1] == (OBD2_SERVICE_04_CLEAR_DTC + 0x40)) {
                return true;
            }
        } else {
            break;
        }
    }

    return false;
}

bool OBD2_QueryVIN(char *out_vin, uint32_t timeout_ms)
{
    if (out_vin == NULL) return false;

    CAN_Frame_t tx_frame;
    CAN_Frame_t rx_frame;
    OBD_MF_RxContext_t iso_tp_ctx;

    CAN_FlushRxQueue();
    OBD_MF_Reset(&iso_tp_ctx);
    OBD2_BuildRequest(OBD2_SERVICE_09_VEHICLE_INFO, 0x02, &tx_frame);
    if (CAN_Transmit(&tx_frame, 50) != CAN_OK) {
        return false;
    }

    TickType_t start_time = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(timeout_ms)) {
        if (CAN_Receive(&rx_frame, 100) == CAN_OK) {
            OBD_MF_ProcessFrame(&iso_tp_ctx, &rx_frame);
            if (iso_tp_ctx.state == OBD_MF_STATE_COMPLETE) {
                memcpy(out_vin, &iso_tp_ctx.buffer[3], 17);
                out_vin[17] = '\0';
                return true;
            } else if (iso_tp_ctx.state == OBD_MF_STATE_ERROR) {
                return false;
            }
        } else {
            break;
        }
    }

    return false;
}

OBD2_ResponseStatus_t OBD2_ProbeService(uint8_t service, uint8_t param1, uint8_t param2, uint8_t param_len, uint8_t *nrc_code, uint32_t timeout_ms)
{
    CAN_Frame_t tx_frame;
    CAN_Frame_t rx_frame;
    uint8_t params[2];

    if (param_len == 1) {
        params[0] = param1;
    } else if (param_len >= 2) {
        params[0] = param1;
        params[1] = param2;
    }

    CAN_FlushRxQueue();
    OBD2_BuildGenericRequest(service, (param_len > 0) ? params : NULL, param_len, &tx_frame);

    if (CAN_Transmit(&tx_frame, 50) != CAN_OK) {
        return OBD2_RESP_TIMEOUT;
    }

    TickType_t start_time = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(timeout_ms)) {
        if (CAN_Receive(&rx_frame, 50) == CAN_OK) {
            uint8_t frame_type = rx_frame.data[0] >> 4;

            uint8_t resp_service_byte = 0;
            if (frame_type == 0) {
                resp_service_byte = rx_frame.data[1];
            } else if (frame_type == 1) {
                resp_service_byte = rx_frame.data[2];
            }

            if (resp_service_byte == (service + 0x40)) {
                return OBD2_RESP_OK;
            }

            if (rx_frame.data[1] == 0x7F && rx_frame.data[2] == service) {
                if (nrc_code != NULL) {
                    *nrc_code = rx_frame.data[3];
                }
                return OBD2_RESP_NRC;
            }
        } else {
            break;
        }
    }

    return OBD2_RESP_TIMEOUT;
}