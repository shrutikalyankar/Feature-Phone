/* Host test: bt815.c against a simulated BT815 that speaks the real SPI framing. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bt815.h"
#include "bt815_regs.h"
#include "lcd_config.h"
#include "lcd_bringup.h"

#define MEM_SIZE 0x310000u
#define LOG_MAX 512

typedef struct { uint32_t addr; uint32_t val; unsigned len; } wlog_t;

typedef struct {
  uint8_t *mem;
  bool present, active, clkext, boot_done, fault;
  uint32_t now_ms, active_at, pclk_at, space;
  /* transaction state */
  bool cs; unsigned nbytes; uint8_t hdr[4]; bool is_write, is_read, is_host; uint32_t addr;
  /* logs */
  wlog_t wlog[LOG_MAX]; unsigned nw;
  uint8_t host_log[16], host_param[16]; unsigned nhost; unsigned clkext_idx, active_idx;
  uint8_t cmd_log[8192]; unsigned ncmd;
  uint32_t spi_hz;
  bool pd_level; uint32_t pd_fell_at, pd_low_ms; unsigned pd_events; unsigned host_at_pd_rise;
} fake_t;

static void fake_reg_read_fixup(fake_t *f, uint32_t addr, uint8_t *b)
{
  /* Dynamic registers: recomputed on every read byte. */
  uint32_t v; bool dyn = true;
  switch (addr & ~3u) {
    case BT815_REG_ID: v = f->boot_done ? BT815_ID_VALUE : 0; break;
    case BT815_REG_CPURESET: v = f->boot_done ? 0 : 2; break;
    case BT815_REG_FRAMES:
      v = (f->pclk_at && f->now_ms >= f->pclk_at) ? (f->now_ms - f->pclk_at) * 60u / 1000u : 0; break;
    case BT815_REG_CMDB_SPACE: v = f->space; break;
    case BT815_REG_CMD_READ: v = f->fault ? 0xFFFu : (uint32_t)(f->mem[BT815_REG_CMD_WRITE] | (f->mem[BT815_REG_CMD_WRITE + 1] << 8)); break;
    default: dyn = false; v = 0;
  }
  if (dyn) { *b = (uint8_t)(v >> (8 * (addr & 3u))); }
}

static void fake_cs(void *c, bool a)
{
  fake_t *f = c;
  if (!a && f->cs && f->present) {
    /* CS released: classify the finished transaction. */
    if (f->nbytes == 3 && f->hdr[2] == 0 && ((f->hdr[0] == 0x00 && f->hdr[1] == 0) || (f->hdr[0] & 0xC0) == 0x40)) {
      uint8_t cmd = f->hdr[0];
      f->host_log[f->nhost] = cmd; f->host_param[f->nhost] = f->hdr[1];
      if (cmd == BT815_HC_CLKEXT) { f->clkext = true; f->clkext_idx = f->nhost; }
      if (cmd == BT815_HC_ACTIVE) { f->active = true; f->active_idx = f->nhost; f->active_at = f->now_ms; }
      ++f->nhost;
    } else if (f->is_write && f->addr != BT815_REG_CMDB_WRITE && f->nbytes > 3 && f->nw < LOG_MAX) {
      unsigned len = f->nbytes - 3;
      uint32_t v = 0;
      for (unsigned i = 0; i < len && i < 4; ++i) { v |= (uint32_t)f->mem[f->addr + i] << (8 * i); }
      f->wlog[f->nw++] = (wlog_t){ f->addr, v, len };
      if (f->addr == BT815_REG_PCLK && f->mem[BT815_REG_PCLK] != 0) { f->pclk_at = f->now_ms; }
    }
  }
  f->cs = a; f->nbytes = 0; f->is_write = f->is_read = f->is_host = false;
}

