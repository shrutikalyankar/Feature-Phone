/* SPDX-License-Identifier: MIT */
#include "bt815.h"
#include "bt815_regs.h"
#include <string.h>

/* SPI framing (datasheet 4.1): write = 10 + 22-bit addr, read = 00 + addr + 1 dummy byte. */
static void addr_hdr(uint8_t *h, uint32_t addr, uint8_t prefix)
{
  h[0] = (uint8_t)(prefix | ((addr >> 16) & 0x3Fu));
  h[1] = (uint8_t)(addr >> 8);
  h[2] = (uint8_t)addr;
}

void bt815_host_cmd(bt815_t *d, uint8_t cmd, uint8_t param)
{
  uint8_t b[3] = { cmd, param, 0x00 };
  d->hal.cs(d->hal.ctx, true);
  d->hal.xfer(d->hal.ctx, b, NULL, 3);
  d->hal.cs(d->hal.ctx, false);
}

void bt815_read(bt815_t *d, uint32_t addr, uint8_t *dst, size_t len)
{
  uint8_t h[4];
  addr_hdr(h, addr, 0x00);
  h[3] = 0x00; /* dummy */
  d->hal.cs(d->hal.ctx, true);
  d->hal.xfer(d->hal.ctx, h, NULL, 4);
  d->hal.xfer(d->hal.ctx, NULL, dst, len);
  d->hal.cs(d->hal.ctx, false);
}

void bt815_write(bt815_t *d, uint32_t addr, const uint8_t *src, size_t len)
{
  uint8_t h[3];
  addr_hdr(h, addr, 0x80);
  d->hal.cs(d->hal.ctx, true);
  d->hal.xfer(d->hal.ctx, h, NULL, 3);
  d->hal.xfer(d->hal.ctx, src, NULL, len);
  d->hal.cs(d->hal.ctx, false);
}

uint8_t bt815_rd8(bt815_t *d, uint32_t a) { uint8_t v; bt815_read(d, a, &v, 1); return v; }
uint16_t bt815_rd16(bt815_t *d, uint32_t a)
{
  uint8_t v[2];
  bt815_read(d, a, v, 2);
  return (uint16_t)(v[0] | (v[1] << 8));
}
uint32_t bt815_rd32(bt815_t *d, uint32_t a)
{
  uint8_t v[4];
  bt815_read(d, a, v, 4);
  return (uint32_t)v[0] | ((uint32_t)v[1] << 8) | ((uint32_t)v[2] << 16) | ((uint32_t)v[3] << 24);
}
void bt815_wr8(bt815_t *d, uint32_t a, uint8_t v) { bt815_write(d, a, &v, 1); }
void bt815_wr16(bt815_t *d, uint32_t a, uint16_t v)
{
  uint8_t b[2] = { (uint8_t)v, (uint8_t)(v >> 8) };
  bt815_write(d, a, b, 2);
}
void bt815_wr32(bt815_t *d, uint32_t a, uint32_t v)
{
  uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
  bt815_write(d, a, b, 4);
}

static void program_panel(bt815_t *d, const bt815_panel_t *p)
{
  bt815_wr16(d, BT815_REG_HSIZE, p->hsize);
  bt815_wr16(d, BT815_REG_HCYCLE, p->hcycle);
  bt815_wr16(d, BT815_REG_HOFFSET, p->hoffset);
  bt815_wr16(d, BT815_REG_HSYNC0, p->hsync0);
  bt815_wr16(d, BT815_REG_HSYNC1, p->hsync1);
  bt815_wr16(d, BT815_REG_VSIZE, p->vsize);
  bt815_wr16(d, BT815_REG_VCYCLE, p->vcycle);
  bt815_wr16(d, BT815_REG_VOFFSET, p->voffset);
  bt815_wr16(d, BT815_REG_VSYNC0, p->vsync0);
  bt815_wr16(d, BT815_REG_VSYNC1, p->vsync1);
  bt815_wr8(d, BT815_REG_SWIZZLE, p->swizzle);
  bt815_wr8(d, BT815_REG_PCLK_POL, p->pclk_pol);
  bt815_wr8(d, BT815_REG_CSPREAD, p->cspread);
  bt815_wr8(d, BT815_REG_DITHER, p->dither);
  bt815_wr16(d, BT815_REG_OUTBITS, 0); /* 8 bits per channel (R0..R7, G0..G7, B0..B7 all routed to the panel connector) */
}

