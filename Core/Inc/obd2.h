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

/* --- OBD-II Services (Modes 0x01 to 0x0A) --- */
#define OBD2_SERVICE_01_LIVE_DATA       0x01
#define OBD2_SERVICE_02_FREEZE_FRAME    0x02
#define OBD2_SERVICE_03_STORED_DTC      0x03
#define OBD2_SERVICE_03_READ_DTC        0x03
#define OBD2_SERVICE_04_CLEAR_DTC       0x04
#define OBD2_SERVICE_05_O2_MONITOR      0x05
#define OBD2_SERVICE_06_ONBOARD_TEST    0x06
#define OBD2_SERVICE_07_PENDING_DTC     0x07
#define OBD2_SERVICE_08_CTRL_OPERATION  0x08
#define OBD2_SERVICE_09_VEHICLE_INFO    0x09
#define OBD2_SERVICE_0A_PERMANENT_DTC   0x0A

/* --- OBD-II PIDs (Service 01 & 09) --- */
#define OBD2_PID_SUPPORTED_PIDS_00      0x00
#define OBD2_PID_MONITOR_STATUS         0x01
#define OBD2_PID_ENGINE_LOAD            0x04
#define OBD2_PID_COOLANT_TEMP           0x05
#define OBD2_PID_SHORT_FUEL_TRIM_1      0x06
#define OBD2_PID_LONG_FUEL_TRIM_1       0x07
#define OBD2_PID_FUEL_PRESSURE          0x0A
#define OBD2_PID_INTAKE_MAP             0x0B
#define OBD2_PID_ENGINE_RPM             0x0C
#define OBD2_PID_VEHICLE_SPEED          0x0D
#define OBD2_PID_TIMING_ADVANCE         0x0E
#define OBD2_PID_INTAKE_AIR_TEMP        0x0F
#define OBD2_PID_MAF_AIR_FLOW           0x10
#define OBD2_PID_THROTTLE_POS           0x11
#define OBD2_PID_ENGINE_RUN_TIME        0x1F
#define OBD2_PID_SUPPORTED_PIDS_20      0x20
#define OBD2_PID_DISTANCE_WITH_MIL      0x21
#define OBD2_PID_FUEL_RAIL_PRESSURE     0x23
#define OBD2_PID_FUEL_LEVEL             0x2F
#define OBD2_PID_BAROMETRIC_PRESSURE    0x33
#define OBD2_PID_SUPPORTED_PIDS_40      0x40
#define OBD2_PID_MODULE_VOLTAGE         0x42
#define OBD2_PID_AMBIENT_AIR_TEMP       0x46
#define OBD2_PID_ENGINE_OIL_TEMP        0x5C

#define OBD2_SUPPORTED_PID_COUNT        20U
#define OBD2_MAX_ACTIVE_PIDS            24U

/**
 * @brief Descriptor for standard OBD-II Service 01 Parameters.
 */
typedef struct {
    uint8_t     pid;
    const char *short_name;   /* 4-5 chars for compact OLED rows e.g. "RPM", "SPD" */
    const char *full_name;    /* Descriptive English/Polish name */
    const char *unit;         /* Physical unit string */
    uint8_t     decimals;     /* Number of decimal places: 0 = integer, 1 = 0.1 */
} OBD2_PIDDescriptor_t;

/**
 * @brief Dynamic live parameter holding decoded value and validity flag.
 */
typedef struct {
    uint8_t pid;
    float   value;
    bool    valid;
} OBD2_LiveParam_t;

/**
 * @brief Diagnostic query response status classification.
 */
typedef enum {
    OBD2_RESP_OK = 0,
    OBD2_RESP_NRC,
    OBD2_RESP_TIMEOUT
} OBD2_ResponseStatus_t;

/**
 * @brief Builds a standard OBD-II CAN frame to request a specific PID (Single Frame).
 * 
 * @param[in]  service   The OBD-II service mode (e.g., 0x01 or 0x09).
 * @param[in]  pid       The specific Parameter ID to request.
 * @param[out] tx_frame  Pointer to the CAN frame structure to be populated.
 */
void OBD2_BuildRequest(uint8_t service, uint8_t pid, CAN_Frame_t *tx_frame);

