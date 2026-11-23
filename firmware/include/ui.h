/* SPDX-License-Identifier: MIT */
/* Phone UI model: screens, the four soft keys beside the LCD, and navigation.
 * Pure logic (no BT815 access) so it runs in host tests. Drawing is in ui_render.c. */
#ifndef UI_H
#define UI_H
#include <stdbool.h>
#include <stdint.h>
#include "keys.h"

typedef enum { UI_HOME, UI_CONTACTS, UI_DIALER, UI_INCALL, UI_MESSAGES, UI_SETTINGS } ui_screen_t;
typedef enum { ICON_NONE, ICON_CALL, ICON_END, ICON_CONTACTS, ICON_HOME, ICON_MESSAGE, ICON_SETTINGS, ICON_BACK } ui_icon_t;

typedef struct {
  const char *label;     /* NULL = key unused on this screen */
  ui_icon_t icon;
  uint8_t tone;          /* 0 neutral, 1 green (call), 2 red (end) */
} ui_softkey_t;

typedef struct { const char *name; const char *number; } ui_contact_t;

/* Host side callbacks: the modem layer plugs in here. */
typedef struct {
  void *ctx;
  void (*dial)(void *ctx, const char *number);
  void (*hangup)(void *ctx);
} ui_ops_t;

#define UI_NUMBER_MAX 24
#define UI_PRESS_FLASH_MS 150u

typedef struct {
  ui_screen_t screen;
  unsigned contact_sel;               /* index into the contact list */
  char number[UI_NUMBER_MAX + 1];
  unsigned number_len;
  const char *call_name;              /* name shown during a call, if known */
  int8_t pressed_soft;                /* 0..3 while its flash is active, else -1 */
  uint32_t pressed_until_ms;
  bool dirty;                         /* screen needs redrawing */
  ui_ops_t ops;
  const ui_contact_t *contacts;
  unsigned contact_count;
} ui_t;

typedef struct {
  uint8_t signal_bars;   /* 0..5 */
  uint8_t battery_pct;   /* 0..100 */
  bool charging;
  uint8_t hour, minute;
  const char *carrier;
} ui_status_t;

void ui_init(ui_t *u, const ui_contact_t *contacts, unsigned count, const ui_ops_t *ops);
/* Feed key events. now_ms is any monotonic millisecond clock. */
void ui_handle_key(ui_t *u, fp_key_t key, bool pressed, uint32_t now_ms);
/* Expire the press flash; call regularly. Sets dirty when the flash ends. */
void ui_tick(ui_t *u, uint32_t now_ms);
/* The four labels for the current screen, top to bottom (index 0 = KEY_SOFT1 = S2). */
void ui_softkeys(const ui_t *u, ui_softkey_t out[4]);
const char *ui_screen_title(ui_screen_t s);
bool ui_take_dirty(ui_t *u);   /* returns and clears the dirty flag */
#endif
