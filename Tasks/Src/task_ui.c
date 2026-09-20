#include "task_ui.h"

#define UI_TASK_STACK_SIZE      768U
#define UI_TASK_PRIORITY        (tskIDLE_PRIORITY + 1)
#define TELEM_PRINT_INTERVAL_MS 2000U

static QueueHandle_t s_telemetry_queue = NULL;

static void Task_UI_PrintTelemetry(const VehicleData_t *data)
{
    UART_SendString("\r\n========================================\r\n");
    UART_SendString("   OBD-II TELEMETRY [");
    UART_SendString(data->datetime);
    UART_SendString("]\r\n");
    UART_SendString("========================================\r\n");

    UART_SendString("  -> VIN: ");
    if (data->vin_valid) {
        UART_SendString(data->vin);
    } else {
        UART_SendString("Searching / Unavailable");
    }
    UART_SendString("\r\n");

    UART_SendString("--- [LIVE SENSORS (SERVICE 01)] ---\r\n");

    UART_SendString("  -> RPM:          ");
    if (data->rpm_valid) {
        UART_SendNumber((uint32_t)data->rpm);
        UART_SendString(" rpm\r\n");
    } else {
        UART_SendString("No response\r\n");
    }

    UART_SendString("  -> Speed:        ");
    if (data->speed_valid) {
        UART_SendNumber((uint32_t)data->speed);
        UART_SendString(" km/h\r\n");
    } else {
        UART_SendString("No response\r\n");
    }

    UART_SendString("  -> Coolant Temp: ");
    if (data->coolant_valid) {
        if ((int32_t)data->coolant < 0) {
            UART_SendString("-");
            UART_SendNumber((uint32_t)(-(int32_t)data->coolant));
        } else {
            UART_SendNumber((uint32_t)data->coolant);
        }
        UART_SendString(" degC\r\n");
    } else {
        UART_SendString("No response\r\n");
    }

    UART_SendString("  -> Engine Load:  ");
    if (data->load_valid) {
        UART_SendNumber((uint32_t)data->load);
        UART_SendString(" %\r\n");
    } else {
        UART_SendString("No response\r\n");
    }

    UART_SendString("  -> Throttle Pos: ");
    if (data->throttle_valid) {
        UART_SendNumber((uint32_t)data->throttle);
        UART_SendString(" %\r\n");
    } else {
        UART_SendString("No response\r\n");
    }

    UART_SendString("  -> MAF Air Flow: ");
    if (data->maf_valid) {
        UART_SendNumber((uint32_t)data->maf);
        UART_SendString(" g/s\r\n");
    } else {
        UART_SendString("No response\r\n");
    }

    if (data->dtc_valid) {
        UART_SendString("--- [DIAGNOSTIC TROUBLE CODES (SERVICE 03)] ---\r\n");
        UART_SendString("  -> DTC Count: ");
        UART_SendNumber(data->dtc_count);
        UART_SendString("\r\n");
        for (uint8_t i = 0; i < data->dtc_count; i++) {
            char dtc_str[6];
            OBD2_FormatDTC(data->dtc_codes[i], dtc_str);
            UART_SendString("     [");
            UART_SendNumber(i + 1);
            UART_SendString("]: ");
            UART_SendString(dtc_str);
            UART_SendString("\r\n");
        }
    }
}

static void Task_UI_Body(void *argument)
{
    (void)argument;
    VehicleData_t data;
    TickType_t last_telemetry_print = 0;

    while (1)
    {
        // Update KY-040 state machine (rotation and button debouncing)
        KY040_Update();

        // Consume any pending navigation events and report over UART
        KY040_Event_t enc_event;
        while (KY040_GetEvent(&enc_event, 0)) {
            switch (enc_event) {
                case KY040_EVENT_CW:
                    UART_SendString(">>> [ENCODER] Turn Right (CW  +1)\r\n");
                    break;
                case KY040_EVENT_CCW:
                    UART_SendString(">>> [ENCODER] Turn Left  (CCW -1)\r\n");
                    break;
                case KY040_EVENT_CLICK:
                    UART_SendString(">>> [ENCODER] Button Click (ENTER)\r\n");
                    break;
                case KY040_EVENT_HOLD:
                    UART_SendString(">>> [ENCODER] Button Hold  (BACK / RETURN)\r\n");
                    break;
                default:
                    break;
            }
        }

        // Receive telemetry snapshots (with 10 ms timeout to ensure responsive encoder scanning)
        if (xQueueReceive(s_telemetry_queue, &data, pdMS_TO_TICKS(10)) == pdPASS)
        {
            TickType_t now = xTaskGetTickCount();
            if ((now - last_telemetry_print) >= pdMS_TO_TICKS(TELEM_PRINT_INTERVAL_MS))
            {
                last_telemetry_print = now;
                Task_UI_PrintTelemetry(&data);
            }
        }
    }
}

BaseType_t Task_UI_Create(void)
{
    if (s_telemetry_queue == NULL) {
        s_telemetry_queue = xQueueCreate(1, sizeof(VehicleData_t));
        if (s_telemetry_queue == NULL) {
            return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
        }
    }

    return xTaskCreate(Task_UI_Body, "Task_UI", UI_TASK_STACK_SIZE, NULL, UI_TASK_PRIORITY, NULL);
}

QueueHandle_t Task_UI_GetTelemetryQueue(void)
{
    return s_telemetry_queue;
}
