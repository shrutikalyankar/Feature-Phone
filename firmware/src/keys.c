/* SPDX-License-Identifier: MIT */
#include "keys.h"

static const fp_key_t MAP[6][5] = {
  /* COL0      COL1       COL2       COL3       COL4 */
  { KEY_1,    KEY_2,     KEY_3,     KEY_A,     KEY_UP    },   /* ROW0 */
  { KEY_4,    KEY_5,     KEY_6,     KEY_B,     KEY_DOWN  },   /* ROW1 */
  { KEY_7,    KEY_8,     KEY_9,     KEY_C,     KEY_LEFT  },   /* ROW2 */
  { KEY_STAR, KEY_0,     KEY_HASH,  KEY_D,     KEY_RIGHT },   /* ROW3 */
  { KEY_OK,   KEY_SOFT1, KEY_SOFT2, KEY_SOFT3, KEY_SOFT4 },   /* ROW4 */
  { KEY_SPEAKER, KEY_VOL_UP, KEY_VOL_DOWN, KEY_NONE, KEY_NONE }, /* ROW5 */
};

fp_key_t keys_from_matrix(unsigned row, unsigned col)
{
  return (row < 6u && col < 5u) ? MAP[row][col] : KEY_NONE;
}

char key_to_dial_char(fp_key_t k)
{
  if (k >= KEY_0 && k <= KEY_9) { return (char)('0' + (k - KEY_0)); }
  if (k == KEY_STAR) { return '*'; }
  if (k == KEY_HASH) { return '#'; }
  return 0;
}
