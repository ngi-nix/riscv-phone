#ifndef EVE_CONFIG_H_
#define EVE_CONFIG_H_

/* FocusLCDs E50RG84885LWAM520-CA */

#define EVE_HLPW    20      /* horizontal low pulse width */
#define EVE_HBP     60      /* horizontal back porch */
#define EVE_HFP     40      /* horizontal front porch */
#define EVE_HACT    480     /* horizontal active pixels */
#define EVE_HTOT    (EVE_HLPW + EVE_HBP + EVE_HFP + EVE_HACT + 10)


#define EVE_VLPW    26      /* vertical low pulse width */
#define EVE_VBP     50      /* vertical back porch */
#define EVE_VFP     30      /* vertical front porch */
#define EVE_VACT    854     /* vertical active pixels */
#define EVE_VTOT    (EVE_VLPW + EVE_VBP + EVE_VFP + EVE_VACT + 10)

#define EVE_HCYCLE          (EVE_HTOT)                      /* Th Total length of line (visible and non-visible) (in PCLKs) */
#define EVE_HSIZE           (EVE_HACT)                      /* Length of visible part of line (in PCLKs) - display width */
#define EVE_HOFFSET         (EVE_HFP + EVE_HLPW + EVE_HBP)  /* Length of non-visible part of line (in PCLK cycles) */
#define EVE_HSYNC0          (EVE_HFP)                       /* Horizontal Front Porch */
#define EVE_HSYNC1          (EVE_HFP + EVE_HLPW)            /* Horizontal Front Porch plus Hsync Pulse width */

#define EVE_VCYCLE          (EVE_VTOT)                      /* Total number of lines (visible and non-visible) (in lines) */
#define EVE_VSIZE           (EVE_VACT)                      /* Number of visible lines (in lines) - display height */
#define EVE_VOFFSET         (EVE_VFP + EVE_VLPW + EVE_VBP)  /* Number of non-visible lines (in lines) */
#define EVE_VSYNC0          (EVE_VFP)                       /* Vertical Front Porch */
#define EVE_VSYNC1          (EVE_VFP + EVE_VLPW)            /* Vertical Front Porch plus Vsync Pulse width */

#define EVE_PCLKPOL         1                               /* PCLK polarity (0 = rising edge, 1 = falling edge) */
#define EVE_SWIZZLE         0                               /* Defines the arrangement of the RGB pins */
#define EVE_CSPREAD         0                               /* helps with noise, when set to 1 fewer signals are changed simultaneously, reset-default: 1 */

#define EVE_PCLK            2                               /* 36 MHz */

#define EVE_GEN             4

#endif /* EVE_CONFIG_H */
