/* SPDX-License-Identifier: MIT */
#include "ui.h"
#include <string.h>

void ui_init(ui_t *u, const ui_contact_t *contacts, unsigned count, const ui_ops_t *ops)
{
  memset(u, 0, sizeof *u);
  u->screen = UI_HOME;
  u->pressed_soft = -1;
  u->contacts = contacts;
  u->contact_count = count;
  if (ops) { u->ops = *ops; }
  u->dirty = true;
}

const char *ui_screen_title(ui_screen_t s)
{
  switch (s) {
    case UI_HOME: return "Home";
    case UI_CONTACTS: return "Contacts";
    case UI_DIALER: return "Dial";
    case UI_INCALL: return "Calling";
    case UI_MESSAGES: return "Messages";
    case UI_SETTINGS: return "Settings";
  }
  return "";
}

void ui_softkeys(const ui_t *u, ui_softkey_t out[4])
{
  static const ui_softkey_t none = { 0, ICON_NONE, 0 };
  for (int i = 0; i < 4; ++i) { out[i] = none; }
  switch (u->screen) {
    case UI_HOME:
      out[0] = (ui_softkey_t){ "Call", ICON_CALL, 1 };
      out[1] = (ui_softkey_t){ "Contacts", ICON_CONTACTS, 0 };
      out[2] = (ui_softkey_t){ "Messages", ICON_MESSAGE, 0 };
      out[3] = (ui_softkey_t){ "Settings", ICON_SETTINGS, 0 };
      break;
    case UI_CONTACTS:
      out[0] = (ui_softkey_t){ "Call", ICON_CALL, 1 };
      out[1] = (ui_softkey_t){ "Message", ICON_MESSAGE, 0 };
      out[3] = (ui_softkey_t){ "Home", ICON_HOME, 0 };
      break;
    case UI_DIALER:
      out[0] = (ui_softkey_t){ "Call", ICON_CALL, 1 };
      out[1] = (ui_softkey_t){ "Delete", ICON_BACK, 0 };
      out[3] = (ui_softkey_t){ "Home", ICON_HOME, 0 };
      break;
    case UI_INCALL:
      out[0] = (ui_softkey_t){ "End", ICON_END, 2 };
      break;
    case UI_MESSAGES:
    case UI_SETTINGS:
      out[3] = (ui_softkey_t){ "Home", ICON_HOME, 0 };
      break;
  }
}

static void go(ui_t *u, ui_screen_t s)
{
  u->screen = s;
  u->dirty = true;
}

static void start_call(ui_t *u, const char *number, const char *name)
{
  if (!number || number[0] == '\0') { return; }
  u->call_name = name;
  if (number != u->number) {
    strncpy(u->number, number, UI_NUMBER_MAX);
    u->number[UI_NUMBER_MAX] = '\0';
    u->number_len = (unsigned)strlen(u->number);
  }
  if (u->ops.dial) { u->ops.dial(u->ops.ctx, u->number); }
  go(u, UI_INCALL);
}

static void softkey_action(ui_t *u, int idx)
{
  switch (u->screen) {
    case UI_HOME:
      if (idx == 0) { go(u, UI_DIALER); }
      else if (idx == 1) { u->contact_sel = 0; go(u, UI_CONTACTS); }
      else if (idx == 2) { go(u, UI_MESSAGES); }
      else { go(u, UI_SETTINGS); }
      break;
    case UI_CONTACTS:
      if (idx == 0 && u->contact_count) {
        const ui_contact_t *c = &u->contacts[u->contact_sel];
        start_call(u, c->number, c->name);
      } else if (idx == 3) { go(u, UI_HOME); }
      /* idx 1 (Message) will open a compose screen later */
      break;
    case UI_DIALER:
      if (idx == 0) { u->call_name = NULL; start_call(u, u->number, NULL); }
      else if (idx == 1 && u->number_len) { u->number[--u->number_len] = '\0'; u->dirty = true; }
      else if (idx == 3) { go(u, UI_HOME); }
      break;
    case UI_INCALL:
      if (idx == 0) {
        if (u->ops.hangup) { u->ops.hangup(u->ops.ctx); }
        go(u, UI_HOME);
      }
      break;
    case UI_MESSAGES:
    case UI_SETTINGS:
      if (idx == 3) { go(u, UI_HOME); }
      break;
  }
}

void ui_handle_key(ui_t *u, fp_key_t key, bool pressed, uint32_t now_ms)
{
  if (!pressed) { return; }                       /* act on press; release only ends the flash */
  if (key >= KEY_SOFT1 && key <= KEY_SOFT4) {
    int idx = key - KEY_SOFT1;
    ui_softkey_t sk[4];
    ui_softkeys(u, sk);
    if (sk[idx].label == NULL) { return; }        /* unused on this screen */
    u->pressed_soft = (int8_t)idx;
    u->pressed_until_ms = now_ms + UI_PRESS_FLASH_MS;
    u->dirty = true;
    softkey_action(u, idx);
    return;
  }
  switch (u->screen) {
    case UI_CONTACTS:
      if (key == KEY_DOWN && u->contact_sel + 1u < u->contact_count) { ++u->contact_sel; u->dirty = true; }
      else if (key == KEY_UP && u->contact_sel > 0u) { --u->contact_sel; u->dirty = true; }
      else if (key == KEY_OK && u->contact_count) {
        const ui_contact_t *c = &u->contacts[u->contact_sel];
        start_call(u, c->number, c->name);
      }
      break;
    case UI_HOME:
      if (key_to_dial_char(key)) {                /* typing a digit on Home opens the dialer */
        u->number_len = 0; u->number[0] = '\0';
        go(u, UI_DIALER);
        ui_handle_key(u, key, true, now_ms);
      }
      break;
    case UI_DIALER: {
      char c = key_to_dial_char(key);
      if (c && u->number_len < UI_NUMBER_MAX) {
        u->number[u->number_len++] = c; u->number[u->number_len] = '\0'; u->dirty = true;
      } else if (key == KEY_OK) {
        u->call_name = NULL; start_call(u, u->number, NULL);
      }
      break;
    }
    default: break;
  }
}

void ui_tick(ui_t *u, uint32_t now_ms)
{
  if (u->pressed_soft >= 0 && (int32_t)(now_ms - u->pressed_until_ms) >= 0) {
    u->pressed_soft = -1;
    u->dirty = true;
  }
}

bool ui_take_dirty(ui_t *u)
{
  bool d = u->dirty;
  u->dirty = false;
  return d;
}
