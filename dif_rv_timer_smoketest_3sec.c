// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// #include "dif/dif_rv_timer.h"
// 
// #include "handler/handler.h"
// #include "irq/irq.h"
// #include "runtime/hart.h"
// #include "runtime/ibex.h"
// #include "runtime/log.h"
// #include "testing/check.h"
// #include "testing/test_main.h"
// #include "top_athos_sw/sw/autogen/top_athos.h"

#include "dif/dif_rv_timer.h"

#include "dif/handler.h"
#include "dif/irq.h"
#include "dif/hart.h"
#include "dif/ibex.h"
#include "dif/log.h"
#include "dif/check.h"
#include "dif/test_main.h"
#include "top_athos_sw/sw/autogen/top_athos.h"

static dif_rv_timer_t timer;

// Flag for checking whether the interrupt handler was called. When the handler
// is entered, this value *must* be set to false, to catch false positives.
//
// This variable is volatile, since it may suddenly change from the compiler's
// perspective (e.g., due to an interrupt firing).
static volatile bool irq_fired = true;

// NOTE: PLIC targets need not line up with hart ids; in the future, we should
// generate hart ID constants elsewhere.
static const uint32_t kHart = (uint32_t)kTopAthosPlicTargetIbex0;
static const uint32_t kComparator = 0;

static const uint64_t kTickFreqHz = 1000 * 1000;  // 1 MHz.

static void test_handler(void) {
  CHECK(!irq_fired, "Entered IRQ handler, but `irq_fired` was not false!");

  bool irq_flag;
  CHECK(dif_rv_timer_irq_get(&timer, kHart, kComparator, &irq_flag) ==
        kDifRvTimerOk);
  CHECK(irq_flag, "Entered IRQ handler but the expected IRQ flag wasn't set!");

  CHECK(dif_rv_timer_counter_set_enabled(&timer, kHart, kDifRvTimerDisabled) ==
        kDifRvTimerOk);
  CHECK(dif_rv_timer_irq_clear(&timer, kHart, kComparator) == kDifRvTimerOk);

  irq_fired = true;
}

// Register our own IRQ handler by overriding a weak symbol in `handler.h`.
void handler_irq_timer(void) {
  LOG_INFO("Entering handler_irq_timer()");
  test_handler();
  LOG_INFO("Exiting handler_irq_timer()");
}

const test_config_t kTestConfig;

bool test_main(void) {
  irq_global_ctrl(true);
  irq_timer_ctrl(true);

  mmio_region_t timer_reg =
      mmio_region_from_addr(TOP_ATHOS_RV_TIMER_BASE_ADDR);

  //-----edited
  LOG_INFO("bfr timer = %x",timer);
  //-----edited

  CHECK(dif_rv_timer_init(
            timer_reg,
            (dif_rv_timer_config_t){.hart_count = 1, .comparator_count = 1},
            &timer) == kDifRvTimerOk);

  //-----edited
  LOG_INFO("aftr timer = %x",timer);
  //-----edited
  
  dif_rv_timer_tick_params_t tick_params;
  CHECK(dif_rv_timer_approximate_tick_params(kClockFreqPeripheralHz,
                                             kTickFreqHz, &tick_params) ==
        kDifRvTimerApproximateTickParamsOk);
  
  //-----edited
  LOG_INFO("kClockFreqPeripheralHz = %x",kClockFreqPeripheralHz);
  LOG_INFO("kTickFreqHz = %x",kTickFreqHz);
  //-----edited
  
  CHECK(dif_rv_timer_set_tick_params(&timer, kHart, tick_params) ==
        kDifRvTimerOk);
  CHECK(dif_rv_timer_irq_enable(&timer, kHart, kComparator,
                                kDifRvTimerEnabled) == kDifRvTimerOk);
  //-----edited
  LOG_INFO("kHart = %x",kHart);
  LOG_INFO("kComparator = %x",kComparator);
  //-----edited
  uint64_t current_time;
  // Logs over UART incur a large runtime overhead. To accommodate that, the
  // timer deadline needs to be large as well. In DV simulations, logs are not
  // sent over UART, so we can reduce the runtime / sim time with a much shorter
  // deadline (3s vs 100 us).
  uint64_t kDeadline = (kDeviceType == kDeviceSimDV) ? 100 /* 100 us */ : 3000000 /* 3s */;
      
  CHECK(dif_rv_timer_counter_read(&timer, kHart, &current_time) ==
        kDifRvTimerOk);
  LOG_INFO("Current time: %d; timer theshold: %d", (uint32_t)current_time,
           (uint32_t)(current_time + kDeadline));
  CHECK(dif_rv_timer_arm(&timer, kHart, kComparator,
                         current_time + kDeadline) == kDifRvTimerOk);
   //-----edited
  LOG_INFO("kDeadline = %x",kDeadline);
   LOG_INFO("Current time: %d; timer theshold: %x", (uint32_t)current_time,
           (uint32_t)(current_time + kDeadline));
  //-----edited

  irq_fired = false;
  CHECK(dif_rv_timer_counter_set_enabled(&timer, kHart, kDifRvTimerEnabled) ==
        kDifRvTimerOk);

  LOG_INFO("Waiting...");
  while (!irq_fired) {
    wait_for_interrupt();
  }

  return true;
}
