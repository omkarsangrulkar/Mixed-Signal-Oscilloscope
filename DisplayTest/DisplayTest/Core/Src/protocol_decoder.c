/*
 * protocol_decoder.c
 *
 *  Created on: Nov 18, 2025
 *      Author: omkar
 */
/*
 * protocol_decoder.c
 * I2C and UART Protocol Decoder Implementation
 */

#include "protocol_decoder.h"
#include "oscilloscope_enhanced.h"
#include "z_displ_ILI9XXX.h"
#include <string.h>
#include <stdio.h>

extern volatile uint8_t Displ_SpiAvailable;

Protocol_Decoder_t protocol_decoder;

// Standard UART baud rates
static const uint32_t standard_baudrates[] = {
    300, 600, 1200, 2400, 4800, 9600, 14400, 19200,
    38400, 57600, 115200, 230400, 460800, 921600
};


/*============================================================================
 * UART DECODER IMPLEMENTATION
 *============================================================================*/

/* File-local UART sampling state */
static uint8_t  uart_prev_level        = 1;  // line idle = high
static uint32_t uart_next_sample_time  = 0;  // next virtual sample (us)
static uint8_t  uart_frame_active      = 0;  // 1 while inside a frame
static uint8_t  uart_init_done         = 0;  // first-call guard


/*============================================================================
 * INITIALIZATION
 *============================================================================*/

void Protocol_Decoder_Init(Protocol_Type_t protocol)
{
    memset(&protocol_decoder, 0, sizeof(Protocol_Decoder_t));

    protocol_decoder.display_needs_update = 0;
    protocol_decoder.active_protocol = protocol;
    protocol_decoder.decoder_enabled = 1;



    // Configure GPIO pins as inputs with pull-up
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DECODER_PIN1_PIN | DECODER_PIN2_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(DECODER_PIN1_PORT, &GPIO_InitStruct);

    // Initialize protocol-specific decoders
    if(protocol == PROTOCOL_I2C || protocol == PROTOCOL_AUTO_DETECT) {
        I2C_Decoder_Reset();
    }

    if(protocol == PROTOCOL_UART || protocol == PROTOCOL_AUTO_DETECT) {
        UART_Decoder_Reset();
        // Try to auto-detect baud rate
        UART_Decoder_AutoBaud();
    }

    // For PE9 (EXTI9_5)
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

    // For PE10 (EXTI15_10)
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

    Protocol_UpdateDisplayLines();
}

void Protocol_Decoder_DeInit(void)
{
    protocol_decoder.decoder_enabled = 0;

    // Reset GPIO to default state
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DECODER_PIN1_PIN | DECODER_PIN2_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(DECODER_PIN1_PORT, &GPIO_InitStruct);
}

/*============================================================================
 * EDGE CAPTURE
 *============================================================================*/

void Protocol_Decoder_CaptureEdge(uint32_t timestamp_us, uint8_t pin1_state,
                                   uint8_t pin2_state, uint8_t channel)
{
    if(!protocol_decoder.decoder_enabled) return;

    // Store in circular buffer
    uint16_t head = protocol_decoder.capture_head;
    protocol_decoder.capture_buffer[head].timestamp_us = timestamp_us;
    protocol_decoder.capture_buffer[head].pin1_state = pin1_state;
    protocol_decoder.capture_buffer[head].pin2_state = pin2_state;
    protocol_decoder.capture_buffer[head].channel = channel;

    protocol_decoder.capture_head = (head + 1) % PROTOCOL_CAPTURE_BUFFER_SIZE;

    if(protocol_decoder.capture_count < PROTOCOL_CAPTURE_BUFFER_SIZE) {
        protocol_decoder.capture_count++;
    } else {
        // Buffer full - advance tail
        protocol_decoder.capture_tail = (protocol_decoder.capture_tail + 1) % PROTOCOL_CAPTURE_BUFFER_SIZE;
    }
}

void Protocol_Decoder_GetPinStates(uint8_t *pin1, uint8_t *pin2)
{
    *pin1 = HAL_GPIO_ReadPin(DECODER_PIN1_PORT, DECODER_PIN1_PIN);
    *pin2 = HAL_GPIO_ReadPin(DECODER_PIN2_PORT, DECODER_PIN2_PIN);
}

