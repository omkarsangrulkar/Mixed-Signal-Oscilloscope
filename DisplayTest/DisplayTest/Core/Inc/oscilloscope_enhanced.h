/*
 * oscilloscope_enhanced.h
 *
 *  Created on: Nov 18, 2025
 *      Author: Tejas
 */

#ifndef INC_OSCILLOSCOPE_ENHANCED_H_
#define INC_OSCILLOSCOPE_ENHANCED_H_
/*
 * oscilloscope_enhanced.h
 * Enhanced Professional Oscilloscope with ADC Integration
 * Created on: Nov 18, 2025
 */

#include "main.h"
#include <stdint.h>

/*============================================================================
 * ANALOG FRONT-END CONFIGURATION
 * Edit these values to match your actual analog circuitry measurements
 *============================================================================*/

// ±5V Range Configuration
#define AFE_5V_GROUND_REF_V      1.65f    // Ground reference voltage (0V input -> this output)
#define AFE_5V_POS5V_OUT_V       2.40f    // Output voltage when +5V input
#define AFE_5V_NEG5V_OUT_V       0.90f    // Output voltage when -5V input (if you measure it)

// ±15V Range Configuration
#define AFE_15V_GROUND_REF_V     1.65f    // Ground reference voltage (0V input -> this output)
#define AFE_15V_POS5V_OUT_V      0.60f    // Output voltage when +5V input (600mV as you mentioned)
#define AFE_15V_NEG5V_OUT_V      2.70f    // Output voltage when -5V input (estimated, please measure)

// Calculated calibration factors (auto-computed from above)
// For ±5V range:
// Gain = (Output for +5V - Output for -5V) / 10V
#define AFE_5V_GAIN    ((AFE_5V_POS5V_OUT_V - AFE_5V_GROUND_REF_V) / 5.0f)
#define AFE_5V_OFFSET  AFE_5V_GROUND_REF_V

// For ±15V range:
#define AFE_15V_GAIN   ((AFE_15V_POS5V_OUT_V - AFE_15V_GROUND_REF_V) / 5.0f)
#define AFE_15V_OFFSET AFE_15V_GROUND_REF_V

/*============================================================================
 * ADC CONFIGURATION
 *============================================================================*/
#define ADC_VREF                 3.3f      // ADC reference voltage
#define ADC_RESOLUTION           4096      // 12-bit ADC
#define ADC_CHANNEL              ADC_CHANNEL_0  // ADC channel (PA0 typically)
#define ADC_OVERSAMPLING         4         // Number of samples to average (power of 2)

/*============================================================================
 * SAMPLING CONFIGURATION
 *============================================================================*/
#define MAX_SAMPLE_RATE_KHZ      100       // Maximum sampling rate in kHz
#define MIN_SAMPLE_RATE_KHZ      1         // Minimum sampling rate in kHz
#define DEFAULT_SAMPLE_RATE_KHZ  10        // Default sampling rate

/*============================================================================
 * TRIGGER CONFIGURATION
 *============================================================================*/
#define TRIGGER_PRETRIGGER_PCT   20        // Percentage of samples before trigger
#define TRIGGER_HYSTERESIS_MV    50        // Trigger hysteresis in mV

/*============================================================================
 * DISPLAY CONFIGURATION (from original)
 *============================================================================*/
// Colors (Rigol-style)
#define OSC_BG           0x0841      // Dark blue background
#define OSC_GRID         0x4208      // Dim gray grid
#define OSC_GRID_BRIGHT  0x7BEF      // Brighter center lines
#define OSC_CH1_COLOR    0xFFE0      // Yellow for Channel 1
#define OSC_CH2_COLOR    0x07FF      // Cyan for Channel 2
#define OSC_TRIGGER_LINE 0xF81F      // Magenta trigger line
#define OSC_TEXT_ORANGE  0xFD20      // Orange text (left panel)
#define OSC_TEXT_RED     0xF800      // Red text (right panel)
#define OSC_TEXT_WHITE   0xFFFF      // White text
#define OSC_TEXT_GREEN   0x07E0      // Green for RUN
#define OSC_TEXT_CYAN    0x07FF      // Cyan for AUTO
#define OSC_PANEL_BG     0x0020      // Transparent/same as BG
#define OSC_MEAS_BG      0x0020      // Semi-transparent for measurements

