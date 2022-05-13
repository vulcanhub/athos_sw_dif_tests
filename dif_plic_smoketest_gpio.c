// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "dif/dif_plic.h"

#include "base/mmio.h"
#include "dif/dif_gpio.h"
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
static dif_gpio_t gpio;

// These flags are used in the test routine to verify that a corresponding
// interrupt has elapsed, and has been serviced. These are declared as volatile
// since they are referenced in the ISR routine as well as in the main program
// flow.
static volatile bool gpio_gpio0;
static volatile bool gpio_gpio1;

/**
 * GPIO interrupt handler
 *
 * Services gpio interrupts, sets the appropriate flags that are used to
 * determine success or failure of the test.
 */
static void handle_gpio_isr(const dif_plic_irq_id_t interrupt_id) {
  // NOTE: This initialization is superfluous, since the `default` case below
  // is effectively noreturn, but the compiler is unable to prove this.
  dif_gpio_irq_trigger_t gpio_irq = 0;

  switch (interrupt_id) {    
    case kTopAthosPlicIrqIdGpioGpio1:
      CHECK(!gpio_gpio1,
            "gpio gpio1 input for falling edge detection IRQ asserted more than once");

      gpio_irq = kDifGpioIrqTriggerEdgeFalling;
      gpio_gpio1 = true;
      break;
      case kTopAthosPlicIrqIdGpioGpio0:
      CHECK(!gpio_gpio0,
            "gpio gpio0 input for rising edge detection IRQ asserted more than once");

      gpio_irq = kDifGpioIrqTriggerEdgeRising;
      gpio_gpio0 = true;
      break;
    default:
      LOG_FATAL("ISR is not implemented!");
      test_status_set(kTestStatusFailed);
  }

  CHECK(dif_gpio_irq_acknowledge(&gpio, gpio_irq) == kDifGpioOk,
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

  // Check if the interrupted peripheral is gpio.
  top_athos_plic_peripheral_t peripheral_id =
      top_athos_plic_interrupt_for_peripheral[interrupt_id];
  CHECK(peripheral_id == kTopAthosPlicPeripheralGpio,
        "ISR interrupted peripheral is not gpio!");
  handle_gpio_isr(interrupt_id);

  // Complete the IRQ by writing the IRQ source to the Ibex specific CC
  // register.
  CHECK(dif_plic_irq_complete(&plic0, kPlicTarget, &interrupt_id) == kDifPlicOk,
        "Unable to complete the IRQ request!");
}

static void gpio_initialise(mmio_region_t base_addr, dif_gpio_t *gpio) {
    CHECK(dif_gpio_init(
            (dif_gpio_params_t){
                .base_addr = base_addr,},gpio) == 
            kDifGpioOk,"gpio init failed!");
}

static void plic_initialise(mmio_region_t base_addr, dif_plic_t *plic) {
  CHECK(dif_plic_init((dif_plic_params_t){.base_addr = base_addr}, plic) ==
            kDifPlicOk,
        "PLIC init failed!");
}

/**
 * Configures all the relevant interrupts in gpio.
 */
static void gpio_configure_irqs(dif_gpio_t *gpio) {
  CHECK(dif_gpio_irq_set_enabled(gpio, kDifGpioIrqTriggerEdgeFalling,
                                 kDifGpioToggleEnabled) == kDifGpioOk,
        "Falling edge IRQ enable failed!");
  
  CHECK(dif_gpio_irq_set_enabled(gpio, kDifGpioIrqTriggerEdgeRising,
                                 kDifGpioToggleEnabled) == kDifGpioOk,
        "Rising edge IRQ enable failed!");

 /* CHECK(dif_gpio_irq_set_enabled(&gpio, kDifGpioIrqTriggerEdgeFalling,
                                 kDifGpioToggleEnabled) == kDifGpioOk,
        "Falling edge IRQ enable failed!");*/
}

/**
 * Configures all the relevant interrupts in PLIC.
 */
static void plic_configure_irqs(dif_plic_t *plic) {
  // Set IRQ triggers to be level triggered
  CHECK(dif_plic_irq_set_trigger(plic, kTopAthosPlicIrqIdGpioGpio1,
                                 kDifPlicIrqTriggerLevel) == kDifPlicOk,
        "Falling edge trigger type set failed!");
  CHECK(dif_plic_irq_set_trigger(plic, kTopAthosPlicIrqIdGpioGpio0,
                                 kDifPlicIrqTriggerLevel) == kDifPlicOk,
        "Rising edge trigger type set failed!");

  // Set IRQ priorities to MAX
  CHECK(dif_plic_irq_set_priority(plic, kTopAthosPlicIrqIdGpioGpio1,
                                  kDifPlicMaxPriority) == kDifPlicOk,
        "priority set for Falling edge trigger failed!");

  CHECK(dif_plic_irq_set_priority(plic, kTopAthosPlicIrqIdGpioGpio0,
                                  kDifPlicMaxPriority) == kDifPlicOk,
        "priority set for Rising edge trigger failed!");

  // Set Ibex IRQ priority threshold level
  CHECK(dif_plic_target_set_threshold(&plic0, kPlicTarget,
                                      kDifPlicMinPriority) == kDifPlicOk,
        "threshold set failed!");

  // Enable IRQs in PLIC
  CHECK(dif_plic_irq_set_enabled(plic, kTopAthosPlicIrqIdGpioGpio1,
                                 kPlicTarget,
                                 kDifPlicToggleEnabled) == kDifPlicOk,
        "interrupt Enable for Falling edge trigger failed!");

  CHECK(dif_plic_irq_set_enabled(plic, kTopAthosPlicIrqIdGpioGpio0,
                                 kPlicTarget,
                                 kDifPlicToggleEnabled) == kDifPlicOk,
        "interrupt Enable for Rising edge trigger failed!");
}

static void execute_test(dif_gpio_t *gpio) {
  // Force gpio Falling edge interrupt.
  gpio_gpio1 = false;
  CHECK(dif_gpio_irq_force(gpio, kDifGpioIrqTriggerEdgeFalling) == kDifGpioOk,
        "failed to force Falling edge IRQ!");
  // Check if the IRQ has occured and has been handled appropriately.
  if (!gpio_gpio1) {
    usleep(10);
  }
  CHECK(gpio_gpio1, "Falling edge IRQ has not been handled!");

  // Force gpio Rising edge trigger interrupt.
  gpio_gpio0 = false;
  CHECK(dif_gpio_irq_force(gpio, kDifGpioIrqTriggerEdgeRising) == kDifGpioOk,
        "failed to force Rising edge IRQ!");
  // Check if the IRQ has occured and has been handled appropriately.
  if (!gpio_gpio0) {
    usleep(10);
  }
  CHECK(gpio_gpio0, "Rising edge IRQ has not been handled!");
}

const test_config_t kTestConfig;

bool test_main(void) {
  // Enable IRQs on Ibex
  irq_global_ctrl(true);
  irq_external_ctrl(true);

  // No debug output in case of GPIO initialisation failure.
  mmio_region_t gpio_base_addr =
      mmio_region_from_addr(TOP_ATHOS_GPIO_BASE_ADDR);
  gpio_initialise(gpio_base_addr, &gpio);
  
  mmio_region_t plic_base_addr =
      mmio_region_from_addr(TOP_ATHOS_RV_PLIC_BASE_ADDR);
  plic_initialise(plic_base_addr, &plic0);

  gpio_configure_irqs(&gpio);
  plic_configure_irqs(&plic0);
  execute_test(&gpio);

  return true;
}