/*============================================================================
 * MAIN PROCESSING LOOP
 *============================================================================*/

void Protocol_Decoder_Process(void)
{
	 //Displ_WString(10, 50, "entered in decoder process func", Font16, 1, OSC_TEXT_WHITE, BLACK);
    if(!protocol_decoder.decoder_enabled)
    	{
    	 //Displ_WString(10, 180, "DECODER NOT ENABLED", Font16, 1, OSC_TEXT_WHITE, BLACK);

    		return;
    	}



    // Process captured edges
    //Displ_WString(10, 190, "capture count NOT detected", Font16, 1, OSC_TEXT_WHITE, BLACK);
    while(protocol_decoder.capture_count > 0) {
    	//Displ_WString(10, 210, "capture count detected", Font16, 1, OSC_TEXT_WHITE, BLACK);
        uint16_t tail = protocol_decoder.capture_tail;
        Edge_Capture_t *edge = &protocol_decoder.capture_buffer[tail];

        switch(protocol_decoder.active_protocol) {
            case PROTOCOL_I2C:
            	//Displ_WString(10, 220, "decoder doing sample I2C", Font16, 1, OSC_TEXT_WHITE, BLACK);
                I2C_Decoder_ProcessEdge(edge->pin1_state, edge->pin2_state, edge->timestamp_us);
                break;

            case PROTOCOL_UART:
            	//Displ_WString(10, 220, "decoder doing sample UART", Font16, 1, OSC_TEXT_WHITE, BLACK);
                UART_Decoder_ProcessSample(edge->pin1_state, edge->timestamp_us);
                break;

            default:
                break;
        }

        protocol_decoder.capture_tail = (tail + 1) % PROTOCOL_CAPTURE_BUFFER_SIZE;
        protocol_decoder.capture_count--;
    }
}


/*============================================================================
 * I2C DECODER IMPLEMENTATION
 *============================================================================*/

void I2C_Decoder_Reset(void)
{
    memset(&protocol_decoder.i2c, 0, sizeof(I2C_Decoder_t));
    protocol_decoder.i2c.state = I2C_IDLE;
    protocol_decoder.i2c.scl_prev = 1;
    protocol_decoder.i2c.sda_prev = 1;

    Displ_WString(20, 30,   "[0x555]  W  99", Font16, 1, OSC_TEXT_RED, BLACK);
    Displ_WString(20, 50,   "[0x111]  R  55", Font16, 1, OSC_TEXT_RED, BLACK);
    Displ_WString(20, 70,   "[0x555]  W  99", Font16, 1, OSC_TEXT_RED, BLACK);
    Displ_WString(20, 90,   "[0x111]  R  55", Font16, 1, OSC_TEXT_RED, BLACK);
    Displ_WString(20, 110,  "[0x555]  W  99", Font16, 1, OSC_TEXT_RED, BLACK);
    Displ_WString(20, 130,  "[0x111]  R  55", Font16, 1, OSC_TEXT_RED, BLACK);
    Displ_WString(20, 150,  "[0x555]  W  99", Font16, 1, OSC_TEXT_RED, BLACK);
    Displ_WString(20, 170,  "[0x111]  R  55", Font16, 1, OSC_TEXT_RED, BLACK);


    Displ_WString(20, 190,   "[0x555]  W  99", Font16, 1, OSC_TEXT_RED, BLACK);
    Displ_WString(20, 210,   "[0x111]  R  55", Font16, 1, OSC_TEXT_RED, BLACK);
    Displ_WString(20, 230,   "[0x555]  W  99", Font16, 1, OSC_TEXT_RED, BLACK);
    Displ_WString(20, 250,   "[0x111]  R  55", Font16, 1, OSC_TEXT_RED, BLACK);
    Displ_WString(20, 270,  "[0x555]  W  99", Font16, 1, OSC_TEXT_RED, BLACK);
    Displ_WString(20, 290,  "[0x111]  R  55", Font16, 1, OSC_TEXT_RED, BLACK);
    Displ_WString(20, 310,  "[0x555]  W  99", Font16, 1, OSC_TEXT_RED, BLACK);
    Displ_WString(20, 330,  "[0x111]  R  55", Font16, 1, OSC_TEXT_RED, BLACK);


    Displ_WString(20, 350,   "[0x555]  W  99", Font16, 1, OSC_TEXT_RED, BLACK);
    Displ_WString(20, 370,   "[0x111]  R  55", Font16, 1, OSC_TEXT_RED, BLACK);
    Displ_WString(20, 390,   "[0x555]  W  99", Font16, 1, OSC_TEXT_RED, BLACK);
    Displ_WString(20, 410,   "[0x111]  R  55", Font16, 1, OSC_TEXT_RED, BLACK);

}

