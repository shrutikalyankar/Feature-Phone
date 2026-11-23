/* SPDX-License-Identifier: MIT */
/* EFR32BG24 I2C0 master for the TCA8418. Polling transfers. NOT yet compiled against
 * the Simplicity SDK. */
#include "keypad_hal_efr32.h"
#include "lcd_config.h"
#include "em_gpio.h"
#include "em_i2c.h"
#include "sl_clock_manager.h"

static bool xfer(uint8_t dev7, uint16_t flags, uint8_t *b0, uint16_t n0, uint8_t *b1, uint16_t n1)
{
  I2C_TransferSeq_TypeDef seq;
  seq.addr = (uint16_t)(dev7 << 1);
  seq.flags = flags;
  seq.buf[0].data = b0; seq.buf[0].len = n0;
  seq.buf[1].data = b1; seq.buf[1].len = n1;
  I2C_TransferReturn_TypeDef r = I2C_TransferInit(I2C0, &seq);
  uint32_t guard = 2000000u;                       /* bounded wait: never hang the UI */
  while (r == i2cTransferInProgress && guard--) {
    r = I2C_Transfer(I2C0);
  }
  return r == i2cTransferDone;
}

static bool wr(void *ctx, uint8_t dev7, uint8_t reg, uint8_t val)
{
  (void)ctx;
  uint8_t b[2] = { reg, val };
  return xfer(dev7, I2C_FLAG_WRITE, b, 2, NULL, 0);
}

static bool rd(void *ctx, uint8_t dev7, uint8_t reg, uint8_t *val)
{
  (void)ctx;
  return xfer(dev7, I2C_FLAG_WRITE_READ, &reg, 1, val, 1);
}

bool keypad_hal_efr32_init(keypad_io_t *io)
{
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_GPIO);
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_I2C0);
  GPIO_PinModeSet(KEYPAD_SDA_PORT, KEYPAD_SDA_PIN, gpioModeWiredAnd, 1);  /* open drain, external pull-up */
  GPIO_PinModeSet(KEYPAD_SCL_PORT, KEYPAD_SCL_PIN, gpioModeWiredAnd, 1);
  GPIO_PinModeSet(KEYPAD_INT_PORT, KEYPAD_INT_PIN, gpioModeInput, 0);
  GPIO->I2CROUTE[0].SDAROUTE = ((uint32_t)KEYPAD_SDA_PORT << _GPIO_I2C_SDAROUTE_PORT_SHIFT)
                             | ((uint32_t)KEYPAD_SDA_PIN << _GPIO_I2C_SDAROUTE_PIN_SHIFT);
  GPIO->I2CROUTE[0].SCLROUTE = ((uint32_t)KEYPAD_SCL_PORT << _GPIO_I2C_SCLROUTE_PORT_SHIFT)
                             | ((uint32_t)KEYPAD_SCL_PIN << _GPIO_I2C_SCLROUTE_PIN_SHIFT);
  GPIO->I2CROUTE[0].ROUTEEN = GPIO_I2C_ROUTEEN_SDAPEN | GPIO_I2C_ROUTEEN_SCLPEN;
  I2C_Init_TypeDef init = I2C_INIT_DEFAULT;        /* 100 kHz standard mode */
  I2C_Init(I2C0, &init);
  io->ctx = NULL; io->write_reg = wr; io->read_reg = rd;
  return true;
}

bool keypad_hal_efr32_int_asserted(void)
{
  return GPIO_PinInGet(KEYPAD_INT_PORT, KEYPAD_INT_PIN) == 0;
}
