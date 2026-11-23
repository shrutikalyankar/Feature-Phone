/* SPDX-License-Identifier: MIT */
#ifndef KEYPAD_HAL_EFR32_H
#define KEYPAD_HAL_EFR32_H
#include <stdbool.h>
#include "keypad_tca8418.h"
/* Configure I2C0 (100 kHz) and the KEY_INT_n input, and fill *io. */
bool keypad_hal_efr32_init(keypad_io_t *io);
/* True while the TCA8418 has key events pending (KEY_INT_n low). */
bool keypad_hal_efr32_int_asserted(void);
#endif
