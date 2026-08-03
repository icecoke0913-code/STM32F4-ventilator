/**
 * @file bsp_dht11.h
 * @brief DHT11温湿度读取状态、定点数据和公共接口。
 */

#ifndef BSP_DHT11_H
#define BSP_DHT11_H

#include <stdbool.h>
#include <stdint.h>

/** DHT11一次读取的结果状态。 */
typedef enum
{
    DHT11_STATUS_OK = 0,          /**< 数据完整且校验和正确。 */
    DHT11_STATUS_TIMEOUT,         /**< 等待响应或数据边沿超时。 */
    DHT11_STATUS_CHECKSUM_ERROR   /**< 收到40位数据但校验和错误。 */
} DHT11_Status_t;

/** DHT11一次有效读取转换后的定点数结果。 */
typedef struct
{
    int16_t temperature_x10; /**< 摄氏温度扩大10倍，可表示负温度。 */
    uint16_t humidity_x10;   /**< 相对湿度百分数扩大10倍。 */
} DHT11_Data_t;

/**
 * @brief 将PD0恢复为输入上拉并启动TIM5微秒计数器。
 * @return TIM5启动成功返回true，否则返回false。
 */
bool BSP_DHT11_Init(void);

/**
 * @brief 按DHT11单总线协议读取一次温湿度。
 * @param data 有效输出结构指针；只有返回OK时才更新其内容。
 * @return OK、TIMEOUT或CHECKSUM_ERROR。
 */
DHT11_Status_t BSP_DHT11_Read(DHT11_Data_t *data);

#endif
