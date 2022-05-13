// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "dif/dif_plic.h"

#include "base/mmio.h"
#include "dif/dif_uart.h"
#include "dif/handler.h"
#include "dif/irq.h"
#include "dif/hart.h"
#include "dif/log.h"
#include "dif/check.h"
#include "dif/test_main.h"
#include "dif/test_status.h"

#include "top/sw/autogen/top_athos.h"  // Generated.

static const uint32_t kPlicTarget = kTopAthosPlicTargetIbex0;

static dif_plic_t plic0;
static dif_uart_t uart0;

// These flags are used in the test routine to verify that a corresponding
// interrupt has elapsed, and has been serviced. These are declared as volatile
// since they are referenced in the ISR routine as well as in the main program
// flow.
static volatile bool uart_rx_overflow_handled;
static volatile bool uart_tx_empty_handled;
//edited
static volatile bool uart_rx_frame_err_handled;
static volatile bool uart_tx_watermark_handled;
static volatile bool uart_rx_watermark_handled;
static volatile bool uart_rx_break_err_handled;
static volatile bool uart_rx_timeout_handled;
static volatile bool uart_rx_parity_err_handled;
//edited

/**
 * UART interrupt handler
 *
 * Services UART interrupts, sets the appropriate flags that are used to
 * determine success or failure of the test.
 */
static void handle_uart_isr(const dif_plic_irq_id_t interrupt_id) {
  // NOTE: This initialization is superfluous, since the `default` case below
  // is effectively noreturn, but the compiler is unable to prove this.
  dif_uart_irq_t uart_irq = 0;

  switch (interrupt_id) {
    //edited
    case kTopAthosPlicIrqIdUart0RxParityErr:
      CHECK(!uart_rx_parity_err_handled,
            "UART rx_parity_err IRQ asserted more than once");

      uart_irq = kDifUartIrqRxParityErr;
      uart_rx_parity_err_handled = true;
      break;
    case kTopAthosPlicIrqIdUart0RxTimeout:
      CHECK(!uart_rx_timeout_handled,
            "UART rx_timeout IRQ asserted more than once");

      uart_irq = kDifUartIrqRxTimeout;
      uart_rx_timeout_handled = true;
      break;
    case kTopAthosPlicIrqIdUart0RxBreakErr:
      CHECK(!uart_rx_break_err_handled,
            "UART rx_break_err IRQ asserted more than once");

      uart_irq = kDifUartIrqRxBreakErr;
      uart_rx_break_err_handled = true;
      break;
    case kTopAthosPlicIrqIdUart0RxFrameErr:
      CHECK(!uart_rx_frame_err_handled,
            "UART RX framing error IRQ asserted more than once");

      uart_irq = kDifUartIrqRxFrameErr;
      uart_rx_frame_err_handled = true;
    //edited
      break;
    case kTopAthosPlicIrqIdUart0RxOverflow:
      CHECK(!uart_rx_overflow_handled,
            "UART RX overflow IRQ asserted more than once");

      uart_irq = kDifUartIrqRxOverflow;
      uart_rx_overflow_handled = true;
      break;
    case kTopAthosPlicIrqIdUart0TxEmpty:
      CHECK(!uart_tx_empty_handled,
            "UART TX empty IRQ asserted more than once");

      uart_irq = kDifUartIrqTxEmpty;
      uart_tx_empty_handled = true;
      break;
    //edited
    case kTopAthosPlicIrqIdUart0RxWatermark:
      CHECK(!uart_rx_watermark_handled,
            "UART rx_watermark IRQ asserted more than once");

      uart_irq = kDifUartIrqRxWatermark;
      uart_rx_watermark_handled = true;
      break;
    case kTopAthosPlicIrqIdUart0TxWatermark:
      CHECK(!uart_tx_watermark_handled,
            "UART tx_watermark IRQ asserted more than once");

      uart_irq = kDifUartIrqTxWatermark;
      uart_tx_watermark_handled = true;
      break;
    //edited
     
    default:
      LOG_FATAL("ISR is not implemented!");
      test_status_set(kTestStatusFailed);
  }

  CHECK(dif_uart_irq_acknowledge(&uart0, uart_irq) == kDifUartOk,
        "ISR failed to clear IRQ!");
}

