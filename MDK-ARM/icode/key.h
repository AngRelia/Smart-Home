#ifndef __KEY_H
#define __KEY_H

#include "main.h"

// 移除 Key_Tick()，RTOS 不需要它
// void Key_Tick(void); 

uint8_t Key_GetNum(void);     // 获取按键值（用于消费者）
uint8_t Key_GetState(void);   // 【新增】公开底层的引脚状态读取函数

#endif
