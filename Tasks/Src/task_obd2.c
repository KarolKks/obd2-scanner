#include "task_obd2.h"

#define OBD2_TASK_STACK_SIZE    768U
#define OBD2_TASK_PRIORITY      (tskIDLE_PRIORITY + 3)
#define OBD2_SWEEP_INTERVAL_MS  500U

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
                    snprintf(dtc_buf, sizeof(dtc_buf), "P%04X", vdata.dtc_codes[i]);
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
