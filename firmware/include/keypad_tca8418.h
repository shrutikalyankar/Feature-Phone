/* SPDX-License-Identifier: MIT */
/* TCA8418 keypad scanner (U10, I2C 0x34, INT -> MCU PA04 "KEY_INT_n", active low). */
#ifndef KEYPAD_TCA8418_H
#define KEYPAD_TCA8418_H
#include <stdbool.h>
#include <stddef.h>
#include "keys.h"

typedef struct {
  void *ctx;
  /* Return true on success. */
  bool (*write_reg)(void *ctx, uint8_t dev7, uint8_t reg, uint8_t val);
  bool (*read_reg)(void *ctx, uint8_t dev7, uint8_t reg, uint8_t *val);
} keypad_io_t;

#define TCA8418_ADDR7 0x34u

/* Configure ROW0..5 and COL0..4 as the matrix (ROW6/ROW7 are used as GPIO on this board:
 * HANDSET_DETECT and HS_BS_MIC_SELECTOR) and enable the key-event interrupt. */
bool keypad_init(const keypad_io_t *io);
/* Drain the key FIFO into out[]. Returns the number of events stored (0 = none). */
size_t keypad_poll(const keypad_io_t *io, key_event_t *out, size_t max);
#endif
