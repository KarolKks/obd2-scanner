#include "obd2.h"

// Builds a standard single-PID query for Service 01 / 02
void OBD2_BuildRequest(uint8_t service, uint8_t pid, CAN_Frame_t *tx_frame) 
{
    OBD2_BuildGenericRequest(service, &pid, 1, tx_frame);
}

// Builds an ISO-TP Single Frame request targeting standard OBD-II broadcast ID 0x7DF
void OBD2_BuildGenericRequest(uint8_t service, const uint8_t *payload, uint8_t length, CAN_Frame_t *tx_frame)
{
    if (tx_frame == NULL) {
        return;
    }

    // Maximum payload for Single Frame is 6 bytes (DLC 8 - Length byte - Service byte)
    if (length > 6) {
        length = 6;
    }

    tx_frame->id = 0x7DF;          // Standard 11-bit OBD-II functional broadcast ID
    tx_frame->is_extended = false;
    tx_frame->is_rtr = false;
    tx_frame->dlc = 8;             // CAN frames always padded to 8 bytes per ISO 15765-4
    
    // Single Frame header: [0] = data length (service + payload count), [1] = service ID
    tx_frame->data[0] = (uint8_t)(1 + length);
    tx_frame->data[1] = service;
    
    // Copy optional parameter bytes (e.g. PID or DTC mode parameters)
    for (uint8_t i = 0; i < length; i++) {
        tx_frame->data[2 + i] = (payload != NULL) ? payload[i] : 0x00;
    }
    
    // Pad remaining unused bytes with 0x00
    for (uint8_t i = (uint8_t)(2 + length); i < 8; i++) {
        tx_frame->data[i] = 0x00;
    }
}

// Builds a Service 02 Freeze Frame request for specific PID and frame number
void OBD2_BuildFreezeFrameRequest(uint8_t pid, uint8_t frame_num, CAN_Frame_t *tx_frame)
{
    uint8_t params[2] = { pid, frame_num };
    OBD2_BuildGenericRequest(OBD2_SERVICE_02_FREEZE_FRAME, params, 2, tx_frame);
}

// Builds a 0-parameter DTC request for Service 03 (Stored), 07 (Pending), or 0A (Permanent)
void OBD2_BuildDTCRequest(uint8_t service, CAN_Frame_t *tx_frame)
{
    OBD2_BuildGenericRequest(service, NULL, 0, tx_frame);
}

// Validates whether received frame is a positive response to requested service and PID
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

// Master descriptor table for 20 standard Service 01 parameters
static const OBD2_PIDDescriptor_t s_pid_descriptors[OBD2_SUPPORTED_PID_COUNT] = {
    { OBD2_PID_ENGINE_RPM,          "RPM",  "Engine RPM",               "rpm",  0 },
    { OBD2_PID_VEHICLE_SPEED,       "SPD",  "Vehicle Speed",            "km/h", 0 },
    { OBD2_PID_MAF_AIR_FLOW,        "MAF",  "MAF Air Flow",             "g/s",  1 },
    { OBD2_PID_COOLANT_TEMP,        "COOL", "Coolant Temp",             "C",    0 },
    { OBD2_PID_ENGINE_LOAD,         "LOAD", "Calculated Load",          "%",    0 },
    { OBD2_PID_THROTTLE_POS,        "TPS",  "Throttle Position",        "%",    0 },
    { OBD2_PID_INTAKE_MAP,          "MAP",  "Intake Manifold MAP",      "kPa",  0 },
    { OBD2_PID_INTAKE_AIR_TEMP,     "IAT",  "Intake Air Temp",          "C",    0 },
    { OBD2_PID_TIMING_ADVANCE,      "ADV",  "Timing Advance",           "deg",  1 },
    { OBD2_PID_SHORT_FUEL_TRIM_1,   "STFT", "Short Term Fuel Trim B1",  "%",    1 },
    { OBD2_PID_LONG_FUEL_TRIM_1,    "LTFT", "Long Term Fuel Trim B1",   "%",    1 },
    { OBD2_PID_FUEL_PRESSURE,       "FP",   "Fuel Pressure",            "kPa",  0 },
    { OBD2_PID_ENGINE_RUN_TIME,     "RUN",  "Engine Run Time",          "s",    0 },
    { OBD2_PID_DISTANCE_WITH_MIL,   "MIL",  "Distance with MIL On",     "km",   0 },
    { OBD2_PID_FUEL_RAIL_PRESSURE,  "FRP",  "Fuel Rail Pressure",       "kPa",  0 },
    { OBD2_PID_FUEL_LEVEL,          "FUEL", "Fuel Tank Level",          "%",    0 },
    { OBD2_PID_BAROMETRIC_PRESSURE, "BARO", "Barometric Pressure",      "kPa",  0 },
    { OBD2_PID_MODULE_VOLTAGE,      "BATT", "Battery Voltage",          "V",    1 },
    { OBD2_PID_AMBIENT_AIR_TEMP,    "AAT",  "Ambient Air Temp",         "C",    0 },
    { OBD2_PID_ENGINE_OIL_TEMP,     "OIL",  "Engine Oil Temp",          "C",    0 }
};

