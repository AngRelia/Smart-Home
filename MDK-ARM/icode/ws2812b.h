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

/* 1.25us 周期内高电平时长
 * 0码：约0.35us -> 30/90
 * 1码：约0.70us -> 60/90
 */
#define WS2812B_T0H_TICKS      30U
#define WS2812B_T1H_TICKS      60U

/* 复位时间 >50us，50*1.25us=62.5us */
#define WS2812B_RESET_SLOTS    50U

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