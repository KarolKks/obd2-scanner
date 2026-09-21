#include "task_obd2.h"

#define OBD2_TASK_STACK_SIZE    768U
#define OBD2_TASK_PRIORITY      (tskIDLE_PRIORITY + 3)
#define OBD2_SWEEP_INTERVAL_MS  500U

static void Task_OBD2_PrintHexByte(uint8_t val)
{
    const char hex_chars[] = "0123456789ABCDEF";
    UART_SendChar(hex_chars[(val >> 4) & 0x0F]);
    UART_SendChar(hex_chars[val & 0x0F]);
}

static void Task_OBD2_PrintServiceRow(uint8_t service, const char *name, OBD2_ResponseStatus_t status, uint8_t nrc, const char *extra_info)
{
    UART_SendString(" [0x");
    Task_OBD2_PrintHexByte(service);
    UART_SendString("] ");
    UART_SendString(name);

    size_t len = strlen(name);
    for (size_t i = len; i < 24; i++) {
        UART_SendChar(' ');
    }

    if (status == OBD2_RESP_OK) {
        UART_SendString("[OK / SUPPORTED]");
        if (extra_info != NULL && extra_info[0] != '\0') {
            UART_SendString(" (");
            UART_SendString(extra_info);
            UART_SendString(")");
        }
    } else if (status == OBD2_RESP_NRC) {
        UART_SendString("[NRC 0x");
        Task_OBD2_PrintHexByte(nrc);
        UART_SendString(" / REJECTED]");
    } else {
        UART_SendString("[TIMEOUT / NO ANSWER]");
    }
    UART_SendString("\r\n");
}

