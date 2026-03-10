#include "light_sensor.h"
#include "adc.h"

uint16_t LightSensor_ReadRaw(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    uint16_t value = 0;

    /* 固定使用 ADC1 通道4 (PA4) */
    sConfig.Channel = ADC_CHANNEL_4;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
    {
        value = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);

    return value;
}

uint32_t LightSensor_ReadMv(uint32_t vref_mv)
{
    uint32_t raw = LightSensor_ReadRaw();
    return (raw * vref_mv) / 4095U;
}
