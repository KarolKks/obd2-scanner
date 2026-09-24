#include "logger.h"

// FatFs file system object and active CSV logfile handle
static FATFS s_fs;
static FIL s_logfile;
static bool s_sd_ready = false;
static uint32_t s_last_header_mask = 0xFFFFFFFFU; // Tracks active CSV column header configuration

// Attempts to mount FatFs volume and open 'OBD2_LOG.CSV' for appending
Logger_Status_t Logger_TryMount(void)
{
    // Reset SD card physical layer state machine
    SD_CardReset();

    // Mount FatFs filesystem immediately (opt = 1)
    FRESULT fr = f_mount(&s_fs, "", 1);
    if (fr != FR_OK) {
        s_sd_ready = false;
        char mbuf[48];
        snprintf(mbuf, sizeof(mbuf), "[SD ERROR] f_mount failed: fr=%d\r\n", (int)fr);
        UART_SendString(mbuf);
        return LOGGER_ERR_INIT;
    }

    // Open or create log file in append mode
    fr = f_open(&s_logfile, "OBD2_LOG.CSV", FA_WRITE | FA_OPEN_APPEND);
    if (fr != FR_OK) {
        f_mount(NULL, "", 0);
        s_sd_ready = false;
        char obuf[48];
        snprintf(obuf, sizeof(obuf), "[SD ERROR] f_open failed: fr=%d\r\n", (int)fr);
        UART_SendString(obuf);
        return LOGGER_ERR_INIT;
    }

    s_sd_ready = true;
    UART_SendString("[SD] MicroSD Card mounted. Log file 'OBD2_LOG.CSV' opened.\r\n");
    return LOGGER_OK;
}

// Cleans up file system state on card ejection or write fault
void Logger_HandleDisconnect(void)
{
    if (s_sd_ready) {
        UART_SendString("[SD] Card removal or write failure detected!\r\n");
    }
    s_sd_ready = false;
    f_close(&s_logfile);
    f_mount(NULL, "", 0);
    SD_CardReset();
    s_last_header_mask = 0xFFFFFFFFU; // Invalidate cached header so next mount writes fresh columns
}

// Binds hardware SPI interface to SD driver and attempts initial mount
Logger_Status_t Logger_Init(SPI_Handle_t *hspi)
{
    if (hspi == NULL) {
        return LOGGER_ERR_INIT;
    }

    // Bind SPI bus instance to FatFs SD card driver
    SD_SPI_AttachBus(hspi);

    // Initialize SPI hardware interface if not already configured
    if (!hspi->is_initialized) {
        if (SPI_Init(hspi) != SPI_STATUS_OK) {
            UART_SendString("[SPI ERROR] SPI Initialization Failed!\r\n");
            return LOGGER_ERR_INIT;
        }
        UART_SendString("SPI Hardware Ready.\r\n");
    }

    UART_SendString("Mounting SD Card...\r\n");
    if (Logger_TryMount() == LOGGER_OK) {
        return LOGGER_OK;
    }

    UART_SendString("SD Mount Failed (card not inserted or invalid). Auto-mount active.\r\n");
    return LOGGER_ERR_INIT;
}

