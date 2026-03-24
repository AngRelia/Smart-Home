#include "dht11.h"
#include "delay.h"
#include "cmsis_os.h"

/* DHT11 建议最小读取间隔 >= 1s */
static uint32_t s_lastReadTick = 0;

/* 默认配置：兼容旧逻辑 */
static DHT11_Config_t s_cfg =
{
    35U,
    2U,
    120U,
    200U,
    20U,
};

static DHT11_Stats_t s_stats = {0};

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

static uint16_t DHT11_ClampU16(uint16_t v, uint16_t minv, uint16_t maxv)
{
    if (v < minv) return minv;
    if (v > maxv) return maxv;
    return v;
}

static uint8_t DHT11_ClampU8(uint8_t v, uint8_t minv, uint8_t maxv)
{
    if (v < minv) return minv;
    if (v > maxv) return maxv;
    return v;
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
    uint16_t high_us = 0;

    /* 等待低电平开始 */
    if (DHT11_WaitLevel(GPIO_PIN_RESET, s_cfg.bit_timeout_us) != HAL_OK)
    {
        return HAL_TIMEOUT;
    }

    /* 等待高电平到来 */
    if (DHT11_WaitLevel(GPIO_PIN_SET, s_cfg.bit_timeout_us) != HAL_OK)
    {
        return HAL_TIMEOUT;
    }

    /* 脉宽判决：测量高电平持续时间
     * DHT11: 0 -> ~26-28us, 1 -> ~70us
     */
    while ((DHT11_ReadPin() == GPIO_PIN_SET) && (high_us < s_cfg.bit_timeout_us))
    {
        delay_us(1);
        high_us++;
    }

    if (high_us >= s_cfg.bit_timeout_us)
    {
        return HAL_TIMEOUT;
    }

    *bit = (high_us >= s_cfg.high_threshold_us) ? 1U : 0U;

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

static HAL_StatusTypeDef DHT11_ReadRawOnce(uint8_t raw[5])
{
    uint8_t i;
    uint32_t primask;

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
    if (DHT11_WaitLevel(GPIO_PIN_RESET, s_cfg.resp_timeout_us) != HAL_OK)
    {
        if (!primask) __enable_irq();
        return HAL_TIMEOUT;
    }
    if (DHT11_WaitLevel(GPIO_PIN_SET, s_cfg.resp_timeout_us) != HAL_OK)
    {
        if (!primask) __enable_irq();
        return HAL_TIMEOUT;
    }
    if (DHT11_WaitLevel(GPIO_PIN_RESET, s_cfg.resp_timeout_us) != HAL_OK)
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
    return HAL_OK;
}

void DHT11_Init(void)
{
    /* 运行期保护配置边界 */
    s_cfg.sample_us       = DHT11_ClampU8(s_cfg.sample_us, 20U, 60U);
    s_cfg.max_retries     = DHT11_ClampU8(s_cfg.max_retries, 0U, 10U);
    s_cfg.bit_timeout_us  = DHT11_ClampU16(s_cfg.bit_timeout_us, 60U, 500U);
    s_cfg.resp_timeout_us = DHT11_ClampU16(s_cfg.resp_timeout_us, 100U, 1000U);
    s_cfg.high_threshold_us = DHT11_ClampU16(s_cfg.high_threshold_us, 8U, 40U);

    DHT11_PinInputPullup();
}

void DHT11_SetConfig(const DHT11_Config_t *cfg)
{
    if (cfg == NULL)
    {
        return;
    }

    s_cfg.sample_us       = DHT11_ClampU8(cfg->sample_us, 20U, 60U);
    s_cfg.max_retries     = DHT11_ClampU8(cfg->max_retries, 0U, 10U);
    s_cfg.bit_timeout_us  = DHT11_ClampU16(cfg->bit_timeout_us, 60U, 500U);
    s_cfg.resp_timeout_us = DHT11_ClampU16(cfg->resp_timeout_us, 100U, 1000U);
    s_cfg.high_threshold_us = DHT11_ClampU16(cfg->high_threshold_us, 8U, 40U);
}

void DHT11_GetConfig(DHT11_Config_t *cfg)
{
    if (cfg == NULL)
    {
        return;
    }
    *cfg = s_cfg;
}

void DHT11_ResetStats(void)
{
    s_stats.total_calls = 0;
    s_stats.ok_count = 0;
    s_stats.busy_count = 0;
    s_stats.timeout_count = 0;
    s_stats.checksum_error_count = 0;
    s_stats.retry_success_count = 0;
}

void DHT11_GetStats(DHT11_Stats_t *stats)
{
    if (stats == NULL)
    {
        return;
    }
    *stats = s_stats;
}

HAL_StatusTypeDef DHT11_Read(DHT11_Data_t *out)
{
    uint8_t raw[5] = {0};
    uint8_t sum;
    uint8_t attempt;
    HAL_StatusTypeDef st;

    s_stats.total_calls++;

    if (out == NULL)
    {
        return HAL_ERROR;
    }

    if ((HAL_GetTick() - s_lastReadTick) < 1000U)
    {
        s_stats.busy_count++;
        return HAL_BUSY;
    }

    for (attempt = 0; attempt <= s_cfg.max_retries; attempt++)
    {
        st = DHT11_ReadRawOnce(raw);
        if (st != HAL_OK)
        {
            if (attempt < s_cfg.max_retries)
            {
                continue;
            }
            s_stats.timeout_count++;
            return HAL_TIMEOUT;
        }

        sum = (uint8_t)(raw[0] + raw[1] + raw[2] + raw[3]);
        if (sum == raw[4])
        {
            out->hum_int  = raw[0];
            out->hum_dec  = raw[1];
            out->temp_int = raw[2];
            out->temp_dec = raw[3];

            s_lastReadTick = HAL_GetTick();
            s_stats.ok_count++;
            if (attempt > 0U)
            {
                s_stats.retry_success_count++;
            }
            return HAL_OK;
        }

        if (attempt < s_cfg.max_retries)
        {
            continue;
        }

        s_stats.checksum_error_count++;
        return HAL_ERROR;
    }

    /* 正常不会走到这里 */
    s_stats.timeout_count++;
    return HAL_TIMEOUT;
}
