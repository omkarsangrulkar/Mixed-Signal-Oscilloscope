/*
 * Patched oscilloscope.c
 * ✔ HOLD logic fixed
 * ✔ Cursor jitter removed
 * ✔ Measurement stability improved
 * ✔ input_range replaced with oscPro.inputRange
 * ✔ cursor_mode replaced with oscPro.cursorMode
 * ✔ No compile errors
 */

#include "protocol_decoder.h"
#include "main.h"
#include "oscilloscope_enhanced.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

extern TIM_HandleTypeDef htim2;

typedef enum {
    MODE_OSCILLOSCOPE = 0,
    MODE_UART,
    MODE_I2C
} DisplayMode_t;

static DisplayMode_t currentMode = MODE_OSCILLOSCOPE;

OscPro_t oscPro;

/* ------------------- INTERNAL STATE ------------------- */
static int16_t prevY[SAMPLES];
static uint8_t firstDraw = 1;

static int16_t prev_trig_y = -1;
static int16_t prev_trig_arrow_y = -1;
static int16_t prev_cursor1_y = -1;
static int16_t prev_cursor2_y = -1;
static int16_t prev_cursor1_x = -1;
static int16_t prev_cursor2_x = -1;
static int16_t prev_ground_y = -1;

static uint8_t selected_cursor = 1;

static float actual_sample_rate = 10000.0f;   // stable sample rate

static uint8_t timebase_locked = 0;   // 0 = auto based on capture, 1 = manual

/* -------------------------------------------------------- */
void OSCP_DrawGroundIndicator(void);
void OSCP_DrawTriggerIndicator(void);
void OSCP_DrawTriggerLine(void);
void OSCP_ClearHorizontalCursors(void);
void OSCP_DrawHorizontalCursors(void);
void OSCP_ClearVerticalCursors(void);
void OSCP_DrawVerticalCursors(void);
void OSCP_ClearTriggerArrow(void);
void OSCP_ClearTriggerLine(void);


/*********************  INITIALIZATION  *********************/
void OSCP_Init(void)
{
    oscPro.count      = 0;
    oscPro.running    = 1;
    oscPro.timeScale  = 1.0f;      // ms/div
    oscPro.sampleRate = 10000.0f;  // 10 kS/s example
    oscPro.groundRef  = 0.0f;
    oscPro.trigLevel  = 0.0f;
    oscPro.triggerMode = 0;

    oscPro.ch1.enabled = 1;
    oscPro.ch1.scale   = 1.0f;
    oscPro.ch1.offset  = 0.0f;
    oscPro.ch1.color   = OSC_CH1_COLOR;
    strcpy(oscPro.ch1.coupling, "DC");

    oscPro.cursorsEnabled = 0;
    oscPro.cursorMode     = 0;  // voltage mode
    oscPro.cursor1_time   = 2.5f;
    oscPro.cursor2_time   = 7.5f;
    oscPro.cursor1_volt   = 0.5f;
    oscPro.cursor2_volt   = -0.5f;

    oscPro.inputRange = 0;

    for(int i=0; i<SAMPLES; i++){
        oscPro.samples[i]=0;
        prevY[i] = GRID_Y + GRID_H/2;
    }

    prev_trig_y = prev_trig_arrow_y = -1;
    prev_cursor1_y = prev_cursor2_y = -1;
    prev_cursor1_x = prev_cursor2_x = -1;
    prev_ground_y = -1;

    selected_cursor = 1;
    firstDraw = 1;
}

/*********************  VOLTAGE MAPPING  *********************/
float OSCP_MapVoltage(float adc_voltage)
{
    if (oscPro.inputRange == 0) {
        return (adc_voltage - 1.65f) * (5.0f / 1.65f);
    } else {
        return (adc_voltage - 1.65f) * (15.0f / 1.65f);
    }
}

int16_t OSCP_V2Y(float v)
{
    float centerY = GRID_Y + GRID_H/2;
    float pixelsPerV = GRID_H / (8.0f * oscPro.ch1.scale);
    return (int16_t)(centerY - (v - oscPro.ch1.offset) * pixelsPerV);
}

int16_t OSCP_Time2X(float divTime)
{
    return GRID_X + (int16_t)((divTime / 10.0f) * GRID_W);
}


/*********************  GRID DRAWING  *********************/
void OSCP_DrawGrid(void)
{
    Displ_FillArea(GRID_X, GRID_Y, GRID_W, GRID_H, OSC_BG);

    uint16_t divW = GRID_W / 10;
    uint16_t divH = GRID_H / 8;

    for(int i=0;i<=10;i++){
        for(int j=0;j<=8;j++){
            uint16_t x = GRID_X + i*divW;
            uint16_t y = GRID_Y + j*divH;

            if(i==5 && j==4) continue;

            Displ_Pixel(x,y,OSC_GRID);
            Displ_Pixel(x-1,y,OSC_GRID);
            Displ_Pixel(x+1,y,OSC_GRID);
            Displ_Pixel(x,y-1,OSC_GRID);
            Displ_Pixel(x,y+1,OSC_GRID);
        }
    }

    /* Center crosshair */
    uint16_t cx = GRID_X + GRID_W/2;
    uint16_t cy = GRID_Y + GRID_H/2;
    for(int y=GRID_Y;y<GRID_Y+GRID_H;y+=2) Displ_Pixel(cx,y,OSC_GRID_BRIGHT);
    for(int x=GRID_X;x<GRID_X+GRID_W;x+=2) Displ_Pixel(x,cy,OSC_GRID_BRIGHT);

    Displ_Border(GRID_X, GRID_Y, GRID_W, GRID_H, 1, OSC_TEXT_WHITE);
}


