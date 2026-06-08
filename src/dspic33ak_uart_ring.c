/* ========================================================================== */
/* Includes                                                                   */
/* ========================================================================== */

#include "dspic33ak_uart_ring.h"

/* ========================================================================== */
/* Module Overview                                                            */
/* ========================================================================== */

/*
 * Single-producer / single-consumer byte ring buffer.
 *
 * Index convention:
 *   rd == wr means empty.
 *
 * One slot is always left unused so that a full buffer is distinguishable from
 * an empty one. Usable capacity is therefore (size - 1) bytes.
 */

/* ========================================================================== */
/* Local Function Prototypes                                                  */
/* ========================================================================== */

static uint16_t ring_next_index(
    const dspic33ak_uart_ring_t *r,
    uint16_t idx);

/* ========================================================================== */
/* Public Functions                                                           */
/* ========================================================================== */

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_ring_init                                                   */
/* -------------------------------------------------------------------------- */
void dspic33ak_uart_ring_init(
    dspic33ak_uart_ring_t *r,
    uint8_t *buf,
    uint16_t size,
    dspic33ak_uart_ring_overflow_policy_t policy)
{
    if (r == 0) {
        return;
    }

    /* A usable ring needs a real buffer and at least 2 slots (1 kept empty). */
    if (buf == 0 || size < 2u) {
        r->buf = buf;
        r->size = 0u;
    } else {
        r->buf = buf;
        r->size = size;
    }

    r->rd = 0u;
    r->wr = 0u;
    r->overflow_count = 0u;
    r->overflow_policy = policy;
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_ring_available                                              */
/* -------------------------------------------------------------------------- */
size_t dspic33ak_uart_ring_available(const dspic33ak_uart_ring_t *r)
{
    if (r == 0 || r->size == 0u) {
        return 0u;
    }

    return (size_t)(((uint32_t)r->wr + (uint32_t)r->size - (uint32_t)r->rd) %
                    (uint32_t)r->size);
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_ring_push                                                   */
/* -------------------------------------------------------------------------- */
bool dspic33ak_uart_ring_push(dspic33ak_uart_ring_t *r, uint8_t data)
{
    uint16_t next;

    if (r == 0 || r->size == 0u || r->buf == 0) {
        return false;
    }

    next = ring_next_index(r, r->wr);

    if (next == r->rd) {
        /* Buffer is full. */
        r->overflow_count++;

        if (r->overflow_policy == DSPIC33AK_UART_RING_DROP_NEW) {
            return false;   /* discard the incoming byte */
        }

        /* DROP_OLD: discard the oldest byte to make room. */
        r->rd = ring_next_index(r, r->rd);
    }

    r->buf[r->wr] = data;
    r->wr = next;
    return true;
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_ring_pop                                                    */
/* -------------------------------------------------------------------------- */
bool dspic33ak_uart_ring_pop(dspic33ak_uart_ring_t *r, uint8_t *data)
{
    if (r == 0 || r->size == 0u || r->buf == 0 || data == 0) {
        return false;
    }

    if (r->rd == r->wr) {
        return false;   /* empty */
    }

    *data = r->buf[r->rd];
    r->rd = ring_next_index(r, r->rd);
    return true;
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_ring_flush                                                  */
/* -------------------------------------------------------------------------- */
void dspic33ak_uart_ring_flush(dspic33ak_uart_ring_t *r)
{
    if (r == 0) {
        return;
    }

    /* Drop all buffered data; keep the lifetime overflow counter intact. */
    r->rd = r->wr;
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_ring_overflow_count                                         */
/* -------------------------------------------------------------------------- */
uint32_t dspic33ak_uart_ring_overflow_count(const dspic33ak_uart_ring_t *r)
{
    if (r == 0) {
        return 0u;
    }

    return r->overflow_count;
}

/* ========================================================================== */
/* Local Functions                                                            */
/* ========================================================================== */

/* -------------------------------------------------------------------------- */
/* ring_next_index                                                            */
/* -------------------------------------------------------------------------- */
static uint16_t ring_next_index(
    const dspic33ak_uart_ring_t *r,
    uint16_t idx)
{
    uint16_t n = (uint16_t)(idx + 1u);

    if (n >= r->size) {
        n = 0u;
    }

    return n;
}
