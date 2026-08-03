/**
 * @file fonts.h
 * @brief 5×7 ASCII列式字模尺寸和查找接口。
 */

#ifndef FONTS_H
#define FONTS_H

#include <stdbool.h>
#include <stdint.h>

/* 每个字模由5列组成，每列使用低7位表示从上到下的7个像素。 */
#define FONT_5X7_WIDTH   5U
#define FONT_5X7_HEIGHT  7U

/**
 * @brief 获取一个字符的5×7列式字模，未知字符回退为问号字形。
 * @param character 需要查找的ASCII字符。
 * @param glyph 长度必须为5字节的输出数组。
 * @return 成功找到目标或问号字形返回true；输出无效或无回退字形返回false。
 */
bool Fonts_GetGlyph5x7(char character,
                       uint8_t glyph[FONT_5X7_WIDTH]);

#endif