static uint8_t fake_byte(fake_t *f, uint8_t tx)
{
  uint8_t rx = 0xFF;
  if (!f->present) { f->nbytes++; return 0xFF; }
  if (f->nbytes == 0) {
    f->hdr[0] = tx;
    f->is_host = (tx & 0xC0) == 0x40 || (tx == 0x00 && 0);
    f->is_write = (tx & 0xC0) == 0x80;
    f->is_read = (tx & 0xC0) == 0x00;
  } else if (f->nbytes < 4) {
    f->hdr[f->nbytes] = tx;
  }
  /* ACTIVE is host command 0x00: indistinguishable from a read of address 0x00xxxx until the
     third byte, so treat "0x00 0x00 0x00 then CS high with exactly 3 bytes" as host cmd at CS release. */
  if ((f->is_write || f->is_read) && f->nbytes == 2) {
    f->addr = ((uint32_t)(f->hdr[0] & 0x3F) << 16) | ((uint32_t)f->hdr[1] << 8) | f->hdr[2];
  }
  if (f->is_read && f->nbytes >= 4) {
    uint32_t a = f->addr + (f->nbytes - 4);
    rx = f->mem[a];
    fake_reg_read_fixup(f, a, &rx);
  }
  if (f->is_write && f->nbytes >= 3) {
    uint32_t a = f->addr + (f->nbytes - 3);
    if (f->addr == BT815_REG_CMDB_WRITE) {
      assert(f->ncmd < sizeof f->cmd_log);
      f->cmd_log[f->ncmd++] = tx;
      uint16_t w = (uint16_t)((f->mem[BT815_REG_CMD_WRITE] | (f->mem[BT815_REG_CMD_WRITE + 1] << 8)) + 1u) & 0xFFFu;
      f->mem[BT815_REG_CMD_WRITE] = (uint8_t)w; f->mem[BT815_REG_CMD_WRITE + 1] = (uint8_t)(w >> 8);
    } else {
      assert(a < MEM_SIZE);
      f->mem[a] = tx;
      if (f->nw < LOG_MAX && (a & 3u) == 0) {
        /* log at first byte of each write; width gathered at CS release */
      }
    }
  }
  f->nbytes++;
  return rx;
}

static void fake_xfer(void *c, const uint8_t *tx, uint8_t *rx, size_t n)
{
  fake_t *f = c;
  for (size_t i = 0; i < n; ++i) {
    uint8_t r = fake_byte(f, tx ? tx[i] : 0);
    if (rx) { rx[i] = r; }
  }
}

static void fake_delay(void *c, uint32_t ms)
{
  fake_t *f = c;
  f->now_ms += ms;
  if (f->active && f->now_ms - f->active_at >= 40u) { f->boot_done = true; }  /* boot takes 40 ms */
}
static uint32_t fake_millis(void *c) { return ((fake_t *)c)->now_ms; }
static void fake_set_hz(void *c, uint32_t hz) { ((fake_t *)c)->spi_hz = hz; }

static void fake_pd(void *c, bool level)
{
  fake_t *f = c;
  if (!level) { f->pd_fell_at = f->now_ms; }
  else { f->pd_low_ms = f->now_ms - f->pd_fell_at; f->host_at_pd_rise = f->nhost; }
  f->pd_level = level; ++f->pd_events;
}

static fake_t F;
static bt815_hal_t hal;

static void reset_fake(bool present)
{
  free(F.mem);
  memset(&F, 0, sizeof F);
  F.mem = calloc(MEM_SIZE, 1);
  F.present = present; F.space = 0xFFC;
  F.mem[BT815_ROM_CHIPID] = 0x08; F.mem[BT815_ROM_CHIPID + 1] = 0x15; F.mem[BT815_ROM_CHIPID + 2] = 0x01;
  hal = (bt815_hal_t){ &F, fake_cs, fake_xfer, fake_delay, fake_millis, fake_set_hz, fake_pd };
}

static const wlog_t *find_w(uint32_t addr, int nth)
{
  for (unsigned i = 0; i < F.nw; ++i) {
    if (F.wlog[i].addr == addr && nth-- == 0) { return &F.wlog[i]; }
  }
  return NULL;
}
static int idx_w(uint32_t addr)
{
  for (unsigned i = 0; i < F.nw; ++i) { if (F.wlog[i].addr == addr) { return (int)i; } }
  return -1;
}
static uint32_t cmd_word(unsigned i)
{
  return F.cmd_log[4*i] | (F.cmd_log[4*i+1] << 8) | (F.cmd_log[4*i+2] << 16) | ((uint32_t)F.cmd_log[4*i+3] << 24);
}