void Task_OBD2_RunServicesScan(void)
{

    UART_SendString("         OBD-II SERVICES SCAN (MODE 0x01 - 0x0A)      \r\n");

    uint8_t nrc = 0;
    OBD2_ResponseStatus_t status;
    char extra[32];

    // Service 0x01: Current Live Data
    status = OBD2_ProbeService(OBD2_SERVICE_01_LIVE_DATA, OBD2_PID_SUPPORTED_PIDS_00, 0, 1, &nrc, 150);
    Task_OBD2_PrintServiceRow(OBD2_SERVICE_01_LIVE_DATA, "Current Live Data", status, nrc, NULL);

    // Service 0x02: Freeze Frame Data
    status = OBD2_ProbeService(OBD2_SERVICE_02_FREEZE_FRAME, 0x02, 0x00, 2, &nrc, 150);
    Task_OBD2_PrintServiceRow(OBD2_SERVICE_02_FREEZE_FRAME, "Freeze Frame Data", status, nrc, NULL);

    // Service 0x03: Stored DTCs
    uint16_t dtcs[6];
    uint8_t dtc_count = 0;
    if (OBD2_QueryDTCs(dtcs, &dtc_count, 6)) {
        snprintf(extra, sizeof(extra), "Codes: %u", (unsigned int)dtc_count);
        Task_OBD2_PrintServiceRow(OBD2_SERVICE_03_STORED_DTC, "Stored DTCs", OBD2_RESP_OK, 0, extra);
    } else {
        status = OBD2_ProbeService(OBD2_SERVICE_03_STORED_DTC, 0, 0, 0, &nrc, 150);
        Task_OBD2_PrintServiceRow(OBD2_SERVICE_03_STORED_DTC, "Stored DTCs", status, nrc, NULL);
    }

    // Service 0x04: Clear DTC Diagnostic Information
    status = OBD2_ProbeService(OBD2_SERVICE_04_CLEAR_DTC, 0, 0, 0, &nrc, 200);
    Task_OBD2_PrintServiceRow(OBD2_SERVICE_04_CLEAR_DTC, "Clear Diagnostic Info", status, nrc, NULL);

    // Service 0x05: Oxygen Sensor Monitoring Results
    status = OBD2_ProbeService(OBD2_SERVICE_05_O2_MONITOR, 0x01, 0, 1, &nrc, 150);
    Task_OBD2_PrintServiceRow(OBD2_SERVICE_05_O2_MONITOR, "O2 Sensor Test Results", status, nrc, NULL);

    // Service 0x06: On-Board Monitoring Test Results
    status = OBD2_ProbeService(OBD2_SERVICE_06_ONBOARD_TEST, 0x00, 0, 1, &nrc, 150);
    Task_OBD2_PrintServiceRow(OBD2_SERVICE_06_ONBOARD_TEST, "On-Board Test Results", status, nrc, NULL);

    // Service 0x07: Pending DTCs
    if (OBD2_QueryPendingDTCs(dtcs, &dtc_count, 6)) {
        snprintf(extra, sizeof(extra), "Codes: %u", (unsigned int)dtc_count);
        Task_OBD2_PrintServiceRow(OBD2_SERVICE_07_PENDING_DTC, "Pending DTCs", OBD2_RESP_OK, 0, extra);
    } else {
        status = OBD2_ProbeService(OBD2_SERVICE_07_PENDING_DTC, 0, 0, 0, &nrc, 150);
        Task_OBD2_PrintServiceRow(OBD2_SERVICE_07_PENDING_DTC, "Pending DTCs", status, nrc, NULL);
    }

    // Service 0x08: Control Operation
    status = OBD2_ProbeService(OBD2_SERVICE_08_CTRL_OPERATION, 0x01, 0, 1, &nrc, 150);
    Task_OBD2_PrintServiceRow(OBD2_SERVICE_08_CTRL_OPERATION, "Control Operation", status, nrc, NULL);

    // Service 0x09: Vehicle Information (VIN)
    char vin_tmp[18];
    if (OBD2_QueryVIN(vin_tmp, 600)) {
        Task_OBD2_PrintServiceRow(OBD2_SERVICE_09_VEHICLE_INFO, "Vehicle Info (VIN)", OBD2_RESP_OK, 0, vin_tmp);
    } else {
        status = OBD2_ProbeService(OBD2_SERVICE_09_VEHICLE_INFO, 0x00, 0, 1, &nrc, 200);
        Task_OBD2_PrintServiceRow(OBD2_SERVICE_09_VEHICLE_INFO, "Vehicle Info", status, nrc, NULL);
    }

    // Service 0x0A: Permanent DTCs
    if (OBD2_QueryPermanentDTCs(dtcs, &dtc_count, 6)) {
        snprintf(extra, sizeof(extra), "Codes: %u", (unsigned int)dtc_count);
        Task_OBD2_PrintServiceRow(OBD2_SERVICE_0A_PERMANENT_DTC, "Permanent DTCs", OBD2_RESP_OK, 0, extra);
    } else {
        status = OBD2_ProbeService(OBD2_SERVICE_0A_PERMANENT_DTC, 0, 0, 0, &nrc, 150);
        Task_OBD2_PrintServiceRow(OBD2_SERVICE_0A_PERMANENT_DTC, "Permanent DTCs", status, nrc, NULL);
    }

    UART_SendString("======================================================\r\n\r\n");
}

