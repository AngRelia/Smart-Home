#ifndef __MY_ENCODER_H__
#define __MY_ENCODER_H__

#include "main.h"
#include "tim.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 旋转编码器方向事件（用于OLED菜单）
 * ENCODER_MOVE_UP   : 菜单指针上移（逆时针）
 * ENCODER_MOVE_DOWN : 菜单指针下移（顺时针）
 */
typedef enum
{
    ENCODER_MOVE_NONE = 0,
    ENCODER_MOVE_UP   = -1,
    ENCODER_MOVE_DOWN = 1
} EncoderMove_t;

/*
 * 初始化编码器驱动：
 * 1) 启动TIM编码器接口
 * 2) 初始化计数器/按键去抖状态
 */
void MY_ENCODER_Init(TIM_HandleTypeDef *htim);

/*
 * 周期处理函数（建议在FreeRTOS任务中每1~5ms调用一次）
 */
void MY_ENCODER_Process(void);

/*
 * 获取累计移动步数：
 * >0 代表菜单下移（顺时针）
 * <0 代表菜单上移（逆时针）
 * 0 代表无新移动
 * 读取后自动清零（一次性）
 */
int16_t MY_ENCODER_GetMoveSteps(void);

/*
 * 获取一次点击事件（短按松手后置位）
 * 返回1表示有点击，返回0表示无
 * 读取后自动清零（一次性）
 */
uint8_t MY_ENCODER_GetClick(void);

/* 读取当前按键稳定状态：1=按下, 0=松开 */
uint8_t MY_ENCODER_IsPressed(void);

#ifdef __cplusplus
}
#endif

#endif
