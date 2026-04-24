/*
 * oscilloscope.h
 *
 *  Created on: Nov 16, 2025
 *      Author: Tejas
 */

#ifndef INC_OSCILLOSCOPE_H_
#define INC_OSCILLOSCOPE_H_
/*
 * oscilloscope.h
 * Professional Rigol DS1054Z-style Oscilloscope
 */


#include "main.h"
#include <stdint.h>

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
#define BUFFER			 14

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
} Measurements_t;

// Channel info
typedef struct {
    uint8_t enabled;
    float scale;       // V/div
    float offset;      // Vertical offset
    uint16_t color;
    char coupling[4];  // "AC" or "DC"
} Channel_t;

// Main oscilloscope structure
typedef struct {
    float samples[SAMPLES];
    uint16_t count;

    Channel_t ch1;
    float timeScale;   // Time/div (ms)
    float trigLevel;   // Trigger level (V)
    uint8_t running;   // 1=RUN, 0=HOLD
    float groundRef;   // Ground reference voltage (typically 1.65V for 3.3V system)

    Measurements_t meas;

    uint8_t triggerMode; // 0=AUTO, 1=NORMAL, 2=SINGLE

    // Cursor system
    uint8_t cursorsEnabled;
    float cursor1_time;  // Time position of cursor 1 (ms from left)
    float cursor2_time;  // Time position of cursor 2 (ms from left)
    float cursor1_volt;  // Voltage position of cursor 1
    float cursor2_volt;  // Voltage position of cursor 2
} OscPro_t;

extern OscPro_t oscPro;

// Functions
void OSCP_Init(void);
void OSCP_DrawUI(void);
void OSCP_DrawGrid(void);
void OSCP_DrawWaveform(void);
void OSCP_DrawLeftPanel(void);
void OSCP_DrawRightPanel(void);
void OSCP_DrawStatusBar(void);
void OSCP_DrawBottomBar(void);
void OSCP_AddSample(float voltage);
void OSCP_Update(void);
void OSCP_CalculateMeasurements(void);
void OSCP_HandleTouch(uint16_t x, uint16_t y);
int16_t OSCP_V2Y(float v);
void OSCP_DrawTriggerIndicator(void);
void OSCP_DrawTriggerLine(void);
void OSCP_DrawCursors(void);
void OSCP_DrawGroundIndicator(void);

#endif /* INC_OSCILLOSCOPE_H_ */
