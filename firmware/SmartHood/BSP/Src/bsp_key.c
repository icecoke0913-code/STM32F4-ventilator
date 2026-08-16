/**
 * @file bsp_key.c
 * @brief PA0单按键消抖、单击、双击和长按状态机。
 */

#include "bsp_key.h"

#include <stddef.h>

/**
 * @brief 使用无符号减法计算时间差。
 *
 * 即使32位毫秒Tick发生回绕，短于约49.7天的时间差仍然正确。
 */
static uint32_t BSP_Key_Elapsed(uint32_t now_ms,
                               uint32_t since_ms)
{
    return (uint32_t)(now_ms - since_ms);
}

void BSP_Key_Init(BSP_Key_t *key,
                  bool initial_pressed,
                  uint32_t now_ms)
{
    if (key == NULL)
    {
        return;
    }

    key->candidate_pressed = initial_pressed;
    key->stable_pressed = initial_pressed;

    /*
     * 如果上电时按键已经按下，先禁止事件识别；
     * 必须稳定释放后才允许识别下一次按压。
     */
    key->armed = !initial_pressed;
    key->press_active = false;
    key->long_reported = false;
    key->click_pending = false;
    key->second_press = false;
    key->candidate_since_ms = now_ms;
    key->press_since_ms = now_ms;
    key->first_release_ms = now_ms;
}

BSP_KeyEvent_t BSP_Key_Process(BSP_Key_t *key,
                               bool raw_pressed,
                               uint32_t now_ms)
{
    if (key == NULL)
    {
        return BSP_KEY_EVENT_NONE;
    }

    /*
     * 原始电平变化时，只更新候选状态并重新开始消抖计时。
     * 在候选状态保持40ms以前，不改变稳定状态。
     */
    if (raw_pressed != key->candidate_pressed)
    {
        key->candidate_pressed = raw_pressed;
        key->candidate_since_ms = now_ms;
    }

    if ((key->candidate_pressed != key->stable_pressed) &&
        (BSP_Key_Elapsed(now_ms,
                         key->candidate_since_ms) >=
         BSP_KEY_DEBOUNCE_MS))
    {
        key->stable_pressed = key->candidate_pressed;

        /*
         * 上电时已经按住按键，必须先稳定释放；
         * 该次释放只负责使能按键，不产生点击事件。
         */
        if (!key->armed)
        {
            if (!key->stable_pressed)
            {
                key->armed = true;
                key->click_pending = false;
                key->second_press = false;
            }

            return BSP_KEY_EVENT_NONE;
        }

        if (key->stable_pressed)
        {
            /* 稳定按下，开始本次按压和长按计时。 */
            key->press_active = true;
            key->press_since_ms = now_ms;
            key->long_reported = false;

            if (key->click_pending)
            {
                /*
                 * 第二次原始按下在350ms窗口内开始，
                 * 即使其消抖完成时间略晚，仍属于双击候选。
                 */
                if (BSP_Key_Elapsed(
                        key->candidate_since_ms,
                        key->first_release_ms) <=
                    BSP_KEY_DOUBLE_CLICK_MS)
                {
                    key->second_press = true;
                }
                else
                {
                    /*
                     * 第二次按下开始得太晚：
                     * 先上报之前等待确认的单击，
                     * 当前按压仍保留为下一次新点击。
                     */
                    key->click_pending = false;
                    key->second_press = false;

                    return BSP_KEY_EVENT_SHORT;
                }
            }
        }
        else
        {
            /* 稳定释放，结束本次按压。 */
            key->press_active = false;

            if (key->long_reported)
            {
                /*
                 * 长按已经在按住期间上报，
                 * 释放时不能再产生单击或双击。
                 */
                key->long_reported = false;
                key->second_press = false;

                return BSP_KEY_EVENT_NONE;
            }

            if (key->second_press)
            {
                /* 第二次点击完成，只上报一次双击。 */
                key->click_pending = false;
                key->second_press = false;

                return BSP_KEY_EVENT_DOUBLE;
            }

            /*
             * 第一次点击完成，先等待350ms；
             * 窗口结束后才能确认它是单击。
             */
            key->click_pending = true;
            key->first_release_ms = now_ms;
        }
    }

    /*
     * 持续稳定按下达到1000ms时立即上报长按。
     * long_reported保证一次按压只上报一次。
     */
    if (key->armed &&
        key->stable_pressed &&
        key->press_active &&
        !key->long_reported &&
        (BSP_Key_Elapsed(now_ms,
                         key->press_since_ms) >=
         BSP_KEY_LONG_PRESS_MS))
    {
        key->long_reported = true;
        key->click_pending = false;
        key->second_press = false;

        return BSP_KEY_EVENT_LONG;
    }

    /*
     * 没有第二次按下且双击窗口已经结束，
     * 此时才把第一次点击确认为单击。
     */
    if (key->click_pending &&
        !key->stable_pressed &&
        !key->candidate_pressed &&
        (BSP_Key_Elapsed(now_ms,
                         key->first_release_ms) >=
         BSP_KEY_DOUBLE_CLICK_MS))
    {
        key->click_pending = false;

        return BSP_KEY_EVENT_SHORT;
    }

    return BSP_KEY_EVENT_NONE;
}

bool BSP_Key_IsPressed(const BSP_Key_t *key)
{
    return (key != NULL) && key->stable_pressed;
}