void I2C_Decoder_ProcessEdge(uint8_t scl, uint8_t sda, uint32_t timestamp_us)
{
    I2C_Decoder_t *dec = &protocol_decoder.i2c;

    // --- DEBUG: raw pin levels ---
    //Displ_WString(70, 260, sda ? "SDA=1" : "SDA=0", Font12, 1, OSC_TEXT_WHITE, BLACK);
    //Displ_WString(70, 280, scl ? "SCL=1" : "SCL=0", Font12, 1, OSC_TEXT_WHITE, BLACK);





    /*======================================================================
      START CONDITION
    ======================================================================*/
    if(scl == 1 && dec->scl_prev == 1 && sda == 0 && dec->sda_prev == 1)
    {
        dec->state = I2C_ADDRESS;
        dec->bit_count = 0;
        dec->current_byte = 0;

        dec->current_transaction.timestamp_us = timestamp_us;
        dec->current_transaction.data_count = 0;
        dec->current_transaction.error = 0;

        // NEW → PRINT START
        //Displ_WString(20, 120, "START", Font16, 1, OSC_TEXT_RED, BLACK);
    }

    /*======================================================================
      STOP CONDITION
    ======================================================================*/
    else if(scl == 1 && dec->scl_prev == 1 && sda == 1 && dec->sda_prev == 0)
    {
        if(dec->state != I2C_IDLE)
        {
            // NEW → PRINT STOP
            //Displ_WString(20, 140, "STOP", Font16, 1, OSC_TEXT_RED, BLACK);

            // Store transaction
            if(dec->transaction_count < 16) {
                memcpy(&dec->transactions[dec->transaction_count],
                       &dec->current_transaction,
                       sizeof(I2C_Transaction_t));
                dec->transaction_count++;
                protocol_decoder.total_transactions++;
            }

            dec->state = I2C_IDLE;

            // NEW → PRINT ENTIRE TRANSACTION SUMMARY
            char line[64];
            snprintf(line, sizeof(line), "[0x%02X %c] %d bytes",
                     dec->current_transaction.address,
                     dec->current_transaction.rw_bit ? 'R' : 'W',
                     dec->current_transaction.data_count);

            Displ_WString(20, 180, line, Font12, 1, OSC_TEXT_WHITE, BLACK);
        }
    }

    /*======================================================================
      DATA SAMPLING ON SCL RISING EDGE
    ======================================================================*/
    else if(scl == 1 && dec->scl_prev == 0)
    {
        switch(dec->state)
        {
            /*----------------------------------------------------------
              ADDRESS BYTE
            ----------------------------------------------------------*/
            case I2C_ADDRESS:
                dec->current_byte = (dec->current_byte << 1) | sda;
                dec->bit_count++;

                if(dec->bit_count == 8)
                {
                    dec->current_transaction.address = dec->current_byte >> 1;
                    dec->current_transaction.rw_bit = dec->current_byte & 0x01;

                    // NEW → PRINT DECODED ADDRESS IMMEDIATELY
                    char msg[64];
                    snprintf(msg, sizeof(msg), "ADDR=0x%02X (%c)",
                             dec->current_transaction.address,
                             dec->current_transaction.rw_bit ? 'R' : 'W');

                    Displ_WString(20, 140, msg, Font16, 1, OSC_TEXT_CYAN, BLACK);

                    dec->state = I2C_READ_ACK;
                    dec->bit_count = 0;
                }
                break;

            /*----------------------------------------------------------
              ACK/NACK AFTER ADDRESS OR DATA
            ----------------------------------------------------------*/
            case I2C_READ_ACK:
                if(sda == 0)
                {
                    // NEW → Print ACK immediately
                    //Displ_WString(20, 160, "ACK", Font16, 1, OSC_TEXT_GREEN, BLACK);

                    dec->state = I2C_DATA;
                    dec->current_byte = 0;
                }
                else
                {
                    // NEW → Print NACK immediately
                    //Displ_WString(20, 160, "NACK", Font16, 1, OSC_TEXT_RED, BLACK);

                    dec->current_transaction.error = 1;
                    dec->state = I2C_IDLE;
                }
                break;

            /*----------------------------------------------------------
              DATA BYTES
            ----------------------------------------------------------*/
            case I2C_DATA:
                dec->current_byte = (dec->current_byte << 1) | sda;
                dec->bit_count++;

                if(dec->bit_count == 8)
                {
                    if(dec->current_transaction.data_count < 32)
                    {
                        uint8_t b = dec->current_byte;
                        dec->current_transaction.data[
                            dec->current_transaction.data_count++] = b;

                        // NEW → Print each DATA byte as soon as it is decoded
                        char data_msg[64];
                        snprintf(data_msg, sizeof(data_msg),
                                 "DATA[%d]=0x%02X",
                                 dec->current_transaction.data_count - 1, b);

                        Displ_WString(20, 200 + 14*(dec->current_transaction.data_count-1),
                                      data_msg,
                                      Font12, 1, OSC_TEXT_WHITE, BLACK);
                    }

                    dec->state = I2C_READ_ACK;
                    dec->bit_count = 0;
                    dec->current_byte = 0;
                }
                break;

            default:
                break;
        }
    }

    dec->scl_prev = scl;
    dec->sda_prev = sda;
}

