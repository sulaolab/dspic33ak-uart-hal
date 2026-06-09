#ifndef DSPIC33AK_UART_RX_ISR_RING_H
#define DSPIC33AK_UART_RX_ISR_RING_H

/* ========================================================================== */
/* Includes                                                                   */
/* ========================================================================== */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "dspic33ak_uart.h"

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
 * UART RX interrupt-driven ring HAL (dsPIC33AK).
 *
 * Provides the RX ISR ring core: a single-producer (ISR) / single-consumer
 * (reader) software ring fed by draining the RX FIFO, plus the RX ISR ring
 * runtime status counters.
 *
 * Design policy (same as the rest of src/hal_uart):
 *   - No printf / halt / blocking calls.
 *   - No application dependencies (main.h, app_debug, console).
 *   - No project-specific compile-time macros: the ring buffer storage, its size,
 *     the interrupt priority value and the RX-backend build switch all stay in the
 *     board/application layer.
 *   - The scattered RX interrupt Flag/Enable/Priority bits (_UxRXIF/IE/IP) are
 *     isolated inside a small per-instance switch in dspic33ak_uart_rx_isr_ring.c.
 *
 * Interrupt vector ownership:
 *   This HAL does NOT define the _UxRXInterrupt vector. The application/integration
 *   layer owns the vector (a thin wrapper) and calls dspic33ak_uart_rx_irq_handler()
 *   from it. dspic33ak_uart_rx_irq_handler() is an ordinary function, not an ISR.
 *
 * Ring buffer ownership:
 *   The ring buffer storage is caller-provided (passed in via config). This HAL
 *   only holds the buffer pointer, its size and the read/write indices, so it
 *   consumes no implicit per-instance RAM for buffers that are never used.
 */

/* ========================================================================== */
/* Public Types                                                               */
/* ========================================================================== */

typedef struct
{
    uint8_t *buffer;        /* caller-provided ring storage (not owned)        */
    uint16_t buffer_size;   /* size of buffer in bytes; must be >= 2           */
    uint8_t irq_priority;   /* CPU interrupt priority for the RX interrupt     */
} dspic33ak_uart_rx_isr_config_t;

/*
 * RX ISR ring runtime status snapshot.
 *
 * This is different from dspic33ak_uart_status_t:
 *   - dspic33ak_uart_status_t          is a function return code.
 *   - dspic33ak_uart_rx_isr_status_t   is accumulated runtime state / counters.
 */
typedef struct
{
    uint32_t rx_isr_count;
    uint32_t rx_byte_count;
    uint32_t rx_fifo_overflow_count;
    uint32_t framing_error_count;
    uint32_t parity_error_count;
    uint32_t autobaud_overflow_count;
    uint32_t tx_collision_count;
    uint32_t rx_ring_overflow_count;
    uint32_t rx_max_drain_count;
} dspic33ak_uart_rx_isr_status_t;

/* ========================================================================== */
/* Public API                                                                 */
/* ========================================================================== */

/*
 * Configure the RX ISR ring for an instance.
 *
 * Validates the instance/config, stores the caller-provided buffer pointer,
 * resets the ring indices and status counters, sets the RX FIFO watermark to
 * interrupt on >= 1 byte, and programs the RX interrupt priority. The RX
 * interrupt is left DISABLED; call dspic33ak_uart_rx_isr_enable() to start it.
 *
 * Returns:
 *   DSPIC33AK_UART_ERR_INVALID_ARG     config/buffer NULL or buffer_size < 2
 *   DSPIC33AK_UART_ERR_NOT_PRESENT     instance not present on this device
 *   DSPIC33AK_UART_ERR_NOT_INITIALIZED UART not initialized yet
 *   DSPIC33AK_UART_ERR_UNSUPPORTED     no RX interrupt mapping for this instance
 *   DSPIC33AK_UART_OK                  configured
 */
dspic33ak_uart_status_t dspic33ak_uart_rx_isr_config(
    dspic33ak_uart_instance_t inst,
    const dspic33ak_uart_rx_isr_config_t *config);

/* Enable the RX interrupt (instance must be configured and initialized). */
dspic33ak_uart_status_t dspic33ak_uart_rx_isr_enable(
    dspic33ak_uart_instance_t inst);

/* Disable the RX interrupt (safe direction; allowed even if not configured). */
dspic33ak_uart_status_t dspic33ak_uart_rx_isr_disable(
    dspic33ak_uart_instance_t inst);

/* True when the ring holds at least one buffered byte. */
bool dspic33ak_uart_rx_isr_ready(
    dspic33ak_uart_instance_t inst);

/*
 * Pop one byte from the ring.
 *   DSPIC33AK_UART_ERR_INVALID_ARG  data == NULL
 *   DSPIC33AK_UART_ERR_RX_EMPTY     ring empty
 *   DSPIC33AK_UART_OK               one byte written to *data
 */
dspic33ak_uart_status_t dspic33ak_uart_rx_isr_read_byte(
    dspic33ak_uart_instance_t inst,
    uint8_t *data);

/* Drop buffered ring contents and drain the hardware RX FIFO. */
void dspic33ak_uart_rx_isr_flush(
    dspic33ak_uart_instance_t inst);

/* Snapshot the RX ISR ring runtime status counters (atomic vs the ISR). */
void dspic33ak_uart_rx_isr_status_get(
    dspic33ak_uart_instance_t inst,
    dspic33ak_uart_rx_isr_status_t *status);

/* Zero the RX ISR ring runtime status counters (atomic vs the ISR). */
void dspic33ak_uart_rx_isr_status_clear(
    dspic33ak_uart_instance_t inst);

/*
 * RX interrupt service routine body. Called from the application-owned
 * _UxRXInterrupt vector. This is an ordinary function, NOT an interrupt vector:
 * it does not carry the interrupt/context attribute and does not declare the
 * vector name. It clears the RX interrupt flag, drains the RX FIFO into the
 * ring, and counts/clears the latched RX error flags. No printf / blocking.
 */
void dspic33ak_uart_rx_irq_handler(
    dspic33ak_uart_instance_t inst);

/* ========================================================================== */
/* C++ Linkage                                                                */
/* ========================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* DSPIC33AK_UART_RX_ISR_RING_H */
