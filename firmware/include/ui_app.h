/* SPDX-License-Identifier: MIT */
#ifndef UI_APP_H
#define UI_APP_H
#include "bt815.h"
#include "keypad_tca8418.h"
#include "ui.h"

/* Bind the UI to an initialised BT815 and keypad. Draws the first frame. */
bt815_status_t ui_app_start(bt815_t *lcd, const keypad_io_t *keys);
/* Call every ~20 ms. key_int = KEY_INT_n asserted. Reads keys, updates the UI, redraws if needed. */
bt815_status_t ui_app_poll(uint32_t now_ms, bool key_int);
/* Last number handed to the modem layer (placeholder until the SIM7600 driver exists). */
const char *ui_app_last_dialed(void);
#endif
