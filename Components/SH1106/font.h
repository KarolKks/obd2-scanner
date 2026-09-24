#ifndef FONT_H
#define FONT_H

#include <stdint.h>

/**
 * @brief Font descriptor structure.
 */
typedef struct {
    const uint8_t width;       /*!< Character cell width in pixels */
    const uint8_t height;      /*!< Character cell height in pixels */
    const uint8_t *data;       /*!< Pointer to font glyph bitmap array */
} FontDef_t;

/**
 * @brief Standard 6x8 ASCII font (5x7 glyph with 1px column spacing).
 *        Allows 21 characters per line on a 128x64 display (up to 8 lines).
 */
extern const FontDef_t Font_6x8;

#endif /* FONT_H */