static void test_init_sequence(void)
{
  reset_fake(true);
  bt815_t d;
  assert(bt815_init(&d, &hal, &LCD_PANEL) == BT815_OK);
  /* Crystal selected before the chip is woken. */
  /* CLKEXT, then CLKSEL (x4 = 48 MHz: 0x44), then ACTIVE: CLKSEL only works with the PLL stopped. */
  assert(F.nhost == 3 && F.host_log[0] == BT815_HC_CLKEXT && F.host_log[1] == BT815_HC_CLKSEL
         && F.host_param[1] == 0x44 && F.host_log[2] == BT815_HC_ACTIVE);
  assert(F.clkext_idx < F.active_idx);
  assert(find_w(BT815_REG_FREQUENCY, 0)->val == 48000000u);
  /* Panel registers programmed. */
  assert(find_w(BT815_REG_HSIZE, 0)->val == 800 && find_w(BT815_REG_VSIZE, 0)->val == 480);
  assert(find_w(BT815_REG_HCYCLE, 0)->val == 816 && find_w(BT815_REG_VCYCLE, 0)->val == 512);
  assert(find_w(BT815_REG_PCLK_POL, 0)->val == 1);
  /* Backlight at 5 kHz and starts dark. */
  assert(find_w(BT815_REG_PWM_HZ, 0)->val == BT815_BACKLIGHT_HZ);
  assert(find_w(BT815_REG_PWM_DUTY, 0)->val == 0);
  /* DISP enable bit set. */
  assert(F.mem[BT815_REG_GPIOX + 1] & 0x80);
  /* Scan-out starts last: PCLK after timing, DL swap and DISP. */
  int pclk = idx_w(BT815_REG_PCLK);
  assert(pclk > idx_w(BT815_REG_VSYNC1) && pclk > idx_w(BT815_REG_DLSWAP) && pclk > idx_w(BT815_REG_GPIOX));
  assert((unsigned)pclk == F.nw - 1);
  assert(F.mem[BT815_REG_PCLK] == LCD_PANEL.pclk_div);
  /* Initial display list: clear colour, clear, display. */
  uint32_t dl0 = F.mem[BT815_RAM_DL] | (F.mem[BT815_RAM_DL+1] << 8) | (F.mem[BT815_RAM_DL+2] << 16) | ((uint32_t)F.mem[BT815_RAM_DL+3] << 24);
  assert(dl0 == BT815_DL_CLEAR_COLOR_RGB(0,0,0));
  assert(F.mem[BT815_RAM_DL + 8] == 0 && F.mem[BT815_RAM_DL + 11] == 0);
  assert(F.mem[BT815_REG_DLSWAP] == BT815_DLSWAP_FRAME);
  assert(F.spi_hz == 10000000u);
}

/* Check LCD_PANEL against the DLC-T5.0AHR#2 datasheet limits (p.10 timing table). */
static void test_panel_meets_datasheet(void)
{
  const bt815_panel_t *p = &LCD_PANEL;
  double sysclk = p->sys_mult ? 12.0 * p->sys_mult : 60.0;
  double dclk = sysclk / p->pclk_div;
  assert(dclk >= 23.0 && dclk <= 27.0);
  assert(p->hsize == 800 && p->vsize == 480);
  assert(p->hcycle >= 808 && p->hcycle <= 848);
  assert(p->vcycle >= 496 && p->vcycle <= 528);
  unsigned hw = p->hsync1 - p->hsync0, vw = p->vsync1 - p->vsync0;
  assert(hw >= 2 && hw <= 8 && vw >= 2 && vw <= 8);
  unsigned hbp = p->hoffset, vbp = p->voffset;           /* back porch counted from pulse start */
  unsigned hfp = p->hcycle - p->hsize - hbp, vfp = p->vcycle - p->vsize - vbp;
  assert(hbp >= 4 && hbp <= 24 && hfp >= 4 && hfp <= 24);
  assert(vbp >= 8 && vbp <= 24 && vfp >= 8 && vfp <= 24);
  assert(hbp == 8 && vbp == 16);                         /* required in SYNC mode */
  assert(hbp > hw && vbp > vw);                          /* pulse fits inside the back porch */
  double hz = dclk * 1e6 / ((double)p->hcycle * p->vcycle);
  assert(hz > 55.0 && hz < 62.0);
}

