/**
 * @file bsp_key.h
 * @brief PA0单按键非阻塞事件识别接口。
 */

#ifndef BSP_KEY_H
#define BSP_KEY_H

#include <stdbool.h>
#include <stdint.h>

/** 原始电平连续稳定40ms后才确认状态变化。 */
#define BSP_KEY_DEBOUNCE_MS     40U

/** 第一次点击后等待第二次按下的最长时间。 */
#define BSP_KEY_DOUBLE_CLICK_MS 350U

/** 持续按下达到1000ms时产生一次长按事件。 */
#define BSP_KEY_LONG_PRESS_MS   1000U

/**
 * @brief 按键模块能够产生的事件。
 */
typedef enum
{
    BSP_KEY_EVENT_NONE = 0, /**< 当前没有完整按键事件。 */
    BSP_KEY_EVENT_SHORT,    /**< 单击。 */
    BSP_KEY_EVENT_DOUBLE,   /**< 双击。 */
    BSP_KEY_EVENT_LONG      /**< 长按。 */
} BSP_KeyEvent_t;

/**
 * @brief 保存按键消抖和事件识别所需的全部状态。
 */
typedef struct
{
    bool candidate_pressed;       /**< 当前候选原始状态。 */
    bool stable_pressed;          /**< 已通过消抖的稳定状态。 */
    bool armed;                   /**< 上电按住时，释放后才置位。 */
    bool press_active;            /**< 当前存在一次有效按压。 */
    bool long_reported;           /**< 本次按压是否已上报长按。 */
    bool click_pending;           /**< 是否正在等待第二次点击。 */
    bool second_press;            /**< 当前按压是否是第二次点击。 */
    uint32_t candidate_since_ms;  /**< 候选状态开始时间。 */
    uint32_t press_since_ms;      /**< 稳定按下开始时间。 */
    uint32_t first_release_ms;    /**< 第一次稳定释放时间。 */
} BSP_Key_t;

void BSP_Key_Init(BSP_Key_t *key,
                  bool initial_pressed,
                  uint32_t now_ms);

BSP_KeyEvent_t BSP_Key_Process(BSP_Key_t *key,
                               bool raw_pressed,
                               uint32_t now_ms);

bool BSP_Key_IsPressed(const BSP_Key_t *key);

#endif /* BSP_KEY_H */
