#include "main.h"

/**
 * @brief Structure for queueing diagnostic records to the SD card logger task.
 */
typedef struct {
    char     datetime[24];
    char     param_name[16];
    uint32_t val;
    char     unit[8];
    char     str_val[24];
    bool     is_string;
} LogRecord_t;

/* Queue handle for inter-task communication between OBD2 and Logger */
static QueueHandle_t g_sd_queue = NULL;

/* SPI handle instance for SD card on SPI1 */
SPI_Handle_t hspi1 = {
    .instance = SPI1,
    .baudrate_div = LL_SPI_BAUDRATEPRESCALER_DIV256,
    .is_initialized = false
};

/* FatFs file system and file objects */
static FATFS g_fs;
static FIL g_logfile;
static bool g_sd_ready = false;

/**
 * @brief Sends an unsigned integer over UART without sprintf.
 */
static void UART_SendNumber(uint32_t num)
{
    char buf[11];
    int i = 0;

    if (num == 0) {
        UART_SendString("0");
        return;
    }

    while (num > 0) {
        buf[i++] = (char)('0' + (num % 10));
        num /= 10;
    }

    char rev[11];
    int j = 0;
    while (i > 0) {
        rev[j++] = buf[--i];
    }
    rev[j] = '\0';

    UART_SendString(rev);
}

/**
 * @brief Sends a 16-bit DTC code in hex format (e.g. P0420).
 */
static void UART_SendDTC(uint16_t dtc)
{
    const char hex_chars[] = "0123456789ABCDEF";
    char dtc_str[6];

    dtc_str[0] = 'P';
    dtc_str[1] = hex_chars[(dtc >> 12) & 0x0F];
    dtc_str[2] = hex_chars[(dtc >> 8) & 0x0F];
    dtc_str[3] = hex_chars[(dtc >> 4) & 0x0F];
    dtc_str[4] = hex_chars[dtc & 0x0F];
    dtc_str[5] = '\0';

    UART_SendString(dtc_str);
}

/**
 * @brief Enqueues a numeric record for SD logging.
 */
static void EnqueueNumericRecord(const char *dt, const char *name, uint32_t val, const char *unit)
{
    if (g_sd_queue == NULL) return;

    LogRecord_t rec;
    strncpy(rec.datetime, dt, sizeof(rec.datetime) - 1);
    rec.datetime[sizeof(rec.datetime) - 1] = '\0';

    strncpy(rec.param_name, name, sizeof(rec.param_name) - 1);
    rec.param_name[sizeof(rec.param_name) - 1] = '\0';

    rec.val = val;

    strncpy(rec.unit, unit, sizeof(rec.unit) - 1);
    rec.unit[sizeof(rec.unit) - 1] = '\0';

    rec.str_val[0] = '\0';
    rec.is_string = false;

    xQueueSend(g_sd_queue, &rec, 0);
}

/**
 * @brief Enqueues a string record (e.g. VIN, status, or DTC) for SD logging.
 */
static void EnqueueStringRecord(const char *dt, const char *name, const char *str_val)
{
    if (g_sd_queue == NULL) return;

    LogRecord_t rec;
    strncpy(rec.datetime, dt, sizeof(rec.datetime) - 1);
    rec.datetime[sizeof(rec.datetime) - 1] = '\0';

    strncpy(rec.param_name, name, sizeof(rec.param_name) - 1);
    rec.param_name[sizeof(rec.param_name) - 1] = '\0';

    rec.val = 0;
    rec.unit[0] = '\0';

    strncpy(rec.str_val, str_val, sizeof(rec.str_val) - 1);
    rec.str_val[sizeof(rec.str_val) - 1] = '\0';

    rec.is_string = true;

    xQueueSend(g_sd_queue, &rec, 0);
}

/**
 * @brief Queries a single Service 01 sensor PID non-blockingly.
 * @param[in]  pid     Parameter ID to request.
 * @param[out] out_val Pointer to store the decoded physical float value.
 * @return true if valid response received, false on timeout or error.
 */
