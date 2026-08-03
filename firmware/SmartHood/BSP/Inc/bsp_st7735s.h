/**
 * @file bsp_st7735s.h
 * @brief 1.8寸ST7735S竖屏尺寸、RGB565颜色和绘图接口。
 */

#ifndef BSP_ST7735S_H
#define BSP_ST7735S_H

#include <stdbool.h>
#include <stdint.h>

/* 经过实物方向校准的逻辑竖屏尺寸，左上角坐标为(0,0)。 */
#define ST7735S_WIDTH   128U
#define ST7735S_HEIGHT  160U

/* RGB565颜色：红色5位、绿色6位、蓝色5位。 */
#define ST7735S_COLOR_BLACK    0x0000U
#define ST7735S_COLOR_WHITE    0xFFFFU
#define ST7735S_COLOR_RED      0xF800U
#define ST7735S_COLOR_GREEN    0x07E0U
#define ST7735S_COLOR_BLUE     0x001FU
#define ST7735S_COLOR_YELLOW   0xFFE0U
#define ST7735S_COLOR_CYAN     0x07FFU
#define ST7735S_COLOR_MAGENTA  0xF81FU

/**
 * @brief 复位并配置ST7735S，清黑屏后点亮背光。
 * @return 全部SPI命令和清屏操作成功返回true，否则返回false。
 */
bool BSP_ST7735S_Init(void);

/**
 * @brief 控制TFT背光，硬件为高电平点亮。
 * @param enabled true点亮背光，false关闭背光。
 */
void BSP_ST7735S_SetBacklight(bool enabled);

/**
 * @brief 设置后续显存写入的矩形窗口。
 * @param x 窗口左上角X坐标。
 * @param y 窗口左上角Y坐标。
 * @param width 窗口宽度，必须大于0且不能越界。
 * @param height 窗口高度，必须大于0且不能越界。
 * @return 参数有效且列、行、写显存命令均发送成功时返回true。
 */
bool BSP_ST7735S_SetAddressWindow(uint16_t x,
                                 uint16_t y,
                                 uint16_t width,
                                 uint16_t height);

/**
 * @brief 向已经设置的显存窗口连续写入RGB565像素。
 * @param pixels 像素数组，颜色以STM32端16位RGB565保存。
 * @param count 需要发送的像素数量，必须大于0。
 * @return 参数有效且全部SPI数据发送成功时返回true。
 */
bool BSP_ST7735S_WritePixels(const uint16_t *pixels,
                             uint32_t count);

/**
 * @brief 使用一种RGB565颜色填充整个128×160屏幕。
 * @param color RGB565颜色值。
 * @return 全屏窗口设置和像素发送均成功时返回true。
 */
bool BSP_ST7735S_FillScreen(uint16_t color);

/**
 * @brief 在指定坐标绘制一个RGB565像素。
 * @param x 像素X坐标。
 * @param y 像素Y坐标。
 * @param color RGB565颜色值。
 * @return 坐标有效且绘制成功时返回true。
 */
bool BSP_ST7735S_DrawPixel(uint16_t x,
                          uint16_t y,
                          uint16_t color);

/**
 * @brief 填充一个矩形，超出右侧或底部的尺寸会被裁剪。
 * @param x 矩形左上角X坐标。
 * @param y 矩形左上角Y坐标。
 * @param width 请求宽度，必须大于0。
 * @param height 请求高度，必须大于0。
 * @param color RGB565填充颜色。
 * @return 起点和尺寸有效且全部像素写入成功时返回true。
 */
bool BSP_ST7735S_FillRect(uint16_t x,
                         uint16_t y,
                         uint16_t width,
                         uint16_t height,
                         uint16_t color);

/**
 * @brief 使用5×7字模在指定坐标绘制单个字符和一列字符间隔。
 * @param x 字符左上角X坐标。
 * @param y 字符左上角Y坐标。
 * @param character 要绘制的ASCII字符，未知字符回退为问号字形。
 * @param foreground 字形前景RGB565颜色。
 * @param background 字形背景RGB565颜色。
 * @return 字符区域未越界、字模可用且绘制成功时返回true。
 */
bool BSP_ST7735S_DrawChar(uint16_t x,
                         uint16_t y,
                         char character,
                         uint16_t foreground,
                         uint16_t background);

/**
 * @brief 从指定位置逐字符绘制字符串，并支持换行和自动折行。
 * @param x 每一行开始的X坐标。
 * @param y 第一行开始的Y坐标。
 * @param text 以空字符结尾的字符串。
 * @param foreground 字形前景RGB565颜色。
 * @param background 字形背景RGB565颜色。
 * @return 参数有效且字符串未超出屏幕底部、全部绘制成功时返回true。
 */
bool BSP_ST7735S_DrawString(uint16_t x,
                           uint16_t y,
                           const char *text,
                           uint16_t foreground,
                           uint16_t background);

#endif
