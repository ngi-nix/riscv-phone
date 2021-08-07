#ifndef __CL_IMX8_COMMON_H__
#define __CL_IMX8_COMMON_H__

#include <asm/mach-imx/iomux-v3.h>

int board_early_init_f(void);

#define I2C_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_HYS | PAD_CTL_PUE)
#define PC		MUX_PAD_CTRL(I2C_PAD_CTRL)

#endif /* __CL_IMX8_COMMON_H__ */
