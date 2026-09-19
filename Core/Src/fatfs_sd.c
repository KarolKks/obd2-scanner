#include "fatfs_sd.h"

// External objects
extern SPI_Handle_t hspi1;

/* SD Card Command Definitions */
#define CMD0    (0)         /* GO_IDLE_STATE: Software reset */
#define CMD8    (8)         /* SEND_IF_COND: Check voltage range */
#define CMD16   (16)        /* SET_BLOCKLEN: Set block length to 512 bytes */
#define CMD17   (17)        /* READ_SINGLE_BLOCK: Read a 512-byte block */
#define CMD24   (24)        /* WRITE_SINGLE_BLOCK: Write a 512-byte block */
#define CMD55   (55)        /* APP_CMD: Prefix for application specific commands */
#define CMD58   (58)        /* READ_OCR: Read OCR register */
#define ACMD41  (0x80 | 41) /* SD_SEND_OP_COND: Initialization sequence */

#define SD_DATA_TOKEN       0xFE
#define SD_READY_TOKEN      0xFF

static volatile DSTATUS Stat = STA_NOINIT;
static uint8_t CardType = 0; /* 0 = Standard Capacity (SDSC), 1 = High Capacity (SDHC/SDXC) */

/* CS Control Helpers */
static void SD_CS_Select(void)
{
    LL_GPIO_ResetOutputPin(SD_CS_PORT, SD_CS_PIN);
}

static void SD_CS_Deselect(void)
{
    LL_GPIO_SetOutputPin(SD_CS_PORT, SD_CS_PIN);
    
    // Send a dummy clock cycle to force the SD card to release the MISO line (High-Z state)
    uint8_t dummy;
    SPI_ReceiveByte(&hspi1, &dummy); 
}

static bool SD_WaitForReady(uint32_t timeout_ms)
{
    uint8_t res;
    uint32_t start = CLK_GetTick();
    
    do {
        if (SPI_ReceiveByte(&hspi1, &res) != SPI_STATUS_OK) {
            return false;
        }
        // The card sends 0xFF when it is ready and idle
        if (res == SD_READY_TOKEN) {
            return true;
        }
    } while ((CLK_GetTick() - start) < timeout_ms);
    
    return false;
}

