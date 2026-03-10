#ifndef __LIGHT_SENSOR_H__
#define __LIGHT_SENSOR_H__

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 光敏传感器：ADC 读取，固定使用 PA4 -> ADC1_IN4 */

/* 返回原始 ADC 值 (0~4095) */
uint16_t LightSensor_ReadRaw(void);

/* 返回电压 (mV) */
uint32_t LightSensor_ReadMv(uint32_t vref_mv);

#ifdef __cplusplus
}
#endif

#endif
