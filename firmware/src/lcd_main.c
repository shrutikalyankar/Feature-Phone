/* SPDX-License-Identifier: MIT */
/* Stand-alone LCD bring-up image: boots BT815, draws a test screen, updates a
 * counter once per second. Use with lcd_bringup.slcp (separate from the energy demo). */
#include "sl_system_init.h"
#include "sl_system_process_action.h"
#include "sl_power_manager.h"
#include "sl_sleeptimer.h"
#include "bt815_hal_efr32.h"
#include "lcd_bringup.h"

static sl_sleeptimer_timer_handle_t sec_timer;
static volatile bool sec_flag;
static void on_sec(sl_sleeptimer_timer_handle_t *t, void *d) { (void)t; (void)d; sec_flag = true; }

bool app_is_ok_to_sleep(void) { return !sec_flag; }
sl_power_manager_on_isr_exit_t app_sleep_on_isr_exit(void)
{
  return sec_flag ? SL_POWER_MANAGER_WAKEUP : SL_POWER_MANAGER_IGNORE;
}

int main(void)
{
  sl_system_init();
  /* EUSART1 needs the high-frequency clock: stay out of EM2 while the LCD is in use. */
  sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);

  static bt815_hal_t hal;
  bool ok = bt815_hal_efr32_init(&hal) && lcd_bringup_start(&hal) == BT815_OK;
  uint32_t seconds = 0;
  if (ok) {
    sl_sleeptimer_start_periodic_timer_ms(&sec_timer, 1000, on_sec, NULL, 0, 0);
  }
  for (;;) {
    sl_system_process_action();
    if (ok && sec_flag) {
      sec_flag = false;
      lcd_bringup_tick(++seconds);
    }
    sl_power_manager_sleep();   /* on failure: halt here, read lcd_selftest in the debugger */
  }
}