/**
 * External interrupt handler
 *
 * Handles all peripheral interrupts on Ibex. PLIC asserts an external interrupt
 * line to the CPU, which results in a call to this handler. This handler
 * overrides the default implementation, and prototype for this handler must
 * include appropriate attributes.
 */
void handler_irq_external(void) {
  // Claim the IRQ by reading the Ibex specific CC register.
  dif_plic_irq_id_t interrupt_id;
  CHECK(dif_plic_irq_claim(&plic0, kPlicTarget, &interrupt_id) == kDifPlicOk,
        "ISR is not implemented!");

  // Check if the interrupted peripheral is UART.
  top_athos_plic_peripheral_t peripheral_id =
      top_athos_plic_interrupt_for_peripheral[interrupt_id];
  CHECK(peripheral_id == kTopAthosPlicPeripheralUart0,
        "ISR interrupted peripheral is not UART!");
  handle_uart_isr(interrupt_id);

  // Complete the IRQ by writing the IRQ source to the Ibex specific CC
  // register.
  CHECK(dif_plic_irq_complete(&plic0, kPlicTarget, &interrupt_id) == kDifPlicOk,
        "Unable to complete the IRQ request!");
}

static void uart_initialise(mmio_region_t base_addr, dif_uart_t *uart) {
  CHECK(dif_uart_init(
            (dif_uart_params_t){
                .base_addr = base_addr,
            },
            uart) == kDifUartOk);
  CHECK(dif_uart_configure(uart,
                           (dif_uart_config_t){
                               .baudrate = kUartBaudrate,
                               .clk_freq_hz = kClockFreqPeripheralHz,
                               .parity_enable = kDifUartToggleDisabled,
                               .parity = kDifUartParityEven,
                           }) == kDifUartConfigOk,
        "UART config failed!");
}

static void plic_initialise(mmio_region_t base_addr, dif_plic_t *plic) {
  CHECK(dif_plic_init((dif_plic_params_t){.base_addr = base_addr}, plic) ==
            kDifPlicOk,
        "PLIC init failed!");
}

/**
 * Configures all the relevant interrupts in UART.
 */
static void uart_configure_irqs(dif_uart_t *uart) {
  //edited--------------------------------------
  CHECK(dif_uart_irq_set_enabled(&uart0, kDifUartIrqRxParityErr,
                                 kDifUartToggleEnabled) == kDifUartOk,
        "RX parity error IRQ enable failed!");
  CHECK(dif_uart_irq_set_enabled(&uart0, kDifUartIrqRxTimeout,
                                 kDifUartToggleEnabled) == kDifUartOk,
        "RX FIFO timeout expires before it is emptied IRQ enable failed!");
  CHECK(dif_uart_irq_set_enabled(&uart0, kDifUartIrqRxBreakErr,
                                 kDifUartToggleEnabled) == kDifUartOk,
        "RX break condition IRQ enable failed!");
  CHECK(dif_uart_irq_set_enabled(&uart0, kDifUartIrqRxFrameErr,
                                 kDifUartToggleEnabled) == kDifUartOk,
        "RX framing error IRQ enable failed!");
  //edited--------------------------------------

  CHECK(dif_uart_irq_set_enabled(&uart0, kDifUartIrqRxOverflow,
                                 kDifUartToggleEnabled) == kDifUartOk,
        "RX overflow IRQ enable failed!");
  CHECK(dif_uart_irq_set_enabled(&uart0, kDifUartIrqTxEmpty,
                                 kDifUartToggleEnabled) == kDifUartOk,
        "TX empty IRQ enable failed!");

  //edited----------------------------------------
  CHECK(dif_uart_irq_set_enabled(&uart0, kDifUartIrqRxWatermark,
                                 kDifUartToggleEnabled) == kDifUartOk,
        "RX FIFO goes over its watermark IRQ enable failed!");
  CHECK(dif_uart_irq_set_enabled(&uart0, kDifUartIrqTxWatermark,
                                 kDifUartToggleEnabled) == kDifUartOk,
        "TX FIFO dips below its watermark IRQ enable failed!");
  //edited---------------------------------------
}

