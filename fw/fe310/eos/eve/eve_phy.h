#include <stdint.h>

typedef struct EVEPhyAcc {
    int a;
    int k;
    int x0;
    int y0;
    int v0x;
    int v0y;
} EVEPhyAcc;

void eve_phy_acc_init(EVEPhyAcc *param, int a);
void eve_phy_acc_start(EVEPhyAcc *param, int x0, int y0, int v0x, int v0y);
int eve_phy_acc_tick(EVEPhyAcc *param, int dt, int *x, int *y);

typedef struct EVEPhyLHO {
    int x;
    int y;
    double f;
    double a;
    uint32_t t_max;
    int x0;
    int y0;
} EVEPhyLHO;

void eve_phy_lho_init(EVEPhyLHO *param, int x, int y, uint32_t T, double d, uint32_t t_max);
int eve_phy_lho_start(EVEPhyLHO *param, int x0, int y0);
int eve_phy_lho_tick(EVEPhyLHO *param, int dt, int *x, int *y);