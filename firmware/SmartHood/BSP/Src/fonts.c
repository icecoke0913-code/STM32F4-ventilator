/**
 * @file fonts.c
 * @brief 启动自检所需5×7 ASCII字形表和回退查找实现。
 */

#include "fonts.h"

#include <stddef.h>

/*
 * 每个字形由5个字节表示，每个字节对应一列；
 * bit0位于字符顶部，bit6位于字符底部，bit7未使用。
 */
typedef struct
{
    char character;
    uint8_t columns[FONT_5X7_WIDTH];
} FontGlyph5x7;

/* 当前集合覆盖启动自检所需的空格、问号、数字和部分英文字母。 */
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

/**
 * @brief 线性查找并复制字符的5列字模。
 * @param character 需要查找的字符。
 * @param glyph 5字节输出数组。
 * @return 找到目标或问号回退字形返回true，否则返回false。
 *
 * 首次查找目标字符；若不存在则再次查找问号作为可见占位符，
 * 最后将5列数据复制到调用者缓冲区，字模常量表本身保持只读。
 */
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

    /* 未收录的字符统一使用问号，避免屏幕上出现无提示的空白。 */
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

    /* 输出数组长度由接口约定为FONT_5X7_WIDTH，即固定5列。 */
    for (column = 0U; column < FONT_5X7_WIDTH; column++)
    {
        glyph[column] = source[column];
    }

    return true;
}
