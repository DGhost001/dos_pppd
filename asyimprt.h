/* Linux PPPD DOS port by ALM, tonilop@redestb.es */

#ifndef _ASYIMPRT_H_
#define _ASYIMPRT_H_

#include "global.h"

typedef struct {
    uint16 stsize;
    int (far *asyf_rxcheck)(void);
    int (far *asyf_txcheck)(void);
    int (far *asyf_getc)(void);
    int (far *asyf_putc)(uint8);
} ASY_HOOKS;

extern const ASY_HOOKS asy_exportinfo;

#endif  /* _ASYIMPRT_H_ */

