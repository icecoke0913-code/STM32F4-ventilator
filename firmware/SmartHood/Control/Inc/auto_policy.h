/**
 * @file auto_policy.h
 * @brief DHT11温湿度驱动的AUTO档位策略接口。
 */

#ifndef AUTO_POLICY_H
#define AUTO_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#include "mode_manager.h"

/**
 * @brief SensorTask发布给AUTO策略的最新有效数据快照。
 */
typedef struct
{
    int16_t temperature_x10; /**< 温度，单位为0.1摄氏度。 */
    uint16_t humidity_x10;   /**< 湿度，单位为0.1%。 */
    bool valid;              /**< true表示快照内容来自校验成功的DHT11帧。 */
    uint32_t updated_tick;   /**< 快照发布时的RTOS Tick。 */
} AutoPolicySnapshot_t;

/**
 * @brief 根据温湿度快照计算AUTO候选电机请求。
 * @param snapshot 最新快照；为空、无效或过期时安全停止。
 * @param now_tick 当前RTOS Tick。
 * @param previous_auto_request 上一次AUTO候选请求，用于迟滞判断。
 * @return STOP、LOW或HIGH请求。
 */
ModeMotorRequest_t AutoPolicy_Evaluate(
    const AutoPolicySnapshot_t *snapshot,
    uint32_t now_tick,
    ModeMotorRequest_t previous_auto_request);

#endif /* AUTO_POLICY_H */