// Layout - Full screen width with measurements overlaid
#define STATUS_BAR_H     22
#define BOTTOM_BAR_H     18
#define GRID_X           5
#define GRID_Y           STATUS_BAR_H + 2
#define GRID_W           470
#define GRID_H           270
#define SAMPLES          470
#define BUFFER           14

/*============================================================================
 * DATA STRUCTURES
 *============================================================================*/

// Measurements
typedef struct {
    float freq;        // Frequency (Hz)
    float period;      // Period (ms)
    float pulseWidth;  // Pulse width (ms)
    float duty;        // Duty cycle %
    float vmax;        // Max voltage
    float vmin;        // Min voltage
    float vavg;        // Average voltage
    float vpp;         // Peak-to-peak
    float vrms;        // RMS voltage
    float riseTime;    // Rise time (10-90%)
    float fallTime;    // Fall time (90-10%)
} Measurements_t;

// Channel info
typedef struct {
    uint8_t enabled;
    float scale;       // V/div
    float offset;      // Vertical offset
    uint16_t color;
    char coupling[4];  // "AC" or "DC"
    uint8_t probe;     // Probe attenuation (1x, 10x, 100x)
} Channel_t;

// ADC Buffer structure
typedef struct {
    uint16_t raw[SAMPLES];           // Raw ADC values
    float voltage[SAMPLES];          // Converted voltages
    uint16_t count;                  // Current sample count
    uint8_t ready;                   // Buffer ready flag
    uint32_t timestamp_start;        // Capture start time
    uint32_t timestamp_end;          // Capture end time
} ADC_Buffer_t;

// Trigger structure
typedef struct {
    uint8_t mode;          // 0=AUTO, 1=NORMAL, 2=SINGLE
    uint8_t edge;          // 0=RISING, 1=FALLING, 2=BOTH
    float level;           // Trigger level (V)
    uint8_t source;        // 0=CH1, 1=CH2, 2=EXT
    uint8_t armed;         // Trigger armed status
    uint16_t position;     // Trigger position in buffer
} Trigger_t;

// Main oscilloscope structure
typedef struct {
    // Sample data
    float samples[SAMPLES];
    uint16_t count;

    // Channel configuration
    Channel_t ch1;

    // Timebase
    float timeScale;       // Time/div (ms)
    float sampleRate;      // Actual sample rate (kHz)

    // Trigger
    Trigger_t trigger;
    float trigLevel;       // Compatibility with old code
    uint8_t triggerMode;   // Compatibility with old code

    // System state
    uint8_t running;       // 1=RUN, 0=HOLD
    uint8_t inputRange;    // 0=±5V, 1=±15V
    float groundRef;       // Ground reference voltage

    // Measurements
    Measurements_t meas;

    // Cursors
    uint8_t cursorsEnabled;
    uint8_t cursorMode;    // 0=voltage, 1=time
    float cursor1_time;
    float cursor2_time;
    float cursor1_volt;
    float cursor2_volt;

    // ADC Buffer
    ADC_Buffer_t adc_buffer;

} OscPro_t;

extern OscPro_t oscPro;

/*============================================================================
 * ADC FUNCTIONS
 *============================================================================*/

/**
 * @brief Initialize ADC peripheral in bare-metal mode
 * @param none
 * @return none
 */
void OSC_ADC_Init(void);

/**
 * @brief Start ADC continuous sampling with DMA
 * @param sample_rate_khz: Desired sample rate in kHz
 * @return none
 */
