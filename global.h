/* Linux PPPD DOS port by ALM, tonilop@redestb.es */

#ifndef _GLOBAL_H
#define _GLOBAL_H

#include <stdlib.h>
#include <string.h>

/* Global definitions used by every source file.
 * Some may be compiler dependent.
 *
 * This file depends only on internal macros or those defined on the
 * command line, so it may be safely included first.
 */

#define READ_BINARY     "rb"
#define WRITE_BINARY    "wb"
#define APPEND_BINARY   "ab+"
#define READ_TEXT       "rt"
#define WRITE_TEXT      "wt"
#define APPEND_TEXT     "at+"

/* These two lines assume that your compiler's longs are 32 bits and
 * shorts are 16 bits. It is already assumed that chars are 8 bits,
 * but it doesn't matter if they're signed or unsigned.
 */
typedef long int32;             /* 32-bit signed integer */
typedef unsigned long uint32;   /* 32-bit unsigned integer */
typedef unsigned short uint16;  /* 16-bit unsigned integer */
typedef unsigned char byte_t;   /* 8-bit unsigned integer */
typedef unsigned char uint8;    /* 8-bit unsigned integer */
#define MAXINT16 0xffff         /* Largest 16-bit integer */
#define MAXINT32 0xffffffff     /* Largest 32-bit integer */
#define NBBY    8               /* 8 bits/byte */

#define INTERRUPT       void interrupt

/* Note that these definitions are on by default if none of the Turbo-C style
 * memory model definitions are on; this avoids having to change them when
 * porting to 68K environments.
 */
#if     !defined(__TINY__) && !defined(__SMALL__) && !defined(__MEDIUM__) && !defined(__GNUC__)
#define LARGEDATA       1
#endif

#if     !defined(__TINY__) && !defined(__SMALL__) && !defined(__COMPACT__) && !defined(__GNUC__)
#define LARGECODE       1
#endif

/* Since not all compilers support structure assignment, the ASSIGN()
 * macro is used. This controls how it's actually implemented.
 */
#ifdef  NOSTRUCTASSIGN  /* Version for old compilers that don't support it */
#define ASSIGN(a,b)     memcpy((char *)&(a),(char *)&(b),sizeof(b);
#else                   /* Version for compilers that do */
#define ASSIGN(a,b)     ((a) = (b))
#endif

/* Define null object pointer in case stdio.h isn't included */
#ifndef NULL
/* General purpose NULL pointer */
#define NULL 0
#endif

/* standard boolean constants */
#define FALSE 0
#define TRUE 1
#define NO 0
#define YES 1

#define CTLA 0x1
#define CTLB 0x2
#define CTLC 0x3
#define CTLD 0x4
#define CTLE 0x5
#define CTLF 0x6
#define CTLG 0x7
#define CTLH 0x8
#define CTLI 0x9
#define CTLJ 0xa
#define CTLK 0xb
#define CTLL 0xc
#define CTLM 0xd
#define CTLN 0xe
#define CTLO 0xf
#define CTLP 0x10
#define CTLQ 0x11
#define CTLR 0x12
#define CTLS 0x13
#define CTLT 0x14
#define CTLU 0x15
#define CTLV 0x16
#define CTLW 0x17
#define CTLX 0x18
#define CTLY 0x19
#define CTLZ 0x1a

#define BELL    CTLG
#define BS      CTLH
#define TAB     CTLI
#define LF      CTLJ
#define FF      CTLL
#define CR      CTLM
#define XON     CTLQ
#define XOFF    CTLS
#define ESC     0x1b
#define DEL     0x7f

/* string equality shorthand */
#define STREQ(x,y) (strcmp(x,y) == 0)

/* Extract a short from a long */
#define hiword(x)       ((uint16)((x) >> 16))
#define loword(x)       ((uint16)(x))

/* Extract a byte from a short */
#define hibyte(x)       ((unsigned char)((x) >> 8))
#define lobyte(x)       ((unsigned char)(x))

/* Extract nibbles from a byte */
#define hinibble(x)     (((x) >> 4) & 0xf)
#define lonibble(x)     ((x) & 0xf)

/* Various low-level and miscellaneous functions */
int dirps(void);
int istate(void);
void restore(int);
uint16 far *getindosflag(void);
#define FREE(p)         {free(p); p = NULL;}

#define movblock(so,ss,do,ds,c) movedata(ss,so,ds,do,c)

#endif  /* _GLOBAL_H */
