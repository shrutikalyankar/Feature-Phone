/* SPDX-License-Identifier: MIT */
/* EFR32BG24 board support for the BT815: EUSART1 in SPI main mode (mode 0, MSB
 * first), chip select on a plain GPIO. Blocking byte transfers; move to LDMA
 * before streaming bitmaps. NOT yet compiled against the Simplicity SDK. */
#include "bt815_hal_efr32.h"
#include "lcd_config.h"
#include "em_gpio.h"
#include "em_eusart.h"
#include "sl_clock_manager.h"
#include "sl_sleeptimer.h"

#define EUSART_INST  EUSART1   /* keep in step with LCD_EUSART_INDEX */

static void hal_cs(void *ctx, bool asserted)
{
  (void)ctx;
  if (asserted) {
    GPIO_PinOutClear(LCD_CS_PORT, LCD_CS_PIN);
  } else {
    GPIO_PinOutSet(LCD_CS_PORT, LCD_CS_PIN);
  }
}

static void hal_xfer(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len)
{
  (void)ctx;
  for (size_t i = 0; i < len; ++i) {
    uint8_t r = EUSART_Spi_TxRx(EUSART_INST, tx ? tx[i] : 0x00u);
    if (rx) {
      rx[i] = r;
    }
  }
}

static void hal_delay(void *ctx, uint32_t ms)
{
  (void)ctx;
  sl_sleeptimer_delay_millisecond((uint16_t)ms);
}

static uint32_t hal_millis(void *ctx)
{
  (void)ctx;
  return sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());
}

static void hal_pd_n(void *ctx, bool level)
{
  (void)ctx;
  if (level) {
    GPIO_PinOutSet(LCD_PD_PORT, LCD_PD_PIN);
  } else {
    GPIO_PinOutClear(LCD_PD_PORT, LCD_PD_PIN);
  }
}

bool bt815_hal_efr32_int_asserted(void)
{
  return GPIO_PinInGet(LCD_INT_PORT, LCD_INT_PIN) == 0;   /* INT_N is active low */
}

static uint32_t ref_hz;

static void hal_set_hz(void *ctx, uint32_t hz)
{
  (void)ctx;
  EUSART_Enable(EUSART_INST, eusartDisable);
  EUSART_BaudrateSet(EUSART_INST, ref_hz, hz);
  EUSART_Enable(EUSART_INST, eusartEnable);
}

bool bt815_hal_efr32_init(bt815_hal_t *hal)
{
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_GPIO);
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_EUSART1);
  if (sl_clock_manager_get_clock_branch_frequency(SL_CLOCK_BRANCH_EM01GRPCCLK, &ref_hz) != SL_STATUS_OK) {
    return false;
  }

  GPIO_PinModeSet(LCD_CS_PORT, LCD_CS_PIN, gpioModePushPull, 1);          /* idle high */
#if LCD_PARK_SD_CS
  /* The SD card shares SCK/MOSI/MISO. Keep it deselected so it never drives MISO. */
  GPIO_PinModeSet(LCD_SD_CS_PORT, LCD_SD_CS_PIN, gpioModePushPull, 1);
#endif
#if LCD_USE_PD_PIN
  GPIO_PinModeSet(LCD_PD_PORT, LCD_PD_PIN, gpioModePushPull, 1);          /* released */
#endif
  GPIO_PinModeSet(LCD_INT_PORT, LCD_INT_PIN, gpioModeInput, 0);           /* external pull-up */
  GPIO_PinModeSet(LCD_SPI_SCK_PORT, LCD_SPI_SCK_PIN, gpioModePushPull, 0); /* CPOL = 0 */
  GPIO_PinModeSet(LCD_SPI_MOSI_PORT, LCD_SPI_MOSI_PIN, gpioModePushPull, 0);
  GPIO_PinModeSet(LCD_SPI_MISO_PORT, LCD_SPI_MISO_PIN, gpioModeInput, 0);

  EUSART_SpiInit_TypeDef init = EUSART_SPI_MASTER_INIT_DEFAULT_HF;
  init.bitRate = LCD_SPI_BOOT_HZ;
  init.clockMode = eusartClockMode0;
  EUSART_SpiInit(EUSART_INST, &init);

  GPIO->EUSARTROUTE[LCD_EUSART_INDEX].TXROUTE =
      ((uint32_t)LCD_SPI_MOSI_PORT << _GPIO_EUSART_TXROUTE_PORT_SHIFT)
      | ((uint32_t)LCD_SPI_MOSI_PIN << _GPIO_EUSART_TXROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[LCD_EUSART_INDEX].RXROUTE =
      ((uint32_t)LCD_SPI_MISO_PORT << _GPIO_EUSART_RXROUTE_PORT_SHIFT)
      | ((uint32_t)LCD_SPI_MISO_PIN << _GPIO_EUSART_RXROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[LCD_EUSART_INDEX].SCLKROUTE =
      ((uint32_t)LCD_SPI_SCK_PORT << _GPIO_EUSART_SCLKROUTE_PORT_SHIFT)
      | ((uint32_t)LCD_SPI_SCK_PIN << _GPIO_EUSART_SCLKROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[LCD_EUSART_INDEX].ROUTEEN =
      GPIO_EUSART_ROUTEEN_RXPEN | GPIO_EUSART_ROUTEEN_TXPEN | GPIO_EUSART_ROUTEEN_SCLKPEN;

  hal->ctx = NULL;
  hal->cs = hal_cs;
  hal->xfer = hal_xfer;
  hal->delay_ms = hal_delay;
  hal->millis = hal_millis;
  hal->set_spi_hz = hal_set_hz;
  hal->pd_n = LCD_USE_PD_PIN ? hal_pd_n : NULL;
  return true;
}