static uint8_t SD_SendCmd(uint8_t cmd, uint32_t arg)
{
    uint8_t res;
    
    // Handle Application Specific Commands (ACMD) which require CMD55 preceding them
    if (cmd & 0x80) {
        cmd &= 0x7F;
        res = SD_SendCmd(CMD55, 0);
        SD_CS_Deselect();
        if (res > 1) {
            return res;
        }
    }

    SD_CS_Select();
    if (cmd != CMD0) {
        if (!SD_WaitForReady(500)) {
            SD_CS_Deselect();
            return 0xFF;
        }
    }

    // Transmit command structure: Index, 32-bit Argument, CRC
    SPI_TransferByte(&hspi1, (cmd | 0x40), NULL);
    SPI_TransferByte(&hspi1, (uint8_t)(arg >> 24), NULL);
    SPI_TransferByte(&hspi1, (uint8_t)(arg >> 16), NULL);
    SPI_TransferByte(&hspi1, (uint8_t)(arg >> 8), NULL);
    SPI_TransferByte(&hspi1, (uint8_t)(arg), NULL);

    // Provide valid CRC for CMD0 and CMD8; default dummy CRC for others
    uint8_t crc = 0x01;
    if (cmd == CMD0) crc = 0x95;
    if (cmd == CMD8) crc = 0x87;
    SPI_TransferByte(&hspi1, crc, NULL);

    // Wait for the R1 response (valid responses have the MSB cleared)
    uint8_t attempts = 200;
    do {
        SPI_ReceiveByte(&hspi1, &res);
    } while ((res & 0x80) && --attempts);

    return res;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0) {
        return STA_NOINIT;
    }

    // Ensure GPIOA clock is enabled for Chip Select pin
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);

    // Initialize CS pin as Output Push-Pull, High Speed, Pull-Up, set HIGH (Deselected)
    LL_GPIO_SetOutputPin(SD_CS_PORT, SD_CS_PIN);
    LL_GPIO_SetPinMode(SD_CS_PORT, SD_CS_PIN, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinOutputType(SD_CS_PORT, SD_CS_PIN, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(SD_CS_PORT, SD_CS_PIN, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinPull(SD_CS_PORT, SD_CS_PIN, LL_GPIO_PULL_UP);

    // Allow SD card internal power-on reset (POR) to stabilize (typical 20-50ms)
    CLK_Delay(50);

    // Start with a slow SPI clock (100 - 400 kHz) for safe initialization
    SPI_SetBaudrate(&hspi1, LL_SPI_BAUDRATEPRESCALER_DIV256);
    
    SD_CS_Deselect();
    
    // Supply minimum 74 dummy clock cycles with CS high to wake up the card into native state
    uint8_t dummy = 0xFF;
    for (uint8_t i = 0; i < 10; i++) {
        SPI_ReceiveByte(&hspi1, &dummy);
    }

    // Diagnostic: Check physical electrical level on PA6 (MISO)
    uint32_t pa6_is_high = LL_GPIO_IsInputPinSet(GPIOA, LL_GPIO_PIN_6);

    UART_SendString("  [DIAG] PA6 Pin electrical state: ");
    if (pa6_is_high) {
        UART_SendString("HIGH (3.3V - Pull-up OK)\r\n");
    } else {
        UART_SendString("LOW (0V - Pin is physically connected to GND!)\r\n");
    }

    // Verify MISO line idle level with CS high (should be 0xFF with pull-up)
    if (dummy != 0xFF) {
        UART_SendString("  [SD WARNING] MISO (PA6) read 0x");
        const char hex_chars[] = "0123456789ABCDEF";
        char hex_str[3];
        hex_str[0] = hex_chars[(dummy >> 4) & 0x0F];
        hex_str[1] = hex_chars[dummy & 0x0F];
        hex_str[2] = '\0';
        UART_SendString(hex_str);
        UART_SendString(" with CS HIGH! (Expected 0xFF - MISO is shorted to GND or connected to wrong pin)\r\n");
    }

    // Force the card into SPI mode and Idle state (CMD0) with retry
    uint8_t res = 0xFF;
    uint32_t start = CLK_GetTick();
    do {
        res = SD_SendCmd(CMD0, 0);
        SD_CS_Deselect();
        if (res == 1) {
            break;
        }
    } while ((CLK_GetTick() - start) < 1000);

    if (res != 1) {
        UART_SendString("  [SD] CMD0 error (received: 0x");
        const char hex_chars[] = "0123456789ABCDEF";
        char hex_str[3];
        hex_str[0] = hex_chars[(res >> 4) & 0x0F];
        hex_str[1] = hex_chars[res & 0x0F];
        hex_str[2] = '\0';
        UART_SendString(hex_str);
        UART_SendString(" - 0xFF means no response / card absent / wiring issue)\r\n");
        Stat = STA_NOINIT;
        return Stat;
    }
    UART_SendString("  [SD] CMD0 OK (Entered SPI Idle state)\r\n");

    // Verify operating voltage and detect SD version (CMD8)
    bool is_v2 = false;
    res = SD_SendCmd(CMD8, 0x1AA);
    if (res == 1) {
        uint8_t ocr[4];
        for (uint8_t i = 0; i < 4; i++) {
            SPI_ReceiveByte(&hspi1, &ocr[i]);
        }
        SD_CS_Deselect();
        
        // Check if the card accepted the 2.7-3.6V range (0x01) and echoed check pattern (0xAA)
        if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
            is_v2 = true;
            UART_SendString("  [SD] Card detected: SDv2+\r\n");
        }
    } else {
        SD_CS_Deselect();
        UART_SendString("  [SD] Card detected: SDv1 or MMC (CMD8 rejected)\r\n");
    }

    // Poll ACMD41 until the card leaves idle state
    uint32_t acmd41_arg = is_v2 ? (1UL << 30) : 0; // High Capacity Support (HCS) flag for SDv2
    start = CLK_GetTick();
    do {
        res = SD_SendCmd(ACMD41, acmd41_arg);
        SD_CS_Deselect();
        if (res == 0) {
            break;
        }
    } while ((CLK_GetTick() - start) < 1500);

    if (res != 0) {
        UART_SendString("  [SD] ACMD41 timeout (card failed to initialize)\r\n");
        Stat = STA_NOINIT;
        return Stat;
    }
    UART_SendString("  [SD] ACMD41 Ready\r\n");

    // Determine card capacity (SDSC vs SDHC/SDXC)
    if (is_v2) {
        if (SD_SendCmd(CMD58, 0) == 0) {
            uint8_t ocr[4];
            for (uint8_t i = 0; i < 4; i++) {
                SPI_ReceiveByte(&hspi1, &ocr[i]);
            }
            SD_CS_Deselect();

            // Bit 30 of OCR is CCS (Card Capacity Status): 1 = SDHC/SDXC, 0 = SDSC
            CardType = (ocr[0] & 0x40) ? 1 : 0;
        } else {
            SD_CS_Deselect();
            CardType = 0;
        }
    } else {
        // SDv1 cards are always Standard Capacity (SDSC, <= 2GB)
        CardType = 0;
    }

    if (CardType == 1) {
        UART_SendString("  [SD] Capacity: SDHC/SDXC (Block Addressing)\r\n");
    } else {
        UART_SendString("  [SD] Capacity: SDSC (Byte Addressing, e.g. 512MB)\r\n");
        // For SDSC, force block length to 512 bytes
        if (SD_SendCmd(CMD16, 512) != 0) {
            UART_SendString("  [SD] CMD16 (SET_BLOCKLEN) failed!\r\n");
            SD_CS_Deselect();
            Stat = STA_NOINIT;
            return Stat;
        }
        SD_CS_Deselect();
    }

    // Reconfigure SPI to maximum operational speed (20 MHz) upon successful initialization
    SPI_SetBaudrate(&hspi1, LL_SPI_BAUDRATEPRESCALER_DIV4);
    Stat &= ~STA_NOINIT;

    return Stat;
}

/* FatFs API: Get disk status */
DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != 0) {
        return STA_NOINIT;
    }
    return Stat;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0 || (Stat & STA_NOINIT)) {
        return RES_NOTRDY;
    }

    // SDSC cards use byte addressing; SDHC/SDXC use block addressing
    if (CardType == 0) {
        sector *= 512;
    }

    for (UINT i = 0; i < count; i++) {
        if (SD_SendCmd(CMD17, sector) != 0) {
            SD_CS_Deselect();
            return RES_ERROR;
        }

        // Wait for the data token indicating the start of the payload
        uint8_t token;
        uint32_t start = CLK_GetTick();
        do {
            SPI_ReceiveByte(&hspi1, &token);
        } while (token == 0xFF && (CLK_GetTick() - start) < 500);

        if (token != SD_DATA_TOKEN) {
            SD_CS_Deselect();
            return RES_ERROR;
        }

        // Retrieve exactly 512 bytes of data
        SPI_ReceiveBuffer(&hspi1, buff, 512);
        buff += 512;

        // Discard the 2-byte hardware CRC appended by the card
        uint8_t dummy;
        SPI_ReceiveByte(&hspi1, &dummy);
        SPI_ReceiveByte(&hspi1, &dummy);
        
        sector++;
    }

    SD_CS_Deselect();
    return RES_OK;
}


