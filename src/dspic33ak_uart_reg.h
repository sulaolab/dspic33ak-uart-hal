#ifndef DSPIC33AK_UART_REG_H
#define DSPIC33AK_UART_REG_H

/* ========================================================================== */
/* Includes                                                                   */
/* ========================================================================== */

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================== */
/* Module Overview                                                            */
/* ========================================================================== */

/*
 * Internal register helper layer.
 *
 * This file intentionally uses 32-bit register pointers and bit masks instead
 * of XC-DSC bitfield structures such as U1CONbits. This keeps compiler / DFP
 * bitfield details away from the public UART API.
 *
 * Bit positions were checked against:
 *   Microchip.dsPIC33AK-MP_DFP.1.3.185
 *   xc16/support/dsPIC33A/h/p33AK512MPS512.h
 *   _U1CON_*_POSITION / _U1STAT_*_POSITION macros
 */

/* ========================================================================== */
/* Internal Types                                                             */
/* ========================================================================== */

typedef struct {
    volatile uint32_t *CON;
    volatile uint32_t *STAT;
    volatile uint32_t *BRG;
    volatile uint32_t *TXB;
    volatile uint32_t *RXB;
} dspic33ak_uart_regs_t;

/* ========================================================================== */
/* Register Bit Masks                                                         */
/* ========================================================================== */

/* UxCON bits (enable + baud / clock control) */
#define DSPIC33AK_UART_CON_RXEN     (1UL << 4)   /* UxCONbits.RXEN   (pos 0x04) */
#define DSPIC33AK_UART_CON_TXEN     (1UL << 5)   /* UxCONbits.TXEN   (pos 0x05) */
#define DSPIC33AK_UART_CON_BRGS     (1UL << 7)   /* UxCONbits.BRGS   (pos 0x07) */
#define DSPIC33AK_UART_CON_ON       (1UL << 15)  /* UxCONbits.ON     (pos 0x0F) */
#define DSPIC33AK_UART_CON_CLKSEL   (1UL << 25)  /* UxCONbits.CLKSEL (pos 0x19) */
#define DSPIC33AK_UART_CON_CLKMOD   (1UL << 27)  /* UxCONbits.CLKMOD (pos 0x1B) */

/* UxSTAT bits (status used by the byte-stream API and the RX ISR ring) */
#define DSPIC33AK_UART_STAT_TXCIF   (1UL << 0)   /* UxSTATbits.TXCIF  (pos 0x00) */
#define DSPIC33AK_UART_STAT_RXFOIF  (1UL << 1)   /* UxSTATbits.RXFOIF (pos 0x01) */
#define DSPIC33AK_UART_STAT_FERIF   (1UL << 3)   /* UxSTATbits.FERIF  (pos 0x03) */
#define DSPIC33AK_UART_STAT_ABDOVIF (1UL << 5)   /* UxSTATbits.ABDOVIF(pos 0x05) */
#define DSPIC33AK_UART_STAT_PERIF   (1UL << 6)   /* UxSTATbits.PERIF  (pos 0x06) */
#define DSPIC33AK_UART_STAT_TXMTIF  (1UL << 7)   /* UxSTATbits.TXMTIF (pos 0x07) */
#define DSPIC33AK_UART_STAT_RXBE    (1UL << 17)  /* UxSTATbits.RXBE   (pos 0x11) */
#define DSPIC33AK_UART_STAT_TXBF    (1UL << 20)  /* UxSTATbits.TXBF   (pos 0x14) */
#define DSPIC33AK_UART_STAT_TXBE    (1UL << 21)  /* UxSTATbits.TXBE   (pos 0x15) */
#define DSPIC33AK_UART_STAT_TXWRE   (1UL << 23)  /* UxSTATbits.TXWRE  (pos 0x17) */
#define DSPIC33AK_UART_STAT_RXWM    (1UL << 24)  /* UxSTATbits.RXWM   (pos 0x18) */

/* ========================================================================== */
/* Internal Inline Helpers                                                    */
/* ========================================================================== */

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_reg_set                                                     */
/* -------------------------------------------------------------------------- */
static inline void dspic33ak_uart_reg_set(volatile uint32_t *reg, uint32_t mask)
{
    *reg |= mask;
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_reg_clear                                                   */
/* -------------------------------------------------------------------------- */
static inline void dspic33ak_uart_reg_clear(volatile uint32_t *reg, uint32_t mask)
{
    *reg &= ~mask;
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_reg_is_set                                                  */
/* -------------------------------------------------------------------------- */
static inline bool dspic33ak_uart_reg_is_set(volatile uint32_t *reg, uint32_t mask)
{
    return ((*reg & mask) != 0u);
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_reg_write_field                                             */
/* -------------------------------------------------------------------------- */
static inline void dspic33ak_uart_reg_write_field(
    volatile uint32_t *reg,
    uint32_t mask,
    uint32_t value)
{
    *reg = (*reg & ~mask) | (value & mask);
}

#endif /* DSPIC33AK_UART_REG_H */
