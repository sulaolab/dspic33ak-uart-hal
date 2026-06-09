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
 * Small, readable dsPIC33AK CLKGEN8 UART byte-stream HAL.
 *
 * Clock assumption:
 *   - This HAL selects the UART clock source as CLKGEN8 (UxCONbits.CLKSEL) and
 *     uses fractional baud mode. The caller passes the resulting CLKGEN8 frequency
 *     as config.uart_clk_hz; the HAL computes the baud divisor from it.
 *   - Board/application code must configure (route/enable) CLKGEN8 appropriately
 *     before calling dspic33ak_uart_init(). Clock generator bring-up, PPS, and
 *     GPIO routing remain outside this HAL.
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

/*
 * RX backend selection (per instance).
 *
 *   POLLING  - RX is read directly from the hardware RX FIFO; no RX interrupt is
 *              enabled. The rx_ring_* / rx_irq_priority config fields are ignored.
 *   ISR_RING - dspic33ak_uart_init() sets up the interrupt-driven RX ring (see
 *              dspic33ak_uart_rx_isr_ring.h): the RX ISR drains the FIFO into a caller-
 *              provided software ring, and rx_ready/read_byte/rx_flush operate on
 *              that ring instead of the FIFO. Requires rx_ring_buffer != NULL,
 *              rx_ring_buffer_size >= 2, and uses rx_irq_priority.
 *
 * Selecting the backend per instance (rather than one global compile-time switch)
 * lets a build mix modes, e.g. UART1 console = ISR ring, UART2 log = polling.
 *
 * Only POLLING and ISR_RING are valid; dspic33ak_uart_init() rejects any other
 * rx_mode value with DSPIC33AK_UART_ERR_INVALID_ARG.
 */
typedef enum {
    DSPIC33AK_UART_RX_MODE_POLLING = 0,
    DSPIC33AK_UART_RX_MODE_ISR_RING
} dspic33ak_uart_rx_mode_t;

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

    /* RX backend (see dspic33ak_uart_rx_mode_t). The rx_ring_* / rx_irq_priority
     * fields are used only when rx_mode == DSPIC33AK_UART_RX_MODE_ISR_RING. The
     * ring buffer storage is caller-provided so the HAL holds no implicit RAM. */
    dspic33ak_uart_rx_mode_t rx_mode;
    uint8_t  *rx_ring_buffer;
    uint16_t  rx_ring_buffer_size;
    uint8_t   rx_irq_priority;
} dspic33ak_uart_config_t;

/*
 * RX runtime status snapshot (backend-aware).
 *
 * This is different from dspic33ak_uart_status_t:
 *   - dspic33ak_uart_status_t      is a function return code.
 *   - dspic33ak_uart_rx_status_t   is runtime RX state / counters.
 *
 * In ISR ring mode the counters are copied from the RX ISR ring backend. In
 * polling mode rx_mode is POLLING and the backend-specific counters are zero
 * (the polling path keeps no counters). Lets callers read RX diagnostics without
 * knowing or branching on the configured backend.
 */
typedef struct {
    dspic33ak_uart_rx_mode_t rx_mode;

    uint32_t rx_isr_count;
    uint32_t rx_byte_count;
    uint32_t rx_fifo_overflow_count;
    uint32_t framing_error_count;
    uint32_t parity_error_count;
    uint32_t autobaud_overflow_count;
    uint32_t tx_collision_count;
    uint32_t rx_ring_overflow_count;
    uint32_t rx_max_drain_count;
} dspic33ak_uart_rx_status_t;

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

/*
 * Backend-aware RX status snapshot / clear.
 *
 * ISR ring mode reports/clears the RX ISR ring counters; polling mode reports a
 * zeroed snapshot (rx_mode = POLLING) and clear is a no-op. Callers use these
 * instead of the backend-specific dspic33ak_uart_rx_isr_status_* API so they
 * stay backend-agnostic.
 *
 * Returns DSPIC33AK_UART_ERR_INVALID_ARG (status NULL), _ERR_NOT_PRESENT,
 * _ERR_NOT_INITIALIZED, or DSPIC33AK_UART_OK.
 */
dspic33ak_uart_status_t dspic33ak_uart_rx_status_get(
    dspic33ak_uart_instance_t inst,
    dspic33ak_uart_rx_status_t *status);

dspic33ak_uart_status_t dspic33ak_uart_rx_status_clear(
    dspic33ak_uart_instance_t inst);

/* ========================================================================== */
/* C++ Linkage                                                                */
/* ========================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* DSPIC33AK_UART_H */
