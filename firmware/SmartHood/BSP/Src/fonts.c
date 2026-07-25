#include "fonts.h"

#include <stddef.h>

typedef struct
{
    char character;
    uint8_t columns[FONT_5X7_WIDTH];
} FontGlyph5x7;

static const FontGlyph5x7 glyphs[] =
{
    { ' ', { 0x00U, 0x00U, 0x00U, 0x00U, 0x00U } },
    { '?', { 0x02U, 0x01U, 0x51U, 0x09U, 0x06U } },

    { '0', { 0x3EU, 0x51U, 0x49U, 0x45U, 0x3EU } },
    { '1', { 0x00U, 0x42U, 0x7FU, 0x40U, 0x00U } },
    { '2', { 0x42U, 0x61U, 0x51U, 0x49U, 0x46U } },
    { '3', { 0x21U, 0x41U, 0x45U, 0x4BU, 0x31U } },
    { '5', { 0x27U, 0x45U, 0x45U, 0x45U, 0x39U } },
    { '6', { 0x3CU, 0x4AU, 0x49U, 0x49U, 0x30U } },
    { '7', { 0x01U, 0x71U, 0x09U, 0x05U, 0x03U } },
    { '8', { 0x36U, 0x49U, 0x49U, 0x49U, 0x36U } },

    { 'H', { 0x7FU, 0x08U, 0x08U, 0x08U, 0x7FU } },
    { 'K', { 0x7FU, 0x08U, 0x14U, 0x22U, 0x41U } },
    { 'O', { 0x3EU, 0x41U, 0x41U, 0x41U, 0x3EU } },
    { 'S', { 0x46U, 0x49U, 0x49U, 0x49U, 0x31U } },
    { 'T', { 0x01U, 0x01U, 0x7FU, 0x01U, 0x01U } },

    { 'a', { 0x20U, 0x54U, 0x54U, 0x54U, 0x78U } },
    { 'd', { 0x38U, 0x44U, 0x44U, 0x48U, 0x7FU } },
    { 'm', { 0x7CU, 0x04U, 0x18U, 0x04U, 0x78U } },
    { 'o', { 0x38U, 0x44U, 0x44U, 0x44U, 0x38U } },
    { 'r', { 0x7CU, 0x08U, 0x04U, 0x04U, 0x08U } },
    { 't', { 0x04U, 0x3FU, 0x44U, 0x40U, 0x20U } },
    { 'x', { 0x44U, 0x28U, 0x10U, 0x28U, 0x44U } }
};

bool Fonts_GetGlyph5x7(char character,
                       uint8_t glyph[FONT_5X7_WIDTH])
{
    size_t index;
    size_t column;
    const uint8_t *source = NULL;

    if (glyph == NULL)
    {
        return false;
    }

    for (index = 0U;
         index < (sizeof(glyphs) / sizeof(glyphs[0]));
         index++)
    {
        if (glyphs[index].character == character)
        {
            source = glyphs[index].columns;
            break;
        }
    }

    if (source == NULL)
    {
        for (index = 0U;
             index < (sizeof(glyphs) / sizeof(glyphs[0]));
             index++)
        {
            if (glyphs[index].character == '?')
            {
                source = glyphs[index].columns;
                break;
            }
        }
    }

    if (source == NULL)
    {
        return false;
    }

    for (column = 0U; column < FONT_5X7_WIDTH; column++)
    {
        glyph[column] = source[column];
    }

    return true;
}