I2C_Transaction_t* I2C_Decoder_GetLastTransaction(void)
{
    if(protocol_decoder.i2c.transaction_count == 0) {
        return NULL;
    }
    return &protocol_decoder.i2c.transactions[protocol_decoder.i2c.transaction_count - 1];
}

void I2C_FormatTransaction(I2C_Transaction_t *trans, char *buffer, uint16_t buffer_size)
{
    if(trans == NULL) {
        snprintf(buffer, buffer_size, "No transaction");
        return;
    }

    char *ptr = buffer;
    int remaining = buffer_size;
    int written;

    // Address and R/W
    written = snprintf(ptr, remaining, "[0x%02X %c] ",
                      trans->address,
                      trans->rw_bit ? 'R' : 'W');
    ptr += written;
    remaining -= written;

    // Data bytes
    for(int i = 0; i < trans->data_count && remaining > 0; i++) {
        written = snprintf(ptr, remaining, "0x%02X ", trans->data[i]);
        ptr += written;
        remaining -= written;
    }

    // Error indicator
    if(trans->error && remaining > 0) {
        snprintf(ptr, remaining, "[ERR]");
    }
}


/*============================================================================
 * UART DECODER IMPLEMENTATION
 *============================================================================*/

void UART_Decoder_Reset(void)
{
    memset(&protocol_decoder.uart, 0, sizeof(UART_Decoder_t));

    protocol_decoder.uart.state            = UART_IDLE;
    protocol_decoder.uart.config.data_bits = 8;
    protocol_decoder.uart.config.parity    = UART_PARITY_NONE;
    protocol_decoder.uart.config.stop_bits = 1;
    protocol_decoder.uart.config.baudrate  = 9600;  // fixed for now

    protocol_decoder.uart.bit_time_us = Protocol_BaudToBitTime(
                                            protocol_decoder.uart.config.baudrate);

    uart_prev_level       = 1;
    uart_next_sample_time = 0;
    uart_frame_active     = 0;
    uart_init_done        = 0;
}

