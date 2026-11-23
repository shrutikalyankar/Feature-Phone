/* Dumps the coprocessor stream for each UI screen to files, for tools/preview_ui.py. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ui.h"
#include "ui_render.h"
#include "bt815_regs.h"

static FILE *out;
static uint8_t hdr[4]; static unsigned nb; static bool wr; static uint32_t addr;
static void cs(void *c, bool a) { (void)c; nb = 0; wr = false; (void)a; }
static void xfer(void *c, const uint8_t *tx, uint8_t *rx, size_t n)
{
  (void)c;
  for (size_t i = 0; i < n; ++i, ++nb) {
    uint8_t t = tx ? tx[i] : 0;
    if (nb < 4) { hdr[nb] = t; }
    if (nb == 0) { wr = (t & 0xC0) == 0x80; }
    if (nb == 2) { addr = ((uint32_t)(hdr[0] & 0x3F) << 16) | (hdr[1] << 8) | hdr[2]; }
    if (wr && nb >= 3 && addr == BT815_REG_CMDB_WRITE) { fputc(t, out); }
    if (rx) { rx[i] = 0; if (!wr && nb >= 4 && (addr & ~3u) == BT815_REG_CMDB_SPACE) { rx[i] = (nb - 4) == 0 ? 0xFC : 0x0F; } }
  }
}
static void delay(void *c, uint32_t ms) { (void)c; (void)ms; }
static uint32_t now; static uint32_t millis(void *c) { (void)c; return now += 1; }

static const ui_contact_t CONTACTS[] = {
  { "Alice Johnson", "303-555-0101" }, { "Bob Martinez", "720-555-0142" }, { "Chris Lee", "303-555-0177" },
  { "Dana Whitfield", "970-555-0188" }, { "Emergency", "911" }, { "Frank Osei", "303-555-0123" },
  { "Grace Kim", "720-555-0199" }, { "Hafsa S.", "303-555-0160" },
};

int main(int argc, char **argv)
{
  const char *dir = argc > 1 ? argv[1] : ".";
  bt815_panel_t panel = { .hsize = UI_SCREEN_W, .vsize = UI_SCREEN_H, .pclk_div = 2 };
  bt815_t d; memset(&d, 0, sizeof d);
  d.hal = (bt815_hal_t){ NULL, cs, xfer, delay, millis, NULL, NULL };
  d.panel = &panel; d.io_timeout_ms = 500;
  ui_t u; ui_init(&u, CONTACTS, 8, NULL);
  ui_status_t st = { .signal_bars = 4, .battery_pct = 72, .charging = false, .hour = 14, .minute = 7, .carrier = "Carrier" };
  struct { const char *name; void (*setup)(ui_t *); } shots[6];
  (void)shots;
  const char *names[] = { "home", "contacts", "dialer", "incall", "settings", "pressed" };
  for (int s = 0; s < 6; ++s) {
    ui_init(&u, CONTACTS, 8, NULL);
    switch (s) {
      case 1: ui_handle_key(&u, KEY_SOFT2, true, 0); u.contact_sel = 2; break;
      case 2: ui_handle_key(&u, KEY_SOFT1, true, 0);
              for (const char *p = "7205550142"; *p; ++p) { ui_handle_key(&u, p[0] == '7' ? KEY_7 : p[0] == '2' ? KEY_2 : p[0] == '0' ? KEY_0 : p[0] == '5' ? KEY_5 : p[0] == '1' ? KEY_1 : p[0] == '4' ? KEY_4 : KEY_0, true, 0); }
              break;
      case 3: ui_handle_key(&u, KEY_SOFT2, true, 0); u.contact_sel = 1; ui_handle_key(&u, KEY_SOFT1, true, 0); break;
      case 4: ui_handle_key(&u, KEY_SOFT4, true, 0); break;
      case 5: ui_handle_key(&u, KEY_SOFT2, true, 0); break;
      default: break;
    }
    if (s != 5) { u.pressed_soft = -1; }
    char path[512]; snprintf(path, sizeof path, "%s/%s.bin", dir, names[s]);
    out = fopen(path, "wb");
    ui_render(&d, &u, &st);
    fclose(out);
  }
  return 0;
}
