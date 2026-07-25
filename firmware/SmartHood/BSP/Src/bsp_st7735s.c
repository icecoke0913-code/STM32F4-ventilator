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

#define ST7735S_SPI_TIMEOUT_MS  100U
#define ST7735S_PIXEL_CHUNK      32U
#define ST7735S_X_OFFSET          0U
#define ST7735S_Y_OFFSET          0U
#define ST7735S_MADCTL_PORTRAIT  0xC8U

static void ST7735S_Select(void)
{
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port,
                      TFT_CS_Pin,
                      GPIO_PIN_RESET);
}

static void ST7735S_Unselect(void)
{
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port,
                      TFT_CS_Pin,
                      GPIO_PIN_SET);
}

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

void BSP_ST7735S_SetBacklight(bool enabled)
{
    HAL_GPIO_WritePin(
        TFT_BLK_GPIO_Port,
        TFT_BLK_Pin,
        enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

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

bool BSP_ST7735S_Init(void)
{
    static const uint8_t frame_rate[] =
    {
        0x01U, 0x2CU, 0x2DU
    };

    static const uint8_t frame_rate_idle[] =
    {
        0x01U, 0x2CU, 0x2DU,
        0x01U, 0x2CU, 0x2DU
    };

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

    static const uint8_t pixel_format[] =
    {
        0x05U
    };

    static const uint8_t madctl[] =
    {
        ST7735S_MADCTL_PORTRAIT
    };

    static const uint8_t positive_gamma[] =
    {
        0x02U, 0x1CU, 0x07U, 0x12U,
        0x37U, 0x32U, 0x29U, 0x2DU,
        0x29U, 0x25U, 0x2BU, 0x39U,
        0x00U, 0x01U, 0x03U, 0x10U
    };

    static const uint8_t negative_gamma[] =
    {
        0x03U, 0x1DU, 0x07U, 0x06U,
        0x2EU, 0x2CU, 0x29U, 0x2DU,
        0x2EU, 0x2EU, 0x37U, 0x3FU,
        0x00U, 0x00U, 0x02U, 0x10U
    };

    BSP_ST7735S_SetBacklight(false);
    ST7735S_Unselect();
    ST7735S_HardwareReset();

    if (!ST7735S_WriteCommand(ST7735S_CMD_SWRESET))
    {
        return false;
    }
    HAL_Delay(150U);

    if (!ST7735S_WriteCommand(ST7735S_CMD_SLPOUT))
    {
        return false;
    }
    HAL_Delay(120U);

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

    if (!BSP_ST7735S_FillScreen(ST7735S_COLOR_BLACK))
    {
        return false;
    }

    BSP_ST7735S_SetBacklight(true);

    return true;
}

bool BSP_ST7735S_SetAddressWindow(uint16_t x,
                                 uint16_t y,
                                 uint16_t width,
                                 uint16_t height)
{
    uint16_t x_end;
    uint16_t y_end;
    uint8_t coordinates[4];

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

bool BSP_ST7735S_FillScreen(uint16_t color)
{
    return BSP_ST7735S_FillRect(
        0U,
        0U,
        ST7735S_WIDTH,
        ST7735S_HEIGHT,
        color);
}

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
