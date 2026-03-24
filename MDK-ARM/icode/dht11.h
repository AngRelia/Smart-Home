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

/* 读取参数配置（用于新PCB调优） */
typedef struct
{
    uint8_t  sample_us;        /* 兼容保留参数（当前版本未使用） */
    uint8_t  max_retries;      /* 失败后的最大重试次数，默认2 */
    uint16_t bit_timeout_us;   /* 等待bit电平翻转超时，默认120us */
    uint16_t resp_timeout_us;  /* 等待传感器响应超时，默认200us */
    uint16_t high_threshold_us;/* 高电平脉宽判1阈值，默认20(计数单位近似us) */
} DHT11_Config_t;

/* 运行统计（用于定位是超时多还是校验失败多） */
typedef struct
{
    uint32_t total_calls;
    uint32_t ok_count;
    uint32_t busy_count;
    uint32_t timeout_count;
    uint32_t checksum_error_count;
    uint32_t retry_success_count;
} DHT11_Stats_t;

/* 初始化引脚为上拉输入（空闲态） */
void DHT11_Init(void);

/* 设置/获取驱动参数（传NULL不生效） */
void DHT11_SetConfig(const DHT11_Config_t *cfg);
void DHT11_GetConfig(DHT11_Config_t *cfg);

/* 重置/读取统计信息 */
void DHT11_ResetStats(void);
void DHT11_GetStats(DHT11_Stats_t *stats);

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
