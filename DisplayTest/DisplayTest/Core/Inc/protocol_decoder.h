/*
 * protocol_decoder.h
 *
 *  Created on: Nov 18, 2025
 *      Author: omkar
 */

#ifndef INC_PROTOCOL_DECODER_H_
#define INC_PROTOCOL_DECODER_H_

/*
 * protocol_decoder.h
 * I2C and UART Protocol Decoder for STM32 Oscilloscope
 *
 * Designed for STM32F411E-DISCO
 * Digital inputs: PB6 (D1), PB7 (D2)
 */

#include "main.h"
#include <stdint.h>

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

// Pin assignments
#define DECODER_PIN1_PORT    GPIOE
#define DECODER_PIN1_PIN     GPIO_PIN_9  // D1 = SCL or TX
#define DECODER_PIN2_PORT    GPIOE
#define DECODER_PIN2_PIN     GPIO_PIN_10  // D2 = SDA or RX

// Buffer sizes
#define PROTOCOL_CAPTURE_BUFFER_SIZE    1000
#define PROTOCOL_DECODED_BUFFER_SIZE    256
#define PROTOCOL_DISPLAY_LINES          8

// Protocol types
typedef enum {
    PROTOCOL_NONE = 0,
    PROTOCOL_I2C,
    PROTOCOL_UART,
    PROTOCOL_SPI,
    PROTOCOL_AUTO_DETECT
} Protocol_Type_t;

/*============================================================================
 * I2C DECODER STRUCTURES
 *============================================================================*/

typedef enum {
    I2C_IDLE,
    I2C_START,
    I2C_ADDRESS,
    I2C_READ_ACK,
    I2C_DATA,
    I2C_STOP,
    I2C_ERROR
} I2C_State_t;

typedef struct {
    uint8_t address;        // 7-bit address
    uint8_t rw_bit;         // 0=Write, 1=Read
    uint8_t data[32];       // Data bytes
    uint8_t data_count;     // Number of data bytes
    uint8_t ack_flags;      // Bit field of ACK/NACK
    uint32_t timestamp_us;  // Transaction start time
    uint8_t error;          // Error flag

} I2C_Transaction_t;

typedef struct {
    I2C_State_t state;
    uint8_t bit_count;
    uint8_t current_byte;
    I2C_Transaction_t current_transaction;
    I2C_Transaction_t transactions[16];  // Last 16 transactions
    uint8_t transaction_count;
    uint8_t scl_prev;
    uint8_t sda_prev;

    uint32_t last_timestamp_us;
} I2C_Decoder_t;

/*============================================================================
 * UART DECODER STRUCTURES
 *============================================================================*/

typedef enum {
    UART_IDLE,
    UART_START_BIT,
    UART_DATA_BITS,
    UART_PARITY_BIT,
    UART_STOP_BIT,
    UART_ERROR
} UART_State_t;

typedef enum {
    UART_PARITY_NONE,
    UART_PARITY_EVEN,
    UART_PARITY_ODD
} UART_Parity_t;

typedef struct {
    uint32_t baudrate;          // Detected baud rate
    uint8_t  data_bits;         // 5, 6, 7, 8
    UART_Parity_t parity;       // None, Even, Odd
    uint8_t  stop_bits;         // 1, 1.5, 2
} UART_Config_t;

typedef struct {
    UART_State_t state;
    UART_Config_t config;
    uint32_t bit_time_us;       // Bit duration in microseconds
    uint32_t last_transition_time;
    uint8_t bit_count;
    uint8_t current_byte;
    uint8_t parity_bit;
    uint8_t rx_buffer[PROTOCOL_DECODED_BUFFER_SIZE];
    uint16_t rx_count;
    uint8_t error_count;


} UART_Decoder_t;

/*============================================================================
 * EDGE CAPTURE STRUCTURES (for input capture method)
 *============================================================================*/

typedef struct {
    uint32_t timestamp_us;  // Time of edge
    uint8_t pin1_state;     // State of pin 1 (SCL/TX)
    uint8_t pin2_state;     // State of pin 2 (SDA/RX)
    uint8_t channel;        // Which channel triggered
} Edge_Capture_t;

/*============================================================================
 * MAIN DECODER STRUCTURE
 *============================================================================*/

typedef struct {
    Protocol_Type_t active_protocol;
    uint8_t decoder_enabled;

    // Capture buffer
    Edge_Capture_t capture_buffer[PROTOCOL_CAPTURE_BUFFER_SIZE];
    uint16_t capture_head;
    uint16_t capture_tail;
    uint16_t capture_count;

    // Protocol-specific decoders
    I2C_Decoder_t i2c;
    UART_Decoder_t uart;

    // Display state
    char display_lines[PROTOCOL_DISPLAY_LINES][64];
    uint8_t display_line_count;

    uint8_t display_needs_update;

    // Statistics
    uint32_t total_transactions;
    uint32_t error_count;

} Protocol_Decoder_t;

extern Protocol_Decoder_t protocol_decoder;

/*============================================================================
 * FUNCTION DECLARATIONS
 *============================================================================*/

