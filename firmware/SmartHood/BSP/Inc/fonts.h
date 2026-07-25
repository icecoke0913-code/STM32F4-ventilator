#ifndef FONTS_H
#define FONTS_H

#include <stdbool.h>
#include <stdint.h>

#define FONT_5X7_WIDTH   5U
#define FONT_5X7_HEIGHT  7U

bool Fonts_GetGlyph5x7(char character,
                       uint8_t glyph[FONT_5X7_WIDTH]);

#endif