static void test_shutdown_order(void)
{
  reset_fake(true);
  bt815_t d;
  assert(bt815_init(&d, &hal, &LCD_PANEL) == BT815_OK);
  bt815_set_backlight(&d, 100);
  unsigned n0 = F.nw; uint32_t t0 = F.now_ms;
  bt815_shutdown(&d);
  assert(F.mem[BT815_REG_PWM_DUTY] == 0);
  assert(!(F.mem[BT815_REG_GPIOX + 1] & 0x80));          /* DISP low */
  assert(F.mem[BT815_REG_PCLK] == 0);
  assert(F.now_ms - t0 >= 100u);                         /* panel discharge time */
  assert(F.wlog[n0].addr == BT815_REG_PWM_DUTY);         /* backlight goes first */
}

static void test_hardware_reset(void)
{
  reset_fake(true);
  bt815_t d;
  assert(bt815_init(&d, &hal, &LCD_PANEL) == BT815_OK);
  /* PD_N pulsed low >= 5 ms, released before any host command, and left high. */
  assert(F.pd_events == 2 && F.pd_level);
  assert(F.pd_low_ms >= 5u);
  assert(F.host_at_pd_rise == 0);
  /* Same init works when the board has no PD_N control (RC reset only). */
  reset_fake(true);
  hal.pd_n = NULL;
  assert(bt815_init(&d, &hal, &LCD_PANEL) == BT815_OK);
  assert(F.pd_events == 0);
}

static void test_no_chip(void)
{
  reset_fake(false);
  bt815_t d;
  assert(bt815_init(&d, &hal, &LCD_PANEL) == BT815_ERR_NO_CHIP);
  assert(F.now_ms >= 500u && F.now_ms < 700u);   /* bounded wait, not a hang */
}

static void test_wrong_chip(void)
{
  reset_fake(true);
  F.mem[BT815_ROM_CHIPID + 1] = 0x13; /* FT813 */
  bt815_t d;
  assert(bt815_init(&d, &hal, &LCD_PANEL) == BT815_ERR_CHIPID);
}

static void test_bad_args(void)
{
  reset_fake(true);
  bt815_t d; bt815_panel_t p = LCD_PANEL; p.pclk_div = 0;
  assert(bt815_init(&d, &hal, &p) == BT815_ERR_ARG);
  p = LCD_PANEL; p.sys_mult = 7;
  assert(bt815_init(&d, &hal, &p) == BT815_ERR_ARG);
  assert(bt815_init(NULL, &hal, &LCD_PANEL) == BT815_ERR_ARG);
}

static void test_rw_roundtrip(void)
{
  reset_fake(true);
  bt815_t d;
  assert(bt815_init(&d, &hal, &LCD_PANEL) == BT815_OK);
  bt815_wr32(&d, BT815_RAM_G + 0x40, 0xDEADBEEFu);
  assert(bt815_rd32(&d, BT815_RAM_G + 0x40) == 0xDEADBEEFu);
  assert(bt815_rd8(&d, BT815_RAM_G + 0x41) == 0xBE);
  bt815_wr16(&d, BT815_RAM_G + 0x80, 0x1234);
  assert(bt815_rd16(&d, BT815_RAM_G + 0x80) == 0x1234);
  /* High address bits survive framing: RAM_DL at 0x300000. */
  bt815_wr32(&d, BT815_RAM_DL + 4, 0x11223344u);
  assert(bt815_rd32(&d, BT815_RAM_DL + 4) == 0x11223344u);
  assert(bt815_rd8(&d, BT815_REG_ID) == BT815_ID_VALUE);
}

