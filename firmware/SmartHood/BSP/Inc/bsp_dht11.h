#ifndef BSP_DHT11_H
#define BSP_DHT11_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    DHT11_STATUS_OK = 0,
    DHT11_STATUS_TIMEOUT,
    DHT11_STATUS_CHECKSUM_ERROR
} DHT11_Status_t;

typedef struct
{
    int16_t temperature_x10;
    uint16_t humidity_x10;
} DHT11_Data_t;

bool BSP_DHT11_Init(void);
DHT11_Status_t BSP_DHT11_Read(DHT11_Data_t *data);

#endif
