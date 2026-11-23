/* SPDX-License-Identifier: MIT */
/* BT815 constants taken from BRT_000220 (BT81X datasheet v1.0): memory map, host
 * commands, registers. Display-list/coprocessor opcodes follow the BT81x
 * programming guide numbering. */
#ifndef BT815_REGS_H
#define BT815_REGS_H

/* Memory map (datasheet 5.1) */
#define BT815_RAM_G        0x000000ul   /* 1 MB general graphics RAM */
#define BT815_ROM_CHIPID   0x0C0000ul   /* 08 15 01 00 on BT815 */
#define BT815_RAM_DL       0x300000ul   /* 8 KB display list RAM */
#define BT815_RAM_REG      0x302000ul   /* 4 KB registers */
#define BT815_RAM_CMD      0x308000ul   /* 4 KB coprocessor FIFO */

/* SPI host commands: 3 bytes, first byte below, then 0x00 0x00 */
#define BT815_HC_ACTIVE    0x00u
#define BT815_HC_STANDBY   0x41u
#define BT815_HC_SLEEP     0x42u
#define BT815_HC_PWRDOWN   0x43u
#define BT815_HC_CLKEXT    0x44u
#define BT815_HC_CLKINT    0x48u
#define BT815_HC_CLKSEL    0x61u
#define BT815_HC_RST_PULSE 0x68u

/* Registers */
#define BT815_REG_ID            (BT815_RAM_REG + 0x000u) /* reads 0x7C when ready */
#define BT815_REG_FRAMES        (BT815_RAM_REG + 0x004u)
#define BT815_REG_FREQUENCY     (BT815_RAM_REG + 0x00Cu)
#define BT815_REG_CPURESET      (BT815_RAM_REG + 0x020u)
#define BT815_REG_HCYCLE        (BT815_RAM_REG + 0x02Cu)
#define BT815_REG_HOFFSET       (BT815_RAM_REG + 0x030u)
#define BT815_REG_HSIZE         (BT815_RAM_REG + 0x034u)
#define BT815_REG_HSYNC0        (BT815_RAM_REG + 0x038u)
#define BT815_REG_HSYNC1        (BT815_RAM_REG + 0x03Cu)
#define BT815_REG_VCYCLE        (BT815_RAM_REG + 0x040u)
#define BT815_REG_VOFFSET       (BT815_RAM_REG + 0x044u)
#define BT815_REG_VSIZE         (BT815_RAM_REG + 0x048u)
#define BT815_REG_VSYNC0        (BT815_RAM_REG + 0x04Cu)
#define BT815_REG_VSYNC1        (BT815_RAM_REG + 0x050u)
#define BT815_REG_DLSWAP        (BT815_RAM_REG + 0x054u)
#define BT815_REG_ROTATE        (BT815_RAM_REG + 0x058u)
#define BT815_REG_OUTBITS       (BT815_RAM_REG + 0x05Cu)
#define BT815_REG_DITHER        (BT815_RAM_REG + 0x060u)
#define BT815_REG_SWIZZLE       (BT815_RAM_REG + 0x064u)
#define BT815_REG_CSPREAD       (BT815_RAM_REG + 0x068u)
#define BT815_REG_PCLK_POL      (BT815_RAM_REG + 0x06Cu)
#define BT815_REG_PCLK          (BT815_RAM_REG + 0x070u)
#define BT815_REG_GPIOX_DIR     (BT815_RAM_REG + 0x098u)
#define BT815_REG_GPIOX         (BT815_RAM_REG + 0x09Cu) /* bit 15 = DISP pin */
#define BT815_REG_INT_FLAGS     (BT815_RAM_REG + 0x0A8u)
#define BT815_REG_PWM_HZ        (BT815_RAM_REG + 0x0D0u) /* backlight PWM */
#define BT815_REG_PWM_DUTY      (BT815_RAM_REG + 0x0D4u) /* 0..128 */
#define BT815_REG_CMD_READ      (BT815_RAM_REG + 0x0F8u)
#define BT815_REG_CMD_WRITE     (BT815_RAM_REG + 0x0FCu)
#define BT815_REG_CMDB_SPACE    (BT815_RAM_REG + 0x574u)
#define BT815_REG_CMDB_WRITE    (BT815_RAM_REG + 0x578u)

#define BT815_ID_VALUE          0x7Cu
#define BT815_GPIOX_DISP        0x8000u
#define BT815_PWM_DUTY_MAX      128u
#define BT815_CMD_FAULT_PTR     0x0FFFu   /* REG_CMD_READ == 0xFFF => coprocessor fault */
#define BT815_DLSWAP_FRAME      2u

/* Display list opcodes */
#define BT815_DL_DISPLAY()              (0x00000000ul)
#define BT815_DL_CLEAR_COLOR_RGB(r,g,b) ((2ul<<24)|((unsigned long)(r)<<16)|((unsigned long)(g)<<8)|(unsigned long)(b))
#define BT815_DL_COLOR_RGB(r,g,b)       ((4ul<<24)|((unsigned long)(r)<<16)|((unsigned long)(g)<<8)|(unsigned long)(b))
#define BT815_DL_BEGIN(prim)            ((31ul<<24)|(unsigned long)(prim))
#define BT815_DL_END()                  (33ul<<24)
#define BT815_DL_CLEAR(c,s,t)           ((38ul<<24)|((unsigned long)(c)<<2)|((unsigned long)(s)<<1)|(unsigned long)(t))
#define BT815_DL_VERTEX_FORMAT(frac)    ((39ul<<24)|(unsigned long)(frac))
#define BT815_DL_VERTEX2F(x,y)          ((1ul<<30)|(((unsigned long)(x)&0x7FFFul)<<15)|((unsigned long)(y)&0x7FFFul))
#define BT815_DL_POINT_SIZE(s)          ((13ul<<24)|((unsigned long)(s)&0x1FFFul))   /* radius, 1/16 px */
#define BT815_DL_LINE_WIDTH(w)          ((14ul<<24)|((unsigned long)(w)&0xFFFul))    /* 1/16 px */
#define BT815_PRIM_POINTS               2u
#define BT815_PRIM_LINES                3u
#define BT815_PRIM_LINE_STRIP           4u
#define BT815_PRIM_RECTS                9u

/* Coprocessor commands */
#define BT815_CMD_DLSTART  0xFFFFFF00ul
#define BT815_CMD_SWAP     0xFFFFFF01ul
#define BT815_CMD_TEXT     0xFFFFFF0Cul
#define BT815_OPT_CENTERY  0x0400u
#define BT815_OPT_CENTER   0x0600u  /* OPT_CENTERX | OPT_CENTERY */

#endif
