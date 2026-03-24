#ifndef __WS2812B_H
#define __WS2812B_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tim.h"
#include <stdint.h>

/* === WS2812B 参数 === */
#define WS2812B_LED_COUNT     10   /* 默认 10 颗，可自行修改 */

/* TIM1 72MHz：800kHz -> Period=89 (ARR=89, 90个tick=1.25us) */
#define WS2812B_TIMER_PERIOD  89U

/* 1.25us 周期内高电平时长 (1 tick ≈ 0.0138us)
 * 0码：手册要求 0.3us -> 0.3 / 0.0138 ≈ 22 ticks
 * 1码：手册要求 0.9us -> 0.9 / 0.0138 ≈ 65 ticks
 */
#define WS2812B_T0H_TICKS      22U
#define WS2812B_T1H_TICKS      65U

/* 复位时间：手册要求 >80us -> 这里给 80 个 slots * 1.25us = 100us，留足裕量 */
#define WS2812B_RESET_SLOTS    80U

typedef enum
{
  WS2812B_MODE_OFF = 0,
  WS2812B_MODE_BREATH,
  WS2812B_MODE_FLOW,
  WS2812B_MODE_GRADIENT,
  WS2812B_MODE_COLORFUL,
  WS2812B_MODE_MAX
} WS2812B_Mode_t;

void WS2812B_Init(TIM_HandleTypeDef *htim);
void WS2812B_SetPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
void WS2812B_SetAll(uint8_t r, uint8_t g, uint8_t b);
void WS2812B_Show(void);
uint8_t WS2812B_IsBusy(void);
void WS2812B_ResetAnim(void);
void WS2812B_UpdateMode(WS2812B_Mode_t mode);
void WS2812B_RenderFrame(void);

#ifdef __cplusplus
}
#endif

#endif /* __WS2812B_H */