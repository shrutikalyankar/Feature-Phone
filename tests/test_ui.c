/* Host tests: key matrix mapping, TCA8418 driver (fake I2C) and UI navigation. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "keys.h"
#include "keypad_tca8418.h"
#include "ui.h"

/* ---- fake TCA8418 ---- */
typedef struct { uint8_t reg[0x40]; uint8_t fifo[10]; unsigned n, head; unsigned writes; uint8_t last_cfg_writes[16]; } fake_i2c_t;
static bool f_write(void *c, uint8_t dev, uint8_t reg, uint8_t v)
{
  fake_i2c_t *f = c; assert(dev == 0x34);
  f->reg[reg] = v; f->writes++;
  return true;
}
static bool f_read(void *c, uint8_t dev, uint8_t reg, uint8_t *v)
{
  fake_i2c_t *f = c; assert(dev == 0x34);
  if (reg == 0x03) { *v = (uint8_t)(f->n - f->head); return true; }
  if (reg == 0x04) { *v = f->head < f->n ? f->fifo[f->head++] : 0; return true; }
  *v = f->reg[reg]; return true;
}
static void press(fake_i2c_t *f, unsigned row, unsigned col, bool down)
{
  f->fifo[f->n++] = (uint8_t)((down ? 0x80 : 0) | (row * 10 + col + 1));
}

static void test_matrix_map(void)
{
  /* The four switches right of the LCD: CONTROL_C1..C4 on ROW4, COL1..COL4 (Control_Input_Buttons sheet). */
  assert(keys_from_matrix(4, 1) == KEY_SOFT1 && keys_from_matrix(4, 2) == KEY_SOFT2);
  assert(keys_from_matrix(4, 3) == KEY_SOFT3 && keys_from_matrix(4, 4) == KEY_SOFT4);
  assert(keys_from_matrix(4, 0) == KEY_OK);
  assert(keys_from_matrix(0, 4) == KEY_UP && keys_from_matrix(1, 4) == KEY_DOWN);
  assert(keys_from_matrix(2, 4) == KEY_LEFT && keys_from_matrix(3, 4) == KEY_RIGHT);
  assert(keys_from_matrix(0, 0) == KEY_1 && keys_from_matrix(3, 1) == KEY_0 && keys_from_matrix(3, 0) == KEY_STAR);
  assert(keys_from_matrix(3, 2) == KEY_HASH && keys_from_matrix(2, 2) == KEY_9);
  assert(keys_from_matrix(5, 0) == KEY_SPEAKER && keys_from_matrix(5, 1) == KEY_VOL_UP && keys_from_matrix(5, 2) == KEY_VOL_DOWN);
  assert(keys_from_matrix(6, 0) == KEY_NONE && keys_from_matrix(0, 5) == KEY_NONE);
  assert(key_to_dial_char(KEY_7) == '7' && key_to_dial_char(KEY_STAR) == '*' && key_to_dial_char(KEY_A) == 0);
}

static void test_keypad_driver(void)
{
  fake_i2c_t f; memset(&f, 0, sizeof f);
  keypad_io_t io = { &f, f_write, f_read };
  assert(keypad_init(&io));
  assert(f.reg[0x1D] == 0x3F && f.reg[0x1E] == 0x1F && f.reg[0x1F] == 0x00);   /* rows 0-5, cols 0-4 only */
  assert(f.reg[0x01] & 0x01);                                                  /* key event interrupt on */
  press(&f, 4, 1, true); press(&f, 4, 1, false); press(&f, 0, 4, true);
  press(&f, 6, 0, true);                                                       /* row 6 is not a keypad row: ignored? */
  key_event_t ev[8];
  size_t n = keypad_poll(&io, ev, 8);
  assert(n == 4 || n == 3);
  assert(ev[0].key == KEY_SOFT1 && ev[0].pressed);
  assert(ev[1].key == KEY_SOFT1 && !ev[1].pressed);
  assert(ev[2].key == KEY_UP && ev[2].pressed);
  assert(f.reg[0x02] == 0x01);                                                 /* interrupt flag cleared */
  assert(keypad_poll(&io, ev, 8) == 0);                                        /* FIFO empty */
  /* GPI events (97..114) are skipped, not mapped to keys. */
  f.n = f.head = 0; f.fifo[f.n++] = 0x80 | 97;
  assert(keypad_poll(&io, ev, 8) == 0);
}

/* ---- UI ---- */
static char dialed[32]; static int hung;
static void on_dial(void *c, const char *n) { (void)c; strncpy(dialed, n, sizeof dialed - 1); }
static void on_hang(void *c) { (void)c; ++hung; }
static const ui_contact_t CONT[] = { { "Alice", "111" }, { "Bob", "222" }, { "Carol", "333" } };

static ui_t mk(void)
{
  ui_t u; ui_ops_t ops = { NULL, on_dial, on_hang };
  ui_init(&u, CONT, 3, &ops); dialed[0] = 0; hung = 0; return u;
}
static void tap(ui_t *u, fp_key_t k, uint32_t t) { ui_handle_key(u, k, true, t); ui_handle_key(u, k, false, t + 50); }

