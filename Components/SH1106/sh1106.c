#include "sh1106.h"

// Pointer to the active SPI bus handle
static SPI_Handle_t *s_hspi = NULL;

// In-memory monochrome display buffer (128x64 pixels = 1024 bytes)
static uint8_t s_buffer[SH1106_BUFFER_SIZE];

void SH1106_AttachBus(SPI_Handle_t *hspi)
{
    // Attach shared SPI handle used for OLED communication
    s_hspi = hspi;
}

void SH1106_WriteCommand(uint8_t cmd)
{
    if (s_hspi == NULL) {
        return;
    }

    // Command mode: pull DC line LOW
    LL_GPIO_ResetOutputPin(SH1106_DC_PORT, SH1106_DC_PIN);

    // Select display: pull CS line LOW
    LL_GPIO_ResetOutputPin(SH1106_CS_PORT, SH1106_CS_PIN);

    // Transmit command byte over SPI bus
    SPI_TransferByte(s_hspi, cmd, NULL);

    // Ensure last clock cycle completed before releasing CS
    while (LL_SPI_IsActiveFlag_BSY(s_hspi->instance));

    // Release display: drive CS line HIGH
    LL_GPIO_SetOutputPin(SH1106_CS_PORT, SH1106_CS_PIN);
}

void SH1106_WriteData(uint8_t data)
{
    if (s_hspi == NULL) {
        return;
    }

    // Data mode: drive DC line HIGH
    LL_GPIO_SetOutputPin(SH1106_DC_PORT, SH1106_DC_PIN);

    // Select display: pull CS line LOW
    LL_GPIO_ResetOutputPin(SH1106_CS_PORT, SH1106_CS_PIN);

    // Transmit graphic data byte over SPI bus
    SPI_TransferByte(s_hspi, data, NULL);

    // Ensure last clock cycle completed before releasing CS
    while (LL_SPI_IsActiveFlag_BSY(s_hspi->instance));

    // Release display: drive CS line HIGH
    LL_GPIO_SetOutputPin(SH1106_CS_PORT, SH1106_CS_PIN);
}

void SH1106_Clear(void)
{
    // Clear entire display buffer to black (all pixels OFF)
    SH1106_Fill(SH1106_COLOR_BLACK);
}

void SH1106_Fill(SH1106_Color_t color)
{
    // Fill buffer with either 0xFF (all pixels ON) or 0x00 (all pixels OFF)
    uint8_t fill_byte = (color == SH1106_COLOR_WHITE) ? 0xFF : 0x00;
    memset(s_buffer, fill_byte, sizeof(s_buffer));
}

void SH1106_DrawPixel(int16_t x, int16_t y, SH1106_Color_t color)
{
    // Boundary check: discard pixels outside screen coordinates
    if (x < 0 || x >= (int16_t)SH1106_WIDTH || y < 0 || y >= (int16_t)SH1106_HEIGHT) {
        return;
    }

    // Map (x, y) coordinates to memory page (8 rows per page) and bit position
    uint16_t page = (uint16_t)(y / 8);
    uint8_t  bit  = (uint8_t)(y % 8);
    uint16_t index = (uint16_t)x + (page * SH1106_WIDTH);

    // Set or clear bit in framebuffer
    if (color == SH1106_COLOR_WHITE) {
        s_buffer[index] |= (uint8_t)(1U << bit);
    } else {
        s_buffer[index] &= (uint8_t)(~(1U << bit));
    }
}

uint8_t* SH1106_GetBuffer(void)
{
    // Return direct pointer to the framebuffer for custom operations
    return s_buffer;
}

void SH1106_UpdateScreen(void)
{
    if (s_hspi == NULL) {
        return;
    }

    // Acquire exclusive bus access
    if (!SPI_Lock(100)) {
        return;
    }

    // Ensure SPI bus runs at safe OLED clock rate (2.5 MHz)
    SPI_SetBaudrate(s_hspi, LL_SPI_BAUDRATEPRESCALER_DIV32);

    // Stream 8 pages (each page is 8 vertical pixels across 128 columns)
    for (uint8_t page = 0; page < 8; page++) {
        // Set page start address (0xB0 to 0xB7)
        SH1106_WriteCommand(0xB0 + page);

        // Set lower column address nibble with +2 offset for SH1106 RAM alignment
        SH1106_WriteCommand(0x00 | (SH1106_COLUMN_OFFSET & 0x0F));

        // Set higher column address nibble
        SH1106_WriteCommand(0x10 | ((SH1106_COLUMN_OFFSET >> 4) & 0x0F));

        // Switch to Data mode (DC HIGH) and select display (CS LOW)
        LL_GPIO_SetOutputPin(SH1106_DC_PORT, SH1106_DC_PIN);
        LL_GPIO_ResetOutputPin(SH1106_CS_PORT, SH1106_CS_PIN);

        // Transmit 128 bytes for the current page in one continuous burst
        SPI_TransmitBuffer(s_hspi, &s_buffer[page * SH1106_WIDTH], SH1106_WIDTH);

        // Ensure transmission complete before deasserting CS
        while (LL_SPI_IsActiveFlag_BSY(s_hspi->instance));

        // Deselect display (CS HIGH)
        LL_GPIO_SetOutputPin(SH1106_CS_PORT, SH1106_CS_PIN);
    }

    // Release exclusive bus access
    SPI_Unlock();
}