/**
 * Configures all the relevant interrupts in PLIC.
 */
static void plic_configure_irqs(dif_plic_t *plic) {
  // Set IRQ triggers to be level triggered
  // //edited-----------------------------------
  CHECK(dif_plic_irq_set_trigger(plic, kTopAthosPlicIrqIdUart0RxParityErr,
                                 kDifPlicIrqTriggerLevel) == kDifPlicOk,
        "RX parity error trigger type set failed!");
  CHECK(dif_plic_irq_set_trigger(plic, kTopAthosPlicIrqIdUart0RxTimeout,
                                 kDifPlicIrqTriggerLevel) == kDifPlicOk,
        "RX FIFO timeout expires before it is emptied trigger type set failed!");
  CHECK(dif_plic_irq_set_trigger(plic, kTopAthosPlicIrqIdUart0RxBreakErr,
                                 kDifPlicIrqTriggerLevel) == kDifPlicOk,
        "RX break condition trigger type set failed!");
  CHECK(dif_plic_irq_set_trigger(plic, kTopAthosPlicIrqIdUart0RxFrameErr,
                                 kDifPlicIrqTriggerLevel) == kDifPlicOk,
        "RX framing error trigger type set failed!");
  //edited--------------------------------------
  CHECK(dif_plic_irq_set_trigger(plic, kTopAthosPlicIrqIdUart0RxOverflow,
                                 kDifPlicIrqTriggerLevel) == kDifPlicOk,
        "RX overflow trigger type set failed!");
  CHECK(dif_plic_irq_set_trigger(plic, kTopAthosPlicIrqIdUart0TxEmpty,
                                 kDifPlicIrqTriggerLevel) == kDifPlicOk,
        "TX empty trigger type set failed!");

  //edited--------------------------------------
  CHECK(dif_plic_irq_set_trigger(plic, kTopAthosPlicIrqIdUart0RxWatermark,
                                 kDifPlicIrqTriggerLevel) == kDifPlicOk,
        "RX FIFO goes over its watermark trigger type set failed!");
  CHECK(dif_plic_irq_set_trigger(plic, kTopAthosPlicIrqIdUart0TxWatermark,
                                 kDifPlicIrqTriggerLevel) == kDifPlicOk,
        "TX FIFO dips below its watermark trigger type set failed!");
  //edited--------------------------------------
  

  // Set IRQ priorities to MAX
  //edited-----------------------------------
  CHECK(dif_plic_irq_set_priority(plic, kTopAthosPlicIrqIdUart0RxParityErr,
                                  kDifPlicMaxPriority) == kDifPlicOk,
        "priority set for RX parity error failed!");
  CHECK(dif_plic_irq_set_priority(plic, kTopAthosPlicIrqIdUart0RxTimeout,
                                  kDifPlicMaxPriority) == kDifPlicOk,
        "priority set for RX FIFO timeout expires before it is emptied failed!");
  CHECK(dif_plic_irq_set_priority(plic, kTopAthosPlicIrqIdUart0RxBreakErr,
                                  kDifPlicMaxPriority) == kDifPlicOk,
        "priority set for RX break condition failed!");
  CHECK(dif_plic_irq_set_priority(plic, kTopAthosPlicIrqIdUart0RxFrameErr,
                                  kDifPlicMaxPriority) == kDifPlicOk,
        "priority set for RX framing error failed!");
  //edited--------------------------------------
  CHECK(dif_plic_irq_set_priority(plic, kTopAthosPlicIrqIdUart0RxOverflow,
                                  kDifPlicMaxPriority) == kDifPlicOk,
        "priority set for RX overflow failed!");

  CHECK(dif_plic_irq_set_priority(plic, kTopAthosPlicIrqIdUart0TxEmpty,
                                  kDifPlicMaxPriority) == kDifPlicOk,
        "priority set for TX empty failed!");
  //edited--------------------------------------
  CHECK(dif_plic_irq_set_priority(plic, kTopAthosPlicIrqIdUart0RxWatermark,
                                  kDifPlicMaxPriority) == kDifPlicOk,
        "priority set for RX FIFO goes over its watermark failed!");
  CHECK(dif_plic_irq_set_priority(plic, kTopAthosPlicIrqIdUart0TxWatermark,
                                  kDifPlicMaxPriority) == kDifPlicOk,
        "priority set for TX FIFO dips below its watermark failed!");
  //edited--------------------------------------
  

  // Set Ibex IRQ priority threshold level
  CHECK(dif_plic_target_set_threshold(&plic0, kPlicTarget,
                                      kDifPlicMinPriority) == kDifPlicOk,
        "threshold set failed!");

  // Enable IRQs in PLIC
  //edited----------------------------------
  CHECK(dif_plic_irq_set_enabled(plic, kTopAthosPlicIrqIdUart0RxParityErr,
                                 kPlicTarget,
                                 kDifPlicToggleEnabled) == kDifPlicOk,
        "interrupt Enable for RX parity error failed!");
  CHECK(dif_plic_irq_set_enabled(plic, kTopAthosPlicIrqIdUart0RxTimeout,
                                 kPlicTarget,
                                 kDifPlicToggleEnabled) == kDifPlicOk,
        "interrupt Enable for RX FIFO timeout expires before it is emptied error failed!");
  CHECK(dif_plic_irq_set_enabled(plic, kTopAthosPlicIrqIdUart0RxBreakErr,
                                 kPlicTarget,
                                 kDifPlicToggleEnabled) == kDifPlicOk,
        "interrupt Enable for  RX break condition failed!");
  CHECK(dif_plic_irq_set_enabled(plic, kTopAthosPlicIrqIdUart0RxFrameErr,
                                 kPlicTarget,
                                 kDifPlicToggleEnabled) == kDifPlicOk,
        "interrupt Enable for RX framing error failed!");
  //edited--------------------------------------
  CHECK(dif_plic_irq_set_enabled(plic, kTopAthosPlicIrqIdUart0RxOverflow,
                                 kPlicTarget,
                                 kDifPlicToggleEnabled) == kDifPlicOk,
        "interrupt Enable for RX overflow failed!");

  CHECK(dif_plic_irq_set_enabled(plic, kTopAthosPlicIrqIdUart0TxEmpty,
                                 kPlicTarget,
                                 kDifPlicToggleEnabled) == kDifPlicOk,
        "interrupt Enable for TX empty failed!");
  //edited
  CHECK(dif_plic_irq_set_enabled(plic, kTopAthosPlicIrqIdUart0RxWatermark,
                                 kPlicTarget,
                                 kDifPlicToggleEnabled) == kDifPlicOk,
        "interrupt Enable for RX FIFO goes over its watermark failed!");

  CHECK(dif_plic_irq_set_enabled(plic, kTopAthosPlicIrqIdUart0TxWatermark,
                                 kPlicTarget,
                                 kDifPlicToggleEnabled) == kDifPlicOk,
        "interrupt Enable for  TX FIFO dips below its watermark failed!");
  //edited
}

