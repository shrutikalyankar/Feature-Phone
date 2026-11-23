/* SPDX-License-Identifier: MIT */
#ifndef LCD_BRINGUP_H
#define LCD_BRINGUP_H

#include "bt815.h"

typedef struct {
  bt815_status_t init_status;
  uint8_t chip_id[4];        /* expect 08 15 01 00 */
  bool ram_pattern_ok;       /* SPI write/read round trip in RAM_G */
  uint32_t frames_before;    /* REG_FRAMES samples 100 ms apart: scan-out running? */
  uint32_t frames_after;
  bt815_status_t draw_status;
} lcd_selftest_t;

extern volatile lcd_selftest_t lcd_selftest;  /* inspect in the debugger / live watch */

/* Boot BT815, run the self-test, draw the test screen, fade the backlight up. */
bt815_status_t lcd_bringup_start(const bt815_hal_t *hal);
/* The BT815 instance used by the bring-up code (valid after lcd_bringup_start succeeds). */
bt815_t *lcd_bringup_device(void);
/* Call about once a second: redraws the screen with an uptime counter. */
bt815_status_t lcd_bringup_tick(uint32_t seconds);

#endif