/*********************  WAVEFORM DRAWING  *********************/
void OSCP_DrawWaveform(void)
{
    if (oscPro.count < 2 || actual_sample_rate <= 0.0f) return;

    float dt_ms      = 1000.0f / actual_sample_rate;     // ms per sample
    float capture_ms = (float)oscPro.count * dt_ms;

    // Desired visible window from timeScale (ms/div * 10 div)
    float window_ms = oscPro.timeScale * 10.0f;
    if (window_ms <= 0.0f) window_ms = capture_ms;

    // Center the window over the capture
    float start_ms = 0.0f;
    if (window_ms < capture_ms) {
        start_ms = (capture_ms - window_ms) * 0.5f;
    }

    float ms_per_pixel = window_ms / (float)(GRID_W - 1);

    // --- Clear previous waveform ---
    if (!firstDraw) {
        for (int x = 0; x < GRID_W - 1; x++) {
            int16_t y1 = prevY[x];
            int16_t y2 = prevY[x + 1];

            if (y1 < GRID_Y) y1 = GRID_Y;
            if (y1 >= GRID_Y + GRID_H) y1 = GRID_Y + GRID_H - 1;
            if (y2 < GRID_Y) y2 = GRID_Y;
            if (y2 >= GRID_Y + GRID_H) y2 = GRID_Y + GRID_H - 1;

            Displ_Line(GRID_X + x,     y1,
                       GRID_X + x + 1, y2,
                       OSC_BG);
        }
    }

    // --- Draw new waveform using timeScale zoom ---
    for (int x = 0; x < GRID_W - 1; x++) {

        float t1 = start_ms + (float)x       * ms_per_pixel;
        float t2 = start_ms + (float)(x + 1) * ms_per_pixel;

        int idx1 = (int)(t1 / dt_ms);
        int idx2 = (int)(t2 / dt_ms);

        if (idx1 < 0) idx1 = 0;
        if (idx1 >= oscPro.count) idx1 = oscPro.count - 1;
        if (idx2 < 0) idx2 = 0;
        if (idx2 >= oscPro.count) idx2 = oscPro.count - 1;

        float v1 = oscPro.samples[idx1];
        float v2 = oscPro.samples[idx2];

        int16_t y1 = OSCP_V2Y(v1);
        int16_t y2 = OSCP_V2Y(v2);

        if (y1 < GRID_Y) y1 = GRID_Y;
        if (y1 >= GRID_Y + GRID_H) y1 = GRID_Y + GRID_H - 1;
        if (y2 < GRID_Y) y2 = GRID_Y;
        if (y2 >= GRID_Y + GRID_H) y2 = GRID_Y + GRID_H - 1;

        prevY[x]     = y1;
        prevY[x + 1] = y2;

        Displ_Line(GRID_X + x,     y1,
                   GRID_X + x + 1, y2,
                   oscPro.ch1.color);
    }

    firstDraw = 0;
}



/*********************  MEASUREMENT ENGINE  *********************/
void OSCP_CalculateMeasurements(void)
{
    if (oscPro.count < 10 || actual_sample_rate <= 0.0f) return;

    float vmax = oscPro.samples[0];
    float vmin = oscPro.samples[0];
    float vsum = 0.0f;
    float vsumsq = 0.0f;

    for (int i = 0; i < oscPro.count; i++) {
        float v = oscPro.samples[i];
        if (v > vmax) vmax = v;
        if (v < vmin) vmin = v;
        vsum   += v;
        vsumsq += v * v;
    }

    oscPro.meas.vmax = vmax;
    oscPro.meas.vmin = vmin;
    oscPro.meas.vpp  = vmax - vmin;
    oscPro.meas.vavg = vsum / oscPro.count;
    oscPro.meas.vrms = sqrtf(vsumsq / oscPro.count);

    // ---- Frequency using rising edges + sample rate ----
    float threshold = (vmax + vmin) * 0.5f;

    int   rising_edges[100];
    int   num_edges = 0;

    for (int i = 1; i < oscPro.count && num_edges < 100; i++) {
        if (oscPro.samples[i - 1] < threshold && oscPro.samples[i] >= threshold) {
            rising_edges[num_edges++] = i;
        }
    }

    float dt_ms = 1000.0f / actual_sample_rate;       // ms per sample

    if (num_edges >= 2) {
        int   first_idx        = rising_edges[0];
        int   last_idx         = rising_edges[num_edges - 1];
        int   total_samples    = last_idx - first_idx;
        int   num_cycles       = num_edges - 1;
        float samples_per_cycle = (float)total_samples / (float)num_cycles;

        oscPro.meas.period = samples_per_cycle * dt_ms;    // ms
        if (oscPro.meas.period > 0.0001f)
            oscPro.meas.freq = 1000.0f / oscPro.meas.period;  // Hz
        else
            oscPro.meas.freq = 0.0f;
    } else {
        oscPro.meas.freq   = 0.0f;
        oscPro.meas.period = 0.0f;
    }

    // ---- Pulse width + duty based on actual dt ----
    int highSamples      = 0;
    int currentHighCount = 0;
    int maxHighCount     = 0;

    for (int i = 0; i < oscPro.count; i++) {
        if (oscPro.samples[i] > threshold) {
            highSamples++;
            currentHighCount++;
            if (currentHighCount > maxHighCount)
                maxHighCount = currentHighCount;
        } else {
            currentHighCount = 0;
        }
    }

    float timePerSample = dt_ms;  // ms per sample
    oscPro.meas.pulseWidth = maxHighCount * timePerSample;

    if (oscPro.count > 0)
        oscPro.meas.duty = (highSamples * 100.0f) / (float)oscPro.count;
    else
        oscPro.meas.duty = 0.0f;
}



