#ifndef BSP_ST7735S_H
#define BSP_ST7735S_H

#include <stdbool.h>
#include <stdint.h>

#define ST7735S_WIDTH   128U
#define ST7735S_HEIGHT  160U

#define ST7735S_COLOR_BLACK    0x0000U
#define ST7735S_COLOR_WHITE    0xFFFFU
#define ST7735S_COLOR_RED      0xF800U
#define ST7735S_COLOR_GREEN    0x07E0U
#define ST7735S_COLOR_BLUE     0x001FU
#define ST7735S_COLOR_YELLOW   0xFFE0U
#define ST7735S_COLOR_CYAN     0x07FFU
#define ST7735S_COLOR_MAGENTA  0xF81FU

bool BSP_ST7735S_Init(void);

void BSP_ST7735S_SetBacklight(bool enabled);

bool BSP_ST7735S_SetAddressWindow(uint16_t x,
                                 uint16_t y,
                                 uint16_t width,
                                 uint16_t height);

bool BSP_ST7735S_WritePixels(const uint16_t *pixels,
                             uint32_t count);

bool BSP_ST7735S_FillScreen(uint16_t color);

bool BSP_ST7735S_DrawPixel(uint16_t x,
                          uint16_t y,
                          uint16_t color);

bool BSP_ST7735S_FillRect(uint16_t x,
                         uint16_t y,
                         uint16_t width,
                         uint16_t height,
                         uint16_t color);

bool BSP_ST7735S_DrawChar(uint16_t x,
                         uint16_t y,
                         char character,
                         uint16_t foreground,
                         uint16_t background);

bool BSP_ST7735S_DrawString(uint16_t x,
                           uint16_t y,
                           const char *text,
                           uint16_t foreground,
                           uint16_t background);

#endif
