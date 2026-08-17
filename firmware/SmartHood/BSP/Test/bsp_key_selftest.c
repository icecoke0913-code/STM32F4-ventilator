/**
 * @file bsp_key_selftest.c
 * @brief 用固定电平和毫秒Tick验证按键事件识别。
 */

#include "bsp_key_selftest.h"

#include "bsp_key.h"

/**
 * @brief 验证单击必须等待双击窗口结束后才上报。
 */
static bool BSP_Key_TestSingle(void)
{
    BSP_Key_t key;

    BSP_Key_Init(&key, false, 0U);

    /* 100ms开始按下，连续稳定40ms后才确认按下。 */
    if (BSP_Key_Process(&key, true, 100U) != BSP_KEY_EVENT_NONE)
    {
        return false;
    }
    if (BSP_Key_Process(&key, true, 140U) != BSP_KEY_EVENT_NONE)
    {
        return false;
    }

    /* 200ms开始释放，240ms确认释放并进入双击等待窗口。 */
    if (BSP_Key_Process(&key, false, 200U) != BSP_KEY_EVENT_NONE)
    {
        return false;
    }
    if (BSP_Key_Process(&key, false, 240U) != BSP_KEY_EVENT_NONE)
    {
        return false;
    }

    /* 349ms时仍不能确认单击，350ms时必须上报一次单击。 */
    if (BSP_Key_Process(&key, false, 589U) != BSP_KEY_EVENT_NONE)
    {
        return false;
    }

    return BSP_Key_Process(&key, false, 590U) ==
           BSP_KEY_EVENT_SHORT;
}

/**
 * @brief 验证两次点击只产生一次双击事件。
 */
static bool BSP_Key_TestDouble(void)
{
    BSP_Key_t key;

    BSP_Key_Init(&key, false, 0U);

    /* 第一次点击的每一步都不能提前夹带单击事件。 */
    if (BSP_Key_Process(&key, true, 100U) != BSP_KEY_EVENT_NONE)
    {
        return false;
    }
    if (BSP_Key_Process(&key, true, 140U) != BSP_KEY_EVENT_NONE)
    {
        return false;
    }
    if (BSP_Key_Process(&key, false, 200U) != BSP_KEY_EVENT_NONE)
    {
        return false;
    }
    if (BSP_Key_Process(&key, false, 240U) != BSP_KEY_EVENT_NONE)
    {
        return false;
    }

    /* 第二次按下在350ms窗口内开始，第二次释放时上报双击。 */
    if (BSP_Key_Process(&key, true, 400U) != BSP_KEY_EVENT_NONE)
    {
        return false;
    }
    if (BSP_Key_Process(&key, true, 440U) != BSP_KEY_EVENT_NONE)
    {
        return false;
    }
    if (BSP_Key_Process(&key, false, 480U) != BSP_KEY_EVENT_NONE)
    {
        return false;
    }

    return BSP_Key_Process(&key, false, 520U) ==
           BSP_KEY_EVENT_DOUBLE;
}

/**
 * @brief 验证一次持续按压只上报一个长按事件。
 */
static bool BSP_Key_TestLongOnce(void)
{
    BSP_Key_t key;

    BSP_Key_Init(&key, false, 0U);
    (void)BSP_Key_Process(&key, true, 100U);
    (void)BSP_Key_Process(&key, true, 140U);

    /* 从稳定按下开始计时，999ms不触发，1000ms触发。 */
    if (BSP_Key_Process(&key, true, 1139U) != BSP_KEY_EVENT_NONE)
    {
        return false;
    }
    if (BSP_Key_Process(&key, true, 1140U) != BSP_KEY_EVENT_LONG)
    {
        return false;
    }

    /* 继续按住和随后释放都不能再次产生长按或单击。 */
    if (BSP_Key_Process(&key, true, 1500U) != BSP_KEY_EVENT_NONE)
    {
        return false;
    }
    (void)BSP_Key_Process(&key, false, 1600U);

    return BSP_Key_Process(&key, false, 1640U) ==
           BSP_KEY_EVENT_NONE;
}

/**
 * @brief 验证上电时按住PA0不会误触发长按。
 */
static bool BSP_Key_TestStartupHeld(void)
{
    BSP_Key_t key;

    BSP_Key_Init(&key, true, 0U);

    /* 未先释放时，即使按住超过长按阈值也不产生事件。 */
    if (BSP_Key_Process(&key, true, 1200U) != BSP_KEY_EVENT_NONE)
    {
        return false;
    }

    /* 稳定释放后模块才允许识别下一次完整点击。 */
    (void)BSP_Key_Process(&key, false, 1300U);
    if (BSP_Key_Process(&key, false, 1340U) != BSP_KEY_EVENT_NONE)
    {
        return false;
    }

    (void)BSP_Key_Process(&key, true, 1400U);
    (void)BSP_Key_Process(&key, true, 1440U);
    (void)BSP_Key_Process(&key, false, 1500U);
    (void)BSP_Key_Process(&key, false, 1540U);

    return BSP_Key_Process(&key, false, 1890U) ==
           BSP_KEY_EVENT_SHORT;
}

/**
 * @brief 验证32位毫秒Tick回绕不破坏长按计时。
 */