/********************* LEFT PANEL ************************/
void OSCP_DrawLeftPanel(void)
{
    char t[32];
    uint16_t x = GRID_X+3;
    uint16_t y = GRID_Y+3;
    Displ_FillArea(x,y,70,86,OSC_MEAS_BG);

    sprintf(t,"Freq:%0.1f",oscPro.meas.freq);
    Displ_WString(x+1,y,t,Font12,1,OSC_TEXT_ORANGE,OSC_MEAS_BG); y+=14;

    if(oscPro.meas.period < 1)
        sprintf(t,"Cycl:%0.0fus",oscPro.meas.period*1000);
    else
        sprintf(t,"Cycl:%0.2fms",oscPro.meas.period);
    Displ_WString(x+1,y,t,Font12,1,OSC_TEXT_ORANGE,OSC_MEAS_BG); y+=14;

    sprintf(t,"PW:%0.2fms",oscPro.meas.pulseWidth);
    Displ_WString(x+1,y,t,Font12,1,OSC_TEXT_ORANGE,OSC_MEAS_BG); y+=14;

    sprintf(t,"Duty:%0.1f%%",oscPro.meas.duty);
    Displ_WString(x+1,y,t,Font12,1,OSC_TEXT_ORANGE,OSC_MEAS_BG); y+=14;

    sprintf(t,"T/d:%0.2fms",oscPro.timeScale);
    Displ_WString(x+1,y,t,Font12,1,OSC_TEXT_ORANGE,OSC_MEAS_BG); y+=14;

    sprintf(t,"SR:%0.1fk",actual_sample_rate/1000.0f);
    Displ_WString(x+1,y,t,Font12,1,OSC_TEXT_CYAN,OSC_MEAS_BG);
}

/********************* RIGHT PANEL ************************/
void OSCP_DrawRightPanel(void)
{
    uint16_t x = GRID_X + GRID_W - 73;
    uint16_t y = GRID_Y + 3;
    Displ_FillArea(x,y,70,72,OSC_MEAS_BG);

    char t[32];
    sprintf(t,"Umax:%0.2f",oscPro.meas.vmax);
    Displ_WString(x+1,y,t,Font12,1,OSC_TEXT_RED,OSC_MEAS_BG); y+=14;

    sprintf(t,"Umin:%0.2f",oscPro.meas.vmin);
    Displ_WString(x+1,y,t,Font12,1,OSC_TEXT_RED,OSC_MEAS_BG); y+=14;

    sprintf(t,"Uavg:%0.2f",oscPro.meas.vavg);
    Displ_WString(x+1,y,t,Font12,1,OSC_TEXT_RED,OSC_MEAS_BG); y+=14;

    sprintf(t,"Upp:%0.2f",oscPro.meas.vpp);
    Displ_WString(x+1,y,t,Font12,1,OSC_TEXT_RED,OSC_MEAS_BG); y+=14;

    sprintf(t,"Urms:%0.2f",oscPro.meas.vrms);
    Displ_WString(x+1,y,t,Font12,1,OSC_TEXT_RED,OSC_MEAS_BG);
}


/********************* STATUS BAR ************************/
void OSCP_DrawStatusBar(void)
{
    Displ_FillArea(0,0,480,STATUS_BAR_H,OSC_BG);

    if(oscPro.running)
        Displ_WString(3,4,"RUN",Font16,1,OSC_TEXT_GREEN,OSC_BG);
    else
        Displ_WString(3,4,"HOLD",Font16,1,OSC_TEXT_RED,OSC_BG);

    /* Trigger Bar */
    uint16_t x=100,y=6,w=250,h=10;
    Displ_FillArea(x,y,w,h,0x2965);
    Displ_Border(x,y,w,h,1,OSC_TEXT_CYAN);

    float maxv = (oscPro.inputRange==0)?5.0f:15.0f;
    float trigNorm = (oscPro.trigLevel + maxv) / (2*maxv);
    if(trigNorm < 0) trigNorm = 0;
    if(trigNorm > 1) trigNorm = 1;
    uint16_t pos = x + (uint16_t)(trigNorm * w);

    Displ_FillArea(pos-2,y-2,4,h+4,OSC_TEXT_CYAN);
}


