#ifndef SH1106_H
#define SH1106_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "stm32l4xx.h"
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_gpio.h"
#include "spi.h"
#include "clk.h"
#include "font.h"

/* Display physical dimensions */
#define SH1106_WIDTH                128U
#define SH1106_HEIGHT               64U
#define SH1106_BUFFER_SIZE          ((SH1106_WIDTH * SH1106_HEIGHT) / 8U) /* 1024 bytes */

/* SH1106 internal RAM column offset (visible 128 cols start at column 2 in 132-col RAM) */
#define SH1106_COLUMN_OFFSET        2U

/* Hardware Pin Definitions (NUCLEO-L476RG) */
#define SH1106_CS_PORT              GPIOB
#define SH1106_CS_PIN               LL_GPIO_PIN_6       

#define SH1106_DC_PORT              GPIOC
#define SH1106_DC_PIN               LL_GPIO_PIN_7       

#define SH1106_RES_PORT             GPIOA
#define SH1106_RES_PIN              LL_GPIO_PIN_9       

/* Display color options */
typedef enum {
    SH1106_COLOR_BLACK = 0,
    SH1106_COLOR_WHITE = 1
} SH1106_Color_t;

/* Status codes */
typedef enum {
    SH1106_OK = 0,
    SH1106_ERR_NULL_PTR,
    SH1106_ERR_SPI
} SH1106_Status_t;

/**
 * @brief  Binds the SPI bus instance (hspi1) to the SH1106 driver.
 * @param  hspi Pointer to initialized SPI handle.
 */
void SH1106_AttachBus(SPI_Handle_t *hspi);

/**
 * @brief  Initializes GPIO control pins (CS, DC, RES) and executes the display power-up sequence.
 * @return SH1106_OK on success, error code otherwise.
 */
SH1106_Status_t SH1106_Init(void);

/**
 * @brief  Sends a single 1-byte command to the SH1106 controller (DC LOW).
 * @param  cmd Byte command to send.
 */
void SH1106_WriteCommand(uint8_t cmd);

/**
 * @brief  Sends a single 1-byte data to the SH1106 display RAM (DC HIGH).
 * @param  data Byte to write.
 */
void SH1106_WriteData(uint8_t data);

/**
 * @brief  Clears the local RAM framebuffer to black (all zeroes).
 */
void SH1106_Clear(void);

/**
 * @brief  Fills the local RAM framebuffer with a uniform color (white or black).
 * @param  color SH1106_COLOR_BLACK or SH1106_COLOR_WHITE.
 */
void SH1106_Fill(SH1106_Color_t color);

/**
 * @brief  Draws a single pixel in the local framebuffer.
 * @param  x     X coordinate (0 to 127).
 * @param  y     Y coordinate (0 to 63).
 * @param  color Pixel color (SH1106_COLOR_WHITE to set, SH1106_COLOR_BLACK to clear).
 */
void SH1106_DrawPixel(int16_t x, int16_t y, SH1106_Color_t color);

/**
 * @brief  Transfers the entire 1024-byte framebuffer to the physical display over SPI.
 * @details Iterates over all 8 pages, sets page and column addresses (with +2 offset),
 *          and streams 128 bytes per page.
 */
void SH1106_UpdateScreen(void);

/**
 * @brief  Returns a pointer to the internal framebuffer for direct rendering.
 * @return Pointer to uint8_t buffer of size SH1106_BUFFER_SIZE.
 */
uint8_t* SH1106_GetBuffer(void);

/**
 * @brief  Draws a single ASCII character onto the framebuffer using the specified font.
 * 
 * @param  x     X top-left coordinate.
 * @param  y     Y top-left coordinate.
 * @param  c     ASCII character to draw (32 ' ' to 126 '~').
 * @param  font  Pointer to font definition (e.g. &Font_6x8).
 * @param  color Character color (SH1106_COLOR_WHITE or SH1106_COLOR_BLACK).
 */
void SH1106_DrawChar(int16_t x, int16_t y, char c, const FontDef_t *font, SH1106_Color_t color);

/**
 * @brief  Draws a null-terminated string onto the framebuffer.
 * 
 * @param  x     X starting coordinate.
 * @param  y     Y starting coordinate.
 * @param  str   Pointer to null-terminated ASCII string.
 * @param  font  Pointer to font definition (e.g. &Font_6x8).
 * @param  color Text color (SH1106_COLOR_WHITE or SH1106_COLOR_BLACK).
 */
void SH1106_DrawString(int16_t x, int16_t y, const char *str, const FontDef_t *font, SH1106_Color_t color);

/**
 * @brief  Draws a straight line from (x0, y0) to (x1, y1) using Bresenham's algorithm.
 */
void SH1106_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, SH1106_Color_t color);

/**
 * @brief  Draws a fast horizontal line.
 */
void SH1106_DrawHLine(int16_t x, int16_t y, int16_t w, SH1106_Color_t color);

/**
 * @brief  Draws a fast vertical line.
 */
void SH1106_DrawVLine(int16_t x, int16_t y, int16_t h, SH1106_Color_t color);

/**
 * @brief  Draws an outlined rectangle.
 */
void SH1106_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, SH1106_Color_t color);

/**
 * @brief  Draws a filled solid rectangle.
 */
void SH1106_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, SH1106_Color_t color);

#endif /* SH1106_H */