static bool BSP_Key_TestTickWrapLong(void)
{
    BSP_Key_t key;

    BSP_Key_Init(&key, false, 0xFFFFFFF0U);
    (void)BSP_Key_Process(&key, true, 0xFFFFFFF5U);

    /* 回绕后的0x1D与0xFFFFFFF5相差40ms，应确认稳定按下。 */
    if (BSP_Key_Process(&key, true, 0x0000001DU) !=
        BSP_KEY_EVENT_NONE)
    {
        return false;
    }

    /* 从稳定按下到0x405正好经过1000ms，应触发长按。 */
    return BSP_Key_Process(&key, true, 0x00000405U) ==
           BSP_KEY_EVENT_LONG;
}

/**
 * @brief 验证单击等待窗口跨越32位毫秒Tick回绕时仍准确。
 */
static bool BSP_Key_TestTickWrapSingle(void)
{
    BSP_Key_t key;

    BSP_Key_Init(&key, false, 0xFFFFFF70U);

    /* 在回绕前完成一次点击，稳定释放时刻为0xFFFFFFE8。 */
    if (BSP_Key_Process(&key, true, 0xFFFFFF80U) !=
        BSP_KEY_EVENT_NONE)
    {
        return false;
    }
    if (BSP_Key_Process(&key, true, 0xFFFFFFA8U) !=
        BSP_KEY_EVENT_NONE)
    {
        return false;
    }
    if (BSP_Key_Process(&key, false, 0xFFFFFFC0U) !=
        BSP_KEY_EVENT_NONE)
    {
        return false;
    }
    if (BSP_Key_Process(&key, false, 0xFFFFFFE8U) !=
        BSP_KEY_EVENT_NONE)
    {
        return false;
    }

    /* 回绕后经过349ms仍等待，经过350ms才上报一次单击。 */
    if (BSP_Key_Process(&key, false, 0x00000145U) !=
        BSP_KEY_EVENT_NONE)
    {
        return false;
    }

    return BSP_Key_Process(&key, false, 0x00000146U) ==
           BSP_KEY_EVENT_SHORT;
}

/**
 * @brief 验证双击窗口跨越32位毫秒Tick回绕时不夹带单击。
 */
static bool BSP_Key_TestTickWrapDouble(void)
{
    BSP_Key_t key;

    BSP_Key_Init(&key, false, 0xFFFFFF70U);

    /* 第一次点击在回绕前完成，所有中间步骤都必须无事件。 */
    if (BSP_Key_Process(&key, true, 0xFFFFFF80U) !=
        BSP_KEY_EVENT_NONE)
    {
        return false;
    }
    if (BSP_Key_Process(&key, true, 0xFFFFFFA8U) !=
        BSP_KEY_EVENT_NONE)
    {
        return false;
    }
    if (BSP_Key_Process(&key, false, 0xFFFFFFC0U) !=
        BSP_KEY_EVENT_NONE)
    {
        return false;
    }
    if (BSP_Key_Process(&key, false, 0xFFFFFFE8U) !=
        BSP_KEY_EVENT_NONE)
    {
        return false;
    }

    /* 第二次点击在回绕后完成，只有稳定释放时上报双击。 */
    if (BSP_Key_Process(&key, true, 0x00000080U) !=
        BSP_KEY_EVENT_NONE)
    {
        return false;
    }
    if (BSP_Key_Process(&key, true, 0x000000A8U) !=
        BSP_KEY_EVENT_NONE)
    {
        return false;
    }
    if (BSP_Key_Process(&key, false, 0x000000C0U) !=
        BSP_KEY_EVENT_NONE)
    {
        return false;
    }

    return BSP_Key_Process(&key, false, 0x000000E8U) ==
           BSP_KEY_EVENT_DOUBLE;
}

/**
 * @brief 验证不足40ms的电平抖动不会改变稳定状态。
 */
static bool BSP_Key_TestBounce(void)
{
    BSP_Key_t key;

    BSP_Key_Init(&key, false, 0U);

    if (BSP_Key_Process(&key, true, 10U) != BSP_KEY_EVENT_NONE)
    {
        return false;
    }
    if (BSP_Key_Process(&key, false, 20U) != BSP_KEY_EVENT_NONE)
    {
        return false;
    }
    if (BSP_Key_Process(&key, true, 30U) != BSP_KEY_EVENT_NONE)
    {
        return false;
    }
    if (BSP_Key_Process(&key, false, 50U) != BSP_KEY_EVENT_NONE)
    {
        return false;
    }
    if (BSP_Key_Process(&key, false, 90U) != BSP_KEY_EVENT_NONE)
    {
        return false;
    }

    return !BSP_Key_IsPressed(&key);
}

bool BSP_Key_RunSelfTests(void)
{
    return BSP_Key_TestBounce() &&
           BSP_Key_TestSingle() &&
           BSP_Key_TestDouble() &&
           BSP_Key_TestLongOnce() &&
           BSP_Key_TestStartupHeld() &&
           BSP_Key_TestTickWrapLong() &&
           BSP_Key_TestTickWrapSingle() &&
           BSP_Key_TestTickWrapDouble();
}
