/* SPDX-License-Identifier: MIT */
#ifndef BT815_HAL_EFR32_H
#define BT815_HAL_EFR32_H
#include <stdbool.h>
#include "bt815.h"
/* Configure GPIO + EUSART1 and fill *hal. Returns false if the clock tree is unavailable. */
bool bt815_hal_efr32_init(bt815_hal_t *hal);
/* DISP_INT_N level (PC00). Not used by the driver yet; handy on a scope or in a debugger. */
bool bt815_hal_efr32_int_asserted(void);
#endif
