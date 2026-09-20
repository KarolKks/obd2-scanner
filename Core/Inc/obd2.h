#ifndef CORE_OBD2_H
#define CORE_OBD2_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "can.h"
#include "obd_multiframe.h"

/* --- OBD-II Services (Modes) --- */
#define OBD2_SERVICE_01_LIVE_DATA       0x01
#define OBD2_SERVICE_03_READ_DTC        0x03
#define OBD2_SERVICE_04_CLEAR_DTC       0x04
#define OBD2_SERVICE_09_VEHICLE_INFO    0x09

/* --- OBD-II PIDs (Service 01 & 09) --- */
#define OBD2_PID_SUPPORTED_PIDS_00      0x00
#define OBD2_PID_MONITOR_STATUS         0x01
#define OBD2_PID_ENGINE_LOAD            0x04
#define OBD2_PID_COOLANT_TEMP           0x05
#define OBD2_PID_ENGINE_RPM             0x0C
#define OBD2_PID_VEHICLE_SPEED          0x0D
#define OBD2_PID_MAF_AIR_FLOW           0x10
#define OBD2_PID_THROTTLE_POS           0x11

/**
 * @brief Builds a standard OBD-II CAN frame to request a specific PID.
 * 
 * @param[in]  service   The OBD-II service mode (e.g., 0x01 or 0x09).
 * @param[in]  pid       The specific Parameter ID to request.
 * @param[out] tx_frame  Pointer to the CAN frame structure to be populated.
 */
void OBD2_BuildRequest(uint8_t service, uint8_t pid, CAN_Frame_t *tx_frame);

/**
 * @brief Checks if the received frame is a valid response to our request.
 * 
 * @param[in] rx_frame          Pointer to the received CAN frame.
 * @param[in] requested_service The service mode we requested (e.g., 0x01).
 * @param[in] requested_pid     The PID we requested.
 * 
 * @return true if the frame is a valid positive response, false otherwise.
 */
bool OBD2_IsResponseValid(const CAN_Frame_t *rx_frame, uint8_t requested_service, uint8_t requested_pid);

/**
 * @brief Parses physical sensor values (RPM, Speed, Temp, etc.) from a valid response.
 * @note  Only applicable for Service 01 physical parameters.
 * 
 * @param[in] rx_frame Pointer to the received CAN frame containing the data.
 * 
 * @return The calculated physical value as a floating-point number. 
 *         Returns -1.0f if the PID is unknown/unsupported.
 */
float OBD2_ParseSensorValue(const CAN_Frame_t *rx_frame);

/**
 * @brief Decodes a 4-byte bitmask from the frame (e.g., for PID 0x00).
 * 
 * @param[in] rx_frame Pointer to the received CAN frame.
 * 
 * @return A 32-bit integer representing the bitmask.
 */
uint32_t OBD2_ParseBitmask(const CAN_Frame_t *rx_frame);

/**
 * @brief Builds a request to clear all stored Diagnostic Trouble Codes (DTCs).
 * @note  Service 04 does not use PIDs, hence a dedicated function.
 * 
 * @param[out] tx_frame Pointer to the CAN frame structure to be populated.
 */
void OBD2_BuildClearDTCRequest(CAN_Frame_t *tx_frame);

/**
 * @brief Parses a list of Diagnostic Trouble Codes (DTCs) from a raw byte payload.
 * 
 * @param[in]  payload_buffer Pointer to the raw data buffer containing DTCs.
 * @param[in]  payload_length The total number of bytes in the payload.
 * @param[out] dtc_list       Pointer to an array where decoded 16-bit DTCs will be stored.
 * @param[in]  max_dtcs       The maximum number of DTCs the dtc_list can hold.
 * 
 * @return The number of DTCs successfully decoded and saved.
 */
uint8_t OBD2_ParseDTCs(const uint8_t *payload_buffer, uint16_t payload_length, uint16_t *dtc_list, uint8_t max_dtcs);

/**
 * @brief Formats a 16-bit DTC code into a standard null-terminated string (e.g., "P0420").
 * @param[in]  dtc     16-bit DTC value.
 * @param[out] out_str Output buffer of at least 6 bytes (5 chars + null).
 */
void OBD2_FormatDTC(uint16_t dtc, char *out_str);

/**
 * @brief Queries a single Service 01 sensor PID using interrupt-driven CAN reception.
 * @param[in]  pid     Parameter ID to request.
 * @param[out] out_val Pointer to store the decoded physical float value.
 * @return true if valid response received, false on timeout or error.
 */
bool OBD2_QuerySensor(uint8_t pid, float *out_val);

/**
 * @brief Queries Vehicle Identification Number (VIN) via Service 09 ISO-TP.
 * @param[out] out_vin     Buffer of at least 18 bytes to store the null-terminated VIN.
 * @param[in]  timeout_ms  Maximum duration in ms to wait for full multi-frame assembly.
 * @return true if VIN successfully decoded, false otherwise.
 */
bool OBD2_QueryVIN(char *out_vin, uint32_t timeout_ms);

/**
 * @brief Queries Diagnostic Trouble Codes (DTCs) via Service 03.
 * @param[out] dtc_list  Array to populate with decoded 16-bit DTC codes.
 * @param[out] out_count Pointer to store number of retrieved DTCs.
 * @param[in]  max_dtcs  Maximum capacity of dtc_list.
 * @return true if DTC response was received and parsed, false on error or timeout.
 */
bool OBD2_QueryDTCs(uint16_t *dtc_list, uint8_t *out_count, uint8_t max_dtcs);

/**
 * @brief Consolidated vehicle state model for diagnostics, telemetry, logging, and UI.
 */
typedef struct {
    char     datetime[24];
    char     vin[18];
    bool     vin_valid;
    float    rpm;
    bool     rpm_valid;
    float    speed;
    bool     speed_valid;
    float    coolant;
    bool     coolant_valid;
    float    load;
    bool     load_valid;
    float    throttle;
    bool     throttle_valid;
    float    maf;
    bool     maf_valid;
    uint16_t dtc_codes[6];
    uint8_t  dtc_count;
    bool     dtc_valid;
} VehicleData_t;

#endif /* CORE_OBD2_H */