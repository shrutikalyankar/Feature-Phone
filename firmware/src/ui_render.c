/* SPDX-License-Identifier: MIT */
#include "ui_render.h"
#include "bt815_regs.h"
#include <math.h>
#include <stdio.h>

#define PI_F 3.14159265f
#define FONT_SMALL 26
#define FONT_MED   27
#define FONT_LARGE 29
#define FONT_XL    30
#define FONT_HUGE  31

int ui_softkey_center_y(int idx)
{
#if UI_SOFTKEYS_REVERSED
  idx = 3 - idx;
#endif
  return UI_SCREEN_H / 2 + (2 * idx - 3) * (UI_SOFT_PITCH / 2);
}

/* ---- tiny drawing helpers: every one sets its own colour and primitive ---- */
static void col(bt815_t *d, uint8_t r, uint8_t g, uint8_t b) { bt815_cmd(d, BT815_DL_COLOR_RGB(r, g, b)); }
static void vtx(bt815_t *d, int x, int y) { bt815_cmd(d, BT815_DL_VERTEX2F(x, y)); }

/* Rounded rectangle. RECTS grow by the corner radius on every side, so inset the vertices. */
static void rrect(bt815_t *d, int x0, int y0, int x1, int y1, int r)
{
  bt815_cmd(d, BT815_DL_LINE_WIDTH(r * 16));
  bt815_cmd(d, BT815_DL_BEGIN(BT815_PRIM_RECTS));
  vtx(d, x0 + r, y0 + r); vtx(d, x1 - r, y1 - r);
  bt815_cmd(d, BT815_DL_END());
}
/* Plain rectangle covering exactly x0..x1, y0..y1 (LINE_WIDTH is sticky, so set it every time). */
static void rect(bt815_t *d, int x0, int y0, int x1, int y1) { rrect(d, x0, y0, x1, y1, 1); }
static void dot(bt815_t *d, int x, int y, int radius)
{
  bt815_cmd(d, BT815_DL_POINT_SIZE(radius * 16));
  bt815_cmd(d, BT815_DL_BEGIN(BT815_PRIM_POINTS));
  vtx(d, x, y);
  bt815_cmd(d, BT815_DL_END());
}
static void strip(bt815_t *d, const int (*p)[2], int n, int width)
{
  bt815_cmd(d, BT815_DL_LINE_WIDTH(width * 8));         /* width is the full thickness */
  bt815_cmd(d, BT815_DL_BEGIN(BT815_PRIM_LINE_STRIP));
  for (int i = 0; i < n; ++i) { vtx(d, p[i][0], p[i][1]); }
  bt815_cmd(d, BT815_DL_END());
}
static void line(bt815_t *d, int x0, int y0, int x1, int y1, int width)
{
  bt815_cmd(d, BT815_DL_LINE_WIDTH(width * 8));
  bt815_cmd(d, BT815_DL_BEGIN(BT815_PRIM_LINES));
  vtx(d, x0, y0); vtx(d, x1, y1);
  bt815_cmd(d, BT815_DL_END());
}
/* center = true: centred on (x, y). false: left edge at x, vertically centred on y. */
static void text(bt815_t *d, int x, int y, int font, bool center, const char *s)
{
  bt815_cmd_text(d, (int16_t)x, (int16_t)y, (int16_t)font, center ? BT815_OPT_CENTER : BT815_OPT_CENTERY, s);
}

/* ---- icons, centred on (cx, cy), roughly 40 px square ---- */
static void handset(bt815_t *d, int cx, int cy, float rot_deg)
{
  /* Quadratic curve from earpiece (top-left) to mouthpiece (bottom-right), bulging
   * toward the bottom-left, with a short pad at each end. Rotated about the centre. */
  const float rot = rot_deg * PI_F / 180.0f, cr = cosf(rot), sr = sinf(rot);
  const float P0[2] = { -14.0f, -14.0f }, C[2] = { -16.0f, 16.0f }, P2[2] = { 14.0f, 14.0f };
  int pts[9][2];
  for (int i = 0; i < 9; ++i) {
    float t = i / 8.0f, u = 1.0f - t;
    float x = u * u * P0[0] + 2 * u * t * C[0] + t * t * P2[0];
    float y = u * u * P0[1] + 2 * u * t * C[1] + t * t * P2[1];
    pts[i][0] = cx + (int)(x * cr - y * sr);
    pts[i][1] = cy + (int)(x * sr + y * cr);
  }
  strip(d, (const int (*)[2])pts, 9, 8);
  /* pads: earpiece extends right, mouthpiece extends up (before rotation) */
  const float pads[2][4] = { { -14.0f, -14.0f, -2.0f, -17.0f }, { 14.0f, 14.0f, 17.0f, 2.0f } };
  for (int i = 0; i < 2; ++i) {
    line(d, cx + (int)(pads[i][0] * cr - pads[i][1] * sr), cy + (int)(pads[i][0] * sr + pads[i][1] * cr),
            cx + (int)(pads[i][2] * cr - pads[i][3] * sr), cy + (int)(pads[i][2] * sr + pads[i][3] * cr), 11);
  }
}

