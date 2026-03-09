#include "dht11.h"
#include "delay.h"
#include "cmsis_os.h"

/* DHT11 建议最小读取间隔 >= 1s */
static uint32_t s_lastReadTick = 0;

static void DHT11_PinInputPullup(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DHT11_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT11_GPIO_Port, &GPIO_InitStruct);
}

static void DHT11_PinOutputOD(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DHT11_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_GPIO_Port, &GPIO_InitStruct);
}

static uint8_t DHT11_ReadPin(void)
{
    return (uint8_t)HAL_GPIO_ReadPin(DHT11_GPIO_Port, DHT11_Pin);
}

/* 等待引脚电平变为 level，超时返回 HAL_TIMEOUT */
static HAL_StatusTypeDef DHT11_WaitLevel(uint8_t level, uint32_t timeout_us)
{
    while (timeout_us--)
    {
        if (DHT11_ReadPin() == level)
        {
            return HAL_OK;
        }
        delay_us(1);
    }
    return HAL_TIMEOUT;
}

/* 读1bit：
 * 低电平 50us 后进入高电平
 * 高电平约 26~28us => 0
 * 高电平约 70us    => 1
 */
static HAL_StatusTypeDef DHT11_ReadBit(uint8_t *bit)
{
    /* 等待低电平开始 */
    if (DHT11_WaitLevel(GPIO_PIN_RESET, 120) != HAL_OK)
    {
        return HAL_TIMEOUT;
    }

    /* 等待高电平到来 */
    if (DHT11_WaitLevel(GPIO_PIN_SET, 120) != HAL_OK)
    {
        return HAL_TIMEOUT;
    }

    /* 延时 35us 采样：
     * 若仍为高电平 -> 1
     * 若已拉低     -> 0
     */
    delay_us(35);
    *bit = (DHT11_ReadPin() == GPIO_PIN_SET) ? 1U : 0U;

    /* 等待本bit结束（回到低电平） */
    if (DHT11_WaitLevel(GPIO_PIN_RESET, 120) != HAL_OK)
    {
        return HAL_TIMEOUT;
    }

    return HAL_OK;
}

static HAL_StatusTypeDef DHT11_ReadByte(uint8_t *value)
{
    uint8_t i;
    uint8_t bit;
    uint8_t data = 0;

    for (i = 0; i < 8; i++)
    {
        if (DHT11_ReadBit(&bit) != HAL_OK)
        {
            return HAL_TIMEOUT;
        }
        data <<= 1;
        data |= bit;
    }

    *value = data;
    return HAL_OK;
}

void DHT11_Init(void)
{
    DHT11_PinInputPullup();
}

HAL_StatusTypeDef DHT11_Read(DHT11_Data_t *out)
{
    uint8_t raw[5] = {0};
    uint8_t i;
    uint8_t sum;
    uint32_t primask;

    if (out == NULL)
    {
        return HAL_ERROR;
    }

    if ((HAL_GetTick() - s_lastReadTick) < 1000U)
    {
        return HAL_BUSY;
    }

    /* 主机起始信号：拉低 >=18ms，随后拉高并切输入等待响应 */
    DHT11_PinOutputOD();
    HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_RESET);
    if (osKernelGetState() == osKernelRunning)
    {
        osDelay(20);
    }
    else
    {
        HAL_Delay(20);
    }

    /* 关键时序阶段禁止中断，避免RTOS任务切换破坏微秒级采样 */
    primask = __get_PRIMASK();
    __disable_irq();

    HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_SET);
    delay_us(30);

    DHT11_PinInputPullup();

    /* 传感器响应：80us低 + 80us高 */
    if (DHT11_WaitLevel(GPIO_PIN_RESET, 200) != HAL_OK)
    {
        if (!primask) __enable_irq();
        return HAL_TIMEOUT;
    }
    if (DHT11_WaitLevel(GPIO_PIN_SET,   200) != HAL_OK)
    {
        if (!primask) __enable_irq();
        return HAL_TIMEOUT;
    }
    if (DHT11_WaitLevel(GPIO_PIN_RESET, 200) != HAL_OK)
    {
        if (!primask) __enable_irq();
        return HAL_TIMEOUT;
    }

    /* 读取 40bit */
    for (i = 0; i < 5; i++)
    {
        if (DHT11_ReadByte(&raw[i]) != HAL_OK)
        {
            if (!primask) __enable_irq();
            return HAL_TIMEOUT;
        }
    }

    if (!primask) __enable_irq();

    sum = (uint8_t)(raw[0] + raw[1] + raw[2] + raw[3]);
    if (sum != raw[4])
    {
        return HAL_ERROR;
    }

    out->hum_int  = raw[0];
    out->hum_dec  = raw[1];
    out->temp_int = raw[2];
    out->temp_dec = raw[3];

    s_lastReadTick = HAL_GetTick();
    return HAL_OK;
}
