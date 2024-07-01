/* Linux PPPD DOS port by ALM, tonilop@redestb.es */

#ifndef _ASY_H
#define _ASY_H

#ifndef _GLOBAL_H
#include "global.h"
#endif

/* If you increase this, you must add additional interrupt vector
 * hooks in asyvec.asm
 */
#define ASY_MAX 4

enum devnames {
    COM1=0,
    COM2,
    COM3,
    COM4
};

/* device parameter control */
enum devparam {
    PARAM_DTR,
    PARAM_RTS,
    PARAM_SPEED,
    PARAM_DOWN,
    PARAM_UP
};

/* In n8250.c: */
int asy_init(int dev, int base, int irq, uint16 bufsize,
             long speed, int cts, int rlsd, int chain);
int asy_stop(int dev, int keepdtr);
int asy_speed(int dev, long bps);
int asy_read(int dev, uint8 *buf, uint16 cnt);
int get_asy(int dev);
int asy_rxcheck(int dev);
int asy_write(int dev, uint8 *buf, uint16 cnt);
int asy_txcheck(int dev);
int32 asy_ioctl(int dev, int cmd, int set, int32 val);
int get_rlsd_asy(int dev);
int wait_rlsd_asy(int dev, int new_rlsd);
int asy_info(int dev);

#endif  /* _ASY_H */