// 32-bit bitmasks representing supported PIDs queried from ECU
static uint32_t s_supported_pids_00 = 0; // PIDs 0x01 to 0x20
static uint32_t s_supported_pids_20 = 0; // PIDs 0x21 to 0x40
static uint32_t s_supported_pids_40 = 0; // PIDs 0x41 to 0x60
static bool     s_masks_discovered = false;

const OBD2_PIDDescriptor_t *OBD2_GetPIDDescriptor(uint8_t pid)
{
    for (uint8_t i = 0; i < OBD2_SUPPORTED_PID_COUNT; i++) {
        if (s_pid_descriptors[i].pid == pid) {
            return &s_pid_descriptors[i];
        }
    }
    return NULL;
}

const OBD2_PIDDescriptor_t *OBD2_GetDescriptorByIndex(uint8_t index)
{
    if (index < OBD2_SUPPORTED_PID_COUNT) {
        return &s_pid_descriptors[index];
    }
    return NULL;
}

uint8_t OBD2_GetTotalSupportedPIDCount(void)
{
    return OBD2_SUPPORTED_PID_COUNT;
}

// Queries ECU for supported PID bitmasks across blocks 0x00, 0x20, and 0x40
void OBD2_DiscoverSupportedPIDs(uint32_t timeout_ms)
{
    CAN_Frame_t tx_frame;
    CAN_Frame_t rx_frame;

    s_supported_pids_00 = 0;
    s_supported_pids_20 = 0;
    s_supported_pids_40 = 0;
    s_masks_discovered = false;

    // 1. Probe PID 0x00: bitmask for PIDs 0x01 to 0x20
    CAN_FlushRxQueue();
    OBD2_BuildRequest(OBD2_SERVICE_01_LIVE_DATA, OBD2_PID_SUPPORTED_PIDS_00, &tx_frame);
    if (CAN_Transmit(&tx_frame, 50) == CAN_OK) {
        TickType_t start = xTaskGetTickCount();
        while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {
            if (CAN_Receive(&rx_frame, timeout_ms) == CAN_OK) {
                if (OBD2_IsResponseValid(&rx_frame, OBD2_SERVICE_01_LIVE_DATA, OBD2_PID_SUPPORTED_PIDS_00)) {
                    s_supported_pids_00 = OBD2_ParseBitmask(&rx_frame);
                    s_masks_discovered = true;
                    break;
                }
            } else {
                break;
            }
        }
    }

    // 2. If bit 0 of 0x00 mask is set, ECU supports PID 0x20 block (PIDs 0x21 to 0x40)
    if (s_supported_pids_00 & 0x01) {
        CAN_FlushRxQueue();
        OBD2_BuildRequest(OBD2_SERVICE_01_LIVE_DATA, OBD2_PID_SUPPORTED_PIDS_20, &tx_frame);
        if (CAN_Transmit(&tx_frame, 50) == CAN_OK) {
            TickType_t start = xTaskGetTickCount();
            while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {
                if (CAN_Receive(&rx_frame, timeout_ms) == CAN_OK) {
                    if (OBD2_IsResponseValid(&rx_frame, OBD2_SERVICE_01_LIVE_DATA, OBD2_PID_SUPPORTED_PIDS_20)) {
                        s_supported_pids_20 = OBD2_ParseBitmask(&rx_frame);
                        break;
                    }
                } else {
                    break;
                }
            }
        }
    }

    // 3. If bit 0 of 0x20 mask is set, ECU supports PID 0x40 block (PIDs 0x41 to 0x60)
    if (s_supported_pids_20 & 0x01) {
        CAN_FlushRxQueue();
        OBD2_BuildRequest(OBD2_SERVICE_01_LIVE_DATA, OBD2_PID_SUPPORTED_PIDS_40, &tx_frame);
        if (CAN_Transmit(&tx_frame, 50) == CAN_OK) {
            TickType_t start = xTaskGetTickCount();
            while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {
                if (CAN_Receive(&rx_frame, timeout_ms) == CAN_OK) {
                    if (OBD2_IsResponseValid(&rx_frame, OBD2_SERVICE_01_LIVE_DATA, OBD2_PID_SUPPORTED_PIDS_40)) {
                        s_supported_pids_40 = OBD2_ParseBitmask(&rx_frame);
                        break;
                    }
                } else {
                    break;
                }
            }
        }
    }
}