static void test_frame_and_text(void)
{
  reset_fake(true);
  bt815_t d;
  assert(bt815_init(&d, &hal, &LCD_PANEL) == BT815_OK);
  F.ncmd = 0;
  bt815_frame_begin(&d);
  bt815_cmd(&d, BT815_DL_CLEAR(1,1,1));
  bt815_cmd_text(&d, 240, 20, 31, BT815_OPT_CENTER, "Hi!");   /* 4 bytes with NUL: no padding */
  bt815_cmd_text(&d, 1, 2, 28, 0, "abcde");                   /* 6 bytes: pads to 8 */
  assert(bt815_frame_end(&d) == BT815_OK);
  assert(F.ncmd % 4 == 0);
  assert(cmd_word(0) == BT815_CMD_DLSTART);
  assert(cmd_word(1) == BT815_DL_CLEAR(1,1,1));
  assert(cmd_word(2) == BT815_CMD_TEXT);
  assert(cmd_word(3) == (240u | (20u << 16)));
  assert(cmd_word(4) == (31u | ((uint32_t)BT815_OPT_CENTER << 16)));
  assert(memcmp(&F.cmd_log[20], "Hi!\0", 4) == 0);
  assert(cmd_word(6) == BT815_CMD_TEXT);
  assert(memcmp(&F.cmd_log[36], "abcde\0\0\0", 8) == 0);
  assert(cmd_word(11) == BT815_DL_DISPLAY());
  assert(cmd_word(12) == BT815_CMD_SWAP);
  assert(F.ncmd == 13 * 4);
}

static void test_flow_control_and_chunking(void)
{
  reset_fake(true);
  bt815_t d;
  assert(bt815_init(&d, &hal, &LCD_PANEL) == BT815_OK);
  F.ncmd = 0;
  /* 300 words = 1200 B > chunk: must be split and arrive intact and in order. */
  for (uint32_t i = 0; i < 300; ++i) { bt815_cmd(&d, 0x01000000u + i); }
  assert(bt815_cmd_wait_idle(&d) == BT815_OK);
  assert(F.ncmd == 1200);
  for (uint32_t i = 0; i < 300; ++i) { assert(cmd_word(i) == 0x01000000u + i); }
  /* Not enough FIFO space: times out instead of overrunning the FIFO. */
  F.space = 8; F.ncmd = 0;
  for (int i = 0; i < 10; ++i) { bt815_cmd(&d, 0x12345678u); }
  assert(bt815_cmd_flush(&d) == BT815_ERR_TIMEOUT);
  assert(F.ncmd == 0);
}

static void test_fault(void)
{
  reset_fake(true);
  bt815_t d;
  assert(bt815_init(&d, &hal, &LCD_PANEL) == BT815_OK);
  F.fault = true;
  bt815_frame_begin(&d);
  assert(bt815_frame_end(&d) == BT815_ERR_FAULT);
}

static void test_backlight_clamp(void)
{
  reset_fake(true);
  bt815_t d;
  assert(bt815_init(&d, &hal, &LCD_PANEL) == BT815_OK);
  bt815_set_backlight(&d, 200);
  assert(F.mem[BT815_REG_PWM_DUTY] == 128);
  bt815_set_backlight(&d, 64);
  assert(F.mem[BT815_REG_PWM_DUTY] == 64);
}

static void test_bringup_app(void)
{
  reset_fake(true);
  assert(lcd_bringup_start(&hal) == BT815_OK);
  assert(lcd_selftest.ram_pattern_ok);
  assert(lcd_selftest.chip_id[0] == 0x08 && lcd_selftest.chip_id[1] == 0x15);
  assert(lcd_selftest.frames_after > lcd_selftest.frames_before);   /* scan-out is running */
  assert(F.mem[BT815_REG_PWM_DUTY] == 96);
  /* Screen content contains the title and uptime string. */
  assert(memmem(F.cmd_log, F.ncmd, "Feature Phone", 13));
  assert(lcd_bringup_tick(7) == BT815_OK);
  assert(memmem(F.cmd_log, F.ncmd, "uptime 7 s", 10));
}

int main(void)
{
  test_panel_meets_datasheet(); test_shutdown_order(); test_init_sequence(); test_hardware_reset(); test_no_chip(); test_wrong_chip(); test_bad_args();
  test_rw_roundtrip(); test_frame_and_text(); test_flow_control_and_chunking();
  test_fault(); test_backlight_clamp(); test_bringup_app();
  puts("bt815 host tests passed");
  return 0;
}