/**
 * @brief Initialize protocol decoder
 * @param protocol: Protocol type to decode
 */
void Protocol_Decoder_Init(Protocol_Type_t protocol);

/**
 * @brief Deinitialize and stop decoder
 */
void Protocol_Decoder_DeInit(void);

/**
 * @brief Process captured edges (call in main loop)
 */
void Protocol_Decoder_Process(void);

/**
 * @brief Capture edge event (call from ISR or timer callback)
 * @param timestamp_us: Timestamp in microseconds
 * @param pin1_state: State of pin 1
 * @param pin2_state: State of pin 2
 * @param channel: Which channel triggered
 */
void Protocol_Decoder_CaptureEdge(uint32_t timestamp_us, uint8_t pin1_state,
                                   uint8_t pin2_state, uint8_t channel);

/**
 * @brief Get current pin states
 */
void Protocol_Decoder_GetPinStates(uint8_t *pin1, uint8_t *pin2);

/*============================================================================
 * I2C SPECIFIC FUNCTIONS
 *============================================================================*/

/**
 * @brief Process I2C edges
 */
void I2C_Decoder_ProcessEdge(uint8_t scl, uint8_t sda, uint32_t timestamp_us);

/**
 * @brief Reset I2C decoder state
 */
void I2C_Decoder_Reset(void);

/**
 * @brief Get last I2C transaction
 * @return Pointer to last transaction or NULL
 */
I2C_Transaction_t* I2C_Decoder_GetLastTransaction(void);

/**
 * @brief Format I2C transaction for display
 * @param trans: Transaction to format
 * @param buffer: Output buffer
 * @param buffer_size: Size of output buffer
 */
void I2C_FormatTransaction(I2C_Transaction_t *trans, char *buffer, uint16_t buffer_size);

/*============================================================================
 * UART SPECIFIC FUNCTIONS
 *============================================================================*/

/**
 * @brief Process UART samples
 * @param rx_state: State of RX pin
 * @param timestamp_us: Current timestamp
 */
void UART_Decoder_ProcessSample(uint8_t rx_state, uint32_t timestamp_us);

/**
 * @brief Auto-detect UART baud rate
 * @return Detected baud rate or 0 if failed
 */
uint32_t UART_Decoder_AutoBaud(void);

/**
 * @brief Reset UART decoder state
 */
void UART_Decoder_Reset(void);

/**
 * @brief Get received bytes
 * @param buffer: Output buffer
 * @param max_bytes: Maximum bytes to copy
 * @return Number of bytes copied
 */
uint16_t UART_Decoder_GetBytes(uint8_t *buffer, uint16_t max_bytes);

/**
 * @brief Format UART data for display
 * @param data: Data byte
 * @param buffer: Output buffer
 * @param buffer_size: Size of output buffer
 */
void UART_FormatByte(uint8_t data, char *buffer, uint16_t buffer_size);

/*============================================================================
 * AUTO-DETECTION FUNCTIONS
 *============================================================================*/

/**
 * @brief Attempt to auto-detect protocol
 * @return Detected protocol type
 */
Protocol_Type_t Protocol_AutoDetect(void);

/**
 * @brief Check if signal looks like I2C
 * @return 1 if likely I2C, 0 otherwise
 */
uint8_t Protocol_IsLikelyI2C(void);

/**
 * @brief Check if signal looks like UART
 * @return 1 if likely UART, 0 otherwise
 */
uint8_t Protocol_IsLikelyUART(void);

/*============================================================================
 * DISPLAY FUNCTIONS
 *============================================================================*/

/**
 * @brief Draw protocol decoder window on screen
 * @param x: X position
 * @param y: Y position
 * @param width: Window width
 * @param height: Window height
 */
void Protocol_DrawWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height);

/**
 * @brief Draw digital signal timing diagram
 * @param x: X position
 * @param y: Y position
 * @param width: Width of diagram
 * @param height: Height per signal
 */
void Protocol_DrawTimingDiagram(uint16_t x, uint16_t y, uint16_t width, uint16_t height);

/**
 * @brief Update display lines with latest decoded data
 */
void Protocol_UpdateDisplayLines(void);

/*============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get current timestamp in microseconds
 * @return Timestamp in microseconds
 */
uint32_t Protocol_GetTimestamp_us(void);

/**
 * @brief Calculate bit time from baud rate
 * @param baudrate: Baud rate in bps
 * @return Bit time in microseconds
 */
uint32_t Protocol_BaudToBitTime(uint32_t baudrate);

/**
 * @brief Calculate baud rate from bit time
 * @param bit_time_us: Bit time in microseconds
 * @return Baud rate in bps
 */
uint32_t Protocol_BitTimeToBaud(uint32_t bit_time_us);

/**
 * @brief Find nearest standard baud rate
 * @param measured_baud: Measured baud rate
 * @return Nearest standard baud rate
 */
uint32_t Protocol_FindNearestBaudRate(uint32_t measured_baud);


#endif /* INC_PROTOCOL_DECODER_H_ */
