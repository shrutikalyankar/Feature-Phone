/* SPDX-License-Identifier: MIT */
/* Stand-alone phone UI image: boots the BT815, shows the self-test screen for a second,
 * then runs the soft-key UI driven by the TCA8418 keypad. Use with lcd_bringup.slcp. */
#include "sl_system_init.h"
#include "sl_system_process_action.h"
#include "sl_power_manager.h"
#include "sl_sleeptimer.h"
#include "bt815_hal_efr32.h"
#include "keypad_hal_efr32.h"
#include "lcd_bringup.h"
#include "ui_app.h"

#define POLL_MS 20u
static sl_sleeptimer_timer_handle_t poll_timer;
static volatile bool poll_flag;
static void on_poll(sl_sleeptimer_timer_handle_t *t, void *d) { (void)t; (void)d; poll_flag = true; }

bool app_is_ok_to_sleep(void) { return !poll_flag; }
sl_power_manager_on_isr_exit_t app_sleep_on_isr_exit(void)
{
  return poll_flag ? SL_POWER_MANAGER_WAKEUP : SL_POWER_MANAGER_IGNORE;
}

volatile int app_stage;   /* debugger: 1 hal ok, 2 lcd ok, 3 keypad ok, 4 running */

int main(void)
{
  sl_system_init();
  /* EUSART1 needs the high-frequency clock: stay out of EM2 while the LCD is in use. */
  sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);

  static bt815_hal_t hal;
  static keypad_io_t keys;
  bool ok = bt815_hal_efr32_init(&hal);
  app_stage = ok ? 1 : 0;
  ok = ok && lcd_bringup_start(&hal) == BT815_OK;       /* self-test + test screen + backlight */
  if (ok) { app_stage = 2; }
  if (ok) {
    sl_sleeptimer_delay_millisecond(1000);              /* leave the test screen visible briefly */
    ok = keypad_hal_efr32_init(&keys) && keypad_init(&keys);
  }
  if (ok) {
    app_stage = 3;
    ok = ui_app_start(lcd_bringup_device(), &keys) == BT815_OK;
  }
  if (ok) {
    app_stage = 4;
    sl_sleeptimer_start_periodic_timer_ms(&poll_timer, POLL_MS, on_poll, NULL, 0, 0);
  }
  for (;;) {
    sl_system_process_action();
    if (ok && poll_flag) {
      poll_flag = false;
      uint32_t now = sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());
      ui_app_poll(now, keypad_hal_efr32_int_asserted());
    }
    sl_power_manager_sleep();   /* on failure: halt here; read app_stage and lcd_selftest in the debugger */
  }
}
