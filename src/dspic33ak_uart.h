#ifndef DSPIC33AK_UART_H
#define DSPIC33AK_UART_H

/* ========================================================================== */
/* Includes                                                                   */
/* ========================================================================== */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* ========================================================================== */
/* C++ Linkage                                                                */
/* ========================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Module Overview                                                            */
/* ========================================================================== */

/*
 * Small, readable dsPIC33AK UART byte-stream HAL.
 *
 * Design policy:
 *   - Public API does not expose XC-DSC / DFP bitfield types.
 *   - Device-specific register names are isolated in dspic33ak_uart_device.c.
 *   - Register pointer and bit-mask helpers are isolated in dspic33ak_uart_reg.h.
 *   - Board-specific clock / PPS / GPIO routing stays outside this HAL.
 *   - stdio retarget, console parsing, and application command handling stay
 *     outside this HAL.
 *
 * Functional model:
 *   - init / deinit
 *   - presence and initialization queries
 *   - TX/RX status queries
 *   - blocking byte write with optional timeout
 *   - non-blocking byte read
 *   - buffer read/write helpers
 *   - RX FIFO flush
 */

/* ========================================================================== */
/* Public Types                                                               */
/* ========================================================================== */

typedef enum {
    DSPIC33AK_UART_INST_1 = 0,
    DSPIC33AK_UART_INST_2,
    DSPIC33AK_UART_INST_3,
    DSPIC33AK_UART_INST_4,
    DSPIC33AK_UART_INST_COUNT
} dspic33ak_uart_instance_t;

typedef enum {
    DSPIC33AK_UART_OK = 0,
    DSPIC33AK_UART_ERR_INVALID_ARG,
    DSPIC33AK_UART_ERR_NOT_PRESENT,
    DSPIC33AK_UART_ERR_NOT_INITIALIZED,
    DSPIC33AK_UART_ERR_BUSY,
    DSPIC33AK_UART_ERR_TIMEOUT,
    DSPIC33AK_UART_ERR_RX_EMPTY,
    DSPIC33AK_UART_ERR_TX_FULL,
    DSPIC33AK_UART_ERR_OVERRUN,
    DSPIC33AK_UART_ERR_FRAMING,
    DSPIC33AK_UART_ERR_PARITY,
    DSPIC33AK_UART_ERR_UNSUPPORTED
} dspic33ak_uart_status_t;

typedef uint32_t (*dspic33ak_uart_get_ms_fn)(void);

typedef enum {
    DSPIC33AK_UART_PARITY_NONE = 0,
    DSPIC33AK_UART_PARITY_EVEN,
    DSPIC33AK_UART_PARITY_ODD
} dspic33ak_uart_parity_t;

typedef struct {
    uint32_t uart_clk_hz;
    uint32_t baudrate;
    uint32_t timeout_ms;

    /*
     * Optional millisecond tick callback for timeout handling.
     * If get_ms is NULL, timeout handling is disabled.
     * If timeout_ms is 0, timeout handling is also disabled.
     */
    dspic33ak_uart_get_ms_fn get_ms;

    uint8_t data_bits;
    uint8_t stop_bits;
    dspic33ak_uart_parity_t parity;
    bool enable_tx;
    bool enable_rx;
} dspic33ak_uart_config_t;

/* ========================================================================== */
/* Public API                                                                 */
/* ========================================================================== */

dspic33ak_uart_status_t dspic33ak_uart_init(
    dspic33ak_uart_instance_t inst,
    const dspic33ak_uart_config_t *config);

dspic33ak_uart_status_t dspic33ak_uart_deinit(
    dspic33ak_uart_instance_t inst);

bool dspic33ak_uart_is_present(
    dspic33ak_uart_instance_t inst);

bool dspic33ak_uart_is_initialized(
    dspic33ak_uart_instance_t inst);

bool dspic33ak_uart_rx_ready(
    dspic33ak_uart_instance_t inst);

bool dspic33ak_uart_tx_ready(
    dspic33ak_uart_instance_t inst);

bool dspic33ak_uart_tx_done(
    dspic33ak_uart_instance_t inst);

dspic33ak_uart_status_t dspic33ak_uart_write_byte(
    dspic33ak_uart_instance_t inst,
    uint8_t data);

dspic33ak_uart_status_t dspic33ak_uart_read_byte(
    dspic33ak_uart_instance_t inst,
    uint8_t *data);

size_t dspic33ak_uart_write(
    dspic33ak_uart_instance_t inst,
    const void *data,
    size_t len);

size_t dspic33ak_uart_read(
    dspic33ak_uart_instance_t inst,
    void *data,
    size_t len);

void dspic33ak_uart_rx_flush(
    dspic33ak_uart_instance_t inst);

/* ========================================================================== */
/* C++ Linkage                                                                */
/* ========================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* DSPIC33AK_UART_H */
