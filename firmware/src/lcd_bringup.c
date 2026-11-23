/* SPDX-License-Identifier: MIT */
#include "lcd_bringup.h"
#include "bt815_regs.h"
#include "lcd_config.h"
#include <stdio.h>
#include <string.h>

volatile lcd_selftest_t lcd_selftest;
static bt815_t lcd;

/* Steps that need no panel at all: they prove SPI wiring, clock and boot. */
static void run_selftest(bt815_t *d)
{
  static const uint8_t pat[8] = { 0xA5, 0x5A, 0x00, 0xFF, 0x12, 0x34, 0x56, 0x78 };
  uint8_t back[8] = { 0 };
  bt815_write(d, BT815_RAM_G + 0x1000u, pat, sizeof pat);
  bt815_read(d, BT815_RAM_G + 0x1000u, back, sizeof back);
  lcd_selftest.ram_pattern_ok = memcmp(pat, back, sizeof pat) == 0;
  bt815_read(d, BT815_ROM_CHIPID, (uint8_t *)lcd_selftest.chip_id, 4);
  lcd_selftest.frames_before = bt815_rd32(d, BT815_REG_FRAMES);
  d->hal.delay_ms(d->hal.ctx, 100u);
  lcd_selftest.frames_after = bt815_rd32(d, BT815_REG_FRAMES);
}

static void draw_bar(bt815_t *d, int x0, int x1, int y0, int y1, uint8_t r, uint8_t g, uint8_t b)
{
  bt815_cmd(d, BT815_DL_COLOR_RGB(r, g, b));
  bt815_cmd(d, BT815_DL_VERTEX2F(x0, y0));
  bt815_cmd(d, BT815_DL_VERTEX2F(x1, y1));
}

static bt815_status_t draw_screen(bt815_t *d, uint32_t seconds)
{
  const int w = d->panel->hsize, h = d->panel->vsize;
  static const uint8_t bars[8][3] = {
    {255,255,255},{255,255,0},{0,255,255},{0,255,0},{255,0,255},{255,0,0},{0,0,255},{0,0,0}
  };
  char line[40];

  bt815_frame_begin(d);
  bt815_cmd(d, BT815_DL_CLEAR_COLOR_RGB(16, 16, 48));
  bt815_cmd(d, BT815_DL_CLEAR(1, 1, 1));
  bt815_cmd(d, BT815_DL_VERTEX_FORMAT(0));                 /* whole-pixel coordinates */
  bt815_cmd(d, BT815_DL_BEGIN(BT815_PRIM_RECTS));
  for (int i = 0; i < 8; ++i) {                            /* colour bars across the bottom third */
    draw_bar(d, i * w / 8, (i + 1) * w / 8, h * 2 / 3, h, bars[i][0], bars[i][1], bars[i][2]);
  }
  draw_bar(d, 0, w, 0, 4, 255, 0, 0);                      /* edge markers: check no clipping/offset */
  draw_bar(d, 0, w, h - 4, h, 0, 255, 0);
  draw_bar(d, 0, 4, 0, h, 255, 255, 0);
  draw_bar(d, w - 4, w, 0, h, 0, 255, 255);
  bt815_cmd(d, BT815_DL_END());
  bt815_cmd(d, BT815_DL_COLOR_RGB(255, 255, 255));
  bt815_cmd_text(d, (int16_t)(w / 2), (int16_t)(h / 8), 31, BT815_OPT_CENTER, "Feature Phone");
  snprintf(line, sizeof line, "BT815 %02X%02X  %ux%u", lcd_selftest.chip_id[0], lcd_selftest.chip_id[1],
           (unsigned)w, (unsigned)h);
  bt815_cmd_text(d, (int16_t)(w / 2), (int16_t)(h * 3 / 10), 28, BT815_OPT_CENTER, line);
  snprintf(line, sizeof line, "uptime %lu s", (unsigned long)seconds);
  bt815_cmd_text(d, (int16_t)(w / 2), (int16_t)(h * 4 / 10), 28, BT815_OPT_CENTER, line);
  return bt815_frame_end(d);
}

bt815_status_t lcd_bringup_start(const bt815_hal_t *hal)
{
  memset((void *)&lcd_selftest, 0, sizeof lcd_selftest);
  bt815_status_t st = bt815_init(&lcd, hal, &LCD_PANEL);
  lcd_selftest.init_status = st;
  if (st != BT815_OK) {
    return st;
  }
  run_selftest(&lcd);
  st = draw_screen(&lcd, 0);
  lcd_selftest.draw_status = st;
  if (st != BT815_OK) {
    return st;
  }
  /* Panel datasheet power-on order: signal first, backlight >= 100 ms later. */
  hal->delay_ms(hal->ctx, LCD_BACKLIGHT_DELAY_MS);
  /* Fade the backlight up after the first real frame so there is no white flash. */
  for (uint8_t duty = 0; duty <= 96u; duty = (uint8_t)(duty + 8u)) {
    bt815_set_backlight(&lcd, duty);
    hal->delay_ms(hal->ctx, 15u);
  }
  return BT815_OK;
}

bt815_status_t lcd_bringup_tick(uint32_t seconds)
{
  bt815_status_t st = draw_screen(&lcd, seconds);
  lcd_selftest.draw_status = st;
  return st;
}