void UART_Decoder_ProcessSample(uint8_t rx_state, uint32_t timestamp_us)
{
    UART_Decoder_t *dec = &protocol_decoder.uart;
    uint32_t t = timestamp_us;

    if (dec->bit_time_us == 0) {
        return;
    }

    // First-ever call: just initialise previous level and bail
    if (!uart_init_done) {
        uart_prev_level       = rx_state;
        uart_next_sample_time = 0;
        uart_frame_active     = 0;
        dec->state            = UART_IDLE;
        uart_init_done        = 1;
        dec->last_transition_time = t;
        return;
    }

    /* 1) If we are in the middle of a frame, generate all virtual samples
     *    between the previous edge and this edge using uart_prev_level
     */
    if (uart_frame_active) {
        while (uart_frame_active &&
               (int32_t)(t - uart_next_sample_time) >= 0) // sample_time <= t
        {
            uint8_t sample = uart_prev_level;

            switch (dec->state) {
            case UART_START_BIT:
                // Expect LOW in middle of start bit
                if (sample == 0U) {
                    dec->state = UART_DATA_BITS;
                } else {
                    // False start → abort
                    dec->state        = UART_IDLE;
                    uart_frame_active = 0;
                    dec->error_count++;
                }
                break;

            case UART_DATA_BITS:
                if (sample) {
                    dec->current_byte |= (1U << dec->bit_count);  // LSB first
                }
                dec->bit_count++;
                if (dec->bit_count >= dec->config.data_bits) {
                    if (dec->config.parity == UART_PARITY_NONE) {
                        dec->state = UART_STOP_BIT;
                    } else {
                        dec->state = UART_PARITY_BIT;
                    }
                }
                break;

            case UART_PARITY_BIT:
                dec->parity_bit = sample;
                dec->state      = UART_STOP_BIT;
                break;

            case UART_STOP_BIT:
                // Stop bit must be HIGH
                if (sample == 1U) {

                    if (dec->rx_count < PROTOCOL_DECODED_BUFFER_SIZE) {
                        // Still space: append
                        dec->rx_buffer[dec->rx_count++] = dec->current_byte;
                    } else {
                        // Buffer full: scroll left, drop oldest, append newest at end
                        memmove(&dec->rx_buffer[0],
                                &dec->rx_buffer[1],
                                PROTOCOL_DECODED_BUFFER_SIZE - 1);
                        dec->rx_buffer[PROTOCOL_DECODED_BUFFER_SIZE - 1] = dec->current_byte;
                    }

                    protocol_decoder.display_needs_update = 1;
                    Protocol_UpdateDisplayLines();
                } else {
                    // Framing error
                    dec->error_count++;
                }

                // Frame done
                dec->state        = UART_IDLE;
                uart_frame_active = 0;
                break;

            default:
                dec->state        = UART_IDLE;
                uart_frame_active = 0;
                break;
            }

            uart_next_sample_time += dec->bit_time_us;
        }
    }

    /* 2) After possibly finishing a frame, check if THIS edge is a new start.
     *    Important: we still use uart_prev_level (pre-edge) here.
     *    If STOP bit just ended and this is a 1->0 transition, this is the
     *    start bit of the NEXT byte in a back-to-back stream.
     */
    if (!uart_frame_active) {
        if (uart_prev_level == 1 && rx_state == 0) {
            // Start bit detected at time t
            uart_frame_active     = 1;
            dec->state            = UART_START_BIT;
            dec->bit_count        = 0;
            dec->current_byte     = 0;
            // First sample at center of START bit
            uart_next_sample_time = t + dec->bit_time_us / 2U;
        }
    }

    /* 3) Now update previous level for the NEXT edge */
    uart_prev_level           = rx_state;
    dec->last_transition_time = t;
}


uint32_t UART_Decoder_AutoBaud(void)
{
    // TODO: Implement auto-baud detection
    // Measure time between transitions
    // Calculate bit time
    // Match to standard baud rate

    // For now, return default
    return 9600;
}

uint16_t UART_Decoder_GetBytes(uint8_t *buffer, uint16_t max_bytes)
{
    UART_Decoder_t *dec = &protocol_decoder.uart;
    uint16_t count = (dec->rx_count < max_bytes) ? dec->rx_count : max_bytes;

    memcpy(buffer, dec->rx_buffer, count);

    return count;
}

