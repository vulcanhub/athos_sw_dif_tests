// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Copyright (C) May 2022, Belmont Computing, Inc. -- All Rights Reserved
// Licensed under the BCI License. See LICENSE for details.

#include "base/memory.h"
#include "dif/dif_clkmgr.h"
#include "dif/check.h"
#include "dif/test_main.h"

#include "top/sw/autogen/top_athos.h"  // Generated.

const test_config_t kTestConfig;

/**
 * Test that all 'gateable' clocks, directly controlled by software,
 * can be enabled and disabled.
 */
static void test_gateable_clocks(const dif_clkmgr_t *clkmgr) {
  const dif_clkmgr_gateable_clock_t clocks[] = {
      kTopAthosGateableClocksIoDiv4Peri,
      kTopAthosGateableClocksUsbPeri,
  };

  for (int i = 0; i < ARRAYSIZE(clocks); ++i) {
    dif_clkmgr_gateable_clock_t clock = clocks[i];

    // Get the initial state of the clock. The clock might be enabled or
    // disabled depending on reset behavior - either is fine for the purposes of
    // this test.
    bool enabled;
    CHECK(dif_clkmgr_gateable_clock_get_enabled(clkmgr, clock, &enabled) ==
          kDifClkmgrOk);

    // Toggle the enable twice so that it ends up in its original state.
    for (int j = 0; j < 2; ++j) {
      bool expected = !enabled;
      CHECK(dif_clkmgr_gateable_clock_set_enabled(
                clkmgr, clock,
                expected ? kDifClkmgrToggleEnabled
                         : kDifClkmgrToggleDisabled) == kDifClkmgrOk);
      CHECK(dif_clkmgr_gateable_clock_get_enabled(clkmgr, clock, &enabled) ==
            kDifClkmgrOk);
      CHECK(enabled == expected);
    }
  }
}

/**
 * Test that all 'hintable' clocks, indirectly controlled by software,
 * can have their hint toggled and status checked.
 */
void test_hintable_clocks(const dif_clkmgr_t *clkmgr) {
  const dif_clkmgr_hintable_clock_t clocks[] = {
//      kTopAthosHintableClocksMainAes,
      kTopAthosHintableClocksMainHmac,
      kTopAthosHintableClocksMainKmac,
//      kTopAthosHintableClocksMainOtbn,
  };

  for (int i = 0; i < ARRAYSIZE(clocks); ++i) {
    dif_clkmgr_hintable_clock_t clock = clocks[i];

    // Get the initial state of the hint for the clock The clock hint might be
    // enabled or disabled depending on reset behavior - either is fine for the
    // purposes of this test.
    bool enabled;
    CHECK(dif_clkmgr_hintable_clock_get_hint(clkmgr, clock, &enabled) ==
          kDifClkmgrOk);

    // Toggle the hint twice so that it ends up in its original state.
    for (int j = 0; j < 2; ++j) {
      bool expected = !enabled;
      CHECK(dif_clkmgr_hintable_clock_set_hint(
                clkmgr, clock,
                expected ? kDifClkmgrToggleEnabled
                         : kDifClkmgrToggleDisabled) == kDifClkmgrOk);
      CHECK(dif_clkmgr_hintable_clock_get_hint(clkmgr, clock, &enabled) ==
            kDifClkmgrOk);
      CHECK(enabled == expected);

      // If the clock hint is enabled then the clock should always be enabled.
      if (enabled) {
        bool status = false;
        CHECK(dif_clkmgr_hintable_clock_get_enabled(clkmgr, clock, &status) ==
              kDifClkmgrOk);
        CHECK(status, "clock %u hint is enabled but status is disabled", clock);
      }
    }
  }
}

bool test_main() {
  const dif_clkmgr_params_t params = {
      .base_addr = mmio_region_from_addr(TOP_ATHOS_CLKMGR_AON_BASE_ADDR),
      .last_gateable_clock = kTopAthosGateableClocksLast,
      .last_hintable_clock = kTopAthosHintableClocksLast,
  };

  dif_clkmgr_t clkmgr;
  CHECK(dif_clkmgr_init(params, &clkmgr) == kDifClkmgrOk);
  test_gateable_clocks(&clkmgr);
  test_hintable_clocks(&clkmgr);

  return true;
}