/**
 * @brief Builds a generic ISO-TP Single Frame request for any service mode and payload.
 * 
 * @param[in]  service   The OBD-II service mode (0x01 to 0x0A).
 * @param[in]  payload   Pointer to optional additional parameter bytes (may be NULL if length is 0).
 * @param[in]  length    Number of parameter bytes in payload (0 to 6).
 * @param[out] tx_frame  Pointer to the CAN frame structure to be populated.
 */
void OBD2_BuildGenericRequest(uint8_t service, const uint8_t *payload, uint8_t length, CAN_Frame_t *tx_frame);

/**
 * @brief Builds a Service 02 Freeze Frame request for a specific PID and frame number.
 * 
 * @param[in]  pid        Parameter ID to request.
 * @param[in]  frame_num  Freeze frame number (typically 0x00).
 * @param[out] tx_frame   Pointer to the CAN frame structure to be populated.
 */
void OBD2_BuildFreezeFrameRequest(uint8_t pid, uint8_t frame_num, CAN_Frame_t *tx_frame);

/**
 * @brief Builds a DTC request frame for Service 03 (Stored), 07 (Pending), or 0A (Permanent).
 * 
 * @param[in]  service   The DTC service mode (0x03, 0x07, or 0x0A).
 * @param[out] tx_frame  Pointer to the CAN frame structure to be populated.
 */
void OBD2_BuildDTCRequest(uint8_t service, CAN_Frame_t *tx_frame);

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
 * @note  Applicable for Service 01 and Service 02 physical parameters.
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
 * @brief Queries Service 02 freeze frame sensor value for a given PID and frame number.
 * @param[in]  pid       Parameter ID to request.
 * @param[in]  frame_num Freeze frame index (typically 0x00).
 * @param[out] out_val   Pointer to store decoded physical value.
 * @return true if valid freeze frame response received, false on error or timeout.
 */
bool OBD2_QueryFreezeFrame(uint8_t pid, uint8_t frame_num, float *out_val);

/**
 * @brief Queries DTCs from a specified DTC service (0x03 Stored, 0x07 Pending, 0x0A Permanent).
 * @param[in]  service   Diagnostic service mode (0x03, 0x07, or 0x0A).
 * @param[out] dtc_list  Array to populate with decoded 16-bit DTC codes.
 * @param[out] out_count Pointer to store number of retrieved DTCs.
 * @param[in]  max_dtcs  Maximum capacity of dtc_list.
 * @return true if response was received and parsed, false on error or timeout.
 */
bool OBD2_QueryDTCsByService(uint8_t service, uint16_t *dtc_list, uint8_t *out_count, uint8_t max_dtcs);

/**
 * @brief Queries Stored Diagnostic Trouble Codes via Service 03.
 * @param[out] dtc_list  Array to populate with decoded 16-bit DTC codes.
 * @param[out] out_count Pointer to store number of retrieved DTCs.
 * @param[in]  max_dtcs  Maximum capacity of dtc_list.
 * @return true if DTC response was received and parsed, false on error or timeout.
 */
bool OBD2_QueryDTCs(uint16_t *dtc_list, uint8_t *out_count, uint8_t max_dtcs);

/**
 * @brief Queries Pending Diagnostic Trouble Codes via Service 07.
 * @param[out] dtc_list  Array to populate with decoded 16-bit DTC codes.
 * @param[out] out_count Pointer to store number of retrieved DTCs.
 * @param[in]  max_dtcs  Maximum capacity of dtc_list.
 * @return true if DTC response was received and parsed, false on error or timeout.
 */
bool OBD2_QueryPendingDTCs(uint16_t *dtc_list, uint8_t *out_count, uint8_t max_dtcs);

/**
 * @brief Queries Permanent Diagnostic Trouble Codes via Service 0A.
 * @param[out] dtc_list  Array to populate with decoded 16-bit DTC codes.
 * @param[out] out_count Pointer to store number of retrieved DTCs.
 * @param[in]  max_dtcs  Maximum capacity of dtc_list.
 * @return true if DTC response was received and parsed, false on error or timeout.
 */
bool OBD2_QueryPermanentDTCs(uint16_t *dtc_list, uint8_t *out_count, uint8_t max_dtcs);

/**
 * @brief Requests clearing of all stored DTCs and emission diagnostics via Service 04.
 * @return true if positive response (0x44) was received from ECU, false otherwise.
 */
bool OBD2_ClearDTCs(void);