static void Task_OBD2_Body(void *argument)
{
    (void)argument;

    VehicleData_t vdata;
    memset(&vdata, 0, sizeof(VehicleData_t));

    uint32_t cycle_counter = 0;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    // Initial VIN query attempt on startup
    if (OBD2_QueryVIN(vdata.vin, 600)) {
        vdata.vin_valid = true;
    } else {
        strncpy(vdata.vin, "SEARCHING...", sizeof(vdata.vin) - 1);
        vdata.vin_valid = false;
    }

    // Run initial all-services diagnostic scan to discover supported modes
    Task_OBD2_RunServicesScan();

    while (1)
    {
        // Timestamp from RTC or monotonic clock
        if (RTC_GetDateTimeString(vdata.datetime, sizeof(vdata.datetime)) != RTC_OK) {
            snprintf(vdata.datetime, sizeof(vdata.datetime), "%lu", (unsigned long)CLK_GetTick());
        }

        // Retry VIN if not resolved yet (every 20 cycles)
        if (!vdata.vin_valid && (cycle_counter % 20) == 0) {
            if (OBD2_QueryVIN(vdata.vin, 500)) {
                vdata.vin_valid = true;
                Task_Logger_EnqueueString(vdata.datetime, "VIN", vdata.vin);
            }
        }

        // Service 01 Live Sensor Sweeps
        vdata.rpm_valid = OBD2_QuerySensor(OBD2_PID_ENGINE_RPM, &vdata.rpm);
        if (vdata.rpm_valid) {
            Task_Logger_EnqueueNumeric(vdata.datetime, "RPM", (uint32_t)vdata.rpm, "rpm");
        } else {
            Task_Logger_EnqueueString(vdata.datetime, "RPM", "NO_RESPONSE");
        }

        vdata.speed_valid = OBD2_QuerySensor(OBD2_PID_VEHICLE_SPEED, &vdata.speed);
        if (vdata.speed_valid) {
            Task_Logger_EnqueueNumeric(vdata.datetime, "SPEED", (uint32_t)vdata.speed, "km/h");
        } else {
            Task_Logger_EnqueueString(vdata.datetime, "SPEED", "NO_RESPONSE");
        }

        vdata.coolant_valid = OBD2_QuerySensor(OBD2_PID_COOLANT_TEMP, &vdata.coolant);
        if (vdata.coolant_valid) {
            Task_Logger_EnqueueNumeric(vdata.datetime, "COOLANT", (uint32_t)((int32_t)vdata.coolant), "degC");
        } else {
            Task_Logger_EnqueueString(vdata.datetime, "COOLANT", "NO_RESPONSE");
        }

        vdata.load_valid = OBD2_QuerySensor(OBD2_PID_ENGINE_LOAD, &vdata.load);
        if (vdata.load_valid) {
            Task_Logger_EnqueueNumeric(vdata.datetime, "LOAD", (uint32_t)vdata.load, "%");
        } else {
            Task_Logger_EnqueueString(vdata.datetime, "LOAD", "NO_RESPONSE");
        }

        vdata.throttle_valid = OBD2_QuerySensor(OBD2_PID_THROTTLE_POS, &vdata.throttle);
        if (vdata.throttle_valid) {
            Task_Logger_EnqueueNumeric(vdata.datetime, "THROTTLE", (uint32_t)vdata.throttle, "%");
        } else {
            Task_Logger_EnqueueString(vdata.datetime, "THROTTLE", "NO_RESPONSE");
        }

        vdata.maf_valid = OBD2_QuerySensor(OBD2_PID_MAF_AIR_FLOW, &vdata.maf);
        if (vdata.maf_valid) {
            Task_Logger_EnqueueNumeric(vdata.datetime, "MAF", (uint32_t)vdata.maf, "g/s");
        } else {
            Task_Logger_EnqueueString(vdata.datetime, "MAF", "NO_RESPONSE");
        }

        // Service 03 DTC Check (every 20 cycles)
        if ((cycle_counter % 20) == 0) {
            vdata.dtc_valid = OBD2_QueryDTCs(vdata.dtc_codes, &vdata.dtc_count, 6);
            if (vdata.dtc_valid) {
                Task_Logger_EnqueueNumeric(vdata.datetime, "DTC_COUNT", vdata.dtc_count, "codes");
                for (uint8_t i = 0; i < vdata.dtc_count; i++) {
                    char dtc_buf[8];
                    OBD2_FormatDTC(vdata.dtc_codes[i], dtc_buf);
                    Task_Logger_EnqueueString(vdata.datetime, "DTC", dtc_buf);
                }
            } else {
                Task_Logger_EnqueueString(vdata.datetime, "DTC", "NO_RESPONSE");
            }
        }

        // Send latest vehicle snapshot to Telemetry/UI queue
        QueueHandle_t telem_q = Task_UI_GetTelemetryQueue();
        if (telem_q != NULL) {
            xQueueOverwrite(telem_q, &vdata);
        }

        cycle_counter++;

        // Periodic cycle: 500 ms sweep interval
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(OBD2_SWEEP_INTERVAL_MS));
    }
}

BaseType_t Task_OBD2_Create(void)
{
    return xTaskCreate(Task_OBD2_Body, "OBD2_Core", OBD2_TASK_STACK_SIZE, NULL, OBD2_TASK_PRIORITY, NULL);
}
