#ifndef FATFS_SD_H
#define FATFS_SD_H

#include "ff.h"
#include "diskio.h"      /* FatFs lower layer API */
#include "spi.h"         
#include "gpio.h"
#include "clk.h"
#include "rtc.h"

/**
 * @brief Diagnostic log callback function type for SD driver messages.
 * @param str Null-terminated string to output.
 */
typedef void (*SD_LogCallback_t)(const char *str);

/**
 * @brief Registers an optional diagnostic logging callback (e.g. UART_SendString, or NULL to disable).
 * @param callback Function pointer to the logger function.
 */
void SD_RegisterLogCallback(SD_LogCallback_t callback);

/**
 * @brief  Binds the hardware SPI handle to the FatFs SD driver.
 * @param  hspi Pointer to initialized SPI handle.
 */
void SD_SPI_AttachBus(SPI_Handle_t *hspi);

/**
 * @brief  Initializes the SD card via SPI.
 * @param[in] pdrv Physical drive number (0).
 * @return DSTATUS Status of the drive (0 on success, STA_NOINIT or STA_NODISK on error).
 */
DSTATUS disk_initialize(BYTE pdrv);

/**
 * @brief  Gets the current status of the SD card.
 * @param[in] pdrv Physical drive number (0).
 * @return DSTATUS Status of the drive.
 */
DSTATUS disk_status(BYTE pdrv);

/**
 * @brief  Reads sector(s) from the SD card.
 * @param[in]  pdrv   Physical drive number (0).
 * @param[out] buff   Pointer to the data buffer to store read data.
 * @param[in]  sector Start sector number (LBA).
 * @param[in]  count  Number of sectors to read (1..128).
 * @return DRESULT RES_OK on success, error code otherwise.
 */
DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count);

/**
 * @brief  Writes sector(s) to the SD card.
 * @param[in] pdrv   Physical drive number (0).
 * @param[in] buff   Pointer to the data to be written.
 * @param[in] sector Start sector number (LBA).
 * @param[in] count  Number of sectors to write (1..128).
 * @return DRESULT RES_OK on success, error code otherwise.
 */
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count);

/**
 * @brief  I/O control function for the SD card.
 * @param[in] pdrv Physical drive number (0).
 * @param[in] cmd  Control command code.
 * @param[in,out] buff Pointer to the control data buffer.
 * @return DRESULT RES_OK on success, error code otherwise.
 */
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff);

/**
 * @brief  Resets SD driver internal status to STA_NOINIT to allow clean re-initialization.
 */
void SD_CardReset(void);

#endif /* FATFS_SD_H */
