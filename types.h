/* Linux PPPD DOS port by ALM, tonilop@redestb.es */

#ifndef __MYTYPES_H__
#define __MYTYPES_H__

typedef unsigned char u_char;
typedef unsigned long int u_int32_t;
typedef char * caddr_t;
typedef unsigned int u_int;
typedef unsigned short u_short;

#define ntohl(x) htonl(x)
#define ntohs(x) htons(x)
#define _fastcall
unsigned long _fastcall htonl(unsigned long);
unsigned short _fastcall htons(unsigned short);
int sprintf(char *, const char *, ...);
int vsprintf(char *, const char *, void *);

/*
 * Definitions of the bits in an Internet address integer.
 * On subnets, host and network parts are found according
 * to the subnet mask, not these masks.
 */
#define IN_CLASSA(a)            ((((long int) (a)) & 0x80000000LU) == 0)
#define IN_CLASSA_NET           0xff000000LU
#define IN_CLASSA_NSHIFT        24
#define IN_CLASSA_HOST          (0xffffffffLU & ~IN_CLASSA_NET)
#define IN_CLASSA_MAX           128

#define IN_CLASSB(a)            ((((long int) (a)) & 0xc0000000LU) == 0x80000000LU)
#define IN_CLASSB_NET           0xffff0000LU
#define IN_CLASSB_NSHIFT        16
#define IN_CLASSB_HOST          (0xffffffffLU & ~IN_CLASSB_NET)
#define IN_CLASSB_MAX           65536L

#define IN_CLASSC(a)            ((((long int) (a)) & 0xe0000000LU) == 0xc0000000LU)
#define IN_CLASSC_NET           0xffffff00LU
#define IN_CLASSC_NSHIFT        8
#define IN_CLASSC_HOST          (0xffffffffLU & ~IN_CLASSC_NET)

#define IN_CLASSD(a)            ((((long int) (a)) & 0xf0000000LU) == 0xe0000000LU)
#define IN_MULTICAST(a)         IN_CLASSD(a)
#define IN_MULTICAST_NET        0xF0000000LU

#define IN_EXPERIMENTAL(a)      ((((long int) (a)) & 0xe0000000LU) == 0xe0000000LU)
#define IN_BADCLASS(a)          ((((long int) (a)) & 0xf0000000LU) == 0xf0000000LU)

/* Address to accept any incoming messages. */
#define INADDR_ANY              ((unsigned long int) 0x00000000LU)

/* Address to send to all hosts. */
#define INADDR_BROADCAST        ((unsigned long int) 0xffffffffLU)

/* Address indicating an error return. */
#define INADDR_NONE             ((unsigned long int) 0xffffffffLU)

/* Network number for local host loopback. */
#define IN_LOOPBACKNET          127

/* Address to loopback in software to local host.  */
#define INADDR_LOOPBACK         0x7f000001LU      /* 127.0.0.1   */
#define IN_LOOPBACK(a)          ((((long int) (a)) & 0xff000000LU) == 0x7f000000LU)

/* Defines for Multicast INADDR */
#define INADDR_UNSPEC_GROUP     0xe0000000LU      /* 224.0.0.0   */
#define INADDR_ALLHOSTS_GROUP   0xe0000001LU      /* 224.0.0.1   */
#define INADDR_MAX_LOCAL_GROUP  0xe00000ffLU      /* 224.0.0.255 */

#endif /* __MYTYPES_H__ */