SH1106_Status_t SH1106_Init(void)
{
    if (s_hspi == NULL) {
        return SH1106_ERR_NULL_PTR;
    }

    // Enable GPIO peripheral clocks for control lines (CS: PB6, DC: PC7, RES: PA9)
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOC);

    // Initialize CS pin (PB6): Output Push-Pull, High Speed, Pull-Up, Default HIGH (Deselected)
    LL_GPIO_SetOutputPin(SH1106_CS_PORT, SH1106_CS_PIN);
    LL_GPIO_SetPinMode(SH1106_CS_PORT, SH1106_CS_PIN, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinOutputType(SH1106_CS_PORT, SH1106_CS_PIN, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(SH1106_CS_PORT, SH1106_CS_PIN, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinPull(SH1106_CS_PORT, SH1106_CS_PIN, LL_GPIO_PULL_UP);

    // Initialize DC pin (PC7): Output Push-Pull, High Speed, Default LOW (Command)
    LL_GPIO_ResetOutputPin(SH1106_DC_PORT, SH1106_DC_PIN);
    LL_GPIO_SetPinMode(SH1106_DC_PORT, SH1106_DC_PIN, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinOutputType(SH1106_DC_PORT, SH1106_DC_PIN, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(SH1106_DC_PORT, SH1106_DC_PIN, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinPull(SH1106_DC_PORT, SH1106_DC_PIN, LL_GPIO_PULL_NO);

    // Initialize RES pin (PA9): Output Push-Pull, High Speed, Pull-Up, Default HIGH
    LL_GPIO_SetOutputPin(SH1106_RES_PORT, SH1106_RES_PIN);
    LL_GPIO_SetPinMode(SH1106_RES_PORT, SH1106_RES_PIN, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinOutputType(SH1106_RES_PORT, SH1106_RES_PIN, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(SH1106_RES_PORT, SH1106_RES_PIN, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinPull(SH1106_RES_PORT, SH1106_RES_PIN, LL_GPIO_PULL_UP);

    // Switch SPI bus to safe 2.5 MHz (DIV32) clock rate
    SPI_SetBaudrate(s_hspi, LL_SPI_BAUDRATEPRESCALER_DIV32);

    // Hardware reset pulse: hold RES LOW for 20 ms then release HIGH
    LL_GPIO_ResetOutputPin(SH1106_RES_PORT, SH1106_RES_PIN);
    CLK_Delay(20);
    LL_GPIO_SetOutputPin(SH1106_RES_PORT, SH1106_RES_PIN);
    CLK_Delay(50);

    // Turn Display OFF during configuration
    SH1106_WriteCommand(0xAE);

    // Set Display Clock Divide Ratio / Oscillator Frequency
    SH1106_WriteCommand(0xD5);
    SH1106_WriteCommand(0x80);

    // Set Multiplex Ratio (64 lines)
    SH1106_WriteCommand(0xA8);
    SH1106_WriteCommand(0x3F);

    // Set Display Offset to 0
    SH1106_WriteCommand(0xD3);
    SH1106_WriteCommand(0x00);

    // Set Display Start Line (line 0)
    SH1106_WriteCommand(0x40);

    // Dual Charge Pump / DC-DC activation:
    // 1) SH1106 Internal DC-DC converter ON (7.4V/8.0V)
    SH1106_WriteCommand(0xAD);
    SH1106_WriteCommand(0x8B);
    SH1106_WriteCommand(0x32);

    // 2) SSD1306 Internal Charge Pump ON (7.5V)
    SH1106_WriteCommand(0x8D);
    SH1106_WriteCommand(0x14);

    // Set Segment Re-map (column 127 mapped to SEG0)
    SH1106_WriteCommand(0xA1);

    // Set COM Output Scan Direction (remapped mode, COM[N-1] to COM0)
    SH1106_WriteCommand(0xC8);

    // Set COM Pins Hardware Configuration
    SH1106_WriteCommand(0xDA);
    SH1106_WriteCommand(0x12);

    // Set Contrast Control (maximum brightness)
    SH1106_WriteCommand(0x81);
    SH1106_WriteCommand(0xFF);

    // Set Pre-charge Period
    SH1106_WriteCommand(0xD9);
    SH1106_WriteCommand(0xF1);

    // Set VCOM Deselect Level
    SH1106_WriteCommand(0xDB);
    SH1106_WriteCommand(0x40);

    // Set Page Addressing Mode (for SSD1306 compatibility)
    SH1106_WriteCommand(0x20);
    SH1106_WriteCommand(0x02);

    // Entire Display ON: Resume to RAM content
    SH1106_WriteCommand(0xA4);

    // Set Normal Display mode (non-inverted)
    SH1106_WriteCommand(0xA6);

    // Draw diagnostic startup splash pattern so the display is verified at boot
    SH1106_Clear();
    SH1106_DrawRect(0, 0, 127, 63, SH1106_COLOR_WHITE);
    SH1106_DrawString(8, 12, "OBD-II SCANNER", &Font_6x8, SH1106_COLOR_WHITE);
    SH1106_DrawString(8, 28, "SH1106 / SSD1306", &Font_6x8, SH1106_COLOR_WHITE);
    SH1106_DrawString(8, 44, "SYSTEM BOOT OK", &Font_6x8, SH1106_COLOR_WHITE);
    SH1106_UpdateScreen();

    // Turn Display ON
    SH1106_WriteCommand(0xAF);

    return SH1106_OK;
}

void SH1106_DrawChar(int16_t x, int16_t y, char c, const FontDef_t *font, SH1106_Color_t color)
{
    if (font == NULL || font->data == NULL) {
        return;
    }

    // Default unknown or non-printable ASCII characters to '?'
    if (c < 32 || c > 126) {
        c = '?';
    }

    // Calculate font glyph data offset
    uint32_t glyph_offset = (uint32_t)(c - 32) * (font->width - 1);
    SH1106_Color_t bg_color = (color == SH1106_COLOR_WHITE) ? SH1106_COLOR_BLACK : SH1106_COLOR_WHITE;

    // Render active glyph columns (e.g. 5 columns for Font_6x8)
    for (uint8_t col = 0; col < (font->width - 1); col++) {
        uint8_t col_data = font->data[glyph_offset + col];
        for (uint8_t row = 0; row < font->height; row++) {
            if ((col_data >> row) & 0x01) {
                SH1106_DrawPixel(x + col, y + row, color);
            } else {
                SH1106_DrawPixel(x + col, y + row, bg_color);
            }
        }
    }

    // Clear rightmost column to ensure clean inter-character spacing
    for (uint8_t row = 0; row < font->height; row++) {
        SH1106_DrawPixel(x + (font->width - 1), y + row, bg_color);
    }
}

void SH1106_DrawString(int16_t x, int16_t y, const char *str, const FontDef_t *font, SH1106_Color_t color)
{
    if (str == NULL || font == NULL) {
        return;
    }

    int16_t curr_x = x;
    int16_t curr_y = y;

    // Render each character handling newlines and carriage returns
    while (*str != '\0') {
        if (*str == '\n') {
            curr_y += font->height;
            curr_x = x;
        } else if (*str == '\r') {
            curr_x = x;
        } else {
            SH1106_DrawChar(curr_x, curr_y, *str, font, color);
            curr_x += font->width;
        }
        str++;
    }
}

void SH1106_DrawHLine(int16_t x, int16_t y, int16_t w, SH1106_Color_t color)
{
    // Fast horizontal line drawing with bounds clipping
    if (y < 0 || y >= (int16_t)SH1106_HEIGHT || w <= 0) return;
    for (int16_t i = 0; i < w; i++) {
        SH1106_DrawPixel(x + i, y, color);
    }
}

void SH1106_DrawVLine(int16_t x, int16_t y, int16_t h, SH1106_Color_t color)
{
    // Fast vertical line drawing with bounds clipping
    if (x < 0 || x >= (int16_t)SH1106_WIDTH || h <= 0) return;
    for (int16_t i = 0; i < h; i++) {
        SH1106_DrawPixel(x, y + i, color);
    }
}

void SH1106_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, SH1106_Color_t color)
{
    // Bresenham's line algorithm for arbitrary slope line rendering
    int16_t dx = (x1 >= x0) ? (x1 - x0) : (x0 - x1);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t dy = (y1 >= y0) ? (y0 - y1) : (y1 - y0);
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx + dy;

    while (1) {
        SH1106_DrawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void SH1106_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, SH1106_Color_t color)
{
    // Draw 4 bounding edges of rectangle outline
    SH1106_DrawHLine(x, y, w, color);
    SH1106_DrawHLine(x, y + h - 1, w, color);
    SH1106_DrawVLine(x, y, h, color);
    SH1106_DrawVLine(x + w - 1, y, h, color);
}

void SH1106_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, SH1106_Color_t color)
{
    // Fill rectangle using successive horizontal lines
    for (int16_t i = 0; i < h; i++) {
        SH1106_DrawHLine(x, y + i, w, color);
    }
}