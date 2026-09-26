#include "task_obd2.h"

// Core OBD-II diagnostic polling task configuration
#define OBD2_TASK_STACK_SIZE    768U
#define OBD2_TASK_PRIORITY      (tskIDLE_PRIORITY + 3)
#define OBD2_SWEEP_INTERVAL_MS  500U

// Formats a byte as two hexadecimal ASCII characters
static void Task_OBD2_PrintHexByte(uint8_t val)
{
    const char hex_chars[] = "0123456789ABCDEF";
    UART_SendChar(hex_chars[(val >> 4) & 0x0F]);
    UART_SendChar(hex_chars[val & 0x0F]);
}

// Formats and prints a single diagnostic service scan result row to UART
static void Task_OBD2_PrintServiceRow(uint8_t service, const char *name, OBD2_ResponseStatus_t status, uint8_t nrc, const char *extra_info)
{
    UART_SendString(" [0x");
    Task_OBD2_PrintHexByte(service);
    UART_SendString("] ");
    UART_SendString(name);

    // Align column output to 24 characters
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

// Executes startup diagnostics across Modes 01-0A to discover ECU capabilities
void Task_OBD2_RunServicesScan(void)
{
    UART_SendString("         OBD-II SERVICES SCAN (MODE 0x01 - 0x0A)      \r\n");

    // Dynamic PID discovery queries Mode 01 PID 0x00, 0x20, 0x40 bitmasks
    OBD2_DiscoverSupportedPIDs(150);

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

    // List all queryable/supported PIDs discovered from ECU bitmasks (Mode 01)
    UART_SendString("\r\n--- [SUPPORTED PIDs (SERVICE 01)] ---\r\n");
    uint8_t supported_pids = 0;
    for (uint8_t i = 0; i < OBD2_SUPPORTED_PID_COUNT; i++) {
        const OBD2_PIDDescriptor_t *desc = OBD2_GetDescriptorByIndex(i);
        if (desc != NULL && OBD2_IsPIDQueryable(desc->pid)) {
            char pbuf[80];
            snprintf(pbuf, sizeof(pbuf), "  -> [0x%02X] %-5s: %s (%s)\r\n",
                     desc->pid, desc->short_name, desc->full_name, desc->unit);
            UART_SendString(pbuf);
            supported_pids++;
        }
    }
    if (supported_pids == 0) {
        UART_SendString("  -> No PIDs responded (ECU silent or bus offline)\r\n");
    }

    UART_SendString("======================================================\r\n");
    UART_SendString("[SYSTEM] Initialization complete. UI on OLED, logs on SD.\r\n");
    UART_SendString("[SYSTEM] UART set to ERROR-ONLY logging mode.\r\n\r\n");
}

static void Task_OBD2_Body(void *argument)
{
    (void)argument;

    VehicleData_t vdata;
    memset(&vdata, 0, sizeof(VehicleData_t));

    uint32_t cycle_counter = 0;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    TickType_t last_sd_log_time = 0;

    // Initial VIN query attempt on startup
    if (OBD2_QueryVIN(vdata.vin, 600)) {
        vdata.vin_valid = true;
    } else {
        strncpy(vdata.vin, "SEARCHING...", sizeof(vdata.vin) - 1);
        vdata.vin_valid = false;
    }

    // Run initial all-services diagnostic scan to discover supported modes
    Task_OBD2_RunServicesScan();

    // Default logging mask to only the discovered supported PIDs (avoid empty columns)
    uint32_t discovered_mask = 0;
    for (uint8_t i = 0; i < OBD2_SUPPORTED_PID_COUNT; i++) {
        const OBD2_PIDDescriptor_t *desc = OBD2_GetDescriptorByIndex(i);
        if (desc != NULL && OBD2_IsPIDQueryable(desc->pid)) {
            discovered_mask |= (1UL << i);
        }
    }
    if (discovered_mask != 0) {
        Task_Logger_SetChannelsMask(discovered_mask);
    }

    while (1)
    {
        // Timestamp from RTC or monotonic clock fallback
        if (RTC_GetDateTimeString(vdata.datetime, sizeof(vdata.datetime)) != RTC_OK) {
            snprintf(vdata.datetime, sizeof(vdata.datetime), "%lu", (unsigned long)CLK_GetTick());
        }

        // Retry VIN resolution every 20 cycles (10s) until successfully read
        if (!vdata.vin_valid && (cycle_counter % 20) == 0) {
            if (OBD2_QueryVIN(vdata.vin, 500)) {
                vdata.vin_valid = true;
            }
        }

        // Service 01: sweep all discovered and queryable sensor PIDs
        vdata.live_params_count = 0;
        for (uint8_t i = 0; i < OBD2_SUPPORTED_PID_COUNT; i++) {
            const OBD2_PIDDescriptor_t *desc = OBD2_GetDescriptorByIndex(i);
            if (desc == NULL) continue;

            // Skip PIDs reported as unsupported by ECU bitmasks
            if (!OBD2_IsPIDQueryable(desc->pid)) continue;

            float val = 0.0f;
            bool ok = OBD2_QuerySensor(desc->pid, &val);

            if (vdata.live_params_count < OBD2_MAX_ACTIVE_PIDS) {
                vdata.live_params[vdata.live_params_count].pid = desc->pid;
                vdata.live_params[vdata.live_params_count].value = val;
                vdata.live_params[vdata.live_params_count].valid = ok;
                vdata.live_params_count++;
            }
        }

        // Periodic SD card logging timer
        uint16_t log_intv_sec = Task_Logger_GetIntervalSeconds();
        TickType_t now_tick = xTaskGetTickCount();

        if (log_intv_sec == 0) {
            // Logging disabled / paused: keep timer synchronized to current time
            last_sd_log_time = now_tick;
        } else {
            // Check if user-configured interval has elapsed
            if ((now_tick - last_sd_log_time) >= pdMS_TO_TICKS(log_intv_sec * 1000U)) {
                last_sd_log_time = now_tick;

                LogSnapshot_t snap;
                memset(&snap, 0, sizeof(LogSnapshot_t));

                // Capture timestamp for CSV log row
                RTC_DateTime_t rtc_dt;
                if (RTC_GetDateTime(&rtc_dt) == RTC_OK) {
                    snprintf(snap.datetime, sizeof(snap.datetime), "%04u-%02u-%02u %02u:%02u:%02u",
                             (unsigned int)rtc_dt.year, (unsigned int)rtc_dt.month, (unsigned int)rtc_dt.day,
                             (unsigned int)rtc_dt.hours, (unsigned int)rtc_dt.minutes, (unsigned int)rtc_dt.seconds);
                } else {
                    uint32_t sec = (uint32_t)(now_tick / configTICK_RATE_HZ);
                    snprintf(snap.datetime, sizeof(snap.datetime), "%02lu:%02lu:%02lu",
                             (unsigned long)(sec / 3600), (unsigned long)((sec % 3600) / 60), (unsigned long)(sec % 60));
                }

                uint32_t effective_channel_mask = 0;
                snap.param_count = OBD2_SUPPORTED_PID_COUNT;

                // Populate telemetry snapshot parameters according to channel settings and ECU capabilities
                for (uint8_t i = 0; i < OBD2_SUPPORTED_PID_COUNT; i++) {
                    const OBD2_PIDDescriptor_t *desc = OBD2_GetDescriptorByIndex(i);
                    if (desc != NULL) {
                        snap.params[i].pid = desc->pid;
                        strncpy(snap.params[i].short_name, desc->short_name, sizeof(snap.params[i].short_name) - 1);
                        snap.params[i].decimals = desc->decimals;

                        // Enabled only if selected by user AND actually supported by vehicle ECU
                        bool is_en = Task_Logger_IsChannelEnabled(i) && OBD2_IsPIDQueryable(desc->pid);
                        snap.params[i].enabled = is_en;
                        if (is_en) {
                            effective_channel_mask |= (1UL << i);
                        }

                        bool found = false;
                        for (uint8_t p = 0; p < vdata.live_params_count; p++) {
                            if (vdata.live_params[p].pid == desc->pid) {
                                snap.params[i].value = vdata.live_params[p].value;
                                snap.params[i].valid = vdata.live_params[p].valid;
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            snap.params[i].value = 0.0f;
                            snap.params[i].valid = false;
                        }
                    }
                }
                snap.channel_mask = effective_channel_mask;

                // Send snapshot to background SD card writer queue
                if (!Task_Logger_EnqueueSnapshot(&snap)) {
                    UART_SendString("[SD ERROR] Snapshot queue full or logger offline.\r\n");
                }
            }
        }

        // Handle on-demand Clear DTC (Mode 04) request triggered from UI
        if (OBD2_GetClearDTCStatus() == 1) {
            bool clear_ok = OBD2_ClearDTCs();
            OBD2_SetClearDTCStatus(clear_ok ? 2 : 3);
            if (clear_ok) {
                vdata.dtc_count = 0;
                vdata.pending_count = 0;
                vdata.permanent_count = 0;
            }
        }

        // Periodic diagnostic sweeps (every 10 cycles = 5 seconds)
        if ((cycle_counter % 10) == 0) {
            // Service 03: Stored DTCs
            vdata.dtc_valid = OBD2_QueryDTCs(vdata.dtc_codes, &vdata.dtc_count, 6);

            // Service 07: Pending DTCs
            vdata.pending_valid = OBD2_QueryPendingDTCs(vdata.pending_codes, &vdata.pending_count, 6);

            // Service 0A: Permanent DTCs
            vdata.permanent_valid = OBD2_QueryPermanentDTCs(vdata.permanent_codes, &vdata.permanent_count, 6);

            // Service 02: Freeze Frame snapshot parameters
            vdata.freeze_valid = OBD2_QueryFreezeFrame(OBD2_PID_ENGINE_RPM, 0, &vdata.freeze_rpm);
            if (vdata.freeze_valid) {
                OBD2_QueryFreezeFrame(OBD2_PID_VEHICLE_SPEED, 0, &vdata.freeze_speed);
                OBD2_QueryFreezeFrame(OBD2_PID_COOLANT_TEMP, 0, &vdata.freeze_coolant);
            }
        }

        // Service 06: Background refresh when Mode 06 view is open in UI (every 4 cycles = ~2s)
        if (OBD2_IsMode06Active()) {
            if ((cycle_counter % 4) == 0) {
                OBD2_QueryMode06(&vdata.mode06_data, 400);
            }
        }

        // Update latest vehicle snapshot in UI queue (overwrite single slot)
        QueueHandle_t telem_q = Task_UI_GetTelemetryQueue();
        if (telem_q != NULL) {
            xQueueOverwrite(telem_q, &vdata);
        }

        // Update latest vehicle snapshot in UART task queue for fault detection
        QueueHandle_t uart_q = Task_UART_GetTelemetryQueue();
        if (uart_q != NULL) {
            xQueueOverwrite(uart_q, &vdata);
        }

        cycle_counter++;

        // Maintain precise periodic execution rate (500 ms sweep interval)
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(OBD2_SWEEP_INTERVAL_MS));
    }
}

BaseType_t Task_OBD2_Create(void)
{
    return xTaskCreate(Task_OBD2_Body, "OBD2_Core", OBD2_TASK_STACK_SIZE, NULL, OBD2_TASK_PRIORITY, NULL);
}
