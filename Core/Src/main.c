#include "main.h"
#include "rtc.h"

/**
 * @brief Wysyła liczbę całkowitą bez znaku przez UART bez użycia sprintf/printf.
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

    // Odwrócenie kolejności cyfr w buforze
    char rev[11];
    int j = 0;
    while (i > 0) {
        rev[j++] = buf[--i];
    }
    rev[j] = '\0';

    UART_SendString(rev);
}

/**
 * @brief Wysyła 16-bitowy kod DTC w formacie szesnastkowym (np. P0420).
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
 * @brief Appends raw text line to the SD card log file and flushes data to flash.
 * @param[in] line Null-terminated string to write.
 */
static void SD_Log(const char *line)
{
    if (!g_sd_ready) {
        return;
    }

    UINT bw = 0;
    FRESULT res = f_write(&g_logfile, line, (UINT)strlen(line), &bw);
    if (res == FR_OK) {
        f_sync(&g_logfile); // Commit immediately so data is preserved if power is lost
    }
}

/**
 * @brief Logs a numeric parameter record in CSV format: DateTime,Parameter,Value,Unit
 */
static void SD_LogRecord(const char *param_name, uint32_t val, const char *unit, const char *status)
{
    char buf[96];
    char time_str[24];
    if (RTC_GetDateTimeString(time_str, sizeof(time_str)) != RTC_OK) {
        snprintf(time_str, sizeof(time_str), "%lu", (unsigned long)BSP_GetTick());
    }

    if (status != NULL) {
        snprintf(buf, sizeof(buf), "%s,%s,%s,\r\n", time_str, param_name, status);
    } else {
        snprintf(buf, sizeof(buf), "%s,%s,%lu,%s\r\n", time_str, param_name, (unsigned long)val, unit);
    }
    SD_Log(buf);
}

/**
 * @brief Logs a string parameter record in CSV format: DateTime,Parameter,StringVal,
 */
static void SD_LogStringRecord(const char *param_name, const char *str_val)
{
    char buf[96];
    char time_str[24];
    if (RTC_GetDateTimeString(time_str, sizeof(time_str)) != RTC_OK) {
        snprintf(time_str, sizeof(time_str), "%lu", (unsigned long)BSP_GetTick());
    }
    snprintf(buf, sizeof(buf), "%s,%s,%s,\r\n", time_str, param_name, str_val);
    SD_Log(buf);
}

