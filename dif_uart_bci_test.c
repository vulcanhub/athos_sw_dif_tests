// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "dif/dif_uart.h"

#include "dif/device.h"
#include "base/mmio.h"
#include "dif/hart.h"
#include "dif/check.h"
#include "dif/test_main.h"
#include "top/sw/autogen/top_athos.h"  // Generated.

static const uint8_t kSendData[] = "BCI DIF Test!";
  
uint8_t debugSendData[128];
uint8_t debugRecvData[128]; 

const test_config_t kTestConfig = {
    .can_clobber_uart = true,
};

bool test_main(void) {
  dif_uart_t uart;
  LOG_INFO("Running new BCI uart dif test");  
  CHECK(
      dif_uart_init(
          (dif_uart_params_t){
              .base_addr = mmio_region_from_addr(TOP_ATHOS_UART0_BASE_ADDR),
          },
          &uart) == kDifUartOk);
  CHECK(dif_uart_configure(&uart,
                           (dif_uart_config_t){
                               .baudrate = kUartBaudrate,
                               .clk_freq_hz = kClockFreqPeripheralHz,
                               .parity_enable = kDifUartToggleDisabled,
                               .parity = kDifUartParityEven,
                           }) == kDifUartConfigOk,
        "UART config failed!");

  CHECK(dif_uart_loopback_set(&uart, kDifUartLoopbackSystem,
                              kDifUartToggleEnabled) == kDifUartOk);
  CHECK(dif_uart_fifo_reset(&uart, kDifUartFifoResetAll) == kDifUartOk);

  // Send all bytes in `kSendData`, and check that they are received via
  // the loopback mechanism.
  CHECK(dif_uart_send_N_bytes(&uart, (uint8_t *) kSendData, sizeof(kSendData)) == kDifUartOk);

  uint8_t receive_bytes[128];

  CHECK(dif_uart_receive_N_bytes(&uart, (uint8_t *) receive_bytes, sizeof(kSendData)) == kDifUartOk);

  for (int i = 0; i < sizeof(kSendData); ++i) {
    CHECK(receive_bytes[i] == kSendData[i]);
    debugSendData[i] = kSendData[i];
    debugRecvData[i] = receive_bytes[i];
  }
  
  LOG_INFO("Completed Running BCI uart dif test");

  return true;
}
