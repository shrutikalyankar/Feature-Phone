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

/* ---- Panel timing: PLACEHOLDER. These are the BT815 reset defaults for a 480x272
 * panel (datasheet reg table). Replace every field from the real panel datasheet
 * before expecting a stable picture. ---- */
static const bt815_panel_t LCD_PANEL = {
  .hsize = 480, .hcycle = 548, .hoffset = 43, .hsync0 = 0,  .hsync1 = 41,
  .vsize = 272, .vcycle = 292, .voffset = 12, .vsync0 = 0,  .vsync1 = 10,
  .pclk_div = 6,   /* 60 MHz / 6 = 10 MHz -> ~62 Hz at 548 x 292 */
  .pclk_pol = 1,   /* typical for 4.3" RGB panels; check the datasheet */
  .swizzle = 0,
  .cspread = 0,
  .dither = 1,
};

#endif
