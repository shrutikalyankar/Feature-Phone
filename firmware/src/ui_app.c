/* SPDX-License-Identifier: MIT */
#include "ui_app.h"
#include "ui_render.h"
#include <string.h>

/* Placeholder contact book until storage exists (SD NAND or internal flash). */
static const ui_contact_t CONTACTS[] = {
  { "Alice Johnson", "303-555-0101" }, { "Bob Martinez", "720-555-0142" },
  { "Chris Lee", "303-555-0177" },     { "Dana Whitfield", "970-555-0188" },
  { "Emergency", "911" },              { "Frank Osei", "303-555-0123" },
};

static ui_t ui;
static bt815_t *lcd;
static const keypad_io_t *keys;
static char last_dialed[UI_NUMBER_MAX + 1];

/* Placeholder status until the modem (signal, time) and fuel gauge (battery) are wired in. */
static ui_status_t status = { .signal_bars = 0, .battery_pct = 100, .charging = false,
                              .hour = 0, .minute = 0, .carrier = "No service" };

static void on_dial(void *ctx, const char *number)
{
  (void)ctx;
  strncpy(last_dialed, number, UI_NUMBER_MAX);   /* TODO: ATD<number>; via the SIM7600 driver */
}
static void on_hangup(void *ctx) { (void)ctx; /* TODO: ATH */ }

const char *ui_app_last_dialed(void) { return last_dialed; }

bt815_status_t ui_app_start(bt815_t *l, const keypad_io_t *k)
{
  static const ui_ops_t ops = { NULL, on_dial, on_hangup };
  lcd = l; keys = k;
  ui_init(&ui, CONTACTS, sizeof CONTACTS / sizeof CONTACTS[0], &ops);
  (void)ui_take_dirty(&ui);
  return ui_render(lcd, &ui, &status);
}

bt815_status_t ui_app_poll(uint32_t now_ms, bool key_int)
{
  if (key_int) {
    key_event_t ev[10];
    size_t n = keypad_poll(keys, ev, 10);
    for (size_t i = 0; i < n; ++i) {
      ui_handle_key(&ui, ev[i].key, ev[i].pressed, now_ms);
    }
  }
  ui_tick(&ui, now_ms);
  if (ui_take_dirty(&ui)) {
    return ui_render(lcd, &ui, &status);
  }
  return BT815_OK;
}