// Checks whether a given PID is queryable based on ECU bitmask responses
bool OBD2_IsPIDQueryable(uint8_t pid)
{
    // Always query the 6 core baseline parameters so essential gauges are never blocked
    if (pid == OBD2_PID_ENGINE_RPM     ||
        pid == OBD2_PID_VEHICLE_SPEED  ||
        pid == OBD2_PID_COOLANT_TEMP   ||
        pid == OBD2_PID_ENGINE_LOAD    ||
        pid == OBD2_PID_THROTTLE_POS   ||
        pid == OBD2_PID_MAF_AIR_FLOW) {
        return true;
    }

    // For other extended parameters, verify against discovered 32-bit ECU bitmasks
    if (s_masks_discovered) {
        if (pid >= 0x01 && pid <= 0x20) {
            // Bit 31 = PID 0x01, bit 30 = PID 0x02, ..., bit 0 = PID 0x20
            return (s_supported_pids_00 & (1UL << (32 - pid))) != 0;
        } else if (pid >= 0x21 && pid <= 0x40) {
            return (s_supported_pids_20 & (1UL << (32 - (pid - 0x20)))) != 0;
        } else if (pid >= 0x41 && pid <= 0x60) {
            return (s_supported_pids_40 & (1UL << (32 - (pid - 0x40)))) != 0;
        }
    }

    return false;
}

// Decodes raw CAN payload bytes into physical engineering units per SAE J1979
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
        // Percentage (0..100%): Formula = (A * 100) / 255
        case OBD2_PID_ENGINE_LOAD:
        case OBD2_PID_THROTTLE_POS:
        case OBD2_PID_FUEL_LEVEL:
            return (float)(a * 100) / 255.0f;

        // Temperatures (-40..215 deg C): Formula = A - 40
        case OBD2_PID_COOLANT_TEMP:
        case OBD2_PID_INTAKE_AIR_TEMP:
        case OBD2_PID_AMBIENT_AIR_TEMP:
        case OBD2_PID_ENGINE_OIL_TEMP:
            return (float)((int32_t)a - 40);

        // Fuel trims (-100%..+99.2%): Formula = (A - 128) * 100 / 128
        case OBD2_PID_SHORT_FUEL_TRIM_1:
        case OBD2_PID_LONG_FUEL_TRIM_1:
            return (float)((int32_t)a - 128) * 100.0f / 128.0f;

        // Fuel Pressure (0..765 kPa): Formula = A * 3
        case OBD2_PID_FUEL_PRESSURE:
            return (float)(a * 3);

        // Direct unsigned byte values: MAP, Baro, Speed (km/h)
        case OBD2_PID_INTAKE_MAP:
        case OBD2_PID_BAROMETRIC_PRESSURE:
        case OBD2_PID_VEHICLE_SPEED:
            return (float)a;

        // Engine RPM (0..16383.75 rpm): Formula = (256 * A + B) / 4
        case OBD2_PID_ENGINE_RPM:
            return (float)((a * 256) + b) / 4.0f;

        // Timing Advance (-64..63.5 deg): Formula = (A / 2) - 64
        case OBD2_PID_TIMING_ADVANCE:
            return ((float)a / 2.0f) - 64.0f;

        // MAF air flow rate (0..655.35 g/s): Formula = (256 * A + B) / 100
        case OBD2_PID_MAF_AIR_FLOW:
            return (float)((a * 256) + b) / 100.0f;

        // Engine runtime (s) and distance with MIL on (km): Formula = 256 * A + B
        case OBD2_PID_ENGINE_RUN_TIME:
        case OBD2_PID_DISTANCE_WITH_MIL:
            return (float)((a * 256) + b);

        // Fuel rail pressure (0..655350 kPa): Formula = (256 * A + B) * 10
        case OBD2_PID_FUEL_RAIL_PRESSURE:
            return (float)(((a * 256) + b) * 10);

        // Control module battery voltage (0..65.535 V): Formula = (256 * A + B) / 1000
        case OBD2_PID_MODULE_VOLTAGE:
            return (float)((a * 256) + b) / 1000.0f;

        default:
            return -1.0f;
    }
}