int main(void)
{
    // Initialize system clocks and 1ms SysTick timebase
    BSP_CLK_Init();

    // Initialize hardware RTC (LSE 32.768 kHz with LSI fallback)
    RTC_Init();
    
    UART_Init();
    UART_SendString("\r\n========================================\r\n");
    UART_SendString("    OBD-II DIAGNOSTIC SCANNER READY     \r\n");
    UART_SendString("========================================\r\n");

    char current_time[24];
    if (RTC_GetDateTimeString(current_time, sizeof(current_time)) == RTC_OK) {
        UART_SendString("RTC Clock: ");
        UART_SendString(current_time);
        UART_SendString("\r\n");
    }

    if (CAN_Init() != CAN_OK) {
        UART_SendString("Blad inicjalizacji CAN!\r\n");
        while(1);
    }
    
    // Ustawienie filtru CAN na ramki odpowiedzi od ECU (0x7E8)
    CAN_FilterOBD2();
    UART_SendString("Magistrala CAN aktywna (500 kbps).\r\n");

    // Initialize SPI1 hardware interface for SD card (PA5: SCK, PA6: MISO, PA7: MOSI, PA4: CS)
    if (SPI_Init(&hspi1) != SPI_STATUS_OK) {
        UART_SendString("SPI1 Init Error!\r\n");
    } else {
        UART_SendString("SPI1 Init OK.\r\n");
    }

    // Mount FatFs file system (mount immediately: opt=1)
    UART_SendString("Mounting SD card filesystem...\r\n");
    FRESULT fr = f_mount(&g_fs, "", 1);
    if (fr == FR_OK) {
        UART_SendString("SD Card mounted successfully!\r\n");
        // Open or create log file in append mode
        fr = f_open(&g_logfile, "OBD2_LOG.CSV", FA_WRITE | FA_OPEN_APPEND);
        if (fr == FR_OK) {
            g_sd_ready = true;
            UART_SendString("Log file 'OBD2_LOG.CSV' opened successfully.\r\n");
            // Write CSV header if file is empty/new
            if (f_size(&g_logfile) == 0) {
                SD_Log("DateTime,Parameter,Value,Unit\r\n");
            }
        } else {
            UART_SendString("Failed to open 'OBD2_LOG.CSV'! Error code: ");
            UART_SendNumber((uint32_t)fr);
            UART_SendString("\r\n");
        }
    } else {
        UART_SendString("SD Card Mount Failed! Error code: ");
        UART_SendNumber((uint32_t)fr);
        UART_SendString(" (check card insertion, formatting and wiring)\r\n");
    }

    CAN_Frame_t tx_frame;
    CAN_Frame_t rx_frame;
    OBD_MF_RxContext_t iso_tp_ctx;

    while (1)
    {
        UART_SendString("\r\n--- [1. LIVE DATA (SERVICE 01)] ---\r\n");

        /* --- ODCZYT OBROTÓW SILNIKA (RPM) --- */
        OBD2_BuildRequest(OBD2_SERVICE_01_LIVE_DATA, OBD2_PID_ENGINE_RPM, &tx_frame);
        CAN_Transmit(&tx_frame, 50);
        LL_mDelay(40);

        if (CAN_IsRxPending() && CAN_Receive(&rx_frame) == CAN_OK) {
            if (OBD2_IsResponseValid(&rx_frame, OBD2_SERVICE_01_LIVE_DATA, OBD2_PID_ENGINE_RPM)) {
                float rpm = OBD2_ParseSensorValue(&rx_frame);
                UART_SendString("  -> RPM: ");
                UART_SendNumber((uint32_t)rpm);
                UART_SendString(" obr/min\r\n");
                SD_LogRecord("RPM", (uint32_t)rpm, "rpm", NULL);
            }
        } else {
            UART_SendString("  -> RPM: Brak odpowiedzi\r\n");
            SD_LogRecord("RPM", 0, "", "NO_RESPONSE");
        }

        /* --- ODCZYT PRĘDKOŚCI POJAZDU (SPEED) --- */
        OBD2_BuildRequest(OBD2_SERVICE_01_LIVE_DATA, OBD2_PID_VEHICLE_SPEED, &tx_frame);
        CAN_Transmit(&tx_frame, 50);
        LL_mDelay(40);

        if (CAN_IsRxPending() && CAN_Receive(&rx_frame) == CAN_OK) {
            if (OBD2_IsResponseValid(&rx_frame, OBD2_SERVICE_01_LIVE_DATA, OBD2_PID_VEHICLE_SPEED)) {
                float speed = OBD2_ParseSensorValue(&rx_frame);
                UART_SendString("  -> Speed: ");
                UART_SendNumber((uint32_t)speed);
                UART_SendString(" km/h\r\n");
                SD_LogRecord("SPEED", (uint32_t)speed, "km/h", NULL);
            }
        } else {
            UART_SendString("  -> Speed: Brak odpowiedzi\r\n");
            SD_LogRecord("SPEED", 0, "", "NO_RESPONSE");
        }


        UART_SendString("\r\n--- [2. VEHICLE INFO / VIN (SERVICE 09 - ISO-TP)] ---\r\n");

        /* --- ODCZYT NUMERU VIN (MULTI-FRAME) --- */
        OBD_MF_Reset(&iso_tp_ctx);
        OBD2_BuildRequest(OBD2_SERVICE_09_VEHICLE_INFO, 0x02, &tx_frame);
        CAN_Transmit(&tx_frame, 50);

        uint32_t start_time = 0;
        bool vin_received = false;

        while (start_time < 50) {
            if (CAN_IsRxPending() && CAN_Receive(&rx_frame) == CAN_OK) {
                OBD_MF_ProcessFrame(&iso_tp_ctx, &rx_frame);

                if (iso_tp_ctx.state == OBD_MF_STATE_COMPLETE) {
                    char vin[18] = {0};
                    memcpy(vin, &iso_tp_ctx.buffer[3], 17);
                    UART_SendString("  -> VIN odebrany: ");
                    UART_SendString(vin);
                    UART_SendString("\r\n");
                    SD_LogStringRecord("VIN", vin);
                    vin_received = true;
                    break;
                } else if (iso_tp_ctx.state == OBD_MF_STATE_ERROR) {
                    UART_SendString("  -> Blad sekwencji ramek ISO-TP!\r\n");
                    SD_LogStringRecord("VIN", "ISO_TP_ERROR");
                    break;
                }
            }
            LL_mDelay(10);
            start_time++;
        }

        if (!vin_received && iso_tp_ctx.state != OBD_MF_STATE_ERROR) {
            UART_SendString("  -> Timeout zapytania o VIN.\r\n");
            SD_LogRecord("VIN", 0, "", "TIMEOUT");
        }


        UART_SendString("\r\n--- [3. DIAGNOSTIC TROUBLE CODES (SERVICE 03)] ---\r\n");

        /* --- ODCZYT BŁĘDÓW DTC --- */
        OBD_MF_Reset(&iso_tp_ctx);
        OBD2_BuildRequest(OBD2_SERVICE_03_READ_DTC, 0x00, &tx_frame);
        CAN_Transmit(&tx_frame, 50);
        LL_mDelay(60);

        if (CAN_IsRxPending() && CAN_Receive(&rx_frame) == CAN_OK) {
            uint8_t frame_type = rx_frame.data[0] >> 4;

            if (frame_type == 0) {
                uint8_t payload_len = rx_frame.data[0] & 0x0F;
                uint16_t dtc_codes[6];
                uint8_t count = OBD2_ParseDTCs(&rx_frame.data[2], payload_len - 1, dtc_codes, 6);
                
                UART_SendString("  -> Single Frame: znaleziono ");
                UART_SendNumber(count);
                UART_SendString(" DTC\r\n");
                SD_LogRecord("DTC_COUNT", count, "codes", NULL);

                for (uint8_t i = 0; i < count; i++) {
                    UART_SendString("     Kod [");
                    UART_SendNumber(i + 1);
                    UART_SendString("]: ");
                    UART_SendDTC(dtc_codes[i]);
                    UART_SendString("\r\n");

                    char dtc_buf[8];
                    snprintf(dtc_buf, sizeof(dtc_buf), "P%04X", dtc_codes[i]);
                    SD_LogStringRecord("DTC", dtc_buf);
                }
            } else {
                OBD_MF_ProcessFrame(&iso_tp_ctx, &rx_frame);
                start_time = 0;

                while (start_time < 30 && iso_tp_ctx.state == OBD_MF_STATE_RECEIVING) {
                    if (CAN_IsRxPending() && CAN_Receive(&rx_frame) == CAN_OK) {
                        OBD_MF_ProcessFrame(&iso_tp_ctx, &rx_frame);
                    }
                    LL_mDelay(10);
                    start_time++;
                }

                if (iso_tp_ctx.state == OBD_MF_STATE_COMPLETE) {
                    uint16_t dtc_codes[6];
                    uint8_t count = OBD2_ParseDTCs(&iso_tp_ctx.buffer[1], iso_tp_ctx.total_length - 1, dtc_codes, 6);
                    
                    UART_SendString("  -> Multi-Frame: odebrano ");
                    UART_SendNumber(count);
                    UART_SendString(" DTC\r\n");
                    SD_LogRecord("DTC_COUNT", count, "codes", NULL);

                    for (uint8_t i = 0; i < count; i++) {
                        UART_SendString("     Kod [");
                        UART_SendNumber(i + 1);
                        UART_SendString("]: ");
                        UART_SendDTC(dtc_codes[i]);
                        UART_SendString("\r\n");

                        char dtc_buf[8];
                        snprintf(dtc_buf, sizeof(dtc_buf), "P%04X", dtc_codes[i]);
                        SD_LogStringRecord("DTC", dtc_buf);
                    }
                }
            }
        } else {
            UART_SendString("  -> Brak odpowiedzi dla Service 03.\r\n");
            SD_LogRecord("DTC", 0, "", "NO_RESPONSE");
        }

        UART_SendString("\r\n========================================\r\n");
        LL_mDelay(2000);
    }
}