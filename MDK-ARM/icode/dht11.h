#ifndef __DHT11_H__
#define __DHT11_H__

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 请在 CubeMX 里把 DHT11 数据引脚的 User Label 设为 "DHT11"
 * 这样 main.h 会自动生成：
 *   #define DHT11_Pin ...
 *   #define DHT11_GPIO_Port ...
 */
#if !defined(DHT11_Pin) || !defined(DHT11_GPIO_Port)
#error "DHT11_Pin / DHT11_GPIO_Port 未定义。请在 CubeMX 将该引脚命名为 DHT11。"
#endif

typedef struct
{
    uint8_t hum_int;   /* 湿度整数部分 */
    uint8_t hum_dec;   /* 湿度小数部分（DHT11通常为0） */
    uint8_t temp_int;  /* 温度整数部分 */
    uint8_t temp_dec;  /* 温度小数部分（DHT11通常为0） */
} DHT11_Data_t;

/* 初始化引脚为上拉输入（空闲态） */
void DHT11_Init(void);

/*
 * 读取一次温湿度
 * 返回值：
 *   HAL_OK      读取成功
 *   HAL_ERROR   校验失败
 *   HAL_TIMEOUT 时序超时（接线/上拉/时序问题）
 *   HAL_BUSY    两次读取间隔<1秒（DHT11限制）
 */
HAL_StatusTypeDef DHT11_Read(DHT11_Data_t *out);

#ifdef __cplusplus
}
#endif

#endif