static void draw_icon(bt815_t *d, ui_icon_t icon, int cx, int cy, uint8_t br, uint8_t bg, uint8_t bb)
{
  col(d, 255, 255, 255);
  switch (icon) {
    case ICON_CALL: handset(d, cx, cy, 0.0f); break;
    case ICON_END:  handset(d, cx, cy, 135.0f); break;
    case ICON_CONTACTS: {
      dot(d, cx, cy - 11, 9);
      int sh[7][2];
      for (int i = 0; i < 7; ++i) {
        float a = (180.0f + i * 30.0f) * PI_F / 180.0f;
        sh[i][0] = cx + (int)(17.0f * cosf(a)); sh[i][1] = cy + 20 + (int)(15.0f * sinf(a));
      }
      strip(d, (const int (*)[2])sh, 7, 7);
      break;
    }
    case ICON_HOME: {
      const int roof[3][2] = { { cx - 19, cy }, { cx, cy - 18 }, { cx + 19, cy } };
      strip(d, roof, 3, 5);
      rrect(d, cx - 13, cy - 2, cx + 13, cy + 17, 2);
      col(d, br, bg, bb);
      rrect(d, cx - 4, cy + 6, cx + 4, cy + 17, 1);
      break;
    }
    case ICON_MESSAGE: {
      rrect(d, cx - 19, cy - 13, cx + 19, cy + 13, 3);
      col(d, br, bg, bb);
      const int flap[3][2] = { { cx - 17, cy - 11 }, { cx, cy + 2 }, { cx + 17, cy - 11 } };
      strip(d, flap, 3, 3);
      break;
    }
    case ICON_SETTINGS:
      for (int i = 0; i < 4; ++i) {                        /* gear teeth, then body, then hub */
        float a = i * PI_F / 4.0f;
        line(d, cx - (int)(17 * cosf(a)), cy - (int)(17 * sinf(a)), cx + (int)(17 * cosf(a)), cy + (int)(17 * sinf(a)), 7);
      }
      dot(d, cx, cy, 13);
      col(d, br, bg, bb);
      dot(d, cx, cy, 6);
      break;
    case ICON_BACK: {
      const int arrow[3][2] = { { cx - 4, cy - 14 }, { cx - 18, cy }, { cx - 4, cy + 14 } };
      strip(d, arrow, 3, 5);
      line(d, cx - 16, cy, cx + 18, cy, 5);
      break;
    }
    case ICON_NONE: break;
  }
}

static void softkeys(bt815_t *d, const ui_t *u)
{
  ui_softkey_t sk[4];
  ui_softkeys(u, sk);
  for (int i = 0; i < 4; ++i) {
    if (!sk[i].label) { continue; }
    const int cy = ui_softkey_center_y(i);
    const int y0 = cy - UI_SOFT_H / 2, y1 = cy + UI_SOFT_H / 2;
    uint8_t r = 36, g = 52, b = 100;                         /* idle */
    if (sk[i].tone == 1) { r = 28; g = 140; b = 74; }
    if (sk[i].tone == 2) { r = 190; g = 40; b = 50; }
    if (u->pressed_soft == i) { r = (uint8_t)(r + 70); g = (uint8_t)(g + 70); b = (uint8_t)(b + 70); }
    col(d, r, g, b);
    rrect(d, UI_SOFT_X0, y0, UI_SOFT_X1, y1, 10);
    draw_icon(d, sk[i].icon, (UI_SOFT_X0 + UI_SOFT_X1) / 2, cy - 16, r, g, b);
    col(d, 255, 255, 255);
    text(d, (UI_SOFT_X0 + UI_SOFT_X1) / 2, cy + 32, FONT_MED, true, sk[i].label);
  }
}

