#include "my_encoder.h"

/* ======================= 用户可按需调整的参数 ======================= */
#ifndef MY_ENCODER_COUNTS_PER_STEP
/*
 * TIM编码器计数换算为“菜单一步”的分频：
 * 在TI12模式下，常见机械编码器每卡点约2~4计数。
 */
#define MY_ENCODER_COUNTS_PER_STEP    2
#endif

#ifndef MY_ENCODER_BUTTON_DEBOUNCE_MS
#define MY_ENCODER_BUTTON_DEBOUNCE_MS 10U
#endif

#ifndef MY_ENCODER_MAX_DELTA_PER_SCAN
/* 每次轮询允许的最大计数变化，超出视为毛刺丢弃 */
#define MY_ENCODER_MAX_DELTA_PER_SCAN  8
#endif
/* ================================================================== */

/* 若CubeMX还未生成PB12宏，则使用默认PB12作为按键输入 */
#ifndef Encoder_button_Pin
#define Encoder_button_Pin       GPIO_PIN_12
#define Encoder_button_GPIO_Port GPIOB
#endif

typedef struct
{
    TIM_HandleTypeDef *htim;

    uint16_t last_cnt;
    int16_t  raw_acc;
    int16_t  step_acc;

    uint8_t  btn_stable;       /* 1:按下, 0:松开 */
    uint8_t  btn_raw_last;     /* 上次原始采样 */
    uint8_t  click_flag;       /* 一次性点击事件 */
    uint8_t  btn_idle_level;   /* 上电空闲电平（用于自动识别按键极性） */
    uint32_t btn_change_tick;  /* 原始电平变化时刻 */
} MyEncoderCtx_t;

static MyEncoderCtx_t s_enc = {0};

static uint8_t MY_ENCODER_ReadButtonRawLevel(void)
{
    return (HAL_GPIO_ReadPin(Encoder_button_GPIO_Port, Encoder_button_Pin) == GPIO_PIN_SET) ? 1U : 0U;
}

void MY_ENCODER_Init(TIM_HandleTypeDef *htim)
{
    if (htim == NULL)
    {
        return;
    }

    s_enc.htim            = htim;
    s_enc.raw_acc         = 0;
    s_enc.step_acc        = 0;
    s_enc.click_flag      = 0;
    s_enc.btn_raw_last    = MY_ENCODER_ReadButtonRawLevel();
    s_enc.btn_stable      = s_enc.btn_raw_last;
    s_enc.btn_idle_level  = s_enc.btn_stable; /* 默认上电时未按下 */
    s_enc.btn_change_tick = HAL_GetTick();

    HAL_TIM_Encoder_Start(s_enc.htim, TIM_CHANNEL_ALL);
    __HAL_TIM_SET_COUNTER(s_enc.htim, 0x7FFF);
    s_enc.last_cnt = (uint16_t)__HAL_TIM_GET_COUNTER(s_enc.htim);
}

void MY_ENCODER_Process(void)
{
    if (s_enc.htim == NULL)
    {
        return;
    }

    /* -------- 1) 处理旋转计数 -------- */
    {
        uint16_t now_cnt = (uint16_t)__HAL_TIM_GET_COUNTER(s_enc.htim);
        int16_t delta = (int16_t)(now_cnt - s_enc.last_cnt);
        s_enc.last_cnt = now_cnt;

        if ((delta > MY_ENCODER_MAX_DELTA_PER_SCAN) || (delta < -MY_ENCODER_MAX_DELTA_PER_SCAN))
        {
            delta = 0;
        }

        /*
         * 方向映射：
         * 用户需求：逆时针=菜单上移(-1), 顺时针=菜单下移(+1)
         * 若实物方向相反，可把下面一行改成 s_enc.raw_acc -= delta;
         */
        s_enc.raw_acc += delta;

        while (s_enc.raw_acc >= MY_ENCODER_COUNTS_PER_STEP)
        {
            s_enc.raw_acc -= MY_ENCODER_COUNTS_PER_STEP;
            s_enc.step_acc += 1;   /* 菜单下移 */
        }
        while (s_enc.raw_acc <= -MY_ENCODER_COUNTS_PER_STEP)
        {
            s_enc.raw_acc += MY_ENCODER_COUNTS_PER_STEP;
            s_enc.step_acc -= 1;   /* 菜单上移 */
        }
    }

    /* -------- 2) 处理按键去抖 + 点击 -------- */
    {
        uint8_t raw_now = MY_ENCODER_ReadButtonRawLevel();
        uint32_t now_ms = HAL_GetTick();

        if (raw_now != s_enc.btn_raw_last)
        {
            s_enc.btn_raw_last = raw_now;
            s_enc.btn_change_tick = now_ms;
        }

        if ((now_ms - s_enc.btn_change_tick) >= MY_ENCODER_BUTTON_DEBOUNCE_MS)
        {
            if (s_enc.btn_stable != raw_now)
            {
                uint8_t old_stable = s_enc.btn_stable;
                s_enc.btn_stable = raw_now;

                /* 自动按键极性：从空闲电平变为非空闲电平，视为“按下” */
                if ((old_stable == s_enc.btn_idle_level) && (s_enc.btn_stable != s_enc.btn_idle_level))
                {
                    s_enc.click_flag = 1U;
                }
            }
        }
    }
}

int16_t MY_ENCODER_GetMoveSteps(void)
{
    int16_t ret = s_enc.step_acc;
    s_enc.step_acc = 0;
    return ret;
}

uint8_t MY_ENCODER_GetClick(void)
{
    uint8_t ret = s_enc.click_flag;
    s_enc.click_flag = 0;
    return ret;
}

uint8_t MY_ENCODER_IsPressed(void)
{
    return (s_enc.btn_stable != s_enc.btn_idle_level) ? 1U : 0U;
}
