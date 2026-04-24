/*
 * oscilloscope_adc.c
 * HAL-based ADC + DMA + TIM2 trigger
 * Fully compatible with STM32F411E-DISCO + ILI9488 oscilloscope project
 */

#include "oscilloscope_enhanced.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* External handles (from main.c) */
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim2;

extern void OSCP_SetSampleRate(uint16_t rate_khz);

/*============================================================================
 * PRIVATE VARIABLES
 *===========================================================================*/

static volatile uint16_t adc_dma_buffer[SAMPLES * ADC_OVERSAMPLING];

/*============================================================================
 * TIMER (TIM2 → TRGO) FOR ADC SAMPLING
 *===========================================================================*/

static void OSC_TIM2_SetSampleRate(uint16_t sample_rate_khz)
{
    uint32_t timer_clk = 84000000 * 2;     // APB1 timer clock = 168 MHz
    uint32_t target_hz = sample_rate_khz * 1000;

    uint32_t arr = (timer_clk / target_hz) - 1;
    uint16_t psc = 0;

    while (arr > 65535)
    {
        psc++;
        arr = (timer_clk / (target_hz * (psc + 1))) - 1;
    }

    __HAL_TIM_DISABLE(&htim2);
    htim2.Instance->PSC = psc;
    htim2.Instance->ARR = arr;
    __HAL_TIM_ENABLE(&htim2);
}

/*============================================================================
 * ADC + DMA INITIALIZATION
 *===========================================================================*/

void OSC_ADC_Init(void)
{
    /* HAL ADC is already initialized by MX_ADC1_Init() */
    /* HAL TIM2 is already initialized by MX_TIM2_Init() */

    /* DMA needs its buffer linked AFTER MX_DMA_Init() */
    HAL_ADC_Stop_DMA(&hadc1);

    HAL_ADC_Start_DMA(
        &hadc1,
        (uint32_t *)oscPro.adc_buffer.raw,
        SAMPLES
    );
}

/*============================================================================
 * START SAMPLING
 *===========================================================================*/

void OSC_ADC_StartSampling(uint16_t sample_rate_khz)
{
    if (sample_rate_khz < MIN_SAMPLE_RATE_KHZ)
        sample_rate_khz = MIN_SAMPLE_RATE_KHZ;
    if (sample_rate_khz > MAX_SAMPLE_RATE_KHZ)
        sample_rate_khz = MAX_SAMPLE_RATE_KHZ;

    oscPro.adc_buffer.ready = 0;
    oscPro.adc_buffer.count = 0;

//    current_sample_rate_khz = sample_rate_khz;
    OSCP_SetSampleRate(sample_rate_khz);
    /* Update TIM2 frequency */
    OSC_TIM2_SetSampleRate(sample_rate_khz);

    /* Restart DMA & ADC */
    HAL_ADC_Stop_DMA(&hadc1);

    HAL_ADC_Start_DMA(
        &hadc1,
        (uint32_t *)oscPro.adc_buffer.raw,
        SAMPLES
    );

    /* Start TIM2 → generates TRGO pulses */
    HAL_TIM_Base_Start(&htim2);
}

/*============================================================================
 * STOP SAMPLING
 *===========================================================================*/

void OSC_ADC_StopSampling(void)
{
    HAL_TIM_Base_Stop(&htim2);
    HAL_ADC_Stop_DMA(&hadc1);
}

/*============================================================================
 * SINGLE CONVERSION
 *===========================================================================*/

float OSC_ADC_ReadSingle(void)
{
    HAL_ADC_Stop_DMA(&hadc1);

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);

    uint16_t raw = HAL_ADC_GetValue(&hadc1);
    float voltage = OSC_ADC_RawToVoltage(raw);

    /* Restart DMA */
    HAL_ADC_Start_DMA(
        &hadc1,
        (uint32_t *)oscPro.adc_buffer.raw,
        SAMPLES
    );

    return voltage;
}

/*============================================================================
 * RAW → VOLTAGE
 *===========================================================================*/

float OSC_ADC_RawToVoltage(uint16_t raw_value)
{
    return ((float)raw_value * ADC_VREF) / (float)ADC_RESOLUTION;
}

float OSC_MapVoltageFromADC(float adc_voltage)
{
    if (oscPro.inputRange == 0)  // ±5V
    {
        return (adc_voltage - AFE_5V_OFFSET) / AFE_5V_GAIN;
    }
    else                         // ±15V
    {
        return (adc_voltage - AFE_15V_OFFSET) / AFE_15V_GAIN;
    }
}

/*============================================================================
 * HAL DMA COMPLETE CALLBACK (FINAL)
 *===========================================================================*/
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1) return;

    // Mark how many samples
    oscPro.adc_buffer.count = SAMPLES;
    oscPro.adc_buffer.ready = 1;

    // Convert raw → voltage, but do NOT touch oscPro.count here
    for (int i = 0; i < SAMPLES; i++) {
        float adc_v = OSC_ADC_RawToVoltage(oscPro.adc_buffer.raw[i]);
        float vin   = OSC_MapVoltageFromADC(adc_v);

        oscPro.adc_buffer.voltage[i] = vin;
    }
}


