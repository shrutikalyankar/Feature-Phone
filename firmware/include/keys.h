/* SPDX-License-Identifier: MIT */
#ifndef KEYS_H
#define KEYS_H
#include <stdbool.h>
#include <stdint.h>

/* Logical keys. Matrix positions come from Control_Input_Buttons / Numeric_Keypad sheets. */
typedef enum {
  KEY_NONE = 0,
  KEY_SOFT1, KEY_SOFT2, KEY_SOFT3, KEY_SOFT4,   /* CONTROL_C1..C4 = S2, S4, S5, S7, right of the LCD, top to bottom */
  KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_OK,
  KEY_VOL_UP, KEY_VOL_DOWN, KEY_SPEAKER,
  KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
  KEY_STAR, KEY_HASH, KEY_A, KEY_B, KEY_C, KEY_D,
  KEY_COUNT
} fp_key_t;

typedef struct { fp_key_t key; bool pressed; } key_event_t;

/* TCA8418 matrix: 6 rows (ROW0..5) x 5 columns (COL0..4) are wired. */
fp_key_t keys_from_matrix(unsigned row, unsigned col);
/* '0'..'9', '*', '#' for the dialer; 0 for any other key. */
char key_to_dial_char(fp_key_t k);
#endif
