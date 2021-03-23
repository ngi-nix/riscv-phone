#include <stdlib.h>
#include <math.h>

#include "eve_platform.h"
#include "eve_phy.h"

/* Constant accelerator */
void eve_phy_acc_init(EVEPhyAcc *param, int a) {
    param->a = a;
}

void eve_phy_acc_start(EVEPhyAcc *param, int x0, int y0, int v0x, int v0y) {
    double v0 = sqrt(v0x * v0x + v0y * v0y);

    param->x0 = x0;
    param->y0 = y0;
    param->v0x = v0x;
    param->v0y = v0y;
    param->k = 2 * v0 / param->a * EVE_RTC_FREQ;
}

int eve_phy_acc_tick(EVEPhyAcc *param, int dt, int *x, int *y) {
    int k = param->k;
    int x0 = param->x0;
    int y0 = param->y0;
    int v0x = param->v0x;
    int v0y = param->v0y;
    int more = 1;

    if ((k < 0) && (dt >= -k / 2)) {
        dt = -k / 2;
        more = 0;
    }
    if (x) *x = x0 + (v0x * dt + v0x * dt / k * dt) / (int)(EVE_RTC_FREQ);
    if (y) *y = y0 + (v0y * dt + v0y * dt / k * dt) / (int)(EVE_RTC_FREQ);

    return more;
}

/* Linear harmonic oscillator */
void eve_phy_lho_init(EVEPhyLHO *param, int x, int y, uint32_t T, double d, uint32_t t_max) {
    double f0 = 2 * M_PI / (T * EVE_RTC_FREQ / 1000);

    if (d < 0) d = 0;
    if (d > 1) d = 1;
    param->x = x;
    param->y = y;
    param->f = f0 * sqrt(1 - d * d);
    param->a = -d * f0;
    param->t_max = t_max * EVE_RTC_FREQ / 1000;
}

int eve_phy_lho_start(EVEPhyLHO *param, int x0, int y0) {
    param->x0 = x0;
    param->y0 = y0;
}

int eve_phy_lho_tick(EVEPhyLHO *param, int dt, int *x, int *y) {
    int ax = param->x0 - param->x;
    int ay = param->y0 - param->y;
    int more = 1;

    if (param->t_max && (dt >= param->t_max)) {
        dt = param->t_max;
        more = 0;
    }
    if (param->a) {
        double e = exp(param->a * dt);
        ax = ax * e;
        ay = ay * e;
        if ((ax == 0) && (ay == 0)) more = 0;
    }
    if (x) *x = param->x + ax * cos(param->f * dt);
    if (y) *y = param->y + ay * cos(param->f * dt);

    return more;
}
