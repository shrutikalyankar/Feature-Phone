/* SPDX-License-Identifier: MIT */
#ifndef LCD_CONFIG_H
#define LCD_CONFIG_H

#include "bt815.h"

/* ---- Pins: defaults are the Feature Phone PCB (MCU.SchDoc / Display_Driver.SchDoc).
 * On a dev kit, wire a BT815 module to any free GPIO and override these with -D. ---- */
#ifndef LCD_SPI_SCK_PORT
#define LCD_SPI_SCK_PORT   gpioPortD   /* SPI_SCK  = PD02, EUSART1 SCLK */
#define LCD_SPI_SCK_PIN    2
#define LCD_SPI_MOSI_PORT  gpioPortD   /* SPI_MOSI = PD03, EUSART1 TX   */
#define LCD_SPI_MOSI_PIN   3
#define LCD_SPI_MISO_PORT  gpioPortD   /* SPI_MISO = PD04, EUSART1 RX   */
#define LCD_SPI_MISO_PIN   4
#define LCD_CS_PORT        gpioPortC   /* DISP_CS_N = PC07 (pull-up 47k on board) */
#define LCD_CS_PIN         7
#define LCD_PD_PORT        gpioPortC   /* DISP_PD_N = PC09 (47k pull-up + 0.1uF to GND) */
#define LCD_PD_PIN         9
#define LCD_INT_PORT       gpioPortC   /* DISP_INT_N = PC00 (47k pull-up, active low) */
#define LCD_INT_PIN        0
#define LCD_SD_CS_PORT     gpioPortC   /* SD_NAND_CS = PC08: same SPI bus, must idle high */
#define LCD_SD_CS_PIN      8
#endif
#define LCD_EUSART_INDEX   1           /* EUSART1 is EM1-only; fine while the LCD is active */

/* Start slow (BT815 boots on its 12 MHz clock), bt815_init() raises it afterwards. */
#define LCD_SPI_BOOT_HZ    4000000u

/* Set to 0 on a breakout where PD_N is not wired to the MCU (RC reset only). */
#ifndef LCD_USE_PD_PIN
#define LCD_USE_PD_PIN 1
#endif
/* Set to 0 if no SD card/SD_NAND shares the bus on your setup. */
#ifndef LCD_PARK_SD_CS
#define LCD_PARK_SD_CS 1
#endif

/* ---- Panel: DLC-T5.0AHR#2, 5" IPS, 800 x 480, 24-bit RGB, ST7265 driver, 3.3 V.
 * Numbers are the datasheet's "typical" column (p.10), as it recommends.
 *   DCLK 25 MHz typ (23..27)   th 816 (808..848)   tv 512 (496..528)
 *   H: sync pulse 4, back porch 8 (measured from the start of the pulse), front porch 8
 *      -> 800 + 8 + 8 = 816
 *   V: sync pulse 4, back porch 16 (from the start of the pulse), front porch 16
 *      -> 480 + 16 + 16 = 512
 * BT815 mapping: HSYNC0 = 0, HSYNC1 = pulse width, HOFFSET = back porch as defined above.
 * The datasheet requires back porch 8 / 16 exactly when HSYNC/VSYNC are used (sync mode).
 * Sysclk 48 MHz / PCLK divisor 2 = 24 MHz -> 57 Hz frames. (60 MHz / 2 = 30 MHz would
 * exceed the panel's 27 MHz limit; 60 MHz / 3 = 20 MHz is below its 23 MHz minimum.)
 * PCLK_POL = 1: the panel latches data on DCLK's rising edge, so the BT815 must change
 * data on the falling edge. Valid with HSYNC/VSYNC fitted (R68/R73) or tied low (R69/R70). */
static const bt815_panel_t LCD_PANEL = {
  .hsize = 800, .hcycle = 816, .hoffset = 8,  .hsync0 = 0, .hsync1 = 4,
  .vsize = 480, .vcycle = 512, .voffset = 16, .vsync0 = 0, .vsync1 = 4,
  .sys_mult = 4,
  .pclk_div = 2,
  .pclk_pol = 1,
  .swizzle = 0,
  .cspread = 0,    /* keep R, G, B aligned to the same clock edge */
  .dither = 0,     /* 8 bits per channel straight through */
};

/* Panel datasheet: backlight may come on >= 100 ms after valid display signal, and the
 * panel needs ~120 ms after DISP goes high before it scans. Wait a safe margin. */
#define LCD_BACKLIGHT_DELAY_MS 300u

#endif