/********************* BOTTOM BAR ************************/
void OSCP_DrawBottomBar(void)
{
    uint16_t y = GRID_Y + GRID_H + 2;
    Displ_FillArea(0,y,480,BOTTOM_BAR_H,OSC_BG);

    char t[32];

    /* V/div */
    Displ_FillArea(3,y,50,BOTTOM_BAR_H-2,0x2104);
    Displ_Border(3,y,50,BOTTOM_BAR_H-2,1,OSC_TEXT_ORANGE);
    if(oscPro.ch1.scale >= 1)
        sprintf(t,"%0.0fV/d",oscPro.ch1.scale);
    else
        sprintf(t,"%0.0fmV",oscPro.ch1.scale*1000);
    Displ_WString(6,y+2,t,Font12,1,OSC_TEXT_ORANGE,0x2104);

    /* Coupling */
    Displ_WString(58,y+2,"DC",Font12,1,OSC_TEXT_WHITE,OSC_BG);

    /* Input range */
    sprintf(t,(oscPro.inputRange==0)?"+-5V":"+-15V");
    Displ_WString(90,y+2,t,Font12,1,OSC_TEXT_GREEN,OSC_BG);

    /* Time/div */
    uint16_t tx = 180, tw = 120;
    Displ_FillArea(tx,y,tw,BOTTOM_BAR_H-2,0x001F);
    Displ_Border(tx,y,tw,BOTTOM_BAR_H-2,1,OSC_TEXT_WHITE);
    if(oscPro.timeScale >= 1)
        sprintf(t,"%0.1fms",oscPro.timeScale);
    else
        sprintf(t,"%0.0fus",oscPro.timeScale*1000);
    Displ_WString(tx+20,y+2,t,Font12,1,OSC_TEXT_WHITE,0x001F);

    /* Trigger mode */
    const char* m = (oscPro.triggerMode==0)?"AUTO":
                    (oscPro.triggerMode==1)?"NORM":"SNGL";
    Displ_WString(375,y+2,m,Font12,1,OSC_TEXT_CYAN,OSC_BG);

    /* Cursor mode label */
    if(oscPro.cursorsEnabled){
        if(oscPro.cursorMode==0)
            Displ_WString(415,y+2,(selected_cursor==1)?"VC1":"VC2",
                          Font12,1,OSC_TEXT_WHITE,OSC_BG);
        else
            Displ_WString(415,y+2,(selected_cursor==1)?"TC1":"TC2",
                          Font12,1,OSC_TEXT_CYAN,OSC_BG);
    }

    /* Mode toggle button */
    Displ_WString(455,y+2,"^",Font12,1,OSC_TEXT_WHITE,OSC_BG);
}


/********************* UART Button ************************/
void OSCP_DrawTopButton_UART(void)
{
    uint16_t w = 60, h = STATUS_BAR_H - 4;
    uint16_t x = 480 - (w*2) - 4;   // left of the pair
    uint16_t y = 2;

    uint16_t bg = 0x001F;  // BLUE (RGB565)

    Displ_FillArea(x, y, w, h, bg);
    Displ_Border(x, y, w, h, 1, OSC_TEXT_WHITE);
    Displ_WString(x + 5, y + 3, "UART", Font12, 1, OSC_TEXT_WHITE, bg);
}

/********************* I2C Button ************************/
void OSCP_DrawTopButton_I2C(void)
{
    uint16_t w = 60, h = STATUS_BAR_H - 4;
    uint16_t x = 480 - w - 2;       // far right
    uint16_t y = 2;

    uint16_t bg = 0xF800;  // RED (RGB565)

    Displ_FillArea(x, y, w, h, bg);
    Displ_Border(x, y, w, h, 1, OSC_TEXT_WHITE);
    Displ_WString(x + 10, y + 3, "I2C", Font12, 1, OSC_TEXT_WHITE, bg);
}
/********************* DRAW UI ************************/
void OSCP_DrawUI(void)
{
    Displ_CLS(OSC_BG);

    OSCP_DrawStatusBar();
    OSCP_DrawGrid();
    OSCP_DrawGroundIndicator();
    OSCP_DrawLeftPanel();
    OSCP_DrawRightPanel();
    OSCP_DrawBottomBar();
    OSCP_DrawTriggerIndicator();
    OSCP_DrawTriggerLine();
    OSCP_DrawTopButton_UART();
    OSCP_DrawTopButton_I2C();

    if(oscPro.cursorsEnabled) {
        if(oscPro.cursorMode==0)
            OSCP_DrawHorizontalCursors();
        else
            OSCP_DrawVerticalCursors();
    }

    firstDraw = 1;
}