static void status_bar(bt815_t *d, const ui_status_t *st)
{
  char buf[16];
  col(d, 20, 30, 62);
  rect(d, 0, 0, UI_SOFT_X0 - 8, 40);
  col(d, 255, 255, 255);
  snprintf(buf, sizeof buf, "%02u:%02u", st->hour, st->minute);
  text(d, 14, 20, FONT_MED, false, buf);
  if (st->carrier) { text(d, 300, 20, FONT_SMALL, true, st->carrier); }
  /* signal bars */
  for (int i = 0; i < 5; ++i) {
    if (i < st->signal_bars) { col(d, 255, 255, 255); } else { col(d, 80, 90, 120); }
    rect(d, 470 + i * 9, 30 - (i + 1) * 4, 475 + i * 9, 30);
  }
  /* battery */
  int fill = st->battery_pct * 30 / 100;
  col(d, 255, 255, 255);
  rrect(d, 540, 10, 576, 30, 2);
  rect(d, 576, 16, 579, 24);
  col(d, 20, 30, 62);
  rect(d, 542, 12, 574, 28);
  if (st->battery_pct > 20) { col(d, 60, 200, 100); } else { col(d, 220, 60, 60); }
  rect(d, 543, 13, 543 + fill, 27);
  col(d, 255, 255, 255);
  snprintf(buf, sizeof buf, "%u%%%s", st->battery_pct, st->charging ? "+" : "");
  text(d, 600, 20, FONT_SMALL, false, buf);
}

static void screen_home(bt815_t *d, const ui_status_t *st)
{
  char buf[16];
  col(d, 255, 255, 255);
  snprintf(buf, sizeof buf, "%02u:%02u", st->hour, st->minute);
  text(d, 330, 200, FONT_HUGE, true, buf);
  col(d, 150, 165, 205);
  text(d, 330, 290, FONT_LARGE, true, "Feature Phone");
  text(d, 330, 400, FONT_SMALL, true, "Press a number to dial");
}

static void screen_list(bt815_t *d, const ui_t *u)
{
  col(d, 255, 255, 255);
  text(d, 20, 72, FONT_LARGE, false, "Contacts");
  const unsigned rows = 6;
  unsigned first = 0;
  if (u->contact_sel >= rows) { first = u->contact_sel - rows + 1u; }
  for (unsigned i = 0; i < rows && first + i < u->contact_count; ++i) {
    const unsigned idx = first + i;
    const int y = 110 + (int)i * 58;
    if (idx == u->contact_sel) {
      col(d, 60, 90, 170);
      rrect(d, 12, y, 650, y + 52, 8);
    }
    col(d, 255, 255, 255);
    text(d, 30, y + 26, FONT_LARGE, false, u->contacts[idx].name);
    col(d, 190, 200, 230);
    text(d, 440, y + 26, FONT_SMALL, false, u->contacts[idx].number);
  }
}

static void screen_dialer(bt815_t *d, const ui_t *u)
{
  col(d, 255, 255, 255);
  text(d, 20, 72, FONT_LARGE, false, "Dial");
  const char *shown = u->number_len ? u->number : "";
  text(d, 330, 220, u->number_len > 14 ? FONT_LARGE : FONT_XL, true, shown);
  col(d, 150, 165, 205);
  if (!u->number_len) { text(d, 330, 220, FONT_MED, true, "Enter a number"); }
  text(d, 330, 400, FONT_SMALL, true, "OK or Call to dial");
}

static void screen_incall(bt815_t *d, const ui_t *u)
{
  col(d, 120, 220, 150);
  text(d, 330, 130, FONT_LARGE, true, "Calling...");
  col(d, 255, 255, 255);
  text(d, 330, 220, FONT_XL, true, u->call_name ? u->call_name : u->number);
  if (u->call_name) {
    col(d, 190, 200, 230);
    text(d, 330, 290, FONT_LARGE, true, u->number);
  }
}

static void screen_placeholder(bt815_t *d, const ui_t *u)
{
  col(d, 255, 255, 255);
  text(d, 20, 72, FONT_LARGE, false, ui_screen_title(u->screen));
  col(d, 150, 165, 205);
  text(d, 330, 240, FONT_LARGE, true, "Coming soon");
}

bt815_status_t ui_render(bt815_t *d, const ui_t *u, const ui_status_t *st)
{
  bt815_frame_begin(d);
  bt815_cmd(d, BT815_DL_CLEAR_COLOR_RGB(12, 18, 38));
  bt815_cmd(d, BT815_DL_CLEAR(1, 1, 1));
  bt815_cmd(d, BT815_DL_VERTEX_FORMAT(0));               /* whole-pixel vertices */
  status_bar(d, st);
  switch (u->screen) {
    case UI_HOME: screen_home(d, st); break;
    case UI_CONTACTS: screen_list(d, u); break;
    case UI_DIALER: screen_dialer(d, u); break;
    case UI_INCALL: screen_incall(d, u); break;
    default: screen_placeholder(d, u); break;
  }
  softkeys(d, u);
  return bt815_frame_end(d);
}
