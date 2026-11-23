/* SPDX-License-Identifier: MIT */
#ifndef UI_RENDER_H
#define UI_RENDER_H
#include "bt815.h"
#include "ui.h"

/* Layout for the 800 x 480 panel. The four soft-key buttons are centred on the four
 * switches beside the LCD (S2, S4, S5, S7: 659 mil = 16.7 mm apart = 124 px). */
#define UI_SCREEN_W 800
#define UI_SCREEN_H 480
#define UI_SOFT_X0 668
#define UI_SOFT_X1 796
#define UI_SOFT_PITCH 124
#define UI_SOFT_H 104
/* Define to 1 if SOFT1 (S2) turns out to be at the bottom of the screen on your mounting. */
#ifndef UI_SOFTKEYS_REVERSED
#define UI_SOFTKEYS_REVERSED 0
#endif
int ui_softkey_center_y(int idx);

/* Draw one complete frame for the current UI state. */
bt815_status_t ui_render(bt815_t *d, const ui_t *u, const ui_status_t *st);
#endif