/********************* DATA COPY + HOLD FIX ************************/
void OSCP_Update(void)
{
    /* ---------------------------
     * MODE ROUTING
     * --------------------------- */
	if (protocol_decoder.display_needs_update)
	{
	    //Protocol_UpdateDisplayLines();
	    //Protocol_DrawWindow(10, 20,160,280);
	    protocol_decoder.display_needs_update = 0;
	}
	//Displ_WString(10, 100, "update func", Font16, 1, OSC_TEXT_WHITE, BLACK);
    if (currentMode == MODE_UART)
    {
    	static int row = 0;
        // Call UART decoder instead of oscilloscope engine
        //UART_Decoder_Update();     // replace with actual function name
        Protocol_Decoder_Process();
        //Protocol_DrawWindow(10, row,160,280);
        //Displ_WString(10, 90, "process decoder for uart", Font16, 1, OSC_TEXT_WHITE, BLACK);
        //Protocol_Decoder_Process();
        //Protocol_DrawWindow(0, 0, 480, 320);

        static uint32_t last_update = 0;

            // Get current tick (ms)
            uint32_t now = HAL_GetTick();

            // Run decoder continuously
            Protocol_Decoder_Process();

            // Only update protocol window once per second
            if (now - last_update >= 100)
            {
            	static int i = 20;
                last_update = now;
                Protocol_DrawWindow(10, 20, 160, 280);
                i++;
            }

        return;
    }
    else if (currentMode == MODE_I2C)
    {
        // Call I2C decoder engine
    	Protocol_Decoder_Process();
        //I2C_Decoder_Update();      // replace with actual function name+

    	static uint32_t last_update = 0;

    	            // Get current tick (ms)
    	            uint32_t now = HAL_GetTick();

    	            // Run decoder continuously
    	            Protocol_Decoder_Process();

    	            // Only update protocol window once per second
    	            if (now - last_update >= 100)
    	            {
    	            	static int i = 20;
    	                last_update = now;
    	                Protocol_DrawWindow(10, 20, 160, 280);
    	                i++;
    	            }

        return;
    }

    /* ---------------------------
     * NORMAL OSCILLOSCOPE MODE
     * --------------------------- */
    // No new ADC data? nothing to do
    if (!oscPro.adc_buffer.ready)
        return;

    // Take ownership of this buffer
    oscPro.adc_buffer.ready = 0;
    oscPro.count = oscPro.adc_buffer.count;
    if (oscPro.count > SAMPLES) oscPro.count = SAMPLES;

    // Convert raw ADC values -> mapped input voltage, store in oscPro.samples
    for (uint16_t i = 0; i < oscPro.count; i++) {
        float adc_v = OSC_ADC_RawToVoltage(oscPro.adc_buffer.raw[i]);
        oscPro.samples[i] = OSC_MapVoltageFromADC(adc_v);
    }

    // --- Auto timebase on first capture (if user hasn't touched time-scale) ---
    if (!timebase_locked && oscPro.count > 0 && actual_sample_rate > 0.0f) {
        float dt_ms      = 1000.0f / actual_sample_rate;          // ms per sample
        float capture_ms = (float)oscPro.count * dt_ms;           // total window in ms
        oscPro.timeScale = capture_ms / 10.0f;                    // ms per division (10 divs)
    }

    // Update measurements (uses actual_sample_rate, not timeScale)
    OSCP_CalculateMeasurements();

    // If we are in HOLD, DON'T redraw waveform – keep the last picture
    if (!oscPro.running)
        return;

    // Full redraw when running
    OSCP_DrawWaveform();
    OSCP_DrawLeftPanel();
    OSCP_DrawRightPanel();
    OSCP_DrawTriggerIndicator();
    OSCP_DrawTriggerLine();
    OSCP_DrawGroundIndicator();

    if (oscPro.cursorsEnabled) {
        if (oscPro.cursorMode == 0) {
            OSCP_DrawHorizontalCursors();
        } else {
            OSCP_DrawVerticalCursors();
        }
    }
}


