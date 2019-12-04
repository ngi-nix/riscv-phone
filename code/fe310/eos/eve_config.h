/*
@file    EVE_config.h
@brief   configuration information for some TFTs
@version 4.0
@date    2019-09-14
@author  Rudolph Riedel

@section LICENSE

MIT License

Copyright (c) 2016-2019 Rudolph Riedel

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

@section History

4.0
- renamed from EVE_config.h to EVE_config.h
- renamed EVE_81X_ENABLE to FT81X_ENABLE
- added a fictitious BT81x entry under the made-up name EVE_EVE3_70G, just to see the project compile with additional BT81x includes and functions
- added profiles for the BT81x 4.3", 5" and 7" modules from Riverdi - the only tested is the 4.3" with a RVT43ULBNWC00
- moved all target specific lines from EVE_config.h to EVE_target.h
- cleaned up history
- added profiles for EVE3-35G, EVE3-43G, EVE3-50G
- added a profile for the CFAF800480E0-050SC from Crystalfontz
- changed EVE_RiTFT50 to use the RVT70 config instead of the RVT50 config since RVT50 uses a different HOFFSET value
- added EVE_PAF90, a profile for the PAF90B5WFNWC01 from Panasys

*/

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


#define EVE_HSIZE	(EVE_THD)                       /* Thd Length of visible part of line (in PCLKs) - display width */
#define EVE_HSYNC0	(EVE_THF)                       /* Thf Horizontal Front Porch */
#define EVE_HSYNC1	(EVE_THF + EVE_THP)             /* Thf + Thp Horizontal Front Porch plus Hsync Pulse width */
#define EVE_HOFFSET	(EVE_THF + EVE_THP + EVE_THB)   /* Thf + Thp + Thb Length of non-visible part of line (in PCLK cycles) */
#define EVE_HCYCLE 	(EVE_TH)                        /* Th Total length of line (visible and non-visible) (in PCLKs) */

#define EVE_VSIZE	(EVE_TVD)                       /* Tvd Number of visible lines (in lines) - display height */
#define EVE_VSYNC0	(EVE_TVF)                       /* Tvf Vertical Front Porch */
#define EVE_VSYNC1	(EVE_TVF + EVE_TVP)             /* Tvf + Tvp Vertical Front Porch plus Vsync Pulse width */
#define EVE_VOFFSET	(EVE_TVF + EVE_TVP + EVE_TVB)	/* Tvf + Tvp + Tvb Number of non-visible lines (in lines) */
#define EVE_VCYCLE	(EVE_TV)                        /* Tv Total number of lines (visible and non-visible) (in lines) */

#define EVE_PCLKPOL	(1L)                            /* PCLK polarity (0 = rising edge, 1 = falling edge) */
#define EVE_SWIZZLE	(0L)                            /* Defines the arrangement of the RGB pins of the FT800 */
#define EVE_PCLK	(2L)                            /* 60MHz / REG_PCLK = PCLK frequency - 30 MHz */
#define EVE_CSPREAD	(1L)                            /* helps with noise, when set to 1 fewer signals are changed simultaneously, reset-default: 1 */
#define EVE_TOUCH_RZTHRESH (1200L)                  /* touch-sensitivity */
#define EVE_HAS_CRYSTAL
#define FT81X_ENABLE

#endif /* EVE_CONFIG_H */
