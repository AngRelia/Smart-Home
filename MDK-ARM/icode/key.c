#include "key.h"

static uint8_t Key_Num = 0;   // 保存最终的按键值

// 【修改点1】：去掉 static，让它变成全局函数
uint8_t Key_GetState(void)
{
    if (HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin) == GPIO_PIN_RESET) return 1;
    if (HAL_GPIO_ReadPin(KEY_2_GPIO_Port, KEY_2_Pin) == GPIO_PIN_RESET) return 2;
    if (HAL_GPIO_ReadPin(KEY_3_GPIO_Port, KEY_3_Pin) == GPIO_PIN_RESET) return 3;
    if (HAL_GPIO_ReadPin(KEY_4_GPIO_Port, KEY_4_Pin) == GPIO_PIN_RESET) return 4;
    return 0;
}

uint8_t Key_GetNum(void)
{
    uint8_t temp = Key_Num;
    Key_Num = 0;
    return temp;
}

// 【修改点2】：删除或注释掉 Key_Tick，逻辑将移到 Task 中
/*
void Key_Tick(void)
{
   ... 旧代码不需要了 ...
}
*/