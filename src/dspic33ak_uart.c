/* ========================================================================== */
/* Includes                                                                   */
/* ========================================================================== */

#include <string.h>

#include "dspic33ak_uart.h"
#include "dspic33ak_uart_rx_isr_ring.h"
#include "dspic33ak_uart_device.h"
#include "dspic33ak_uart_reg.h"

/* ========================================================================== */
/* Module Variables                                                           */
/* ========================================================================== */

static uint32_t g_timeout_ms[DSPIC33AK_UART_INST_COUNT];
static dspic33ak_uart_get_ms_fn g_get_ms[DSPIC33AK_UART_INST_COUNT];
static bool g_initialized[DSPIC33AK_UART_INST_COUNT];

/* Per-instance RX backend (defaults to polling; set from config at init). The
 * RX query/read/flush API consults this to pick the FIFO or the ISR ring. */
static dspic33ak_uart_rx_mode_t g_rx_mode[DSPIC33AK_UART_INST_COUNT];

/* ========================================================================== */
/* Local Function Prototypes                                                  */
/* ========================================================================== */

static bool uart_inst_is_valid(dspic33ak_uart_instance_t inst);
static dspic33ak_uart_status_t uart_get_regs(
    dspic33ak_uart_instance_t inst,
    const dspic33ak_uart_regs_t **regs);
static dspic33ak_uart_status_t uart_require_initialized(
    dspic33ak_uart_instance_t inst,
    const dspic33ak_uart_regs_t **regs);
static dspic33ak_uart_status_t uart_check_initialized(
    dspic33ak_uart_instance_t inst);
static uint32_t uart_calc_brg(
    uint32_t uart_clk_hz,
    uint32_t baudrate);
static bool uart_timeout_enabled(
    dspic33ak_uart_instance_t inst);
static uint32_t uart_timeout_start_ms(
    dspic33ak_uart_instance_t inst);
static bool uart_timeout_expired(
    dspic33ak_uart_instance_t inst,
    uint32_t start_ms);