/**
 * @brief Queries Vehicle Identification Number (VIN) via Service 09 ISO-TP.
 * @param[out] out_vin     Buffer of at least 18 bytes to store the null-terminated VIN.
 * @param[in]  timeout_ms  Maximum duration in ms to wait for full multi-frame assembly.
 * @return true if VIN successfully decoded, false otherwise.
 */
bool OBD2_QueryVIN(char *out_vin, uint32_t timeout_ms);

/**
 * @brief Probes any OBD-II service mode and returns status (OK, NRC, or Timeout).
 * 
 * @param[in]  service    The service mode to probe (0x01 to 0x0A).
 * @param[in]  param1     First parameter (PID / OBDMID / TID), or 0 if unused.
 * @param[in]  param2     Second parameter (e.g. frame number), or 0 if unused.
 * @param[in]  param_len  Number of parameter bytes (0, 1, or 2).
 * @param[out] nrc_code   Pointer to store Negative Response Code if status is OBD2_RESP_NRC.
 * @param[in]  timeout_ms Maximum duration in ms to wait for response.
 * 
 * @return OBD2_ResponseStatus_t indicating positive response, negative response, or timeout.
 */
OBD2_ResponseStatus_t OBD2_ProbeService(uint8_t service, uint8_t param1, uint8_t param2, uint8_t param_len, uint8_t *nrc_code, uint32_t timeout_ms);

typedef struct {
    char     datetime[24];
    
    // Service 09: Vehicle Identification
    char     vin[18];
    bool     vin_valid;

    // Service 03: Stored DTCs
    uint16_t dtc_codes[6];
    uint8_t  dtc_count;
    bool     dtc_valid;

    // Service 07: Pending DTCs
    uint16_t pending_codes[6];
    uint8_t  pending_count;
    bool     pending_valid;

    // Service 0A: Permanent DTCs
    uint16_t permanent_codes[6];
    uint8_t  permanent_count;
    bool     permanent_valid;

    // Service 02: Freeze Frame Status & Data
    bool     freeze_valid;
    float    freeze_rpm;
    float    freeze_speed;
    float    freeze_coolant;

    // Service 01: Dynamic Live Telemetry Array
    OBD2_LiveParam_t live_params[OBD2_MAX_ACTIVE_PIDS];
    uint8_t          live_params_count;
} VehicleData_t;

/**
 * @brief Retrieves the PID descriptor for a given PID.
 * @param pid OBD-II Parameter ID (e.g. 0x0C).
 * @return Pointer to OBD2_PIDDescriptor_t, or NULL if not in standard list.
 */
const OBD2_PIDDescriptor_t *OBD2_GetPIDDescriptor(uint8_t pid);

/**
 * @brief Retrieves the PID descriptor by its 0-based index in the standard table.
 * @param index 0 to OBD2_SUPPORTED_PID_COUNT-1.
 * @return Pointer to OBD2_PIDDescriptor_t, or NULL if out of range.
 */
const OBD2_PIDDescriptor_t *OBD2_GetDescriptorByIndex(uint8_t index);

/**
 * @brief Discovers supported PIDs by querying Mode 01 PID 0x00, 0x20, 0x40 bitmasks.
 * @param timeout_ms Timeout for each bitmask query.
 */
void OBD2_DiscoverSupportedPIDs(uint32_t timeout_ms);

/**
 * @brief Checks if a given PID is marked as supported by the ECU bitmasks (or core fallback).
 * @param pid Parameter ID to check.
 * @return true if queryable, false otherwise.
 */
bool OBD2_IsPIDQueryable(uint8_t pid);

/**
 * @brief  Requests asynchronous execution of Service 04 (Clear DTCs & reset MIL).
 */
void OBD2_TriggerClearDTC(void);

/**
 * @brief  Returns the result/status of the Clear DTCs request.
 * @return 0 = Idle, 1 = In Progress, 2 = Success (ACK received), 3 = Timeout / Failed.
 */
int8_t OBD2_GetClearDTCStatus(void);

/**
 * @brief  Sets the result/status of the Clear DTC operation (called by Task_OBD2).
 * @param  status 0 = Idle, 1 = In Progress, 2 = Success, 3 = Timeout / Failed.
 */
void OBD2_SetClearDTCStatus(int8_t status);

/**
 * @brief Returns short human-readable description for common standard SAE DTCs.
 * @param dtc 16-bit raw DTC value.
 * @return Null-terminated description string.
 */
const char *OBD2_GetDTCDescription(uint16_t dtc);

#endif /* CORE_OBD2_H */