bt815_status_t bt815_init(bt815_t *d, const bt815_hal_t *hal, const bt815_panel_t *panel)
{
  if (!d || !hal || !panel || !hal->cs || !hal->xfer || !hal->delay_ms || !hal->millis
      || panel->pclk_div == 0u || panel->hsize == 0u || panel->vsize == 0u) {
    return BT815_ERR_ARG;
  }
  memset(d, 0, sizeof *d);
  d->hal = *hal;
  d->panel = panel;
  d->io_timeout_ms = 500u;

  d->hal.cs(d->hal.ctx, false);
  if (d->hal.pd_n) {
    /* PD_N is an async hardware reset: low for >= 5 ms (datasheet 4.9.2, table 6-8). */
    d->hal.pd_n(d->hal.ctx, false);
    d->hal.delay_ms(d->hal.ctx, 10u);
    d->hal.pd_n(d->hal.ctx, true);
    d->hal.delay_ms(d->hal.ctx, 25u);   /* 47k/0.1uF on PD_N also delays the release */
  } else {
    /* No PD_N control: the 47k/0.1uF RC (~5 ms) is the only reset. Let it settle. */
    d->hal.delay_ms(d->hal.ctx, 25u);
  }

  /* External 12 MHz crystal (Y on the Display_Driver sheet), then wake. 60 MHz sysclk. */
  bt815_host_cmd(d, BT815_HC_CLKEXT, 0x00);
  bt815_host_cmd(d, BT815_HC_ACTIVE, 0x00);

  /* Boot can take up to 300 ms (datasheet 4.9.4): poll REG_ID. */
  uint32_t t0 = d->hal.millis(d->hal.ctx);
  while (bt815_rd8(d, BT815_REG_ID) != BT815_ID_VALUE) {
    if (d->hal.millis(d->hal.ctx) - t0 > d->io_timeout_ms) {
      return BT815_ERR_NO_CHIP;
    }
    d->hal.delay_ms(d->hal.ctx, 1u);
  }
  t0 = d->hal.millis(d->hal.ctx);
  while (bt815_rd16(d, BT815_REG_CPURESET) != 0u) {
    if (d->hal.millis(d->hal.ctx) - t0 > d->io_timeout_ms) {
      return BT815_ERR_BOOT;
    }
    d->hal.delay_ms(d->hal.ctx, 1u);
  }

  uint8_t id[4];
  bt815_read(d, BT815_ROM_CHIPID, id, 4);
  if (id[0] != 0x08u || (id[1] != 0x15u && id[1] != 0x16u)) {
    return BT815_ERR_CHIPID;
  }

  bt815_wr32(d, BT815_REG_FREQUENCY, 60000000ul);
  bt815_wr8(d, BT815_REG_PWM_DUTY, 0);              /* backlight off until first frame */
  bt815_wr16(d, BT815_REG_PWM_HZ, BT815_BACKLIGHT_HZ);
  program_panel(d, panel);

  /* First frame: black. Written straight into RAM_DL, then swapped in. */
  const uint32_t dl[3] = { BT815_DL_CLEAR_COLOR_RGB(0, 0, 0), BT815_DL_CLEAR(1, 1, 1), BT815_DL_DISPLAY() };
  uint8_t raw[12];
  for (int i = 0; i < 3; ++i) {
    raw[4 * i] = (uint8_t)dl[i]; raw[4 * i + 1] = (uint8_t)(dl[i] >> 8);
    raw[4 * i + 2] = (uint8_t)(dl[i] >> 16); raw[4 * i + 3] = (uint8_t)(dl[i] >> 24);
  }
  bt815_write(d, BT815_RAM_DL, raw, sizeof raw);
  bt815_wr8(d, BT815_REG_DLSWAP, BT815_DLSWAP_FRAME);

  /* DISP pin is the panel enable (LCD_DISPLAY_ENABLE). Read-modify-write bit 15. */
  bt815_wr16(d, BT815_REG_GPIOX_DIR, (uint16_t)(bt815_rd16(d, BT815_REG_GPIOX_DIR) | BT815_GPIOX_DISP));
  bt815_wr16(d, BT815_REG_GPIOX, (uint16_t)(bt815_rd16(d, BT815_REG_GPIOX) | BT815_GPIOX_DISP));

  /* Starting PCLK last starts the scan-out. */
  bt815_wr8(d, BT815_REG_PCLK, panel->pclk_div);

  if (d->hal.set_spi_hz) {
    d->hal.set_spi_hz(d->hal.ctx, 10000000u);
  }
  return BT815_OK;
}