static void test_home_softkeys(void)
{
  ui_t u = mk(); ui_softkey_t sk[4];
  ui_softkeys(&u, sk);
  assert(!strcmp(sk[0].label, "Call") && sk[0].icon == ICON_CALL && sk[0].tone == 1);
  assert(!strcmp(sk[1].label, "Contacts") && !strcmp(sk[2].label, "Messages") && !strcmp(sk[3].label, "Settings"));
  tap(&u, KEY_SOFT2, 0); assert(u.screen == UI_CONTACTS);
  ui_softkeys(&u, sk);
  assert(!strcmp(sk[3].label, "Home") && sk[3].icon == ICON_HOME && sk[2].label == NULL);
  tap(&u, KEY_SOFT4, 10); assert(u.screen == UI_HOME);
  tap(&u, KEY_SOFT3, 20); assert(u.screen == UI_MESSAGES);
  tap(&u, KEY_SOFT4, 30); assert(u.screen == UI_HOME);
  tap(&u, KEY_SOFT4, 40); assert(u.screen == UI_SETTINGS);
  tap(&u, KEY_SOFT4, 50); assert(u.screen == UI_HOME);
}

static void test_unused_softkey_ignored(void)
{
  ui_t u = mk();
  tap(&u, KEY_SOFT2, 3);                                              /* Contacts */
  ui_tick(&u, 1000);                                                  /* let the Contacts-press flash end */
  (void)ui_take_dirty(&u);
  tap(&u, KEY_SOFT3, 1100);                                              /* unused here: no change, no redraw */
  assert(u.screen == UI_CONTACTS && !ui_take_dirty(&u) && u.pressed_soft == -1);
}

static void test_contacts_call(void)
{
  ui_t u = mk();
  tap(&u, KEY_SOFT2, 0);
  tap(&u, KEY_DOWN, 1); tap(&u, KEY_DOWN, 2); tap(&u, KEY_DOWN, 3);   /* clamps at last entry */
  assert(u.contact_sel == 2);
  tap(&u, KEY_UP, 4); assert(u.contact_sel == 1);
  tap(&u, KEY_SOFT1, 5);
  assert(u.screen == UI_INCALL && !strcmp(dialed, "222") && !strcmp(u.call_name, "Bob"));
  ui_softkey_t sk[4]; ui_softkeys(&u, sk);
  assert(!strcmp(sk[0].label, "End") && sk[0].tone == 2 && sk[1].label == NULL);
  tap(&u, KEY_SOFT1, 6);
  assert(hung == 1 && u.screen == UI_HOME);
  /* OK also calls the highlighted contact. */
  tap(&u, KEY_SOFT2, 7); tap(&u, KEY_OK, 8);
  assert(u.screen == UI_INCALL && !strcmp(dialed, "111"));
}

static void test_dialer(void)
{
  ui_t u = mk();
  tap(&u, KEY_5, 0);                                  /* digit on Home opens the dialer and keeps the digit */
  assert(u.screen == UI_DIALER && !strcmp(u.number, "5"));
  tap(&u, KEY_5, 1); tap(&u, KEY_5, 2); tap(&u, KEY_STAR, 3); tap(&u, KEY_HASH, 4);
  assert(!strcmp(u.number, "555*#"));
  tap(&u, KEY_SOFT2, 5); assert(!strcmp(u.number, "555*"));          /* Delete */
  tap(&u, KEY_A, 6); assert(!strcmp(u.number, "555*"));              /* letters ignored */
  tap(&u, KEY_SOFT1, 7);
  assert(u.screen == UI_INCALL && !strcmp(dialed, "555*") && u.call_name == NULL);
  tap(&u, KEY_SOFT1, 8); assert(hung == 1);
  /* Empty number: Call does nothing. */
  u = mk(); tap(&u, KEY_SOFT1, 0); assert(u.screen == UI_DIALER);
  tap(&u, KEY_SOFT1, 1); assert(u.screen == UI_DIALER && dialed[0] == 0);
  /* Number length is bounded. */
  for (int i = 0; i < 40; ++i) { tap(&u, KEY_1, 10 + i); }
  assert(u.number_len == UI_NUMBER_MAX && strlen(u.number) == UI_NUMBER_MAX);
}

static void test_press_flash(void)
{
  ui_t u = mk(); (void)ui_take_dirty(&u);
  ui_handle_key(&u, KEY_SOFT2, true, 1000);
  assert(u.pressed_soft == 1 && ui_take_dirty(&u));
  ui_tick(&u, 1000 + UI_PRESS_FLASH_MS - 1);
  assert(u.pressed_soft == 1);
  ui_tick(&u, 1000 + UI_PRESS_FLASH_MS);
  assert(u.pressed_soft == -1 && ui_take_dirty(&u));
  /* Millisecond counter wrap-around. */
  ui_handle_key(&u, KEY_SOFT4, true, 0xFFFFFFF0u);
  ui_tick(&u, 0xFFFFFFF0u + UI_PRESS_FLASH_MS);
  assert(u.pressed_soft == -1);
}

int main(void)
{
  test_matrix_map(); test_keypad_driver(); test_home_softkeys(); test_unused_softkey_ignored();
  test_contacts_call(); test_dialer(); test_press_flash();
  puts("ui tests passed");
  return 0;
}