void OSC_ADC_StartSampling(uint16_t sample_rate_khz);

/**
 * @brief Stop ADC sampling
 * @param none
 * @return none
 */
void OSC_ADC_StopSampling(void);

/**
 * @brief Read single ADC sample (blocking)
 * @param none
 * @return Voltage reading in volts
 */
float OSC_ADC_ReadSingle(void);

/**
 * @brief Convert raw ADC value to voltage
 * @param raw_value: 12-bit ADC reading
 * @return Voltage in volts
 */
float OSC_ADC_RawToVoltage(uint16_t raw_value);

/**
 * @brief Map ADC voltage to actual input voltage based on range
 * @param adc_voltage: ADC reading in volts (0-3.3V)
 * @return Actual input voltage considering AFE scaling
 */
float OSC_MapVoltageFromADC(float adc_voltage);

/**
 * @brief DMA transfer complete callback
 * @param none
 * @return none
 */
void OSC_ADC_DMA_Complete_Callback(void);

/*============================================================================
 * CORE OSCILLOSCOPE FUNCTIONS
 *============================================================================*/

void OSCP_Init(void);
void OSCP_DrawUI(void);
void OSCP_DrawGrid(void);
void OSCP_DrawWaveform(void);
void OSCP_DrawLeftPanel(void);
void OSCP_DrawRightPanel(void);
void OSCP_DrawStatusBar(void);
void OSCP_DrawBottomBar(void);
void OSCP_Update(void);
void OSCP_HandleTouch(uint16_t x, uint16_t y);

/*============================================================================
 * MEASUREMENT FUNCTIONS
 *============================================================================*/

void OSCP_CalculateMeasurements(void);
void OSCP_MeasureRiseTime(void);
void OSCP_MeasureFallTime(void);

/*============================================================================
 * TRIGGER FUNCTIONS
 *============================================================================*/

/**
 * @brief Check if trigger condition is met
 * @param samples: Array of voltage samples
 * @param count: Number of samples
 * @return Trigger position (sample index) or -1 if no trigger
 */
int16_t OSCP_FindTrigger(float *samples, uint16_t count);

/**
 * @brief Arm the trigger system
 * @param none
 * @return none
 */
void OSCP_ArmTrigger(void);

/*============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

int16_t OSCP_V2Y(float v);
void OSCP_DrawTriggerIndicator(void);
void OSCP_DrawTriggerLine(void);
void OSCP_DrawCursors(void);
void OSCP_DrawGroundIndicator(void);
void OSCP_DrawHorizontalCursors(void);
void OSCP_DrawVerticalCursors(void);
void OSCP_ClearHorizontalCursors(void);
void OSCP_ClearVerticalCursors(void);
void OSCP_ClearTriggerArrow(void);
void OSCP_ClearTriggerLine(void);

// NEW: Get/Set functions
uint8_t OSCP_GetInputRange(void);
void OSCP_SetInputRange(uint8_t range);
uint8_t OSCP_GetCursorMode(void);
void OSCP_SetSampleRate(uint16_t rate_khz);
uint16_t OSCP_GetSampleRate(void);

/*============================================================================
 * AUTO-SETUP FUNCTIONS
 *============================================================================*/

/**
 * @brief Automatically configure vertical scale based on signal amplitude
 * @param none
 * @return none
 */
void OSCP_AutoScale(void);

/**
 * @brief Automatically configure timebase based on signal frequency
 * @param none
 * @return none
 */
void OSCP_AutoTimebase(void);

/**
 * @brief Automatically set trigger level to 50% of signal
 * @param none
 * @return none
 */
void OSCP_AutoTrigger(void);

/**
 * @brief Run all auto-setup functions
 * @param none
 * @return none
 */
void OSCP_AutoSetup(void);



#endif /* INC_OSCILLOSCOPE_ENHANCED_H_ */