// Reconstructs a 32-bit big-endian bitmask from 4 response data bytes
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

// Builds a Service 04 request frame to clear DTC diagnostic memory and reset MIL
void OBD2_BuildClearDTCRequest(CAN_Frame_t *tx_frame)
{
    OBD2_BuildGenericRequest(OBD2_SERVICE_04_CLEAR_DTC, NULL, 0, tx_frame);
}

// Parses 2-byte diagnostic fault codes from single or multi-frame payload buffers
uint8_t OBD2_ParseDTCs(const uint8_t *payload_buffer, uint16_t payload_length, uint16_t *dtc_list, uint8_t max_dtcs)
{
    uint8_t count = 0;

    if (payload_buffer == NULL || dtc_list == NULL || max_dtcs == 0) {
        return 0;
    }

    for (size_t i = 0; (i + 1 < payload_length) && (count < max_dtcs); i += 2) {
        uint16_t dtc = ((uint16_t)payload_buffer[i] << 8) | payload_buffer[i + 1];

        // 0x0000 denotes empty slot or padding; only store valid active DTCs
        if (dtc != 0x0000) {
            dtc_list[count] = dtc;
            count++;
        }
    }

    return count;
}

// Formats 16-bit raw DTC value into standard 5-character string (e.g. P0300, C0123)
void OBD2_FormatDTC(uint16_t dtc, char *out_str)
{
    if (out_str == NULL) return;

    // ISO 15031-6 / SAE J2012 DTC format:
    // Bits [15:14]: System prefix (00=P Powertrain, 01=C Chassis, 10=B Body, 11=U Network)
    // Bits [13:12]: First digit (0..3)
    // Bits [11:8], [7:4], [3:0]: 2nd, 3rd, 4th hexadecimal digits
    const char prefixes[] = { 'P', 'C', 'B', 'U' };
    const char hex_chars[] = "0123456789ABCDEF";

    out_str[0] = prefixes[(dtc >> 14) & 0x03];
    out_str[1] = (char)('0' + ((dtc >> 12) & 0x03));
    out_str[2] = hex_chars[(dtc >> 8) & 0x0F];
    out_str[3] = hex_chars[(dtc >> 4) & 0x0F];
    out_str[4] = hex_chars[dtc & 0x0F];
    out_str[5] = '\0';
}

