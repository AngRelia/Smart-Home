#include "ws2812b.h"
#include "stm32f1xx_hal.h"

/* === 驱动内部变量 === */
static TIM_HandleTypeDef *s_ws_tim = NULL;
static volatile uint8_t s_busy = 0;
static WS2812B_Mode_t s_mode = WS2812B_MODE_OFF;

static uint16_t s_animTick = 0;
static uint16_t s_flowIndex = 0;
static uint8_t s_hue = 0;

/* GRB 每颗 24bit + RESET 槽位 */
static uint16_t s_pwmBuf[WS2812B_LED_COUNT * 24 + WS2812B_RESET_SLOTS];
static uint8_t s_colorBuf[WS2812B_LED_COUNT * 3];

static void WS2812B_Encode(void)
{
  uint32_t idx = 0;
  for (uint16_t i = 0; i < WS2812B_LED_COUNT; i++)
  {
    /* WS2812B 顺序：G R B */
    uint8_t g = s_colorBuf[i * 3 + 0];
    uint8_t r = s_colorBuf[i * 3 + 1];
    uint8_t b = s_colorBuf[i * 3 + 2];

    for (int8_t bit = 7; bit >= 0; bit--)
    {
      s_pwmBuf[idx++] = (g & (1U << bit)) ? WS2812B_T1H_TICKS : WS2812B_T0H_TICKS;
    }
    for (int8_t bit = 7; bit >= 0; bit--)
    {
      s_pwmBuf[idx++] = (r & (1U << bit)) ? WS2812B_T1H_TICKS : WS2812B_T0H_TICKS;
    }
    for (int8_t bit = 7; bit >= 0; bit--)
    {
      s_pwmBuf[idx++] = (b & (1U << bit)) ? WS2812B_T1H_TICKS : WS2812B_T0H_TICKS;
    }
  }

  for (uint16_t i = 0; i < WS2812B_RESET_SLOTS; i++)
  {
    s_pwmBuf[idx++] = 0U;
  }
}

void WS2812B_Init(TIM_HandleTypeDef *htim)
{
  s_ws_tim = htim;
}

void WS2812B_SetPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
  if (index >= WS2812B_LED_COUNT) return;
  s_colorBuf[index * 3 + 0] = g;
  s_colorBuf[index * 3 + 1] = r;
  s_colorBuf[index * 3 + 2] = b;
}

void WS2812B_SetAll(uint8_t r, uint8_t g, uint8_t b)
{
  for (uint16_t i = 0; i < WS2812B_LED_COUNT; i++)
  {
    WS2812B_SetPixel(i, r, g, b);
  }
}

void WS2812B_Show(void)
{
  if (s_ws_tim == NULL || s_busy) return;

  /* 【修复点】：在开始下一次 DMA 传输前，先停止上一次的传输，清理 HAL 状态机 */
  HAL_TIM_PWM_Stop_DMA(s_ws_tim, TIM_CHANNEL_3);

  WS2812B_Encode();
  s_busy = 1U;

  /* 启动 PWM DMA：使用 TIM1 CH3 */
  if (HAL_TIM_PWM_Start_DMA(s_ws_tim, TIM_CHANNEL_3,
                            (uint32_t *)s_pwmBuf,
                            (uint16_t)(WS2812B_LED_COUNT * 24 + WS2812B_RESET_SLOTS)) != HAL_OK)
  {
    s_busy = 0U;
  }
}

uint8_t WS2812B_IsBusy(void)
{
  return s_busy;
}

void WS2812B_ResetAnim(void)
{
  s_animTick = 0;
  s_flowIndex = 0;
  s_hue = 0;
}

static void WS2812B_SetAllScale(uint8_t r, uint8_t g, uint8_t b, uint8_t scale)
{
  uint16_t rr = ((uint16_t)r * scale) / 100U;
  uint16_t gg = ((uint16_t)g * scale) / 100U;
  uint16_t bb = ((uint16_t)b * scale) / 100U;
  WS2812B_SetAll((uint8_t)rr, (uint8_t)gg, (uint8_t)bb);
}

static void WS2812B_HueToRGB(uint8_t hue, uint8_t *r, uint8_t *g, uint8_t *b)
{
  /* 简单 0~255 hue 转 RGB */
  uint8_t region = hue / 43;       /* 0~5 */
  uint8_t remainder = (hue - (region * 43)) * 6; /* 0~255 */

  uint8_t p = 0;
  uint8_t q = (uint8_t)(255 - remainder);
  uint8_t t = (uint8_t)(remainder);

  switch (region)
  {
    case 0: *r = 255; *g = t;   *b = p;   break;
    case 1: *r = q;   *g = 255; *b = p;   break;
    case 2: *r = p;   *g = 255; *b = t;   break;
    case 3: *r = p;   *g = q;   *b = 255; break;
    case 4: *r = t;   *g = p;   *b = 255; break;
    default:*r = 255; *g = p;   *b = q;   break;
  }
}

void WS2812B_UpdateMode(WS2812B_Mode_t mode)
{
  if (mode >= WS2812B_MODE_MAX) return;
  s_mode = mode;
  WS2812B_ResetAnim();
}

/* 调用周期：建议 20~30ms */
static void WS2812B_RenderOff(void)
{
  WS2812B_SetAll(0, 0, 0);
}

static void WS2812B_RenderBreath(void)
{
  /* 0~100~0 循环 */
  uint16_t phase = s_animTick % 200U;
  uint8_t scale = (phase <= 100U) ? (uint8_t)phase : (uint8_t)(200U - phase);
  WS2812B_SetAllScale(0, 0, 255, scale); /* 蓝色呼吸 */
}

static void WS2812B_RenderFlow(void)
{
  /* 流水：单颗白点 */
  WS2812B_SetAll(0, 0, 0);
  WS2812B_SetPixel((uint16_t)(s_flowIndex % WS2812B_LED_COUNT), 255, 255, 255);
  s_flowIndex++;
}

static void WS2812B_RenderGradient(void)
{
  /* 全灯珠动态渐变：红->绿->蓝 循环 */
  uint8_t r, g, b;
  WS2812B_HueToRGB(s_hue, &r, &g, &b);
  WS2812B_SetAll(r, g, b);
  s_hue += 2U;
}

static void WS2812B_RenderColorful(void)
{
  /* 彩虹滚动 */
  for (uint16_t i = 0; i < WS2812B_LED_COUNT; i++)
  {
    uint8_t r, g, b;
    WS2812B_HueToRGB((uint8_t)(s_hue + i * 10U), &r, &g, &b);
    WS2812B_SetPixel(i, r, g, b);
  }
  s_hue += 3U;
}

/* 对外渲染入口：根据模式更新颜色缓存 */
void WS2812B_RenderFrame(void)
{
  switch (s_mode)
  {
    case WS2812B_MODE_OFF:      WS2812B_RenderOff();      break;
    case WS2812B_MODE_BREATH:   WS2812B_RenderBreath();   break;
    case WS2812B_MODE_FLOW:     WS2812B_RenderFlow();     break;
    case WS2812B_MODE_GRADIENT: WS2812B_RenderGradient(); break;
    case WS2812B_MODE_COLORFUL: WS2812B_RenderColorful(); break;
    default:                    WS2812B_RenderOff();      break;
  }

  s_animTick++;
  WS2812B_Show();
}

/* 【修复点】：DMA 完成回调只负责解除阻塞状态，不再调用 Stop_DMA 打断状态机 */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
  if (htim == s_ws_tim)
  {
    s_busy = 0U;
  }
}