void UART_FormatByte(uint8_t data, char *buffer, uint16_t buffer_size)
{
    // Format as ASCII if printable, otherwise hex
    if(data >= 32 && data <= 126) {
        snprintf(buffer, buffer_size, "'%c' (0x%02X)", data, data);
    } else {
        snprintf(buffer, buffer_size, "0x%02X", data);
    }
}

/*============================================================================
 * AUTO-DETECTION
 *============================================================================*/

Protocol_Type_t Protocol_AutoDetect(void)
{
    // Simple heuristics for protocol detection

    if(Protocol_IsLikelyI2C()) {
        return PROTOCOL_I2C;
    }

    if(Protocol_IsLikelyUART()) {
        return PROTOCOL_UART;
    }

    return PROTOCOL_NONE;
}

uint8_t Protocol_IsLikelyI2C(void)
{
    // I2C characteristics:
    // - Two lines that change together (clock + data)
    // - Regular clock signal on one line
    // - Start/Stop conditions

    // TODO: Implement proper detection
    // For now, check if both pins are active

    uint8_t pin1, pin2;
    Protocol_Decoder_GetPinStates(&pin1, &pin2);

    // Very basic check - both pins should be pulled up when idle
    if(pin1 == 1 && pin2 == 1) {
        return 1;
    }

    return 0;
}

uint8_t Protocol_IsLikelyUART(void)
{
    // UART characteristics:
    // - One data line (RX)
    // - Regular bit intervals
    // - Idle state is HIGH
    // - Start bit is LOW

    // TODO: Implement proper detection

    return 0;
}

/*============================================================================
 * DISPLAY FUNCTIONS
 *============================================================================*/

void Protocol_UpdateDisplayLines(void)
{
	//Displ_WString(10, 200, "display i2c", Font16, 1, OSC_TEXT_WHITE, BLACK);
    memset(protocol_decoder.display_lines, 0, sizeof(protocol_decoder.display_lines));
    protocol_decoder.display_line_count = 0;

    char *lines = (char*)protocol_decoder.display_lines;
    //char *lines_1 = (char*)protocol_decoder.display_lines;

    if(protocol_decoder.active_protocol == PROTOCOL_I2C) {
        // Display I2C transactions
    	//Displ_WString(10, 210, "display i2c if func", Font16, 1, OSC_TEXT_WHITE, BLACK);
        snprintf(lines, 64, "I2C Transactions: %lu", protocol_decoder.total_transactions);
        protocol_decoder.display_line_count = 1;

        // Show last few transactions
        for(int i = 0; i < 7 && i < protocol_decoder.i2c.transaction_count; i++) {
            int idx = protocol_decoder.i2c.transaction_count - 1 - i;
            I2C_Transaction_t *trans = &protocol_decoder.i2c.transactions[idx];
            I2C_FormatTransaction(trans, lines + (i+1)*64, 64);
            protocol_decoder.display_line_count++;
        }
    }
    else if(protocol_decoder.active_protocol == PROTOCOL_UART) {
        // Header line
        snprintf(lines, 64, "UART: %lu baud, %dN%d",
                 protocol_decoder.uart.config.baudrate,
                 protocol_decoder.uart.config.data_bits,
                 protocol_decoder.uart.config.stop_bits);
        protocol_decoder.display_line_count = 1;

        UART_Decoder_t *dec = &protocol_decoder.uart;

        // How many payload lines can we show?
        int max_payload_lines = PROTOCOL_DISPLAY_LINES - 1;
        int lines_to_show     = dec->rx_count;
        if (lines_to_show > max_payload_lines)
            lines_to_show = max_payload_lines;

        // Show the last lines_to_show bytes, oldest at top, newest at bottom
        int first_idx = dec->rx_count - lines_to_show;

        for (int i = 0; i < lines_to_show; i++) {
            int idx = first_idx + i;    // chronological
            char byte_str[32];
            UART_FormatByte(dec->rx_buffer[idx], byte_str, sizeof(byte_str));

            snprintf(lines + (protocol_decoder.display_line_count)*64,
                     64, "RX: %s", byte_str);
            protocol_decoder.display_line_count++;
        }
    }
}

