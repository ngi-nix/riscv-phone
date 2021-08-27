/*
 * Copyright 2017 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __DDR_H__
#define __DDR_H__

extern struct dram_timing_info dram_timing_2g;
extern struct dram_timing_info dram_timing_1g;
void ddr_init(struct dram_timing_info *dram_timing);

#endif