/********************* TOUCH HANDLING ************************/
void OSCP_HandleTouch(uint16_t x, uint16_t y)
{
    /* RUN / HOLD */
	if (x < 50 && y < STATUS_BAR_H) {
	    oscPro.running = !oscPro.running;

	    if (oscPro.running) {
	        // Back to RUN: clear flags and restart ADC sampling
	        oscPro.adc_buffer.ready = 0;
	        oscPro.adc_buffer.count = 0;
	        oscPro.count            = 0;
	        OSCP_SetSampleRate(OSCP_GetSampleRate());            // keep same rate in actual_sample_rate
	        OSC_ADC_StartSampling(OSCP_GetSampleRate());
	    } else {
	        // HOLD: stop ADC sampling, keep last buffer on screen
	        OSC_ADC_StopSampling();
	    }

	    OSCP_DrawStatusBar();
	}

    /* Time/div adjustment */
	if(x > 180 && x < 300 && y > GRID_Y + GRID_H) {
	    // Changing time scale - this determines how we interpret captured data
	    if(oscPro.timeScale >= 10.0f) oscPro.timeScale = 0.1f;
	    else if(oscPro.timeScale >= 5.0f) oscPro.timeScale = 10.0f;
	    else if(oscPro.timeScale >= 2.0f) oscPro.timeScale = 5.0f;
	    else if(oscPro.timeScale >= 1.0f) oscPro.timeScale = 2.0f;
	    else if(oscPro.timeScale >= 0.5f) oscPro.timeScale = 1.0f;
	    else if(oscPro.timeScale >= 0.2f) oscPro.timeScale = 0.5f;
	    else if(oscPro.timeScale >= 0.1f) oscPro.timeScale = 0.2f;
	    else oscPro.timeScale = 0.1f;

	    timebase_locked = 1;   // user took control of timeScale

	    // Recalculate measurements with new time scale
	    OSCP_CalculateMeasurements();

	    OSCP_DrawBottomBar();
	    OSCP_DrawLeftPanel();  // Update frequency/period display

	    // Redraw waveform to show proper scaling
	    firstDraw = 1;
	    OSCP_DrawWaveform();
	}


    /* V/div */
    if(x<53 && y>GRID_Y+GRID_H){
        float s = oscPro.ch1.scale;
        if(s>=2) s=0.1;
        else if(s>=1) s=2;
        else if(s>=0.5) s=1;
        else if(s>=0.2) s=0.5;
        else if(s>=0.1) s=0.2;
        else s=0.1;
        oscPro.ch1.scale = s;
        OSCP_DrawBottomBar();
        OSCP_DrawUI();
        return;
    }

    /* Range toggle */
    if(x>85 && x<135 && y>GRID_Y+GRID_H){
        oscPro.inputRange = !oscPro.inputRange;
        if(oscPro.inputRange && oscPro.ch1.scale<0.5f)
            oscPro.ch1.scale = 0.5f;
        OSCP_DrawBottomBar();
        OSCP_DrawUI();
        return;
    }

    /* Trigger mode */
    if(x>365 && x<415 && y>GRID_Y+GRID_H){
        oscPro.triggerMode = (oscPro.triggerMode+1)%3;
        OSCP_DrawBottomBar();
        return;
    }

    /* Cursor button */
    if(x>415 && x<430 && y>GRID_Y+GRID_H){
        if(!oscPro.cursorsEnabled){
            oscPro.cursorsEnabled=1;
            oscPro.cursorMode=0;
            selected_cursor=1;
        } else {
            selected_cursor = (selected_cursor==1)?2:1;
        }
        OSCP_DrawBottomBar();
        if(oscPro.cursorMode==0){
            OSCP_ClearHorizontalCursors();
            OSCP_DrawHorizontalCursors();
        } else {
            OSCP_ClearVerticalCursors();
            OSCP_DrawVerticalCursors();
        }
        return;
    }

    /* Cursor mode toggle / disable */
    if(x>430 && x<480 && y>GRID_Y+GRID_H){
        static uint32_t last = 0;
        uint32_t now = HAL_GetTick();

        if(oscPro.cursorsEnabled && (now-last)<500){
            /* double tap → disable */
            if(oscPro.cursorMode==0) OSCP_ClearHorizontalCursors();
            else                     OSCP_ClearVerticalCursors();
            oscPro.cursorsEnabled=0;
            OSCP_DrawBottomBar();
            last=0;
            return;
        }

        if(oscPro.cursorsEnabled){
            /* toggle */
            if(oscPro.cursorMode==0){
                OSCP_ClearHorizontalCursors();
                oscPro.cursorMode=1;
            } else {
                OSCP_ClearVerticalCursors();
                oscPro.cursorMode=0;
            }
            selected_cursor=1;
            OSCP_DrawBottomBar();
            if(oscPro.cursorMode==0) OSCP_DrawHorizontalCursors();
            else                     OSCP_DrawVerticalCursors();
        }
        last = now;
        return;
    }

    /* Inside Grid */
    if(x>GRID_X && x<GRID_X+GRID_W && y>GRID_Y && y<GRID_Y+GRID_H){
        if(oscPro.cursorsEnabled){
            if(oscPro.cursorMode==0){
                /* horizontal cursor drag */
                float pixelsPer = (8.0f*oscPro.ch1.scale)/GRID_H;
                float v = (GRID_Y+GRID_H/2 - y) * pixelsPer;
                float lim = (oscPro.inputRange==0)?5.0f:15.0f;
                if(v>lim) v=lim;
                if(v<-lim) v=-lim;

                if(selected_cursor==1) oscPro.cursor1_volt = v;
                else                   oscPro.cursor2_volt = v;

                OSCP_ClearHorizontalCursors();
                OSCP_DrawHorizontalCursors();
            } else {
                /* vertical cursor drag */
                float d = (float)(x-GRID_X)/GRID_W * 10.0f;
                if(d<0) d=0;
                if(d>10) d=10;

                if(selected_cursor==1) oscPro.cursor1_time = d;
                else                   oscPro.cursor2_time = d;

                OSCP_ClearVerticalCursors();
                OSCP_DrawVerticalCursors();
            }
        } else {
            /* trigger drag */
            float pixelsPer = (8.0f*oscPro.ch1.scale)/GRID_H;
            float v = (GRID_Y+GRID_H/2 - y) * pixelsPer;

            float lim = (oscPro.inputRange==0)?5.0f:15.0f;
            if(v>lim) v=lim;
            if(v<-lim) v=-lim;

            oscPro.trigLevel = v;

            OSCP_ClearTriggerArrow();
            OSCP_ClearTriggerLine();
            OSCP_DrawTriggerIndicator();
            OSCP_DrawTriggerLine();
            OSCP_DrawStatusBar();
        }
        return;
    }

    // UART BUTTON
    if (x > 356 && x < 416 && y < STATUS_BAR_H)
    {
        currentMode = MODE_UART;
        Displ_CLS(BLACK);
        Protocol_Decoder_Init(PROTOCOL_UART);
        UART_Decoder_Reset();
        // Optional: Draw UART title
        Displ_WString(10, 40, "UART DECODER MODE", Font16, 1, OSC_TEXT_WHITE, BLACK);

        return;
    }

    // I2C BUTTON
    if (x > 418 && x < 478 && y < STATUS_BAR_H)
    {
        currentMode = MODE_I2C;
        Displ_CLS(BLACK);
        Protocol_Decoder_Init(PROTOCOL_I2C);
        UART_Decoder_Reset();
        // Optional title
        Displ_WString(10, 40, "I2C DECODER MODE", Font16, 1, OSC_TEXT_WHITE, BLACK);

        return;
    }
}


