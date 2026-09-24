#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "ff.h"
#include "fatfs_sd.h"
#include "spi.h"
#include "uart.h"

typedef enum {
    LOGGER_OK        = 0,
    LOGGER_ERR_INIT  = 1,
    LOGGER_ERR_WRITE = 2
} Logger_Status_t;

typedef struct {
    char     datetime[24];
    char     param_name[16];
    uint32_t val;
    char     unit[8];
    char     str_val[24];
    bool     is_string;
} LogRecord_t;

#define LOGGER_MAX_SNAPSHOT_PARAMS 20U

typedef struct {
    uint8_t pid;
    char    short_name[10];
    float   value;
    uint8_t decimals;
    bool    valid;
    bool    enabled;
} LogParam_t;

typedef struct {
    char        datetime[32];
    uint8_t     param_count;
    uint32_t    channel_mask;
    LogParam_t  params[LOGGER_MAX_SNAPSHOT_PARAMS];
} LogSnapshot_t;

/**
 * @brief  Initializes SD card filesystem and opens the default CSV logfile.
 * @param  hspi Pointer to SPI handle configured for SD card.
 * @return LOGGER_OK if mounted and file opened, LOGGER_ERR_INIT otherwise.
 */
Logger_Status_t Logger_Init(SPI_Handle_t *hspi);

/**
 * @brief  Attempts to mount SD card and open OBD2_LOG.CSV (used for initial mount and hot-plug reconnect).
 * @return LOGGER_OK on success, LOGGER_ERR_INIT on failure.
 */
Logger_Status_t Logger_TryMount(void);

/**
 * @brief  Handles card removal or write failure by unmounting and resetting state.
 */
void Logger_HandleDisconnect(void);

/**
 * @brief  Writes a single LogRecord_t to the open logfile.
 * @param  rec Pointer to the log record to serialize and write.
 * @return LOGGER_OK on success, LOGGER_ERR_WRITE on failure.
 */
Logger_Status_t Logger_WriteRecord(const LogRecord_t *rec);

/**
 * @brief  Writes a full telemetry snapshot row to OBD2_LOG.CSV and flushes immediately.
 * @param  snap Pointer to snapshot data.
 * @return LOGGER_OK on success, LOGGER_ERR_WRITE on failure.
 */
Logger_Status_t Logger_WriteSnapshot(const LogSnapshot_t *snap);

/**
 * @brief  Flushes cached file buffers to physical SD flash memory.
 * @return None.
 */
void Logger_Sync(void);

/**
 * @brief  Checks whether the SD card logger is mounted and ready for writes.
 * @return true if ready, false otherwise.
 */
bool Logger_IsReady(void);

#endif /* LOGGER_H */
