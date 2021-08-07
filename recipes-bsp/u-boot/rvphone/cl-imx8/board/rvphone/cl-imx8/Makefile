#
# Copyright 2017 CompuLab LTD.
#
# SPDX-License-Identifier:      GPL-2.0+
#
ccflags-y += -I../../freescale

obj-y += common.o
obj-y += ../../freescale/common/mmc.o
obj-$(CONFIG_$(SPL_)DM_PMIC_PFUZE100) += ../../freescale/common/pfuze.o
obj-$(CONFIG_POWER_PFUZE100) += ../../freescale/common/pfuze.o

ifdef CONFIG_SPL_BUILD
obj-y += spl.o ddr/lpddr4_timing_1g.o ddr/lpddr4_timing_2g.o
else
obj-y += cl-imx8.o
endif