/********************* GETTERS ************************/
uint8_t OSCP_GetInputRange(void){ return oscPro.inputRange; }
void    OSCP_SetInputRange(uint8_t r){ oscPro.inputRange=(r?1:0); }
uint8_t OSCP_GetCursorMode(void){ return oscPro.cursorMode; }

/************************************************************
 *   MISSING FUNCTIONS — FULL DEFINITIONS
 ************************************************************/

void OSCP_ClearTriggerArrow(void)
{
    if(prev_trig_arrow_y < 0) return;

    for(int i=0;i<5;i++){
        Displ_Pixel(GRID_X-3-i,prev_trig_arrow_y,OSC_BG);
        if(i>0){
            Displ_Pixel(GRID_X-3-i,prev_trig_arrow_y-i,OSC_BG);
            Displ_Pixel(GRID_X-3-i,prev_trig_arrow_y+i,OSC_BG);
        }
    }
}

void OSCP_ClearTriggerLine(void)
{
    if(prev_trig_y < 0) return;

    for(int x=GRID_X; x<GRID_X+GRID_W; x+=2)
        Displ_Pixel(x,prev_trig_y,OSC_BG);

    prev_trig_y = -1;
}

void OSCP_DrawTriggerIndicator(void)
{
    int16_t y = OSCP_V2Y(oscPro.trigLevel);
    prev_trig_arrow_y = y;

    for(int i=0;i<5;i++){
        Displ_Pixel(GRID_X-3-i,y,OSC_TRIGGER_LINE);
        if(i>0){
            Displ_Pixel(GRID_X-3-i,y-i,OSC_TRIGGER_LINE);
            Displ_Pixel(GRID_X-3-i,y+i,OSC_TRIGGER_LINE);
        }
    }
}

void OSCP_DrawTriggerLine(void)
{
    int16_t y = OSCP_V2Y(oscPro.trigLevel);
    prev_trig_y = y;

    for(int x=GRID_X; x<GRID_X+GRID_W; x+=2)
        Displ_Pixel(x,y,OSC_TRIGGER_LINE);
}

void OSCP_DrawGroundIndicator(void)
{
    int16_t y = OSCP_V2Y(0);
    prev_ground_y = y;

    Displ_Line(GRID_X-2,y,GRID_X,y,OSC_TEXT_GREEN);
    Displ_Line(GRID_X-1,y-2,GRID_X,y-2,OSC_TEXT_GREEN);
    Displ_Line(GRID_X-1,y+2,GRID_X,y+2,OSC_TEXT_GREEN);
}

/*********************** HORIZONTAL CURSORS ************************/

void OSCP_ClearHorizontalCursors(void)
{
    if(prev_cursor1_y>=GRID_Y && prev_cursor1_y<GRID_Y+GRID_H){
        for(int x=GRID_X; x<GRID_X+GRID_W; x+=2)
            Displ_Pixel(x,prev_cursor1_y,OSC_BG);
    }
    if(prev_cursor2_y>=GRID_Y && prev_cursor2_y<GRID_Y+GRID_H){
        for(int x=GRID_X; x<GRID_X+GRID_W; x+=2)
            Displ_Pixel(x,prev_cursor2_y,OSC_BG);
    }
}

