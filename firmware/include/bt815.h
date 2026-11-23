/* SPDX-License-Identifier: MIT */
/* Portable BT815 (EVE2) driver: SPI memory/host-command access, boot sequence,
 * RGB panel timing and the coprocessor command FIFO. No MCU headers here, so it
 * builds on a PC against a simulated chip. */
#ifndef BT815_H
#define BT815_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  BT815_OK = 0,
  BT815_ERR_ARG,
  BT815_ERR_NO_CHIP,   /* REG_ID never read 0x7C: wiring, power, clock or SPI mode */
  BT815_ERR_BOOT,      /* REG_CPURESET never cleared after ACTIVE */
  BT815_ERR_CHIPID,    /* ROM chip id at 0xC0000 is not BT815/BT816 */
  BT815_ERR_TIMEOUT,   /* coprocessor did not drain */
  BT815_ERR_FAULT      /* coprocessor fault (REG_CMD_READ == 0xFFF) */
} bt815_status_t;

/* Board support supplied by the caller. tx may be NULL (clock out zeros),
 * rx may be NULL (discard). CS must stay low for a whole transaction. */
typedef struct {
  void *ctx;
  void (*cs)(void *ctx, bool asserted);
  void (*xfer)(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len);
  void (*delay_ms)(void *ctx, uint32_t ms);
  uint32_t (*millis)(void *ctx);
  /* Optional: raise SCK after boot. May be NULL. */
  void (*set_spi_hz)(void *ctx, uint32_t hz);
  /* Optional: drive the PD_N pin (true = high). When present, bt815_init() does a
   * real hardware reset (>= 5 ms low). When NULL the RC power-on reset is relied on. */
  void (*pd_n)(void *ctx, bool level);
} bt815_hal_t;

/* RGB panel timing, straight from the panel datasheet (see docs/lcd_bringup.md). */
typedef struct {
  uint16_t hsize, hcycle, hoffset, hsync0, hsync1;
  uint16_t vsize, vcycle, voffset, vsync0, vsync1;
  uint8_t pclk_div;     /* PCLK = 60 MHz / pclk_div */
  uint8_t pclk_pol;     /* 0: data changes on rising edge */
  uint8_t swizzle;      /* RGB pin order, 0 = R/G/B straight through */
  uint8_t cspread;
  uint8_t dither;
} bt815_panel_t;

#define BT815_CMD_CHUNK 256u   /* bytes buffered before a FIFO flush; multiple of 4 */

typedef struct {
  bt815_hal_t hal;
  const bt815_panel_t *panel;
  uint8_t cmdbuf[BT815_CMD_CHUNK];
  size_t cmdlen;
  uint32_t io_timeout_ms;
} bt815_t;

/* Raw access */
void bt815_host_cmd(bt815_t *d, uint8_t cmd, uint8_t param);
void bt815_read(bt815_t *d, uint32_t addr, uint8_t *dst, size_t len);
void bt815_write(bt815_t *d, uint32_t addr, const uint8_t *src, size_t len);
uint8_t bt815_rd8(bt815_t *d, uint32_t addr);
uint16_t bt815_rd16(bt815_t *d, uint32_t addr);
uint32_t bt815_rd32(bt815_t *d, uint32_t addr);
void bt815_wr8(bt815_t *d, uint32_t addr, uint8_t v);
void bt815_wr16(bt815_t *d, uint32_t addr, uint16_t v);
void bt815_wr32(bt815_t *d, uint32_t addr, uint32_t v);

/* Boot the chip on the external 12 MHz crystal, program the panel, enable DISP.
 * Backlight stays off (duty 0) until bt815_set_backlight(). */
bt815_status_t bt815_init(bt815_t *d, const bt815_hal_t *hal, const bt815_panel_t *panel);

/* 0..128 (128 = 100 %), PWM at BT815_BACKLIGHT_HZ. */
void bt815_set_backlight(bt815_t *d, uint8_t duty);

/* Coprocessor FIFO. Words are buffered and sent via REG_CMDB_WRITE with flow control. */
void bt815_cmd(bt815_t *d, uint32_t word);
void bt815_cmd_text(bt815_t *d, int16_t x, int16_t y, int16_t font, uint16_t options, const char *s);
bt815_status_t bt815_cmd_flush(bt815_t *d);
bt815_status_t bt815_cmd_wait_idle(bt815_t *d);
bt815_status_t bt815_frame_begin(bt815_t *d);
bt815_status_t bt815_frame_end(bt815_t *d);   /* DISPLAY + SWAP, flush, wait idle */

#ifndef BT815_BACKLIGHT_HZ
/* TPS61169 CTRL wants 5-100 kHz PWM; BT815 REG_PWM_HZ tops out at 10 kHz. */
#define BT815_BACKLIGHT_HZ 5000u
#endif

#endif
