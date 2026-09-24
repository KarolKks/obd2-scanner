#include "task_uart.h"

// FreeRTOS asynchronous diagnostic error & alert logging task configuration
#define UART_TASK_STACK_SIZE    384U
#define UART_TASK_PRIORITY      (tskIDLE_PRIORITY + 1)

// Input queue for receiving vehicle telemetry snapshots
static QueueHandle_t s_uart_queue = NULL;

// Analyzes vehicle state and outputs UART alerts only on faults or state changes
static void Task_UART_CheckAndReportErrors(const VehicleData_t *data)
{
    // State tracking to detect transitions and prevent repetitive log spam
    static uint8_t s_prev_dtc_count = 0;
    static uint16_t s_prev_dtc_codes[6];
    static bool s_prev_connected = false;

    // 1. Monitor ECU CAN communication link
    bool has_valid_sensor = false;
    for (uint8_t i = 0; i < data->live_params_count; i++) {
        if (data->live_params[i].valid) {
            has_valid_sensor = true;
            break;
        }
    }

    // Report transitions between connected and disconnected CAN bus states
    if (!s_prev_connected && has_valid_sensor) {
        s_prev_connected = true;
        UART_SendString("[OBD2] ECU Communication link established.\r\n");
    } else if (s_prev_connected && !has_valid_sensor && data->live_params_count > 0) {
        s_prev_connected = false;
        UART_SendString("\r\n[OBD2 ERROR] ECU Communication Lost (No response on CAN)!\r\n");
    }

    // 2. Active Diagnostic Trouble Codes (DTC) error monitoring
    if (data->dtc_valid && data->dtc_count > 0) {
        // Check if active DTC code list changed since last report
        bool dtc_changed = (data->dtc_count != s_prev_dtc_count);
        if (!dtc_changed) {
            for (uint8_t i = 0; i < data->dtc_count; i++) {
                if (data->dtc_codes[i] != s_prev_dtc_codes[i]) {
                    dtc_changed = true;
                    break;
                }
            }
        }

        // Print alert with all active diagnostic fault codes
        if (dtc_changed) {
            s_prev_dtc_count = data->dtc_count;
            memcpy(s_prev_dtc_codes, data->dtc_codes, sizeof(s_prev_dtc_codes));

            UART_SendString("\r\n[DTC ALERT] Active Trouble Codes Detected: ");
            UART_SendNumber(data->dtc_count);
            UART_SendString(" code(s)\r\n");

            for (uint8_t i = 0; i < data->dtc_count; i++) {
                char dtc_str[8];
                OBD2_FormatDTC(data->dtc_codes[i], dtc_str);
                UART_SendString("  -> Fault [");
                UART_SendNumber(i + 1);
                UART_SendString("]: ");
                UART_SendString(dtc_str);
                UART_SendString("\r\n");
            }
        }
    } else if (data->dtc_valid && data->dtc_count == 0 && s_prev_dtc_count > 0) {
        // Report when all fault codes have been successfully cleared
        s_prev_dtc_count = 0;
        memset(s_prev_dtc_codes, 0, sizeof(s_prev_dtc_codes));
        UART_SendString("\r\n[DTC INFO] Diagnostic Trouble Codes cleared (0 active faults).\r\n");
    }
}

static void Task_UART_Body(void *argument)
{
    (void)argument;
    VehicleData_t data;
    memset(&data, 0, sizeof(VehicleData_t));

    while (1)
    {
        // Block indefinitely until new telemetry snapshot arrives from OBD-II task
        if (xQueueReceive(s_uart_queue, &data, portMAX_DELAY) == pdPASS) {
            Task_UART_CheckAndReportErrors(&data);
        }
    }
}

BaseType_t Task_UART_Create(void)
{
    // Create single-slot telemetry buffer queue for overwrite updates
    if (s_uart_queue == NULL) {
        s_uart_queue = xQueueCreate(1, sizeof(VehicleData_t));
        if (s_uart_queue == NULL) {
            return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
        }
    }

    return xTaskCreate(Task_UART_Body, "Task_UART", UART_TASK_STACK_SIZE, NULL, UART_TASK_PRIORITY, NULL);
}

QueueHandle_t Task_UART_GetTelemetryQueue(void)
{
    return s_uart_queue;
}
