/**
 * @file bsp_st7735s.c
 * @brief 基于SPI2的ST7735S初始化、显存传输和基础绘图实现。
 */

#include "bsp_st7735s.h"

#include <stddef.h>

#include "fonts.h"
#include "main.h"
#include "spi.h"

#define ST7735S_CMD_SWRESET  0x01U
#define ST7735S_CMD_SLPOUT   0x11U
#define ST7735S_CMD_NORON    0x13U
#define ST7735S_CMD_INVOFF   0x20U
#define ST7735S_CMD_DISPON   0x29U
#define ST7735S_CMD_CASET    0x2AU
#define ST7735S_CMD_RASET    0x2BU
#define ST7735S_CMD_RAMWR    0x2CU
#define ST7735S_CMD_MADCTL   0x36U
#define ST7735S_CMD_COLMOD   0x3AU
#define ST7735S_CMD_FRMCTR1  0xB1U
#define ST7735S_CMD_FRMCTR2  0xB2U
#define ST7735S_CMD_FRMCTR3  0xB3U
#define ST7735S_CMD_INVCTR   0xB4U
#define ST7735S_CMD_PWCTR1   0xC0U
#define ST7735S_CMD_PWCTR2   0xC1U
#define ST7735S_CMD_PWCTR3   0xC2U
#define ST7735S_CMD_PWCTR4   0xC3U
#define ST7735S_CMD_PWCTR5   0xC4U
#define ST7735S_CMD_VMCTR1   0xC5U
#define ST7735S_CMD_GMCTRP1  0xE0U
#define ST7735S_CMD_GMCTRN1  0xE1U

/* SPI发送超时与分块大小；32像素块用于控制栈占用和单次发送长度。 */
#define ST7735S_SPI_TIMEOUT_MS  100U
#define ST7735S_PIXEL_CHUNK      32U

/* M2实物边框/四角测试确认无坐标偏移，0xC0对应当前竖屏方向和RGB顺序。 */
#define ST7735S_X_OFFSET          0U
#define ST7735S_Y_OFFSET          0U
#define ST7735S_MADCTL_PORTRAIT  0xC0U

/** @brief 拉低低有效CS，选中TFT控制器。 */
static void ST7735S_Select(void)
{
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port,
                      TFT_CS_Pin,
                      GPIO_PIN_RESET);
}

/** @brief 拉高CS，结束当前SPI事务并释放TFT控制器。 */
static void ST7735S_Unselect(void)
{
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port,
                      TFT_CS_Pin,
                      GPIO_PIN_SET);
}

/**
 * @brief 使用SPI2阻塞发送一段非空字节流。
 * @param data 待发送数据指针。
 * @param length 字节数，必须大于0。
 * @return 参数有效且在100 ms内发送成功时返回true。
 */
static bool ST7735S_Transmit(const uint8_t *data,
                             uint16_t length)
{
    if ((data == NULL) || (length == 0U))
    {
        return false;
    }

    return HAL_SPI_Transmit(&hspi2,
                            (uint8_t *)data,
                            length,
                            ST7735S_SPI_TIMEOUT_MS) == HAL_OK;
}

/**
 * @brief 以DC低电平发送一个命令字节。
 * @param command ST7735S命令码。
 * @return 命令SPI事务成功返回true。
 */
static bool ST7735S_WriteCommand(uint8_t command)
{
    bool result;

    HAL_GPIO_WritePin(TFT_DC_GPIO_Port,
                      TFT_DC_Pin,
                      GPIO_PIN_RESET);

    ST7735S_Select();
    result = ST7735S_Transmit(&command, 1U);
    ST7735S_Unselect();

    return result;
}

/**
 * @brief 以DC高电平发送一段参数或像素数据。
 * @param data 数据指针。
 * @param length 字节数。
 * @return 数据SPI事务成功返回true。
 */
static bool ST7735S_WriteData(const uint8_t *data,
                              uint16_t length)
{
    bool result;

    HAL_GPIO_WritePin(TFT_DC_GPIO_Port,
                      TFT_DC_Pin,
                      GPIO_PIN_SET);

    ST7735S_Select();
    result = ST7735S_Transmit(data, length);
    ST7735S_Unselect();

    return result;
}

/**
 * @brief 依次发送命令及其可选参数。
 * @param command ST7735S命令码。
 * @param data 参数数据；length为0时不会访问该指针。
 * @param length 参数字节数。
 * @return 命令和可选数据阶段均成功时返回true。
 *
 * 命令阶段和数据阶段分别形成完整的CS低有效SPI事务，
 * DC电平用于让控制器区分命令与参数。
 */
