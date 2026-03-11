#ifndef __MQ2_H__
#define __MQ2_H__

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MQ-2 气敏传感器：ADC 读取，固定使用 ADC1 通道5 (PA5)
 * CubeMX 请将 PA5 配置为 ADC1_IN5
 */

/* 返回原始 ADC 值 (0~4095) */
uint16_t MQ2_ReadRaw(void);

/* 返回电压 (mV) */
uint32_t MQ2_ReadMv(uint32_t vref_mv);

/* 返回 0~100% 的相对浓度（线性映射，仅用于显示） */
uint8_t MQ2_ReadPercent(void);

#ifdef __cplusplus
}
#endif

#endif

