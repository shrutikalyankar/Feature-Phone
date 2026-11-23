/* SPDX-License-Identifier: MIT */
#include "keypad_tca8418.h"

/* Register addresses: TI TCA8418 datasheet section 8.6 (CFG 0x01, INT_STAT 0x02,
 * KEY_LCK_EC 0x03, KEY_EVENT_A 0x04, KP_GPIO1..3 0x1D..0x1F). Key event number =
 * 10 * row + column + 1 (Key Event Table 1). Bit layouts below (KE_IEN = CFG bit 0,
 * K_INT = INT_STAT bit 0 write-1-to-clear, event bit 7 = press, EC = low nibble of
 * KEY_LCK_EC) are from the datasheet as I remember it: verify on first power-up. */
#define REG_CFG        0x01u
#define REG_INT_STAT   0x02u
#define REG_KEY_LCK_EC 0x03u
#define REG_KEY_EVENT  0x04u
#define REG_KP_GPIO1   0x1Du
#define REG_KP_GPIO2   0x1Eu
#define REG_KP_GPIO3   0x1Fu
#define CFG_KE_IEN     0x01u
#define INT_K_INT      0x01u

bool keypad_init(const keypad_io_t *io)
{
  return io->write_reg(io->ctx, TCA8418_ADDR7, REG_KP_GPIO1, 0x3Fu)   /* ROW0..5 */
      && io->write_reg(io->ctx, TCA8418_ADDR7, REG_KP_GPIO2, 0x1Fu)   /* COL0..4 */
      && io->write_reg(io->ctx, TCA8418_ADDR7, REG_KP_GPIO3, 0x00u)
      && io->write_reg(io->ctx, TCA8418_ADDR7, REG_INT_STAT, 0x1Fu)   /* clear stale flags */
      && io->write_reg(io->ctx, TCA8418_ADDR7, REG_CFG, CFG_KE_IEN);
}

size_t keypad_poll(const keypad_io_t *io, key_event_t *out, size_t max)
{
  size_t n = 0;
  uint8_t ec = 0;
  if (!io->read_reg(io->ctx, TCA8418_ADDR7, REG_KEY_LCK_EC, &ec)) {
    return 0;
  }
  ec &= 0x0Fu;                                   /* events waiting in the FIFO (max 10) */
  while (ec-- && n < max) {
    uint8_t ev = 0;
    if (!io->read_reg(io->ctx, TCA8418_ADDR7, REG_KEY_EVENT, &ev) || ev == 0u) {
      break;
    }
    unsigned num = ev & 0x7Fu;
    if (num >= 1u && num <= 80u) {               /* 97..114 are GPI events: ignored */
      fp_key_t k = keys_from_matrix((num - 1u) / 10u, (num - 1u) % 10u);
      if (k != KEY_NONE) {
        out[n].key = k;
        out[n].pressed = (ev & 0x80u) != 0u;
        ++n;
      }
    }
  }
  (void)io->write_reg(io->ctx, TCA8418_ADDR7, REG_INT_STAT, INT_K_INT);  /* re-arm INT_N */
  return n;
}
