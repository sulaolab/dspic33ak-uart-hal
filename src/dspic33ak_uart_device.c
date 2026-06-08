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
 * symbol names. Driver logic uses only the register pointer table returned
 * from dspic33ak_uart_get_device().
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
