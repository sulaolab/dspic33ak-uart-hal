/* ========================================================================== */
/* Includes                                                                   */
/* ========================================================================== */

#include <xc.h>
#include "dspic33ak_uart_device.h"

/* ========================================================================== */
/* Module Overview                                                            */
/* ========================================================================== */

/*
 * Device / instance mapping layer.
 *
 * This is the only place that should know about U1CON / U2CON / U3CON / U4CON
 * symbol names and the matching CPU UART RX/TX interrupt flag, enable, and
 * priority mappings. Driver logic uses only the register pointer table returned
 * from dspic33ak_uart_get_device() and the internal priority setter functions.
 */

/* ========================================================================== */
/* Module Constants                                                           */
/* ========================================================================== */

static const dspic33ak_uart_device_t g_uart_devices[DSPIC33AK_UART_INST_COUNT] = {
#if defined(U1CON)
    [DSPIC33AK_UART_INST_1] = {
        .present = true,
        .regs = {
            .CON = &U1CON,
            .STAT = &U1STAT,
            .BRG = &U1BRG,
            .TXB = &U1TXB,
            .RXB = &U1RXB,
#if defined(_U1RXIF) && defined(_IFS3_U1RXIF_MASK)
            .irq_rx = { &IFS3, &IEC3, _IFS3_U1RXIF_MASK },
#endif
#if defined(_U1TXIF) && defined(_IFS3_U1TXIF_MASK)
            .irq_tx = { &IFS3, &IEC3, _IFS3_U1TXIF_MASK },
#endif
        },
    },
#else
    [DSPIC33AK_UART_INST_1] = { .present = false },
#endif

#if defined(U2CON)
    [DSPIC33AK_UART_INST_2] = {
        .present = true,
        .regs = {
            .CON = &U2CON,
            .STAT = &U2STAT,
            .BRG = &U2BRG,
            .TXB = &U2TXB,
            .RXB = &U2RXB,
#if defined(_U2RXIF) && defined(_IFS3_U2RXIF_MASK)
            .irq_rx = { &IFS3, &IEC3, _IFS3_U2RXIF_MASK },
#endif
#if defined(_U2TXIF) && defined(_IFS3_U2TXIF_MASK)
            .irq_tx = { &IFS3, &IEC3, _IFS3_U2TXIF_MASK },
#endif
        },
    },
#else
    [DSPIC33AK_UART_INST_2] = { .present = false },
#endif

#if defined(U3CON)
    [DSPIC33AK_UART_INST_3] = {
        .present = true,
        .regs = {
            .CON = &U3CON,
            .STAT = &U3STAT,
            .BRG = &U3BRG,
            .TXB = &U3TXB,
            .RXB = &U3RXB,
#if defined(_U3RXIF) && defined(_IFS3_U3RXIF_MASK)
            .irq_rx = { &IFS3, &IEC3, _IFS3_U3RXIF_MASK },
#endif
#if defined(_U3TXIF) && defined(_IFS3_U3TXIF_MASK)
            .irq_tx = { &IFS3, &IEC3, _IFS3_U3TXIF_MASK },
#endif
        },
    },
#else
    [DSPIC33AK_UART_INST_3] = { .present = false },
#endif

#if defined(U4CON)
    [DSPIC33AK_UART_INST_4] = {
        .present = true,
        .regs = {
            .CON = &U4CON,
            .STAT = &U4STAT,
            .BRG = &U4BRG,
            .TXB = &U4TXB,
            .RXB = &U4RXB,
#if defined(_U4RXIF) && defined(_IFS3_U4RXIF_MASK)
            .irq_rx = { &IFS3, &IEC3, _IFS3_U4RXIF_MASK },
#endif
#if defined(_U4TXIF) && defined(_IFS3_U4TXIF_MASK)
            .irq_tx = { &IFS3, &IEC3, _IFS3_U4TXIF_MASK },
#endif
        },
    },
#else
    [DSPIC33AK_UART_INST_4] = { .present = false },
#endif
};

/* ========================================================================== */
/* Internal API                                                               */
/* ========================================================================== */

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_get_device                                                  */
/* -------------------------------------------------------------------------- */
const dspic33ak_uart_device_t *dspic33ak_uart_get_device(
    dspic33ak_uart_instance_t inst)
{
    if ((unsigned)inst >= (unsigned)DSPIC33AK_UART_INST_COUNT) {
        return 0;
    }

    if (!g_uart_devices[inst].present) {
        return 0;
    }

    return &g_uart_devices[inst];
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_instance_is_present                                         */
/* -------------------------------------------------------------------------- */
bool dspic33ak_uart_instance_is_present(dspic33ak_uart_instance_t inst)
{
    return (dspic33ak_uart_get_device(inst) != 0);
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_device_set_rx_irq_priority                                  */
/* -------------------------------------------------------------------------- */
bool dspic33ak_uart_device_set_rx_irq_priority(
    dspic33ak_uart_instance_t inst,
    uint8_t priority)
{
    switch (inst) {
#if defined(_U1RXIP)
    case DSPIC33AK_UART_INST_1: _U1RXIP = priority; return true;
#endif
#if defined(_U2RXIP)
    case DSPIC33AK_UART_INST_2: _U2RXIP = priority; return true;
#endif
#if defined(_U3RXIP)
    case DSPIC33AK_UART_INST_3: _U3RXIP = priority; return true;
#endif
#if defined(_U4RXIP)
    case DSPIC33AK_UART_INST_4: _U4RXIP = priority; return true;
#endif
    default: break;
    }

    return false;
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_device_set_tx_irq_priority                                  */
/* -------------------------------------------------------------------------- */
bool dspic33ak_uart_device_set_tx_irq_priority(
    dspic33ak_uart_instance_t inst,
    uint8_t priority)
{
    switch (inst) {
#if defined(_U1TXIP)
    case DSPIC33AK_UART_INST_1: _U1TXIP = priority; return true;
#endif
#if defined(_U2TXIP)
    case DSPIC33AK_UART_INST_2: _U2TXIP = priority; return true;
#endif
#if defined(_U3TXIP)
    case DSPIC33AK_UART_INST_3: _U3TXIP = priority; return true;
#endif
#if defined(_U4TXIP)
    case DSPIC33AK_UART_INST_4: _U4TXIP = priority; return true;
#endif
    default: break;
    }

    return false;
}