#if FF_FS_READONLY == 0
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0 || (Stat & STA_NOINIT)) {
        return RES_NOTRDY;
    }
    
    if (CardType == 0) {
        sector *= 512;
    }

    for (UINT i = 0; i < count; i++) {
        if (SD_SendCmd(CMD24, sector) != 0) {
            SD_CS_Deselect();
            return RES_ERROR;
        }

        // Pad with a dummy byte, then send the data token to begin the block
        SPI_TransferByte(&hspi1, 0xFF, NULL);
        SPI_TransferByte(&hspi1, SD_DATA_TOKEN, NULL);

        // Transmit the 512-byte payload
        SPI_TransmitBuffer(&hspi1, buff, 512);
        buff += 512;

        // Pad with a dummy 2-byte CRC
        SPI_TransferByte(&hspi1, 0xFF, NULL);
        SPI_TransferByte(&hspi1, 0xFF, NULL);

        // Validate the data response token from the card
        uint8_t response;
        SPI_ReceiveByte(&hspi1, &response);
        if ((response & 0x1F) != 0x05) {
            SD_CS_Deselect();
            return RES_ERROR;
        }

        // Wait for the flash memory programming cycle to finish
        if (!SD_WaitForReady(500)) {
            SD_CS_Deselect();
            return RES_ERROR;
        }
        
        sector++;
    }

    SD_CS_Deselect();
    return RES_OK;
}
#endif

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    (void)buff;
    if (pdrv != 0 || (Stat & STA_NOINIT)) {
        return RES_NOTRDY;
    }

    switch (cmd) {
        case CTRL_SYNC:
            SD_CS_Select();
            if (SD_WaitForReady(500)) {
                SD_CS_Deselect();
                return RES_OK;
            }
            SD_CS_Deselect();
            return RES_ERROR;
        default:
            return RES_PARERR;
    }
}

/**
 * @brief Returns current time for FatFs file timestamping.
 * @return 32-bit packed FAT format timestamp.
 */
DWORD get_fattime(void)
{
    RTC_DateTime_t dt;
    if (RTC_GetDateTime(&dt) == RTC_OK) {
        return ((DWORD)(dt.year >= 1980 ? (dt.year - 1980) : 0) << 25)
             | ((DWORD)dt.month << 21)
             | ((DWORD)dt.day << 16)
             | ((DWORD)dt.hours << 11)
             | ((DWORD)dt.minutes << 5)
             | ((DWORD)(dt.seconds / 2));
    }

    // Fallback default timestamp if RTC fails
    return ((DWORD)(2026 - 1980) << 25)
         | ((DWORD)9 << 21)
         | ((DWORD)19 << 16)
         | ((DWORD)12 << 11)
         | ((DWORD)0 << 5);
}