void bt815_set_backlight(bt815_t *d, uint8_t duty)
{
  if (duty > BT815_PWM_DUTY_MAX) {
    duty = BT815_PWM_DUTY_MAX;
  }
  bt815_wr8(d, BT815_REG_PWM_DUTY, duty);
}

/* ---- Coprocessor FIFO ---- */

bt815_status_t bt815_cmd_flush(bt815_t *d)
{
  if (d->cmdlen == 0u) {
    return BT815_OK;
  }
  uint32_t t0 = d->hal.millis(d->hal.ctx);
  for (;;) {
    if (bt815_rd16(d, BT815_REG_CMD_READ) == BT815_CMD_FAULT_PTR) {
      d->cmdlen = 0;
      return BT815_ERR_FAULT;
    }
    if (bt815_rd16(d, BT815_REG_CMDB_SPACE) >= d->cmdlen) {
      break;
    }
    if (d->hal.millis(d->hal.ctx) - t0 > d->io_timeout_ms) {
      d->cmdlen = 0;
      return BT815_ERR_TIMEOUT;
    }
    d->hal.delay_ms(d->hal.ctx, 1u);
  }
  bt815_write(d, BT815_REG_CMDB_WRITE, d->cmdbuf, d->cmdlen);
  d->cmdlen = 0;
  return BT815_OK;
}

static void put_byte(bt815_t *d, uint8_t b)
{
  if (d->cmdlen == BT815_CMD_CHUNK) {
    (void)bt815_cmd_flush(d);   /* errors resurface in the next flush/wait_idle */
  }
  d->cmdbuf[d->cmdlen++] = b;
}

void bt815_cmd(bt815_t *d, uint32_t w)
{
  put_byte(d, (uint8_t)w); put_byte(d, (uint8_t)(w >> 8));
  put_byte(d, (uint8_t)(w >> 16)); put_byte(d, (uint8_t)(w >> 24));
}

void bt815_cmd_text(bt815_t *d, int16_t x, int16_t y, int16_t font, uint16_t options, const char *s)
{
  bt815_cmd(d, BT815_CMD_TEXT);
  bt815_cmd(d, (uint16_t)x | ((uint32_t)(uint16_t)y << 16));
  bt815_cmd(d, (uint16_t)font | ((uint32_t)options << 16));
  size_t n = strlen(s) + 1u;           /* include NUL */
  for (size_t i = 0; i < n; ++i) {
    put_byte(d, (uint8_t)s[i]);
  }
  while (d->cmdlen & 3u) {             /* pad to a 4-byte boundary */
    put_byte(d, 0);
  }
}

bt815_status_t bt815_cmd_wait_idle(bt815_t *d)
{
  bt815_status_t st = bt815_cmd_flush(d);
  if (st != BT815_OK) {
    return st;
  }
  uint32_t t0 = d->hal.millis(d->hal.ctx);
  for (;;) {
    uint16_t rd = bt815_rd16(d, BT815_REG_CMD_READ);
    if (rd == BT815_CMD_FAULT_PTR) {
      return BT815_ERR_FAULT;
    }
    if (rd == (bt815_rd16(d, BT815_REG_CMD_WRITE) & 0x0FFFu)) {
      return BT815_OK;
    }
    if (d->hal.millis(d->hal.ctx) - t0 > d->io_timeout_ms) {
      return BT815_ERR_TIMEOUT;
    }
    d->hal.delay_ms(d->hal.ctx, 1u);
  }
}

bt815_status_t bt815_frame_begin(bt815_t *d)
{
  bt815_cmd(d, BT815_CMD_DLSTART);
  return BT815_OK;
}

bt815_status_t bt815_frame_end(bt815_t *d)
{
  bt815_cmd(d, BT815_DL_DISPLAY());
  bt815_cmd(d, BT815_CMD_SWAP);
  return bt815_cmd_wait_idle(d);
}
