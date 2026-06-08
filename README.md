# dspic33ak-uart-hal

Small, readable UART HAL for Microchip dsPIC33AK devices.

This repository provides a reusable byte-stream UART driver with a clean public API. The public API avoids exposing XC-DSC / DFP bitfield types, while the device-specific register mapping is isolated in small internal files.

## Status

Initial public release candidate.

This HAL was developed and smoke-tested on a dsPIC33AK512MPS512 project using UART1. The project-specific compatibility, stdio retargeting, and RX ISR ring layers used during bring-up are intentionally not part of this repository.

## Repository layout

```text
src/
  dspic33ak_uart.h          Public UART HAL API
  dspic33ak_uart.c          UART HAL implementation
  dspic33ak_uart_device.h   Device instance mapping interface
  dspic33ak_uart_device.c   Device instance mapping implementation
  dspic33ak_uart_reg.h      Internal register / bit-mask helper definitions
  dspic33ak_uart_ring.h     Small byte ring utility
  dspic33ak_uart_ring.c     Small byte ring utility implementation
```

## Design policy

- Public API does not expose XC-DSC / DFP bitfield types.
- Board-specific PPS / GPIO / clock routing stays outside this HAL.
- `printf()`, `read()`, `write()`, and other stdio retargeting stay outside this HAL.
- Application console / command parsing stays outside this HAL.
- Interrupt vector ownership stays outside this HAL. A project may build an ISR ring or DMA layer on top of this HAL, but the vector name itself is project/integration-specific.
- Public functions are placed near the top of each source file. Static helper functions are placed near the bottom, with only prototypes near the top when needed.

## Public API overview

The main public header is:

```c
#include "dspic33ak_uart.h"
```

Core API groups:

```text
Initialization:
  dspic33ak_uart_init()
  dspic33ak_uart_deinit()
  dspic33ak_uart_is_present()
  dspic33ak_uart_is_initialized()

Status:
  dspic33ak_uart_rx_ready()
  dspic33ak_uart_tx_ready()
  dspic33ak_uart_tx_done()

Byte I/O:
  dspic33ak_uart_write_byte()
  dspic33ak_uart_read_byte()

Buffer helpers:
  dspic33ak_uart_write()
  dspic33ak_uart_read()

RX cleanup:
  dspic33ak_uart_rx_flush()
```

## Minimal usage example

```c
#include "dspic33ak_uart.h"

static uint32_t app_get_ms(void)
{
    /* Return a monotonic millisecond tick, or remove this and set get_ms = NULL. */
    return 0u;
}

void app_uart_init(void)
{
    dspic33ak_uart_config_t cfg;

    cfg.uart_clk_hz = 100000000u;
    cfg.baudrate    = 115200u;
    cfg.timeout_ms  = 10u;
    cfg.get_ms      = app_get_ms;
    cfg.data_bits   = 8u;
    cfg.stop_bits   = 1u;
    cfg.parity      = DSPIC33AK_UART_PARITY_NONE;
    cfg.enable_tx   = true;
    cfg.enable_rx   = true;

    /* Board-level PPS / pin / clock setup should be completed before this call. */
    (void)dspic33ak_uart_init(DSPIC33AK_UART_INST_1, &cfg);
}

void app_uart_poll(void)
{
    uint8_t ch;

    if (dspic33ak_uart_read_byte(DSPIC33AK_UART_INST_1, &ch) == DSPIC33AK_UART_OK) {
        (void)dspic33ak_uart_write_byte(DSPIC33AK_UART_INST_1, ch); /* echo */
    }
}
```

## Build notes

Add these C files to your project:

```text
src/dspic33ak_uart.c
src/dspic33ak_uart_device.c
src/dspic33ak_uart_ring.c
```

Add `src/` to your include path.

The header `dspic33ak_uart_reg.h` is an internal helper header used by the HAL implementation. It is part of the source distribution, but user application code should normally include only `dspic33ak_uart.h`.

## Scope and limitations

This repository provides a blocking/polling UART byte-stream HAL. It does not provide a complete board support package.

You still need project-specific code for:

- Peripheral clock routing.
- PPS input/output routing.
- GPIO direction / analog-disable setup.
- stdio retargeting.
- RX interrupt ring or DMA ownership, if required.
- Application console or command parser.

## License

MIT License. See [LICENSE](LICENSE).