static bool OBD2_QuerySensor(uint8_t pid, float *out_val)
{
    CAN_Frame_t tx_frame;
    CAN_Frame_t rx_frame;

    OBD2_BuildRequest(OBD2_SERVICE_01_LIVE_DATA, pid, &tx_frame);
    if (CAN_Transmit(&tx_frame, 50) != CAN_OK) {
        return false;
    }

    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(40)) {
        if (CAN_IsRxPending() && CAN_Receive(&rx_frame) == CAN_OK) {
            if (OBD2_IsResponseValid(&rx_frame, OBD2_SERVICE_01_LIVE_DATA, pid)) {
                *out_val = OBD2_ParseSensorValue(&rx_frame);
                return true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    return false;
}

/**
 * @brief FreeRTOS Task: Periodic OBD-II vehicle diagnostics.
 *        Requests all simulator parameters (RPM, Speed, Temp, Load, Throttle, MAF, VIN, DTC).
 */
static void Task_OBD2(void *argument)
{
    (void)argument;

    CAN_Frame_t tx_frame;
    CAN_Frame_t rx_frame;
    OBD_MF_RxContext_t iso_tp_ctx;

    uint32_t cycle_counter = 0;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1)
    {
        char dt_str[24];
        if (RTC_GetDateTimeString(dt_str, sizeof(dt_str)) != RTC_OK) {
            snprintf(dt_str, sizeof(dt_str), "%lu", (unsigned long)CLK_GetTick());
        }

        UART_SendString("\r\n--- [1. LIVE SENSORS (SERVICE 01)] ---\r\n");

        /* --- 1. ENGINE RPM (PID 0x0C) --- */
        float rpm = 0.0f;
        if (OBD2_QuerySensor(OBD2_PID_ENGINE_RPM, &rpm)) {
            UART_SendString("  -> RPM: ");
            UART_SendNumber((uint32_t)rpm);
            UART_SendString(" rpm\r\n");
            EnqueueNumericRecord(dt_str, "RPM", (uint32_t)rpm, "rpm");
        } else {
            UART_SendString("  -> RPM: No response\r\n");
            EnqueueStringRecord(dt_str, "RPM", "NO_RESPONSE");
        }

        /* --- 2. VEHICLE SPEED (PID 0x0D) --- */
        float speed = 0.0f;
        if (OBD2_QuerySensor(OBD2_PID_VEHICLE_SPEED, &speed)) {
            UART_SendString("  -> Speed: ");
            UART_SendNumber((uint32_t)speed);
            UART_SendString(" km/h\r\n");
            EnqueueNumericRecord(dt_str, "SPEED", (uint32_t)speed, "km/h");
        } else {
            UART_SendString("  -> Speed: No response\r\n");
            EnqueueStringRecord(dt_str, "SPEED", "NO_RESPONSE");
        }

        /* --- 3. COOLANT TEMPERATURE (PID 0x05) --- */
        float coolant = 0.0f;
        if (OBD2_QuerySensor(OBD2_PID_COOLANT_TEMP, &coolant)) {
            UART_SendString("  -> Coolant Temp: ");
            if ((int32_t)coolant < 0) {
                UART_SendString("-");
                UART_SendNumber((uint32_t)(-(int32_t)coolant));
            } else {
                UART_SendNumber((uint32_t)coolant);
            }
            UART_SendString(" degC\r\n");
            EnqueueNumericRecord(dt_str, "COOLANT", (uint32_t)((int32_t)coolant), "degC");
        } else {
            UART_SendString("  -> Coolant: No response\r\n");
            EnqueueStringRecord(dt_str, "COOLANT", "NO_RESPONSE");
        }

        /* --- 4. ENGINE LOAD (PID 0x04) --- */
        float load = 0.0f;
        if (OBD2_QuerySensor(OBD2_PID_ENGINE_LOAD, &load)) {
            UART_SendString("  -> Engine Load: ");
            UART_SendNumber((uint32_t)load);
            UART_SendString(" %\r\n");
            EnqueueNumericRecord(dt_str, "LOAD", (uint32_t)load, "%");
        } else {
            UART_SendString("  -> Load: No response\r\n");
            EnqueueStringRecord(dt_str, "LOAD", "NO_RESPONSE");
        }

        /* --- 5. THROTTLE POSITION (PID 0x11) --- */
        float throttle = 0.0f;
        if (OBD2_QuerySensor(OBD2_PID_THROTTLE_POS, &throttle)) {
            UART_SendString("  -> Throttle Pos: ");
            UART_SendNumber((uint32_t)throttle);
            UART_SendString(" %\r\n");
            EnqueueNumericRecord(dt_str, "THROTTLE", (uint32_t)throttle, "%");
        } else {
            UART_SendString("  -> Throttle: No response\r\n");
            EnqueueStringRecord(dt_str, "THROTTLE", "NO_RESPONSE");
        }

        /* --- 6. MASS AIR FLOW / MAF (PID 0x10) --- */
        float maf = 0.0f;
        if (OBD2_QuerySensor(OBD2_PID_MAF_AIR_FLOW, &maf)) {
            UART_SendString("  -> MAF Air Flow: ");
            UART_SendNumber((uint32_t)maf);
            UART_SendString(" g/s\r\n");
            EnqueueNumericRecord(dt_str, "MAF", (uint32_t)maf, "g/s");
        } else {
            UART_SendString("  -> MAF: No response\r\n");
            EnqueueStringRecord(dt_str, "MAF", "NO_RESPONSE");
        }

        /* Periodically check VIN and DTCs every 5 cycles (~2.5 seconds) */
        if ((cycle_counter % 5) == 0)
        {
            UART_SendString("\r\n--- [2. VEHICLE INFO / VIN (SERVICE 09 - ISO-TP)] ---\r\n");
            OBD_MF_Reset(&iso_tp_ctx);
            OBD2_BuildRequest(OBD2_SERVICE_09_VEHICLE_INFO, 0x02, &tx_frame);
            CAN_Transmit(&tx_frame, 50);

            TickType_t start_time = xTaskGetTickCount();
            bool vin_received = false;

            while ((xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(500)) {
                if (CAN_IsRxPending() && CAN_Receive(&rx_frame) == CAN_OK) {
                    OBD_MF_ProcessFrame(&iso_tp_ctx, &rx_frame);

                    if (iso_tp_ctx.state == OBD_MF_STATE_COMPLETE) {
                        char vin[18] = {0};
                        memcpy(vin, &iso_tp_ctx.buffer[3], 17);
                        UART_SendString("  -> VIN: ");
                        UART_SendString(vin);
                        UART_SendString("\r\n");
                        EnqueueStringRecord(dt_str, "VIN", vin);
                        vin_received = true;
                        break;
                    } else if (iso_tp_ctx.state == OBD_MF_STATE_ERROR) {
                        UART_SendString("  -> ISO-TP Sequence Error!\r\n");
                        EnqueueStringRecord(dt_str, "VIN", "ISO_TP_ERROR");
                        break;
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(5));
            }

            if (!vin_received && iso_tp_ctx.state != OBD_MF_STATE_ERROR) {
                UART_SendString("  -> VIN: Timeout / No response\r\n");
                EnqueueStringRecord(dt_str, "VIN", "TIMEOUT");
            }

            UART_SendString("\r\n--- [3. DIAGNOSTIC TROUBLE CODES (SERVICE 03)] ---\r\n");
            OBD_MF_Reset(&iso_tp_ctx);
            OBD2_BuildRequest(OBD2_SERVICE_03_READ_DTC, 0x00, &tx_frame);
            CAN_Transmit(&tx_frame, 50);

            start_time = xTaskGetTickCount();
            bool dtc_processed = false;

            while ((xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(500)) {
                if (CAN_IsRxPending() && CAN_Receive(&rx_frame) == CAN_OK) {
                    uint8_t frame_type = rx_frame.data[0] >> 4;

                    if (frame_type == 0) {
                        uint8_t payload_len = rx_frame.data[0] & 0x0F;
                        uint16_t dtc_codes[6];
                        uint8_t count = OBD2_ParseDTCs(&rx_frame.data[2], payload_len - 1, dtc_codes, 6);

                        UART_SendString("  -> Single Frame: ");
                        UART_SendNumber(count);
                        UART_SendString(" DTC(s) found\r\n");
                        EnqueueNumericRecord(dt_str, "DTC_COUNT", count, "codes");

                        for (uint8_t i = 0; i < count; i++) {
                            UART_SendString("     [");
                            UART_SendNumber(i + 1);
                            UART_SendString("]: ");
                            UART_SendDTC(dtc_codes[i]);
                            UART_SendString("\r\n");

                            char dtc_buf[8];
                            snprintf(dtc_buf, sizeof(dtc_buf), "P%04X", dtc_codes[i]);
                            EnqueueStringRecord(dt_str, "DTC", dtc_buf);
                        }
                        dtc_processed = true;
                        break;
                    } else {
                        OBD_MF_ProcessFrame(&iso_tp_ctx, &rx_frame);
                        if (iso_tp_ctx.state == OBD_MF_STATE_COMPLETE) {
                            uint16_t dtc_codes[6];
                            uint8_t count = OBD2_ParseDTCs(&iso_tp_ctx.buffer[1], iso_tp_ctx.total_length - 1, dtc_codes, 6);

                            UART_SendString("  -> Multi-Frame: ");
                            UART_SendNumber(count);
                            UART_SendString(" DTC(s) found\r\n");
                            EnqueueNumericRecord(dt_str, "DTC_COUNT", count, "codes");

                            for (uint8_t i = 0; i < count; i++) {
                                UART_SendString("     [");
                                UART_SendNumber(i + 1);
                                UART_SendString("]: ");
                                UART_SendDTC(dtc_codes[i]);
                                UART_SendString("\r\n");

                                char dtc_buf[8];
                                snprintf(dtc_buf, sizeof(dtc_buf), "P%04X", dtc_codes[i]);
                                EnqueueStringRecord(dt_str, "DTC", dtc_buf);
                            }
                            dtc_processed = true;
                            break;
                        }
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(5));
            }

            if (!dtc_processed) {
                UART_SendString("  -> Service 03: No active DTCs or response\r\n");
                EnqueueStringRecord(dt_str, "DTC", "NO_RESPONSE");
            }
        }

        cycle_counter++;
        UART_SendString("\r\n========================================\r\n");

        /* Precise periodic cycle: 500ms between diagnostic sweeps */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1500));
    }
}

/**
 * @brief FreeRTOS Task: Background SD card file logger.
 *        Consumes records from xQueueSD and writes to OBD2_LOG.CSV.
 *        Performs periodic f_sync() to prevent Flash wear while ensuring data integrity.
 */
static void Task_Logger(void *argument)
{
    (void)argument;

    LogRecord_t rec;
    char line_buf[128];
    TickType_t last_sync_time = xTaskGetTickCount();

    while (1)
    {
        // Block waiting for new log records (up to 1000 ms timeout)
        if (xQueueReceive(g_sd_queue, &rec, pdMS_TO_TICKS(1000)) == pdTRUE) {
            if (g_sd_ready) {
                UINT bw = 0;
                if (rec.is_string) {
                    snprintf(line_buf, sizeof(line_buf), "%s,%s,%s,\r\n",
                             rec.datetime, rec.param_name, rec.str_val);
                } else {
                    snprintf(line_buf, sizeof(line_buf), "%s,%s,%lu,%s\r\n",
                             rec.datetime, rec.param_name, (unsigned long)rec.val, rec.unit);
                }
                f_write(&g_logfile, line_buf, (UINT)strlen(line_buf), &bw);
            }
        }

        // Periodic flush to physical Flash: every 2000 ms or when queue has been drained
        if (g_sd_ready && ((xTaskGetTickCount() - last_sync_time) >= pdMS_TO_TICKS(2000))) {
            f_sync(&g_logfile);
            last_sync_time = xTaskGetTickCount();
        }
    }
}

/**
 * @brief FreeRTOS Stack Overflow Hook (called when a task exceeds its stack watermark).
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    UART_SendString("\r\n[CRITICAL ERROR] FreeRTOS Stack Overflow in Task: ");
    UART_SendString(pcTaskName);
    UART_SendString("\r\n");
    while (1);
}

/**
 * @brief FreeRTOS Malloc Failed Hook (called when heap memory is exhausted).
 */
void vApplicationMallocFailedHook(void)
{
    UART_SendString("\r\n[CRITICAL ERROR] FreeRTOS Heap Exhausted (pvPortMalloc returned NULL)!\r\n");
    while (1);
}

int main(void)
{
    // 1. Initialize core system clocks and 1ms SysTick timebase
    CLK_Init();

    // 2. Initialize hardware RTC peripheral (with LSE/LSI fallback)
    RTC_Init();

    // 3. Initialize UART (115200 baud, 8N1) for ST-Link Virtual COM Port
    UART_Init();
    UART_SendString("\r\n========================================\r\n");
    UART_SendString("   OBD-II SCANNER (FreeRTOS Active)     \r\n");
    UART_SendString("========================================\r\n");

    char current_time[24];
    if (RTC_GetDateTimeString(current_time, sizeof(current_time)) == RTC_OK) {
        UART_SendString("RTC Clock: ");
        UART_SendString(current_time);
        UART_SendString("\r\n");
    }

    // 4. Initialize CAN hardware (500 kbps) and configure OBD-II response filter
    if (CAN_Init() != CAN_OK) {
        UART_SendString("[CAN ERROR] Initialization Failed!\r\n");
        while (1);
    }
    CAN_FilterOBD2();
    UART_SendString("CAN Bus Active (500 kbps, Filter 0x7E8-0x7EF).\r\n");

    // 5. Initialize SPI1 hardware interface for SD card
    if (SPI_Init(&hspi1) != SPI_STATUS_OK) {
        UART_SendString("[SPI ERROR] SPI1 Initialization Failed!\r\n");
    } else {
        UART_SendString("SPI1 Hardware Ready.\r\n");
    }

    // 6. Mount FatFs file system and open OBD2_LOG.CSV
    UART_SendString("Mounting SD Card...\r\n");
    FRESULT fr = f_mount(&g_fs, "", 1);
    if (fr == FR_OK) {
        fr = f_open(&g_logfile, "OBD2_LOG.CSV", FA_WRITE | FA_OPEN_APPEND);
        if (fr == FR_OK) {
            g_sd_ready = true;
            UART_SendString("SD Card Mounted. Log file 'OBD2_LOG.CSV' ready.\r\n");
            if (f_size(&g_logfile) == 0) {
                UINT bw = 0;
                const char *header = "DateTime,Parameter,Value,Unit\r\n";
                f_write(&g_logfile, header, (UINT)strlen(header), &bw);
                f_sync(&g_logfile);
            }
        } else {
            UART_SendString("Failed to open 'OBD2_LOG.CSV'! Error: ");
            UART_SendNumber((uint32_t)fr);
            UART_SendString("\r\n");
        }
    } else {
        UART_SendString("SD Mount Failed! Error code: ");
        UART_SendNumber((uint32_t)fr);
        UART_SendString(" (check card insertion and wiring)\r\n");
    }

    // 7. Create inter-task communication queue (capacity: 32 records)
    g_sd_queue = xQueueCreate(32, sizeof(LogRecord_t));
    if (g_sd_queue == NULL) {
        UART_SendString("[RTOS ERROR] Failed to create SD Queue!\r\n");
        while (1);
    }

    // 8. Create FreeRTOS Tasks
    BaseType_t ret;
    ret = xTaskCreate(Task_OBD2, "Task_OBD2", 768, NULL, tskIDLE_PRIORITY + 2, NULL);
    if (ret != pdPASS) {
        UART_SendString("[RTOS ERROR] Failed to create Task_OBD2!\r\n");
        while (1);
    }

    ret = xTaskCreate(Task_Logger, "Task_Logger", 768, NULL, tskIDLE_PRIORITY + 1, NULL);
    if (ret != pdPASS) {
        UART_SendString("[RTOS ERROR] Failed to create Task_Logger!\r\n");
        while (1);
    }

    UART_SendString("Starting FreeRTOS Scheduler...\r\n");

    // 9. Start FreeRTOS real-time preemptive scheduler
    vTaskStartScheduler();

    // Code reaches here only if heap was insufficient to start scheduler
    UART_SendString("[FATAL] FreeRTOS Scheduler Terminated unexpectedly!\r\n");
    while (1);
}