/* ========================================================================== */
/* Public Functions                                                           */
/* ========================================================================== */

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_init                                                        */
/* -------------------------------------------------------------------------- */
dspic33ak_uart_status_t dspic33ak_uart_init(
    dspic33ak_uart_instance_t inst,
    const dspic33ak_uart_config_t *config)
{
    const dspic33ak_uart_regs_t *r;
    dspic33ak_uart_status_t st;
    uint32_t brg;

    if (!uart_inst_is_valid(inst)) {
        return DSPIC33AK_UART_ERR_INVALID_ARG;
    }

    if (config == 0 || config->uart_clk_hz == 0u || config->baudrate == 0u) {
        return DSPIC33AK_UART_ERR_INVALID_ARG;
    }

    /* Current implementation supports 8N1 (UxCON MODE=0 reset default). */
    if (config->data_bits != 8u ||
        config->stop_bits != 1u ||
        config->parity != DSPIC33AK_UART_PARITY_NONE) {
        return DSPIC33AK_UART_ERR_UNSUPPORTED;
    }

    /* RX backend must be a known mode. Reject unknown/uninitialized rx_mode here,
     * before touching any register, so a bad config never silently falls through
     * to polling and never leaves the UART peripheral half-configured. */
    if ((config->rx_mode != DSPIC33AK_UART_RX_MODE_POLLING) &&
        (config->rx_mode != DSPIC33AK_UART_RX_MODE_ISR_RING)) {
        return DSPIC33AK_UART_ERR_INVALID_ARG;
    }

    st = uart_get_regs(inst, &r);
    if (st != DSPIC33AK_UART_OK) {
        return st;
    }

    brg = uart_calc_brg(config->uart_clk_hz, config->baudrate);
    if (brg == 0u) {
        return DSPIC33AK_UART_ERR_INVALID_ARG;
    }

    /* Turn the module off and start from a known 8N1 state. */
    *r->CON = 0u;
    *r->STAT = 0u;

    /* Clock: select CLKGEN8 (CLKSEL) and fractional baud mode (CLKMOD; BRGS stays
     * 0). This HAL assumes CLKGEN8 as the UART clock source; the board/application
     * must have brought CLKGEN8 up before init, and config.uart_clk_hz must be the
     * CLKGEN8 frequency used for the baud-divisor calculation. */
    dspic33ak_uart_reg_set(r->CON, DSPIC33AK_UART_CON_CLKSEL);
    dspic33ak_uart_reg_set(r->CON, DSPIC33AK_UART_CON_CLKMOD);

    *r->BRG = brg;

    /* TX FIFO write enable. */
    dspic33ak_uart_reg_set(r->STAT, DSPIC33AK_UART_STAT_TXWRE);

    if (config->enable_tx) {
        dspic33ak_uart_reg_set(r->CON, DSPIC33AK_UART_CON_TXEN);
    }
    if (config->enable_rx) {
        dspic33ak_uart_reg_set(r->CON, DSPIC33AK_UART_CON_RXEN);
    }

    /* Enable the module last. */
    dspic33ak_uart_reg_set(r->CON, DSPIC33AK_UART_CON_ON);

    g_timeout_ms[inst] = config->timeout_ms;
    g_get_ms[inst] = config->get_ms;
    g_initialized[inst] = true;
    g_rx_mode[inst] = config->rx_mode;

    /*
     * RX backend setup. ISR ring mode configures and enables the interrupt-driven
     * RX ring now. dspic33ak_uart_rx_isr_config() requires the instance to be
     * initialized, so this runs after g_initialized = true. On failure, unwind via
     * deinit so a half-initialized instance is never left behind.
     */
    if (config->rx_mode == DSPIC33AK_UART_RX_MODE_ISR_RING) {
        dspic33ak_uart_rx_isr_config_t rx_cfg;

        rx_cfg.buffer       = config->rx_ring_buffer;
        rx_cfg.buffer_size  = config->rx_ring_buffer_size;
        rx_cfg.irq_priority = config->rx_irq_priority;

        st = dspic33ak_uart_rx_isr_config(inst, &rx_cfg);
        if (st != DSPIC33AK_UART_OK) {
            (void)dspic33ak_uart_deinit(inst);
            return st;
        }

        st = dspic33ak_uart_rx_isr_enable(inst);
        if (st != DSPIC33AK_UART_OK) {
            (void)dspic33ak_uart_deinit(inst);
            return st;
        }
    }

    return DSPIC33AK_UART_OK;
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_deinit                                                      */
/* -------------------------------------------------------------------------- */
dspic33ak_uart_status_t dspic33ak_uart_deinit(
    dspic33ak_uart_instance_t inst)
{
    const dspic33ak_uart_regs_t *r;
    dspic33ak_uart_status_t st;

    if (!uart_inst_is_valid(inst)) {
        return DSPIC33AK_UART_ERR_INVALID_ARG;
    }

    st = uart_get_regs(inst, &r);
    if (st != DSPIC33AK_UART_OK) {
        return st;
    }

    /* Stop the RX ISR first (if this instance ran the ring) so it cannot touch
     * the FIFO mid-teardown. Safe/no-op in polling mode. */
    if (g_rx_mode[inst] == DSPIC33AK_UART_RX_MODE_ISR_RING) {
        (void)dspic33ak_uart_rx_isr_disable(inst);
    }

    dspic33ak_uart_reg_clear(r->CON,
                             DSPIC33AK_UART_CON_TXEN |
                             DSPIC33AK_UART_CON_RXEN |
                             DSPIC33AK_UART_CON_ON);

    g_timeout_ms[inst] = 0u;
    g_get_ms[inst] = 0;
    g_initialized[inst] = false;
    g_rx_mode[inst] = DSPIC33AK_UART_RX_MODE_POLLING;

    return DSPIC33AK_UART_OK;
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_is_present                                                  */
/* -------------------------------------------------------------------------- */
bool dspic33ak_uart_is_present(dspic33ak_uart_instance_t inst)
{
    return dspic33ak_uart_instance_is_present(inst);
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_is_initialized                                              */
/* -------------------------------------------------------------------------- */
bool dspic33ak_uart_is_initialized(dspic33ak_uart_instance_t inst)
{
    if (!uart_inst_is_valid(inst)) {
        return false;
    }

    return g_initialized[inst];
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_rx_ready                                                    */
/* -------------------------------------------------------------------------- */
bool dspic33ak_uart_rx_ready(dspic33ak_uart_instance_t inst)
{
    const dspic33ak_uart_regs_t *r;

    if (uart_require_initialized(inst, &r) != DSPIC33AK_UART_OK) {
        return false;
    }

    /* ISR ring backend: readiness reflects buffered ring contents. */
    if (g_rx_mode[inst] == DSPIC33AK_UART_RX_MODE_ISR_RING) {
        return dspic33ak_uart_rx_isr_ready(inst);
    }

    /* Polling backend: RX has data when the RX FIFO is NOT empty. */
    return !dspic33ak_uart_reg_is_set(r->STAT, DSPIC33AK_UART_STAT_RXBE);
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_tx_ready                                                    */
/* -------------------------------------------------------------------------- */
bool dspic33ak_uart_tx_ready(dspic33ak_uart_instance_t inst)
{
    const dspic33ak_uart_regs_t *r;

    if (uart_require_initialized(inst, &r) != DSPIC33AK_UART_OK) {
        return false;
    }

    /* TX can accept a byte when the TX buffer is NOT full. */
    return !dspic33ak_uart_reg_is_set(r->STAT, DSPIC33AK_UART_STAT_TXBF);
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_tx_done                                                     */
/* -------------------------------------------------------------------------- */
bool dspic33ak_uart_tx_done(dspic33ak_uart_instance_t inst)
{
    const dspic33ak_uart_regs_t *r;

    if (uart_require_initialized(inst, &r) != DSPIC33AK_UART_OK) {
        return false;
    }

    return dspic33ak_uart_reg_is_set(r->STAT, DSPIC33AK_UART_STAT_TXMTIF) &&
           dspic33ak_uart_reg_is_set(r->STAT, DSPIC33AK_UART_STAT_TXBE);
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_write_byte                                                  */
/* -------------------------------------------------------------------------- */
dspic33ak_uart_status_t dspic33ak_uart_write_byte(
    dspic33ak_uart_instance_t inst,
    uint8_t data)
{
    const dspic33ak_uart_regs_t *r;
    dspic33ak_uart_status_t st;
    uint32_t start_ms;

    st = uart_require_initialized(inst, &r);
    if (st != DSPIC33AK_UART_OK) {
        return st;
    }

    start_ms = uart_timeout_start_ms(inst);
    while (dspic33ak_uart_reg_is_set(r->STAT, DSPIC33AK_UART_STAT_TXBF)) {
        if (uart_timeout_enabled(inst) && uart_timeout_expired(inst, start_ms)) {
            return DSPIC33AK_UART_ERR_TIMEOUT;
        }
    }

    *r->TXB = (uint32_t)data;
    return DSPIC33AK_UART_OK;
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_read_byte                                                   */
/* -------------------------------------------------------------------------- */
dspic33ak_uart_status_t dspic33ak_uart_read_byte(
    dspic33ak_uart_instance_t inst,
    uint8_t *data)
{
    const dspic33ak_uart_regs_t *r;
    dspic33ak_uart_status_t st;

    if (data == 0) {
        return DSPIC33AK_UART_ERR_INVALID_ARG;
    }

    st = uart_require_initialized(inst, &r);
    if (st != DSPIC33AK_UART_OK) {
        return st;
    }

    /* ISR ring backend: pop from the software ring (the ISR owns the FIFO). */
    if (g_rx_mode[inst] == DSPIC33AK_UART_RX_MODE_ISR_RING) {
        return dspic33ak_uart_rx_isr_read_byte(inst, data);
    }

    /* Polling backend: read directly from the RX FIFO. */
    if (dspic33ak_uart_reg_is_set(r->STAT, DSPIC33AK_UART_STAT_RXBE)) {
        return DSPIC33AK_UART_ERR_RX_EMPTY;
    }

    *data = (uint8_t)(*r->RXB);
    return DSPIC33AK_UART_OK;
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_write                                                       */
/* -------------------------------------------------------------------------- */
size_t dspic33ak_uart_write(
    dspic33ak_uart_instance_t inst,
    const void *data,
    size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    size_t i;

    if (len != 0u && data == 0) {
        return 0u;
    }

    if (uart_check_initialized(inst) != DSPIC33AK_UART_OK) {
        return 0u;
    }

    for (i = 0u; i < len; i++) {
        if (dspic33ak_uart_write_byte(inst, p[i]) != DSPIC33AK_UART_OK) {
            break;
        }
    }

    return i;
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_read                                                        */
/* -------------------------------------------------------------------------- */
size_t dspic33ak_uart_read(
    dspic33ak_uart_instance_t inst,
    void *data,
    size_t len)
{
    uint8_t *p = (uint8_t *)data;
    size_t i;

    if (len != 0u && data == 0) {
        return 0u;
    }

    if (uart_check_initialized(inst) != DSPIC33AK_UART_OK) {
        return 0u;
    }

    for (i = 0u; i < len; i++) {
        if (dspic33ak_uart_read_byte(inst, &p[i]) != DSPIC33AK_UART_OK) {
            break;
        }
    }

    return i;
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_rx_flush                                                    */
/* -------------------------------------------------------------------------- */
void dspic33ak_uart_rx_flush(dspic33ak_uart_instance_t inst)
{
    const dspic33ak_uart_regs_t *r;

    if (uart_require_initialized(inst, &r) != DSPIC33AK_UART_OK) {
        return;
    }

    /* ISR ring backend: flush the ring (it also drains the hardware FIFO). */
    if (g_rx_mode[inst] == DSPIC33AK_UART_RX_MODE_ISR_RING) {
        dspic33ak_uart_rx_isr_flush(inst);
        return;
    }

    /* Polling backend: drain the hardware RX FIFO and clear the overflow flag. */
    while (!dspic33ak_uart_reg_is_set(r->STAT, DSPIC33AK_UART_STAT_RXBE)) {
        (void)(*r->RXB);
    }

    if (dspic33ak_uart_reg_is_set(r->STAT, DSPIC33AK_UART_STAT_RXFOIF)) {
        dspic33ak_uart_reg_clear(r->STAT, DSPIC33AK_UART_STAT_RXFOIF);
    }
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_rx_status_get                                               */
/* -------------------------------------------------------------------------- */
dspic33ak_uart_status_t dspic33ak_uart_rx_status_get(
    dspic33ak_uart_instance_t inst,
    dspic33ak_uart_rx_status_t *status)
{
    dspic33ak_uart_status_t st;

    if (status == 0) {
        return DSPIC33AK_UART_ERR_INVALID_ARG;
    }

    st = uart_check_initialized(inst);
    if (st != DSPIC33AK_UART_OK) {
        return st;
    }

    memset(status, 0, sizeof(*status));
    status->rx_mode = g_rx_mode[inst];

    /* ISR ring backend: copy the ring counters. Polling backend keeps no
     * counters, so the zeroed snapshot above is the result. */
    if (g_rx_mode[inst] == DSPIC33AK_UART_RX_MODE_ISR_RING) {
        dspic33ak_uart_rx_isr_status_t isr_status;

        dspic33ak_uart_rx_isr_status_get(inst, &isr_status);

        status->rx_isr_count            = isr_status.rx_isr_count;
        status->rx_byte_count           = isr_status.rx_byte_count;
        status->rx_fifo_overflow_count  = isr_status.rx_fifo_overflow_count;
        status->framing_error_count     = isr_status.framing_error_count;
        status->parity_error_count      = isr_status.parity_error_count;
        status->autobaud_overflow_count = isr_status.autobaud_overflow_count;
        status->tx_collision_count      = isr_status.tx_collision_count;
        status->rx_ring_overflow_count  = isr_status.rx_ring_overflow_count;
        status->rx_max_drain_count      = isr_status.rx_max_drain_count;
    }

    return DSPIC33AK_UART_OK;
}

/* -------------------------------------------------------------------------- */
/* dspic33ak_uart_rx_status_clear                                             */
/* -------------------------------------------------------------------------- */
dspic33ak_uart_status_t dspic33ak_uart_rx_status_clear(
    dspic33ak_uart_instance_t inst)
{
    dspic33ak_uart_status_t st;

    st = uart_check_initialized(inst);
    if (st != DSPIC33AK_UART_OK) {
        return st;
    }

    /* ISR ring backend: clear the ring counters. Polling backend has none. */
    if (g_rx_mode[inst] == DSPIC33AK_UART_RX_MODE_ISR_RING) {
        dspic33ak_uart_rx_isr_status_clear(inst);
    }

    return DSPIC33AK_UART_OK;
}

/* ========================================================================== */
/* Local Functions                                                            */
/* ========================================================================== */

/* -------------------------------------------------------------------------- */
/* uart_inst_is_valid                                                         */
/* -------------------------------------------------------------------------- */
static bool uart_inst_is_valid(dspic33ak_uart_instance_t inst)
{
    return ((unsigned)inst < (unsigned)DSPIC33AK_UART_INST_COUNT);
}

/* -------------------------------------------------------------------------- */
/* uart_get_regs                                                              */
/* -------------------------------------------------------------------------- */
static dspic33ak_uart_status_t uart_get_regs(
    dspic33ak_uart_instance_t inst,
    const dspic33ak_uart_regs_t **regs)
{
    const dspic33ak_uart_device_t *dev;

    if (regs == 0) {
        return DSPIC33AK_UART_ERR_INVALID_ARG;
    }

    if (!uart_inst_is_valid(inst)) {
        return DSPIC33AK_UART_ERR_INVALID_ARG;
    }

    dev = dspic33ak_uart_get_device(inst);
    if (dev == 0) {
        return DSPIC33AK_UART_ERR_NOT_PRESENT;
    }

    *regs = &dev->regs;
    return DSPIC33AK_UART_OK;
}

/* -------------------------------------------------------------------------- */
/* uart_require_initialized                                                   */
/* -------------------------------------------------------------------------- */
static dspic33ak_uart_status_t uart_require_initialized(
    dspic33ak_uart_instance_t inst,
    const dspic33ak_uart_regs_t **regs)
{
    dspic33ak_uart_status_t st;

    if (!uart_inst_is_valid(inst)) {
        return DSPIC33AK_UART_ERR_INVALID_ARG;
    }

    st = uart_get_regs(inst, regs);
    if (st != DSPIC33AK_UART_OK) {
        return st;
    }

    if (!g_initialized[inst]) {
        return DSPIC33AK_UART_ERR_NOT_INITIALIZED;
    }

    return DSPIC33AK_UART_OK;
}

/* -------------------------------------------------------------------------- */
/* uart_check_initialized                                                     */
/* -------------------------------------------------------------------------- */
static dspic33ak_uart_status_t uart_check_initialized(
    dspic33ak_uart_instance_t inst)
{
    const dspic33ak_uart_regs_t *r;
    return uart_require_initialized(inst, &r);
}

/* -------------------------------------------------------------------------- */
/* uart_calc_brg                                                              */
/* -------------------------------------------------------------------------- */
static uint32_t uart_calc_brg(
    uint32_t uart_clk_hz,
    uint32_t baudrate)
{
    uint64_t div;

    if (baudrate == 0u) {
        return 0u;
    }

    /*
     * Fractional mode: BRG = round(uart_clk_hz / baudrate) - 1.
     * 64-bit math avoids overflow on faster clocks.
     */
    div = ((uint64_t)uart_clk_hz + ((uint64_t)baudrate / 2u)) /
          (uint64_t)baudrate;
    if (div == 0u) {
        return 0u;
    }

    return (uint32_t)(div - 1u);
}

/* -------------------------------------------------------------------------- */
/* uart_timeout_enabled                                                       */
/* -------------------------------------------------------------------------- */
static bool uart_timeout_enabled(dspic33ak_uart_instance_t inst)
{
    return uart_inst_is_valid(inst) &&
           (g_get_ms[inst] != 0) &&
           (g_timeout_ms[inst] != 0u);
}

/* -------------------------------------------------------------------------- */
/* uart_timeout_start_ms                                                      */
/* -------------------------------------------------------------------------- */
static uint32_t uart_timeout_start_ms(dspic33ak_uart_instance_t inst)
{
    if (!uart_timeout_enabled(inst)) {
        return 0u;
    }

    return g_get_ms[inst]();
}

/* -------------------------------------------------------------------------- */
/* uart_timeout_expired                                                       */
/* -------------------------------------------------------------------------- */
static bool uart_timeout_expired(
    dspic33ak_uart_instance_t inst,
    uint32_t start_ms)
{
    uint32_t now;

    if (!uart_timeout_enabled(inst)) {
        return false;
    }

    now = g_get_ms[inst]();
    return ((uint32_t)(now - start_ms) >= g_timeout_ms[inst]);
}