void Protocol_DrawWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
	//Displ_WString(10, 240, "protocol draw window", Font16, 1, OSC_TEXT_WHITE, BLACK);
    while (!Displ_SpiAvailable) {};


    // CLEAR THE WINDOW FIRST
    //Displ_FillArea(x, y, width, height, OSC_MEAS_BG);
    //Displ_Border(x, y, width, height, 1, OSC_TEXT_WHITE);

    // Title
    const char *title = "Protocol Decoder";
    if(protocol_decoder.active_protocol == PROTOCOL_I2C) {
        title = "I2C Decoder";
    } else if(protocol_decoder.active_protocol == PROTOCOL_UART) {
        title = "UART Decoder";
    }
    Displ_WString(x+5, y+5, (char*)title, Font12, 1, OSC_TEXT_CYAN, OSC_MEAS_BG);

    // Render text buffer
     static uint16_t line_y = 20 + 22;
    for (int i = 1; i < protocol_decoder.display_line_count && i < PROTOCOL_DISPLAY_LINES; i++) {
        Displ_WString(x+5, line_y,
                      protocol_decoder.display_lines[i],
                      Font12, 1,
                      OSC_TEXT_WHITE,
                      OSC_MEAS_BG);
        line_y += 12;
    }
}

void Protocol_DrawTimingDiagram(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    // Draw two digital signal traces
    while (!Displ_SpiAvailable) {};

    // D1 line
    Displ_WString(x, y, "D1:", Font8, 1, OSC_TEXT_ORANGE, OSC_BG);
    uint16_t d1_y = y + 10;

    // D2 line
    Displ_WString(x, y + height, "D2:", Font8, 1, OSC_TEXT_CYAN, OSC_BG);
    uint16_t d2_y = y + height + 10;

    // TODO: Draw actual signal edges from capture buffer
    // This would show the digital waveforms similar to analog scope
}

/*============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

#include "stm32f4xx.h"   // at the top of protocol_decoder.c if not already pulled in

uint32_t Protocol_GetTimestamp_us(void)
{
    // DWT->CYCCNT counts CPU cycles
    return DWT->CYCCNT / (SystemCoreClock / 1000000U);
}


uint32_t Protocol_BaudToBitTime(uint32_t baudrate)
{
    if(baudrate == 0) return 0;
    return 1000000 / baudrate;  // Convert to microseconds
}

uint32_t Protocol_BitTimeToBaud(uint32_t bit_time_us)
{
    if(bit_time_us == 0) return 0;
    return 1000000 / bit_time_us;
}

uint32_t Protocol_FindNearestBaudRate(uint32_t measured_baud)
{
    uint32_t nearest = standard_baudrates[0];
    uint32_t min_diff = 0xFFFFFFFF;

    for(int i = 0; i < sizeof(standard_baudrates)/sizeof(uint32_t); i++) {
        uint32_t diff = (measured_baud > standard_baudrates[i]) ?
                        (measured_baud - standard_baudrates[i]) :
                        (standard_baudrates[i] - measured_baud);

        if(diff < min_diff) {
            min_diff = diff;
            nearest = standard_baudrates[i];
        }
    }

    return nearest;
}



/*void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    // First, keep any existing handling for TOUCH_INT, etc.
    if (GPIO_Pin == TOUCH_INT_Pin)
    {
        // your existing touch handler, e.g.
        // XPT2046_EXTI_Callback();
        return;
    }

    // Now handle protocol decoder pins
    if (GPIO_Pin == DECODER_PIN1_PIN || GPIO_Pin == DECODER_PIN2_PIN)
    {
        uint32_t t = Protocol_GetTimestamp_us();  // you already have this
        uint8_t pin1 = HAL_GPIO_ReadPin(DECODER_PIN1_PORT, DECODER_PIN1_PIN);
        uint8_t pin2 = HAL_GPIO_ReadPin(DECODER_PIN2_PORT, DECODER_PIN2_PIN);

        Protocol_Decoder_CaptureEdge(t, pin1, pin2, GPIO_Pin);
    }
}*/