// Performs a synchronous request-response query for a single Service 01 sensor
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

    // Wait for ECU response frame matching requested PID
    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(100)) {
        if (CAN_Receive(&rx_frame, 100) == CAN_OK) {
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

// Queries a Service 02 Freeze Frame parameter for specific PID and frame number
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
            // Check for Service 02 positive response (0x42) matching requested PID & frame number
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

// Queries DTCs for specified service mode supporting Single Frame and ISO-TP multi-frame
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
                // ISO-TP Single Frame: response fits in one CAN frame (up to 3 DTCs)
                if (rx_frame.data[1] == (service + 0x40)) {
                    uint8_t payload_len = rx_frame.data[0] & 0x0F;
                    *out_count = OBD2_ParseDTCs(&rx_frame.data[2], (payload_len >= 1) ? (payload_len - 1) : 0, dtc_list, max_dtcs);
                    return true;
                }
            } else {
                // ISO-TP Multi-Frame: First Frame / Consecutive Frames handled by reassembly engine
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

// Service 03: Stored (confirmed) diagnostic trouble codes
bool OBD2_QueryDTCs(uint16_t *dtc_list, uint8_t *out_count, uint8_t max_dtcs)
{
    return OBD2_QueryDTCsByService(OBD2_SERVICE_03_STORED_DTC, dtc_list, out_count, max_dtcs);
}

// Service 07: Pending diagnostic trouble codes
bool OBD2_QueryPendingDTCs(uint16_t *dtc_list, uint8_t *out_count, uint8_t max_dtcs)
{
    return OBD2_QueryDTCsByService(OBD2_SERVICE_07_PENDING_DTC, dtc_list, out_count, max_dtcs);
}

// Service 0A: Permanent diagnostic trouble codes
bool OBD2_QueryPermanentDTCs(uint16_t *dtc_list, uint8_t *out_count, uint8_t max_dtcs)
{
    return OBD2_QueryDTCsByService(OBD2_SERVICE_0A_PERMANENT_DTC, dtc_list, out_count, max_dtcs);
}

// Service 04: Transmits clear diagnostic information command to ECU
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
            // Service 04 positive response is 0x44 (0x04 + 0x40)
            if (rx_frame.data[1] == (OBD2_SERVICE_04_CLEAR_DTC + 0x40)) {
                return true;
            }
        } else {
            break;
        }
    }

    return false;
}

// Service 09 PID 02: Queries and reassembles 17-character VIN via multi-frame ISO-TP
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
                // VIN string starts at byte offset 3 in assembled buffer
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

// Probes an OBD-II service to determine if supported (OK), rejected with NRC (0x7F), or timed out
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

            // Check for positive response (Service + 0x40)
            if (resp_service_byte == (service + 0x40)) {
                return OBD2_RESP_OK;
            }

            // Check for UDS / OBD-II Negative Response Code: [0x7F, Service, NRC]
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

// Asynchronous trigger flags for Clear DTC requested from OLED UI
static volatile int8_t s_clear_dtc_status = 0; // 0=idle, 1=triggered/in-progress, 2=cleared OK, 3=timeout

void OBD2_TriggerClearDTC(void)
{
    s_clear_dtc_status = 1;
}

int8_t OBD2_GetClearDTCStatus(void)
{
    return s_clear_dtc_status;
}

void OBD2_SetClearDTCStatus(int8_t status)
{
    s_clear_dtc_status = status;
}

// Maps standard 16-bit DTCs to human-readable fault descriptions
const char *OBD2_GetDTCDescription(uint16_t dtc)
{
    switch (dtc)
    {
        case 0x0100: return "MAF Circuit Malf";
        case 0x0101: return "MAF Circuit Range";
        case 0x0102: return "MAF Low Input";
        case 0x0103: return "MAF High Input";
        case 0x0112: return "IAT Low Input";
        case 0x0113: return "IAT High Input";
        case 0x0117: return "ECT Low Input";
        case 0x0118: return "ECT High Input";
        case 0x0121: return "TPS Circuit Range";
        case 0x0122: return "TPS Low Input";
        case 0x0123: return "TPS High Input";
        case 0x0171: return "System Too Lean B1";
        case 0x0172: return "System Too Rich B1";
        case 0x0300: return "Random Misfire";
        case 0x0301: return "Cyl 1 Misfire";
        case 0x0302: return "Cyl 2 Misfire";
        case 0x0303: return "Cyl 3 Misfire";
        case 0x0304: return "Cyl 4 Misfire";
        case 0x0420: return "Catalyst Low Eff";
        case 0x0430: return "Catalyst Low Eff B2";
        case 0x0500: return "Vehicle Speed Sens";
        case 0x0600: return "Serial Comm Link";
        default:
            break;
    }

    // Generic category fallback based on top 2 bits (P/C/B/U)
    uint8_t prefix = (dtc >> 14) & 0x03;
    switch (prefix) {
        case 0: return "Powertrain Fault";
        case 1: return "Chassis Fault";
        case 2: return "Body Fault";
        case 3: return "Network Fault";
        default: return "Diagnostic Fault";
    }
}