static void execute_test(dif_uart_t *uart) {
  //edited---------------------------------------
  // Force UART RX parity error interrupt.
  uart_rx_parity_err_handled = false;
  CHECK(dif_uart_irq_force(uart, kDifUartIrqRxParityErr) == kDifUartOk,
        "failed to force RX parity error IRQ!");
  // Check if the IRQ has occured and has been handled appropriately.
  if (!uart_rx_parity_err_handled) {
    usleep(10);
  }
  CHECK(uart_rx_parity_err_handled, "RX parity error IRQ has not been handled!");

  // Force UART RX FIFO timeout expires before it is emptied interrupt.
  uart_rx_timeout_handled = false;
  CHECK(dif_uart_irq_force(uart, kDifUartIrqRxTimeout) == kDifUartOk,
        "failed to force RX FIFO timeout expires before it is emptied IRQ!");
  // Check if the IRQ has occured and has been handled appropriately.
  if (!uart_rx_timeout_handled) {
    usleep(10);
  }
  CHECK(uart_rx_timeout_handled, "RX FIFO timeout expires before it is emptied IRQ has not been handled!");

  // Force UART RX break condition interrupt.
  uart_rx_break_err_handled = false;
  CHECK(dif_uart_irq_force(uart, kDifUartIrqRxBreakErr) == kDifUartOk,
        "failed to force RX break condition IRQ!");
  // Check if the IRQ has occured and has been handled appropriately.
  if (!uart_rx_break_err_handled) {
    usleep(10);
  }
  CHECK(uart_rx_break_err_handled, "RX break condition IRQ has not been handled!");

  // Force UART RX framing error interrupt.
  uart_rx_frame_err_handled = false;
  CHECK(dif_uart_irq_force(uart, kDifUartIrqRxFrameErr) == kDifUartOk,
        "failed to force RX framing error IRQ!");
  // Check if the IRQ has occured and has been handled appropriately.
  if (!uart_rx_frame_err_handled) {
    usleep(10);
  }
  CHECK(uart_rx_frame_err_handled, "RX framing erro IRQ has not been handled!");
  //edited---------------------------------------

  // Force UART RX overflow interrupt.
  uart_rx_overflow_handled = false;
  CHECK(dif_uart_irq_force(uart, kDifUartIrqRxOverflow) == kDifUartOk,
        "failed to force RX overflow IRQ!");
  // Check if the IRQ has occured and has been handled appropriately.
  if (!uart_rx_overflow_handled) {
    usleep(10);
  }
  CHECK(uart_rx_overflow_handled, "RX overflow IRQ has not been handled!");

  // Force UART TX empty interrupt.
  uart_tx_empty_handled = false;
  CHECK(dif_uart_irq_force(uart, kDifUartIrqTxEmpty) == kDifUartOk,
        "failed to force TX empty IRQ!");
  // Check if the IRQ has occured and has been handled appropriately.
  if (!uart_tx_empty_handled) {
    usleep(10);
  }
  CHECK(uart_tx_empty_handled, "TX empty IRQ has not been handled!"); 

  //edited
  // Force UART RX FIFO goes over its watermark interrupt.
  uart_rx_watermark_handled = false;
  CHECK(dif_uart_irq_force(uart, kDifUartIrqRxWatermark) == kDifUartOk,
        "failed to force RX FIFO goes over its watermark IRQ!");
  // Check if the IRQ has occured and has been handled appropriately.
  if (!uart_rx_watermark_handled) {
    usleep(10);
  }
  CHECK(uart_rx_watermark_handled, "RX FIFO goes over its watermark IRQ has not been handled!");

  // Force UART TX FIFO dips below its watermark interrupt.
  uart_tx_watermark_handled = false;
  CHECK(dif_uart_irq_force(uart, kDifUartIrqTxWatermark) == kDifUartOk,
        "failed to force TX FIFO dips below its watermark IRQ!");
  // Check if the IRQ has occured and has been handled appropriately.
  if (!uart_tx_watermark_handled) {
    usleep(10);
  }
  CHECK(uart_tx_watermark_handled, "TX FIFO dips below its watermark IRQ has not been handled!");
  //edited
}

const test_config_t kTestConfig = {
    .can_clobber_uart = true,
};

bool test_main(void) {
  // Enable IRQs on Ibex
  irq_global_ctrl(true);
  irq_external_ctrl(true);

  // No debug output in case of UART initialisation failure.
  mmio_region_t uart_base_addr =
      mmio_region_from_addr(TOP_ATHOS_UART0_BASE_ADDR);
  uart_initialise(uart_base_addr, &uart0);

  mmio_region_t plic_base_addr =
      mmio_region_from_addr(TOP_ATHOS_RV_PLIC_BASE_ADDR);
  plic_initialise(plic_base_addr, &plic0);

  uart_configure_irqs(&uart0);
  plic_configure_irqs(&plic0);
  execute_test(&uart0);

  return true;
}
