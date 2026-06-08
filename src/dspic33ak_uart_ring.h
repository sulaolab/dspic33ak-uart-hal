#ifndef DSPIC33AK_UART_RING_H
#define DSPIC33AK_UART_RING_H

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
 * Simple single-producer / single-consumer byte ring buffer.
 *
 * One slot is kept empty so that rd == wr means empty. Usable capacity is
 * therefore (size - 1). The ring is independent of UART registers and can be
 * used by polling, interrupt, or integration-layer code.
 */

/* ========================================================================== */
/* Public Types                                                               */
/* ========================================================================== */

typedef enum {
    DSPIC33AK_UART_RING_DROP_NEW = 0,   /* on overflow, discard incoming byte */
    DSPIC33AK_UART_RING_DROP_OLD        /* on overflow, discard oldest byte   */
} dspic33ak_uart_ring_overflow_policy_t;

typedef struct {
    uint8_t *buf;
    uint16_t size;                 /* 0 => unusable (buf NULL or size < 2) */
    volatile uint16_t rd;
    volatile uint16_t wr;
    volatile uint32_t overflow_count;
    dspic33ak_uart_ring_overflow_policy_t overflow_policy;
} dspic33ak_uart_ring_t;

/* ========================================================================== */
/* Public API                                                                 */
/* ========================================================================== */

void dspic33ak_uart_ring_init(
    dspic33ak_uart_ring_t *r,
    uint8_t *buf,
    uint16_t size,
    dspic33ak_uart_ring_overflow_policy_t policy);

size_t dspic33ak_uart_ring_available(
    const dspic33ak_uart_ring_t *r);

bool dspic33ak_uart_ring_push(
    dspic33ak_uart_ring_t *r,
    uint8_t data);

bool dspic33ak_uart_ring_pop(
    dspic33ak_uart_ring_t *r,
    uint8_t *data);

void dspic33ak_uart_ring_flush(
    dspic33ak_uart_ring_t *r);

uint32_t dspic33ak_uart_ring_overflow_count(
    const dspic33ak_uart_ring_t *r);

/* ========================================================================== */
/* C++ Linkage                                                                */
/* ========================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* DSPIC33AK_UART_RING_H */
