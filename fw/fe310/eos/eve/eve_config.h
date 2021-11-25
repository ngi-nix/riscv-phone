#ifndef EVE_CONFIG_H_
#define EVE_CONFIG_H_

/* my display */

#define EVE_TH      1200L
#define EVE_THD     800L
#define EVE_THF     210L
#define EVE_THP     20L
#define EVE_THB     46L

#define EVE_TV      650L
#define EVE_TVD     480L
#define EVE_TVF     22L
#define EVE_TVP     12L
#define EVE_TVB     23L


#define EVE_HSIZE           (EVE_THD)                       /* Thd Length of visible part of line (in PCLKs) - display width */
#define EVE_HSYNC0          (EVE_THF)                       /* Thf Horizontal Front Porch */
#define EVE_HSYNC1          (EVE_THF + EVE_THP)             /* Thf + Thp Horizontal Front Porch plus Hsync Pulse width */
#define EVE_HOFFSET         (EVE_THF + EVE_THP + EVE_THB)   /* Thf + Thp + Thb Length of non-visible part of line (in PCLK cycles) */
#define EVE_HCYCLE          (EVE_TH)                        /* Th Total length of line (visible and non-visible) (in PCLKs) */

#define EVE_VSIZE           (EVE_TVD)                       /* Tvd Number of visible lines (in lines) - display height */
#define EVE_VSYNC0          (EVE_TVF)                       /* Tvf Vertical Front Porch */
#define EVE_VSYNC1          (EVE_TVF + EVE_TVP)             /* Tvf + Tvp Vertical Front Porch plus Vsync Pulse width */
#define EVE_VOFFSET         (EVE_TVF + EVE_TVP + EVE_TVB)   /* Tvf + Tvp + Tvb Number of non-visible lines (in lines) */
#define EVE_VCYCLE          (EVE_TV)                        /* Tv Total number of lines (visible and non-visible) (in lines) */

#define EVE_PCLKPOL         (1L)                            /* PCLK polarity (0 = rising edge, 1 = falling edge) */
#define EVE_SWIZZLE         (0L)                            /* Defines the arrangement of the RGB pins of the FT800 */
#define EVE_PCLK            (1L)                            /* 60MHz / REG_PCLK = PCLK frequency - 30 MHz */
#define EVE_CSPREAD         (0L)                            /* helps with noise, when set to 1 fewer signals are changed simultaneously, reset-default: 1 */

#define EVE_GEN             4

#endif /* EVE_CONFIG_H */