// Writes a structured multi-channel vehicle telemetry snapshot to CSV
Logger_Status_t Logger_WriteSnapshot(const LogSnapshot_t *snap)
{
    if (!s_sd_ready || snap == NULL) {
        return LOGGER_ERR_WRITE;
    }

    UINT bw = 0;
    FRESULT fr;

    // Output new CSV header if file is empty or user changed enabled channel selection
    if (f_size(&s_logfile) == 0 || s_last_header_mask != snap->channel_mask) {
        char hdr_buf[256];
        int hlen = snprintf(hdr_buf, sizeof(hdr_buf), "DateTime");
        for (uint8_t i = 0; i < snap->param_count && i < LOGGER_MAX_SNAPSHOT_PARAMS; i++) {
            if (snap->params[i].enabled) {
                hlen += snprintf(hdr_buf + hlen, sizeof(hdr_buf) - hlen, ",%s", snap->params[i].short_name);
            }
        }
        snprintf(hdr_buf + hlen, sizeof(hdr_buf) - hlen, "\r\n");
        fr = f_write(&s_logfile, hdr_buf, (UINT)strlen(hdr_buf), &bw);
        if (fr == FR_OK) {
            f_sync(&s_logfile);
            s_last_header_mask = snap->channel_mask;
            UART_SendString("[SD] CSV header updated.\r\n");
        } else {
            char ebuf[48];
            snprintf(ebuf, sizeof(ebuf), "[SD ERROR] Header write failed: fr=%d\r\n", (int)fr);
            UART_SendString(ebuf);
            Logger_HandleDisconnect();
            return LOGGER_ERR_WRITE;
        }
    }

    // Format telemetry row values for all enabled channels
    char line_buf[256];
    int len = snprintf(line_buf, sizeof(line_buf), "%s", snap->datetime);

    for (uint8_t i = 0; i < snap->param_count && i < LOGGER_MAX_SNAPSHOT_PARAMS; i++) {
        if (!snap->params[i].enabled) {
            continue;
        }
        if (snap->params[i].valid) {
            // Integer or fixed-point decimal formatting
            if (snap->params[i].decimals == 0) {
                len += snprintf(line_buf + len, sizeof(line_buf) - len, ",%d", (int)snap->params[i].value);
            } else if (snap->params[i].decimals == 1) {
                float fval = snap->params[i].value;
                int32_t ipart = (int32_t)fval;
                int32_t fpart = (int32_t)((fval - (float)ipart) * 10.0f);
                if (fpart < 0) fpart = -fpart;
                len += snprintf(line_buf + len, sizeof(line_buf) - len, ",%d.%01d", (int)ipart, (int)fpart);
            } else {
                float fval = snap->params[i].value;
                int32_t ipart = (int32_t)fval;
                int32_t fpart = (int32_t)((fval - (float)ipart) * 100.0f);
                if (fpart < 0) fpart = -fpart;
                len += snprintf(line_buf + len, sizeof(line_buf) - len, ",%d.%02d", (int)ipart, (int)fpart);
            }
        } else {
            // Output dash for sensors that did not respond
            len += snprintf(line_buf + len, sizeof(line_buf) - len, ",-");
        }
    }
    snprintf(line_buf + len, sizeof(line_buf) - len, "\r\n");

    // Write row and sync filesystem cache to physical flash
    fr = f_write(&s_logfile, line_buf, (UINT)strlen(line_buf), &bw);
    if (fr == FR_OK) {
        fr = f_sync(&s_logfile);
    }

    if (fr != FR_OK) {
        char ebuf[48];
        snprintf(ebuf, sizeof(ebuf), "[SD ERROR] Row write failed: fr=%d\r\n", (int)fr);
        UART_SendString(ebuf);
        Logger_HandleDisconnect();
        return LOGGER_ERR_WRITE;
    }

    return LOGGER_OK;
}

// Writes a single parameter record (key-value pair) to logfile
Logger_Status_t Logger_WriteRecord(const LogRecord_t *rec)
{
    if (!s_sd_ready || rec == NULL) {
        return LOGGER_ERR_WRITE;
    }

    char line_buf[128];
    UINT bw = 0;

    if (rec->is_string) {
        snprintf(line_buf, sizeof(line_buf), "%s,%s,%s,\r\n",
                 rec->datetime, rec->param_name, rec->str_val);
    } else {
        snprintf(line_buf, sizeof(line_buf), "%s,%s,%lu,%s\r\n",
                 rec->datetime, rec->param_name, (unsigned long)rec->val, rec->unit);
    }

    FRESULT fr = f_write(&s_logfile, line_buf, (UINT)strlen(line_buf), &bw);
    if (fr == FR_OK) {
        fr = f_sync(&s_logfile);
    }

    if (fr != FR_OK) {
        Logger_HandleDisconnect();
        return LOGGER_ERR_WRITE;
    }

    return LOGGER_OK;
}

// Flushes unwritten cached data to physical SD card flash
void Logger_Sync(void)
{
    if (s_sd_ready) {
        if (f_sync(&s_logfile) != FR_OK) {
            Logger_HandleDisconnect();
        }
    }
}

// Returns true if card is mounted and logfile is ready for writes
bool Logger_IsReady(void)
{
    return s_sd_ready;
}