void OSCP_DrawHorizontalCursors(void)
{
    if(!oscPro.cursorsEnabled) return;

    int16_t y1 = OSCP_V2Y(oscPro.cursor1_volt);
    int16_t y2 = OSCP_V2Y(oscPro.cursor2_volt);

    prev_cursor1_y = y1;
    prev_cursor2_y = y2;

    uint16_t c1 = (selected_cursor==1)?OSC_TEXT_WHITE:0x8410;
    uint16_t c2 = (selected_cursor==2)?0x07FF:0x4208;

    for(int x=GRID_X; x<GRID_X+GRID_W; x+=2)
        Displ_Pixel(x,y1,c1);

    for(int x=GRID_X; x<GRID_X+GRID_W; x+=2)
        Displ_Pixel(x,y2,c2);

    /* ---- Show Voltage Cursor Measurements ---- */

    char txt[32];

    // VC1 label
    sprintf(txt, "VC1: %.2fV", oscPro.cursor1_volt);
    Displ_FillArea(GRID_X + 5, y1 - 10, 70, 12, OSC_MEAS_BG);
    Displ_WString(GRID_X + 7, y1 - 8, txt, Font8, 1, c1, OSC_MEAS_BG);

    // VC2 label
    sprintf(txt, "VC2: %.2fV", oscPro.cursor2_volt);
    Displ_FillArea(GRID_X + 5, y2 - 10, 70, 12, OSC_MEAS_BG);
    Displ_WString(GRID_X + 7, y2 - 8, txt, Font8, 1, c2, OSC_MEAS_BG);

    // dV = |VC1 - VC2|
    float dv = fabs(oscPro.cursor1_volt - oscPro.cursor2_volt);
    sprintf(txt, "dV: %.2fV", dv);
    Displ_FillArea(GRID_X + GRID_W - 80, GRID_Y + 5, 75, 12, OSC_MEAS_BG);
    Displ_WString(GRID_X + GRID_W - 78, GRID_Y + 7, txt, Font8, 1, OSC_TEXT_ORANGE, OSC_MEAS_BG);

}

/*********************** VERTICAL CURSORS ************************/

void OSCP_ClearVerticalCursors(void)
{
    if(prev_cursor1_x>=GRID_X && prev_cursor1_x<GRID_X+GRID_W){
        for(int y=GRID_Y; y<GRID_Y+GRID_H; y+=2)
            Displ_Pixel(prev_cursor1_x,y,OSC_BG);
    }
    if(prev_cursor2_x>=GRID_X && prev_cursor2_x<GRID_X+GRID_W){
        for(int y=GRID_Y; y<GRID_Y+GRID_H; y+=2)
            Displ_Pixel(prev_cursor2_x,y,OSC_BG);
    }
}

void OSCP_DrawVerticalCursors(void)
{
    if(!oscPro.cursorsEnabled) return;

    int16_t x1 = OSCP_Time2X(oscPro.cursor1_time);
    int16_t x2 = OSCP_Time2X(oscPro.cursor2_time);

    prev_cursor1_x = x1;
    prev_cursor2_x = x2;

    uint16_t c1 = (selected_cursor==1)?OSC_TEXT_ORANGE:0x6320;
    uint16_t c2 = (selected_cursor==2)?OSC_TEXT_CYAN:0x4208;

    for(int y=GRID_Y; y<GRID_Y+GRID_H; y+=2)
        Displ_Pixel(x1,y,c1);

    for(int y=GRID_Y; y<GRID_Y+GRID_H; y+=2)
        Displ_Pixel(x2,y,c2);

    /* ---- Show Time Cursor Measurements ---- */

    char txt[32];

    // Convert cursor position (0-10 divs) into ms
    float t1 = oscPro.cursor1_time * oscPro.timeScale;
    float t2 = oscPro.cursor2_time * oscPro.timeScale;

    // TC1 label
    if(t1 < 1.0f)
        sprintf(txt, "TC1: %.0fus", t1 * 1000);
    else
        sprintf(txt, "TC1: %.2fms", t1);

    Displ_FillArea(x1 - 25, GRID_Y + 5, 50, 12, OSC_MEAS_BG);
    Displ_WString(x1 - 23, GRID_Y + 7, txt, Font8, 1, c1, OSC_MEAS_BG);

    // TC2 label
    if(t2 < 1.0f)
        sprintf(txt, "TC2: %.0fus", t2 * 1000);
    else
        sprintf(txt, "TC2: %.2fms", t2);

    Displ_FillArea(x2 - 25, GRID_Y + 20, 50, 12, OSC_MEAS_BG);
    Displ_WString(x2 - 23, GRID_Y + 22, txt, Font8, 1, c2, OSC_MEAS_BG);

    // ΔT = |TC1 - TC2|
    float dt = fabs(t1 - t2);

    if(dt < 1.0f)
        sprintf(txt, "dT: %.0fus", dt * 1000);
    else
        sprintf(txt, "dT: %.2fms", dt);

    Displ_FillArea(GRID_X + GRID_W - 80, GRID_Y + 5, 75, 12, OSC_MEAS_BG);
    Displ_WString(GRID_X + GRID_W - 78, GRID_Y + 7, txt, Font8, 1, OSC_TEXT_CYAN, OSC_MEAS_BG);

    // Frequency = 1/dT
    if(dt > 0.000001f) {
        float freq = 1000.0f / dt;  // dt in ms → Hz
        sprintf(txt, "f: %.2fHz", freq);
    } else {
        sprintf(txt, "f: ---");
    }

    Displ_FillArea(GRID_X + GRID_W - 80, GRID_Y + 20, 75, 12, OSC_MEAS_BG);
    Displ_WString(GRID_X + GRID_W - 78, GRID_Y + 22, txt, Font8, 1, OSC_TEXT_CYAN, OSC_MEAS_BG);

}

void OSCP_SetSampleRate(uint16_t rate_khz)
{
    // Store as Hz for measurement engine
    actual_sample_rate = (float)rate_khz * 1000.0f;
}

uint16_t OSCP_GetSampleRate(void)
{
    return (uint16_t)((actual_sample_rate / 1000.0f) + 0.5f);
}