static bool ST7735S_WriteCommandData(uint8_t command,
                                     const uint8_t *data,
                                     uint16_t length)
{
    if (!ST7735S_WriteCommand(command))
    {
        return false;
    }

    if (length == 0U)
    {
        return true;
    }

    return ST7735S_WriteData(data, length);
}

/**
 * @brief 控制高电平有效的TFT背光引脚。
 * @param enabled true点亮，false关闭。
 */
void BSP_ST7735S_SetBacklight(bool enabled)
{
    HAL_GPIO_WritePin(
        TFT_BLK_GPIO_Port,
        TFT_BLK_Pin,
        enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/** @brief 产生低脉冲硬件复位，并等待控制器内部稳定。 */
static void ST7735S_HardwareReset(void)
{
    HAL_GPIO_WritePin(TFT_RST_GPIO_Port,
                      TFT_RST_Pin,
                      GPIO_PIN_SET);
    HAL_Delay(10U);

    HAL_GPIO_WritePin(TFT_RST_GPIO_Port,
                      TFT_RST_Pin,
                      GPIO_PIN_RESET);
    HAL_Delay(20U);

    HAL_GPIO_WritePin(TFT_RST_GPIO_Port,
                      TFT_RST_Pin,
                      GPIO_PIN_SET);
    HAL_Delay(120U);
}

/**
 * @brief 按当前1.8寸ST7735S实物参数完成控制器初始化。
 * @return 任一命令、参数或清屏操作失败返回false，全部成功返回true。
 */
bool BSP_ST7735S_Init(void)
{
    /* 正常、空闲和局部模式使用的帧率参数。 */
    static const uint8_t frame_rate[] =
    {
        0x01U, 0x2CU, 0x2DU
    };

    static const uint8_t frame_rate_idle[] =
    {
        0x01U, 0x2CU, 0x2DU,
        0x01U, 0x2CU, 0x2DU
    };

    /* 显示反转方式、供电电压和VCOM电压参数。 */
    static const uint8_t inversion_control[] =
    {
        0x07U
    };

    static const uint8_t power_control_1[] =
    {
        0xA2U, 0x02U, 0x84U
    };

    static const uint8_t power_control_2[] =
    {
        0xC5U
    };

    static const uint8_t power_control_3[] =
    {
        0x0AU, 0x00U
    };

    static const uint8_t power_control_4[] =
    {
        0x8AU, 0x2AU
    };

    static const uint8_t power_control_5[] =
    {
        0x8AU, 0xEEU
    };

    static const uint8_t vcom_control[] =
    {
        0x0EU
    };

    /* 0x05选择16位RGB565像素格式。 */
    static const uint8_t pixel_format[] =
    {
        0x05U
    };

    /* 0xC0由实物方向与颜色测试确定为当前竖屏设置。 */
    static const uint8_t madctl[] =
    {
        ST7735S_MADCTL_PORTRAIT
    };

    /* 正极性伽马曲线，用于校准不同灰阶下的显示响应。 */
    static const uint8_t positive_gamma[] =
    {
        0x02U, 0x1CU, 0x07U, 0x12U,
        0x37U, 0x32U, 0x29U, 0x2DU,
        0x29U, 0x25U, 0x2BU, 0x39U,
        0x00U, 0x01U, 0x03U, 0x10U
    };

    /* 负极性伽马曲线，与正极性曲线共同控制灰阶响应。 */
    static const uint8_t negative_gamma[] =
    {
        0x03U, 0x1DU, 0x07U, 0x06U,
        0x2EU, 0x2CU, 0x29U, 0x2DU,
        0x2EU, 0x2EU, 0x37U, 0x3FU,
        0x00U, 0x00U, 0x02U, 0x10U
    };

    /* 初始化期间关闭背光并释放CS，随后执行硬件与软件复位。 */
    BSP_ST7735S_SetBacklight(false);
    ST7735S_Unselect();
    ST7735S_HardwareReset();

    if (!ST7735S_WriteCommand(ST7735S_CMD_SWRESET))
    {
        return false;
    }
    HAL_Delay(150U);

    /* 退出休眠后必须等待内部电源和显示时钟稳定。 */
    if (!ST7735S_WriteCommand(ST7735S_CMD_SLPOUT))
    {
        return false;
    }
    HAL_Delay(120U);

    /*
     * 依次写入帧率、反转、电源、VCOM、方向、像素格式和正负伽马参数；
     * 任一SPI阶段失败都立即停止初始化，避免在未知配置下继续显示。
     */
    if (!ST7735S_WriteCommandData(
            ST7735S_CMD_FRMCTR1,
            frame_rate,
            sizeof(frame_rate)) ||
        !ST7735S_WriteCommandData(
            ST7735S_CMD_FRMCTR2,
            frame_rate,
            sizeof(frame_rate)) ||
        !ST7735S_WriteCommandData(
            ST7735S_CMD_FRMCTR3,
            frame_rate_idle,
            sizeof(frame_rate_idle)) ||
        !ST7735S_WriteCommandData(
            ST7735S_CMD_INVCTR,
            inversion_control,
            sizeof(inversion_control)) ||
        !ST7735S_WriteCommandData(
            ST7735S_CMD_PWCTR1,
            power_control_1,
            sizeof(power_control_1)) ||
        !ST7735S_WriteCommandData(
            ST7735S_CMD_PWCTR2,
            power_control_2,
            sizeof(power_control_2)) ||
        !ST7735S_WriteCommandData(
            ST7735S_CMD_PWCTR3,
            power_control_3,
            sizeof(power_control_3)) ||
        !ST7735S_WriteCommandData(
            ST7735S_CMD_PWCTR4,
            power_control_4,
            sizeof(power_control_4)) ||
        !ST7735S_WriteCommandData(
            ST7735S_CMD_PWCTR5,
            power_control_5,
            sizeof(power_control_5)) ||
        !ST7735S_WriteCommandData(
            ST7735S_CMD_VMCTR1,
            vcom_control,
            sizeof(vcom_control)) ||
        !ST7735S_WriteCommand(ST7735S_CMD_INVOFF) ||
        !ST7735S_WriteCommandData(
            ST7735S_CMD_MADCTL,
            madctl,
            sizeof(madctl)) ||
        !ST7735S_WriteCommandData(
            ST7735S_CMD_COLMOD,
            pixel_format,
            sizeof(pixel_format)) ||
        !ST7735S_WriteCommandData(
            ST7735S_CMD_GMCTRP1,
            positive_gamma,
            sizeof(positive_gamma)) ||
        !ST7735S_WriteCommandData(
            ST7735S_CMD_GMCTRN1,
            negative_gamma,
            sizeof(negative_gamma)))
    {
        return false;
    }

    /* 进入正常显示模式并开启面板输出。 */
    if (!ST7735S_WriteCommand(ST7735S_CMD_NORON))
    {
        return false;
    }
    HAL_Delay(10U);

    if (!ST7735S_WriteCommand(ST7735S_CMD_DISPON))
    {
        return false;
    }
    HAL_Delay(100U);

    /* 先清黑屏再点亮背光，避免用户看到未初始化显存中的杂色。 */
    if (!BSP_ST7735S_FillScreen(ST7735S_COLOR_BLACK))
    {
        return false;
    }

    BSP_ST7735S_SetBacklight(true);

    return true;
}

/**
 * @brief 设置矩形显存写入窗口并进入RAM写入状态。
 * @param x 左上角X坐标。
 * @param y 左上角Y坐标。
 * @param width 窗口宽度。
 * @param height 窗口高度。
 * @return 区域有效且三条命令发送成功时返回true。
 */
bool BSP_ST7735S_SetAddressWindow(uint16_t x,
                                 uint16_t y,
                                 uint16_t width,
                                 uint16_t height)
{
    uint16_t x_end;
    uint16_t y_end;
    uint8_t coordinates[4];

    /* 拒绝零尺寸、起点越界及任何超出屏幕范围的窗口。 */
    if ((width == 0U) ||
        (height == 0U) ||
        (x >= ST7735S_WIDTH) ||
        (y >= ST7735S_HEIGHT) ||
        (width > (ST7735S_WIDTH - x)) ||
        (height > (ST7735S_HEIGHT - y)))
    {
        return false;
    }

    x = (uint16_t)(x + ST7735S_X_OFFSET);
    y = (uint16_t)(y + ST7735S_Y_OFFSET);

    /* 控制器使用包含首尾像素的端点坐标，因此终点为起点加尺寸减1。 */
    x_end = (uint16_t)(x + width - 1U);
    y_end = (uint16_t)(y + height - 1U);

    coordinates[0] = (uint8_t)(x >> 8);
    coordinates[1] = (uint8_t)x;
    coordinates[2] = (uint8_t)(x_end >> 8);
    coordinates[3] = (uint8_t)x_end;

    if (!ST7735S_WriteCommandData(
            ST7735S_CMD_CASET,
            coordinates,
            sizeof(coordinates)))
    {
        return false;
    }

    coordinates[0] = (uint8_t)(y >> 8);
    coordinates[1] = (uint8_t)y;
    coordinates[2] = (uint8_t)(y_end >> 8);
    coordinates[3] = (uint8_t)y_end;

    if (!ST7735S_WriteCommandData(
            ST7735S_CMD_RASET,
            coordinates,
            sizeof(coordinates)))
    {
        return false;
    }

    return ST7735S_WriteCommand(ST7735S_CMD_RAMWR);
}

/**
 * @brief 将RGB565像素按高字节在前的顺序分块写入显存。
 * @param pixels 16位RGB565像素数组。
 * @param count 像素数量。
 * @return 参数有效且所有SPI块发送成功时返回true。
 */
bool BSP_ST7735S_WritePixels(const uint16_t *pixels,
                             uint32_t count)
{
    uint8_t buffer[ST7735S_PIXEL_CHUNK * 2U];
    uint32_t processed = 0U;
    uint32_t chunk;
    uint32_t index;

    if ((pixels == NULL) || (count == 0U))
    {
        return false;
    }

    while (processed < count)
    {
        chunk = count - processed;

        if (chunk > ST7735S_PIXEL_CHUNK)
        {
            chunk = ST7735S_PIXEL_CHUNK;
        }

        /* ST7735S在线路上传输RGB565时要求每个像素高字节先发送。 */
        for (index = 0U; index < chunk; index++)
        {
            buffer[index * 2U] =
                (uint8_t)(pixels[processed + index] >> 8);

            buffer[(index * 2U) + 1U] =
                (uint8_t)pixels[processed + index];
        }

        if (!ST7735S_WriteData(
                buffer,
                (uint16_t)(chunk * 2U)))
        {
            return false;
        }

        processed += chunk;
    }

    return true;
}

/**
 * @brief 使用固定32像素缓冲区重复发送同一种颜色。
 * @param color RGB565颜色。
 * @param count 需要写入的像素总数。
 * @return 所有颜色块发送成功时返回true。
 *
 * 分块发送避免按矩形总面积分配大数组，同时减少逐像素SPI调用开销。
 */
static bool ST7735S_WriteRepeatedColor(uint16_t color,
                                       uint32_t count)
{
    uint16_t pixels[ST7735S_PIXEL_CHUNK];
    uint32_t chunk;
    uint32_t index;

    for (index = 0U;
         index < ST7735S_PIXEL_CHUNK;
         index++)
    {
        pixels[index] = color;
    }

    while (count > 0U)
    {
        chunk = count;

        if (chunk > ST7735S_PIXEL_CHUNK)
        {
            chunk = ST7735S_PIXEL_CHUNK;
        }

        if (!BSP_ST7735S_WritePixels(pixels, chunk))
        {
            return false;
        }

        count -= chunk;
    }

    return true;
}

/**
 * @brief 填充矩形，并将超出右侧或底部的尺寸裁剪到屏幕范围。
 * @param x 左上角X坐标。
 * @param y 左上角Y坐标。
 * @param width 请求宽度。
 * @param height 请求高度。
 * @param color RGB565填充颜色。
 * @return 起点和尺寸有效且写入成功时返回true。
 */
bool BSP_ST7735S_FillRect(uint16_t x,
                         uint16_t y,
                         uint16_t width,
                         uint16_t height,
                         uint16_t color)
{
    if ((width == 0U) ||
        (height == 0U) ||
        (x >= ST7735S_WIDTH) ||
        (y >= ST7735S_HEIGHT))
    {
        return false;
    }

    if (width > (ST7735S_WIDTH - x))
    {
        width = (uint16_t)(ST7735S_WIDTH - x);
    }

    if (height > (ST7735S_HEIGHT - y))
    {
        height = (uint16_t)(ST7735S_HEIGHT - y);
    }

    if (!BSP_ST7735S_SetAddressWindow(x,
                                      y,
                                      width,
                                      height))
    {
        return false;
    }

    return ST7735S_WriteRepeatedColor(
        color,
        (uint32_t)width * (uint32_t)height);
}

/**
 * @brief 使用指定RGB565颜色填充整个逻辑屏幕。
 * @param color 填充颜色。
 * @return 全屏矩形绘制结果。
 */
bool BSP_ST7735S_FillScreen(uint16_t color)
{
    return BSP_ST7735S_FillRect(
        0U,
        0U,
        ST7735S_WIDTH,
        ST7735S_HEIGHT,
        color);
}

/**
 * @brief 通过1×1矩形在指定坐标绘制单个像素。
 * @param x 像素X坐标。
 * @param y 像素Y坐标。
 * @param color RGB565颜色。
 * @return 像素绘制结果。
 */
bool BSP_ST7735S_DrawPixel(uint16_t x,
                          uint16_t y,
                          uint16_t color)
{
    return BSP_ST7735S_FillRect(x,
                               y,
                               1U,
                               1U,
                               color);
}

/**
 * @brief 按5列×7位字模绘制字符，并补绘1列背景作为字符间距。
 * @param x 字符左上角X坐标。
 * @param y 字符左上角Y坐标。
 * @param character ASCII字符。
 * @param foreground 前景RGB565颜色。
 * @param background 背景RGB565颜色。
 * @return 区域和字模有效且全部像素绘制成功时返回true。
 */
bool BSP_ST7735S_DrawChar(uint16_t x,
                         uint16_t y,
                         char character,
                         uint16_t foreground,
                         uint16_t background)
{
    uint8_t glyph[FONT_5X7_WIDTH];
    uint8_t column;
    uint8_t row;
    uint8_t bits;

    if ((x + FONT_5X7_WIDTH >= ST7735S_WIDTH) ||
        (y + FONT_5X7_HEIGHT > ST7735S_HEIGHT) ||
        !Fonts_GetGlyph5x7(character, glyph))
    {
        return false;
    }

    for (column = 0U;
         column < FONT_5X7_WIDTH;
         column++)
    {
        bits = glyph[column];

        /* 每列从bit0开始向下读取7个像素。 */
        for (row = 0U;
             row < FONT_5X7_HEIGHT;
             row++)
        {
            if (!BSP_ST7735S_DrawPixel(
                    (uint16_t)(x + column),
                    (uint16_t)(y + row),
                    ((bits & 0x01U) != 0U)
                        ? foreground
                        : background))
            {
                return false;
            }

            bits >>= 1;
        }
    }

    return BSP_ST7735S_FillRect(
        (uint16_t)(x + FONT_5X7_WIDTH),
        y,
        1U,
        FONT_5X7_HEIGHT,
        background);
}

/**
 * @brief 逐字符绘制字符串，处理换行符和右边界自动折行。
 * @param x 每行起始X坐标。
 * @param y 第一行起始Y坐标。
 * @param text 空字符结尾的字符串。
 * @param foreground 前景RGB565颜色。
 * @param background 背景RGB565颜色。
 * @return 文本有效、未超出屏幕底部且全部绘制成功时返回true。
 */
bool BSP_ST7735S_DrawString(uint16_t x,
                           uint16_t y,
                           const char *text,
                           uint16_t foreground,
                           uint16_t background)
{
    uint16_t cursor_x = x;
    uint16_t cursor_y = y;

    if (text == NULL)
    {
        return false;
    }

    while (*text != '\0')
    {
        if (*text == '\n')
        {
            cursor_x = x;
            cursor_y =
                (uint16_t)(cursor_y +
                           FONT_5X7_HEIGHT +
                           1U);
            text++;
            continue;
        }

        /* 当前行剩余宽度不足一个字符时，从初始X坐标开始下一行。 */
        if ((cursor_x +
             FONT_5X7_WIDTH +
             1U) > ST7735S_WIDTH)
        {
            cursor_x = x;
            cursor_y =
                (uint16_t)(cursor_y +
                           FONT_5X7_HEIGHT +
                           1U);
        }

        /* 自动折行后若字符高度超过屏幕底部，则停止并报告失败。 */
        if ((cursor_y +
             FONT_5X7_HEIGHT) > ST7735S_HEIGHT)
        {
            return false;
        }

        if (!BSP_ST7735S_DrawChar(
                cursor_x,
                cursor_y,
                *text,
                foreground,
                background))
        {
            return false;
        }

        cursor_x =
            (uint16_t)(cursor_x +
                       FONT_5X7_WIDTH +
                       1U);

        text++;
    }

    return true;
}
